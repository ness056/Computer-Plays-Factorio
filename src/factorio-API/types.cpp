#include "../factorio-API/prototypes.hpp"
#include "types.hpp"

namespace ComputerPlaysFactorio {

    std::future<void> Entity::FetchProperties() {
        if (IsAbstract()) throw RuntimeErrorF("Cannot use FetchProperties method on not abstract entities.");
        
        auto properties = m_instance->Request("FetchEntityProperties", {{ "entity", m_name }, { "position", m_position }});

        return std::async(std::launch::deferred, [this, properties = std::move(properties)] mutable {
            properties.wait();
            auto j = properties.get();
            from_json(j["data"], *this);
        });
    }

    void Entity::SetName(const std::string &name) {
        CheckAbstract();
        m_name = name;
        if (m_type.empty()) {
            m_prototype = &g_prototypes.GetEntity(m_name);
            m_type = (*m_prototype)["type"].get<std::string>();
        }
        else m_prototype = &g_prototypes.Get(m_type, m_name);
        UpdateBoundingBox();
    }

    const double Entity::GetReach() const {
        if (m_type == "simple-entity" || m_type == "tree" || m_type == "resource") {
            return g_prototypes.Get("character", "character")["reach_resource_distance"];
        } else {
            return g_prototypes.Get("character", "character")["reach_distance"];
        }
    }

    void to_json(json &j, const Entity &e) {
        j["type"] = e.m_type;
        j["name"] = e.m_name;
        j["position"] = e.m_position;
        j["direction"] = e.m_direction;
        j["mirror"] = e.m_mirror;
        j["recipe"] = e.m_recipe;
        j["underground_type"] = e.m_underground_type;
        j["input_priority"] = e.m_input_priority;
        j["output_priority"] = e.m_output_priority;
        j["resource_amount"] = e.m_resource_amount;
        j["inventories"] = json::object();
        for (const auto &[type, inventory] : e.m_inventories) {
            j["inventories"][Inventory::TypeToString(type)] = inventory;
        }
        j["crafting_progress"] = e.m_crafting_progress;
    }

    void from_json(const json &j, Entity &e) {
        const Entity default_{};
        e.m_type = !j.is_null() ? j.value("type", default_.m_type) : default_.m_type;
        e.m_name = !j.is_null() ? j.value("name", default_.m_name) : default_.m_name;
        e.m_position = !j.is_null() ? j.value("position", default_.m_position) : default_.m_position;
        e.m_direction = !j.is_null() ? j.value("direction", default_.m_direction) : default_.m_direction;
        e.m_mirror = !j.is_null() ? j.value("mirror", default_.m_mirror) : default_.m_mirror;
        e.m_recipe = !j.is_null() ? j.value("recipe", default_.m_recipe) : default_.m_recipe;
        e.m_underground_type = !j.is_null() ? j.value("underground_type", default_.m_underground_type) : default_.m_underground_type;
        e.m_input_priority = !j.is_null() ? j.value("input_priority", default_.m_input_priority) : default_.m_input_priority;
        e.m_output_priority = !j.is_null() ? j.value("output_priority", default_.m_output_priority) : default_.m_output_priority;
        e.m_resource_amount = !j.is_null() ? j.value("resource_amount", default_.m_resource_amount) : default_.m_resource_amount;
        e.m_inventories.clear();
        if (!j.is_null() && j.contains("inventories")) {
            const auto &inventories = j.at("inventories");
            for (const auto &[type, inventory] : inventories.items()) {
                e.m_inventories[Inventory::StringToType(type)] = inventory.get<Inventory>();
            }
        }
        e.m_crafting_progress = !j.is_null() ? j.value("crafting_progress", default_.m_crafting_progress) : default_.m_crafting_progress;

        if (e.m_type.empty()) {
            e.m_prototype = &g_prototypes.GetEntity(e.m_name);
            e.m_type = (*e.m_prototype)["type"].get<std::string>();
        }
        else e.m_prototype = &g_prototypes.Get(e.m_type, e.m_name);
        e.UpdateBoundingBox();
    }

    void Blueprint::Shift(const MapPosition &vector) {
        for (auto &entity : entities) {
            entity.SetPosition(entity.GetPosition() + vector);
        }
        center += vector;
    }

