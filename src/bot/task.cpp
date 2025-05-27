#include "task.hpp"

namespace ComputerPlaysFactorio {

    std::shared_ptr<Instruction> Subtask::PopInstruction() {
        if (m_instructions.empty()) return nullptr;
        auto instruction = m_instructions.front();
        m_instructions.pop_back();
        return instruction;
    }

    std::shared_ptr<Instruction> Task::PopInstruction() {
        std::unique_lock lock(m_mutex);
        return PopInstructionNoLock();
    }

    std::shared_ptr<Instruction> Task::PopInstructionNoLock() {
        if (m_subtasks.empty()) return nullptr;
        auto &subtask = m_subtasks.front();
        
        auto instruction = subtask.PopInstruction();
        if (instruction->Name() == "EndSubtask") {
            m_subtasks.pop_front();
            return PopInstructionNoLock();
        } else {
            return instruction;
        }
    }

    Subtask &Task::QueueSubtask() {
        std::unique_lock lock(m_mutex);
        m_subtasks.emplace_back();
        return m_subtasks.back();
    }

    size_t Task::InstructionCount() {
        std::unique_lock lock(m_mutex);
        size_t sum = 0;
        for (auto &subtask : m_subtasks) {
            sum += subtask.InstructionCount();
        }
        return sum;
    }
    
    void Task::QueueMineEntities(MapPosition &playerPos, const std::vector<Entity> &entities, SubtaskCallback) {
        auto &subtask = QueueSubtask();

        std::vector<std::tuple<MapPosition, float>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.position, 5);
        }

        PathfinderData pathfinderData = m_instance->GetPathFinderData();
        auto paths = FindStepPath(pathfinderData, playerPos, points);
        for (auto &path : paths) {
            subtask.QueueInstruction<Walk>(std::get<Path>(path));
        }
        playerPos = std::get<MapPosition>(*paths.crbegin());
    }
}