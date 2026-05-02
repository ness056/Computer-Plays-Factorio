#include "map-data.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    std::vector<Blueprint> Patch::GetBurnerCityBP(const MapPosition &attraction_point, int min_running_time, int amount) const {
        std::scoped_lock lock(m_mutex);

        std::vector<Blueprint> vec;

        const int min_resource_amount = min_running_time / 4;
        Direction attraction_direction = CardinalDirection((attraction_point - m_bounding_box.Center()).ToDirection());
        Blueprint blueprint;
        MapPosition step_vec;
        if (m_type == COAL) {
            blueprint = Blueprint::Load("core/burners/coal.txt");
            step_vec = MapPosition(0, -2);
        }
        else if (m_type == STONE) {
            blueprint = Blueprint::Load("core/burners/stone.txt");
            step_vec = MapPosition(1, -4);
        }
        else {
            blueprint = Blueprint::Load("core/burners/normal.txt");
            step_vec = MapPosition(0, -2);
        }
        blueprint.Rotate(attraction_direction);
        step_vec = step_vec.Rotate(attraction_direction);

        int amount_in_bp = blueprint.CountEntityType("mining-drill");
        assert(amount % amount_in_bp == 0 && "TODO");
        amount /= amount_in_bp;

        MapPosition first;
        IterateFromClosestPointArea(
            attraction_point, {1, 0}, m_bounding_box,
            [this, &first, &blueprint, min_resource_amount](const MapPosition &pos) {
                blueprint.Shift(pos);
                int resource_amount = ResourceAmountMinNoLock(blueprint);
                if (resource_amount > min_resource_amount &&
                    !m_map_data->PathfinderCollides(blueprint, MapData::PLANNING) &&
                    !m_map_data->ResourceEntityCollides(blueprint, m_type)
                ) {
                    first = pos;
                    blueprint.Shift(-pos);
                    return true;
                }
                blueprint.Shift(-pos);
                return false;
            }, [] { throw RuntimeErrorF("No valid position for burner city found."); }
        );

        IterateFromClosestPointArea(
            first, step_vec, m_bounding_box,
            [this, &vec, &first, &blueprint, min_resource_amount, amount](const MapPosition &pos) {
                blueprint.Shift(pos);
                int resource_amount = ResourceAmountMinNoLock(blueprint);
                if (resource_amount > min_resource_amount &&
                    !m_map_data->PathfinderCollides(blueprint, MapData::PLANNING) &&
                    !m_map_data->ResourceEntityCollides(blueprint, m_type)
                ) {
                    m_map_data->AddEntities(blueprint, MapData::PLANNING);
                    vec.emplace_back(blueprint);
                }

                blueprint.Shift(-pos);
                if (vec.size() < amount) return false;
                return true;
            }
        );

        return vec;
    }

    // Blueprint Patch::GetElectricBP(Direction output_direction, int min_running_time, int amount) const {
    //    std::scoped_lock lock(m_mutex);

    // }

    int Patch::ResourceAmountNoLock(const Entity &entity) const {
        assert(entity.GetType() == "mining-drill");
        int sum = 0;
        ForEachDiagonal([this, &sum, pos = entity.GetPosition()](Direction d) {
            auto pos_ = pos + MapPosition(d) * 0.5;
            if (m_resources.contains(pos_)) {
                sum += m_resources.at(pos_);
            }
        });
        return sum;
    }

    int Patch::ResourceAmountMinNoLock(const Blueprint &blueprint) const {
        int min = INFINITE >> 1;
        for (const auto &entity : blueprint.entities) {
            if (entity.GetType() != "mining-drill") continue;

            int amount = ResourceAmountNoLock(entity);
            if (amount < min) {
                min = amount;
            }
        }
        return min;
    }

    bool Chunk::Collides(const Area &bounding_box) const {
        for (const auto &entity : m_entities) {
            if (entity->GetBoundingBox().Collides(bounding_box)) return true;
        }
        return false;
    }

    MapPosition MapData::GetPlayerPosition(Branch branch) const {
        assert(branch != PLANNING && "Cannot get the player position from the planning branch.");

        std::scoped_lock lock(m_mutex);
        return m_player_positions[branch];
    }

    void MapData::SetPlayerPosition(const MapPosition &pos, Branch branch) {
        assert(branch != PLANNING && "Cannot set the player position in the planning branch.");

        std::scoped_lock lock(m_mutex);
        m_player_positions[branch] = pos;
    }

    std::future<void> MapData::UpdatePlayerMainInventory() {
        auto inventory_future = m_instance->Request("GetInventory");
        return std::async(std::launch::deferred, [this, inventory_future = std::move(inventory_future)] mutable {
            inventory_future.wait();
            m_player_main_inventory = inventory_future.get()["data"].get<Inventory>();
        });
    }

    SEntity MapData::AddEntityNoLock(const Entity &entity, Branch branch, bool is_auto_place) {
        if (is_auto_place) {
            AddEntityNoLock(entity, MAIN, false);
            AddEntityNoLock(entity, BUILDING, false);
            return nullptr;
        }

        auto &chunks = m_chunks.at(branch);
        const auto chunk_position = entity.GetPosition().ChunkPosition();
        if (!chunks.contains(chunk_position)) {
            ChunkGeneratedNoLock(chunk_position, branch);
        }

        auto &chunk = chunks.at(chunk_position);
        auto s_entity = std::make_shared<Entity>(entity);
        if (branch == MAIN) {
            s_entity->SetInstance(m_instance);
        } else {
            s_entity->SetInstance(nullptr);
        }
        s_entity->SetValid(true);

        chunk.m_entities.push_back(s_entity);

        if (entity.GetType() == "resource") {
            auto patch_it = std::find_if(m_patchs.begin(), m_patchs.end(), [&entity](const SPatch &patch) {
                return patch->TypeToString() == entity.GetName() && patch->GetBoundingBox().Collides(entity.GetPosition());
            });
            
            if (patch_it == m_patchs.end()) {
                auto patch = std::make_shared<Patch>(this, entity.GetName(), entity.GetPosition());
                m_patchs.push_back(patch);
                patch_it = --m_patchs.end();
            }
            
            auto patch = *patch_it;

            const auto &pos = entity.GetPosition();
            auto &left_top = patch->m_bounding_box.left_top,
                 &right_bottom = patch->m_bounding_box.right_bottom;
            patch->m_resource_amount += entity.GetResourceAmount();
            patch->m_resources[entity.GetPosition()] = entity.GetResourceAmount();

            if (pos.x <= left_top.x) left_top.x = pos.x - 1;
            else if (pos.x >= right_bottom.x) right_bottom.x = pos.x + 1;

            if (pos.y <= left_top.y) left_top.y = pos.y - 1;
            else if (pos.y >= right_bottom.y) right_bottom.y = pos.y + 1;

            std::vector<SPatch>::iterator other_it;
            while ((other_it = std::find_if(m_patchs.begin(), m_patchs.end(), [&](const SPatch &other) {
                return patch != other && patch->m_type == other->GetType() && patch->m_bounding_box.Collides(other->GetBoundingBox());
            })) != m_patchs.end()) {
                auto &other = *other_it;

                patch->m_resource_amount += other->m_resource_amount;

                auto &other_left_top = other->m_bounding_box.left_top,
                        &other_right_bottom = other->m_bounding_box.right_bottom;

                if (other_left_top.x <= left_top.x) left_top.x = other_left_top.x;
                else if (other_right_bottom.x >= right_bottom.x) right_bottom.x = other_right_bottom.x;

                if (other_left_top.y <= left_top.y) left_top.y = other_left_top.y;
                else if (other_right_bottom.y >= right_bottom.y) right_bottom.y = other_right_bottom.y;

                patch->m_resources.insert(other->m_resources.begin(), other->m_resources.end());

                m_patchs.erase(other_it);
            }
        }

        auto collides_with_player = g_prototypes.HasCollisionMask(entity, "player");
        if (!collides_with_player) return s_entity;

        // Update pathfinder data
        
        const auto placeable_off_grid = g_prototypes.HasFlag(entity, "placeable-off-grid");
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = HalfFloor(entity.GetBoundingBox().right_bottom.x + n);
        const double y2 = HalfFloor(entity.GetBoundingBox().right_bottom.y + n);

        auto &collisions = m_colliders_entity[branch];
        for (double x1 = HalfCeil(entity.GetBoundingBox().left_top.x - n); x1 <= x2; x1 += 0.5) {
            for (double y1 = HalfCeil(entity.GetBoundingBox().left_top.y - n); y1 <= y2; y1 += 0.5) {
                if (!collisions.contains({x1, y1})) {
                    collisions.emplace(x1, y1);
                }
            }
        }

        return s_entity;
    }

    SEntity MapData::AddEntity(const Entity &entity, Branch branch, bool is_auto_place) {
        std::scoped_lock lock(m_mutex);
        return AddEntityNoLock(entity, branch, is_auto_place);
    }

    std::vector<SEntity> MapData::AddEntities(const Blueprint &blueprint, Branch branch) {
        std::vector<SEntity> vec;
        for (const auto &entity : blueprint.entities) {
            vec.push_back(AddEntity(entity, branch));
        }
        return vec;
    }

    void MapData::RemoveEntity(const std::string &name, const MapPosition &pos, Branch branch) {
        std::scoped_lock lock(m_mutex);
        
        auto &chunks = m_chunks.at(branch);
        auto chunk_position = pos.ChunkPosition();
        if (!chunks.contains(chunk_position)) {
            Warn("Tried to remove entity from mapData in a chunk that is not generated.");
            return;
        }

        auto &chunk = chunks.at(chunk_position);
        auto &entities = chunk.m_entities;

        const auto entity_it = std::find_if(entities.begin(), entities.end(), [&](const SEntity &e) {
            return e->GetName() == name && e->GetPosition() == pos;
        });
        const auto entity = *entity_it;

        Area bounding_box = entity->GetBoundingBox();
        auto collides_with_player = g_prototypes.HasCollisionMask(*entity, "player");
        const auto placeable_off_grid = g_prototypes.HasFlag(*entity, "placeable-off-grid");
        entity->SetValid(false);
        entities.erase(entity_it);

        if (!collides_with_player) return;

        // Update pathfinder data
        
        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double n = placeable_off_grid ? character_size * 2 : character_size;
        const double x2 = HalfFloor(bounding_box.right_bottom.x + n);
        const double y2 = HalfFloor(bounding_box.right_bottom.y + n);

        auto &collisions = m_colliders_entity[branch];
        for (double x1 = HalfCeil(bounding_box.left_top.x - n); x1 <= x2; x1 += 0.5) {
            for (double y1 = HalfCeil(bounding_box.left_top.y - n); y1 <= y2; y1 += 0.5) {
                if (collisions.contains({x1, y1}) && !chunk.Collides(bounding_box)) {
                    collisions.erase({x1, y1});
                }
            }
        }
    }

    std::vector<SEntity> MapData::FindEntities(Branch branch, std::function<bool(const SEntity&)> callback) const {
        std::scoped_lock lock(m_mutex);

        std::vector<SEntity> found;
        const auto &chunks = m_chunks.at(branch);

        for (const auto &[chunk_pos, chunk] : chunks) {
            for (auto &entity : chunk.m_entities) {
                if (!entity->IsValid()) continue;
                if (!callback || callback(entity)) {
                    found.push_back(entity);
                }
            }
        }

        return found;
    }

    std::vector<SEntity> MapData::FindEntities(const Area &area, Branch branch, std::function<bool(const SEntity&)> callback) const {
        std::scoped_lock lock(m_mutex);

        std::vector<SEntity> vec;
        const auto &chunks = m_chunks.at(branch);

        auto chunk_pos_first = area.left_top.ChunkPosition();
        auto chunk_pos_last = area.right_bottom.ChunkPosition();

        for (double x = chunk_pos_first.x; x <= chunk_pos_last.x; x++) {
            for (double y = chunk_pos_first.y; y <= chunk_pos_last.y; y++) {
                MapPosition chunk_pos = MapPosition(x, y);
                if (!chunks.contains(chunk_pos)) continue;
                auto &entities = chunks.at(chunk_pos).m_entities;
                for (auto &entity : entities) {
                    if (!entity->IsValid()) continue;
                    if ((!callback || callback(entity)) && area.Collides(entity->GetBoundingBox())) {
                        vec.push_back(entity);
                    }
                }
            }
        }

        return vec;
    }

    SEntity MapData::FindEntity(const std::string &name, const MapPosition &pos, Branch branch) {
        std::scoped_lock lock(m_mutex);

        auto chunk_pos = pos.ChunkPosition();
        const auto &chunks = m_chunks.at(branch);

        if (!chunks.contains(chunk_pos)) return nullptr;
        auto &entities = chunks.at(chunk_pos).m_entities;
        for (auto &entity : entities) {
            if (entity->GetName() == name && entity->GetPosition() == pos) return entity;
        }

        return nullptr;
    }

    TileType MapData::GetTile(const MapPosition &pos, Branch branch) const {
        std::scoped_lock lock(m_mutex);

        auto &tiles = m_tiles.at(branch);

        if (!tiles.contains(pos)) return TileType::NORMAL;
        else return tiles.at(pos);
    }

    void MapData::SetTile(MapPosition pos, TileType tile, Branch branch, bool is_auto_place) {
        std::scoped_lock lock(m_mutex);

        if (is_auto_place) branch = MAIN;
        auto &tiles = m_tiles.at(branch);

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

        auto &colliders = m_colliders_tile[branch];
        const bool collides = tile == TileType::WATER;
    
        if (collides) {
            if (!colliders.contains(pos)) colliders.insert(pos);
        } else {
            if (colliders.contains(pos)) colliders.erase(pos);
        }

        for (int i = 0; i < straight_vecs.size(); i++) {
            auto corner = pos + straight_vecs[i];
            if (collides) {
                if (!colliders.contains(corner)) {
                    colliders.insert(corner);
                }
                if (is_auto_place) {
                    if (m_colliders_tile[BUILDING].contains(corner)) {
                        m_colliders_tile[BUILDING].insert(corner);
                    }
                    if (m_colliders_tile[PLANNING].contains(corner)) {
                        m_colliders_tile[PLANNING].insert(corner);
                    }
                }
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
                if (!colliders.contains(corner)) {
                    colliders.insert(corner);
                }
                if (is_auto_place) {
                    if (m_colliders_tile[BUILDING].contains(corner)) {
                        m_colliders_tile[BUILDING].insert(corner);
                    }
                    if (m_colliders_tile[PLANNING].contains(corner)) {
                        m_colliders_tile[PLANNING].insert(corner);
                    }
                }
            } else {
                if (!colliders.contains(corner)) continue;
                for (int j : {0, 1, 3}) {
                    auto other_pos = corner + diagonal_vecs[(i + j) % 4];
                    if (tiles[other_pos] == TileType::WATER) goto ContinueOuterLoop;
                }
                colliders.erase(corner);
            }
            ContinueOuterLoop:;
        }
    }

    void MapData::ChunkGenerated(const MapPosition &chunk_position, Branch branch) {
        std::scoped_lock lock(m_mutex);
        ChunkGeneratedNoLock(chunk_position, branch);
    }

    void MapData::ChunkGeneratedNoLock(const MapPosition &chunk_position, Branch branch) {
        auto &chunks = m_chunks.at(branch);

        if (chunks.contains(chunk_position)) return;
        chunks.emplace(chunk_position, chunk_position);

        if (branch != MAIN) return;

        // Update pathfinder data
        auto area = Area::FromChunkPosition(chunk_position);
        auto left_top = area.left_top;
        auto right_bottom = area.right_bottom;
        auto left_bottom = area.GetLeftBottom();
        auto right_top = area.GetRightTop();
        auto &collisions = m_colliders_chunk;

        auto check_chunk = [this, &chunks, &chunk_position, &collisions](const MapPosition &vec,
            MapPosition tile, const MapPosition &last_tile
        ) {
            auto other_chunk = chunk_position + vec;
            bool collide = !chunks.contains(other_chunk);

            if (!collide && vec.x != 0 && vec.y != 0) {
                auto horizontal_chunk = chunk_position + MapPosition(vec.x, 0);
                auto vertical_chunk = chunk_position + MapPosition(0, vec.y);

                collide = !(chunks.contains(horizontal_chunk) &&
                            chunks.contains(vertical_chunk));
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
    
    bool MapData::PathfinderCollides(const MapPosition &pos, Branch branch) const {
        std::scoped_lock lock(m_mutex);

        return m_colliders_chunk.contains(pos) ||
            m_colliders_entity.at(branch).contains(pos) ||
            m_colliders_tile.at(branch).contains(pos);
    }

    bool MapData::PathfinderCollides(const Area &area, Branch branch) const {
        const double x2 = HalfCeil(area.right_bottom.x);
        const double y2 = HalfCeil(area.right_bottom.y);

        for (double x = HalfFloor(area.left_top.x); x <= x2; x += 0.5) {
            for (double y = HalfFloor(area.left_top.y); y <= y2; y += 0.5) {
                if (PathfinderCollides(MapPosition(x, y), branch)) return true;
            }
        }

        return false;
    }

    bool MapData::PathfinderCollides(const Blueprint &blueprint, Branch branch) const {
        for (const auto &entity : blueprint.entities) {
            if (PathfinderCollides(entity.GetBoundingBox(), branch)) return true;
        }

        return false;
    }

    MapPosition MapData::FindNonCollidingPosition(MapPosition pos, Branch branch) const {
        MapPosition d(0, -0.5);

        while (true) {
            if (!PathfinderCollides(pos, branch)) return pos;

            if (pos.x == pos.y || (pos.x < 0 && pos.x == -pos.y) || (pos.x > 0 && pos.x == 1 - pos.y)) {
                d = d.Rotate(Direction::EAST);
            }
            pos += d;
        }
    }

    bool MapData::ResourceEntityCollides(const MapPosition &pos, Patch::Type type) const {
        std::scoped_lock lock(m_mutex);

        for (const auto &patch : m_patchs) {
            if (patch->m_resources.contains(pos)) {
                if (patch->GetType() != type) return true;
                else return false;
            }
        }
        
        return false;
    }

    bool MapData::ResourceEntityCollides(const Area &area, Patch::Type type) const {
        const double x2 = HalfCeil(area.right_bottom.x);
        const double y2 = HalfCeil(area.right_bottom.y);

        for (double x = HalfFloor(area.left_top.x); x <= x2; x += 0.5) {
            for (double y = HalfFloor(area.left_top.y); y <= y2; y += 0.5) {
                if (ResourceEntityCollides(MapPosition(x, y), type)) return true;
            }
        }

        return false;
    }

    bool MapData::ResourceEntityCollides(const Blueprint &blueprint, Patch::Type type) const {
        for (const auto &entity : blueprint.entities) {
            if (entity.GetType() == "mining-drill") {
                double radius = entity.GetPrototype()["resource_searching_radius"].get<double>();
                if (ResourceEntityCollides(Area(entity.GetPosition(), radius), type)) return true;
            }
        }

        return false;
    }

    void MapData::ForPatchs(std::function<void(const SPatch&)> callback) const {
        std::scoped_lock lock(m_mutex);

        for (const auto &patch : m_patchs) {
            callback(patch);
        }
    }

    void MapData::DrawPathfinderData(Branch branch) {
        if (!m_instance) return;
        json chunk_json(m_colliders_chunk);
        m_instance->Request("DrawRectangleBulk", {
            { "positions", chunk_json },
            { "side_length", 0.4 },
            { "color", { 0, 255, 0 } },
            { "filled", false }
        });
        json entity_json(m_colliders_entity.at(branch));
        m_instance->Request("DrawRectangleBulk", {
            { "positions", entity_json },
            { "side_length", 0.4 },
            { "color", { 255, 0, 0 } },
            { "filled", false }
        });
        json tile_json(m_colliders_tile.at(branch));
        m_instance->Request("DrawRectangleBulk", {
            { "positions", tile_json },
            { "side_length", 0.4 },
            { "color", { 0, 0, 255 } },
            { "filled", false }
        });
    }

    void MapData::DrawPatchs() {
        std::scoped_lock lock(m_mutex);

        if (!m_instance) return;
        for (const auto &patch : m_patchs) {
            auto color = std::make_tuple(std::rand() % 256, std::rand() % 256, std::rand() % 256);

            for (const auto &[pos, amount] : patch->m_resources) {
                m_instance->Request("DrawRectangle", {
                    { "area", Area(pos, 0.3) },
                    { "color", color },
                    { "filled", false }
                });
            }

            m_instance->Request("DrawRectangle", {
                { "area", patch->m_bounding_box },
                { "color", { 255, 0, 255 } },
                { "filled", false }
            });
        }
    }
}