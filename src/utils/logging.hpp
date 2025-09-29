#pragma once

#include <string>
#include <sstream>
#include <format>
#include <thread>
#include <functional>
#include <iostream>
#include <fstream>

#include "utils.hpp"

namespace ComputerPlaysFactorio {

    enum class LogLevel {
        NORMAL,
        DEBUG
    };

    void CreateLogFile();
    void CloseLogFile();
    std::ofstream &GetLogFile();

    void SetLogLevel(LogLevel new_level);
    LogLevel GetLogLevel();

    template<typename... Types>
    void LogMessage(std::ostream &out, const std::string& level,
        const std::format_string<Types...> format, Types&&... args
    ) {
        auto time_elapsed = std::chrono::high_resolution_clock::now() - g_start_time;
        double sec = time_elapsed.count() / 1e9;
        int nb_spaces = 4 - (int)std::floor(std::log10(std::max(1., sec)));

        std::stringstream msg;
        msg << std::string(std::max(0, nb_spaces), ' ') << std::fixed << std::setprecision(3) << sec;
        
        msg << " [" << level << "] " << std::format(format, std::forward<Types>(args)...);

        auto &file = GetLogFile();
        if (file.is_open()) {
            file << msg.str() << std::endl;
        }
        out << msg.str() << std::endl;
    }

    template<typename... Types>
    void Info(const std::format_string<Types...> format, Types&&... args) {
        LogMessage(std::cout, "INFO", format, std::forward<Types>(args)...);
    }

    template<typename... Types>
    void Warn(const std::format_string<Types...> format, Types&&... args) {
        LogMessage(std::cout, "WARN", format, std::forward<Types>(args)...);
    }

    template<typename... Types>
    void Error(const std::format_string<Types...> format, Types&&... args) {
        LogMessage(std::cerr, "ERROR", format, std::forward<Types>(args)...);
    }

    template<typename... Types>
    void ErrorAndExit(const std::format_string<Types...> format, Types&&... args) {
        Error(format, std::forward<Types>(args)...);
        exit(1);
    }

    template<typename... Types>
    void Debug(const std::format_string<Types...> format, Types&&... args) {
        if (GetLogLevel() == LogLevel::DEBUG) {
            LogMessage(std::cout, "DEBUG", format, std::forward<Types>(args)...);
        }
    }

    void ShowStacktrace();

    template<typename... Types>
    inline std::runtime_error RuntimeErrorF(
        const std::format_string<Types...> format, Types&&... args
    ) {
        return std::runtime_error(std::format(format, std::forward<Types>(args)...));
    }

    #define DEBUG1 Debug("{}", __LINE__);
    #define DEBUG2(msg) Debug("{}: {}", __LINE__, msg);
}