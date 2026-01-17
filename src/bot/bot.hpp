#pragma once

#include <deque>
#include <type_traits>

#include "scheduler.hpp"
#include "map-data.hpp"
#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    enum class ActionMode {
        NORMAL = 0,         // If the action is not possible, nothing happens
        WAIT = 1,           // Wait until the action is possible
        THROW = 2,          // If the action is not possible, throws an exception

        // Only relevent for building actions
        FORCE = 4,          // Same as in-game force blueprint placement
        SUPER_FORCE = 8,    // Same as in-game super force blueprint placement

        WAIT_FORCE = WAIT | FORCE,
        THROW_FORCE = THROW | FORCE,
        WAIT_SUPER_FORCE = WAIT | SUPER_FORCE,
        THROW_SUPER_FORCE = THROW | SUPER_FORCE
    };

    class Bot {
    public:
        Result Start();
        void Stop();
        void Join();
        bool Running();

    protected:
        Bot();

        inline const FactorioInstance &GetFactorioInstance() const {
            return m_instance;
        }

        inline const MapData &GetMapData() const {
            return m_map_data;
        }

        // Tasks

        void PickUpCrashSiteItems();
        void BuildBurnerCity(int iron, int copper, int coal, int stone);

        // Events

        virtual void OnReady() {}

    private:
        // Helper struct for BuildBurnerCity
        struct BurnerCity {
            double min_distance;
            int amount;
            SPatch patch;
            std::queue<Blueprint> blueprints;
            std::vector<std::pair<SEntity, SEntity>> burners;   // Only used for iron patches
        };

        // Helper function for BuildBurnerCity
        int BuildBurnerCityPlaceBurners(BurnerCity &burner_city, int n);

        // Makes the bot follow a path where it will be that least once in range of every entity.
        // The entity_callback is called for each entity once it is in range.
        // The finally callback is called after the entity_callback has been called for every entity.
        // The path_callback is called for each entity before the pathfinder. It should be used
        // if any entities are going to be placed/removed by the entity callback.
        void MakeEntitiesPath(const std::vector<SEntity> &entities,
            std::function<void(SEntity)> entity_callback,
            std::function<void(SEntity)> path_callback = nullptr,
            std::function<void()> finally_callback = nullptr
        );
        std::future<std::vector<SEntity>> BuildBlueprint(const Blueprint &bp, ActionMode,
            const MapPosition &offset = {0, 0}, Direction = Direction::NORTH, bool mirror = false);
        std::future<void> Mine(const std::vector<SEntity> &entities);
        std::future<void> TakeAllItems(const std::vector<SEntity> &entities, Inventory::Type);

        // Instructions

        std::future<std::pair<json, SEntity>> Build(const Entity&);
        std::future<json> Mine(const std::string&, const MapPosition&);
        template <class T>
        std::future<json> SetEntityProperty(const std::string &entity_name, const MapPosition &pos, const std::string &property, const T &value);
        template <class T>
        inline std::future<json> SetEntityProperty(SEntity entity, const std::string &property, const T &value) {
            return SetEntityProperty(entity->GetName(), entity->GetPosition(), property, value);
        }
        std::future<json> TakeAllItems(SEntity, Inventory::Type);

        MapData m_map_data;

        Scheduler m_scheduler;

        FactorioInstance m_instance;
    };
    
    template <class T>
    std::future<json> Bot::SetEntityProperty(const std::string &name, const MapPosition &pos, const std::string &property, const T &value) {
        auto future = m_instance.Request("SetEntityProperty", {
            {"entity", name},
            {"position", pos},
            {"property", property},
            {"value", json(value)}
        });
        return std::async(std::launch::async, [this, name, pos, property, value, future = std::move(future)] mutable {
            future.wait();
            auto json = std::move(future.get());
            if (json["success"].get<bool>()) {
                m_map_data.UpdateEntity(name, pos, property, value, MapData::MAIN);
            }
            return json;
        });
    }

    class BotFactory {
    public:
        using FactoryFunction = std::function<std::unique_ptr<Bot>()>;

        BotFactory() = delete;

        static bool Register(const std::string &name, const FactoryFunction&);

        static inline const std::map<std::string, FactoryFunction> &GetBots() {
            return s_bot_factories;
        }

    private:
        static inline std::map<std::string, FactoryFunction> s_bot_factories;
    };

    #define REGISTER_BOT(name, class_name) \
        private: \
        static inline bool __s_registered__ = ::ComputerPlaysFactorio::BotFactory::Register(name, \
            [] { return ::std::make_unique<class_name>(); });
}