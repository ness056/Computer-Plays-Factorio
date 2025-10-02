#include "bot.hpp"

#include <queue>

#include "../algorithms/path-finder.hpp"
#include "../algorithms/TSP.hpp"

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

        m_instance.RegisterEvent("Ready", [this](const json&) {
            OnReady();
        });

        m_instance.RegisterEvent("PlayerMoved", [this](const json &event) {
            m_map_data.SetPlayerPosition(event["data"].get<MapPosition>(), false); 
        });

        m_instance.RegisterEvent("EntityAutoPlace", [this](const json &event) {
            auto entities = event["data"].get<std::vector<Entity>>();
            for (const auto &entity : entities) {
                m_map_data.AddEntity(entity, false, true);
            }
        });

        m_instance.RegisterEvent("RemoveEntities", [this](const json &event) {
            auto entities = event["data"].get<std::vector<std::tuple<std::string, MapPosition>>>();
            for (const auto &entity : entities) {
                m_map_data.RemoveEntity(std::get<0>(entity), std::get<1>(entity), false);
            }
        });

        m_instance.RegisterEvent("ChunkGenerated", [this](const json &event) {
            auto pos = event["data"].get<MapPosition>();
            m_map_data.ChunkGenerated(pos, false);
        });

        m_instance.RegisterEvent("SetTiles", [this](const json &event) {
            auto tiles = event["data"].get<std::vector<std::tuple<MapPosition, TileType>>>();
            for (const auto &t : tiles) {
                m_map_data.SetTile(std::get<0>(t), std::get<1>(t), false);
            }
        });

        m_instance.RegisterEvent("ExportPathfinderData", [this](const json&) {
            m_map_data.ExportPathfinderData(false);
        });

        s_burner_bp = DecodeBlueprintStr(s_burner_bp_str);
        s_test_bp = DecodeBlueprintFile(s_test_bp_str);
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
    
    void Bot::BuildBlueprint(Task &task, const Blueprint &blueprint, const MapPosition &offset, Direction direction, bool mirror) {
        using EntityTuple = std::tuple<Entity, bool>;
        using SEntityTuple = std::shared_ptr<EntityTuple>;

        const MapPosition center = (blueprint.center.Rotate(direction) + offset).HalfRound();
        const auto comp = [&center](const SEntityTuple &lhs, const SEntityTuple &rhs) {
            return
                MapPosition::SqDistance(center, std::get<0>(*lhs).GetPosition()) <
                MapPosition::SqDistance(center, std::get<0>(*rhs).GetPosition());
        };

        std::priority_queue<SEntityTuple, std::vector<SEntityTuple>, decltype(comp)> queue(comp);
        std::vector<SEntityTuple> entities;

        const double reach_distance = g_prototypes.Get("character", "character")["reach_distance"];
        const double sq_reach_distance = reach_distance * reach_distance;

        // First we transform all the entities of the blueprint accordingly to the offset, direction and mirror args.

        auto starting_position = m_map_data.GetPlayerPosition(m_map_data.ForksEmpty() ? false : true);
        m_map_data.NewFork();

        for (const auto &e : blueprint.entities) {
            auto copy = std::make_shared<EntityTuple>(e, true);
            auto &entity = std::get<Entity>(*copy);
            entity.SetDirection(direction + entity.GetDirection());
            entity.SetPosition(entity.GetPosition().Rotate(direction) + offset);
            if (mirror) entity.SetMirror(!entity.GetMirror());

            queue.push(copy);
            entities.push_back(copy);
            m_map_data.AddEntity(entity, true);
        }

        // In order to find a path where the bot will be at least once in building range of every entities,
        // we first find a list of points where all entities are in range of at least 1 of those points.
        // Then we link all the points to find the shortest path as the crow flies (which is the travelling
        // salesman problem). And finally we use a normal pathfinder to get the actual path between each points.
        // We also need to find the closest point to the starting location of the bot so that it can join the
        // building path.

        // Find the list of points
        std::vector<std::tuple<MapPosition, std::vector<Entity>, double>> waypoints;
        waypoints.push_back({starting_position, {}, 0});

        while (!queue.empty()) {
            auto &t = queue.top();
            if (!std::get<bool>(*t)) {
                queue.pop();
                continue;
            }
            auto &entity = std::get<Entity>(*t);

            const auto comp2 = [&center](const MapPosition &lhs, const MapPosition &rhs) {
                return MapPosition::SqDistance(center, lhs) < MapPosition::SqDistance(center, rhs);
            };
            std::priority_queue<MapPosition, std::vector<MapPosition>, decltype(comp2)> points(comp2);
            std::unordered_set<MapPosition> visited;

            MapPosition pos = center.HalfRound();
            if (MapPosition::SqDistance(pos, entity.GetPosition()) > sq_reach_distance) {
                double angle = (pos - entity.GetPosition()).Angle();
                pos.x = entity.GetPosition().x + HalfRound(reach_distance * std::cos(angle));
                pos.y = entity.GetPosition().y + HalfRound(reach_distance * std::sin(angle));
            }

            do {
                for (double dx = -0.5; dx <= 0.5; dx += 0.5) {
                    for (double dy = -0.5; dy <= 0.5; dy += 0.5) {
                        if (dx == 0 && dy == 0) continue;
                        MapPosition neighbor = pos + MapPosition(dx, dy);
                        if (
                            !visited.contains(neighbor) &&
                            MapPosition::SqDistance(neighbor, entity.GetPosition()) <= sq_reach_distance
                        ) {
                            points.push(neighbor);
                        }
                    }
                }

                pos = points.top();
                points.pop();
                
                if (!m_map_data.PathfinderCollides(pos, true)) {
                    goto Found;
                }
            } while (!points.empty());

            throw "TODO";

            Found:

            waypoints.push_back({pos, {}, 0});
            auto &vec = std::get<std::vector<Entity>>(waypoints.back());
            auto &max_distance = std::get<double>(waypoints.back());

            for (auto &t_ : entities) {
                auto &valid = std::get<bool>(*t_);
                if (!valid) continue;
                auto &entity_ = std::get<Entity>(*t_);

                double d = MapPosition::SqDistance(pos, entity_.GetPosition());
                if (d > sq_reach_distance) continue;
                
                valid = false;
                if (max_distance < d) max_distance = d;
                vec.emplace_back(std::move(entity_));
            }

            max_distance = reach_distance - std::sqrt(max_distance);

            queue.pop();
        }

        // TSP
        TSP<std::tuple<MapPosition, std::vector<Entity>, double>>
            (waypoints, [](const std::tuple<MapPosition, std::vector<Entity>, double> &e) -> const MapPosition& {
                return std::get<MapPosition>(e);
            }, false
        );

        // Find actual paths
        std::vector<Path> paths;
        paths.reserve(waypoints.size());

        for (int i = 0; i < waypoints.size() - 1; i++) {
            const auto &first = waypoints[i];
            const auto &second = waypoints[i + 1];

            auto path = FindPath(
                m_map_data,
                true,
                std::get<MapPosition>(first),
                std::get<MapPosition>(second),
                std::get<double>(second)
            );
            if (!path) throw "TODO";

            paths.emplace_back(std::move(path.value()));
        }

        m_map_data.SetPlayerPosition(paths.back().back(), true);

        task.QueueInstruction([
            this, waypoints = std::move(waypoints),
            paths = std::move(paths)
        ](FactorioInstance &instance) {
            for (int i = 0; i < paths.size(); i++) {
                auto walk_futur = instance.Request("WalkAndStay", paths[i]);
                auto &waypoint = waypoints[i + 1];
                for (auto entity : std::get<std::vector<Entity>>(waypoint)) {
                    instance.Request("Build", entity);
                }
                instance.Request("WaitAllRangedRequests").wait();
                instance.Request("WalkFinishAndStop").wait();
                walk_futur.wait();
            }

            instance.Request("PauseToggle");
            m_map_data.ValidateAndMergeFork();
        });
    }

    void Bot::MineEntities(Task &, const std::vector<Entity> &entities) {
        const double mine_distance = g_prototypes.Get("character", "character")["reach_resource_distance"];

        std::vector<std::tuple<MapPosition, double>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.GetPosition(), mine_distance);
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