#include "app.hpp"

#include <cpptrace/cpptrace.hpp>

#include "bot/bot.hpp"
#include "utils/thread.hpp"

namespace ComputerPlaysFactorio {

    static std::mutex s_terminate_mutex;
    
    void App::SetTerminate() {
        std::set_terminate([]() noexcept {
            std::scoped_lock lock(s_terminate_mutex); // In case multiple threads throw at the same time.

            try {
                auto ptr = std::current_exception();
                if (!ptr) {
                    Error("Terminate called without exception");
                    ShowStacktrace();
                } else {
                    std::rethrow_exception(ptr);
                }
            } catch (const LuaError &e) {
                Error("The lua side has thrown an exception: ", e.what());
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
        const json default_config = {
            {"factorio_path", "Replace with the Factorio binary path"},
            {"project_name", "Replace with the name of the project you want to run"},
            {"debug", false}
        };

        const auto config_path = GetConfigPath();
        if (std::filesystem::exists(config_path)) {
            std::ifstream file(config_path);

            try {
                m_config = json::parse(file);
            } catch (...) {
                ErrorAndExit("Invalid config file. Please delete it to generate a new one.");
            }
        } else {
            std::filesystem::create_directories(config_path.parent_path());
            std::ofstream file(config_path);
            
            file << default_config.dump(4);
        }

        FactorioInstance::SetFactorioPath(m_config["factorio_path"]);
        if (m_config["debug"]) {
            SetLogLevel(LogLevel::DEBUG);
        } else {
            SetLogLevel(LogLevel::NORMAL);
        }
    }
    
    App::App() {
        SetTerminate();

        CreateTempDirectory();
        CreateLogFile();

        ThreadPool::Start(8);

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
        if (!bots.contains(m_config["project_name"])) {
            ErrorAndExit("Unknown project name \"{}\"", m_config["project_name"].get<std::string>());
        }

        Info("Initializing bot.");
        auto bot = bots.at(m_config["project_name"])();
        Info("Starting bot.");
        auto result = bot->Start();
        if (result != SUCCESS) {
            ErrorAndExit("Failed to start the bot: {}", Result2String(result));
        }
    }
}