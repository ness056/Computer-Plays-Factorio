#include "factorioAPI.hpp"

#include <cstring>

#include "../utils/inicpp.h"
#include "../utils/logging.hpp"
#include "../utils/thread.hpp"

namespace ComputerPlaysFactorio {

    std::string FactorioInstance::s_factorioPath = "C:\\Users\\louis\\Code Projects\\Computer-Plays-Factorio\\factorio\\bin\\x64\\factorio.exe";

    bool FactorioInstance::IsFactorioPathValid(const std::string &path, std::string &message) {
        const std::vector<const char*> argv = {path.c_str(), "--version", nullptr};
        FactorioInstance instance(FactorioInstance::HEADLESS);

        if (!instance.StartPrivate(argv)) {
            message = "Invalid path. You must select the Factorio binary.";
            return false;
        }

        char buffer[12];
        auto readBytes = instance.ReadStdout(buffer, 12);

        if (std::string(buffer, std::min(readBytes, 9)) != "Version: ") {
            message = "Invalid path. You must select the Factorio binary.";
            return false;
        } else if (buffer[9] != '2' || buffer[11] != '0') {
            message = "Wrong Factorio version. Supported versions: 2.0.x";
            return false;
        }

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

        std::filesystem::create_directory(GetInstanceTempPath());

        if (std::filesystem::exists(GetDataPath() / "factorioConfig.ini")) {
            std::filesystem::copy(GetDataPath() / "factorioConfig.ini", 
                GetConfigPath(), std::filesystem::copy_options::overwrite_existing);
        }

        ini::IniFile config;
        config.load(GetConfigPath().string());

        config["path"]["write-data"] = GetInstanceTempPath().string();
        config["path"]["mods"] = GetModsPath().string();
        config["path"]["saves"] = (GetDataPath() / "saves").string();
        config["other"]["local-rcon-password"] = "pass";
        config["other"]["check-updates"] = false;
        config["other"]["autosave-interval"] = 0;
        config["interface"]["show-tips-and-tricks-notifications"] = false;
        config["interface"]["enable-recipe-notifications"] = false;
        config["graphics"]["full-screen"] = false;

        config.save(GetConfigPath().string());
    }

    FactorioInstance::~FactorioInstance() {
        s_instances.erase(this);
        Stop();
        Join();
        Clean();
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

        ini::IniFile config;
        config.load(GetConfigPath().string());

        config["other"]["local-rcon-socket"] = "0.0.0.0:" + std::to_string(m_rconPort);

        config.save(GetConfigPath().string());
        
        std::vector<const char*> argv = {
            s_factorioPath.c_str(),
            "--config", GetConfigPath().string().c_str(),
            "--map-gen-seed", std::to_string(seed).c_str()
        };

        if (instanceType == HEADLESS) {
            argv.push_back("--start-server-load-scenario");
            argv.push_back("base/freeplay");
            argv.push_back("--rcon-port");
            argv.push_back(std::to_string(m_rconPort).c_str());
            argv.push_back("--rcon-password");
            argv.push_back("pass");
        } else {
            argv.push_back("--load-scenario");
            argv.push_back("base/freeplay");
        }

        argv.push_back(nullptr);

        if (!StartPrivate(argv)) return false;
        
        m_outListener = std::thread(&FactorioInstance::OutListener, this);
        m_outListener.detach();

        return true;
    }

    void FactorioInstance::CheckWord(const std::string &previousWord, const std::string &word) {
        if (previousWord == "Starting" && word == "RCON") {
            StartRCON();
        }

        else if (word.starts_with("response") && word.length() > 8) {
            int count;
            try {
                count = std::stoi(word.substr(8));
            } catch (std::invalid_argument const&) {
                return;
            }

            char *buffer = new char[count];
            if (ReadStdout(buffer, count) < count) {
                return;
            }
            std::string data = std::string(buffer, count);

            auto rDataless = rfl::json::read<ResponseBase>(data).value();
            if (m_pendingRequests.count(rDataless.id)) {
                ThreadPool::QueueJob([f = m_pendingRequests[rDataless.id], data] {
                    f(data);
                });
                m_pendingRequests.erase(rDataless.id);
            }
        }

        else if (word.starts_with("event") && word.length() > 5) {
            int count;
            try {
                count = std::stoi(word.substr(8));
            } catch (std::invalid_argument const&) {
                return;
            }

            char *buffer = new char[count];
            if (ReadStdout(buffer, count) < count) {
                return;
            }
            std::string data = std::string(buffer, count);

            auto rDataless = rfl::json::read<EventBase>(data).value();
            if (m_eventHandlers.contains(rDataless.name)) {
                ThreadPool::QueueJob([f = m_eventHandlers[rDataless.name], data] {
                    f(data);
                });
            } else {
                Warn("No event handler named {} is registered.", rDataless.name);
            }
        }
    }

    void FactorioInstance::OutListener() {
        std::string previousWord, word;

        while (Running()) {
            while (true) {
                char byte;
                if (ReadStdout(&byte, 1)) {
                    if (byte == ' ') break;
                    else word += byte;
                } else {
                    break;
                }
            }

            CheckWord(previousWord, word);
            previousWord = word;
            word.clear();
        }

        CloseRCON();
        for (auto &[id, request] : m_pendingRequests) {
            auto res = ResponseBase{.id = id, .success = false, .error = RequestError::FACTORIO_EXITED};
            const auto json = rfl::json::write(res);
            request(json);
        }
        m_pendingRequests.clear();

        if (m_exitedCallback) m_exitedCallback(m_exitCode);
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

        if (m_readyCallback) m_readyCallback();
        m_rconConnected = true;
    }

    void FactorioInstance::CloseRCON() {
        if (!m_rconConnected) return;

        closesocket(m_rconSocket);
        if (m_closedCallback) m_closedCallback();

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