#include "bot/bot.hpp"

#include <cpptrace/cpptrace.hpp>

using namespace ComputerPlaysFactorio;

void Clean() noexcept {
    try {
        Info("Cleaning up...");

        FactorioInstance::StopAll();
        FactorioInstance::JoinAll();
        
        ClearTempDirectory();
        
        CloseLogFile();
    } catch (const std::exception &e) {
        Error("Cleaning Failed: Exception of type {}: {}",
            cpptrace::demangle(typeid(e).name()), e.what());
        ShowStacktrace();
    } catch (...) {
        Error("Cleaning Failed: Unknown exception");
        ShowStacktrace();
    }
    
    std::cout << "Goodbye :)" << std::endl;
}

void Init() {
    CreateLogFile();

    std::set_terminate([]() noexcept {
        printf("terminate");
        try {
            auto ptr = std::current_exception();
            if (!ptr) {
                Error("Terminate called without exception");
                ShowStacktrace();
            } else {
                std::rethrow_exception(ptr);
            }
        } catch (const std::exception &e) {
            Error("Exception of type {}: {}",
                cpptrace::demangle(typeid(e).name()), e.what());
            ShowStacktrace();
        } catch (...) {
            Error("Unknown exception");
            ShowStacktrace();
        }

        Clean();
        abort();
    });
}

int main(int argc, const char *argv[]) {
    if (argc == 1) {
        Error("Usage: ComputerPlaysFactorio <project path>");
        return 1;
    }

    Init();
    std::filesystem::path projectPath(argv[1]);

    Bot bot;
    bot.Exec(projectPath / "main.lua");
    
    bot.Start();
    Clean();
    printf("exit");
    return 0;
}