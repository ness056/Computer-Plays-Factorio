#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <expected>

#include "../utils/base64.h"
#include <zlib.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../factorio-API/factorio-API.hpp"

namespace ComputerPlaysFactorio {

    class Inventory {
    public:
        enum Type {
            // Character, vehicles and chests main inventory
            MAIN = 0,
            // Character guns
            GUNS,
            // Character ammos
            AMMO,
            // Character armor
            ARMOR,
            // Burners, furnaces and vehicles fuel
            FUEL,
            // Crafters, furnaces and labs input
            INPUT,
            // Crafters and furnaces outpost
            OUTPUT,
            // All modules inventories
            MODULES
        };

        static constexpr Type StringToType(const std::string &str) {
            if (str == "main") return MAIN;
            if (str == "guns") return GUNS;
            if (str == "ammo") return AMMO;
            if (str == "armor") return ARMOR;
            if (str == "fuel") return FUEL;
            if (str == "input") return INPUT;
            if (str == "output") return OUTPUT;
            if (str == "modules") return MODULES;
            throw;
        }

        static constexpr std::string TypeToString(Type t) {
            switch (t) {
                case MAIN: return "main";
                case GUNS: return "guns";
                case AMMO: return "ammo";
                case ARMOR: return "armor";
                case FUEL: return "fuel";
                case INPUT: return "input";
                case OUTPUT: return "output";
                case MODULES: return "modules";
            }
            throw;
        }

        inline uint32_t Count(const std::string &str) const {
            return items.contains(str) ? items.at(str) : 0;
        }

        std::map<std::string, uint32_t> items;
        uint32_t size = 0;
    };

    NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Inventory, items, size);

    constexpr Inventory::Type& operator++(Inventory::Type& a) {
        int n = static_cast<int>(a);
        ++n;
        a = static_cast<Inventory::Type>(n);
        return a;
    }

    constexpr Inventory::Type operator++(Inventory::Type& a, int) {
        Inventory::Type copy = a;
        ++a;
        return copy;
    }

    enum class TileType {
        NORMAL,
        WATER
    };

    struct Blueprint;

    // May represent an actual entity on the map, or an abstract entity that does not correspond to any entity on the map.
    // If a nullptr is passed to the constructor, the entity will be abstract and the Set methods will work.
    // Otherwise, the Set methods will throw an exception and the FetchProperties method should be used before using
    // the Get methods.
    // Note that the SetValid method works for both abstract and actual entities.
    class Entity {
    public:
        Entity(FactorioInstance *instance = nullptr) : m_instance(instance), m_valid(instance != nullptr) {}

        friend constexpr bool operator==(const Entity &lhs, const Entity &rhs) {
            return lhs.m_type == rhs.m_type &&
                lhs.m_name == rhs.m_name &&
                lhs.m_position == rhs.m_position &&
                lhs.m_direction == rhs.m_direction &&
                lhs.m_mirror == rhs.m_mirror &&
                lhs.m_recipe == rhs.m_recipe &&
                lhs.m_underground_type == rhs.m_underground_type &&
                lhs.m_input_priority == rhs.m_input_priority &&
                lhs.m_resource_amount == rhs.m_resource_amount &&
                lhs.m_output_priority == rhs.m_output_priority;
        }

        inline void SetInstance(FactorioInstance *instance) { m_instance = instance; }
        inline bool IsAbstract() { return m_instance == nullptr; }
        inline void CheckAbstract() {
            if (!IsAbstract()) throw RuntimeErrorF("Cannot use Set methods on abstract entities.");
        }

        inline bool IsValid() const { return m_valid; }
        inline void SetValid(bool valid) { m_valid = valid; }

        std::future<void> FetchProperties();

        inline const auto &GetType() const { return m_type; }
        inline void SetType(const std::string &type) { CheckAbstract(); m_type = type; }

        inline const auto &GetName() const { return m_name; }
        void SetName(const std::string &name);

        inline const auto &GetPosition() const { return m_position; }
        inline void SetPosition(const MapPosition &position) {
            CheckAbstract();
            m_position = position;
            UpdateBoundingBox();
        }

        inline const auto &GetDirection() const { return m_direction; }
        inline void SetDirection(Direction direction) {
            CheckAbstract();
            m_direction = direction;
            UpdateBoundingBox();
        }

        inline const auto &GetMirror() const { return m_mirror; }
        inline void SetMirror(bool mirror) { CheckAbstract(); m_mirror = mirror; }

        inline const auto &GetRecipe() const { return m_recipe; }
        inline void SetRecipe(const std::string &recipe) { CheckAbstract(); m_recipe = recipe; }

        inline const auto &GetUndergroundType() const { return m_underground_type; }
        inline void SetUndergroundType(const std::string &underground_type) { CheckAbstract(); m_underground_type = underground_type; }

        inline const auto &GetInputPriority() const { return m_input_priority; }
        inline void SetInputPriority(const std::string &input_priority) { CheckAbstract(); m_input_priority = input_priority; }

        inline const auto &GetOutputPriority() const { return m_output_priority; }
        inline void SetOutputPriority(const std::string &output_priority) { CheckAbstract(); m_output_priority = output_priority; }

        inline const auto &GetResourceAmount() const { return m_resource_amount; }
        inline void SetResourceAmount(int resource_amount) { CheckAbstract(); m_resource_amount = resource_amount; }

        inline const auto &GetInventories() const { return m_inventories; }
        inline const auto &GetCraftingProgress() const { return m_crafting_progress; }
        
        inline const auto &GetBoundingBox() const { return m_bounding_box; }
        inline const auto &GetPrototype() const { return *m_prototype; }

        const double GetReach() const;

    private:
        friend void to_json(json &j, const Entity &e);
        friend void from_json(const json &j, Entity &e);
        friend struct Blueprint;
        friend struct std::hash<Entity>;

        void UpdateBoundingBox() {
            const auto &proto = *m_prototype;
            if (proto.contains("collision_box")) {
                m_bounding_box = proto["collision_box"].get<Area>().Rotate(m_direction) + m_position;
            }
        }

        FactorioInstance *m_instance;
        bool m_valid;

        const json *m_prototype = nullptr;
        Area m_bounding_box;

        std::string m_type;
        std::string m_name;
        MapPosition m_position;
        Direction m_direction = Direction::NORTH;
        bool m_mirror = false;

        std::string m_recipe;
        std::string m_underground_type;   // type of underground "input" or "output"
        std::string m_input_priority;     // "left", "right" or "none"
        std::string m_output_priority;    // "left", "right" or "none"
        int m_resource_amount = 0;

        std::map<Inventory::Type, Inventory> m_inventories;
        float m_crafting_progress = 0;
    };
    using UEntity = std::unique_ptr<Entity>;
    using SEntity = std::shared_ptr<Entity>;

    void to_json(json &j, const Entity &e);
    void from_json(const json &j, Entity &e);

    struct Blueprint {
        std::vector<Entity> entities;
        MapPosition center;

        void Shift(const MapPosition &vector);
        void Rotate(Direction direction);

        int CountEntityType(const std::string &type) const;
        int CountEntityName(const std::string &name) const;

        // Load should be preferred.
        static Blueprint LoadString(const std::string &str);

        // Decode the blueprint in the file at path data/blueprints/{path}.
        // The blueprints are cached, meaning that if a file is requested multiple times, it will only be loaded once.
        static const Blueprint &Load(const std::filesystem::path &path);
    };
}

template<>
struct std::hash<ComputerPlaysFactorio::Entity> {
    size_t operator()(const ComputerPlaysFactorio::Entity &e) const {
        return std::hash<std::string>()(e.m_type) ^
            (std::hash<std::string>()(e.m_name) << 1) ^
            (std::hash<ComputerPlaysFactorio::MapPosition>()(e.m_position) << 2) ^
            (std::hash<ComputerPlaysFactorio::Direction>()(e.m_direction) << 3) ^
            (std::hash<double>()(e.m_mirror) << 4) ^
            (std::hash<std::string>()(e.m_recipe) << 5) ^
            (std::hash<std::string>()(e.m_underground_type) << 6) ^
            (std::hash<std::string>()(e.m_input_priority) << 7) ^
            (std::hash<std::string>()(e.m_output_priority) << 8) ^
            (std::hash<double>()(e.m_resource_amount) << 9);
    }
};