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
            std::scoped_lock lock(m_instructionMutex);
            m_instructionCond.notify_all();
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
        ClearInstructions();
        m_exit = true;
        m_instance.Stop();
    }

    size_t Bot::InstructionCount() {
        std::scoped_lock lock(m_instructionMutex);
        size_t sum = 0;
        for (auto &task : m_tasks) {
            sum += task->InstructionCount();
        }
        return sum;
    }

    Instruction *Bot::GetInstruction() {
        std::unique_lock lock(m_instructionMutex);
        while (true) {
            if (!m_tasks.empty()) {
                auto instruction = m_tasks.front()->GetInstruction();
                if (instruction) return instruction;
            }
    
            m_instructionCond.wait(lock);
        }
    }

    void Bot::PopInstruction() {
        std::scoped_lock lock(m_instructionMutex);
        if (m_tasks.empty()) return;
        m_tasks.front()->PopInstruction();
    }

    void Bot::ClearInstructions() {
        std::scoped_lock lock(m_instructionMutex);
        m_tasks.clear();
    }

    void Bot::PopTask() {
        std::scoped_lock lock(m_instructionMutex);
        m_tasks.pop_front();
    }

    bool Bot::Join() {
        if (m_loopThread.joinable()) m_loopThread.join();
        return m_instance.Join();
    }

    bool Bot::Running() {
        return m_instance.Running();
    }

    void Bot::Loop() {
        while(!m_exit) {
            Instruction *instruction = GetInstruction();
            if (instruction->GetType() == Instruction::TASK_END) {
                PopTask();
                continue;
            }

            instruction->Call(m_instance);
            PopInstruction();
        }
    }
}