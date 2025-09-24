#pragma once

#include <string>

namespace ComputerPlaysFactorio {

    struct Config {
        std::string factorio_path = "Replace with the Factorio binary path";
        std::string project_name = "Replace with the name of the project you want to run";
        bool debug = false;
    };
    
    class App {
    public:
        App();
        ~App();

        void Run();

    private:
        void SetTerminate();
        void LoadConfig();

        Config m_config;
    };
}