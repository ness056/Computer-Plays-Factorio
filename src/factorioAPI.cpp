#include "factorioAPI.hpp"

#include <algorithm>
#include <format>
#include <cassert>
#include <cstring>

#include "thread.hpp"

#ifdef __linux__
extern char **environ;
#endif

namespace ComputerPlaysFactorio {

bool FactorioInstance::s_initStatic = false;

std::string FactorioInstance::s_factorioPath = "C:\\Users\\louis\\Code Projects\\Computer-Plays-Factorio\\factorio\\bin\\x64\\factorio.exe";
std::string FactorioInstance::s_luaPath;

#ifdef _WIN32
HANDLE FactorioInstance::s_jobObject = nullptr;
#endif

void FactorioInstance::InitStatic() {
    assert(!s_initStatic && "FactorioInstance::InitStatic was already been called.");

#ifdef _WIN32
    s_jobObject = CreateJobObjectA(nullptr, nullptr);
    if (!s_jobObject) {
        std::cerr << "CreateJobObjectA" << std::endl;
        exit(1);
    }
    
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(s_jobObject, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
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

    char path[MAX_PATH + 1];
    GetModuleFileNameA(nullptr, path, MAX_PATH + 1);
    s_luaPath = std::filesystem::path(path).parent_path().parent_path().string();
    s_luaPath += "\\lua";
#elif defined(__linux__)
    s_luaPath = std::filesystem::canonical("/proc/self/exe").parent_path();
    s_luaPath += "/lua";
#endif

    s_initStatic = true;
}

bool FactorioInstance::IsFactorioPathValid(const std::string &path, std::string &message) {
    std::string path_(path);

    FactorioInstance instance("PathChecker", false);
    if (instance.StartPrivate({
        path_.data(),
        (char*)"--version",
        nullptr
    })) {
        message = "Invalid path. You must select the Factorio binary.";
        return false;
    }

    char buffer[12];
    auto readBytes = instance.StdoutRead(buffer, 12);

    if (std::string(buffer, std::min(readBytes, 9)) != "Version: ") {
        message = "Invalid path. You must select the Factorio binary.";
        return false;
    } else if (buffer[9] != '2' || buffer[11] != '0') {
        message = "Wrong Factorio version. Supported versions: 2.0.x";
        return false;
    }

    return true;
}

FactorioInstance::FactorioInstance(const std::string &n, bool g) : name(n), graphical(g), m_rconPort(27015) {
    assert(s_initStatic && "FactorioInstance::InitStatic must be called before constructing any FactorioInstance.");
}

FactorioInstance::~FactorioInstance() {
    Stop();
    Join();
    Clean();
}

bool FactorioInstance::Running() {
#ifdef _WIN32
    GetExitCodeProcess(m_processInfo.hProcess, reinterpret_cast<LPDWORD>(&m_exitCode));
    return m_exitCode == STILL_ACTIVE;
#elif defined(__linux__)
    int status;
    if (waitpid(m_process, &status, WNOHANG) == 0) return true;
    else {
        if (WIFEXITED(status)) {
            m_exitCode = WEXITSTATUS(status);
        } else {
            m_exitCode = EXIT_FAILURE;
        }
        return false;
    }

#else
#error "Only Windows and Linux are supported for now"
#endif
}

bool FactorioInstance::Start(const std::function<void(FactorioInstance&)> &callback, std::string *message) {
    std::string m;
    if (!IsFactorioPathValid(s_factorioPath, m)) {
        *message = m;
        return false;
    }

    m_startCallback = callback;
    
    std::vector<char*> argv = {
        s_factorioPath.data(),
        (char*)"--mod-directory",
        s_luaPath.data(),
        (char*)"--rcon-port",
        (char*)"27015",
        (char*)"--rcon-password",
        m_rconPass.data()
    };

    if (!graphical) {
        argv.push_back((char*)"--start-server-load-scenario");
        argv.push_back((char*)"base/freeplay");
    }

    argv.push_back(nullptr);

    int err = StartPrivate(argv);

    if (err) {
        *message = "Error: ";
        *message += std::to_string(err);
    } else {
        m_fstdoutListener = std::thread(&FactorioInstance::StdoutListener, this);
        m_fstdoutListener.detach();
    }

    return !err;
}

int FactorioInstance::StartPrivate(const std::vector<char*> &argv) {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (Running()) return 0;
    if (!m_cleaned) Clean();
    m_cleaned = false;

#ifdef _WIN32
    ZeroMemory(&m_startupInfo, sizeof(m_startupInfo));
    m_startupInfo.cb = sizeof(STARTUPINFOA);
    m_startupInfo.dwFlags = STARTF_USESTDHANDLES;

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

    m_fstdinWrite = wr;
    m_startupInfo.hStdInput = rd;

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

    m_fstdoutEvent = CreateEventA(&sa, 1, 1, NULL);
    if (!m_fstdoutEvent) {
        std::cerr << "CreateEventA" << std::endl;
        exit(1);
    }
    m_fstdoutOl = {0, 0, {{0, 0}}, NULL};
    m_fstdoutOl.hEvent = m_fstdoutEvent;

    m_fstdoutRead = rd;
    m_startupInfo.hStdOutput = wr;
    m_startupInfo.hStdError = wr;

    std::string command;
    for (const auto &str : argv) {
        if (str == nullptr) break;
        command += str;
        command += ' ';
    }

    if (!CreateProcessA(
        NULL,
        command.data(),
        NULL,
        NULL,
        true,
        CREATE_SUSPENDED,
        NULL,
        NULL,
        &m_startupInfo,
        &m_processInfo
    )) {
        auto err = GetLastError();
        Clean();
        return err;
    }

    if (!AssignProcessToJobObject(s_jobObject, m_processInfo.hProcess)) {
        std::cerr << "AssignProcessToJobObject" << std::endl;
        lock.unlock();
        Stop();
        exit(1);
    }

    if (!ResumeThread(m_processInfo.hThread)) {
        std::cerr << "ResumeThread" << std::endl;
        lock.unlock();
        Stop();
        exit(1);
    }
#elif defined(__linux__)
    int stdinfd[2];
    int stdoutfd[2];

    if (pipe(stdinfd) != 0 || pipe(stdoutfd) != 0) {
        std::cerr << "pipe" << std::endl;
        exit(1);
    }

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions)) {
        std::cerr << "posix_spawn_file_actions_init" << std::endl;
        exit(1);
    }

