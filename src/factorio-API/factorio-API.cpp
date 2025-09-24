#include "factorio-API.hpp"

#include <cstring>

#include "../utils/inicpp.h"
#include "../utils/logging.hpp"
#include "../utils/thread.hpp"

namespace ComputerPlaysFactorio {

    void FactorioInstance::InitStatic() {
        std::scoped_lock lock(s_static_mutex);
        if (s_init_static) return;
        s_init_static = true;

        s_job_object = CreateJobObjectA(nullptr, nullptr);
        if (!s_job_object) {
            throw RuntimeErrorF("CreateJobObjectA failed: {}", GetLastError());
        }
        
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(s_job_object, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
            throw RuntimeErrorF("SetInformationJobObject failed: {}", GetLastError());
        }

        // Init sockets
        WSADATA wsa_data;
        int r = WSAStartup(MAKEWORD(2, 2), &wsa_data);
        if (r != 0) {
            throw RuntimeErrorF("WSAStartup failed: {}", r);
        }
    }

    Result FactorioInstance::IsFactorioPathValid(const std::string &path) {
        FactorioInstance instance(HEADLESS);
        auto factorio_path_quote = "\"" + path + "\"";
        auto config_path_quote = "\"" + instance.GetConfigPath().string() + "\"";
        const std::vector<const char*> argv = {
            factorio_path_quote.c_str(),
            "--config", config_path_quote.c_str(),
            "--version"
        };

        {
            std::scoped_lock lock(s_static_mutex);
            auto result = instance.StartPrivate(argv);
            if (result != SUCCESS) {
                return result;
            }
        }

        char buffer[12];
        auto read_bytes = instance.ReadStdout(buffer, 12);

        if (std::string(buffer, std::min(read_bytes, 9)) != "Version: ") {
            return INVALID_FACTORIO_PATH;
        } else if (buffer[9] != '2' || buffer[11] != '0') {
            return UNSUPPORTED_FACTORIO_VERSION;
        }

        return SUCCESS;
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

    FactorioInstance::FactorioInstance(Type t) : instance_type(t) {
        InitStatic();
        s_instances.insert(this);

        std::filesystem::create_directory(GetInstanceTempDir());

        if (std::filesystem::exists(GetDataPath() / "factorioConfig.ini")) {
            std::filesystem::copy(GetDataPath() / "factorioConfig.ini", 
                GetConfigPath(), std::filesystem::copy_options::overwrite_existing);
        }

        ini::IniFile config;
        config.load(GetConfigPath().string());

        config["path"]["write-data"] = GetInstanceTempDir().string();
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

    Result FactorioInstance::Start(uint32_t seed) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (Running()) return INSTANCE_ALREADY_RUNNING;

        auto result = IsFactorioPathValid(s_factorio_path);
        if (result != SUCCESS) return result;

        FindAvailablePort();
        
        auto config_path = GetConfigPath().string();
        ini::IniFile config;
        config.load(config_path);

        config["other"]["local-rcon-socket"] = "0.0.0.0:" + std::to_string(m_rcon_port);

        config.save(config_path);
        
        auto config_path_quote = "\"" + config_path + "\"";
        auto factorio_path_quote = "\"" + s_factorio_path + "\"";
        auto seed_str = std::to_string(seed);
        std::vector<const char*> argv = {
            factorio_path_quote.c_str(),
            "--config", config_path_quote.c_str(),
            "--map-gen-seed", seed_str.c_str()
        };

        auto port_str = std::to_string(m_rcon_port);
        if (instance_type == HEADLESS) {
            argv.push_back("--start-server-load-scenario");
            argv.push_back("base/freeplay");
            argv.push_back("--rcon-port");
            argv.push_back(port_str.c_str());
            argv.push_back("--rcon-password");
            argv.push_back("pass");
        } else {
            argv.push_back("--load-scenario");
            argv.push_back("base/freeplay");
        }

        {
            std::scoped_lock static_lock(s_static_mutex);
            result = StartPrivate(argv);
            if (result != SUCCESS) {
                return result;
            }
        }

        WaitForInputIdle(m_process_info.hProcess, INFINITE);
        
        // According to cppreference the set_terminate should propagate to all threads,
        // even if created afterwards, but it seems MSVC does not follow the cpp standard...
        auto terminate = std::get_terminate();
        m_out_listener = std::thread(&FactorioInstance::OutListener, this, terminate);
        m_out_listener.detach();

        return SUCCESS;
    }

    Result FactorioInstance::StartPrivate(const std::vector<const char*> &argv) {
        if (Running()) return INSTANCE_ALREADY_RUNNING;
        if (!m_cleaned) Clean();
        m_cleaned = false;
        
        ZeroMemory(&m_startup_info, sizeof(m_startup_info));
        m_startup_info.cb = sizeof(STARTUPINFOA);
        m_startup_info.dwFlags = STARTF_USESTDHANDLES;

        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, true };
        HANDLE rd, wr;
        if (!CreatePipe(&rd, &wr, &sa, 0)) {
            throw RuntimeErrorF("CreatePipe failed: {}", GetLastError());
        };

        if (!SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0)) {
            throw RuntimeErrorF("SetHandleInformation failed: {}", GetLastError());
        }

