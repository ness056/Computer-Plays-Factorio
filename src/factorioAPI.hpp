#pragma once

#include <QProcess>
#include <QString>
#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <filesystem>
#include <deque>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "WS2_32")
#elif defined(__linux__)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#else
#error "Only Windows and Linux are supported for now"
#endif

#include "serializer.hpp"
#include "logging.hpp"
#include "thread.hpp"
#include "instruction.hpp"

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

    template<class T>
    struct Request {
        uint32_t id;
        std::string name;
        T data;

        QUICK_AUTO_NAMED_PROPERTIES(Request<T>, id, name, data)
    };

    struct ResponseDataless {
        uint32_t id;
        bool success;
        std::string message;

        QUICK_AUTO_NAMED_PROPERTIES(ResponseDataless, id, success, message)
    };

    template<class T>
    struct Response {
        uint32_t id;
        bool success;
        std::string message;
        T data;

        QUICK_AUTO_NAMED_PROPERTIES(Response<T>, id, success, message, data)
    };

    struct EventDataless {
        uint32_t id;
        std::string name;
    };

    template<class T>
    struct Event {
        uint32_t id;
        std::string name;
        T data;
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

        FactorioInstance(const std::string &name, Type type, bool defaultStdoutListener = true);
        // Blocks the thread to wait for Factorio to stop !
        inline ~FactorioInstance() {
            Stop();
            Join();
        }

        FactorioInstance(const FactorioInstance&)  = delete;
        FactorioInstance(const FactorioInstance&&) = delete;
        void operator=(const FactorioInstance&)    = delete;
        void operator=(const FactorioInstance&&)   = delete;

        // Returns true if Factorio is running
        inline bool Running() {
            return m_process.state() != QProcess::ProcessState::NotRunning;
        }
        bool Start(std::string *message = nullptr);
        bool Stop();
        inline bool Join(int ms = -1) {
            return m_process.waitForFinished(ms);
        }

        bool SendRCON(const std::string &data, RCONPacketType type = RCON_EXECCOMMAND);
        template<class T>
        using RequestCallback = std::function<void(FactorioInstance&, const Response<T>&)>;
        template<class T, class R=int>
        bool SendRequest(const std::string &name, const T &data, RequestCallback<R> callback = nullptr);

        bool QueueWalk(double x, double y);
        // bool QueueCraft();
        // bool QueueCancel();
        // bool QueueTech();
        // bool QueuePickup();
        // bool QueueDrop();
        // bool QueueMine();
        // bool QueueShoot();
        // bool QueueUse();
        // bool QueueEquip();
        // bool QueueTake();
        // bool QueuePut();
        // bool QueueBuild();
        // bool QueueRecipe();
        // bool QueueLimit();
        // bool QueueFilter();
        // bool QueuePriority();
        // bool QueueLaunch();
        // bool QueueRotate();

        const std::string name;
        const Type type;

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

        static std::string s_factorioPath;
        static std::string s_luaPath;

        QProcess m_process;
        std::mutex m_mutex;
        std::mutex m_stdoutListenerMutex;

        static inline uint32_t s_requestIndex = 0;
        std::map<int32_t, std::function<void(const QJsonObject &data)>> m_pendingRequests;
        std::deque<Instruction> m_instructions;

        void StartRCON();
        void CloseRCON();
        bool ReadPacket(int32_t &id, int32_t &type, char body[4096]);
        std::string m_rconAddr = "127.0.0.1";
        std::string m_rconPass = "pass";
        uint16_t m_rconPort;
        bool m_rconConnected = false;
    #ifdef _WIN32
        SOCKET m_rconSocket = INVALID_SOCKET;
    #elif defined(__linux__)
        int m_rconSocket = -1;
    #else
    #error "Only Windows and Linux are supported for now"
    #endif
    };

    template<class T, class R>
    bool FactorioInstance::SendRequest(const std::string &name, const T &data, RequestCallback<R> callback) {
        const uint32_t id = s_requestIndex++;

        Request<T> request;
        request.id = id;
        request.name = name;
        request.data = data;
        auto json = ToJson(request);

        if (!SendRCON("/request " + QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString())) return false;
        
        if (callback == nullptr) return true;
        m_pendingRequests[id] = [=, this](const QJsonObject &json) {
            Response<R> data;
            FromJson(json, data);
            ThreadPool::QueueJob([this, data, callback] {
                callback(*this, data);
            });
        };

        return true;
    }
}