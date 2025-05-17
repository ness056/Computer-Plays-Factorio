#include "playerController.hpp"

namespace ComputerPlaysFactorio {

    PlayerController::PlayerController() : 
        m_gInstance(FactorioInstance::GRAPHICAL),
        m_hInstance(FactorioInstance::HEADLESS) {
        
        m_gInstance.connect(&m_gInstance, &FactorioInstance::Terminated, [this] {
            Stop();
        });
        
        m_gInstance.connect(&m_gInstance, &FactorioInstance::Closed, [this] {
            ClearQueue();
        });
    }

    bool PlayerController::Start(uint32_t seed, std::string *message) {
        if (Running()) return false;

        if (m_gInstance.Start(seed, message) && m_hInstance.Start(seed, message)) {
            m_exit = false;
            m_loopThread = std::thread(&PlayerController::Loop, this);
            m_loopThread.detach();
            return true;
        } else {
            Stop();
            return false;
        }
    }

    void PlayerController::ClearQueue() {
        std::unique_lock lock(m_mutex);

        if (m_currentInstruction) m_currentInstruction->Cancel();
        for (auto &i : m_instructions) {
            i->Cancel();
        }
        m_instructions.clear();
    }

    void PlayerController::Stop() {
        m_exit = true;
        m_cond.notify_all();
        m_gInstance.Stop();
        m_hInstance.Stop();
        ClearQueue();
    }

    bool PlayerController::Join(int ms) {
        if (m_loopThread.joinable()) m_loopThread.join();
        return m_gInstance.Join(ms) && m_hInstance.Join(ms);
    }

    bool PlayerController::Running() {
        return m_gInstance.Running() && m_hInstance.Running();
    }

    void PlayerController::Loop() {
        while (!m_exit) {
            {
                std::unique_lock lock(m_mutex);
                m_cond.wait(lock, [this] { return !m_instructions.empty() || m_exit; });
            }

            if (m_exit) break;

            {
                std::unique_lock lock(m_mutex);
                m_currentInstruction = std::move(m_instructions.front());
                m_instructions.pop_front();
            }

            m_currentInstruction->Send(m_gInstance);
            g_info << m_currentInstruction->Wait() << " Walk success" << std::endl;
        }
    }

    bool PlayerController::QueueWalk(MapPosition goal, std::function<void(const ResponseDataless&)> callback) {
        if (!Running()) return false;

        {
            std::unique_lock lock(m_mutex);
            m_instructions.push_back(std::make_unique<InstructionWalk>(m_gInstance, m_playerPosition, goal, callback));
            m_playerPosition = goal;
        }
        m_cond.notify_all();

        return true;
    }
}