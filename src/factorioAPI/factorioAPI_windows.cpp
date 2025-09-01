// Windows implementation of the OS dependent methods of FactorioInstance

#ifdef _WIN32

#include "factorioAPI.hpp"

namespace ComputerPlaysFactorio {

    void FactorioInstance::InitStatic() {
        auto lock = std::scoped_lock(s_staticMutex);
        if (s_initStatic) return;
        s_initStatic = true;

        s_jobObject = CreateJobObjectA(nullptr, nullptr);
        if (!s_jobObject) {
            throw RuntimeErrorFormat("CreateJobObjectA failed: {}", GetLastError());
        }
        
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(s_jobObject, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
            throw RuntimeErrorFormat("SetInformationJobObject failed: {}", GetLastError());
        }

        // Init sockets
        WSADATA wsaData;
        int r = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (r != 0) {
            throw RuntimeErrorFormat("WSAStartup failed: {}", r);
        }
    }

    bool FactorioInstance::Running() {
        DWORD exitCode = static_cast<DWORD>(m_exitCode);
        GetExitCodeProcess(m_processInfo.hProcess, &exitCode);
        return m_exitCode == STILL_ACTIVE;
    }

    bool FactorioInstance::StartPrivate(const std::vector<const char*> &argv) {
        if (Running()) return false;
        if (!m_cleaned) Clean();
        m_cleaned = false;
        
        ZeroMemory(&m_startupInfo, sizeof(m_startupInfo));
        m_startupInfo.cb = sizeof(STARTUPINFOA);
        m_startupInfo.dwFlags = STARTF_USESTDHANDLES;

        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, true };
        HANDLE rd, wr;
        if (!CreatePipe(&rd, &wr, &sa, 0)) {
            throw RuntimeErrorFormat("CreatePipe failed: {}", GetLastError());
        };

        if (!SetHandleInformation(wr, HANDLE_FLAG_INHERIT, 0)) {
            throw RuntimeErrorFormat("SetHandleInformation failed: {}", GetLastError());
        }

        m_inWrite = wr;
        m_startupInfo.hStdInput = rd;

        static long index = 0;
        const long i = index++;
        const std::string pipeName = std::format("\\\\.\\pipe\\factorio_api_cpp.{}.{}.{}",
            GetCurrentProcessId(), GetCurrentThreadId(), i);
        const char *c_name = pipeName.c_str();

        rd = CreateNamedPipeA(c_name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, &sa);
        if (rd == INVALID_HANDLE_VALUE) {
            throw RuntimeErrorFormat("CreateNamedPipeA failed: {}", GetLastError());
        }

        wr = CreateFileA(c_name, GENERIC_WRITE, 0, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (wr == INVALID_HANDLE_VALUE) {
            throw RuntimeErrorFormat("CreateFileA failed: {}", GetLastError());
        }

        m_outEvent = CreateEventA(&sa, 1, 1, NULL);
        if (!m_outEvent) {
            throw RuntimeErrorFormat("CreateEventA failed: {}", GetLastError());
        }
        m_outOl = {0, 0, {{0, 0}}, NULL};
        m_outOl.hEvent = m_outEvent;

        m_outRead = rd;
        m_startupInfo.hStdOutput = wr;
        m_startupInfo.hStdError = wr;

        size_t size = 1;
        for (const auto &str : argv) {
            if (str != nullptr) {
                size += strlen(str) + 1;
            }
        }

        char *command = new char[size];
        command[0] = '\0';
        
        const char *sep = " ";
        for (const auto &str : argv) {
            if (str != nullptr) {
                strcat_s(command, size, str);
                strcat_s(command, size, sep);
            }
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
            &m_startupInfo,
            &m_processInfo
        )) {
            throw RuntimeErrorFormat("CreateProcessA failed: {}", GetLastError());
        }

        delete[] command;

        if (!AssignProcessToJobObject(s_jobObject, m_processInfo.hProcess)) {
            Stop();
            throw RuntimeErrorFormat("AssignProcessToJobObject failed: {}", GetLastError());
        }

        if (!ResumeThread(m_processInfo.hThread)) {
            Stop();
            throw RuntimeErrorFormat("ResumeThread failed: {}", GetLastError());
        }

        return true;
    }

    void FactorioInstance::Stop() {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!Running()) return;

        if (!TerminateProcess(m_processInfo.hProcess, 0)) {
            throw RuntimeErrorFormat("TerminateProcess failed: {}", GetLastError());
        }

        CloseRCON();
    }

    void FactorioInstance::Clean() {
        if (m_cleaned) return;

        CloseHandle(m_startupInfo.hStdInput);
        CloseHandle(m_startupInfo.hStdOutput);
        CloseHandle(m_processInfo.hProcess);
        CloseHandle(m_processInfo.hThread);
        CloseHandle(m_inWrite);
        CloseHandle(m_outRead);
        CloseHandle(m_outEvent);

        m_cleaned = true;
    }

    bool FactorioInstance::Join(int ms) {
        WaitForSingleObject(m_processInfo.hProcess, ms);

        DWORD exitCode = static_cast<DWORD>(m_exitCode);
        if (!GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
            return false;
        }

        if (m_outListener.joinable()) m_outListener.join();

        return true;
    }
    
    int FactorioInstance::ReadStdout(char *buffer, int size) {
        DWORD readBytes;

        if (!ReadFile(m_outRead, buffer, size, &readBytes, &m_outOl) && GetLastError() == ERROR_IO_PENDING) {
            if (!GetOverlappedResult(m_outRead, &m_outOl, &readBytes, 1)) {
                DWORD e = GetLastError();
                if (e != ERROR_IO_INCOMPLETE && e != ERROR_HANDLE_EOF) {
                    throw RuntimeErrorFormat("ReadFile failed: {}", GetLastError());
                }
            }
        }

        return (int)readBytes;
    }
}

#endif