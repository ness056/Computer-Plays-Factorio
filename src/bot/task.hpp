#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class Task {
    public:
        Instruction *GetInstruction();
        void PopInstruction();
        size_t InstructionCount();

    protected:
        Task(FactorioInstance *instance, EventManager *eventManager) {
            assert(eventManager && "nullptr event manager");
            assert(instance && "nullptr instance");
            m_eventManager = eventManager;
            m_instance = instance;
        }
        
        void QueueInstruction(const Instruction::Handler&);

        void QueueMineEntities(const std::vector<Entity> &entities);

        FactorioInstance *m_instance;
        EventManager *m_eventManager;

        std::deque<Instruction> m_instructions;
        std::mutex m_mutex;
    };

    class BuildBurnerCity : public Task {
    public:
        BuildBurnerCity(FactorioInstance *instance, EventManager *eventManager)
            : Task(instance, eventManager) {

            auto entitiesFuture = m_instance->Request<EntitySearchFilters, std::vector<Entity>>(
                "FindEntitiesFiltered", EntitySearchFilters{.name = {"big-rock"}});
            entitiesFuture.wait();
            auto entities = entitiesFuture.get();
            QueueMineEntities(entities.data);
        }
    };
}