    void Blueprint::Rotate(Direction direction) {
        for (auto &entity : entities) {
            entity.SetDirection(entity.GetDirection() + direction);
            entity.SetPosition(entity.GetPosition().Rotate(direction));
        }
        center = center.Rotate(direction);
    }

    int Blueprint::CountEntityType(const std::string &type) const {
        int count = 0;
        for (const auto &entity : entities) {
            if (entity.GetType() == type) count++;
        }
        
        return count;
    }

    int Blueprint::CountEntityName(const std::string &name) const {
        int count = 0;
        for (const auto &entity : entities) {
            if (entity.GetName() == name) count++;
        }
        
        return count;
    }

    Blueprint Blueprint::LoadString(const std::string &str) {
        auto compressed = base64_decode(str.substr(1), true);

        json blueprint_json;
        uLongf buff_size = (uLongf)compressed.size() * 2;
        while (true) {
            char *buffer = new char[buff_size];
            uLongf buff_size_ = buff_size;
            int r = uncompress((Bytef*)buffer, &buff_size_, 
                (const Bytef*)compressed.c_str(), (uLongf)compressed.size() + 1);

            switch (r) {
            case Z_BUF_ERROR:
                buff_size *= 2;
                break;

            case Z_DATA_ERROR:
                ErrorAndExit("Invalid blueprint string.");

            case Z_OK: {
                auto j_str = std::string(buffer, buff_size_);
                json j;
                try {
                    j = json::parse(j_str);
                } catch (...) {
                    ErrorAndExit("Invalid blueprint string");
                }

                if (!j.contains("blueprint")) {
                    Debug("Blueprint json: {}", j_str);
                    ErrorAndExit("Invalid blueprint string. Make sure that it is an actual blueprint and not a book.");
                }
                blueprint_json = std::move(j["blueprint"]);
                goto Found;
            }

            default:
                throw RuntimeErrorF("Unhandled zlib error: {}", r);
            }
        }

        Found:

        if (!blueprint_json.contains("entities") || !blueprint_json["entities"].is_array()) return {};

        size_t n_entity = blueprint_json["entities"].size();
        Blueprint blueprint;
        blueprint.center = {0, 0};
        blueprint.entities.reserve(n_entity);

        for (auto &e : blueprint_json["entities"]) {
            if (!e.contains("name") || !e.contains("position")) {
                ErrorAndExit("Invalid blueprint string.");
            }

            auto &p = g_prototypes.GetEntity(e["name"]);
            auto pos = e["position"].get<MapPosition>();
            blueprint.center += pos;

            auto &entity = blueprint.entities.emplace_back();
            entity.m_type = p["type"].get<std::string>();
            entity.m_name = e["name"].get<std::string>();
            entity.m_position = pos;
            entity.m_direction = e.contains("direction") ? e["direction"].get<Direction>() : Direction::NORTH;
            entity.m_mirror = e.contains("mirror") ? e["mirror"].get<bool>() : false;
            entity.m_recipe = e.contains("recipe") ? e["recipe"].get<std::string>() : "";
            entity.m_underground_type = e.contains("type") ? e["type"].get<std::string>() : "";
            entity.m_input_priority = e.contains("input_priority") ? e["input_priority"].get<std::string>() : "";
            entity.m_output_priority = e.contains("output_priority") ? e["output_priority"].get<std::string>() : "";

            if (entity.m_type.empty()) {
                entity.m_prototype = &g_prototypes.GetEntity(entity.m_name);
                entity.m_type = (*entity.m_prototype)["type"].get<std::string>();
            }
            else entity.m_prototype = &g_prototypes.Get(entity.m_type, entity.m_name);
            entity.UpdateBoundingBox();
        }

        blueprint.center /= (double)n_entity;

        return blueprint;
    }

    const Blueprint &Blueprint::Load(const std::filesystem::path &path_) {
        auto path = GetDataPath() / "blueprints" / path_;
        static std::map<std::filesystem::path, Blueprint> blueprints;

        if (!blueprints.contains(path)) {
            Info("Loading blueprint {}", path_.string());

            if (!std::filesystem::exists(path)) {
                throw RuntimeErrorF("Failed to load blueprint {}, the file doesn't exist.", path.string());
            }

            auto size = std::filesystem::file_size(path);
            std::string bp(size, '\0');
            std::ifstream in(path);
            in.read(&bp[0], size);
            blueprints.emplace(path, LoadString(bp));
        }
       
        return blueprints[path];
    }
}