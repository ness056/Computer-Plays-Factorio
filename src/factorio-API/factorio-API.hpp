#pragma once

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <filesystem>
#include <algorithm>
#include <future>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "WS2_32")
#define SOCKLEN int
#define LAST_SOCKET_ERROR WSAGetLastError()
#else
#error "Only Windows is supported for now"
#endif

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "types.hpp"
#include "../utils/logging.hpp"

/**
 *      Protocol used to communicate with Factorio:
 * 
 * - To send data from C++ to Factorio the rcon is used. Factorio's stdin should not be used
 *   since it is disabled for graphical instances.
 *
 * - To send data from Factorio to C++, Factorio's write_file function is used.
 *   The rcon responses could be used in some situations but Factorio only allows to respond
 *   to a rcon request in the same game tick as it was received, which is really impractical.
 * 
 * - All data sent between C++ and Factorio is serialized in JSON.
 * 
 * - C++ can send request to Factorio using the "/request Request<DataType>" command. Factorio can
 *   respond with "response[size] Response<DataType>". The id fields in Request and Response should be the
 *   same. [size] being the size in byte of the serialized data.
 * 
 * - Factorio can send events to C++ by sending "event[size] Event<DataType>".
 */

namespace ComputerPlaysFactorio {

    enum RCONPacketType : int32_t {
        RCON_AUTH = 3,
        RCON_EXECCOMMAND = 2,
        RCON_AUTH_RESPONSE = 2,
        RCON_RESPONSE_VALUE = 0
    };

    enum class RequestError {
        SUCCESS,
        FACTORIO_NOT_RUNNING,
        FACTORIO_EXITED,
        BUSY,
        NO_PATH_FOUND,
        EMPTY_PATH,
        ENTITY_DOESNT_EXIST,
        ITEM_DOESNT_EXIST,
        FLUID_DOESNT_EXIST,
        RECIPE_DOESNT_EXIST,
        NOT_ENOUGH_ITEM,
        NOT_ENOUGH_ROOM,
        NOT_IN_RANGE,
        ITEM_NOT_BUILDABLE,
        NO_ENTITY_FOUND,
        NO_INVENTORY_FOUND,
        NOT_ENOUGH_INGREDIENTS,
        ENTITY_NOT_ROTATABLE
    };

    struct ResponseBase {
        uint32_t id;
        bool success;
        RequestError error;
    };

    template<class T>
    struct Response {
        uint32_t id;
        bool success;
        RequestError error;
        std::optional<T> data;
    };

    struct EventBase {
        uint32_t id;
        std::string name;
        uint64_t tick;
    };

    template<class T>
    struct Event {
        uint32_t id;
        std::string name;
        uint64_t tick;
        T data;
    };

    class FactorioInstance {
    public:
        enum Type {
            GRAPHICAL,
            HEADLESS
        };

        friend class FactorioPrototypes;

        static inline void SetFactorioPath(const std::string &path) {
            s_factorio_path = path;
        }
        static Result IsFactorioPathValid(const std::string &path);
        static void StopAll();
        static void JoinAll();

        FactorioInstance(Type type);
        // Blocks the thread to wait for Factorio to stop !
        ~FactorioInstance();

        FactorioInstance(const FactorioInstance&)  = delete;
        void operator=(const FactorioInstance&)    = delete;

        bool Running();
        Result Start(uint32_t seed);
        void Stop();
        void Join(int ms = -1);

        // Called when a save was loaded/created and you may start sending requests
        inline void SetReadyCallback(const std::function<void()> &callback) {
            m_ready_callback = callback;
        }

        // Called when a save is closed
        inline void SetClosedCallback(const std::function<void()> &callback) {
            m_closed_callback = callback;
        }

        // Called when Factorio process has exited
        inline void SetExitedCallback(const std::function<void(int exitCode)> &callback) {
            m_exited_callback = callback;
        }

        Result SendRCON(const std::string &data, RCONPacketType type = RCON_EXECCOMMAND) const;

        template<class T, class R>
        std::future<Response<R>> Request(const std::string &name, const T &data);
        template<class R>
        std::future<Response<R>> Request(const std::string &name);
        template<class T>
        std::future<ResponseBase> RequestNoRes(const std::string &name, const T &data);
        std::future<ResponseBase> RequestNoRes(const std::string &name);

        inline auto Broadcast(const std::string &msg) { return RequestNoRes("Broadcast", msg); }
        inline auto SetGameSpeed(float ticks) { return RequestNoRes("GameSpeed", ticks); }
        inline auto PauseToggle() { return RequestNoRes("PauseToggle"); }
        inline auto Save(const std::string &name) { return RequestNoRes("Save", name); }
        inline auto PlayerPosition() { return Request<MapPosition>("PlayerPosition"); }

        template<typename T>
        using EventCallback = std::function<void(const Event<T>&)>;
        using EventCallbackDL = std::function<void(const EventBase&)>;

        template<typename T>
        void RegisterEvent(const std::string &name, const EventCallback<T>&);
        void RegisterEvent(const std::string &name, const EventCallbackDL&);

        const Type instance_type;

    private:
        struct RequestDataBase {
            uint32_t id;
            std::string name;
        };

