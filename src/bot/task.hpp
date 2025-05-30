#pragma once

#include "subtask.hpp"

namespace ComputerPlaysFactorio {

    class Task {
    public:
        std::shared_ptr<Instruction> PopInstruction();
        size_t InstructionCount();

    protected:
        Task(FactorioInstance *computingInstance) {
            assert(computingInstance && "nullptr instance");
            m_instance = computingInstance;
        }

        std::shared_ptr<Instruction> PopInstructionNoLock();

        Subtask &QueueSubtask();

        using SubtaskCallback = std::function<void()>;
        void QueueMineEntities(MapPosition &playerPos,
            const std::vector<Entity> &entities, SubtaskCallback = nullptr);

        FactorioInstance *m_instance;

        std::deque<Subtask> m_subtasks;
        std::mutex m_mutex;
    };

    class BuildBurnerCity : public Task {
    public:
        BuildBurnerCity(FactorioInstance *computingInstance, MapPosition &playerPos) : Task(computingInstance) {
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
            QueueMineEntities(playerPos, entities);
        }
    };
}