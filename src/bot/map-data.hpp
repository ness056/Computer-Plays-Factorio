#pragma once

#define _USE_MATH_DEFINES
#include <set>
#include <map>
#include <list>
#include <mutex>
#include <expected>

#include "../factorio-API/factorio-API.hpp"
#include "../factorio-API/prototypes.hpp"
#include "../factorio-API/types.hpp"

namespace ComputerPlaysFactorio {

    class Patch {
    public:
        enum Type {
            NORMAL,
            FLUID
        };

        // starting_direction can only be NORTH, SOUTH, WEST or EAST.
        Blueprint GetBurnerCityBP(Direction starting_direction, int min_running_time, int amount) const;
        // output_direction can only be NORTH, SOUTH, WEST or EAST.
        Blueprint GetElectricBP(Direction output_direction, int min_running_time, int amount = -1) const;

    private:
        friend class MapData;
        Type m_type;
        std::string m_name;
        std::vector<Entity> m_entities;

        int m_resource_amount;
        Area m_bounding_box;
    };
    using SPatch = std::shared_ptr<Patch>;

    class Chunk {
    public:
        Chunk(const MapPosition &pos) : m_position(pos) {}

        bool Collides(const Area&) const;

    private:
        friend class MapData;
        const MapPosition m_position;
        std::list<Entity> m_entities;
        std::vector<SPatch> m_patchs;
    };

    class MapData {
    public:
        inline void NewFork() {
            m_forks.emplace();
        }
        inline bool ForksEmpty() const {
            return m_forks.empty();
        }
        void ValidateAndMergeFork();
        void DestroyForks();

        MapPosition GetPlayerPosition(bool use_fork) const;
        void SetPlayerPosition(const MapPosition &pos, bool use_fork);
        
        // is_auto_place is ignored if use_fork = true
        void AddEntity(const Entity&, bool use_fork, bool is_auto_place = false);
        void RemoveEntity(const std::string &name, const MapPosition &pos, bool use_fork);
        template <class T>
        void UpdateEntity(const std::string &name, const MapPosition &pos, const std::string &property, const T &value, bool fork);

        TileType GetTile(const MapPosition &pos, bool use_fork) const;
        void SetTile(MapPosition pos, TileType tile, bool use_fork);

        void ChunkGenerated(const MapPosition &chunkPos, bool use_fork);

        bool PathfinderCollides(const MapPosition &pos, bool use_fork) const;

        // Debug function
        void ExportPathfinderData(bool use_fork) const;

    private:
        struct Fork {
            bool position_set = false;
            MapPosition final_player_position;
            std::unordered_map<MapPosition, Chunk> chunks;
            std::unordered_map<MapPosition, TileType> tiles;
        };

        std::expected<Entity*, bool> FindEntity(const std::string &name, const MapPosition &pos, bool use_fork);

        inline const Fork &GetFork() const {
            if (ForksEmpty()) throw RuntimeErrorF("No fork exists.");
            return m_forks.back();
        }
        inline Fork &GetFork() {
            if (ForksEmpty()) throw RuntimeErrorF("No fork exists.");
            return m_forks.back();
        }

        void ChunkGeneratedNoLock(const MapPosition &chunkPos, bool use_fork);

        MapPosition m_player_position;

        std::unordered_map<MapPosition, Chunk> m_chunks;
        std::unordered_map<MapPosition, TileType> m_tiles;
        
        std::unordered_set<MapPosition> m_colliders_entity;
        std::unordered_set<MapPosition> m_colliders_chunk;
        std::unordered_set<MapPosition> m_colliders_tile;

        std::queue<Fork, std::list<Fork>> m_forks;

        std::unordered_set<MapPosition> m_colliders_fork_entity;
        std::unordered_set<MapPosition> m_colliders_fork_tile;

        std::vector<SPatch> m_patchs;

        mutable std::mutex m_mutex;
    };

    template <class T>
    void MapData::UpdateEntity(const std::string &name, const MapPosition &pos, const std::string &property, const T &value, bool fork) {
        auto entity_exp = FindEntity(name, pos, fork);
        if (!entity_exp) return;

        auto &entity = **entity_exp;
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