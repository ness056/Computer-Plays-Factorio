#pragma once

#include <deque>
#include <type_traits>

#include "task.hpp"

/**
 *      Definitions:
 * 
 * - Instructions are the lowest level of abstraction for the bot to control the player.
 *   Instructions are generated can be generated either by tasks or by the event manager.
 *   The execution of an instruction will never be interrupted (See interruption definition).
 * 
 * - Tasks are the highest level of abstraction to control the bot and are directly
 *   created by the user.
 *   The execution of a task may be interrupted (See interruption definition).
 *   When a task is interrupted or if one instruction fails, the task may need to
 *   regenerate its instruction queue. Tasks should be independent from one another,
 *   meaning if one task fails and regenerate its queue, other tasks should not be
 *   affected.
 *   For instance, one task could be: Build a burner city with 15 iron miners, 6 coppers,
 *   4 stones and 12 coals.
 * 
 * - The bot may interrupt the current task in order to execute another more important
 *   task. When the interruption is done, the bot will resume the execution of paused
 *   task and notify it that it had been interrupted. Interruptions are raised
 *   by the event manager.
 *   For instance an interruption will be raised if a biter attack is triggered
 *   or if power blacks out.
 */

namespace ComputerPlaysFactorio {

    class Bot {
    public:
        void Test();
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
        
        void ClearQueue();

        std::thread m_loopThread;
        std::mutex m_loopMutex;
        std::condition_variable m_loopCond;
        bool m_exit;

        EventManager m_eventManager;

        std::deque<std::unique_ptr<Task>> m_tasks;

        FactorioInstance m_instance;
    };
}