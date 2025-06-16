#include "factorioAPI.hpp"

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
            g_error << "WSAStartup: " << r << std::endl;
            exit(1);
        }
#elif defined(__linux__)
#endif
    }

    bool FactorioInstance::IsFactorioPathValid(const std::string &path, std::string &message) {
        static FactorioInstance instance(HEADLESS);
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

        instance.Stop();
        instance.Join();

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

        FindAvailablePort();

        RegisterEvents();

        connect(&m_process, &QProcess::started, [this]() {
            emit Started(*this);
        });

        connect(&m_process, &QProcess::finished, [this](int exitCode, QProcess::ExitStatus status) {
            m_stdoutListenerExit = true;
            CloseRCON();
            for (auto &[id, request] : m_pendingRequests) {
                auto res = ResponseBase{.id = id, .success = false, .error = FACTORIO_EXITED};
                auto json = ToJson(res);
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
    
    void FactorioInstance::RegisterEvents() {
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

        EditConfig("path", "write-data", GetInstanceTempPath().string());
        EditConfig("path", "mods", GetLuaPath().string());
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

        m_stdoutListenerExit = false;
        m_stdoutListener = std::thread(&FactorioInstance::StdoutListener, this);
        m_stdoutListener.detach();
        return true;
    }

    void FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return;

        m_stdoutListenerExit = true;
        m_process.kill();
        CloseRCON();
    }

    QJsonObject FactorioInstance::ReadJson(int count, bool &success) {
        QJsonDocument json;
        success = false;

        char *buffer = new char[count];
        uint64_t size = 0;
        while (size < count && !m_stdoutListenerExit) {
            if (!m_process.waitForReadyRead(500)) continue;

            uint64_t readCount = m_process.read(buffer + size, count - size);
            if (readCount == -1) goto returnLabel;

            size += readCount;
        }

        if (m_stdoutListenerExit) goto returnLabel;

        json = QJsonDocument::fromJson(QByteArray(buffer, -1));
        if (!json.isObject()) {
            g_error << "Data is not a json object: " << buffer << std::endl;
            goto returnLabel;
        }

        success = true;
returnLabel:
        delete[] buffer;
        return json.object();
    }

    void FactorioInstance::CheckWord(const std::string &previousWord, const std::string &word) {
        if (previousWord == "Starting" && word == "RCON") {
            StartRCON();
        }

        else if (word.starts_with("response") && word.length() > 8) {
            int count = std::stoi(word.substr(8));
            bool success;
            auto data = ReadJson(count, success);
            if (!success) return;

            ResponseBase rDataless;
            FromJson(data, rDataless);
            if (m_pendingRequests.count(rDataless.id)) {
                m_pendingRequests[rDataless.id](data);
                m_pendingRequests.erase(rDataless.id);
            }
        }

        else if (word.starts_with("event") && word.length() > 5) {
            int count = std::stoi(word.substr(5));
            bool success;
            auto data = ReadJson(count, success);
            if (!success) return;

            EventBase rDataless;
            FromJson(data, rDataless);
            if (m_eventHandlers.contains(rDataless.name)) {
                m_eventHandlers[rDataless.name](data);
            } else {
                g_warring << "No event handler named " << rDataless.name << " is registered." << std::endl;
            }
        }
    }

    void FactorioInstance::StdoutListener() {
        char c;
        std::string word, previousWord;
        while (!m_stdoutListenerExit) {
            if (!m_process.waitForReadyRead(500)) continue;
            g_info << m_process.pos() << std::endl;
            if (!m_process.getChar(&c)) continue;

            if (c != ' ' && c != '\n' && c != '\r' && c != '\0') {
                word += c;
            } else {
                CheckWord(previousWord, word);

                previousWord = std::move(word);
                word.clear();
            }
        }
    }

    void FactorioInstance::RegisterEvent(const std::string &name, EventCallbackDL callback) {
        assert(callback);
        m_eventHandlers[name] = [=](const QJsonObject &json) {
            EventBase data;
            FromJson(json, data);
            ThreadPool::QueueJob([data, callback] {
                callback(data);
            });
        };
    }

    void FactorioInstance::FindAvailablePort() {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            g_error << "socket" << std::endl;
            exit(1);
        }
        SOCKADDR_IN addr;
        SOCKLEN len = sizeof(addr);
        memset(&addr, 0, len);
        addr.sin_family = AF_INET;

        if (bind(s, (SOCKADDR*)&addr, len) == SOCKET_ERROR) {
            g_error << "bind" << std::endl;
            exit(1);
        }

        if (getsockname(s, (SOCKADDR*)&addr, &len) == SOCKET_ERROR) {
            g_error << "getsockname " << std::endl;
            exit(1);
        }
        closesocket(s);
        m_rconPort = addr.sin_port;
    }

    void FactorioInstance::StartRCON() {
        if (m_rconConnected) CloseRCON();

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

        if (!SendRCON("pass", RCON_AUTH)) {
            g_error << "auth" << std::endl;
            exit(1);
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
            g_error << "send packet" << std::endl;
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
        RequestBase r;
        r.id = s_id++;
        r.name = name;
        if (id) *id = r.id;
        auto json = ToJson(r);
        return SendRCON("/request " + QJsonDocument(json).toJson(QJsonDocument::Compact).toStdString());
    }

    std::future<ResponseBase> FactorioInstance::AddResponseBase(uint32_t id) {
        auto promise = std::make_shared<std::promise<ResponseBase>>();
        m_pendingRequests[id] = [promise](const QJsonObject &json) {
            ResponseBase data;
            FromJson(json, data);
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
                .error = FACTORIO_NOT_RUNNING
            });
            return promise.get_future();
        }

        return AddResponseBase(id);
    }

    const PathfinderData &FactorioInstance::GetPathfinderData(RequestError *error) {
        std::unique_lock lock(m_pullPathfinderDataMutex);
        auto future = Request<LuaPathfinderData>("PathfinderDataUpdate");
        future.wait();
        auto res = future.get();
        lock.unlock();

        if (!res.success) {
            if (error) *error = res.error;
            return m_pathfinderData;
        }

        auto transform = [](std::unordered_set<MapPosition> &data,
            const std::vector<LuaPathfinderData::SubData> &rawData) {

            for (const auto &subdata : rawData) {
                if (subdata.a) {
                    if (!data.contains(subdata.p)) data.insert(subdata.p);
                } else {
                    if (data.contains(subdata.p)) data.erase(subdata.p);
                }
            }
        };

        transform(m_pathfinderData.entity, res.data.entity);
        transform(m_pathfinderData.chunk, res.data.chunk);

        return m_pathfinderData;
    }
}