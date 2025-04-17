#include "factorioAPI.hpp"

#include <format>
#include <cassert>
#include <cstring>

namespace ComputerPlaysFactorio {

bool FactorioInstance::initStatic = false;

std::string FactorioInstance::factorioPath = "C:\\Users\\louis\\Code Projects\\Computer-Plays-Factorio\\factorio\\bin\\x64\\factorio.exe";
std::string FactorioInstance::luaPath = "..\\lua";

#ifdef _WIN32
HANDLE FactorioInstance::jobObject = nullptr;
#endif

void FactorioInstance::InitStatic() {
    assert(!initStatic && "FactorioInstance::InitStatic was already been called.");

#ifdef _WIN32
    jobObject = CreateJobObjectA(nullptr, nullptr);
    if (!jobObject) {
        std::cerr << "CreateJobObjectA" << std::endl;
        exit(1);
    }
    
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(jobObject, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        std::cerr << GetLastError() << std::endl;
        std::cerr << "SetInformationJobObject" << std::endl;
        exit(1);
    }

    // Init sockets
    WSADATA wsaData;
    int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (r != 0) {
        std::cerr << "WSAStartup: " << r << std::endl;
        exit(1);
    }
#endif

    initStatic = true;
}

FactorioInstance::FactorioInstance(const std::string &n, bool g) : name(n), graphical(g), rconPort(27015) {
    assert(initStatic && "FactorioInstance::InitStatic must be called before constructing any FactorioInstance.");

#ifdef _WIN32
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(STARTUPINFOA);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;

    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, true };
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) {
        std::cerr << "stdin CreatePipe" << std::endl;
        exit(1);
    };

    if (!SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0)) {
        std::cerr << "stdin SetHandleInformation" << std::endl;
        exit(1);
    }
    
    fstdinWrite = wr;
    startupInfo.hStdInput = rd;

    static long index = 0;
    const long i = index++;
    const std::string pipeName = std::format("\\\\.\\pipe\\factorio_api_cpp.{}.{}.{}",
        GetCurrentProcessId(), GetCurrentThreadId(), i);
    const char *c_name = pipeName.c_str();

    rd = CreateNamedPipeA(c_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
    if (rd == INVALID_HANDLE_VALUE) {
        std::cerr << GetLastError() << std::endl;
        std::cerr << "CreateNamedPipeA" << std::endl;
        exit(1);
    }

    wr = CreateFileA(c_name, GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (wr == INVALID_HANDLE_VALUE) {
        std::cerr << "CreateFileA" << std::endl;
        exit(1);
    }

    fstdoutEvent = CreateEventA(&sa, 1, 1, NULL);
    if (!fstdoutEvent) {
        std::cerr << "CreateEventA" << std::endl;
        exit(1);
    }

    fstdoutRead = rd;
    startupInfo.hStdOutput = wr;
    startupInfo.hStdError = wr;
#elif defined(__linux__)
    int stdinfd[2];
    int stdoutfd[2];

    if (pipe(stdinfd) != 0 || pipe(stdoutfd) != 0) {
        std::cerr << "pipe" << std::endl;
        exit(1);
    }
#else
#error "Only Windows and Linux are supported for now"
#endif
}

FactorioInstance::~FactorioInstance() {
    std::unique_lock<std::mutex> lock(mutex);
    Close();
    Join();
    Clean();
    fstdoutListener.join();
}

bool FactorioInstance::Running() {
#ifdef _WIN32
    DWORD exitCode;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);
    return exitCode == STILL_ACTIVE;
#else
#error "Only Windows and Linux are supported for now"
#endif
}

bool FactorioInstance::Start(const std::function<void(FactorioInstance&)> &callback) {
    std::unique_lock<std::mutex> lock(mutex);
    if (Running()) return false;

    startCallback = callback;
    
    std::string command = factorioPath;
    command += " --mod-directory " + luaPath;
    command += " --start-server-load-scenario base/freeplay --rcon-port 27015 --rcon-password " + rconPass;

#ifdef _WIN32
    if (!CreateProcessA(
        NULL,
        command.data(),
        NULL,
        NULL,
        true,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    )) {
        std::cerr << "CreateProcessA" << std::endl;
        exit(1);
    }

    if (!AssignProcessToJobObject(jobObject, processInfo.hProcess)) {
        std::cerr << "AssignProcessToJobObject" << std::endl;
        Close();
        exit(1);
    }

    if (!ResumeThread(processInfo.hThread)) {
        std::cerr << "ResumeThread" << std::endl;
        Close();
        exit(1);
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    fstdoutListener = std::thread(&FactorioInstance::StdoutListener, this);
    fstdoutListener.detach();

    return true;
}

bool FactorioInstance::Close() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!Running()) return false;

#ifdef _WIN32
    if (!TerminateProcess(processInfo.hProcess, 0)) {
        std::cerr << "TerminateProcess" << std::endl;
        exit(1);
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    fstdoutListener.join();
    return true;
}

