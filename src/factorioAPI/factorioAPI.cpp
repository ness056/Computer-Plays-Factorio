#include "factorioAPI.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    bool FactorioInstance::s_initStatic = false;

    std::string FactorioInstance::s_factorioPath = "C:\\Users\\louis\\Code Projects\\Computer-Plays-Factorio\\factorio\\bin\\x64\\factorio.exe";

    void FactorioInstance::InitStatic() {
        if (s_initStatic) return;
        s_initStatic = true;

#ifdef _WIN32
        // Init sockets
        WSADATA wsaData;
        int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (r != 0) {
            throw RuntimeErrorFormat("WSAStartup failed: {}", r);
        }
#endif
    }

    bool FactorioInstance::IsFactorioPathValid(const std::string &path, std::string &message) {
        QProcess process;
        process.start(QString::fromStdString(path), { "--version" });

        if (!process.waitForFinished(100)) {
            message = "Invalid path. You must select the Factorio binary. (Debug: Timeout)";
            return false;
        }

        auto out = process.readAllStandardOutput();

        if (!out.startsWith("Version: ")) {
            message = "Invalid path. You must select the Factorio binary. (Debug: Wrong format)";
            return false;
        } else if (out.length() < 12 || out[9] != '2' || out[11] != '0') {
            message = "Wrong Factorio version. Supported versions: 2.0.x";
            return false;
        }

        process.kill();

        return true;
    }
    
    void FactorioInstance::StopAll() {
        for (auto instance : s_instances) {
            instance->Stop();
        }
    }
    
    void FactorioInstance::JoinAll() {
        for (auto instance : s_instances) {
            instance->Join();
        }
    }

    FactorioInstance::FactorioInstance(Type t) : instanceType(t) {
        InitStatic();
        s_instances.insert(this);

        connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &FactorioInstance::StdoutListener);

        connect(&m_process, &QProcess::started, [this]() {
            emit Started(*this);
        });

        connect(&m_process, &QProcess::finished, [this](int exitCode, QProcess::ExitStatus status) {
            CloseRCON();
            for (auto &[id, request] : m_pendingRequests) {
                auto res = ResponseBase{.id = id, .success = false, .error = RequestError::FACTORIO_EXITED};
                const auto json = rfl::json::write(res);
                request(json);
            }
            m_pendingRequests.clear();

            emit Terminated(*this, exitCode, status);
        });

        std::filesystem::create_directory(GetInstanceTempPath());

        if (std::filesystem::exists(GetDataPath() / "factorioConfig.ini")) {
            std::filesystem::copy(GetDataPath() / "factorioConfig.ini", 
                GetConfigPath(), std::filesystem::copy_options::overwrite_existing);
        }
    }

    void FactorioInstance::EditConfig(const std::string &category, const std::string &name, const std::string &value) {
        const auto config = GetConfigPath();
        std::string str;

        if (std::filesystem::exists(config)) {
            std::ostringstream text;
            std::ifstream in(config);
    
            text << in.rdbuf();
            str = text.str();
            in.close();
        }

        size_t pos = str.find(name + "=");
        if (pos != std::string::npos && (str[pos - 1] == '\n' || str[pos - 1] == ' ' || str[pos - 1] == ';')) {
            size_t start = str.rfind('\n', pos) + 1;
            size_t end = str.find('\n', pos);
            str.replace(start, end - start, name + "=" + value);
        } else {
            size_t cat = str.find("[" + category + "]");
            if (cat == std::string::npos) {
                str += "\n[" + category + "]\n" + name + "=" + value + "\n";
            } else {
                size_t start = str.find('\n', cat) + 1;
                if (start == std::string::npos) {
                    str += name + "=" + value + "\n";
                } else {
                    str.insert(start, name + "=" + value + "\n");
                }
            }
        }

        std::ofstream out(config);
        out << str;
    }

    bool FactorioInstance::Start(uint32_t seed, std::string *message) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (Running()) return false;

        std::string m;
        if (!IsFactorioPathValid(s_factorioPath, m)) {
            *message = m;
            return false;
        }

        FindAvailablePort();

        EditConfig("path", "write-data", GetInstanceTempPath().string());
        EditConfig("path", "mods", GetModsPath().string());
        EditConfig("path", "saves", (GetDataPath() / "saves").string());
        EditConfig("other", "local-rcon-socket", "0.0.0.0:" + std::to_string(m_rconPort));
        EditConfig("other", "local-rcon-password", "pass");
        EditConfig("other", "check-updates", "false");
        EditConfig("other", "autosave-interval", "0");
        EditConfig("interface", "show-tips-and-tricks-notifications", "false");
        EditConfig("interface", "enable-recipe-notifications", "false");
        EditConfig("graphics", "full-screen", "false");
        
        QStringList argv = {
            QString::fromStdString(s_factorioPath),
            "--config", QString(GetConfigPath().c_str()),
            "--map-gen-seed", QString::number(seed)
        };

        if (instanceType == HEADLESS) {
            argv.push_back("--start-server-load-scenario");
            argv.push_back("base/freeplay");
            argv.push_back("--rcon-port");
            argv.push_back(QString::number(m_rconPort));
            argv.push_back("--rcon-password");
            argv.push_back("pass");
        } else {
            argv.push_back("--load-scenario");
            argv.push_back("base/freeplay");
        }

        m_process.start(QString::fromStdString(s_factorioPath), argv);
        return true;
    }

    void FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return;

        m_process.kill();
        CloseRCON();
    }

    void FactorioInstance::CheckWord(const QByteArray &previousWord, const QByteArray &word) {
        if (previousWord == "Starting" && word == "RCON") {
            StartRCON();
        }

        else if (word.startsWith("response") && word.length() > 8) {
            bool success;
            int count = word.sliced(8).toInt(&success);
            if (!success) return;

            m_msgByteRemaining = count;
            m_msgBuffer.reserve(count);
            m_msgCallback = [this](const std::string &data) {
                auto rDataless = rfl::json::read<ResponseBase>(data).value();
                if (m_pendingRequests.count(rDataless.id)) {
                    m_pendingRequests[rDataless.id](data);
                    m_pendingRequests.erase(rDataless.id);
                }
            };
        }

        else if (word.startsWith("event") && word.length() > 5) {
            bool success;
            int count = word.sliced(5).toInt(&success);
            if (!success) return;

            m_msgByteRemaining = count;
            m_msgBuffer.reserve(count);
            m_msgCallback = [this](const std::string &data) {
                auto rDataless = rfl::json::read<EventBase>(data).value();
                if (m_eventHandlers.contains(rDataless.name)) {
                    m_eventHandlers[rDataless.name](data);
                } else {
                    Warn("No event handler named {} is registered.", rDataless.name);
                }
            };
        }
    }

    QByteArray::iterator FactorioInstance::ReadJson(
        const QByteArray::iterator start, const QByteArray::iterator end
    ) {
        if (m_msgByteRemaining > 0) {
            const QByteArray bytes(start, end - start);
            if (bytes.size() < m_msgByteRemaining) {
                m_msgBuffer.append(bytes);
                m_msgByteRemaining -= bytes.size();
                return end;
            } else {
                m_msgBuffer.append(bytes, m_msgByteRemaining);
                auto endJson = start + m_msgByteRemaining;

                m_msgBuffer.clear();
                m_msgByteRemaining = 0;

                m_msgCallback(m_msgBuffer.toStdString());
                return endJson;
            }
        }

        return start;
    }

    void FactorioInstance::StdoutListener() {
        auto bytes = m_process.readAllStandardOutput();

        QByteArray previousWord, word;
        auto wordBegin = bytes.begin(), wordEnd = bytes.begin();
        const auto bytesEnd = bytes.end();
        
        while (wordEnd != bytesEnd) {
            wordBegin = ReadJson(wordBegin, bytesEnd);

            wordEnd = std::find_if(wordBegin, bytesEnd, [](const auto &x) {
                return x == ' ' || x == '\n' || x == '\r' || x == '\0';
            });

            word = QByteArray(wordBegin, wordEnd - wordBegin);
            wordBegin = wordEnd + 1;

            CheckWord(previousWord, word);
            previousWord = word;
        }
    }

    void FactorioInstance::RegisterEvent(const std::string &name, const EventCallbackDL &callback) {
        m_eventHandlers[name] = [=](const std::string &json) {
            auto data = rfl::json::read<EventBase>(json).value();
            callback(data);
        };
    }

    void FactorioInstance::FindAvailablePort() {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            throw RuntimeErrorFormat("socket function returned INVALID_SOCKET: {}", LAST_SOCKET_ERROR);
        }
        SOCKADDR_IN addr;
        SOCKLEN len = sizeof(addr);
        memset(&addr, 0, len);
        addr.sin_family = AF_INET;

        if (bind(s, (SOCKADDR*)&addr, len) == SOCKET_ERROR) {
            throw RuntimeErrorFormat("bind function returned SOCKET_ERROR: {}", LAST_SOCKET_ERROR);
        }

        if (getsockname(s, (SOCKADDR*)&addr, &len) == SOCKET_ERROR) {
            throw RuntimeErrorFormat("getsockname function returned SOCKET_ERROR: {}", LAST_SOCKET_ERROR);
        }
        closesocket(s);
        m_rconPort = addr.sin_port;
    }

    void FactorioInstance::StartRCON() {
        if (m_rconConnected) CloseRCON();

        m_rconSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_rconSocket == INVALID_SOCKET) {
            throw RuntimeErrorFormat("socket function returned INVALID_SOCKET: {}", LAST_SOCKET_ERROR);
        }
        SOCKADDR_IN server;
        server.sin_family = AF_INET;
        InetPtonA(AF_INET, m_rconAddr.c_str(), &server.sin_addr.s_addr);
        server.sin_port = htons(m_rconPort);

        const char timeout = static_cast<char>(5000);
        setsockopt(m_rconSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (::connect(m_rconSocket, (const SOCKADDR*)&server, sizeof(server)) == -1) {
            throw RuntimeErrorFormat("connect function returned -1: {}", LAST_SOCKET_ERROR);
        }

        if (!SendRCON("pass", RCON_AUTH)) {
            throw RuntimeErrorFormat("RCON authentification failed.");
        }

        emit Ready(*this);
        m_rconConnected = true;
    }

    void FactorioInstance::CloseRCON() {
        if (!m_rconConnected) return;

        closesocket(m_rconSocket);
        emit Closed(*this);

        m_rconConnected = false;
    }

    bool FactorioInstance::SendRCON(const std::string &data, RCONPacketType packetType) const {
        if (!m_rconConnected && packetType != RCON_AUTH) return false;

        static int32_t index = 0;
        int32_t id = index++;

        size_t dataSize = data.size();
        int32_t size = (int32_t)dataSize + 10;

        char *packet = new char[size + 4];
        std::memcpy(packet,                     &size,       sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t),   &id,         sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*2, &packetType, sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*3, data.data(), dataSize);
        packet[size + 2] = '\0';
        packet[size + 3] = '\0';

        if (send(m_rconSocket, packet, size + 4, 0) < 0) {
            throw RuntimeErrorFormat("Failed to send RCON packet: {}", LAST_SOCKET_ERROR);
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

    bool FactorioInstance::ReadPacket(int32_t &id, int32_t &packetType, char body[4096]) const {
        int32_t responseSize;
        if (recv(m_rconSocket, (char*)&responseSize, 4, 0) == -1) return false;
        if (responseSize < 10) return false;

        if (recv(m_rconSocket, (char*)&id, 4, 0) == -1) return false;
        if (recv(m_rconSocket, (char*)&packetType, 4, 0) == -1) return false;
        if (recv(m_rconSocket, body, responseSize - 2, 0) == -1) return false;

        return true;
    }

    bool FactorioInstance::RequestPrivate(const std::string &name, uint32_t *id) const {
        RequestDataBase r;
        r.id = s_id++;
        r.name = name;
        if (id) *id = r.id;
        const auto json = rfl::json::write(r);
        return SendRCON("/request " + json);
    }

    std::future<ResponseBase> FactorioInstance::AddResponseBase(uint32_t id) {
        auto promise = std::make_shared<std::promise<ResponseBase>>();
        m_pendingRequests[id] = [promise](const std::string &json) {
            auto data = rfl::json::read<ResponseBase>(json).value();
            promise->set_value(data);
        };
        return promise->get_future();
    }

    std::future<ResponseBase> FactorioInstance::RequestNoRes(const std::string &name) {
        uint32_t id;
        if (!RequestPrivate(name, &id)) {
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
}