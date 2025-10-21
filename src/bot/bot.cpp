#include "bot.hpp"

#include <queue>

#include "../algorithms/path-finder.hpp"
#include "../algorithms/TSP.hpp"
#include "../config.hpp"

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
            m_map_data.SetPlayerPosition(event["data"].get<MapPosition>(), MapData::MAIN); 
        });

        m_instance.RegisterEvent("EntityAutoPlace", [this](const json &event) {
            auto entities = event["data"].get<std::vector<Entity>>();
            for (const auto &entity : entities) {
                m_map_data.AddEntity(entity, MapData::MAIN, true);
            }
        });

        m_instance.RegisterEvent("RemoveEntities", [this](const json &event) {
            auto entities = event["data"].get<std::vector<std::tuple<std::string, MapPosition>>>();
            for (const auto &entity : entities) {
                m_map_data.RemoveEntity(std::get<0>(entity), std::get<1>(entity), MapData::MAIN);
            }
        });

        m_instance.RegisterEvent("ChunkGenerated", [this](const json &event) {
            auto pos = event["data"].get<MapPosition>();
            m_map_data.ChunkGenerated(pos, MapData::MAIN);
        });

        m_instance.RegisterEvent("SetTiles", [this](const json &event) {
            auto tiles = event["data"].get<std::vector<std::tuple<MapPosition, TileType, bool>>>();
            for (const auto &t : tiles) {
                m_map_data.SetTile(
                    std::get<0>(t),
                    std::get<1>(t),
                    MapData::MAIN,
                    std::get<2>(t)
                );
            }
        });

        m_instance.RegisterEvent("DrawPathfinderData", [this](const json &j) {
            m_map_data.DrawPathfinderData(m_instance, j["data"].get<MapData::Branch>());
        });

        m_instance.RegisterEvent("DrawPatchs", [this](const json&) {
            m_map_data.DrawPatchs(m_instance);
        });
    }
   
    Result Bot::Start() {
        std::string message;
        if (Running()) return INSTANCE_ALREADY_RUNNING;

        auto result = m_instance.Start(1234);
        if (result != SUCCESS) return result;

        m_exit = false;
        Loop();

        return SUCCESS;
    }
   
    void Bot::Stop() {
        ClearSubTasks();
        m_exit = true;
        m_instance.Stop();
    }

    size_t Bot::SubTaskCount() {
        std::scoped_lock lock(m_sub_task_mutex);
        size_t sum = 0;
        for (auto &task : m_tasks) {
            sum += task.SubTaskCount();
        }
        return sum;
    }

    SubTask *Bot::GetSubTask() {
        while (true) {
            std::unique_lock lock(m_sub_task_mutex);
            if (!m_tasks.empty()) {
                auto sub_task = m_tasks.front().GetSubTask();
                if (sub_task) return sub_task;
            }
    
            m_sub_task_cond.wait(lock);
        }
    }

    void Bot::PopSubTask() {
        std::scoped_lock lock(m_sub_task_mutex);
        if (m_tasks.empty()) return;
        m_tasks.front().PopSubTask();
    }

    void Bot::ClearSubTasks() {
        std::scoped_lock lock(m_sub_task_mutex);
        m_tasks.clear();
    }

    Task &Bot::QueueTask() {
        std::scoped_lock lock(m_sub_task_mutex);
        return m_tasks.emplace_back(m_sub_task_cond);
    }

    void Bot::PopTask() {
        std::scoped_lock lock(m_sub_task_mutex);
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
            SubTask *sub_task = GetSubTask();
            if (sub_task->GetType() == SubTask::TASK_END) {
                PopTask();
                continue;
            }

            sub_task->Call(m_instance);
            PopSubTask();
        }
    }

    std::future<json> Bot::Build(const Entity &entity) {
        auto promise = std::make_shared<std::promise<json>>();

        m_instance.Request("Build", json(entity), [this, promise, entity](const json &j) {
            if (j["success"].get<bool>()) {
                m_map_data.AddEntity(entity, MapData::MAIN);
            }
            promise->set_value(j);
        });

        return promise->get_future();
    }

    std::future<json> Bot::Mine(const std::string &name, const MapPosition &pos) {
        auto promise = std::make_shared<std::promise<json>>();

        m_instance.Request("Mine", {
            {"name", name},
            {"pos", pos}
        }, [this, promise, name, pos](const json &j) {
            if (j["success"].get<bool>()) {
                m_map_data.RemoveEntity(name, pos, MapData::MAIN);
            }
            promise->set_value(j);
        });

        return promise->get_future();
    }
    
    void Bot::BuildBlueprint(Task &task, const Blueprint &blueprint, const MapPosition &offset, Direction direction, bool mirror) {
        struct Entity_ {
            Entity entity;
            std::string name;
            MapPosition pos;
            bool to_build;  // If false, the entity corresponds to an entity that needs to be mined.
            bool assigned;

            Entity_(const Entity &e) : entity(e), to_build(true), assigned(false) {}
            Entity_(const std::string &n, const MapPosition &p) : name(n), pos(p), to_build(false), assigned(false) {}

            inline auto &GetPosition() {
                return to_build ? entity.GetPosition() : pos;
            }
        };
        using SEntity_ = std::shared_ptr<Entity_>;

        const MapPosition center = (blueprint.center.Rotate(direction) + offset).HalfRound();

        if (g_config.debug_path) {
            m_instance.Request("DrawCircle", {
                {"position", center},
                {"radius", 0.5},
                {"color", {0, 0, 255}},
                {"filled", true}
            });
        }

        const auto comp = [&center](const SEntity_ &lhs, const SEntity_ &rhs) {
            return
                (lhs->to_build == true && rhs->to_build == false) || (
                    lhs->to_build == rhs->to_build &&
                    MapPosition::SqDistance(center, lhs->GetPosition()) <
                    MapPosition::SqDistance(center, rhs->GetPosition())
                );
        };

        std::priority_queue<SEntity_, std::vector<SEntity_>, decltype(comp)> queue(comp);
        std::vector<SEntity_> entities;

        const double reach_distance = g_prototypes.Get("character", "character")["reach_distance"];
        const double sq_reach_distance = reach_distance * reach_distance;

        const double mine_distance = g_prototypes.Get("character", "character")["reach_resource_distance"];
        const double sq_mine_distance = mine_distance * mine_distance;

        // First we transform all the entities of the blueprint accordingly to the offset, direction and mirror args.

        auto starting_position = m_map_data.GetPlayerPosition(MapData::BUILDING);
        m_map_data.NewCheckpoint();

        for (const auto &e : blueprint.entities) {
            auto copy = std::make_shared<Entity_>(e);
            auto &entity = copy->entity;
            entity.SetDirection(direction + entity.GetDirection());
            entity.SetPosition(entity.GetPosition().Rotate(direction) + offset);
            if (mirror) entity.SetMirror(!entity.GetMirror());

            queue.push(copy);
            entities.push_back(copy);
            m_map_data.AddEntity(entity, MapData::BUILDING);
        }

        size_t size = entities.size();
        for (int i = 0; i < size; i++) {
            while (true) {
                const auto &e = entities[i];

                auto other = m_map_data.FindEntityType(
                    e->entity.GetBoundingBox(),
                    { "tree", "simple-entity" },
                    MapData::BUILDING
                );

                if (!other) break;
                m_map_data.RemoveEntity(other->GetName(), other->GetPosition(), MapData::BUILDING);
                auto s = std::make_shared<Entity_>(other->GetName(), other->GetPosition());
                queue.push(s);
                entities.push_back(s);
            }
        }

        // In order to find a path where the bot will be at least once in building range of every entities,
        // we first find a list of points where all entities are in range of at least 1 of those points.
        // Then we link all the points to find the shortest path as the crow flies (which is the travelling
        // salesman problem).
        // And finally we use a normal pathfinder to get the actual path between each points.

        // Find the list of points
        std::vector<std::tuple<MapPosition, std::vector<SEntity_>, double>> waypoints;
        
        waypoints.push_back({starting_position, {}, 0});
        if (m_map_data.PathfinderCollides(starting_position, MapData::BUILDING)) {
            waypoints.push_back({
                m_map_data.FindNonCollidingPosition(starting_position, MapData::BUILDING),
                {}, 0
            });
        }

        while (!queue.empty()) {
            auto &t = queue.top();
            if (t->assigned) {
                queue.pop();
                continue;
            }

            if (g_config.debug_path) {
                m_instance.Request("DrawCircle", {
                    {"position", t->GetPosition()},
                    {"radius", 0.5},
                    {"color", {0, 255, 0}},
                    {"filled", false}
                });
            }

            MapPosition pos;
            bool found = false;
            IterateFromClosestPointCircle(
                center,
                true,
                t->GetPosition(),
                t->to_build ? reach_distance : mine_distance,
                [this, &pos, &found](const MapPosition &pos_) -> bool {
                    if (!m_map_data.PathfinderCollides(pos_, MapData::BUILDING)) {
                        found = true;
                        pos = pos_;
                        return true;
                    }
                    return false;
                }, []{ throw RuntimeErrorF("Could not find a valid position."); }
            );

            if (!found) throw "TODO";

            waypoints.push_back({pos, {}, INFINITY});
            auto &vec = std::get<std::vector<SEntity_>>(waypoints.back());
            auto &max_distance = std::get<double>(waypoints.back());

            for (auto &t_ : entities) {
                if (t_->assigned) continue;

                double d = MapPosition::SqDistance(pos, t_->GetPosition());
                if (d > (t_->to_build ? sq_reach_distance : sq_mine_distance)) continue;
                
                t_->assigned = true;
                if (max_distance > d) {
                    max_distance = t_->to_build ? reach_distance : mine_distance - std::sqrt(d);
                }
                vec.push_back(t_);
            }

            queue.pop();
        }

        // TSP
        TSP<std::tuple<MapPosition, std::vector<SEntity_>, double>>
            (waypoints, [](const std::tuple<MapPosition, std::vector<SEntity_>, double> &e) -> const MapPosition& {
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
                MapData::BUILDING,
                std::get<MapPosition>(first),
                std::get<MapPosition>(second),
                std::get<double>(second)
            );
            if (!path) throw "TODO";
            
            if (g_config.debug_path) {
                m_instance.Request("DrawCircleBulk", {
                    { "positions", path.value() },
                    { "radius", 0.2 },
                    { "color", { 255, 0, 0 } },
                    { "filled", true }
                });
                
                // m_instance.Request("DrawCircle", {
                //     { "position", std::get<MapPosition>(second) },
                //     { "radius", reach_distance },
                //     { "color", { 255, 0, 255 } },
                //     { "filled", false }
                // });
                
                m_instance.Request("DrawCircle", {
                    { "position", std::get<MapPosition>(second) },
                    { "radius", 0.2 },
                    { "color", { 255, 255, 0 } },
                    { "filled", true }
                });
            }

            paths.emplace_back(std::move(path.value()));
        }

        m_map_data.SetPlayerPosition(paths.back().back(), MapData::BUILDING);

        task.QueueSubTask([
            this, waypoints = std::move(waypoints),
            paths = std::move(paths)
        ](FactorioInstance &instance) {
            for (int i = 0; i < paths.size(); i++) {
                auto walk_futur = instance.Request("WalkUntil", paths[i]);
                auto &waypoint = waypoints[i + 1];
                for (const auto &entity : std::get<std::vector<SEntity_>>(waypoint)) {
                    if (entity->to_build) {
                        const auto &e = entity->entity;
                        Build(e);
                        
                        if (!e.GetRecipe().empty()) {
                            SetEntityProperty(e, "recipe", e.GetRecipe());
                        }
                        if (!e.GetInputPriority().empty()) {
                            SetEntityProperty(e, "splitter_input_priority", e.GetInputPriority());
                        }
                        if (!e.GetOutputPriority().empty()) {
                            SetEntityProperty(e, "splitter_output_priority", e.GetOutputPriority());
                        }
                    } else {
                        Mine(entity->name, entity->pos);
                    }
                }
                instance.Request("WaitAllEntityRequests").wait();
                instance.Request("WalkFinishAndStop").wait();
                walk_futur.wait();
            }

            m_map_data.ValidateCheckpoint();
        });
    }

    void Bot::MineEntities(Task &, const std::vector<Entity> &entities) {
        const double mine_distance = g_prototypes.Get("character", "character")["reach_resource_distance"];

        std::vector<std::tuple<MapPosition, double>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.GetPosition(), mine_distance);
        }

        // auto paths = FindMultiPath(m_map_data, {0, 0}, points);

        // task.QueueSubTask([paths](FactorioInstance &instance) {
        //     for (auto &path : paths) {
        //         instance.RequestNoRes("Walk", std::get<Path>(path)).wait();
        //         instance.RequestNoRes("Mine", std::get<MapPosition>(path)).wait();
        //     }
        // });
    }

    void Bot::BuildBurnerCity(int iron, int copper, int coal, int stone) {
        auto &task = QueueTask();

        BuildBlueprint(task, Blueprint::Load("test/onpatch.txt"), {0, 0}, Direction::NORTH, false);
        if (true) return;

        std::map<std::string, Patch> patchs;
        std::map<std::string, double> min_distances{
            { "iron-ore", INFINITY },
            { "copper-ore", INFINITY },
            { "coal", INFINITY },
            { "stone", INFINITY }
        };
        std::map<std::string, int> amounts{
            { "iron-ore", iron },
            { "copper-ore", copper },
            { "coal", coal },
            { "stone", stone }
        };

        m_map_data.ForPatchs([&patchs, &min_distances](const Patch &patch) {
            double d = MapPosition::SqDistance(MapPosition(0, 0), patch.GetBoundingBox().Center());
            if (d < min_distances[patch.GetName()]) {
                min_distances[patch.GetName()] = d;
                patchs[patch.GetName()] = patch;
            }
        });

        MapPosition center;
        for (const auto &[name, patch] : patchs) {
            center += patch.GetBoundingBox().Center();
        }
        center /= 4;

        for (const auto &[name, patch] : patchs) {
            auto blueprints = patch.GetBurnerCityBP(center, 300, amounts[name], m_instance);

            for (const auto &bp : blueprints) {
                BuildBlueprint(task, bp);
            }
        }
    }
}