    if (posix_spawn_file_actions_addclose(&actions, stdinfd[1]) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        std::cerr << "stdin posix_spawn_file_actions_addclose" << std::endl;
        exit(1);
    }

    if (posix_spawn_file_actions_adddup2(&actions, stdinfd[0], STDIN_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        std::cerr << "stdin posix_spawn_file_actions_adddup2" << std::endl;
        exit(1);
    }

    if (posix_spawn_file_actions_addclose(&actions, stdoutfd[0]) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        std::cerr << "stdout posix_spawn_file_actions_addclose" << std::endl;
        exit(1);
    }

    if (posix_spawn_file_actions_adddup2(&actions, stdoutfd[1], STDOUT_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        std::cerr << "stdout posix_spawn_file_actions_adddup2" << std::endl;
        exit(1);
    }

    if (posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO) != 0) {
        posix_spawn_file_actions_destroy(&actions);
        std::cerr << "stderr posix_spawn_file_actions_adddup2" << std::endl;
        exit(1);
    }

    int err = posix_spawnp(
        &m_process,
        argv[0],
        &actions,
        nullptr,
        argv.data(),
        environ
    );

    m_fstdinWrite = stdinfd[1];
    m_fstdoutRead = stdoutfd[0];
    close(stdinfd[0]);
    close(stdoutfd[1]);
    posix_spawn_file_actions_destroy(&actions);

    if (err != 0) {
        Clean();
        return err;
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    return 0;
}

bool FactorioInstance::Stop() {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (!Running()) return false;

#ifdef _WIN32
    if (!TerminateProcess(m_processInfo.hProcess, 0)) {
        std::cerr << "TerminateProcess" << std::endl;
        exit(1);
    }
#elif defined(__linux__)
    if (kill(m_process, 9)) {
        std::cerr << "kill" << std::endl;
        exit(1);
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    CloseRCON();

    return true;
}

void FactorioInstance::Clean() {
    if (m_cleaned) return;
#ifdef _WIN32
    CloseHandle(m_startupInfo.hStdInput);
    CloseHandle(m_startupInfo.hStdOutput);
    CloseHandle(m_processInfo.hProcess);
    CloseHandle(m_processInfo.hThread);
    CloseHandle(m_fstdinWrite);
    CloseHandle(m_fstdoutRead);
    CloseHandle(m_fstdoutEvent);
#elif defined(__linux__)
    close(m_fstdinWrite);
    close(m_fstdoutRead);
#else
#error "Only Windows and Linux are supported for now"
#endif
    m_cleaned = true;
}

bool FactorioInstance::Join() {
#ifdef _WIN32
    WaitForSingleObject(m_processInfo.hProcess, 0xFFFFFFFF);

    if (!GetExitCodeProcess(m_processInfo.hProcess, reinterpret_cast<LPDWORD>(&m_exitCode))) {
        return false;
    }
#elif defined(__linux__)
    int status;

    if (waitpid(m_process, &status, 0) != m_process) return false;

    if (WIFEXITED(status)) {
        m_exitCode = WEXITSTATUS(status);
    } else {
        m_exitCode = EXIT_FAILURE;
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    if (m_fstdoutListener.joinable()) m_fstdoutListener.join();

    return true;
}

bool FactorioInstance::WriteStdin(const std::string &data, int *writtenBytes) {
    if (!Running() || !m_fstdinWrite) return false;

#ifdef _WIN32
    DWORD wb;
    WriteFile(m_fstdinWrite, data.data(), (DWORD)data.length(), &wb, NULL);
#elif defined(__linux__)
    int wb = write(m_fstdinWrite, data.data(), data.length());
#else
#error "Only Windows and Linux are supported for now"
#endif

    if (writtenBytes) *writtenBytes = wb;
    return true;
}

bool FactorioInstance::SendRCON(const std::string &data, RCONPacketType type) {
    if (!m_rconConnected && type != RCON_AUTH) return false;

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

    if (send(m_rconSocket, packet, size + 4, 0) < 0) {
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

int FactorioInstance::StdoutRead(char *buffer, int size) {
    int32_t readBytes;

#ifdef _WIN32
    DWORD rb;
    if (!ReadFile(m_fstdoutRead, buffer, size, &rb, &m_fstdoutOl) && GetLastError() == ERROR_IO_PENDING) {
        if (!GetOverlappedResult(m_fstdoutRead, &m_fstdoutOl, &rb, 1)) {
            DWORD e = GetLastError();
            if (e == ERROR_IO_INCOMPLETE || e == ERROR_HANDLE_EOF) {
                return 0;
            } else {
                std::cerr << "ReadFile: " << e << std::endl;
                exit(1);
            }
        }
    }
    readBytes = (uint32_t)rb;
#elif defined(__linux__)
    readBytes = read(m_fstdoutRead, buffer, size);
    if (readBytes == -1) {
        std::cerr << "read" << std::endl;
        exit(1);
    }
#else
#error "Only Windows and Linux are supported for now"
#endif

    if (printStdout) std::cout << std::string(buffer, readBytes);
    return readBytes;
}

void FactorioInstance::StdoutListener() {
    std::string prevWord = "";
    while (Running()) {
        std::string word = "";
        while (true) {
            char byte;
            if (StdoutRead(&byte, 1)) {
                if (byte == ' ') break;
                else word += byte;
            } else {
                break;
            }
        }
    
        if (prevWord == "Starting" && word == "RCON" && !m_rconConnected) {
            StartRCON();
            if (m_startCallback) {
                auto f = m_startCallback;
                m_startCallback = nullptr;

                ThreadPool::QueueJob([=, this] {
                    f(*this);
                });
            }
        }

        else if (word.rfind("response", 0) == 0 && word.length() > 8) {
            uint32_t size = std::stoul(word.substr(8)), read = 0;
            char *buffer = new char[size];

            while (size > read) {
                read += StdoutRead(buffer + read, size);
            }
            size_t i = 0;
            std::string message(buffer, size);
            RequestDataless rDataless;

            Deserialize(message, i, rDataless);
            if (m_pendingRequests.count(rDataless.id)) {
                auto handle = m_pendingRequests[rDataless.id];
                m_pendingRequests.erase(rDataless.id);

                ThreadPool::QueueJob([=, this] {
                    handle(message);
                });
            }

            delete[] buffer;
        }

        else if (word.rfind("event", 0) == 0 && word.length() > 5) {
            std::cout << "event TODO" << std::endl;
        }

        prevWord = word;
    }

    if (m_terminateCallback) m_terminateCallback(*this, m_exitCode);
}

void FactorioInstance::StartRCON() {
    if (m_rconConnected) return;

#ifdef __linux__
#define INVALID_SOCKET -1
#define SOCKADDR_IN sockaddr_in
#define SOCKADDR sockaddr
#define InetPtonA inet_pton
#endif

    m_rconSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_rconSocket == INVALID_SOCKET) {
        std::cerr << "socket" << std::endl;
        exit(1);
    }
    SOCKADDR_IN server;
    server.sin_family = AF_INET;
    InetPtonA(AF_INET, m_rconAddr.c_str(), &server.sin_addr.s_addr);
    server.sin_port = htons(m_rconPort);

    const char timeout = static_cast<char>(5000);
    setsockopt(m_rconSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(m_rconSocket, (const SOCKADDR*)&server, sizeof(server)) == -1) {
        std::cerr << "connect" << std::endl;
        exit(1);
    }

    if (!SendRCON(m_rconPass, RCON_AUTH)) {
        std::cerr << "auth" << std::endl;
        exit(1);
    }

    m_rconConnected = true;
}

void FactorioInstance::CloseRCON() {
    if (!m_rconConnected) return;

#ifdef __linux__
#define closesocket close
#endif

    closesocket(m_rconSocket);

    m_rconConnected = false;
}

bool FactorioInstance::ReadPacket(int32_t &id, int32_t &type, char body[4096]) {
    std::unique_lock<std::mutex> lock(m_mutex);
    int32_t responseSize;
    if (recv(m_rconSocket, (char*)&responseSize, 4, 0) == -1) return false;
    if (responseSize < 10) return false;

    if (recv(m_rconSocket, (char*)&  id,                4, 0) == -1) return false;
    if (recv(m_rconSocket, (char*)&type,                4, 0) == -1) return false;
    if (recv(m_rconSocket,         body, responseSize - 2, 0) == -1) return false;

    return true;
}
}