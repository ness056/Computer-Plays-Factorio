#include "bot.hpp"

namespace ComputerPlaysFactorio {

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
        m_exit = true;
        m_instance.Stop();
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
                std::unique_lock lock(m_loopMutex);
                m_loopCond.wait(lock, [this] { return !m_instructions.empty() || m_exit; });
            }

            if (m_exit) break;

            std::shared_ptr<Waiter> waiter;
            {
                std::unique_lock lock(m_loopMutex);
                waiter = m_instructions.front()->Send(m_instance);
                if (waiter == nullptr) {
                    g_error << "TODO line: " << __LINE__ << std::endl;
                    exit(1);
                }
                m_instructions.pop_front();
            }
            waiter->Wait();
        }
    }

    bool Bot::QueueInstruction(const Instruction &instruction) {
        if (!Running()) return false;

        {
            std::unique_lock lock(m_loopMutex);
            m_instructions.push_back(std::make_unique<Instruction>(instruction));
            m_instructions.back()->Precompute();
        }
        m_loopCond.notify_all();

        return true;
    }
}