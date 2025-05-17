#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cmath>

namespace ComputerPlaysFactorio {

    extern const std::chrono::steady_clock::time_point g_startTime;

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
}