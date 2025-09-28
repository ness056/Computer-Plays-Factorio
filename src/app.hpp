#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ComputerPlaysFactorio {
    
    class App {
    public:
        App();
        ~App();

        void Run();

    private:
        void SetTerminate();
        void LoadConfig();

        json m_config;
    };
}