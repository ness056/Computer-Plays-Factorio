#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "WS2_32")
#elif defined(__linux__)
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#else
#error "Only Windows and Linux are supported for now"
#endif

#include "serializer.hpp"

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
 * - All data send from and to C++ or Factorio is serialized using serializer.hpp or serializer.lua
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

enum RequestName : int8_t {
    TEST_REQUEST_NAME = 0,
};

struct RequestDataless {
    uint32_t id;
    RequestName name;

    constexpr static auto properties = MakeSerializerProperties(&RequestDataless::id, &RequestDataless::name);
};

template<class T>
struct Request {
    uint32_t id;
    RequestName name;
    T data;

    constexpr static auto properties = MakeSerializerProperties(&Request<T>::id, &Request<T>::name, &Request<T>::data);
};

template<class T>
struct Response {
    uint32_t id;
    T data;
};

enum EventName : int8_t {

};

struct EventDataless {
    uint32_t id;
    EventName name;
};

template<class T>
struct Event {
    uint32_t id;
    EventName name;
    T data;
};

class FactorioInstance {
public:
    // Must be called before constructing any FactorioInstance
    static void InitStatic();
    static inline void SetFactorioPath(const std::string &path) {
        s_factorioPath = path;
    }
    static bool IsFactorioPathValid(const std::string &path, std::string &message);

    FactorioInstance(const std::string &name, bool graphical);
    // Blocks the thread to wait for Factorio to stop !
    ~FactorioInstance();

    FactorioInstance(const FactorioInstance&) = delete;
    void operator=(const FactorioInstance&)   = delete;
    void operator=(const FactorioInstance&&)  = delete;

    inline void SetTerminateCallback(const std::function<void(FactorioInstance&, int exitCode)> &callback) {
        m_terminateCallback = callback;
    }

    // Returns true if Factorio is running
    bool Running();
    // callback is called when the instance is up and ready
    bool Start(const std::function<void(FactorioInstance&)> &callback = nullptr, std::string *message = nullptr);
    bool Stop();
    bool Join();

    // SendRCON should be preferred (see comment at top of file)
    bool WriteStdin(const std::string &data, int *writtenBytes = nullptr);
    bool SendRCON(const std::string &data, RCONPacketType type = RCON_EXECCOMMAND);
    template<class T, class R = int>
    bool SendRequest(RequestName name, const T &data, std::function<void(FactorioInstance&, R&)> callback = nullptr);

    const std::string name;
    const bool graphical;

    bool printStdout = false;

private:
    static bool s_initStatic;
#ifdef _WIN32
    static HANDLE s_jobObject;
#endif

    static std::string s_factorioPath;
    static std::string s_luaPath;

    std::mutex m_mutex;

    std::function<void(FactorioInstance&)> m_startCallback = nullptr;
    std::function<void(FactorioInstance&, int exitCode)> m_terminateCallback = nullptr;
    std::map<int32_t, std::function<void(const std::string &data)>> m_pendingRequests;
    
    int m_exitCode;
#ifdef _WIN32
    STARTUPINFOA m_startupInfo;
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_fstdinWrite;
    HANDLE m_fstdoutRead;
    HANDLE m_fstdoutEvent;
    OVERLAPPED m_fstdoutOl;
#elif defined(__linux__)
    pid_t m_process = INT_MAX;

    int m_fstdinWrite;
    int m_fstdoutRead;
#else
#error "Only Windows and Linux are supported for now"
#endif
    std::thread m_fstdoutListener;

    bool m_cleaned = true;
    void Clean();
    int StartPrivate(const std::vector<char*> &argv);

    int StdoutRead(char *buffer, int size);
    void StdoutListener();

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
bool FactorioInstance::SendRequest(RequestName name, const T &data, std::function<void(FactorioInstance&, R&)> callback) {
    static uint32_t index = 0;
    const uint32_t id = index++;

    Request<T> request;
    request.id = id;
    request.name = name;
    request.data = data;
    std::string serialized;
    Serialize(request, serialized);
    std::cout << "s" << serialized << std::endl;

    if (!SendRCON("/request " + serialized)) return false;

    if (callback != nullptr) {
        m_pendingRequests[id] = [this, name, id, callback](const std::string &serialized) {
            R data;
            size_t i = 0;
            if (!Deserialize(serialized, i, data)) {
                std::cerr << "Response to request of name " << name << " and id " << id << "could not be deserialize." << std::endl;
                exit(1);
            }
            callback(*this, data);
        };
    }

    return true;
}
}