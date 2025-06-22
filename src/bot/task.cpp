#include "task.hpp"

namespace ComputerPlaysFactorio {

    Instruction *Task::GetInstruction() {
        std::scoped_lock lock(m_mutex);
        if (m_instructions.empty()) return nullptr;
        return &m_instructions.front();
    }

    void Task::PopInstruction() {
        std::scoped_lock lock(m_mutex);
        if (m_instructions.empty()) return;
        m_instructions.pop_front();
    }

    size_t Task::InstructionCount() {
        std::scoped_lock lock(m_mutex);
        return m_instructions.size();
    }

    void Task::QueueInstruction(const Instruction::Handler &handler) {
        std::scoped_lock lock(m_mutex);
        m_instructions.emplace_back(handler);
        m_eventManager->NotifyNewInstruction();
    }
    
    void Task::QueueMineEntities(const std::vector<Entity> &entities) {
        std::vector<std::tuple<MapPosition, float>> points;
        for (auto &entity : entities) {
            points.emplace_back(entity.position, 5);
        }

        PathfinderData pathfinderData = m_instance->GetPathfinderData();
        auto paths = FindMultiPath(pathfinderData, {0, 0}, points);

        QueueInstruction([paths](FactorioInstance &instance) {
            for (auto &path : paths) {
                instance.RequestNoRes("Walk", std::get<Path>(path)).wait();
            }
        });
    }
}