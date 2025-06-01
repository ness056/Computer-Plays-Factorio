#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class Task {
    public:
        Instruction *GetInstruction();
        void PopInstruction();
        size_t InstructionCount() const;

        const std::deque<Instruction> &GetAllInstruction() const {
            return m_instructions;
        }

        inline size_t InstructionCount() const {
            m_instructions.size();
        }

    protected:
        Task(FactorioInstance *instance, EventManager *eventManager) {
            assert(eventManager && "nullptr event manager");
            assert(instance && "nullptr instance");
            m_eventManager = eventManager;
            m_instance = instance;
        }
        
        void QueueInstruction(const Instruction::Handler&);

        void QueueMineEntities(MapPosition &playerPos,
            const std::vector<Entity> &entities);

        FactorioInstance *m_instance;
        EventManager *m_eventManager;

        std::deque<Instruction> m_instructions;
        std::mutex m_mutex;
    };

    class BuildBurnerCity : public Task {
    public:
        BuildBurnerCity(FactorioInstance *instance, EventManager *eventManager)
            : Task(instance, eventManager) {

            std::vector<Entity> entities;
            Waiter w;
            w.Lock();
            m_instance->SendRequestDataRes<EntitySearchFilters, std::vector<Entity>>(
                "FindEntitiesFiltered",
                EntitySearchFilters{.name = {"big-rock"}},
                [&entities, &w](const Response<std::vector<Entity>> &res) {
                    entities = res.data;
                    w.Unlock();
                }
            );
            QueueMineEntities(entities);
        }
    };
}