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

    class MapData;

    class Patch {
    public:
        enum Type {
            NORMAL,
            FLUID
        };

        Patch() = default;
        Patch(MapData* map_data, Type type, const std::string &name, MapPosition first_pos) :
            m_map_data(map_data),
            m_type(type),
            m_name(name),
            m_bounding_box(first_pos - MapPosition(1, 1), first_pos + MapPosition(1, 1)) {}

        // The burner city will be placed as close as possible to the attraction_point.
        // For example, that point may be the average position of all the patches of the burner city so that everything is close together.
        std::vector<Blueprint> GetBurnerCityBP(const MapPosition &attraction_point, int min_running_time, int amount, FactorioInstance&f) const;
        // output_direction can only be NORTH, SOUTH, WEST or EAST.
        Blueprint GetElectricBP(Direction output_direction, int min_running_time, int amount = -1) const;

        int ResourceAmount(const Entity&) const;
        int ResourceAmountMin(const Blueprint&) const;

        inline Type GetType() const { return m_type; }
        inline const std::string &GetName() const { return m_name; }
        inline Area GetBoundingBox() const { return m_bounding_box; }

    private:
        friend class MapData;
        MapData *m_map_data;
        Type m_type;
        std::string m_name;
        std::unordered_map<MapPosition, int> m_resources;

        int m_resource_amount = 0;
        Area m_bounding_box;
    };

    class Chunk {
    public:
        Chunk(const MapPosition &pos) : m_position(pos) {}

        bool Collides(const Area&) const;

    private:
        friend class MapData;
        MapPosition m_position;
        std::list<Entity> m_entities;
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
            // Should be used for validation when the bot is building (see rest of this comment) and to plan short
            // term things like pathing.
            // Contrary to the 2 other branches, this one has a queue of checkpoints.
            // At the beginning of each task, a new checkpoint should be queued and then validated using
            // the ValidateCheckpoint function at the end of the task. This function compares the MAIN branch with
            // whatever was added to the BUILDING branch between the queueing of the front checkpoint and the
            // queueing of the next one to check that everything that was expected to be built was built.
            // The bot should react if the validation fails (by retrying the task, canceling it, crashing...)
            BUILDING
        };

        void NewCheckpoint();
        inline bool CheckpointEmpty() const {
            return m_checkpoints.empty();
        }
        void ValidateCheckpoint();
        void DestroyCheckpoints();

        MapPosition GetPlayerPosition(Branch) const;
        void SetPlayerPosition(const MapPosition &pos, Branch);
        
        // branch is ignored if is_auto_place is true
        void AddEntity(const Entity&, Branch, bool is_auto_place = false);
        void AddEntities(const Blueprint&, Branch);
        void RemoveEntity(const std::string &name, const MapPosition &pos, Branch);
        template <class T>
        void UpdateEntity(const std::string &name, const MapPosition &pos, const std::string &property, const T &value, Branch);
        std::optional<Entity> FindEntityType(const Area &area, const std::set<std::string> &types, Branch) const;

        TileType GetTile(const MapPosition &pos, Branch) const;
        // branch is ignored if is_auto_place is true
        void SetTile(MapPosition pos, TileType tile, Branch, bool is_auto_place = false);

        void ChunkGenerated(const MapPosition &chunkPos, Branch);

        bool PathfinderCollides(const MapPosition&, Branch) const;
        bool PathfinderCollides(const Area&, Branch) const;
        bool PathfinderCollides(const Blueprint&, Branch) const;

        MapPosition FindNonCollidingPosition(MapPosition, Branch) const;

        bool ResourceEntityCollides(const MapPosition&, const std::string &name) const;
        bool ResourceEntityCollides(const Area&, const std::string &name) const;
        bool ResourceEntityCollides(const Blueprint&, const std::string &name) const;

        void ForPatchs(std::function<void(const Patch&)> callback) const;

        // Debug function
        void DrawPathfinderData(FactorioInstance&, Branch) const;
        void DrawPatchs(FactorioInstance&) const;

    private:
        struct Checkpoint {
            bool position_set = false;
            MapPosition final_player_position;
            std::unordered_map<MapPosition, Chunk> chunks;
            std::unordered_map<MapPosition, TileType> tiles;
        };

        Entity* FindEntity(const std::string &name, const MapPosition &pos, Branch);

        inline const Checkpoint &GetCheckpoint() const {
            if (CheckpointEmpty()) throw RuntimeErrorF("No checkpoint exists.");
            return m_checkpoints.back();
        }
        inline Checkpoint &GetCheckpoint() {
            if (CheckpointEmpty()) throw RuntimeErrorF("No checkpoint exists.");
            return m_checkpoints.back();
        }

        inline std::unordered_map<MapPosition, Chunk> *GetChunks(Branch branch) {
            if (branch == BUILDING) {
                if (CheckpointEmpty()) return nullptr;
                return &GetCheckpoint().chunks;
            }
            return &m_chunks[branch];
        }
        inline std::unordered_map<MapPosition, TileType> *GetTiles(Branch branch) {
            if (branch == BUILDING) {
                if (CheckpointEmpty()) return nullptr;
                return &GetCheckpoint().tiles;
            }
            return &m_tiles[branch];
        }
        inline const std::unordered_map<MapPosition, Chunk> *GetChunksC(Branch branch) const {
            if (branch == BUILDING) {
                if (CheckpointEmpty()) return nullptr;
                return &GetCheckpoint().chunks;
            }
            return &m_chunks.at(branch);
        }
        inline const std::unordered_map<MapPosition, TileType> *GetTilesC(Branch branch) const {
            if (branch == BUILDING) {
                if (CheckpointEmpty()) return nullptr;
                return &GetCheckpoint().tiles;
            }
            return &m_tiles.at(branch);
        }

        void AddEntityNoLock(const Entity&, Branch, bool is_auto_place);

        void ChunkGeneratedNoLock(const MapPosition &chunkPos, Branch);

        MapPosition m_player_position;

        std::map<Branch, std::unordered_map<MapPosition, Chunk>> m_chunks = {
            { MAIN, {} },
            { PLANNING, {} }
        };
        std::map<Branch, std::unordered_map<MapPosition, TileType>> m_tiles = {
            { MAIN, {} },
            { PLANNING, {} }
        };
        
        std::unordered_set<MapPosition> m_colliders_chunk;
        std::map<Branch, std::unordered_set<MapPosition>> m_colliders_entity = {
            { MAIN, {} },
            { PLANNING, {} },
            { BUILDING, {} }
        };
        std::map<Branch, std::unordered_set<MapPosition>> m_colliders_tile = {
            { MAIN, {} },
            { PLANNING, {} },
            { BUILDING, {} }
        };

        // Checkpoints and branch use the same type.
        std::queue<Checkpoint, std::list<Checkpoint>> m_checkpoints;

        std::list<Patch> m_patchs;

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