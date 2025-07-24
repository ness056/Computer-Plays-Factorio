#include "logging.hpp"

#include <filesystem>
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

namespace ComputerPlaysFactorio {

    static std::ofstream s_file;

    void CreateLogFile() {
        std::filesystem::path logFilePath = GetRootPath() / "current.log";
        if (std::filesystem::exists(logFilePath)) {
            std::filesystem::rename(logFilePath, logFilePath.parent_path() / "old.log");
        }

        s_file.open(logFilePath);
        if (!s_file.is_open()) {
            throw RuntimeErrorFormat("Failed to create log file: {}", logFilePath.string());
        }
    }

    void CloseLogFile() {
        if (s_file.is_open()) {
            s_file.close();
        }
    }

    std::ofstream &GetLogFile() {
        return s_file;
    }

    static LogLevel level = LOG_NORMAL;

    void SetLogLevel(LogLevel newLevel) {
        level = newLevel;
    }

    LogLevel GetLogLevel() {
        return level;
    }

    void ShowStacktrace(std::ostream &out) {
        const auto trace = cpptrace::generate_trace();
        trace.print(out);
    }
}