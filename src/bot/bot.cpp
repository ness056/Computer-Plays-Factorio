#include "bot.hpp"

namespace ComputerPlaysFactorio {

    void Bot::Test() {
        MapPosition pos(0, 0);
        m_tasks.push_back(std::make_unique<BuildBurnerCity>(&m_instance, &m_eventManager));
    }

    Bot::Bot() : m_instance(FactorioInstance::GRAPHICAL) {
        m_instance.connect(&m_instance, &FactorioInstance::Terminated, [this] {
            Stop();
        });

        m_eventManager.connect(&m_eventManager, &EventManager::NewInstruction, [this] {

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
    }

    void Bot::ClearQueue() {
        std::unique_lock lock(m_loopMutex);
        m_tasks.clear();
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
                m_loopCond.wait(lock);
            }
            if (m_exit) break;

            Instruction *instruction;
            {
                std::scoped_lock lock(m_loopMutex);

                if (m_tasks.empty()) continue;
                instruction = m_tasks.front()->GetInstruction();
                if (!instruction) continue;
            }
            instruction->Call(m_instance);
        }
    }
}