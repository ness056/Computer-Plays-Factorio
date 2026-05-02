#pragma once

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ComputerPlaysFactorio {
    
    struct Config {
        std::string factorio_path = "Replace with the Factorio binary path";
        std::string project_name = "Replace with the name of the project you want to run";
        bool debug_print = false;
        bool debug_path = false;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Config, factorio_path, project_name, debug_print, debug_path)

    extern Config g_config;
}