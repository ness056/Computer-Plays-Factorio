#pragma once

#include <deque>

#include "../factorioAPI/factorioAPI.hpp"

namespace ComputerPlaysFactorio {

    class Bot {
    public:
        Bot();

        bool Start(std::string *message = nullptr);
        void Stop();
        bool Join(int ms = -1);
        bool Running();

        inline const FactorioInstance &GetFactorioInstance() {
            return m_instance;
        }

    private:
        void Loop();

        std::thread m_loopThread;
        std::mutex m_mutex;
        std::condition_variable m_cond;
        bool m_exit;

        std::deque<std::function<>>

        FactorioInstance m_instance;
    };
}