#pragma once

#include <QProcess>
#include <QString>
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
#include <format>
#include <cassert>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32")
#define SOCKLEN int
#elif defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#define SOCKLEN socklen_t
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#define InetPtonA inet_pton
#define closesocket close
#else
#error "Only Windows and Linux are supported for now"
#endif

#include "types.hpp"
#include "../utils/serializer.hpp"
#include "../utils/thread.hpp"
#include "../utils/utils.hpp"
#include "../algorithms/pathFinder.hpp"

/**
 *      Protocol used to communicate with Factorio:
 * 
 * - To send data from C++ to Factorio the rcon is used. Factorio's stdin should not be used
 *   since it is disabled for graphical instances.
 *
 * - To send data from Factorio to C++, Factorio's stdout is used. The rcon responses could be
 *   used in some situations but Factorio only allows to respond to a rcon request in the same
 *   game tick as it was received, which is really impractical.
 * 
 * - All data sent between C++ and Factorio is serialized in JSON using serializer.hpp
 * 
 * - C++ can send request to Factorio using the "/request Request<DataType>" command. Factorio can
 *   respond with "response[size] Response<DataType>". The id fields in Request and Response should be the
 *   same. [size] being the size in byte of the serialized data.
 * 
 * - Factorio can send events to C++ by sending "event[size] Event<DataType>". C++ cannot respond to events.
 */

namespace ComputerPlaysFactorio {

    enum RCONPacketType : int32_t {
        RCON_AUTH = 3,
        RCON_EXECCOMMAND = 2,
        RCON_AUTH_RESPONSE = 2,
        RCON_RESPONSE_VALUE = 0
    };

    struct RequestBase {
        uint32_t id;
        std::string name;

        SERIALIZABLE(RequestBase, id, name)
    };

    enum RequestError {
        FACTORIO_NOT_RUNNING = 1,
        FACTORIO_EXITED = 2,
        BUSY = 3,
        NO_PATH_FOUND = 4,
        EMPTY_PATH = 5,
        ENTITY_DOESNT_EXIST = 6,
        ITEM_DOESNT_EXIST = 7,
        FLUID_DOESNT_EXIST = 8,
        RECIPE_DOESNT_EXIST = 9,
        NOT_ENOUGH_ITEM = 10,
        NOT_ENOUGH_ROOM = 11,
        NOT_IN_RANGE = 12,
        ITEM_NOT_BUILDABLE = 13,
        NO_ENTITY_FOUND = 14,
        NO_INVENTORY_FOUND = 15,
        NOT_ENOUGH_INGREDIENTS = 16,
        ENTITY_NOT_ROTATABLE = 17
    };

    struct ResponseBase {
        uint32_t id;
        bool success;
        RequestError error;

        SERIALIZABLE(ResponseBase, id, success, error)
    };

    template<class T>
    struct Response : public ResponseBase {
        T data;

        SERIALIZABLE(Response<T>, id, success, error, data)
    };

    struct EventBase {
        uint32_t id;
        std::string name;

        SERIALIZABLE(EventBase, id, name);
    };

    template<class T>
    struct Event : public EventBase {
        T data;

        SERIALIZABLE(Event, id, name, data);
    };

    class FactorioInstance : public QObject {
        Q_OBJECT

    public:
        enum Type {
            GRAPHICAL,
            HEADLESS
        };

        // Must be called before constructing any FactorioInstance
        static void InitStatic();
        static inline void SetFactorioPath(const std::string &path) {
            s_factorioPath = path;
        }
        static bool IsFactorioPathValid(const std::string &path, std::string &message);
        static void StopAll();
        static void JoinAll();

        FactorioInstance(Type type, bool defaultStdoutListener = true);
        // Blocks the thread to wait for Factorio to stop !
        inline ~FactorioInstance() {
            s_instances.erase(this);
            Stop();
            Join();
        }
        void RegisterEvents();

        FactorioInstance(const FactorioInstance&)  = delete;
        FactorioInstance(const FactorioInstance&&) = delete;
        void operator=(const FactorioInstance&)    = delete;
        void operator=(const FactorioInstance&&)   = delete;

        // Returns true if Factorio is running
        inline bool Running() const {
            return m_process.state() != QProcess::ProcessState::NotRunning;
        }
        bool Start(uint32_t seed, std::string *message = nullptr);
        void Stop();
        inline bool Join(int ms = -1) {
            return m_process.waitForFinished(ms);
        }

        bool SendRCON(const std::string &data, RCONPacketType type = RCON_EXECCOMMAND) const;

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
        inline auto GetPlayerPosition() { Request<MapPosition>("GetPlayerPosition"); }
        inline const PathfinderData &GetPathFinderData() { return m_pathfinderData; }

        const Type instanceType;

        bool printStdout = false;

