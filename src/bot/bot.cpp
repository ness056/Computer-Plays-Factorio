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

    Bot::Bot() : m_instance(FactorioInstance::GRAPHICAL), m_map_data(&m_instance) {
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
            m_map_data.DrawPathfinderData(j["data"].get<MapData::Branch>());
        });

        m_instance.RegisterEvent("DrawPatchs", [this](const json&) {
            m_map_data.DrawPatchs();
        });
    }
   
    Result Bot::Start() {
        std::string message;
        if (Running()) return INSTANCE_ALREADY_RUNNING;

        auto result = m_instance.Start(3923243918);
        if (result != SUCCESS) return result;

        m_scheduler.Loop();

        return SUCCESS;
    }
   
    void Bot::Stop() {
        m_scheduler.Stop();
        m_instance.Stop();
    }

    void Bot::Join() {
        m_instance.Join();
    }

    bool Bot::Running() {
        return m_instance.Running();
    }

    std::future<std::pair<json, SEntity>> Bot::Build(const Entity &entity) {
        auto future = m_instance.Request("Build", json(entity));
        return std::async(std::launch::async, [this, entity, future = std::move(future)] mutable {
            future.wait();
            auto json = std::move(future.get());
            SEntity e;
            if (json["success"].get<bool>()) {
                e = m_map_data.AddEntity(entity, MapData::MAIN);
            }
            return std::make_pair(json, e);
        });
    }

    std::future<json> Bot::Mine(const std::string &name, const MapPosition &pos) {
        auto future = m_instance.Request("Mine", {{ "name", name }, { "position", pos }});
        return std::async(std::launch::async, [this, name, pos, future = std::move(future)] mutable {
            future.wait();
            auto json = std::move(future.get());
            if (json["success"].get<bool>()) {
                m_map_data.RemoveEntity(name, pos, MapData::MAIN);
            }
            return json;
        });
    }

    std::future<json> Bot::TakeAllItems(SEntity e, Inventory::Type inventory) {
        auto future = m_instance.Request("TakeAll", {
            { "entity", e->GetName() }, { "position", e->GetPosition() }, { "inventory", Inventory::TypeToString(inventory) }
        });
        return std::async(std::launch::deferred, [this, future = std::move(future)] mutable {
            future.wait();
            return future.get();
        });
    }

    void Bot::MakeEntitiesPath(const std::vector<SEntity> &entities,
        std::function<void(SEntity)> entity_callback,
        std::function<void(SEntity)> path_callback,
        std::function<void()> finally_callback
    ) {
        if (entities.empty()) return;
    
        MapPosition center(0, 0);
        for (const auto &e : entities) {
            center += e->GetPosition();
        }
        center = (center / (double)entities.size()).HalfRound();

        if (g_config.debug_path) {
            m_instance.Request("DrawCircle", {
                {"position", center},
                {"radius", 0.5},
                {"color", {0, 0, 255}},
                {"filled", true}
            });
        }

        const auto comp = [&center](const SEntity &lhs, const SEntity &rhs) {
            return MapPosition::SqDistance(center, lhs->GetPosition()) <
                   MapPosition::SqDistance(center, rhs->GetPosition());
        };

        std::priority_queue<SEntity, std::vector<SEntity>, decltype(comp)> queue(comp);
        std::unordered_set<SEntity> assigned_entities;

        auto starting_position = m_map_data.GetPlayerPosition(MapData::BUILDING).HalfRound();

        for (const auto &e : entities) {
            queue.push(e);
            if (path_callback) path_callback(e);
        }

        // In order to find a path where the bot will be at least once in building range of every entities,
        // we first find a list of points where all entities are in range of at least 1 of those points.
        // Then we link all the points to find the shortest path as the crow flies (which is the traveling
        // salesman problem).
        // And finally we use a normal pathfinder to get the actual path between each points.

        // Find the list of points
        std::vector<std::tuple<MapPosition, std::vector<SEntity>, double>> waypoints;
        
        waypoints.push_back({starting_position, {}, 0});
        if (m_map_data.PathfinderCollides(starting_position, MapData::BUILDING)) {
            waypoints.push_back({
                m_map_data.FindNonCollidingPosition(starting_position, MapData::BUILDING),
                {}, 0
            });
        }

        while (!queue.empty()) {
            auto &e = queue.top();
            if (assigned_entities.contains(e)) {
                queue.pop();
                continue;
            }

            if (g_config.debug_path) {
                m_instance.Request("DrawCircle", {
                    {"position", e->GetPosition()},
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
                e->GetPosition(),
                e->GetReach(),
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
            auto &vec = std::get<std::vector<SEntity>>(waypoints.back());
            auto &radius = std::get<double>(waypoints.back());

            for (auto &e_ : entities) {
                if (assigned_entities.contains(e_)) continue;

                auto range = e_->GetReach();
                double d = MapPosition::Distance(pos, e_->GetPosition());
                if (d > range) continue;
                assigned_entities.insert(e_);
                
                auto r = range - d;
                if (radius > r) {
                    radius = r;
                }
                vec.emplace_back(e_);
            }

            queue.pop();
        }

        // TSP
        TSP<std::tuple<MapPosition, std::vector<SEntity>, double>>
            (waypoints, [](const std::tuple<MapPosition, std::vector<SEntity>, double> &e) -> const MapPosition& {
                return std::get<MapPosition>(e);
            }, false
        );

        // Find actual paths
        std::vector<Path> paths;
        paths.reserve(waypoints.size());

        MapPosition prev = std::get<MapPosition>(waypoints[0]);
        for (int i = 1; i < waypoints.size(); i++) {
            const auto &waypoint = waypoints[i];

            auto path = FindPath(
                m_map_data,
                MapData::BUILDING,
                prev,
                std::get<MapPosition>(waypoint),
                std::get<double>(waypoint)
            );
            if (!path) {
                if (i != 1) throw "TODO";
                path = { std::get<MapPosition>(waypoint) };
            }

            prev = (*path)[path->size() - 1];
            
            if (g_config.debug_path) {
                m_instance.Request("DrawCircleBulk", {
                    { "positions", path.value() },
                    { "radius", 0.2 },
                    { "color", { 255, 0, 0 } },
                    { "filled", true }
                });
                
                // m_instance.Request("DrawCircle", {
                //     { "position", std::get<MapPosition>(waypoint) },
                //     { "radius", reach_distance },
                //     { "color", { 255, 0, 255 } },
                //     { "filled", false }
                // });
                
                m_instance.Request("DrawCircle", {
                    { "position", std::get<MapPosition>(waypoint) },
                    { "radius", 0.2 },
                    { "color", { 255, 255, 0 } },
                    { "filled", true }
                });
            }

            paths.emplace_back(std::move(path.value()));
        }

        m_map_data.SetPlayerPosition(paths.back().back(), MapData::BUILDING);

        for (int i = 0; i < paths.size(); i++) {
            auto walk_future = m_instance.Request("WalkUntil", paths[i]);
            auto &waypoint = waypoints[i + 1];
            for (const auto &entity : std::get<std::vector<SEntity>>(waypoint)) {
                entity_callback(entity);
            }
            m_instance.Request("WaitAllEntityRequests").wait();
            m_instance.Request("WalkFinishAndStop").wait();
            walk_future.wait();
        }

        if (finally_callback) finally_callback();
    }
    
    std::future<std::vector<SEntity>> Bot::BuildBlueprint(const Blueprint &blueprint,
        ActionMode mode, const MapPosition &offset, Direction direction, bool mirror
    ) {
        const MapPosition center = (blueprint.center.Rotate(direction) + offset).HalfRound();
        std::vector<SEntity> entities_to_build;
        std::vector<SEntity> entities_to_mine;

        entities_to_build.reserve(blueprint.entities.size());
        for (const auto &e : blueprint.entities) {
            auto copy = std::make_shared<Entity>(e);
            copy->SetDirection(direction + copy->GetDirection());
            copy->SetPosition(copy->GetPosition().Rotate(direction) + offset);
            if (mirror) copy->SetMirror(!copy->GetMirror());
            entities_to_build.push_back(copy);
        }

        if (mode == ActionMode::FORCE || mode == ActionMode::SUPER_FORCE) {
            std::unordered_set<SEntity> entity_set;
            for (const auto e : entities_to_build) {
                if (e->GetPosition() == MapPosition(69, -81)) {
                    DEBUG1
                }
                std::vector<SEntity> found;
                if (mode == ActionMode::FORCE) {
                    found = std::move(m_map_data.FindEntities(
                        e->GetBoundingBox(),
                        MapData::BUILDING,
                        [](SEntity e) { return e->GetType() == "tree" || e->GetType() == "simple-entity"; }
                    ));
                } else {
                    found = std::move(m_map_data.FindEntities(
                        e->GetBoundingBox(),
                        MapData::BUILDING,
                        [](SEntity e) { return e->GetType() != "resource"; }
                    ));
                }

                for (const auto &entity : found) {
                    if (entity_set.contains(entity)) continue;
                    entity_set.emplace(entity);
                    entities_to_mine.push_back(entity);
                }
            }
            
            MakeEntitiesPath(entities_to_mine,
                [this](SEntity e) {
                    Mine(e->GetName(), e->GetPosition());
                }, [this](SEntity e) {
                    m_map_data.RemoveEntity(e->GetName(), e->GetPosition(), MapData::BUILDING);
                }
            );
        }

        auto created_entities = std::make_shared<std::vector<std::future<std::pair<json, SEntity>>>>();
        auto promise = std::make_shared<std::promise<std::vector<SEntity>>>();
        auto future = promise->get_future();
        MakeEntitiesPath(entities_to_build,
            [this, created_entities](SEntity e) {
                created_entities->emplace_back(std::move(Build(*e)));
                
                if (!e->GetRecipe().empty()) {
                    SetEntityProperty(e, "recipe", e->GetRecipe());
                }
                if (!e->GetInputPriority().empty()) {
                    SetEntityProperty(e, "splitter_input_priority", e->GetInputPriority());
                }
                if (!e->GetOutputPriority().empty()) {
                    SetEntityProperty(e, "splitter_output_priority", e->GetOutputPriority());
                }
            }, [this](SEntity e) {
                m_map_data.AddEntity(*e, MapData::BUILDING);
            }, [promise, created_entities] {
                std::vector<SEntity> vec;
                for (auto &e : *created_entities) {
                    e.wait();
                    auto r = e.get();
                    if (r.second) vec.push_back(r.second);
                }
                promise->set_value(vec);
            }
        );

        return future;
    }

    std::future<void> Bot::Mine(const std::vector<SEntity> &entities) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        MakeEntitiesPath(entities, [this](SEntity e) {
            Mine(e->GetName(), e->GetPosition());
        }, [this](SEntity e) {
            m_map_data.RemoveEntity(e->GetName(), e->GetPosition(), MapData::BUILDING);
        });

        return future;
    }

    std::future<void> Bot::TakeAllItems(const std::vector<SEntity> &entities, Inventory::Type inventory) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        
        MakeEntitiesPath(entities, [this, inventory](SEntity e) {
            TakeAllItems(e, inventory);
        }, nullptr, [promise] {
            promise->set_value();
        });

        return future;
    }

    int Bot::BuildBurnerCityPlaceBurners(BurnerCity &burner_city, int n) {
        int i = 0;
        for (; i < n && !burner_city.blueprints.empty(); i++) {
            const auto &bp = burner_city.blueprints.front();
            auto future = BuildBlueprint(bp, ActionMode::FORCE);
            future.wait();
            auto entities = future.get();

            if (burner_city.patch->GetType() == Patch::IRON) {
                auto it1 = std::find_if(entities.begin(), entities.end(), [](const SEntity e) {
                    return e->GetName() == "burner-mining-drill";
                });
                if (it1 == entities.end()) throw RuntimeErrorF("No stone burner was found in the built entities.");
                
                auto it2 = std::find_if(entities.begin(), entities.end(), [](const SEntity e) {
                    return e->GetName() == "stone-furnace";
                });
                if (it2 == entities.end()) throw RuntimeErrorF("No stone furnace was found in the built entities.");
                
                burner_city.burners.emplace_back(*it1, *it2);
            }

            burner_city.blueprints.pop();
        }

        return i;
    }

    void Bot::BuildBurnerCity(int iron, int copper, int coal, int stone) {
        // Implements this decision tree: https://www.mermaidchart.com/d/940662c1-16b2-467c-b077-f65797d3d221

        m_scheduler.Enqueue([=] -> Task {
            constexpr double player_move_speed = 0.15;
            constexpr double rock_search_distance = 120;
            constexpr uint32_t max_crafting_idle_time = 10 * 60;

            std::map<Patch::Type, BurnerCity> burners{
                { Patch::IRON, { INFINITY, iron } },
                { Patch::COPPER, { INFINITY, copper } },
                { Patch::COAL, { INFINITY, coal } },
                { Patch::STONE, { INFINITY, stone } }
            };
            int burner_to_craft = iron + copper + coal + stone;
    
            m_map_data.ForPatchs([&burners](const SPatch &patch) {
                auto type = patch->GetType();
                if (type == Patch::OIL) return;

                double d = MapPosition::SqDistance(MapPosition(0, 0), patch->GetBoundingBox().Center());
                auto &b = burners[type];
                if (d < b.min_distance) {
                    b.min_distance = d;
                    b.patch = patch;
                }
            });
    
            MapPosition center;
            for (const auto &[type, b] : burners) {
                center += b.patch->GetBoundingBox().Center();
            }
            center /= 4;
            
            for (Patch::Type type = Patch::IRON; type <= Patch::STONE; type++) {
                auto &b = burners[type];
                auto bps = b.patch->GetBurnerCityBP(center, 300, b.amount);
                for (const auto bp : bps) {
                    b.blueprints.push(std::move(bp));
                }
            }

            MapPosition iron_center = burners[Patch::IRON].patch->GetBoundingBox().Center();

            while (std::find_if(burners.begin(), burners.end(), [&](const std::pair<Patch::Type, BurnerCity> &p) {
                return !p.second.blueprints.empty();
            }) != burners.end()) {

                m_map_data.UpdatePlayerMainInventory().wait();
                Inventory player_inventory = m_map_data.GetPlayerMainInventory();

                auto f_crafting_time_remaining = m_instance.Request("CraftingTimeRemaining");
                f_crafting_time_remaining.wait();
                auto crafting_time_remaining = f_crafting_time_remaining.get()["data"].get<uint32_t>();

                auto player_position = m_map_data.GetPlayerPosition(MapData::BUILDING);
                Area rock_search_area(player_position, rock_search_distance);
                
                uint32_t coal_amount = player_inventory.Count("coal");
                uint32_t stone_amount = player_inventory.Count("stone") + player_inventory.Count("stone-furnace") * 5;

                // Get the time remaining until there will be enough iron in the burners to craft more drills.

                auto &iron_burner_pairs = burners[Patch::IRON].burners;
                std::vector<std::future<void>> futures;
                futures.reserve(iron_burner_pairs.size() * 2);
                for (auto &pair : iron_burner_pairs) {
                    futures.push_back(pair.first->FetchProperties());
                    futures.push_back(pair.second->FetchProperties());
                }
                WaitAll(futures);

                uint32_t iron_amount = player_inventory.Count("iron-plate");

                for (auto &pair : iron_burner_pairs) {
                    auto &inventory = pair.second->GetInventories().at(Inventory::OUTPUT);
                    iron_amount += inventory.Count("iron-plate");
                }

                uint32_t iron_time_remaining = 0;
                const auto nb_burner = iron_burner_pairs.size();
                
                if (nb_burner > 0 && iron_amount < 9) {
                    auto missing_iron = 9 - iron_amount;
                    constexpr auto plate_craft_time = 192;
                    constexpr auto miner_craft_time = 240;

                    auto full_cycles = (int)std::floor(missing_iron / nb_burner);
                    if (full_cycles > 0) full_cycles--;
                    iron_time_remaining += miner_craft_time * full_cycles;

                    auto n = missing_iron % nb_burner;
                    if (n == 0) n = nb_burner;

                    std::sort(iron_burner_pairs.begin(), iron_burner_pairs.end(),
                        [](const std::pair<SEntity, SEntity> &lhs, const std::pair<SEntity, SEntity> &rhs) {
                            if (lhs.second->GetCraftingProgress() == 0 && rhs.second->GetCraftingProgress() == 0) {
                                return lhs.first->GetCraftingProgress() < rhs.first->GetCraftingProgress();
                            } else {
                                return lhs.second->GetCraftingProgress() < rhs.second->GetCraftingProgress();
                            }
                        }
                    );

                    auto &pair = iron_burner_pairs[iron_burner_pairs.size() - n];
                    iron_time_remaining += (int)(
                        pair.second->GetCraftingProgress() > 0 ?
                        plate_craft_time * (1 - pair.second->GetCraftingProgress()) :
                        miner_craft_time * (1 - pair.first->GetCraftingProgress()) + plate_craft_time
                    );
                }

                auto time_remaining = std::max(iron_time_remaining, crafting_time_remaining) + max_crafting_idle_time;

                auto coal_rocks = m_map_data.FindEntities(rock_search_area, MapData::BUILDING, [](SEntity e) {
                    return e->GetName() == "huge-rock";
                });
                if (!coal_rocks.empty()) {
                    auto coal_rock = *std::min_element(coal_rocks.begin(), coal_rocks.end(), 
                        [player_position, iron_center](const SEntity &lhs, const SEntity &rhs) {
                            return MapPosition::SqDistance(player_position, lhs->GetPosition()) +
                                   MapPosition::SqDistance(iron_center, lhs->GetPosition()) <
                                   MapPosition::SqDistance(player_position, rhs->GetPosition()) +
                                   MapPosition::SqDistance(iron_center, rhs->GetPosition());
                        }
                    );
    
                    auto coal_rock_d = MapPosition::SqDistance(player_position, coal_rock->GetPosition()) +
                        MapPosition::SqDistance(iron_center, coal_rock->GetPosition());
    
                    constexpr int coal_rock_mining_time = 6 * 60;
                    if (time_remaining > coal_rock_d * player_move_speed + coal_rock_mining_time ||
                        coal_amount < 24 || stone_amount < 19
                    ) {
                        auto e_patch = FindPath(m_map_data, MapData::BUILDING, player_position, coal_rock->GetPosition(), coal_rock->GetReach());
                        if (!e_patch) throw "TODO";
                        
                        auto walk_future = m_instance.Request("Walk", *e_patch);
                        Mine(coal_rock->GetName(), coal_rock->GetPosition()).wait();
                        walk_future.wait();

                        m_instance.Request("Craft", {
                            { "recipe", "stone-furnace" },
                            { "amount", 1000 },
                            { "force", true }
                        });

                        continue;
                    }
                }

                auto stone_rocks = m_map_data.FindEntities(rock_search_area, MapData::BUILDING, [](SEntity e) {
                    return e->GetName() == "big-rock" || e->GetName() == "big-sand-rock";
                });
                if (!stone_rocks.empty()) {
                    auto stone_rock = *std::min_element(stone_rocks.begin(), stone_rocks.end(), 
                        [player_position, iron_center](const SEntity &lhs, const SEntity &rhs) {
                            return MapPosition::SqDistance(player_position, lhs->GetPosition()) +
                                   MapPosition::SqDistance(iron_center, lhs->GetPosition()) <
                                   MapPosition::SqDistance(player_position, rhs->GetPosition()) +
                                   MapPosition::SqDistance(iron_center, rhs->GetPosition());
                        }
                    );
    
                    auto stone_rock_d = MapPosition::SqDistance(player_position, stone_rock->GetPosition()) +
                        MapPosition::SqDistance(iron_center, stone_rock->GetPosition());
    
                    constexpr int stone_rock_mining_time = 4 * 60;
                    if (time_remaining > stone_rock_d * player_move_speed + stone_rock_mining_time ||
                        stone_amount < 19
                    ) {
                        auto e_patch = FindPath(m_map_data, MapData::BUILDING, player_position, stone_rock->GetPosition(), stone_rock->GetReach());
                        if (!e_patch) throw "TODO";
                        
                        auto walk_future = m_instance.Request("Walk", *e_patch);
                        Mine(stone_rock->GetName(), stone_rock->GetPosition()).wait();
                        walk_future.wait();

                        m_instance.Request("Craft", {
                            { "recipe", "stone-furnace" },
                            { "amount", 1000 },
                            { "force", true }
                        });

                        continue;
                    }
                }

                if (burner_to_craft > 0) {
                    std::vector<SEntity> iron_furnaces;
                    iron_furnaces.reserve(iron_burner_pairs.size());
                    for (auto &pair : iron_burner_pairs) {
                        iron_furnaces.emplace_back(pair.second);
                    }
    
                    TakeAllItems(iron_furnaces, Inventory::OUTPUT).wait();
                    
                    m_map_data.UpdatePlayerMainInventory().wait();
                    player_inventory = m_map_data.GetPlayerMainInventory();
    
                    int craft_amount = (int)std::floor(player_inventory.Count("iron-plate") / 9);
                    std::vector<std::future<json>> futures_;
                    futures_.reserve(craft_amount);

                    for (int i = 0; i < craft_amount; i++) {
                        futures_.emplace_back(m_instance.Request("Craft", {
                            { "recipe", "burner-mining-drill" },
                            { "amount", 1 }
                        }));
                    }
                    WaitAll(futures_);

                    burner_to_craft -= craft_amount;

                    if (craft_amount > 0 && iron_burner_pairs.size() < 6) {
                        m_instance.Request("WaitCraftingQueue").wait();
                    }
                }

                m_map_data.UpdatePlayerMainInventory().wait();
                player_inventory = m_map_data.GetPlayerMainInventory();
                int burner_amount = std::min(
                    player_inventory.Count("burner-mining-drill"),
                    player_inventory.Count("stone-furnace")
                );
                if (burner_amount == 0) continue;

                if (coal_rocks.empty() && stone_rocks.empty() && player_inventory.Count("stone-furnace") < 6) {
                    burner_amount -= BuildBurnerCityPlaceBurners(burners[Patch::STONE], burner_amount);
                    if (burner_amount == 0) continue;
                }

                if (coal_rocks.empty() && coal < 40) {
                    burner_amount -= BuildBurnerCityPlaceBurners(burners[Patch::COAL], burner_amount);
                    if (burner_amount == 0) continue;
                }

                if (!burners[Patch::IRON].blueprints.empty()) {
                    burner_amount -= BuildBurnerCityPlaceBurners(burners[Patch::IRON], burner_amount);
                    if (burner_amount == 0) continue;
                }

                if (!burners[Patch::COPPER].blueprints.empty()) {
                    burner_amount -= BuildBurnerCityPlaceBurners(burners[Patch::COPPER], burner_amount);
                    if (burner_amount == 0) continue;
                }

                if (!burners[Patch::COAL].blueprints.empty()) {
                    BuildBurnerCityPlaceBurners(burners[Patch::COAL], burner_amount);
                } else {
                    BuildBurnerCityPlaceBurners(burners[Patch::STONE], burner_amount);
                }
            }

            co_return;
        });
    }

    void Bot::PickUpCrashSiteItems() {
        m_scheduler.Enqueue([this] -> Task {
            auto entities = m_map_data.FindEntities(Area(-60, -20, -1, 20), MapData::MAIN, [](SEntity e) {
                return e->GetType() == "container" && e->GetName().starts_with("crash-site-");
            });

            TakeAllItems(entities, Inventory::MAIN);
            co_return;
        });
    }
}