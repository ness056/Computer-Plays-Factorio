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

    enum LogLevel {
        LOG_NORMAL,
        LOG_DEBUG
    };

    void CreateLogFile();
    void CloseLogFile();
    std::ofstream &GetLogFile();

    void SetLogLevel(LogLevel newLevel);
    LogLevel GetLogLevel();

    template<typename... Types>
    void LogMessage(std::ostream &out, const std::string& level,
        const std::format_string<Types...> format, Types&&... args
    ) {
        auto timeElapsed = std::chrono::high_resolution_clock::now() - g_startTime;
        double sec = timeElapsed.count() / 1e9;
        int nbSpaces = 4 - (int)std::floor(std::log10(sec));

        std::stringstream msg;
        msg << std::string(std::max(0, nbSpaces), ' ') << std::fixed << std::setprecision(3) << sec;
        
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
    void Debug(const std::format_string<Types...> format, Types&&... args) {
        if (GetLogLevel() == LOG_DEBUG) {
            LogMessage(std::cout, "DEBUG", format, std::forward<Types>(args)...);
        }
    }

    void ShowStacktrace(std::ostream &out = std::cerr);

    template<typename... Types>
    inline std::runtime_error RuntimeErrorFormat(
        const std::format_string<Types...> format, Types&&... args
    ) {
        return std::runtime_error(std::format(format, std::forward<Types>(args)...));
    }
}