    signals:
        void Started(FactorioInstance&);
        void Ready(FactorioInstance&);      // Emitted after RCON connection was opened
        void Closed(FactorioInstance&);     // Emitted after RCON connection was closed
        void Terminated(FactorioInstance&, int exitCode, QProcess::ExitStatus);

    private slots:
        void StdoutListener();

    private:
        static bool s_initStatic;

        static inline std::atomic<uint32_t> s_id = 0;
        const uint32_t m_id = s_id++;
        static inline std::set<FactorioInstance*> s_instances;

        static std::string s_factorioPath;

        QProcess m_process;
        std::mutex m_mutex;
        std::mutex m_stdoutListenerMutex;

        inline std::filesystem::path GetInstanceTempPath() { return GetTempDir() / ("data" + std::to_string(m_id)); }
        inline std::filesystem::path GetConfigPath() { return GetInstanceTempPath() / "config.ini"; }
        void EditConfig(const std::string &category, const std::string &name, const std::string &value);

        std::map<uint32_t, std::function<void(const QJsonObject &data)>> m_pendingRequests;

        bool RequestPrivate(const std::string &name, uint32_t *id = nullptr) const;
        template<class T>
        bool RequestPrivate(const std::string &name, const T &data, uint32_t *id = nullptr) const;
        template<class R>
        std::future<Response<R>> AddResponse(uint32_t id);
        std::future<ResponseBase> AddResponseBase(uint32_t id);

        std::map<std::string, std::function<void(const QJsonObject &data)>> m_eventHandlers;

        template<typename T>
        using EventCallback = std::function<void(const Event<T>&)>;
        using EventCallbackDL = std::function<void(const EventBase&)>;

        template<typename T>
        void RegisterEvent(const std::string &name, EventCallback<T>);
        void RegisterEvent(const std::string &name, EventCallbackDL);

        struct UpdatePathfinderData {
            std::vector<MapPosition> add;
            std::vector<MapPosition> remove;

            SERIALIZABLE(UpdatePathfinderData, add, remove);
        };

        PathfinderData m_pathfinderData;
        void UpdatePathfinderDataHandler(const Event<UpdatePathfinderData>&);

        void StartRCON();
        void CloseRCON();
        bool ReadPacket(int32_t &id, int32_t &type, char body[4096]) const;
        const std::string m_rconAddr = "127.0.0.1";
        uint16_t m_rconPort;
        bool m_rconConnected = false;
        SOCKET m_rconSocket = INVALID_SOCKET;
    };

    template <class T>
    bool FactorioInstance::RequestPrivate(const std::string &name, const T &data, uint32_t *id) const {
        RequestBase r;
        r.id = s_id++;
        r.name = name;
        if (id) *id = r.id;
        auto json = ToJson(r);
        json["data"] = ToJson(data);
        return SendRCON("/request " + QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString());
    }

    template<class R>
    std::future<Response<R>> FactorioInstance::AddResponse(uint32_t id) {
        auto promise = std::make_shared<std::promise<Response<R>>>();
        m_pendingRequests[id] = [promise](const QJsonObject &json) {
            Response<R> data;
            FromJson(json, data);
            promise->set_value(data);
        };
        return promise->get_future();
    }

    template<class T, class R>
    std::future<Response<R>> FactorioInstance::Request(const std::string &name, const T &data) {
        uint32_t id;
        if (!RequestPrivate(name, data, &id)) {
            Response<R> res;
            res.id = id;
            res.success = false;
            res.error = FACTORIO_NOT_RUNNING;

            std::promise<Response<R>> promise;
            promise.set_value(res);
            return promise.get_future();
        }

        return AddResponse<R>(id);
    }

    template<class R>
    std::future<Response<R>> FactorioInstance::Request(const std::string &name) {
        uint32_t id;
        if (!RequestPrivate(name, &id)) {
            Response<R> res;
            res.id = id;
            res.success = false;
            res.error = FACTORIO_NOT_RUNNING;

            std::promise<Response<R>> promise;
            promise.set_value(res);
            return promise.get_future();
        }

        return AddResponse<R>(id);
    }

    template<class T>
    std::future<ResponseBase> FactorioInstance::RequestNoRes(const std::string &name, const T &data) {
        uint32_t id;
        if (!RequestPrivate(name, data, &id)) {
            std::promise<ResponseBase> promise;
            promise.set_value(ResponseBase{
                .id = id,
                .success = false,
                .error = FACTORIO_NOT_RUNNING
            });
            return promise.get_future();
        }

        return AddResponseBase(id);
    }

    template<typename T>
    void FactorioInstance::RegisterEvent(const std::string &name, EventCallback<T> callback) {
        assert(callback);
        m_eventHandlers[name] = [=](const QJsonObject &json) {
            Event<T> data;
            FromJson(json, data);
            ThreadPool::QueueJob([data, callback] {
                callback(data);
            });
        };
    }
}