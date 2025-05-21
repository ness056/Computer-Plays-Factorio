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

        bool SendRequest(const std::string &name) const;
        template<class T>
        bool SendRequestData(const std::string &name, const T &data) const;
        template<class R>
        bool SendRequestRes(const std::string &name, RequestCallback<R> callback);
        template<class T, class R>
        bool SendRequestDataRes(const std::string &name, const T &data, RequestCallback<R> callback);
        bool SendRequestResDL(const std::string &name, RequestDatalessCallback callback);
        template<class T>
        bool SendRequestDataResDL(const std::string &name, const T &data, RequestDatalessCallback callback);

        inline bool Broadcast(const std::string &msg) const { return SendRequestData("Broadcast", msg); }
        inline bool SetGameSpeed(float ticks) const { return SendRequestData("GameSpeed", ticks); }
        inline bool PauseToggle() const { return SendRequest("PauseToggle"); }
        inline bool Save(const std::string &name) const { return SendRequestData("Save", name); }

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

        bool SendRequestPrivate(std::string name, uint32_t *id = nullptr) const;
        template<class T>
        bool SendRequestPrivate(std::string name, const T &data, uint32_t *id = nullptr) const;
        template<class R>
        void AddResponse(uint32_t id, RequestCallback<R> callback);
        void AddResponseDataless(uint32_t id, RequestDatalessCallback callback);

        void StartRCON();
        void CloseRCON();
        bool ReadPacket(int32_t &id, int32_t &type, char body[4096]) const;
        const std::string m_rconAddr = "127.0.0.1";
        uint16_t m_rconPort;
        bool m_rconConnected = false;
        SOCKET m_rconSocket = INVALID_SOCKET;
    };

    template <class T>
    bool FactorioInstance::SendRequestPrivate(std::string name, const T &data, uint32_t *id) const {
        RequestDataless r;
        r.id = s_id++;
        r.name = name;
        if (id) *id = r.id;
        auto json = ToJson(r);
        json["data"] = ToJson(data);
        return SendRCON("/request " + QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString());
    }

    template<class R>
    void FactorioInstance::AddResponse(uint32_t id, RequestCallback<R> callback) {
        assert(callback);
        m_pendingRequests[id] = [=, this](const QJsonObject &json) {
            Response<R> data;
            FromJson(json, data);
            ThreadPool::QueueJob([this, data, callback] {
                callback(*this, data);
            });
        };
    }

    template<class T>
    bool FactorioInstance::SendRequestData(const std::string &name, const T &data) const {
        return SendRequestPrivate(name, data);
    }

    template<class R>
    bool FactorioInstance::SendRequestRes(const std::string &name, RequestCallback<R> callback) {
        uint32_t id;
        if (!SendRequestPrivate(name, &id)) return false;

        AddResponse(id, callback);
        return true;
    }

    template<class T, class R>
    bool FactorioInstance::SendRequestDataRes(const std::string &name, const T &data, RequestCallback<R> callback) {
        uint32_t id;
        if (!SendRequestPrivate(name, data, &id)) return false;

        AddResponse(id, callback);
        return true;
    }

    template<class T>
    bool FactorioInstance::SendRequestDataResDL(const std::string &name, const T &data, RequestDatalessCallback callback) {
        uint32_t id;
        if (!SendRequestPrivate(name, data, &id)) return false;

        AddResponseDataless(id, callback);
        return true;
    }
}