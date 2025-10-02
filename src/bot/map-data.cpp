#include "map-data.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    bool Chunk::Collides(const Area &bounding_box) const {
        for (const auto &entity : entities) {
            if (entity.GetBoundingBox().Collides(bounding_box)) return true;
        }
        return false;
    }

    static void ForkValidationFailed() {
        throw RuntimeErrorF("MapData fork validation failed.");
    }

    void MapData::ValidateAndMergeFork() {
        std::scoped_lock lock(m_mutex);

        const auto &fork = m_forks.front();

        if (!fork.position_set) ForkValidationFailed();

        if ((fork.final_player_position - m_player_position).Round() != MapPosition(0, 0)) {
            ForkValidationFailed();
        }

        for (const auto &[pos, tile] : fork.tiles) {
            if (tile == TileType::NORMAL && m_tiles.contains(pos) && m_tiles[pos] != TileType::NORMAL) {
                ForkValidationFailed();
            } else if (tile == TileType::WATER && (!m_tiles.contains(pos) || m_tiles[pos] != TileType::WATER)) {
                ForkValidationFailed();
            }
        }

        for (const auto &[chunk_pos, chunk] : fork.chunks) {
            if (!m_chunks.contains(chunk_pos)) {
                ForkValidationFailed();
            }
            const auto &other_chunk = m_chunks.at(chunk_pos);
            
            for (const auto &entity : chunk.entities) {
                for (const auto &other_entity : other_chunk.entities) {
                    if (entity == other_entity) goto Found;
                }
                ForkValidationFailed();
                Found:;
            }
        }

        m_forks.pop();
    }

    void MapData::DestroyForks() {
        std::scoped_lock lock(m_mutex);

        while (!m_forks.empty()) {
            m_forks.pop();
        }

        m_colliders_fork_entity.clear();
        m_colliders_fork_tile.clear();
    }

    MapPosition MapData::GetPlayerPosition(bool use_fork) const {
        std::scoped_lock lock(m_mutex);

        if (!use_fork) return m_player_position;
        else {
            const auto &fork = GetFork();
            if (fork.position_set) return fork.final_player_position;
            else throw RuntimeErrorF("Final player position not set.");
        }
    }

    void MapData::SetPlayerPosition(const MapPosition &pos, bool use_fork) {
        std::scoped_lock lock(m_mutex);

        if (use_fork) {
            auto &fork = GetFork();
            fork.final_player_position = pos;
            fork.position_set = true;
        } else {
            m_player_position = pos;
        }
    }

    void MapData::AddEntity(const Entity &entity, bool use_fork, bool is_auto_place) {
        std::unique_lock lock(m_mutex);

        auto &chunks = use_fork ? GetFork().chunks : m_chunks;

        const auto chunk_position = entity.GetPosition().ChunkPosition();
        if (!chunks.contains(chunk_position)) {
            ChunkGeneratedNoLock(chunk_position, use_fork);
        }

        auto &chunk = chunks.at(chunk_position);
        chunk.entities.push_back(entity);

        auto collides_with_player = g_prototypes.HasCollisionMask(entity, "player");
        if (!collides_with_player) return;

        // Update pathfinder data
        
        const auto placeable_off_grid = g_prototypes.HasFlag(entity, "placeable-off-grid");
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = HalfFloor(entity.GetBoundingBox().right_bottom.x + n);
        const double y2 = HalfFloor(entity.GetBoundingBox().right_bottom.y + n);

        auto &collisions = use_fork ? m_colliders_fork_entity : m_colliders_entity;
        for (double x1 = HalfCeil(entity.GetBoundingBox().left_top.x - n); x1 <= x2; x1 += 0.5) {
            for (double y1 = HalfCeil(entity.GetBoundingBox().left_top.y - n); y1 <= y2; y1 += 0.5) {
                if (!collisions.contains({x1, y1})) {
                    collisions.emplace(x1, y1);
                    if (!use_fork && is_auto_place && !ForksEmpty()) {
                        m_colliders_fork_entity.emplace(x1, y1);
                    }
                }
            }
        }

        lock.unlock();
    }

    void MapData::RemoveEntity(const std::string &name, const MapPosition &pos, bool use_fork) {
        std::scoped_lock lock(m_mutex);
        
        auto &chunks = use_fork ? GetFork().chunks : m_chunks;

        auto chunk_position = pos.ChunkPosition();
        if (!chunks.contains(chunk_position)) {
            Warn("Tried to remove entity from mapData in a chunk that is not generated.");
            return;
        }

        auto &chunk = chunks.at(chunk_position);
        auto &entities = chunk.entities;

        const auto entity_it = std::find_if(entities.begin(), entities.end(), [&](const Entity &e) {
            return e.GetName() == name && e.GetPosition() == pos;
        });

        Area bounding_box = entity_it->GetBoundingBox();
        auto collides_with_player = g_prototypes.HasCollisionMask(*entity_it, "player");
        const auto placeable_off_grid = g_prototypes.HasFlag(*entity_it, "placeable-off-grid");
        entities.erase(entity_it);

        if (!collides_with_player) return;

        // Update pathfinder data
        
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = HalfFloor(bounding_box.right_bottom.x + n);
        const double y2 = HalfFloor(bounding_box.right_bottom.y + n);

        auto &collisions = use_fork ? m_colliders_fork_entity : m_colliders_entity;
        for (double x1 = HalfCeil(bounding_box.left_top.x - n); x1 <= x2; x1 += 0.5) {
            for (double y1 = HalfCeil(bounding_box.left_top.y - n); y1 <= y2; y1 += 0.5) {
                if (collisions.contains({x1, y1}) && !chunk.Collides(bounding_box)) {
                    collisions.erase({x1, y1});
                }
            }
        }
    }

    std::expected<Entity, bool> MapData::FindEntity(const std::string &name, const MapPosition &pos, bool use_fork) const {
        std::scoped_lock lock(m_mutex);

        auto chunk_pos = pos.ChunkPosition();
        const auto &chunks = use_fork ? GetFork().chunks : m_chunks;

        if (!chunks.contains(chunk_pos)) return;
        const auto &entities = chunks.at(chunk_pos).entities;
        for (const auto &entity : entities) {
            if (entity.GetName() == name && entity.GetPosition() == pos) return entity;
        }

        return std::unexpected(false);
    }

    TileType MapData::GetTile(const MapPosition &pos, bool use_fork) const {
        std::scoped_lock lock(m_mutex);
        const auto &tiles = use_fork ? GetFork().tiles : m_tiles;
        if (!tiles.contains(pos)) return TileType::NORMAL;
        else return tiles.at(pos);
    }

    void MapData::SetTile(MapPosition pos, TileType tile, bool use_fork) {
        std::scoped_lock lock(m_mutex);

        auto &tiles = use_fork ? GetFork().tiles : m_tiles;

        TileType old = TileType::NORMAL;
        if (tiles.contains(pos)) old = tiles.at(pos);
        if (old == tile) return;

        tiles[pos] = tile;
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

        auto &colliders = use_fork ? m_colliders_fork_tile : m_colliders_tile;
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

                if (tiles[other_pos] == TileType::WATER) continue;
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
                    if (tiles[other_pos] == TileType::WATER) goto ContinueTwice;
                }
                colliders.erase(corner);
            }
            ContinueTwice:;
        }
    }

    void MapData::ChunkGenerated(const MapPosition &chunk_position, bool use_fork) {
        std::scoped_lock lock(m_mutex);
        ChunkGeneratedNoLock(chunk_position, use_fork);
    }

    void MapData::ChunkGeneratedNoLock(const MapPosition &chunk_position, bool use_fork) {
        auto &chunks = use_fork ? GetFork().chunks : m_chunks;

        if (chunks.contains(chunk_position)) return;
        chunks.emplace(chunk_position, chunk_position);

        if (use_fork) return;

        // Update pathfinder data
        auto area = Area::FromChunkPosition(chunk_position);
        auto left_top = area.left_top;
        auto right_bottom = area.right_bottom;
        auto left_bottom = area.GetLeftBottom();
        auto right_top = area.GetRightTop();
        auto &collisions = m_colliders_chunk;

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
    
    bool MapData::PathfinderCollides(const MapPosition &pos, bool use_fork) const {
        std::scoped_lock lock(m_mutex);

        if (use_fork) {
            if (m_colliders_fork_entity.contains(pos) ||
                m_colliders_chunk.contains(pos) ||
                m_colliders_fork_tile.contains(pos)) return true;
        } else {
            if (m_colliders_entity.contains(pos) ||
                m_colliders_chunk.contains(pos) ||
                m_colliders_tile.contains(pos)) return true;
        }

        return false;
    }

    void MapData::ExportPathfinderData(bool use_fork) const {
        // /c for k,v in pairs(helpers.json_to_table(json)) do rendering.draw_circle{color={255,0,0},surface=1,filled=true,radius=0.18,target=v} end
        if (use_fork) {
            Debug("Current pathfinder data: {}", json(m_colliders_fork_entity).dump());
            Debug("Current pathfinder data: {}", json(m_colliders_chunk).dump());
            Debug("Current pathfinder data: {}", json(m_colliders_fork_tile).dump());
        } else {
            Debug("Current pathfinder data: {}", json(m_colliders_entity).dump());
            Debug("Current pathfinder data: {}", json(m_colliders_chunk).dump());
            Debug("Current pathfinder data: {}", json(m_colliders_tile).dump());
        }
    }
}