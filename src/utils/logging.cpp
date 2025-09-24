#include "logging.hpp"

#include <filesystem>
#include <cpptrace/cpptrace.hpp>
#include <cpptrace/from_current.hpp>

namespace ComputerPlaysFactorio {

    static std::ofstream s_file;

    void CreateLogFile() {
        std::filesystem::path log_file_path = GetRootPath() / "current.log";
        if (std::filesystem::exists(log_file_path)) {
            std::filesystem::rename(log_file_path, log_file_path.parent_path() / "old.log");
        }

        s_file.open(log_file_path);
        if (!s_file.is_open()) {
            throw RuntimeErrorF("Failed to create log file: {}", log_file_path.string());
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

    static LogLevel level = LogLevel::NORMAL;

    void SetLogLevel(LogLevel new_level) {
        level = new_level;
    }

    LogLevel GetLogLevel() {
        return level;
    }

    void ShowStacktrace(std::ostream &out) {
        const auto trace = cpptrace::generate_trace();
        trace.print(out);
    }
}