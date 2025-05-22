#pragma once

#include <deque>
#include <type_traits>

#include "../factorioAPI/factorioAPI.hpp"
#include "instruction.hpp"

/**
 *      Definitions:
 * 
 * - Instructions are the lowest level of abstraction for the bot to control the player.
 *   The are more or less equivalent to the Factorio-TAS-Generator's tasks. If one
 *   instruction fails, the subtask will be need to be regenerated. (see subtask definition)
 * 
 * - Instruction groups are containers of instructions. They require a queue of
 *   instructions and a map position to which the player will move when the group is executed.
 *   Note that all the instructions need to be in range of the map position, otherwise the
 *   out of range instruction will fail. An instruction group execution will never be
 *   interrupted. (See interruption definition)
 * 
 * - Subtasks are containers of instruction groups. The execution of a subtask may be
 *   interrupted (See interruption definition). When a subtask is interrupted or if one
 *   instruction fails, the subtask may need to regenerate its instruction group queue.
 *   Subtasks should be independent from one another, meaning if one subtask fails and
 *   regenerate its queue, other subtasks should not be affected.
 * 
 * - Tasks are the highest level of abstraction to control the bot and are directly
 *   created by the user. Tasks should be seed independent, meaning no coordinates should
 *   be in a task's arguments.
 *   For instance, one task could be: Build a burner city with 15 iron miners, 6 coppers,
 *   4 stones and 12 coals.
 * 
 * - The bot may interrupt the current subtask in order to execute another more important
 *   subtask. When the interruption subtask is done, the bot will resume the execution of
 *   paused subtask and notify it that it has been interrupted. Interruptions are raised
 *   by events, which are sent by the lua mod. For instance when a biter attack is
 *   triggered or a power black out happens.
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
        bool m_exit;

        std::deque<std::unique_ptr<Instruction>> m_instructions;
        std::mutex m_instructionsMutex;
        std::condition_variable m_instructionsCond;
        std::shared_ptr<Waiter> m_currentWaiter;

        FactorioInstance m_instance;
    };
}