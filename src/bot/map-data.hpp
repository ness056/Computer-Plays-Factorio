#pragma once

#define _USE_MATH_DEFINES
#include <set>
#include <map>
#include <list>
#include <mutex>

#include "../factorio-API/factorio-API.hpp"
#include "../factorio-API/prototypes.hpp"
#include "../factorio-API/types.hpp"

namespace ComputerPlaysFactorio {

    struct Chunk {
        Chunk(const MapPosition &pos) : position(pos) {}

        bool Collides(const Area&) const;

        const MapPosition position;
        std::list<Entity> entities;
    };

    class MapData {
    public:
        inline void NewFork() {
            m_forks.emplace();
        }
        void ValidateAndMergeFork();
        void DestroyForks();

        const MapPosition &GetPlayerPosition(bool use_fork) const;
        void SetPlayerPosition(const MapPosition &pos, bool use_fork);
        
        // is_auto_place is ignored if use_fork = true
        void AddEntity(const Entity&, bool use_fork, bool is_auto_place = false);
        void RemoveEntity(const std::string &name, const MapPosition &pos, bool use_fork);

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

        inline const Fork &GetFork() const {
            return m_forks.back();
        }
        inline Fork &GetFork() {
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

        mutable std::mutex m_mutex;
    };
}