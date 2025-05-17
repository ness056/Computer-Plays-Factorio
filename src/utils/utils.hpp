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

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "serializer.hpp"

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

    std::filesystem::path GetRootPath();
    std::filesystem::path GetTempDir();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetLuaPath() { return GetDataPath() / "lua"; }
    void ClearTempDirectory();

    enum Direction {
        North,
        Northeast,
        East,
        Southeast,
        South,
        Southwest,
        West,
        Northwest
    };

    struct MapPosition {
        constexpr MapPosition() : x(0), y(0) {}
        constexpr MapPosition(double x_, double y_) : x(x_), y(y_) {}

        inline std::string ToString() {
            return "(" + std::to_string(x) + "; " + std::to_string(y) + ")";
        }

        constexpr MapPosition &operator+=(const MapPosition &rhs) {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        friend constexpr MapPosition &operator+(MapPosition lhs, const MapPosition &rhs) {
            lhs += rhs;
            return lhs;
        }

        friend constexpr bool operator==(const MapPosition &lhs, const MapPosition &rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        friend constexpr bool operator!=(const MapPosition &lhs, const MapPosition &rhs) {
            return !(lhs == rhs);
        }

        constexpr MapPosition ChunkPosition() {
            return MapPosition(std::floor(x / 32), std::floor(y / 32));
        }

        double x;
        double y;

        SERIALIZABLE(MapPosition, x, y)
    };
}