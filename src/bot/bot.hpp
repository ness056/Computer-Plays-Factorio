#pragma once

#include <deque>
#include <type_traits>

#include "task.hpp"
#include "map-data.hpp"
#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    class Bot {
    public:
        Result Start();
        void Stop();
        void Join();
        bool Running();

    protected:
        Bot();

        size_t SubTaskCount();

        inline const FactorioInstance &GetFactorioInstance() const {
            return m_instance;
        }

        inline const MapData &GetMapData() const {
            return m_map_data;
        }

        // Instructions

        std::future<json> Build(const Entity&);
        std::future<json> Mine(const std::string&, const MapPosition&);
        template <class T>
        std::future<json> SetEntityProperty(const std::string &entity_name, const MapPosition &pos, const std::string &property, const T &value);
        template <class T>
        inline std::future<json> SetEntityProperty(const Entity &entity, const std::string &property, const T &value) {
            return SetEntityProperty(entity.GetName(), entity.GetPosition(), property, value);
        }

        // Subtasks

        void BuildBlueprint(Task &task, const Blueprint &bp, const MapPosition &offset = {0, 0}, Direction = Direction::NORTH, bool mirror = false);
        void MineEntities(Task &task, const std::vector<Entity> &entities);
        bool CraftItems(Task &task, const std::vector<std::tuple<std::string, int>> &items);
        bool CraftItemsUpTo(Task &task, const std::vector<std::tuple<std::string, int>> &items);

        // Tasks

        void BuildBurnerCity(int iron, int copper, int coal, int stone);

        virtual void OnReady() {}

    private:
        SubTask *GetSubTask(); // The calling function should lock m_sub_task_mutex
        void PopSubTask();
        void ClearSubTasks();

        Task &QueueTask();
        void PopTask();

        void Loop();

        std::mutex m_sub_task_mutex;
        std::condition_variable m_sub_task_cond;
        std::thread m_loop_thread;
        bool m_exit;

        MapData m_map_data;

        std::deque<Task> m_tasks;

        FactorioInstance m_instance;
    };
    
    template <class T>
    std::future<json> Bot::SetEntityProperty(const std::string &name, const MapPosition &pos, const std::string &property, const T &value) {
        auto promise = std::make_shared<std::promise<json>>();

        m_instance.Request("SetEntityProperty", {
            {"entity", name},
            {"position", pos},
            {"property", property},
            {"value", json(value)}
        }, [this, promise, name, pos, property, value](const json &j) {
            if (j["success"].get<bool>()) {
                m_map_data.UpdateEntity(name, pos, property, value, MapData::MAIN);
            }
            promise->set_value(j);
        });

        return promise->get_future();
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