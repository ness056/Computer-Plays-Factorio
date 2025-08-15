#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class Task {
    public:
        Instruction *GetInstruction();
        void PopInstruction();
        size_t InstructionCount();

    protected:
        Task(FactorioInstance *instance, EventManager *eventManager,
            MapData *mapData, std::function<void()> notifyNewInstruction) :
        m_instance(instance), m_eventManager(eventManager),
        m_mapData(mapData), m_notifyNewInstruction(m_notifyNewInstruction) {
            assert(instance && "nullptr instance");
        }
        
        void QueueInstruction(const Instruction::Handler&);

        void QueueMineEntities(const std::vector<Entity> &entities);

        FactorioInstance *m_instance;
        EventManager *m_eventManager;
        MapData *m_mapData;

        std::deque<Instruction> m_instructions;
        std::mutex m_mutex;
        std::function<void()> m_notifyNewInstruction;
    };

    class BuildBurnerCity : public Task {
    public:
        BuildBurnerCity(FactorioInstance *instance, EventManager *eventManager,
            MapData *mapData, std::function<void()> notifyNewInstruction)
            : Task(instance, eventManager, mapData, notifyNewInstruction) {

            auto entitiesFuture = instance->Request<EntitySearchFilters, std::vector<Entity>>(
                "FindEntitiesFiltered", EntitySearchFilters{.area = {{-100, -100}, {100, 100}}, .name = {"huge-rock"}});
            entitiesFuture.wait();
            auto entities = entitiesFuture.get();
            QueueMineEntities(entities.data);
        }
    };
}