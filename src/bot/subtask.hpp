#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class InstructionGroup {
    public:
        
    };

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
}