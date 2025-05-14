#pragma once

#include <filesystem>
#include <string>
#include <chrono>
#include <cstdint>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include "serializer.hpp"

namespace ComputerPlaysFactorio {

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