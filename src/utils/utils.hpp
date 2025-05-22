#pragma once

#include <filesystem>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/file.h>
#endif

#include "serializer.hpp"

namespace ComputerPlaysFactorio {

    class Waiter {
    public:
        inline void Lock() {
            std::unique_lock lock(m_mutex);
            m_locked = true;
        }
        inline void Unlock(bool success = true) {
            m_success = success;
            m_locked = false;
            m_cond.notify_all();
        }
        inline bool Wait() {
            std::unique_lock lock(m_mutex);
            m_cond.wait(lock, [this] { return !m_locked; });
            return m_success;
        }
        inline bool IsLocked() {
            return m_locked;
        }

    private:
        bool m_success;
        bool m_locked = false;
        std::mutex m_mutex;
        std::condition_variable m_cond;
    };

    extern const std::chrono::high_resolution_clock::time_point g_startTime;

    class LoggingStream {
    public:
        LoggingStream(const std::string &name);

        LoggingStream &operator<<(std::ostream&(*endl)(std::ostream&)) {
            LoggingStream::file << endl;
            std::cout << endl;
            if (endl == std::endl<char, std::char_traits<char>>) {
                newLine = true;
            }
            return *this;
        }

        const std::string name;

        template<class T>
        friend LoggingStream &operator<<(LoggingStream &ls, T val);

        static inline void CloseFile() {
            if (file.is_open()) file.close();
        }

    private:
        static inline std::ofstream file;
        bool newLine = true;
    };

    extern LoggingStream g_info;
    extern LoggingStream g_warring;
    extern LoggingStream g_error;

    template<class T>
    LoggingStream &operator<<(LoggingStream &ls, T val) {
        if (ls.newLine) {
            auto time = std::chrono::high_resolution_clock::now() - g_startTime;
            double sec = time.count() / 1e9;
            int spaces = 4 - (int)std::floor(std::log10(sec));
            std::stringstream ss;
            ss << std::string(std::max(0, spaces), ' ') << std::fixed << std::setprecision(3) << sec << ls.name;

            LoggingStream::file << ss.str();
            std::cout << ss.str();
            ls.newLine = false;
        }

        LoggingStream::file << val;
        std::cout << val;
        return ls;
    }

    std::filesystem::path GetRootPath();
    std::filesystem::path GetTempDir();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetLuaPath() { return GetDataPath() / "lua"; }
    void ClearTempDirectory();
}