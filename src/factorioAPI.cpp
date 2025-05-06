#include "factorioAPI.hpp"

#include <algorithm>
#include <format>
#include <cassert>
#include <cstring>

namespace ComputerPlaysFactorio {

    bool FactorioInstance::s_initStatic = false;

    std::string FactorioInstance::s_factorioPath = "C:\\Users\\louis\\Code Projects\\Computer-Plays-Factorio\\factorio\\bin\\x64\\factorio.exe";
    std::string FactorioInstance::s_luaPath;

    void FactorioInstance::InitStatic() {
        assert(!s_initStatic && "FactorioInstance::InitStatic was already been called.");

    #ifdef _WIN32
        // Init sockets
        WSADATA wsaData;
        int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (r != 0) {
            g_error << "WSAStartup: " << r << std::endl;
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
        FactorioInstance instance("PathChecker", HEADLESS, false);
        instance.m_process.start(QString::fromStdString(path), { "--version" });

        if (!instance.m_process.waitForFinished(100)) {
            message = "Invalid path. You must select the Factorio binary. (Debug: Timeout)";
            return false;
        }

        auto out = instance.m_process.readAllStandardOutput();

        if (!out.startsWith("Version: ")) {
            message = "Invalid path. You must select the Factorio binary. (Debug: Wrong format)";
            return false;
        } else if (out.length() < 12 || out[9] != '2' || out[11] != '0') {
            message = "Wrong Factorio version. Supported versions: 2.0.x";
            return false;
        }

        return true;
    }

    FactorioInstance::FactorioInstance(const std::string &n, Type t, bool listener) : name(n), type(t), m_rconPort(27015) {
        assert(s_initStatic && "FactorioInstance::InitStatic must be called before constructing any FactorioInstance.");

        if (listener) {
            connect(&m_process, &QProcess::readyReadStandardOutput, this, &FactorioInstance::StdoutListener);
        }

        connect(&m_process, &QProcess::started, [this]() {
            emit Started(*this);
        });

        connect(&m_process, &QProcess::finished, [this](int exitCode, QProcess::ExitStatus status) {
            emit Terminated(*this, exitCode, status);
        });
    }

    bool FactorioInstance::Start(std::string *message) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (Running()) return false;

        std::string m;
        if (!IsFactorioPathValid(s_factorioPath, m)) {
            *message = m;
            return false;
        }
        
        QStringList argv = {
            QString::fromStdString(s_factorioPath),
            "--mod-directory",
            QString::fromStdString(s_luaPath),
            "--rcon-port",
            "27015",
            "--rcon-password",
            QString::fromStdString(m_rconPass)
        };

        if (type == HEADLESS) {
            argv.push_back("--start-server-load-scenario");
            argv.push_back("base/freeplay");
        }

        m_process.start(QString::fromStdString(s_factorioPath), argv);
        return true;
    }

    bool FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return false;

        m_process.kill();
        CloseRCON();

        return true;
    }

    void FactorioInstance::StdoutListener() {
        auto bytes = m_process.readAllStandardOutput();
        std::unique_lock<std::mutex> lock(m_stdoutListenerMutex);
        if (printStdout) std::cout << bytes.toStdString() << std::flush;

        static QByteArray prevWord = "";

        auto pos = bytes.begin();
        bool exit = false;
        while (!exit) {
            auto it = std::find_if(pos, bytes.end(), [](const auto &x) {
                return x == ' ' || x == '\n' || x == '\r' || x == '\0';
            });
            if (it == bytes.end()) exit = true;
            QByteArray word(pos, it - pos);
            pos = it + 1;

            if (prevWord == "Starting" && word == "RCON" && !m_rconConnected) {
                StartRCON();
            }

            else if (word.startsWith("response") && word.length() > 8) {
                uint32_t size = word.sliced(8).toInt();
                auto end = bytes.end();
                if (end - pos - 1 < size) {
                    g_error << "Response too big: size: " << size << " data: " << std::string(pos, end) << std::endl;
                    ::exit(1);
                }

                auto data = QJsonDocument::fromJson(QByteArray(pos, size));
                if (!data.isObject()) {
                    g_error << "Response is not a json object: " << std::string(pos, size) << std::endl;
                }

                ResponseDataless rDataless;
                FromJson(data.object(), rDataless);
                if (m_pendingRequests.count(rDataless.id)) {
                    m_pendingRequests[rDataless.id](data.object());
                    m_pendingRequests.erase(rDataless.id);
                }

                pos += size;
            }

            // else if (word == "event" && word.length() > 5) {
            //     g_warning << "event TODO" << std::endl;
            // }

            prevWord = word;
        }
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
            g_error << "socket" << std::endl;
            exit(1);
        }
        SOCKADDR_IN server;
        server.sin_family = AF_INET;
        InetPtonA(AF_INET, m_rconAddr.c_str(), &server.sin_addr.s_addr);
        server.sin_port = htons(m_rconPort);

        const char timeout = static_cast<char>(5000);
        setsockopt(m_rconSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (::connect(m_rconSocket, (const SOCKADDR*)&server, sizeof(server)) == -1) {
            g_error << "connect" << std::endl;
            exit(1);
        }

        if (!SendRCON(m_rconPass, RCON_AUTH)) {
            g_error << "auth" << std::endl;
            exit(1);
        }

        emit Ready(*this);
        m_rconConnected = true;
    }

    void FactorioInstance::CloseRCON() {
        if (!m_rconConnected) return;

    #ifdef __linux__
    #define closesocket close
    #endif

        closesocket(m_rconSocket);
        emit Closed(*this);

        m_rconConnected = false;
    }

    bool FactorioInstance::SendRCON(const std::string &data, RCONPacketType packetType) {
        if (!m_rconConnected && packetType != RCON_AUTH) return false;

        static int32_t index = 0;
        int32_t id = index++;

        size_t dataSize = data.size();
        int32_t size = (int32_t)dataSize + 10;
        if (size > 4096) {
            g_error << "need to implement multi-packet request" << std::endl;
            exit(1);
        }

        char *packet = new char[size + 4];
        std::memcpy(packet,                     &size,       sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t),   &id,         sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*2, &packetType, sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*3, data.data(), dataSize);
        packet[size + 2] = '\0';
        packet[size + 3] = '\0';

        if (send(m_rconSocket, packet, size + 4, 0) < 0) {
            g_error << "send" << std::endl;
            exit(1);
        }

        delete[] packet;

        if (packetType != RCON_AUTH) return true;

        for (int i = 0; i < 512; i++) {
            int32_t resId, resType;
            char resBody[4096];
            if (!ReadPacket(resId, resType, resBody)) return false;

            if (resId == id && resType == RCON_AUTH_RESPONSE) return true;
        }

        return false;
    }

    bool FactorioInstance::ReadPacket(int32_t &id, int32_t &packetType, char body[4096]) {
        std::unique_lock<std::mutex> lock(m_mutex);
        int32_t responseSize;
        if (recv(m_rconSocket, (char*)&responseSize, 4, 0) == -1) return false;
        if (responseSize < 10) return false;

        if (recv(m_rconSocket, (char*)&id, 4, 0) == -1) return false;
        if (recv(m_rconSocket, (char*)&packetType, 4, 0) == -1) return false;
        if (recv(m_rconSocket, body, responseSize - 2, 0) == -1) return false;

        return true;
    }
}