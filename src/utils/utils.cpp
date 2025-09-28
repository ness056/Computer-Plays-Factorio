#include "utils.hpp"

#include "logging.hpp"

namespace ComputerPlaysFactorio {

    const std::chrono::high_resolution_clock::time_point g_start_time =
        std::chrono::high_resolution_clock::now();

    std::filesystem::path GetRootPath() {
        char path[MAX_PATH + 1];
        GetModuleFileNameA(nullptr, path, MAX_PATH + 1);
        const auto root_path = std::filesystem::canonical(std::filesystem::path(path) / "../..");

        return root_path;
    }

    static HANDLE lock = INVALID_HANDLE_VALUE;

    void CreateTempDirectory() {
        auto path = GetTempDirectory();
        const auto lock_path = path / ".lock";

        if (!std::filesystem::exists(path)) std::filesystem::create_directory(path);

        if (lock == INVALID_HANDLE_VALUE) {
            lock = CreateFileA(lock_path.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (lock == INVALID_HANDLE_VALUE) {
                throw RuntimeErrorF("Failed to create lock. Make sure that no other instance of Computer Plays Factorio is running.");
            }
        }
    }

    static void RemoveAllTempDir() {
        const auto path = GetRootPath() / "temp";

        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            std::filesystem::remove_all(entry.path());
        }
    }

    void ClearTempDirectory() {
        if (lock != INVALID_HANDLE_VALUE) {
            CloseHandle(lock);
            lock = INVALID_HANDLE_VALUE;
            RemoveAllTempDir();
        }
    }
}