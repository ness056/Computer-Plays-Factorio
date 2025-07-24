#include "mapData.hpp"

#define _USE_MATH_DEFINES
#include <cmath>

#include "../factorioAPI/factorioData.hpp"
#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    bool Chunk::Collides(const Area &boundingBox) const {
        for (const auto &entity : entities) {
            if (entity.boundingBox.Collides(boundingBox)) return true;
        }
        return false;
    }

    void MapData::AddEntity(const Entity &entity) {
        auto chunkPosition = entity.position.ChunkPosition();
        if (!m_chunks.contains(chunkPosition)) {
            ChunkGenerated(chunkPosition);
        }

        m_chunks.at(chunkPosition).entities.push_back(entity);

        if (!entity.collidesWithPlayer) return;

        // Update pathfinder data
        double x1 = std::ceil(entity.boundingBox.leftTop.x - CHARACTER_SIZE);
        double x2 = std::floor(entity.boundingBox.rightBottom.x + CHARACTER_SIZE);
        double y1 = std::ceil(entity.boundingBox.leftTop.y - CHARACTER_SIZE);
        double y2 = std::floor(entity.boundingBox.rightBottom.y + CHARACTER_SIZE);

        auto &collisions = m_pathfinderData.entity;
        for (; x1 < x2; x1++) {
            for (; y1 < y2; y1++) {
                if (!collisions.contains({x1, y1})) {
                    collisions.emplace(x1, y1);
                }
            }
        }
    }

    void MapData::RemoveEntity(const std::string &name, const MapPosition &pos) {
        auto chunkPosition = pos.ChunkPosition();
        if (!m_chunks.contains(chunkPosition)) {
            Warn("Tried to remove entity from mapData in a chunk that is not generated.");
            return;
        }

        auto &chunk = m_chunks.at(chunkPosition);
        const auto entity_it = m_chunks.at(chunkPosition).FindEntity(name, pos);

        const auto boundingBox = entity_it->boundingBox;
        const bool collidesWithPlayer = entity_it->collidesWithPlayer;
        chunk.entities.erase(entity_it);

        if (!collidesWithPlayer) return;

        // Update pathfinder data
        double x1 = std::ceil(boundingBox.leftTop.x - CHARACTER_SIZE);
        double x2 = std::floor(boundingBox.rightBottom.x + CHARACTER_SIZE);
        double y1 = std::ceil(boundingBox.leftTop.y - CHARACTER_SIZE);
        double y2 = std::floor(boundingBox.rightBottom.y + CHARACTER_SIZE);

        auto &collisions = m_pathfinderData.entity;
        for (; x1 < x2; x1++) {
            for (; y1 < y2; y1++) {
                auto tile = MapPosition(x1, y1);
                if (collisions.contains(tile) && !chunk.Collides(boundingBox)) {
                    collisions.erase(tile);
                }
            }
        }
    }

    void MapData::ChunkGenerated(const MapPosition &chunkPosition) {
        if (m_chunks.contains(chunkPosition)) return;

        m_chunks.emplace(chunkPosition, chunkPosition);

        // Update pathfinder data
        auto area = Area::FromChunkPosition(chunkPosition);
        auto leftTop = area.leftTop;
        auto rightBottom = area.rightBottom;
        auto leftBottom = area.GetLeftBottom();
        auto rightTop = area.GetRightTop();
        auto &collisions = m_pathfinderData.chunk;

        auto CheckChunk = [this, &chunkPosition, &collisions](const MapPosition &vec,
            MapPosition tile, const MapPosition &lastTile
        ) {
            auto otherChunk = chunkPosition + vec;
            bool collide = m_chunks.contains(otherChunk);

            if (!collide && vec.x != 0 && vec.y != 0) {
                auto horizontalChunk = chunkPosition + MapPosition(vec.x, 0);
                auto verticalChunk = chunkPosition + MapPosition(0, vec.y);

                collide = !(m_chunks.contains(horizontalChunk) &&
                            m_chunks.contains(verticalChunk));
            }

            auto increment = vec.Rotate(M_PI_2).Abs();
            for (;;) {
                if (collide) {
                    if (!collisions.contains(tile)) {
                        collisions.emplace(tile);
                    }
                } else {
                    if (collisions.contains(tile)) {
                        collisions.erase(tile);
                    }
                }

                if (tile == lastTile) break;
                tile += increment;
            }
        };

        auto n = MapPosition(Direction::NORTH), s = MapPosition(Direction::SOUTH),
            w = MapPosition(Direction::WEST), e = MapPosition(Direction::EAST);

        CheckChunk(n, leftTop + e, rightTop + w);
        CheckChunk(s, leftBottom + e, rightBottom + w);
        CheckChunk(w, leftTop + s, leftBottom + n);
        CheckChunk(e, rightTop + s, rightBottom + n);

        CheckChunk(MapPosition(Direction::NORTH_WEST), leftTop, leftTop);
        CheckChunk(MapPosition(Direction::NORTH_EAST), rightTop, rightTop);
        CheckChunk(MapPosition(Direction::SOUTH_WEST), leftBottom, leftBottom);
        CheckChunk(MapPosition(Direction::SOUTH_EAST), rightBottom, rightBottom);
    }
}