        m_in_write = wr;
        m_startup_info.hStdInput = rd;

        static long index = 0;
        const long i = index++;
        const std::string pipe_name = std::format("\\\\.\\pipe\\factorio_api_cpp.{}.{}.{}",
            GetCurrentProcessId(), GetCurrentThreadId(), i);
        const char *c_name = pipe_name.c_str();

        rd = CreateNamedPipeA(c_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
        if (rd == INVALID_HANDLE_VALUE) {
            throw RuntimeErrorF("CreateNamedPipeA failed: {}", GetLastError());
        }

        wr = CreateFileA(c_name, GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (wr == INVALID_HANDLE_VALUE) {
            throw RuntimeErrorF("CreateFileA failed: {}", GetLastError());
        }

        if (!SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0)) {
            throw RuntimeErrorF("SetHandleInformation failed: {}", GetLastError());
        }

        m_out_event = CreateEventA(&sa, 1, 1, NULL);
        if (!m_out_event) {
            throw RuntimeErrorF("CreateEventA failed: {}", GetLastError());
        }
        m_out_ol = {0, 0, {{0, 0}}, NULL};
        m_out_ol.hEvent = m_out_event;

        m_out_read = rd;
        m_startup_info.hStdOutput = wr;
        m_startup_info.hStdError = wr;

        size_t size = 1;
        for (const auto &str : argv) {
            size += strlen(str) + 1;
        }

        char *command = new char[size];
        command[0] = '\0';
        
        const char *sep = " ";
        for (const auto &str : argv) {
            strcat_s(command, size, str);
            strcat_s(command, size, sep);
        }
        command[size - 1] = '\0';

        if (!CreateProcessA(
            nullptr,
            command,
            nullptr,
            nullptr,
            true,
            CREATE_SUSPENDED,
            nullptr,
            nullptr,
            &m_startup_info,
            &m_process_info
        )) {
            delete[] command;
            return INVALID_FACTORIO_PATH;
        }

        delete[] command;

        if (!AssignProcessToJobObject(s_job_object, m_process_info.hProcess)) {
            Stop();
            throw RuntimeErrorF("AssignProcessToJobObject failed: {}", GetLastError());
        }

        if (!ResumeThread(m_process_info.hThread)) {
            Stop();
            throw RuntimeErrorF("ResumeThread failed: {}", GetLastError());
        }

        return SUCCESS;
    }

    void FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return;

        if (!TerminateProcess(m_process_info.hProcess, 0)) {
            throw RuntimeErrorF("TerminateProcess failed: {}", GetLastError());
        }

