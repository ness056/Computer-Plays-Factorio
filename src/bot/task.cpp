#include "task.hpp"

namespace ComputerPlaysFactorio {

    Instruction *Task::GetInstruction() {
        std::unique_lock lock(m_mutex);
        if (m_instructions.empty()) return nullptr;
        return &m_instructions.front();
    }

    void Task::PopInstruction() {
        std::unique_lock lock(m_mutex);
        if (m_instructions.empty()) return;
        m_instructions.pop_front();
    }

    size_t Task::InstructionCount() const {
        std::unique_lock lock(m_mutex);
        m_instructions.size();
    }

    void Task::QueueInstruction(const Instruction::Handler &handler) {
        std::unique_lock lock(m_mutex);
        m_instructions.emplace_back(handler);
        m_eventManager->NotifyNewInstruction();
    }
    
    void Task::QueueMineEntities(const std::vector<Entity> &entities) {
        std::vector<std::tuple<MapPosition, float>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.position, 5);
        }

        PathfinderData pathfinderData = m_instance->GetPathFinderData();
        auto paths = FindStepPath(pathfinderData, {0, 0}, points);

        QueueInstruction([this, paths](FactorioInstance &instance) {
            SharedWaiter waiter;

            for (auto &path : paths) {
                auto &group = subtask.QueueGroup();
                group.SetPath(std::get<Path>(path));
            }

            return waiter;
        });
    }
}