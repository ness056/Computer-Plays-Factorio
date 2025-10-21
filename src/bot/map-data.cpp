#include "map-data.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    std::vector<Blueprint> Patch::GetBurnerCityBP(const MapPosition &attraction_point, int min_running_time, int amount, FactorioInstance&f) const {
        std::vector<Blueprint> vec;

        const int min_resource_amount = min_running_time / 4;
        Direction attraction_direction = CardinalDirection((attraction_point - m_bounding_box.Center()).ToDirection());
        Blueprint blueprint;
        MapPosition step_vec;
        if (GetName() == "coal") {
            blueprint = Blueprint::Load("core/burners/coal.txt");
            step_vec = MapPosition(0, -2);
        }
        else if (GetName() == "stone") {
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
            [this, &first, &blueprint, min_resource_amount, &f](const MapPosition &pos) {
                f.Request("DrawCircle", {
                    { "position", pos },
                    { "radius", 0.2 },
                    { "color", { 255, 0, 0 } },
                    { "filled", true }
                });
                blueprint.Shift(pos);
                int resource_amount = ResourceAmountMin(blueprint);
                if (resource_amount > min_resource_amount &&
                    !m_map_data->PathfinderCollides(blueprint, MapData::PLANNING) &&
                    !m_map_data->ResourceEntityCollides(blueprint, GetName())
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
            [this, &vec, &first, &blueprint, min_resource_amount, amount, &f](const MapPosition &pos) {
                f.Request("DrawCircle", {
                    { "position", pos },
                    { "radius", 0.2 },
                    { "color", { 255, 255, 0 } },
                    { "filled", true }
                });
                blueprint.Shift(pos);
                int resource_amount = ResourceAmountMin(blueprint);
                if (resource_amount > min_resource_amount &&
                    !m_map_data->PathfinderCollides(blueprint, MapData::PLANNING) &&
                    !m_map_data->ResourceEntityCollides(blueprint, GetName())
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

    int Patch::ResourceAmount(const Entity &entity) const {
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

    int Patch::ResourceAmountMin(const Blueprint &blueprint) const {
        int min = INFINITE >> 1;
        for (const auto &entity : blueprint.entities) {
            if (entity.GetType() != "mining-drill") continue;

            int amount = ResourceAmount(entity);
            if (amount < min) {
                min = amount;
            }
        }
        return min;
    }

    bool Chunk::Collides(const Area &bounding_box) const {
        for (const auto &entity : m_entities) {
            if (entity.GetBoundingBox().Collides(bounding_box)) return true;
        }
        return false;
    }
    
    void MapData::NewCheckpoint() {        
        if (CheckpointEmpty()) {
            auto &checkpoint = m_checkpoints.emplace();
            checkpoint.chunks = *GetChunks(MAIN);
            checkpoint.tiles = *GetTiles(MAIN);
        } else {
            auto chunks = *GetChunks(BUILDING);
            auto tiles = *GetTiles(BUILDING);

            auto &checkpoint = m_checkpoints.emplace();
            checkpoint.chunks = std::move(chunks);
            checkpoint.tiles = std::move(tiles);
        }
    }

    static void CheckpointValidationFailed() {
        throw RuntimeErrorF("MapData checkpoint validation failed.");
    }

    void MapData::ValidateCheckpoint() {
        std::scoped_lock lock(m_mutex);

        const auto &checkpoint = m_checkpoints.front();

        if (!checkpoint.position_set) CheckpointValidationFailed();

        if ((checkpoint.final_player_position - m_player_position).Trunc() != MapPosition(0, 0)) {
            CheckpointValidationFailed();
        }

        auto &tiles = m_tiles[MAIN];
        auto &chunks = m_chunks[MAIN];

        for (const auto &[pos, tile] : checkpoint.tiles) {
            if (tile == TileType::NORMAL && tiles.contains(pos) && tiles[pos] != TileType::NORMAL) {
                CheckpointValidationFailed();
            } else if (tile == TileType::WATER && (!tiles.contains(pos) || tiles[pos] != TileType::WATER)) {
                CheckpointValidationFailed();
            }
        }

        for (const auto &[chunk_pos, chunk] : checkpoint.chunks) {
            if (!chunks.contains(chunk_pos)) {
                CheckpointValidationFailed();
            }
            const auto &other_chunk = chunks.at(chunk_pos);
            
            for (const auto &entity : chunk.m_entities) {
                for (const auto &other_entity : other_chunk.m_entities) {
                    if (entity == other_entity) {
                        goto Found;
                    }
                }
                CheckpointValidationFailed();
                Found:;
            }
        }

        m_checkpoints.pop();
    }

    void MapData::DestroyCheckpoints() {
        std::scoped_lock lock(m_mutex);

        while (!m_checkpoints.empty()) {
            m_checkpoints.pop();
        }

        m_colliders_entity[BUILDING] = m_colliders_entity[MAIN];
        m_colliders_tile[BUILDING] = m_colliders_tile[MAIN];
    }

    MapPosition MapData::GetPlayerPosition(Branch branch) const {
        assert(branch != PLANNING && "Cannot get the player position from the planning branch.");

        std::scoped_lock lock(m_mutex);

        if (branch == MAIN || CheckpointEmpty() ) {
            return m_player_position;
        } else {
            const auto &checkpoint = GetCheckpoint();
            if (checkpoint.position_set) return checkpoint.final_player_position;
            else throw RuntimeErrorF("Final player position not set.");
        }
    }

    void MapData::SetPlayerPosition(const MapPosition &pos, Branch branch) {
        assert(branch != PLANNING && "Cannot set the player position in the planning branch.");

        std::scoped_lock lock(m_mutex);

        if (branch == MAIN) {
            m_player_position = pos;
        } else {
            auto &checkpoint = GetCheckpoint();
            checkpoint.final_player_position = pos;
            checkpoint.position_set = true;
        }
    }

    void MapData::AddEntityNoLock(const Entity &entity, Branch branch, bool is_auto_place) {
        if (is_auto_place) {
            AddEntityNoLock(entity, MAIN, false);
            AddEntityNoLock(entity, BUILDING, false);
            return;
        }
        auto chunks_ptr = GetChunks(branch);
        if (chunks_ptr) {
            auto &chunks = *chunks_ptr;

            const auto chunk_position = entity.GetPosition().ChunkPosition();
            if (!chunks.contains(chunk_position)) {
                ChunkGeneratedNoLock(chunk_position, branch);
            }

            auto &chunk = chunks.at(chunk_position);
            chunk.m_entities.push_back(entity);
        }

        if (entity.GetType() == "resource") {
            auto patch = std::find_if(m_patchs.begin(), m_patchs.end(), [&entity](const Patch &patch) {
                return patch.m_name == entity.GetName() && patch.m_bounding_box.Collides(entity.GetPosition());
            });
            
            auto &p = g_prototypes.Get(entity);
            if (patch == m_patchs.end()) {
                Patch::Type type = p.contains("category") && p["category"] == "basic-fluid" ? Patch::FLUID : Patch::NORMAL;
                m_patchs.emplace_back(this, type, entity.GetName(), entity.GetPosition());
                patch = --m_patchs.end();
            }

            const auto &pos = entity.GetPosition();
            auto &left_top = patch->m_bounding_box.left_top,
                 &right_bottom = patch->m_bounding_box.right_bottom;
            patch->m_resource_amount += entity.GetResourceAmount();
            patch->m_resources[entity.GetPosition()] = entity.GetResourceAmount();

            if (pos.x <= left_top.x) left_top.x = pos.x - 1;
            else if (pos.x >= right_bottom.x) right_bottom.x = pos.x + 1;

            if (pos.y <= left_top.y) left_top.y = pos.y - 1;
            else if (pos.y >= right_bottom.y) right_bottom.y = pos.y + 1;

            std::list<Patch>::iterator other;
            while ((other = std::find_if(m_patchs.begin(), m_patchs.end(), [&](const Patch &other) {
                return &*patch != &other && patch->m_name == other.m_name && patch->m_bounding_box.Collides(other.m_bounding_box);
            })) != m_patchs.end()) {
                patch->m_resource_amount += other->m_resource_amount;

                auto &other_left_top = other->m_bounding_box.left_top,
                     &other_right_bottom = other->m_bounding_box.right_bottom;

                if (other_left_top.x <= left_top.x) left_top.x = other_left_top.x;
                else if (other_right_bottom.x >= right_bottom.x) right_bottom.x = other_right_bottom.x;

                if (other_left_top.y <= left_top.y) left_top.y = other_left_top.y;
                else if (other_right_bottom.y >= right_bottom.y) right_bottom.y = other_right_bottom.y;

                patch->m_resources.insert(other->m_resources.begin(), other->m_resources.end());

                m_patchs.erase(other);
            }
        }

        auto collides_with_player = g_prototypes.HasCollisionMask(entity, "player");
        if (!collides_with_player) return;

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
                    if (is_auto_place) {
                        m_colliders_entity[BUILDING].emplace(x1, y1);
                    }
                }
            }
        }
    }

    void MapData::AddEntity(const Entity &entity, Branch branch, bool is_auto_place) {
        std::scoped_lock lock(m_mutex);
        AddEntityNoLock(entity, branch, is_auto_place);
    }

    void MapData::AddEntities(const Blueprint &blueprint, Branch branch) {
        for (const auto &entity : blueprint.entities) {
            AddEntity(entity, branch);
        }
    }

    void MapData::RemoveEntity(const std::string &name, const MapPosition &pos, Branch branch) {
        std::scoped_lock lock(m_mutex);
        
        auto chunks_ptr = GetChunks(branch);
        if (!chunks_ptr) return;
        auto &chunks = *chunks_ptr;

        auto chunk_position = pos.ChunkPosition();
        if (!chunks.contains(chunk_position)) {
            Warn("Tried to remove entity from mapData in a chunk that is not generated.");
            return;
        }

        auto &chunk = chunks.at(chunk_position);
        auto &entities = chunk.m_entities;

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

        auto &collisions = m_colliders_entity[branch];
        for (double x1 = HalfCeil(bounding_box.left_top.x - n); x1 <= x2; x1 += 0.5) {
            for (double y1 = HalfCeil(bounding_box.left_top.y - n); y1 <= y2; y1 += 0.5) {
                if (collisions.contains({x1, y1}) && !chunk.Collides(bounding_box)) {
                    collisions.erase({x1, y1});
                }
            }
        }
    }

    std::optional<Entity> MapData::FindEntityType(const Area &area, const std::set<std::string> &types, Branch branch) const {
        std::scoped_lock lock(m_mutex);

        auto chunk_pos_first = area.left_top.ChunkPosition();
        auto chunk_pos_last = area.right_bottom.ChunkPosition();

        auto chunks_ptr = GetChunksC(branch);
        if (!chunks_ptr) return std::nullopt;
        auto &chunks = *chunks_ptr;

        for (double x = chunk_pos_first.x; x <= chunk_pos_last.x; x++) {
            for (double y = chunk_pos_first.y; y <= chunk_pos_last.y; y++) {
                MapPosition chunk_pos = MapPosition(x, y);
                if (!chunks.contains(chunk_pos)) continue;
                auto &entities = chunks.at(chunk_pos).m_entities;
                for (auto &entity : entities) {
                    if (types.contains(entity.GetType()) && area.Collides(entity.GetBoundingBox())) {
                        return entity;
                    }
                }
            }
        }

        return std::nullopt;
    }

    Entity* MapData::FindEntity(const std::string &name, const MapPosition &pos, Branch branch) {
        std::scoped_lock lock(m_mutex);

        auto chunk_pos = pos.ChunkPosition();
        auto chunks_ptr = GetChunks(branch);
        if (!chunks_ptr) return nullptr;
        auto &chunks = *chunks_ptr;

        if (!chunks.contains(chunk_pos)) return nullptr;
        auto &entities = chunks.at(chunk_pos).m_entities;
        for (auto &entity : entities) {
            if (entity.GetName() == name && entity.GetPosition() == pos) return &entity;
        }

        return nullptr;
    }

    TileType MapData::GetTile(const MapPosition &pos, Branch branch) const {
        std::scoped_lock lock(m_mutex);

        auto tiles_op = GetTilesC(branch);
        if (!tiles_op) return TileType::NORMAL;
        auto &tiles = *tiles_op;

        if (!tiles.contains(pos)) return TileType::NORMAL;
        else return tiles.at(pos);
    }

    void MapData::SetTile(MapPosition pos, TileType tile, Branch branch, bool is_auto_place) {
        std::scoped_lock lock(m_mutex);

        if (is_auto_place) branch = MAIN;
        auto tiles_op = GetTiles(branch);
        if (!tiles_op) return;
        auto &tiles = *tiles_op;

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
                    if (tiles[other_pos] == TileType::WATER) goto ContinueTwice;
                }
                colliders.erase(corner);
            }
            ContinueTwice:;
        }
    }

    void MapData::ChunkGenerated(const MapPosition &chunk_position, Branch branch) {
        std::scoped_lock lock(m_mutex);
        ChunkGeneratedNoLock(chunk_position, branch);
    }

    void MapData::ChunkGeneratedNoLock(const MapPosition &chunk_position, Branch branch) {
        auto chunks_ptr = GetChunks(branch);
        if (!chunks_ptr) return;
        auto &chunks = *chunks_ptr;

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

    bool MapData::ResourceEntityCollides(const MapPosition &pos, const std::string &name) const {
        for (const auto &patch : m_patchs) {
            if (patch.m_resources.contains(pos)) {
                if (patch.GetName() != name) return true;
                else return false;
            }
        }
        
        return false;
    }

    bool MapData::ResourceEntityCollides(const Area &area, const std::string &name) const {
        const double x2 = HalfCeil(area.right_bottom.x);
        const double y2 = HalfCeil(area.right_bottom.y);

        for (double x = HalfFloor(area.left_top.x); x <= x2; x += 0.5) {
            for (double y = HalfFloor(area.left_top.y); y <= y2; y += 0.5) {
                if (ResourceEntityCollides(MapPosition(x, y), name)) return true;
            }
        }

        return false;
    }

    bool MapData::ResourceEntityCollides(const Blueprint &blueprint, const std::string &name) const {
        for (const auto &entity : blueprint.entities) {
            if (entity.GetType() == "mining-drill") {
                double radius = entity.GetPrototype()["resource_searching_radius"].get<double>();
                if (ResourceEntityCollides(Area(entity.GetPosition(), radius), name)) return true;
            }
        }

        return false;
    }

    void MapData::ForPatchs(std::function<void(const Patch&)> callback) const {
        std::scoped_lock lock(m_mutex);

        for (const auto &patch : m_patchs) {
            callback(patch);
        }
    }

    void MapData::DrawPathfinderData(FactorioInstance &f, Branch branch) const {
        json chunk_json(m_colliders_chunk);
        f.Request("DrawRectangleBulk", {
            { "positions", chunk_json },
            { "side_length", 0.4 },
            { "color", { 0, 255, 0 } },
            { "filled", false }
        });
        json entity_json(m_colliders_entity.at(branch));
        f.Request("DrawRectangleBulk", {
            { "positions", entity_json },
            { "side_length", 0.4 },
            { "color", { 255, 0, 0 } },
            { "filled", false }
        });
        json tile_json(m_colliders_tile.at(branch));
        f.Request("DrawRectangleBulk", {
            { "positions", tile_json },
            { "side_length", 0.4 },
            { "color", { 0, 0, 255 } },
            { "filled", false }
        });
    }

    void MapData::DrawPatchs(FactorioInstance &instance) const {
        for (const auto &patch : m_patchs) {
            auto color = std::make_tuple(std::rand() % 256, std::rand() % 256, std::rand() % 256);

            for (const auto &[pos, amount] : patch.m_resources) {
                instance.Request("DrawRectangle", {
                    { "area", Area(pos, 0.3) },
                    { "color", color },
                    { "filled", false }
                });
            }

            instance.Request("DrawRectangle", {
                { "area", patch.m_bounding_box },
                { "color", { 255, 0, 255 } },
                { "filled", false }
            });
        }
    }
}