        template<class T>
        struct RequestData {
            uint32_t id;
            std::string name;
            T data;
        };

        static void InitStatic();
        static inline std::mutex s_static_mutex;
        static inline bool s_init_static = false;
        static inline HANDLE s_job_object = nullptr;

        static inline std::atomic<uint32_t> s_id = 0;
        const uint32_t m_id = s_id++;
        static inline std::set<FactorioInstance*> s_instances;

        static inline std::string s_factorio_path = "";

        std::mutex m_mutex;
        DWORD m_exit_code;
        STARTUPINFOA m_startup_info;
        PROCESS_INFORMATION m_process_info;
        HANDLE m_in_write;
        HANDLE m_out_read;
        HANDLE m_out_event;
        OVERLAPPED m_out_ol;

        Result StartPrivate(const std::vector<const char*> &argv);
        void Clean();
        bool m_cleaned = true;

        std::function<void()> m_ready_callback;
        std::function<void()> m_closed_callback;
        std::function<void(int exitCode)> m_exited_callback;
        
        int ReadStdout(char *buffer, int size);
        void CheckWord(const std::string &previous_word, const std::string &word);
        void OutListener(terminate_handler terminate);
        std::thread m_out_listener;

        inline std::filesystem::path GetInstanceTempDir() { return GetTempDir() / ("data" + std::to_string(m_id)); }
        inline std::filesystem::path GetConfigPath() { return GetInstanceTempDir() / "config.ini"; }

        std::map<uint32_t, std::function<void(const std::string &data)>> m_pending_requests;

        Result RequestPrivate(const std::string &name, uint32_t *id = nullptr) const;
        template<class T>
        Result RequestPrivate(const std::string &name, const T &data, uint32_t *id = nullptr) const;
        template<class R>
        std::future<Response<R>> AddResponse(uint32_t id);
        std::future<ResponseBase> AddResponseBase(uint32_t id);

        std::map<std::string, std::function<void(const std::string &data)>> m_event_handlers;

        // Find a suitable port for Factorio to use for RCON.
        void FindAvailablePort();
        void StartRCON();
        void CloseRCON();
        Result ReadPacket(int32_t &id, int32_t &type, char body[4096]) const;
        const std::string m_rcon_addr = "127.0.0.1";
        uint16_t m_rcon_port;
        bool m_rcon_connected = false;
        SOCKET m_rcon_socket = INVALID_SOCKET;
    };

    template <class T>
    Result FactorioInstance::RequestPrivate(const std::string &name, const T &data, uint32_t *id) const {
        RequestData<T> r;
        r.id = s_id++;
        r.name = name;
        r.data = data;
        if (id) *id = r.id;
        const auto json = rfl::json::write(r);
        return SendRCON("/request " + json);
    }

    template<class R>
    std::future<Response<R>> FactorioInstance::AddResponse(uint32_t id) {
        auto promise = std::make_shared<std::promise<Response<R>>>();
        m_pending_requests[id] = [promise](const std::string &str) {
            auto json = rfl::json::read<Response<R>, rfl::DefaultIfMissing>(str);
            if (!json) {
                throw RuntimeErrorF("Malformed response json: {}\nstring: {}", json.error().what(), str);
            }
            const auto &data = json.value();
            promise->set_value(data);
        };
        return promise->get_future();
    }

    template<class T, class R>
    std::future<Response<R>> FactorioInstance::Request(const std::string &name, const T &data) {
        uint32_t id;
        if (RequestPrivate(name, data, &id) != SUCCESS) {
            Response<R> res;
            res.id = id;
            res.success = false;
            res.error = RequestError::FACTORIO_NOT_RUNNING;

            std::promise<Response<R>> promise;
            promise.set_value(res);
            return promise.get_future();
        }

        return AddResponse<R>(id);
    }

    template<class R>
    std::future<Response<R>> FactorioInstance::Request(const std::string &name) {
        uint32_t id;
        if (RequestPrivate(name, &id) != SUCCESS) {
            Response<R> res;
            res.id = id;
            res.success = false;
            res.error = RequestError::FACTORIO_NOT_RUNNING;

            std::promise<Response<R>> promise;
            promise.set_value(res);
            return promise.get_future();
        }

        return AddResponse<R>(id);
    }

    template<class T>
    std::future<ResponseBase> FactorioInstance::RequestNoRes(const std::string &name, const T &data) {
        uint32_t id;
        if (RequestPrivate(name, data, &id) != SUCCESS) {
            std::promise<ResponseBase> promise;
            promise.set_value(ResponseBase{
                .id = id,
                .success = false,
                .error = RequestError::FACTORIO_NOT_RUNNING
            });
            return promise.get_future();
        }

        return AddResponseBase(id);
    }

    template<typename T>
    void FactorioInstance::RegisterEvent(const std::string &name, const EventCallback<T> &callback) {
        m_event_handlers[name] = [=](const std::string &str) {
            auto json = rfl::json::read<Event<T>, rfl::DefaultIfMissing>(str);
            if (!json) {
                throw RuntimeErrorF("Malformed event json: {}\nstring: {}", json.error().what(), str);
            }
            const auto &data = json.value();
            callback(data);
        };
    }
}