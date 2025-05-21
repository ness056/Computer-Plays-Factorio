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
            
        }
    }
}