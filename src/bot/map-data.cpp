#include "map-data.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    bool Chunk::Collides(const Area &bounding_box) const {
        for (const auto &entity : entities) {
            if (!entity.bounding_box) {
                throw RuntimeErrorF("No bounding_box");
            }
            if (entity.bounding_box->Collides(bounding_box)) return true;
        }
        return false;
    }

    void MapData::AddEntity(const Entity &entity) {
        if (!entity.bounding_box) throw RuntimeErrorF("No bounding_box");

        std::scoped_lock lock(m_mutex);

        const auto chunk_position = entity.position.ChunkPosition();
        if (!m_chunks.contains(chunk_position)) {
            ChunkGeneratedNoLock(chunk_position);
        }

        auto &chunk = m_chunks.at(chunk_position);
        chunk.entities.push_back(entity);

        auto collides_with_player = g_prototypes.HasCollisionMask(entity, "player");
        if (!collides_with_player) return;

        // Update pathfinder data
        
        const auto placeable_off_grid = g_prototypes.HasFlag(entity, "placeable-off-grid");
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = std::floor((entity.bounding_box->right_bottom.x + n) * 2) / 2;
        const double y2 = std::floor((entity.bounding_box->right_bottom.y + n) * 2) / 2;

        auto &collisions = m_pathfinder_entity;
        for (double x1 = std::ceil((entity.bounding_box->left_top.x - n) * 2) / 2; x1 <= x2; x1 += 0.5) {
            for (double y1 = std::ceil((entity.bounding_box->left_top.y - n) * 2) / 2; y1 <= y2; y1 += 0.5) {
                if (!collisions.contains({x1, y1})) {
                    collisions.emplace(x1, y1);
                }
            }
        }
    }

    void MapData::RemoveEntity(const std::string &name, const MapPosition &pos) {
        std::scoped_lock lock(m_mutex);

        auto chunk_position = pos.ChunkPosition();
        if (!m_chunks.contains(chunk_position)) {
            Warn("Tried to remove entity from mapData in a chunk that is not generated.");
            return;
        }

        auto &chunk = m_chunks.at(chunk_position);
        auto &entities = chunk.entities;

        const auto entity_it = std::find_if(entities.begin(), entities.end(), [&](const Entity &e) {
            return e.name == name && e.position == pos;
        });

        if (!entity_it->bounding_box) throw RuntimeErrorF("No bounding_box");

        Area bounding_box = entity_it->bounding_box.value();
        auto collides_with_player = g_prototypes.HasCollisionMask(*entity_it, "player");
        const auto placeable_off_grid = g_prototypes.HasFlag(*entity_it, "placeable-off-grid");
        entities.erase(entity_it);

        if (!collides_with_player) return;

        // Update pathfinder data
        
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = std::floor((bounding_box.right_bottom.x + n) * 2) / 2;
        const double y2 = std::floor((bounding_box.right_bottom.y + n) * 2) / 2;

        auto &collisions = m_pathfinder_entity;
        for (double x1 = std::ceil((bounding_box.left_top.x - n) * 2) / 2; x1 <= x2; x1 += 0.5) {
            for (double y1 = std::ceil((bounding_box.left_top.y - n) * 2) / 2; y1 <= y2; y1 += 0.5) {
                if (collisions.contains({x1, y1}) && !chunk.Collides(bounding_box)) {
                    collisions.erase({x1, y1});
                }
            }
        }
    }

    TileType MapData::GetTile(const MapPosition &pos) const {
        std::scoped_lock lock(m_mutex);
        if (!m_tiles.contains(pos)) return TileType::NORMAL;
        else return m_tiles.at(pos);
    }

    void MapData::SetTile(MapPosition pos, TileType tile) {
        std::scoped_lock lock(m_mutex);

        TileType old = TileType::NORMAL;
        if (m_tiles.contains(pos)) old = m_tiles.at(pos);
        if (old == tile) return;

        m_tiles[pos] = tile;
        // In factorio the position of a tile is the position of its left top corner,
        // here we want the center of the tile.
        pos += MapPosition(0.5, 0.5);
        
        // Update pathfinder data
        constexpr std::array<MapPosition, 4> straight_vecs = {
            MapPosition(Direction::NORTH) / 2,
            MapPosition(Direction::EAST) / 2,
            MapPosition(Direction::SOUTH) / 2,
            MapPosition(Direction::WEST) / 2
        };
        constexpr std::array<MapPosition, 4> diagonal_vecs = {
            MapPosition(Direction::NORTH_WEST) / 2,
            MapPosition(Direction::NORTH_EAST) / 2,
            MapPosition(Direction::SOUTH_EAST) / 2,
            MapPosition(Direction::SOUTH_WEST) / 2
        };

        auto &colliders = m_pathfinder_tile;
        const bool collides = tile == TileType::WATER;
    
        if (collides) {
            if (!colliders.contains(pos)) colliders.insert(pos);
        } else {
            if (colliders.contains(pos)) colliders.erase(pos);
        }

        for (int i = 0; i < straight_vecs.size(); i++) {
            auto corner = pos + straight_vecs[i];
            if (collides) {
                if (colliders.contains(corner)) continue;
                colliders.insert(corner);
            } else {
                if (!colliders.contains(corner)) continue;
                auto other_pos = corner + straight_vecs[i];

                if (m_tiles[other_pos] == TileType::WATER) continue;
                colliders.erase(corner);
            }
        }

        for (int i = 0; i < diagonal_vecs.size(); i++) {
            auto corner = pos + diagonal_vecs[i];
            if (collides) {
                if (colliders.contains(corner)) continue;
                colliders.insert(corner);
            } else {
                if (!colliders.contains(corner)) continue;
                for (int j : {0, 1, 3}) {
                    auto other_pos = corner + diagonal_vecs[(i + j) % 4];
                    if (m_tiles[other_pos] == TileType::WATER) goto ContinueTwice;
                }
                colliders.erase(corner);
            }
            ContinueTwice:;
        }
    }

    void MapData::ChunkGenerated(const MapPosition &chunk_position) {
        std::scoped_lock lock(m_mutex);
        ChunkGeneratedNoLock(chunk_position);
    }

    void MapData::ChunkGeneratedNoLock(const MapPosition &chunk_position) {
        if (m_chunks.contains(chunk_position)) return;

        m_chunks.emplace(chunk_position, chunk_position);

        // Update pathfinder data
        auto area = Area::FromChunkPosition(chunk_position);
        auto left_top = area.left_top;
        auto right_bottom = area.right_bottom;
        auto left_bottom = area.GetLeftBottom();
        auto right_top = area.GetRightTop();
        auto &collisions = m_pathfinder_chunk;

        auto check_chunk = [this, &chunk_position, &collisions](const MapPosition &vec,
            MapPosition tile, const MapPosition &last_tile
        ) {
            auto other_chunk = chunk_position + vec;
            bool collide = !m_chunks.contains(other_chunk);

            if (!collide && vec.x != 0 && vec.y != 0) {
                auto horizontal_chunk = chunk_position + MapPosition(vec.x, 0);
                auto vertical_chunk = chunk_position + MapPosition(0, vec.y);

                collide = !(m_chunks.contains(horizontal_chunk) &&
                            m_chunks.contains(vertical_chunk));
            }

            auto increment = (vec.Rotate(M_PI_2).Abs()).Round() / 2;
            for (;;) {
                if (collide) {
                    if (!collisions.contains(tile)) {
                        collisions.insert(tile);
                    }
                } else {
                    if (collisions.contains(tile)) {
                        collisions.erase(tile);
                    }
                }

                if (tile == last_tile) break;
                tile += increment;
            }
        };

        auto n = MapPosition(Direction::NORTH), s = MapPosition(Direction::SOUTH),
            w = MapPosition(Direction::WEST), e = MapPosition(Direction::EAST);

        check_chunk(n, left_top + e, right_top + w);
        check_chunk(s, left_bottom + e, right_bottom + w);
        check_chunk(w, left_top + s, left_bottom + n);
        check_chunk(e, right_top + s, right_bottom + n);

        check_chunk(MapPosition(Direction::NORTH_WEST), left_top, left_top);
        check_chunk(MapPosition(Direction::NORTH_EAST), right_top, right_top);
        check_chunk(MapPosition(Direction::SOUTH_WEST), left_bottom, left_bottom);
        check_chunk(MapPosition(Direction::SOUTH_EAST), right_bottom, right_bottom);
    }
    
    bool MapData::PathfinderCollides(const MapPosition &pos) const {
        std::scoped_lock lock(m_mutex);

        if (m_pathfinder_entity.contains(pos) ||
            m_pathfinder_chunk.contains(pos) ||
            m_pathfinder_tile.contains(pos)) return true;

        return false;
    }

    void MapData::ExportPathfinderData() const {
        // /c for k,v in pairs(helpers.json_to_table(json)) do rendering.draw_circle{color={255,0,0},surface=1,filled=true,radius=0.18,target=v} end
        Debug("Current pathfinder data: {}", rfl::json::write(m_pathfinder_chunk));
        Debug("Current pathfinder data: {}", rfl::json::write(m_pathfinder_entity));
        Debug("Current pathfinder data: {}", rfl::json::write(m_pathfinder_tile));
    }
}