        CloseRCON();
    }

    void FactorioInstance::Clean() {
        if (m_cleaned) return;

        CloseHandle(m_startup_info.hStdInput);
        CloseHandle(m_startup_info.hStdOutput);
        CloseHandle(m_process_info.hProcess);
        CloseHandle(m_process_info.hThread);
        CloseHandle(m_in_write);
        CloseHandle(m_out_read);
        CloseHandle(m_out_event);

        m_cleaned = true;
    }

    bool FactorioInstance::Running() {
        GetExitCodeProcess(m_process_info.hProcess, &m_exit_code);
        return m_exit_code == STILL_ACTIVE;
    }

    void FactorioInstance::Join(int ms) {
        WaitForSingleObject(m_process_info.hProcess, ms);

        if (!GetExitCodeProcess(m_process_info.hProcess, &m_exit_code)) {
            return;
        }

        if (m_out_listener.joinable()) m_out_listener.join();
    }
    
    int FactorioInstance::ReadStdout(char *buffer, int size) {
        DWORD read_bytes;

        if (!ReadFile(m_out_read, buffer, size, &read_bytes, &m_out_ol) && GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(m_out_read, &m_out_ol, &read_bytes, 1)) {
                DWORD e = GetLastError();
                if (e != ERROR_IO_INCOMPLETE && e != ERROR_HANDLE_EOF) {
                    throw RuntimeErrorF("ReadFile failed: {}", GetLastError());
                }
            }
        }

        // std::cout << std::string(buffer, read_bytes);
        return (int)read_bytes;
    }

    void FactorioInstance::CheckWord(const std::string &previous_word, const std::string &word) {
        if (previous_word == "Starting" && word == "RCON") {
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
            int read_bytes = 0;
            while (read_bytes < count) {
                read_bytes += ReadStdout(buffer + read_bytes, count - read_bytes);
            }
            std::string data = std::string(buffer, count);
            delete[] buffer;

            auto json = rfl::json::read<ResponseBase, rfl::DefaultIfMissing>(data);
            if (!json) {
                throw RuntimeErrorF("Malformed response json: {}\nstring: {}", json.error().what(), data);
            }
            const auto &r_dataless = json.value();
            if (m_pending_requests.count(r_dataless.id)) {
                ThreadPool::QueueJob([f = m_pending_requests[r_dataless.id], data] {
                    f(data);
                });
                m_pending_requests.erase(r_dataless.id);
            }
        }

        else if (word.starts_with("event") && word.length() > 5) {
            int count;
            try {
                count = std::stoi(word.substr(5));
            } catch (std::invalid_argument const&) {
                return;
            }

            char *buffer = new char[count];
            int read_bytes = 0;
            while (read_bytes < count) {
                read_bytes += ReadStdout(buffer + read_bytes, count - read_bytes);
            }
            std::string data = std::string(buffer, count);
            delete[] buffer;

            auto json = rfl::json::read<EventBase, rfl::DefaultIfMissing>(data);
            if (!json) {
                throw RuntimeErrorF("Malformed event json: {}\nstring: {}", json.error().what(), data);
            }
            const auto &e_dataless = json.value();
            if (m_event_handlers.contains(e_dataless.name)) {
                ThreadPool::QueueJob([f = m_event_handlers[e_dataless.name], data] {
                    f(data);
                });
            } else {
                Warn("No event handler named {} is registered.", e_dataless.name);
            }
        }
    }

    void FactorioInstance::OutListener(terminate_handler terminate) {
        std::set_terminate(terminate);
        std::string previous_word, word;

        while (Running()) {
            while (true) {
                char byte;
                if (ReadStdout(&byte, 1)) {
                    if (byte == ' ' || byte == '\n' || byte == '\r' || byte == '\0') break;
                    else word += byte;
                } else {
                    break;
                }
            }

            CheckWord(previous_word, word);
            previous_word = word;
            word.clear();
        }
        
        CloseRCON();
        for (auto &[id, request] : m_pending_requests) {
            auto res = ResponseBase{.id = id, .success = false, .error = RequestError::FACTORIO_EXITED};
            const auto json = rfl::json::write(res);
            request(json);
        }
        m_pending_requests.clear();

        Join();
        if (m_exited_callback) m_exited_callback(m_exit_code);
    }

    void FactorioInstance::RegisterEvent(const std::string &name, const EventCallbackDL &callback) {
        m_event_handlers[name] = [=](const std::string &str) {
            auto json = rfl::json::read<EventBase, rfl::DefaultIfMissing>(str);
            if (!json) {
                throw RuntimeErrorF("Malformed event json: {}\nstring: {}", json.error().what(), str);
            }
            const auto &data = json.value();
            callback(data);
        };
    }

    void FactorioInstance::FindAvailablePort() {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            throw RuntimeErrorF("socket function returned INVALID_SOCKET: {}", LAST_SOCKET_ERROR);
        }
        SOCKADDR_IN addr;
        SOCKLEN len = sizeof(addr);
        memset(&addr, 0, len);
        addr.sin_family = AF_INET;

        if (bind(s, (SOCKADDR*)&addr, len) == SOCKET_ERROR) {
            throw RuntimeErrorF("bind function returned SOCKET_ERROR: {}", LAST_SOCKET_ERROR);
        }

        if (getsockname(s, (SOCKADDR*)&addr, &len) == SOCKET_ERROR) {
            throw RuntimeErrorF("getsockname function returned SOCKET_ERROR: {}", LAST_SOCKET_ERROR);
        }
        closesocket(s);
        m_rcon_port = addr.sin_port;
    }

    void FactorioInstance::StartRCON() {
        if (m_rcon_connected) CloseRCON();

        m_rcon_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_rcon_socket == INVALID_SOCKET) {
            throw RuntimeErrorF("socket function returned INVALID_SOCKET: {}", LAST_SOCKET_ERROR);
        }
        SOCKADDR_IN server;
        server.sin_family = AF_INET;
        InetPtonA(AF_INET, m_rcon_addr.c_str(), &server.sin_addr.s_addr);
        server.sin_port = htons(m_rcon_port);

        const char timeout = static_cast<char>(5000);
        setsockopt(m_rcon_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        if (::connect(m_rcon_socket, (const SOCKADDR*)&server, sizeof(server)) == -1) {
            throw RuntimeErrorF("connect function returned -1: {}", LAST_SOCKET_ERROR);
        }

        if (SendRCON("pass", RCON_AUTH) == RCON_AUTH_FAILED) {
            throw RuntimeErrorF("RCON authentification failed.");
        }

        if (m_ready_callback) m_ready_callback();
        m_rcon_connected = true;
    }

    void FactorioInstance::CloseRCON() {
        if (!m_rcon_connected) return;

        closesocket(m_rcon_socket);
        if (m_closed_callback) m_closed_callback();

        m_rcon_connected = false;
    }

    Result FactorioInstance::SendRCON(const std::string &data, RCONPacketType packet_type) const {
        if (!m_rcon_connected && packet_type != RCON_AUTH) return RCON_NOT_READY;

        static int32_t index = 0;
        int32_t id = index++;

        size_t data_size = data.size();
        int32_t size = (int32_t)data_size + 10;

        char *packet = new char[size + 4];
        std::memcpy(packet,                     &size,        sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t),   &id,          sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*2, &packet_type, sizeof(int32_t));
        std::memcpy(packet + sizeof(int32_t)*3, data.data(),  data_size);
        packet[size + 2] = '\0';
        packet[size + 3] = '\0';

        if (send(m_rcon_socket, packet, size + 4, 0) < 0) {
            throw RuntimeErrorF("Failed to send RCON packet: {}", LAST_SOCKET_ERROR);
        }

        delete[] packet;

        if (packet_type != RCON_AUTH) return SUCCESS;

        int32_t res_id, res_type;
        char res_body[4096];
        if (ReadPacket(res_id, res_type, res_body) == RCON_AUTH_FAILED) return RCON_AUTH_FAILED;

        if (res_id == id && res_type == RCON_AUTH_RESPONSE) return SUCCESS;

        return RCON_AUTH_FAILED;
    }

    Result FactorioInstance::ReadPacket(int32_t &id, int32_t &packetType, char body[4096]) const {
        int32_t response_size;
        if (recv(m_rcon_socket, (char*)&response_size, 4, 0) == -1) return RCON_AUTH_FAILED;
        if (response_size < 10) return RCON_AUTH_FAILED;

        if (recv(m_rcon_socket, (char*)&id, 4, 0) == -1) return RCON_AUTH_FAILED;
        if (recv(m_rcon_socket, (char*)&packetType, 4, 0) == -1) return RCON_AUTH_FAILED;
        if (recv(m_rcon_socket, body, response_size - 2, 0) == -1) return RCON_AUTH_FAILED;

        return SUCCESS;
    }

    Result FactorioInstance::RequestPrivate(const std::string &name, uint32_t *id) const {
        RequestDataBase r;
        r.id = s_id++;
        r.name = name;
        if (id) *id = r.id;
        const auto json = rfl::json::write(r);
        return SendRCON("/request " + json);
    }

    std::future<ResponseBase> FactorioInstance::AddResponseBase(uint32_t id) {
        auto promise = std::make_shared<std::promise<ResponseBase>>();
        m_pending_requests[id] = [promise](const std::string &str) {
            auto json = rfl::json::read<ResponseBase, rfl::DefaultIfMissing>(str);
            if (!json) {
                throw RuntimeErrorF("Malformed response json: {}\nstring: {}", json.error().what(), str);
            }
            const auto &data = json.value();
            promise->set_value(data);
        };
        return promise->get_future();
    }

    std::future<ResponseBase> FactorioInstance::RequestNoRes(const std::string &name) {
        uint32_t id;
        if (RequestPrivate(name, &id) != SUCCESS) {
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