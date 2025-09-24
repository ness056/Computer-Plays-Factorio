#include "app.hpp"

#include <cpptrace/cpptrace.hpp>

#include "bot/bot.hpp"
#include "utils/thread.hpp"

namespace ComputerPlaysFactorio {
    
    void App::SetTerminate() {
        std::set_terminate([]() noexcept {
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

            FactorioInstance::StopAll();
            FactorioInstance::JoinAll();
    
            ClearTempDirectory();
            
            CloseLogFile();

            abort();
        });
    }

    void App::LoadConfig() {
        const auto config_path = GetConfigPath();
        if (std::filesystem::exists(config_path)) {
            std::ifstream file(config_path);

            const auto json = rfl::json::read<Config, rfl::DefaultIfMissing>(file);
            if (json) {
                m_config = json.value();
            } else {
                ErrorAndExit("Invalid config file.");
            }
        } else {
            std::filesystem::create_directories(config_path.parent_path());
            std::ofstream file(config_path);
            
            file << rfl::json::write(m_config, rfl::json::pretty);
        }

        FactorioInstance::SetFactorioPath(m_config.factorio_path);
        if (m_config.debug) {
            SetLogLevel(LogLevel::DEBUG);
        } else {
            SetLogLevel(LogLevel::NORMAL);
        }
    }
    
    App::App() {
        SetTerminate();

        ThreadPool::Start(8);

        CreateLogFile();

        Info("Loading config file.");
        LoadConfig();
        
        Info("Fetching Factorio prototypes.");
        g_prototypes.Fetch();
    }

    App::~App() {
        FactorioInstance::StopAll();
        FactorioInstance::JoinAll();

        ThreadPool::Stop();
        ThreadPool::Wait();
        
        ClearTempDirectory();
        
        CloseLogFile();
    }

    void App::Run() {
        const auto &bots = BotFactory::GetBots();
        if (!bots.contains(m_config.project_name)) {
            ErrorAndExit("Unknown project name \"{}\"", m_config.project_name);
        }

        Info("Initializing bot.");
        auto bot = bots.at(m_config.project_name)();
        Info("Starting bot.");
        auto result = bot->Start();
        if (result != SUCCESS) {
            ErrorAndExit("Failed to start the bot: {}", Result2String(result));
        }
    }
}