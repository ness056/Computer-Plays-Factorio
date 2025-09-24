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
        void AddEntity(const Entity&);
        void RemoveEntity(const std::string &name, const MapPosition &pos);

        TileType GetTile(const MapPosition &pos) const;
        void SetTile(MapPosition pos, TileType tile);

        void ChunkGenerated(const MapPosition &chunkPos);

        bool PathfinderCollides(const MapPosition &pos) const;

        // Debug function
        void ExportPathfinderData() const;

    private:
        void ChunkGeneratedNoLock(const MapPosition &chunkPos);

        std::unordered_map<MapPosition, Chunk> m_chunks;
        std::unordered_map<MapPosition, TileType> m_tiles;  // Empty is considered as NORMAL tile type.
        
        std::unordered_set<MapPosition> m_pathfinder_entity;
        std::unordered_set<MapPosition> m_pathfinder_chunk;
        std::unordered_set<MapPosition> m_pathfinder_tile;

        mutable std::mutex m_mutex;
    };
}