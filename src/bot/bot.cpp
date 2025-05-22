#include "bot.hpp"

namespace ComputerPlaysFactorio {

    void Bot::Test() {
        MapPosition pos(0, 0);
        QueueInstruction<Walk>(pos, pos = MapPosition(10, 10));
        QueueInstruction<Walk>(pos, pos = MapPosition(0, 0));
        QueueInstruction<Walk>(pos, pos = MapPosition(10, 0));
        QueueInstruction<Walk>(pos, pos = MapPosition(0, 10));
        QueueInstruction<Walk>(pos, pos = MapPosition(5, 9));
    }

    Bot::Bot() : m_instance(FactorioInstance::GRAPHICAL) {
        m_instance.connect(&m_instance, &FactorioInstance::Terminated, [this] {
            Stop();
        });
    }
   
    bool Bot::Start(std::string *message) {
        if (Running()) return false;
        if (!m_instance.Start(123, message)) return false;

        m_exit = false;
        m_loopThread = std::thread(&Bot::Loop, this);
        m_loopThread.detach();
        return true;
    }
   
    void Bot::Stop() {
        ClearQueue();
        m_exit = true;
        m_instance.Stop();
        m_instructionsCond.notify_all();
    }

    void Bot::ClearQueue() {
        std::unique_lock lock(m_instructionsMutex);
        for (auto &i : m_instructions) {
            i->Cancel();
        }
        m_instructions.clear();
        if (m_currentWaiter) m_currentWaiter->Unlock(false);
    }

    bool Bot::Join(int ms) {
        if (m_loopThread.joinable()) m_loopThread.join();
        return m_instance.Join(ms);
    }

    bool Bot::Running() {
        return m_instance.Running();
    }

    void Bot::Loop() {
        while(!m_exit) {
            {
                std::unique_lock lock(m_instructionsMutex);
                m_instructionsCond.wait(lock, [this] { return !m_instructions.empty() || m_exit; });
            }

            if (m_exit) break;

            m_currentWaiter = m_instructions.front()->Send(m_instance);
            if (m_currentWaiter == nullptr) {
                g_error << "TODO line: " << __LINE__ << std::endl;
                exit(1);
            }

            {
                std::scoped_lock lock(m_instructionsMutex);
                m_instructions.pop_front();
            }

            m_currentWaiter->Wait();
        }
    }
}