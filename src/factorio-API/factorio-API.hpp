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
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <deque>
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
    
    class LuaError : public std::exception {
    public:
        LuaError(const std::string &msg_) : msg(msg_) {}
        const char *what() const {
            return msg.c_str();
        }

    private:
        std::string msg;
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

        std::future<json> Request(const std::string &name);
        std::future<json> Request(const std::string &name, const json &data);
        template<class T>
        std::future<json> Request(const std::string &name, const T &data) {
            json j(data);
            return Request(name, j);
        }

        inline auto Broadcast(const std::string &msg) { return Request("Broadcast", msg); }
        inline auto SetGameSpeed(float ticks) { return Request("GameSpeed", ticks); }
        inline auto PauseToggle() { return Request("PauseToggle"); }
        inline auto Save(const std::string &name) { return Request("Save", name); }
        inline auto PlayerPosition() { return Request("PlayerPosition"); }

        void RegisterEvent(const std::string &name, std::function<void(const json&)>);

        const Type instance_type;

    private:

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
        std::function<void(int exit_code)> m_exited_callback;
        
        int ReadStdout(char *buffer, int size);
        void CheckWord(const std::string &previous_word, const std::string &word);
        void OutListener(terminate_handler terminate);
        std::thread m_out_listener;

        inline std::filesystem::path GetInstanceTempDir() { return GetTempDirectory() / ("data" + std::to_string(m_id)); }
        inline std::filesystem::path GetConfigPath() { return GetInstanceTempDir() / "config.ini"; }

        std::map<uint32_t, std::function<void(const json&)>> m_pending_requests;

        std::future<json> RequestPrivate(const std::string &name, const json*);

        std::map<std::string, std::function<void(const json &data)>> m_event_handlers;

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
}