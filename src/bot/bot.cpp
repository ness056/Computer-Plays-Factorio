#include "bot.hpp"

#include <queue>

#include "../algorithms/path-finder.hpp"

namespace ComputerPlaysFactorio {

    bool BotFactory::Register(const std::string &name, const FactoryFunction &factory) {
        if (s_bot_factories.contains(name)) {
            throw RuntimeErrorF("A bot with name {} is already registered.", name);
        }

        s_bot_factories[name] = factory;
        return true;
    }

    Bot::Bot() : m_instance(FactorioInstance::GRAPHICAL) {
        m_instance.SetExitedCallback([this](int) {
            Stop();
        });

        m_instance.RegisterEvent("Ready", [this](const EventBase&) {
            OnReady();
        });

        m_instance.RegisterEvent<std::vector<Entity>>("AddEntities", [this](const Event<std::vector<Entity>> &event) {
            for (const auto &entity : event.data) {
                m_map_data.AddEntity(entity);
            }
        });

        m_instance.RegisterEvent<std::vector<std::tuple<std::string, MapPosition>>>("RemoveEntities",
            [this](const Event<std::vector<std::tuple<std::string, MapPosition>>> &event) {
                for (const auto &entity : event.data) {
                    m_map_data.RemoveEntity(std::get<0>(entity), std::get<1>(entity));
                }
            }
        );

        m_instance.RegisterEvent<MapPosition>("ChunkGenerated", [this](const Event<MapPosition> &event) {
            m_map_data.ChunkGenerated(event.data);
        });

        m_instance.RegisterEvent<std::vector<std::tuple<MapPosition, TileType>>>("SetTiles",
            [this](const Event<std::vector<std::tuple<MapPosition, TileType>>> &event) {
                for (const auto &t : event.data) {
                    m_map_data.SetTile(std::get<0>(t), std::get<1>(t));
                }
            }
        );

        m_instance.RegisterEvent("ExportPathfinderData", [this](const EventBase&) {
            m_map_data.ExportPathfinderData();
        });

        s_burner_bp = DecodeBlueprint(s_burner_bp_str);
        s_test_bp = DecodeBlueprint(s_test_bp_str);
    }
   
    Result Bot::Start() {
        std::string message;
        if (Running()) return INSTANCE_ALREADY_RUNNING;

        auto result = m_instance.Start(123);
        if (result != SUCCESS) return result;

        m_exit = false;
        Loop();

        return SUCCESS;
    }
   
    void Bot::Stop() {
        ClearInstructions();
        m_exit = true;
        m_instance.Stop();
    }

    size_t Bot::InstructionCount() {
        std::scoped_lock lock(m_instruction_mutex);
        size_t sum = 0;
        for (auto &task : m_tasks) {
            sum += task.InstructionCount();
        }
        return sum;
    }

    Instruction *Bot::GetInstruction() {
        while (true) {
            std::unique_lock lock(m_instruction_mutex);
            if (!m_tasks.empty()) {
                auto instruction = m_tasks.front().GetInstruction();
                if (instruction) return instruction;
            }
    
            m_instruction_cond.wait(lock);
        }
    }

    void Bot::PopInstruction() {
        std::scoped_lock lock(m_instruction_mutex);
        if (m_tasks.empty()) return;
        m_tasks.front().PopInstruction();
    }

    void Bot::ClearInstructions() {
        std::scoped_lock lock(m_instruction_mutex);
        m_tasks.clear();
    }

    Task &Bot::QueueTask() {
        std::scoped_lock lock(m_instruction_mutex);
        return m_tasks.emplace_back(m_instruction_cond);
    }

    void Bot::PopTask() {
        std::scoped_lock lock(m_instruction_mutex);
        m_tasks.pop_front();
    }

    void Bot::Join() {
        if (m_loop_thread.joinable()) m_loop_thread.join();
        m_instance.Join();
    }

    bool Bot::Running() {
        return m_instance.Running();
    }

    void Bot::Loop() {
        while(!m_exit) {
            Instruction *instruction = GetInstruction();
            if (instruction->GetType() == Instruction::TASK_END) {
                PopTask();
                continue;
            }

            instruction->Call(m_instance);
            PopInstruction();
        }
    }

    struct BuildBlueprintCmp {
        bool operator()(const std::shared_ptr<Entity> &lhs, const std::shared_ptr<Entity> &rhs) {
            return lhs->position.x < rhs->position.x && lhs->position.y < rhs->position.y;
        }
    };
    
