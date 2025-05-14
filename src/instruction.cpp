#include "instruction.hpp"

namespace ComputerPlaysFactorio {

    bool Instruction::Wait() {
        std::unique_lock lock(m_mutex);
        m_cond.wait(lock, [this] { return !m_waiting; });
        return !m_canceled;
    }

    bool Instruction::Lock() {
        if (m_waiting || m_canceled) return false;
        std::unique_lock lock(m_mutex);
        m_waiting = true;
        return true;
    }

    void Instruction::Notify(bool cancel) {
        {
            std::unique_lock lock(m_mutex);
            m_canceled = cancel;
            m_waiting = false;
        }
        m_cond.notify_all();
    }
}