#include "bot.hpp"

namespace ComputerPlaysFactorio {

    void Bot::Test() {
        MapPosition pos(0, 0);
        m_tasks.push_back(std::make_unique<BuildBurnerCity>(&m_instance, pos));
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
        if (m_instructionWaiter) m_instructionWaiter->ForceUnlock();
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

            {
                std::scoped_lock lock(m_loopMutex);

                if (m_tasks.empty()) continue;
                auto instruction = m_tasks.front()->GetInstruction();
                if (!instruction) continue;

                m_instructionWaiter = instruction->Call(m_instance);
                if (m_instructionWaiter == nullptr) {
                    g_error << "TODO line: " << __LINE__ << std::endl;
                    exit(1);
                }
            }
            if (!m_exit) m_instructionWaiter->Wait();
        }
    }
}