#pragma once

#define _USE_MATH_DEFINES
#include <set>
#include <map>
#include <list>
#include <mutex>
#include <expected>

#include "../factorio-API/prototypes.hpp"

namespace ComputerPlaysFactorio {

    class MapData;

    class Patch {
    public:
        enum Type {
            IRON,
            COPPER,
            COAL,
            STONE,
            OIL
        };

        static constexpr Type StringToType(const std::string &s) {
            if (s == "iron-ore") return IRON;
            else if (s == "copper-ore") return COPPER;
            else if (s == "coal") return COAL;
            else if (s == "stone") return STONE;
            else return OIL;
        }

        static constexpr std::string TypeToString(Type p) {
            switch (p) {
                case IRON: return "iron-ore";
                case COPPER: return "copper-ore";
                case COAL: return "coal";
                case STONE: return "stone";
                case OIL: return "crude-oil";
                default: throw;
            }
        }

        constexpr std::string TypeToString() const { return TypeToString(m_type); }

        Patch() = default;
        Patch(MapData *map_data, Type type, MapPosition first_pos) :
            m_map_data(map_data),
            m_type(type),
            m_bounding_box(first_pos - MapPosition(1, 1), first_pos + MapPosition(1, 1)) {}
        Patch(MapData *map_data, const std::string &type, MapPosition first_pos) :
            Patch(map_data, StringToType(type), first_pos) {}

        // The burner city will be placed as close as possible to the attraction_point.
        // For example, that point may be the average position of all the patches of the burner city so that everything is close together.
        std::vector<Blueprint> GetBurnerCityBP(const MapPosition &attraction_point, int min_running_time, int amount) const;
        // output_direction can only be NORTH, SOUTH, WEST or EAST.
        Blueprint GetElectricBP(Direction output_direction, int min_running_time, int amount = -1) const;

        inline int ResourceAmount(const Entity &entity) const {
            std::scoped_lock lock(m_mutex);
            return ResourceAmountNoLock(entity);
        }

        inline int ResourceAmountMin(const Blueprint &blueprint) const {
            std::scoped_lock lock(m_mutex);
            return ResourceAmountMinNoLock(blueprint);
        }

        inline Type GetType() const { return m_type; }
        inline Area GetBoundingBox() const { std::scoped_lock lock(m_mutex); return m_bounding_box; }

    private:
        // Calling function should lock m_mutex.
        int ResourceAmountNoLock(const Entity&) const;
        // Calling function should lock m_mutex.
        int ResourceAmountMinNoLock(const Blueprint&) const;

        friend class MapData;
        MapData *m_map_data;
        const Type m_type;
        std::unordered_map<MapPosition, int> m_resources;

        int m_resource_amount = 0;
        Area m_bounding_box;

        mutable std::mutex m_mutex;
    };

    using SPatch = std::shared_ptr<Patch>;

    constexpr Patch::Type& operator++(Patch::Type& a) {
        int n = static_cast<int>(a);
        ++n;
        a = static_cast<Patch::Type>(n);
        return a;
    }

    constexpr Patch::Type operator++(Patch::Type& a, int) {
        Patch::Type copy = a;
        ++a;
        return copy;
    }

    class Chunk {
    public:
        Chunk(const MapPosition &pos) : m_position(pos) {}

        bool Collides(const Area&) const;

    private:
        friend class MapData;
        MapPosition m_position;
        std::list<SEntity> m_entities;
    };

    class MapData {
    public:
        enum Branch {
            // Contains the current map data.
            // Should be used when the bot needs information of what's on the map right now.
            MAIN,
            // Contains the map data of the blueprints and everything that the bot is planning to do at some point.
            // Should be used to plan the position of whatever the bot will build in middle to long term.
            PLANNING,
            // Contains the expected map data when the bot will be done building whatever it's building.
            // Should be used for to plan short term things like pathing.
            BUILDING
        };

