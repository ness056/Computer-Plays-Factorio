#pragma once

#include <unordered_map>

#include "../algorithms/pathFinder.hpp"

namespace ComputerPlaysFactorio {

    struct Chunk {
        Chunk(const MapPosition &pos) : position(pos) {}

        inline auto FindEntity(const std::string &name, const MapPosition &pos) const {
            return std::find_if(entities.begin(), entities.end(), [&](const Entity &e) {
                return e.name == name && e.position == pos;
            });
        }

        bool Collides(const Area&) const;

        MapPosition position;

        std::list<Entity> entities;
    };

    class MapData {
    public:
        inline const auto &GetPathfinderData() const {
            return m_pathfinderData;
        }

        inline const auto &GetChunks() const {
            return m_chunks;
        }

        void AddEntity(const Entity&);
        void RemoveEntity(const std::string &name, const MapPosition &pos);

        void ChunkGenerated(const MapPosition&);

    private:
        PathfinderData m_pathfinderData;
        std::unordered_map<MapPosition, Chunk> m_chunks;
    };
}