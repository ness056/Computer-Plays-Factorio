#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class Subtask {
    public:
        // Must be used kinda like make_unique or make_shared
        template<typename T, typename ...Types, std::enable_if_t<std::is_base_of_v<Instruction, T>, bool> = true>
        inline void QueueInstruction(Types &&...args) {
            m_instructions.push_back(std::make_shared<T>(args...));
        }

        std::shared_ptr<Instruction> PopInstruction();
        inline size_t InstructionCount() {
            return m_instructions.size();
        }

    private:
        std::deque<std::shared_ptr<Instruction>> m_instructions;
    };

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
            QueueMineEntities(playerPos, {});
        }
    };
}