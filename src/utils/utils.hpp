#pragma once

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <typeinfo>
#include <QSettings>

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
    constexpr std::string TypeName() {
        auto name = typeid(T).name();

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

    // https://gist.github.com/klmr/2775736#file-make_array-hpp
    template<typename... T>
    constexpr auto make_array(T&&... values) ->
            std::array<
                typename std::decay<
                    typename std::common_type<T...>::type>::type,
                sizeof...(T)> {
        return {std::forward<T>(values)...};
    }

    template<typename>
    struct is_std_array : std::false_type {};

    template<typename T, size_t Size>
    struct is_std_array<std::array<T, Size>> : std::true_type {};

    template<typename>
    struct is_vector : std::false_type {};

    template<typename T, typename A>
    struct is_vector<std::vector<T, A>> : std::true_type {};

    template<typename>
    struct is_string_key_map : std::false_type {};

    template<typename T, typename C, typename A>
    struct is_string_key_map<std::map<std::string, T, C, A>> : std::true_type {};

    extern const std::chrono::high_resolution_clock::time_point g_startTime;

    std::filesystem::path GetRootPath();
    std::filesystem::path GetTempDir();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetModsPath() { return GetDataPath() / "mods"; }
    inline std::filesystem::path GetLuaPath() { return GetDataPath() / "lua"; }
    inline std::filesystem::path GetDefaultProjectDir() { return GetRootPath() / "projects"; }
    void CreateProjectsDir();
    void ClearTempDirectory();
}