        MapData(FactorioInstance *instance) : m_instance(instance) { assert(instance != nullptr); }

        MapPosition GetPlayerPosition(Branch) const;
        void SetPlayerPosition(const MapPosition &pos, Branch);

        inline Inventory GetPlayerMainInventory() const { return m_player_main_inventory; }
        std::future<void> UpdatePlayerMainInventory();
        
        // branch is ignored if is_auto_place is true
        SEntity AddEntity(const Entity&, Branch, bool is_auto_place = false);
        std::vector<SEntity> AddEntities(const Blueprint&, Branch);
        void RemoveEntity(const std::string &name, const MapPosition &pos, Branch);
        template <class T>
        void UpdateEntity(const std::string &name, const MapPosition &pos, const std::string &property, const T &value, Branch);

        std::vector<SEntity> FindEntities(Branch, std::function<bool(const SEntity&)>) const;
        std::vector<SEntity> FindEntities(const Area&, Branch, std::function<bool(const SEntity&)> = nullptr) const;

        TileType GetTile(const MapPosition &pos, Branch) const;
        // branch is ignored if is_auto_place is true
        void SetTile(MapPosition pos, TileType tile, Branch, bool is_auto_place = false);

        void ChunkGenerated(const MapPosition &chunkPos, Branch);

        bool PathfinderCollides(const MapPosition&, Branch) const;
        bool PathfinderCollides(const Area&, Branch) const;
        bool PathfinderCollides(const Blueprint&, Branch) const;

        MapPosition FindNonCollidingPosition(MapPosition, Branch) const;

        bool ResourceEntityCollides(const MapPosition&, Patch::Type type) const;
        bool ResourceEntityCollides(const Area&, Patch::Type type) const;
        bool ResourceEntityCollides(const Blueprint&, Patch::Type type) const;

        void ForPatchs(std::function<void(const SPatch&)> callback) const;

        // Debug function
        void DrawPathfinderData(Branch);
        void DrawPatchs();

    private:

        SEntity FindEntity(const std::string &name, const MapPosition &pos, Branch);

        SEntity AddEntityNoLock(const Entity&, Branch, bool is_auto_place);

        void ChunkGeneratedNoLock(const MapPosition &chunkPos, Branch);

        std::array<MapPosition, 3> m_player_positions;
        Inventory m_player_main_inventory;

        std::array<std::unordered_map<MapPosition, Chunk>, 3> m_chunks;
        std::array<std::unordered_map<MapPosition, TileType>, 3> m_tiles;
        
        std::unordered_set<MapPosition> m_colliders_chunk;
        std::array<std::unordered_set<MapPosition>, 3> m_colliders_entity;
        std::array<std::unordered_set<MapPosition>, 3> m_colliders_tile;

        std::vector<SPatch> m_patchs;

        FactorioInstance *m_instance;

        mutable std::mutex m_mutex;
    };

    template <class T>
    void MapData::UpdateEntity(const std::string &name, const MapPosition &pos, const std::string &property, const T &value, Branch branch) {
        auto entity_ptr = FindEntity(name, pos, branch);
        if (!entity_ptr) return;

        auto &entity = *entity_ptr;
        if constexpr (std::is_same_v<T, Direction>) {
            if (property == "direction") entity.SetDirection(value);
            else throw RuntimeErrorF("Unknown property name {}.", property);
        }

        if constexpr (std::is_same_v<T, bool>) {
            if (property == "mirror") entity.SetMirror(value);
            else throw RuntimeErrorF("Unknown property name {}.", property);
        }

        if constexpr (std::is_same_v<T, std::string>) {
            if (property == "recipe") entity.SetRecipe(value);
            else if (property == "underground_type") entity.SetUndergroundType(value);
            else if (property == "splitter_input_priority") entity.SetInputPriority(value);
            else if (property == "splitter_output_priority") entity.SetOutputPriority(value);
            else throw RuntimeErrorF("Unknown property name {}.", property);
        }
    }
}