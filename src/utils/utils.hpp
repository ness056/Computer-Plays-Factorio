#pragma once

#define _USE_MATH_DEFINES
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <typeinfo>
#include <map>

#ifdef __GNUG__
#include <memory>
#include <cxxabi.h>
#endif

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/file.h>
#endif

#define HasStaticField(Class, Field) \
    [](){ \
        return overloaded{[]<typename T>(int) -> decltype(T::Field, void(), std::true_type()) { return {}; }, \
                          []<typename T>(...) { return std::false_type{}; }}.operator()<Class>(0); \
    }()

#define PARENS ()
#define COMMA ,

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

namespace ComputerPlaysFactorio {

    // For some reason ReflectCpp does not have anything to get the name of a type,
    // and I'm too lazy to change to a better reflection lib
    template<typename T>
    std::string TypeName() {
        const auto name = typeid(T).name();

#ifdef __GNUG__
        int status = 0;

        std::unique_ptr<char, void(*)(void*)> res {
            abi::__cxa_demangle(name, NULL, NULL, &status),
            std::free
        };

        return (status == 0) ? res.get() : name;
#else
        return name;
#endif
    }

    extern const std::chrono::high_resolution_clock::time_point g_startTime;

    std::filesystem::path GetRootPath();
    std::filesystem::path GetTempDir();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetModsPath() { return GetDataPath() / "mods"; }
    inline std::filesystem::path GetLuaPath() { return GetDataPath() / "lua"; }
    inline std::filesystem::path GetConfigPath() { return GetRootPath() / "config.ini"; }
    void ClearTempDirectory();
}