    void Bot::BuildBlueprint(Task &task, const Blueprint &blueprint, const MapPosition &offset, Direction direction, bool mirror) {
        std::priority_queue<SEntity, std::vector<SEntity>, BuildBlueprintCmp> queue;
        std::vector<SEntity> entities;
        std::unordered_set<MapPosition> colliders;

        const double character_size = g_prototypes.Get("character", "character")["collision_box"][1][0];
        const double reach_distance = g_prototypes.Get("character", "character")["reach_distance"];
        const double sq_reach_distance = reach_distance * reach_distance;

        // First we transform all the entities of the blueprint accordingly to the offset, direction and mirror args.

        for (const auto &e : blueprint.entities) {
            auto copy = std::make_shared<Entity>(e);
            copy->direction += direction;
            copy->position = copy->position.Rotate(direction);
            copy->position += offset;
            if (mirror) copy->mirror = !copy->mirror;

            queue.push(copy);
            entities.push_back(copy);

            auto &p = g_prototypes.Get(e);
            if (!p.contains("collision_box")) continue;
            auto &c = p["collision_box"];
            Area bounding_box = Area({c[0][0], c[0][1]}, {c[1][0], c[1][1]}) + copy->position;

            const double x2 = std::floor((bounding_box.right_bottom.x + character_size) * 2) / 2;
            const double y2 = std::floor((bounding_box.right_bottom.y + character_size) * 2) / 2;

            for (double x1 = std::ceil((bounding_box.left_top.x - character_size) * 2) / 2; x1 <= x2; x1 += 0.5) {
                for (double y1 = std::ceil((bounding_box.left_top.y - character_size) * 2) / 2; y1 <= y2; y1 += 0.5) {
                    if (!colliders.contains({x1, y1})) {
                        colliders.emplace(x1, y1);
                    }
                }
            }
        }

        // In order to find a path where the bot will be at least once in building range of every entities,
        // we first find a list of points where all entities are in range of at least 1 of those points.
        // Then we link all the points to find the shortest path as the crow flies (which is the travelling
        // salesman problem). And finally we use a normal pathfinder to get the actual path between each points.
        // We also need to find the closest point to the starting location of the bot so that it can join the
        // building path.

        // Find the list of points
        std::vector<std::tuple<MapPosition, std::vector<SEntity>, double>> waypoints;
        while (!queue.empty()) {
            auto e = queue.top();
            queue.pop();
            if (!e->valid) continue;

            MapPosition pos = e->position;
            double ceiled_x = std::ceil(e->position.x * 2) / 2;
            for (double ix = -reach_distance; ix <= reach_distance; ix += 0.5) {
                double x = ix + ceiled_x;
                double dy = std::sqrt(reach_distance * reach_distance - ix * ix);
                for (double y = std::ceil((e->position.y - dy) * 2) / 2; y < std::floor((e->position.y + dy) * 2) / 2; y += 0.5) {
                    if (!m_map_data.PathfinderCollides({x, y}) && !colliders.contains({x, y})) {
                        pos = {x, y};
                        goto Found;
                    }
                }
            }


            Found:

            waypoints.push_back({pos, {}, 0});
            auto &vec = std::get<std::vector<SEntity>>(waypoints.back());
            auto &max_distance = std::get<double>(waypoints.back());

            for (auto &e_ : entities) {
                if (!e_->valid) continue;

                double d = MapPosition::SqDistance(pos, e_->position);
                if (d > sq_reach_distance) continue;
                
                e_->valid = false;
                if (max_distance < d) max_distance = d;
                vec.push_back(e_);
            }

            max_distance = reach_distance - std::sqrt(max_distance);
        }

        // TSP
        std::vector<std::tuple<MapPosition, std::vector<SEntity>, double>> waypoints_final;
        if (waypoints.empty()) throw "TODO";
        auto &p = waypoints_final.emplace_back(std::move(waypoints.back()));
        MapPosition prev_pos = std::get<MapPosition>(p);
        waypoints.pop_back();

        while (!waypoints.empty()) {
            double min_distance = INFINITY;
            std::vector<std::tuple<MapPosition, std::vector<SEntity>, double>>::iterator min_distance_it;

            auto it = waypoints.begin();
            for (; it != waypoints.end(); it++) {
                double d = MapPosition::SqDistance(prev_pos, std::get<MapPosition>(*it));
                if (d < min_distance) {
                    min_distance = d;
                    min_distance_it = it;
                }
            }

            prev_pos = std::get<MapPosition>(*min_distance_it);
            waypoints_final.emplace_back(std::move(*min_distance_it));
            waypoints.erase(min_distance_it);
        }

        // Find actual paths
        std::vector<Path> paths;
        auto end = waypoints_final.end();
        for (auto first = waypoints_final.begin(); first != end; first++) {
            auto second = std::next(first);
            if (second == end) {
                second = waypoints_final.begin();
                if (first == second) break;
            }

            auto path = FindPath(
                m_map_data,
                std::get<MapPosition>(*first),
                std::get<MapPosition>(*second),
                std::get<double>(*second),
                &colliders
            );
            if (!path) throw "TODO";

            if (path) paths.emplace_back(std::move(path.value()));
            else paths.emplace_back();
        }

        task.QueueInstruction([
            this, waypoints = std::move(waypoints_final),
            paths = std::move(paths), colliders = std::move(colliders)
        ](FactorioInstance &instance) {
            auto player_pos_futur = instance.PlayerPosition();
            player_pos_futur.wait();
            auto player_pos_r = player_pos_futur.get();

            if (player_pos_r.error != RequestError::SUCCESS || !player_pos_r.data) throw "TODO";

            auto &player_pos = player_pos_r.data.value();
            double min_distance = INFINITY;
            size_t current_idx = 0;

            for (size_t i = 0; i < waypoints.size(); i++) {
                double d = MapPosition::SqDistance(player_pos, std::get<MapPosition>(waypoints[i]));
                if (d < min_distance) {
                    min_distance = d;
                    current_idx = i;
                }
            }

            auto &current_waypoint = waypoints[current_idx];
            auto join_path = FindPath(
                m_map_data,
                player_pos,
                std::get<MapPosition>(current_waypoint),
                std::get<double>(current_waypoint),
                &colliders
            );
            if (!join_path) throw;

            {
                auto walk_futur = instance.RequestNoRes("Walk", join_path.value());
                for (auto entity : std::get<std::vector<SEntity>>(current_waypoint)) {
                    auto build_futur = instance.RequestNoRes("Build", entity);
                    build_futur.wait();
                }
                walk_futur.wait();
            }

            if (paths.size() == 0) return;
            auto first_idx = current_idx;
            do {
                auto walk_futur = instance.RequestNoRes("Walk", paths[current_idx]);
                auto &waypoint = waypoints[current_idx + 1 == paths.size() ? 0 : current_idx + 1];
                for (auto entity : std::get<std::vector<SEntity>>(waypoint)) {
                    instance.RequestNoRes("Build", entity).wait();
                }
                walk_futur.wait();

                current_idx++;
                if (current_idx == paths.size()) current_idx = 0;
            } while (first_idx != current_idx);
        });
    }

