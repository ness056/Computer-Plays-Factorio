#include "logging.hpp"
#include "utils.hpp"

namespace ComputerPlaysFactorio {

    extern const std::chrono::steady_clock::time_point g_startTime = std::chrono::high_resolution_clock::now();

    LoggingStream g_info("Info");
    LoggingStream g_warring("Warring");
    LoggingStream g_error("Error");

    LoggingStream::LoggingStream(const std::string &n) : name(" " + n + " ") {
        if (!file.is_open()) {
            file.open(GetTempDir() / "computer-plays-factorio.log");
        }
    }
}
