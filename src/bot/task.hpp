#pragma once

#include "../factorio-API/factorio-API.hpp"

namespace ComputerPlaysFactorio {

    class Instruction {
    public:
        using Handler = std::function<void(FactorioInstance&)>;

        enum Type {
            NORMAL,
            TASK_END    // Marks the end of a task.
        };

        Instruction(const Handler &handler, Type type = NORMAL) :
            m_handler(handler), m_type(type) {}

        inline void Call(FactorioInstance &instance) const {
            m_handler(instance);
        }

        inline Type GetType() { return m_type; }
        
        inline void SetDescription(const std::string &str) {
            m_description = str;
        }

        inline const std::string &GetDescription() const {
            return m_description;
        }

    private:
        Handler m_handler;
        const Type m_type;
        std::string m_description;
    };

    class Task {
    public:
        Task(std::condition_variable &instruction_cond) : m_instruction_cond(instruction_cond) {}

        void QueueInstruction(const Instruction::Handler&);
        Instruction *GetInstruction();
        void PopInstruction();
        size_t InstructionCount();

    private:
        std::deque<Instruction> m_instructions;
        std::mutex m_mutex;
        std::condition_variable &m_instruction_cond;
    };
}