    void Bot::MineEntities(Task &, const std::vector<Entity> &entities) {
        const double mine_distance = g_prototypes.Get("character", "character")["reach_resource_distance"];

        std::vector<std::tuple<MapPosition, double>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.position, mine_distance);
        }

        // auto paths = FindMultiPath(m_map_data, {0, 0}, points);

        // task.QueueInstruction([paths](FactorioInstance &instance) {
        //     for (auto &path : paths) {
        //         instance.RequestNoRes("Walk", std::get<Path>(path)).wait();
        //         instance.RequestNoRes("Mine", std::get<MapPosition>(path)).wait();
        //     }
        // });
    }

    void Bot::BuildBurnerCity(int, int, int, int) {
        auto &task = QueueTask();
        BuildBlueprint(task, s_test_bp, {0, 0}, Direction::NORTH, false);
        // BuildBlueprint(task, s_test_bp, {-30, 10}, Direction::NORTH, false);
        
        // auto entities_future = m_instance.Request<EntitySearchFilters, std::vector<Entity>>(
        //     "FindEntitiesFiltered", EntitySearchFilters{.area = {{-50, -50}, {60, 40}}, .name = {"huge-rock"}});
        // entities_future.wait();
        // auto entities = entities_future.get();

        // if (entities.error != RequestError::SUCCESS) {
        //     Warn("QueueBuildBurnerCity failed: {}", (int)entities.error);
        //     return;
        // }

        // MineEntities(task, entities.data.value());
    }
}