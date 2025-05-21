#pragma once

#include <deque>

#include "../factorioAPI/factorioAPI.hpp"
#include "../factorioAPI/instruction.hpp"

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
        std::mutex m_loopMutex;
        std::condition_variable m_loopCond;
        bool m_exit;

        bool QueueInstruction(const Instruction&);

        std::deque<std::unique_ptr<Instruction>> m_instructions;

        FactorioInstance m_instance;
    };
}