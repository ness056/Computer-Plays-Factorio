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
    }
}