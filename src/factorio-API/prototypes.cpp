#include "prototypes.hpp"

namespace ComputerPlaysFactorio {

    FactorioPrototypes g_prototypes;

    Result FactorioPrototypes::Fetch() {
        auto result = FactorioInstance::IsFactorioPathValid(FactorioInstance::s_factorio_path);
        if (result != SUCCESS) return result;

        FactorioInstance instance(FactorioInstance::HEADLESS);
        auto factorio_path_quote = "\"" + FactorioInstance::s_factorio_path + "\"";
        auto config_path_quote = "\"" + instance.GetConfigPath().string() + "\"";
        const std::vector<const char*> argv = {
            factorio_path_quote.c_str(), "--dump-data",
            "--config", config_path_quote.c_str()
        };

        {
            std::scoped_lock lock(FactorioInstance::s_static_mutex);
            result = instance.StartPrivate(argv);
            if (result != SUCCESS) {
                return result;
            }
            instance.Join();
        }

        auto f_path = instance.GetInstanceTempDir() / "script-output" / "data-raw-dump.json";
        std::ifstream f(f_path);

        m_prototypes = json::parse(f);

        valid = true;
        return SUCCESS;
    }

    const json &FactorioPrototypes::GetEntity(const std::string &name) const {
        CheckValid();
        for (const auto &type : s_entity_types) {
            if (!m_prototypes.contains(type)) continue; // I'm too lazy to manually remove space age types
            if (m_prototypes[type].contains(name)) return m_prototypes[type][name];
        }
        throw RuntimeErrorF("Not entity prototype with name \"{}\" exists", name);
    }
    
    bool FactorioPrototypes::HasFlag(const std::string &type, const std::string &name, const std::string &flag) const {
        CheckValid();
        const auto &p = m_prototypes[type][name];
        if (!p.contains("flags")) return false;
        for (const auto &f : p["flags"]) {
            if (f == flag) return true;
        }
        return false;
    }

    bool FactorioPrototypes::HasCollisionMask(const std::string &type, const std::string &name, const std::string &mask) const {
        CheckValid();
        const auto &p_masks = m_prototypes[type][name];
        if (p_masks.contains("collision_mask")) {
            return p_masks["collision_mask"]["layers"].contains(mask);
        }

        const auto &default_masks = m_prototypes["utility-constants"]["default"]["default_collision_masks"];
        if (!default_masks.contains(type)) return false;

        return default_masks[type]["layers"].contains(mask);
    }

    void FactorioPrototypes::CheckValid() const {
        if (!valid) {
            throw RuntimeErrorF("You must call g_prototypes.Fetch() but calling any other methods.");
        }
    }
}