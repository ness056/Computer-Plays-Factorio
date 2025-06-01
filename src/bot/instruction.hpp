#pragma once

#include "event.hpp"
#include "../factorioAPI/factorioAPI.hpp"

namespace ComputerPlaysFactorio {

    class Instruction {
    public:
        using Handler = std::function<SharedWaiter(FactorioInstance&)>;

        enum Type {
            NORMAL,
            TASK_END    // Marks the end of a task.
        };

        Instruction(const Handler &handler, Type type = NORMAL) :
            m_handler(handler), m_type(type) {}

        inline SharedWaiter Call(FactorioInstance &instance) const {
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
}