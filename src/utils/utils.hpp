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

    extern const std::chrono::high_resolution_clock::time_point g_startTime;

    class LoggingStream {
    public:
        LoggingStream();

        LoggingStream &operator<<(std::ostream&(*f)(std::ostream&)) {
            LoggingStream::s_file << f;
            std::cout << f;
            if (f == std::endl<char, std::char_traits<char>>) {
                m_newLine = true;
            }
            return *this;
        }

        template<class T>
        LoggingStream &operator<<(T val) {
            if (m_newLine) {
                auto timeElapsed = std::chrono::high_resolution_clock::now() - g_startTime;
                double sec = timeElapsed.count() / 1e9;
                int nbSpaces = 4 - (int)std::floor(std::log10(sec));

                std::stringstream ss;
                ss << std::string(std::max(0, nbSpaces), ' ') << std::fixed << std::setprecision(3) << sec;

                LoggingStream::s_file << ss.str();
                std::cout << ss.str();
                m_newLine = false;
            }

            LoggingStream::s_file << val;
            std::cout << val;
            return *this;
        }

        static inline void CloseFile() {
            if (s_file.is_open()) s_file.close();
        }

    private:
        static inline std::ofstream s_file;
        bool m_newLine = true;
    };

    extern LoggingStream g_log;

    std::filesystem::path GetRootPath();
    std::filesystem::path GetTempDir();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetLuaPath() { return GetDataPath() / "lua"; }
    void ClearTempDirectory();
}