#include "utils.hpp"

namespace ComputerPlaysFactorio {

    const std::chrono::high_resolution_clock::time_point g_startTime =
        std::chrono::high_resolution_clock::now();

    LoggingStream g_log;

    LoggingStream::LoggingStream() {
        if (!s_file.is_open()) {
            s_file.open(GetTempDir() / "computer-plays-factorio.log");
        }
    }

    std::filesystem::path GetRootPath() {
#ifdef _WIN32
        char path[MAX_PATH + 1];
        GetModuleFileNameA(nullptr, path, MAX_PATH + 1);
        const auto rootPath = std::filesystem::canonical(std::filesystem::path(path) / "../..");
#elif defined(__linux__)
        const auto rootPath = std::filesystem::canonical("/proc/self/exe") / "..";
#endif
        return rootPath;
    }

#ifdef _WIN32
    HANDLE lock = INVALID_HANDLE_VALUE;
#elif defined(__linux__)
    int fd = -1;
#endif
    
    std::filesystem::path GetTempDir() {
        const auto path = GetRootPath() / "temp";
        const auto lockPath = path / ".lock";

        if (!std::filesystem::exists(path)) std::filesystem::create_directory(path);

#ifdef _WIN32
        if (lock == INVALID_HANDLE_VALUE) {
            lock = CreateFileA(lockPath.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (lock == INVALID_HANDLE_VALUE) {
                g_log << "Failed to create lock" << std::endl;
                exit(1);
            }
        }
#elif defined(__linux__)
        if (fd < 0) {
            fd = open(lockPath.string().c_str(), O_RDWR | O_CREAT, 0666);
            if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) {
                g_log << "Failed to create lock" << std::endl;
                exit(1);
            }
        }
#endif

        return path;
    }

    static void RemoveAllTempDir() {
        const auto path = GetRootPath() / "temp";

        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            std::filesystem::remove_all(entry.path());
        }
    }

    void ClearTempDirectory() {
#ifdef _WIN32
        if (lock != INVALID_HANDLE_VALUE) {
            LoggingStream::CloseFile();
            CloseHandle(lock);
            lock = INVALID_HANDLE_VALUE;
            RemoveAllTempDir();
        }
#elif defined(__linux__)
        if (fd < 0) {
            LoggingStream::CloseFile();
            flock(fd, LOCK_UN);
            close(fd);
            RemoveAllTempDir();
        }
#endif
    }
}