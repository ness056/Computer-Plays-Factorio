#pragma once

#define _USE_MATH_DEFINES
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#error "Only Windows is supported for now"
#endif

namespace ComputerPlaysFactorio {
    enum Result {
        SUCCESS,
        INVALID_FACTORIO_PATH,
        UNSUPPORTED_FACTORIO_VERSION,
        INSTANCE_ALREADY_RUNNING,
        INSTANCE_NOT_RUNNING,
        RCON_NOT_READY,
        RCON_AUTH_FAILED,
        PATH_FINDER_FAILED
    };

    constexpr std::string Result2String(Result result) {
        switch (result)
        {
        case SUCCESS: return "SUCCESS";
        case INVALID_FACTORIO_PATH: return "INVALID_FACTORIO_PATH";
        case UNSUPPORTED_FACTORIO_VERSION: return "UNSUPPORTED_FACTORIO_VERSION";
        case INSTANCE_ALREADY_RUNNING: return "INSTANCE_ALREADY_RUNNING";
        case INSTANCE_NOT_RUNNING: return "INSTANCE_NOT_RUNNING";
        case RCON_NOT_READY: return "RCON_NOT_READY";
        case RCON_AUTH_FAILED: return "RCON_AUTH_FAILED";
        case PATH_FINDER_FAILED: return "PATH_FINDER_FAILED";
        }

        throw;
    }

    double HalfFloor(double x) {
        return std::floor(x * 2) / 2;
    }

    double HalfCeil(double x) {
        return std::ceil(x * 2) / 2;
    }

    double HalfRound(double x) {
        return std::round(x * 2) / 2;
    }

    extern const std::chrono::high_resolution_clock::time_point g_start_time;

    std::filesystem::path GetRootPath();
    inline std::filesystem::path GetTempDirectory() { return GetRootPath() / "temp"; }
    void CreateTempDirectory();
    void ClearTempDirectory();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetModsPath() { return GetDataPath() / "mods"; }
    inline std::filesystem::path GetConfigPath() { return GetDataPath() / "config.json"; }
}