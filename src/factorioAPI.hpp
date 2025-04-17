#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <thread>
#include <mutex>
#include <map>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "WS2_32")
#elif defined(__linux__)
#include <signal.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
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
 *   respond with "response[size]:Response<DataType>". The id fields in Request and Response should be the
 *   same.
 * 
 * - Factorio can send events to C++ by sending "event[size]:Event<DataType>". C++ cannot respond to events.
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

    FactorioInstance(const std::string &name, bool graphical);
    // Blocks the thread to wait for Factorio to stop !
    ~FactorioInstance();

    FactorioInstance(const FactorioInstance&) = delete;
    void operator=(const FactorioInstance&)   = delete;

    // Returns true if Factorio is running
    bool Running();
    // callback is called when the instance is up and ready
    bool Start(const std::function<void(FactorioInstance&)> &callback = nullptr);
    bool Close();
    bool Join(int *exitCode = nullptr);

    // SendRCON should be preferred
    bool WriteStdin(const std::string &data, int &writtenBytes);
    bool SendRCON(const std::string &data, RCONPacketType type = RCON_EXECCOMMAND);
    template<class T, class R = int>
    bool SendRequest(RequestName name, const T &data, std::function<void(FactorioInstance&, R&)> callback = nullptr) {
        static uint32_t index = 0;
        const uint32_t id = index++;
    
        Request<T> request;
        request.id = id;
        request.name = name;
        request.data = data;
        std::string serialized;
        Serialize(request, serialized);
        std::cout << "d; " << request.id << ";" << (int)request.name << std::endl;
        std::cout << "s" << serialized << std::endl;
    
        if (!SendRCON("/request " + serialized)) return false;
    
        if (callback != nullptr) {
            pendingRequests[id] = [this, name, id, callback](const std::string &serialized) {
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

    const std::string name;
    const bool graphical;

    bool printStdout = false;

private:
    static bool initStatic;

    static std::string factorioPath;
    static std::string luaPath;

    std::mutex mutex;

    std::function<void(FactorioInstance&)> startCallback = nullptr;
    std::map<int32_t, std::function<void(const std::string &data)>> pendingRequests;
    
#ifdef _WIN32
    static HANDLE jobObject;

    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION processInfo;
    HANDLE fstdinWrite;
    HANDLE fstdoutRead;
    HANDLE fstdoutEvent;
#elif defined(__linux__)
    pid_t process;
    int returnStatus;

    int fstdinWrite;
    int fstdoutRead;
#else
#error "Only Windows and Linux are supported for now"
#endif
    std::thread fstdoutListener;

    void Clean();
    void StdoutListener();

    void StartRCON();
    void CloseRCON();
    bool ReadPacket(int32_t &id, int32_t &type, char body[4096]);
    const std::string rconAddr = "127.0.0.1";
    const std::string rconPass = "pass";
    const uint16_t rconPort;
    bool rconConnected = false;
#ifdef _WIN32
    SOCKET rconSocket = INVALID_SOCKET;
#else
#error "Only Windows and Linux are supported for now"
#endif
};
}