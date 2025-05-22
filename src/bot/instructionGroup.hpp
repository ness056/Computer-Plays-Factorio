#pragma once

#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    class InstructionGroup {
    public:
        void ComputePath(FactorioInstance&, MapPosition start);

        // Must be used kinda like make_unique or make_shared
        template<typename T, typename ...Types, std::enable_if_t<std::is_base_of_v<Instruction, T>, bool> = true>
        bool QueueInstruction(Types &&...args) {
            if (!Running()) return false;

            {
                std::unique_lock lock(m_instructionsMutex);
                m_instructions.push_back(std::make_unique<T>(args...));
                // m_instructions.back()->Precompute(m_instance);
            }
            m_instructionsCond.notify_all();

            return true;
        }
        
        MapPosition position;

    private:
        std::deque<std::unique_ptr<Instruction>> m_instructions;>
        Path m_path;
        bool m_pathRequested = false;
    }
}