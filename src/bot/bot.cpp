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
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            std::shared_ptr<Instruction> instruction;
            {
                std::unique_lock lock(m_loopMutex);
                if (m_tasks.empty()) continue;
                instruction = m_tasks.front()->PopInstruction();
            }

            if (instruction == nullptr) continue;

            m_currentWaiter = instruction->Send(m_instance);
            if (m_currentWaiter == nullptr) {
                g_error << "TODO line: " << __LINE__ << std::endl;
                exit(1);
            }
            m_currentWaiter->Wait();
        }
    }
}