bool FactorioInstance::Join(int *exitCode) {
#ifdef _WIN32
    WaitForSingleObject(processInfo.hProcess, 0xFFFFFFFF);

    if (exitCode) {
        if (!GetExitCodeProcess(processInfo.hProcess, reinterpret_cast<LPDWORD>(exitCode))) {
            return false;
        }
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    return true;
}

bool FactorioInstance::WriteStdin(const std::string &data, int &writtenBytes) {
    if (!Running() || !fstdinWrite) return false;

#ifdef _WIN32
    DWORD wb;
    WriteFile(fstdinWrite, data.data(), (DWORD)data.length(), &wb, NULL);
    writtenBytes = (int)wb;
    return true;
#else
#error "Only Windows and Linux are supported for now"
#endif
}

bool FactorioInstance::SendRCON(const std::string &data, RCONPacketType type) {
    std::cout << "send " << data << std::endl;
    if (!rconConnected && type != RCON_AUTH) return false;

    static int32_t index = 0;
    int32_t id = index++;

    size_t dataSize = data.size();
    int32_t size = (int32_t)dataSize + 10;
    if (size > 4096) {
        std::cerr << "need to implement multi-packet request" << std::endl;
        exit(1);
    }

    char *packet = new char[size + 4];
    std::memcpy(packet,                     &size,       sizeof(int32_t));
    std::memcpy(packet + sizeof(int32_t),   &id,         sizeof(int32_t));
    std::memcpy(packet + sizeof(int32_t)*2, &type,       sizeof(int32_t));
    std::memcpy(packet + sizeof(int32_t)*3, data.data(), dataSize);
    packet[size + 2] = '\0';
    packet[size + 3] = '\0';

    if (send(rconSocket, packet, size + 4, 0) < 0) {
        std::cerr << "send" << std::endl;
        exit(1);
    }

    delete[] packet;

    if (type != RCON_AUTH) return true;

    for (int i = 0; i < 512; i++) {
        int32_t resId, resType;
        char resBody[4096];
        if (!ReadPacket(resId, resType, resBody)) return false;

        if (resId == id && resType == RCON_AUTH_RESPONSE) return true;
    }

    return false;
}

void FactorioInstance::Clean() {
    CloseRCON();
#ifdef _WIN32
    CloseHandle(startupInfo.hStdInput);
    CloseHandle(startupInfo.hStdOutput);
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    CloseHandle(fstdinWrite);
    CloseHandle(fstdoutRead);
    CloseHandle(fstdoutEvent);
#else
#error "Only Windows and Linux are supported for now"
#endif
}

void FactorioInstance::StdoutListener() {
    while (Running()) {
        OVERLAPPED ol = {0, 0, {{0, 0}}, NULL};
        ol.hEvent = fstdoutEvent;
        DWORD readBytes;
        char byte;
        std::string line;
        while (true) {
            if (!ReadFile(fstdoutRead, &byte, 1, &readBytes, &ol) && GetLastError() == ERROR_IO_PENDING) {
                if (!GetOverlappedResult(fstdoutRead, &ol, &readBytes, 1)) {
                    DWORD e = GetLastError();
                    if (e != ERROR_IO_INCOMPLETE && e != ERROR_HANDLE_EOF) {
                        std::cerr << "ReadFile" << std::endl;
                        exit(1);
                    }
                }
            }

            if (byte == '\n') break;
            else line += byte;
        }
    
        std::cout << line << std::endl;
        if (line.starts_with("response")) {
            std::cout << "response" << std::endl;
        }

        else if (line.starts_with("event")) {
            std::cout << "event" << std::endl;
        }

        else if (line.find("Starting RCON") != std::string::npos) {
            StartRCON();
            if (startCallback) startCallback(*this);
        }
    }
}

void FactorioInstance::StartRCON() {
    if (rconConnected) return;

    rconSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (rconSocket == INVALID_SOCKET) {
        std::cerr << "socket" << std::endl;
        exit(1);
    }

    SOCKADDR_IN server;
    server.sin_family = AF_INET;
    InetPtonA(AF_INET, rconAddr.c_str(), &server.sin_addr.s_addr);
    server.sin_port = htons(rconPort);

    const char timeout = static_cast<char>(5000);
    setsockopt(rconSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(rconSocket, (const SOCKADDR*)&server, sizeof(server)) == -1) {
        std::cerr << "connect" << std::endl;
        exit(1);
    }

    if (!SendRCON(rconPass, RCON_AUTH)) {
        std::cerr << "auth" << std::endl;
        exit(1);
    }

    rconConnected = true;
}

void FactorioInstance::CloseRCON() {
    if (!rconConnected) return;

    closesocket(rconSocket);

    rconConnected = false;
}

bool FactorioInstance::ReadPacket(int32_t &id, int32_t &type, char body[4096]) {
    std::unique_lock<std::mutex> lock(mutex);
    int32_t responseSize;
    if (recv(rconSocket, (char*)&responseSize, 4, 0) == -1) return false;
    if (responseSize < 10) return false;

    if (recv(rconSocket, (char*)&  id,                4, 0) == -1) return false;
    if (recv(rconSocket, (char*)&type,                4, 0) == -1) return false;
    if (recv(rconSocket,         body, responseSize - 2, 0) == -1) return false;

    return true;
}
}