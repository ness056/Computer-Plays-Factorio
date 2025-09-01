#pragma once

#include <deque>
#include <type_traits>

#include "task.hpp"
#include "mapData.hpp"
#include "event.hpp"
#include "../utils/logging.hpp"
#include "../utils/luaUtils.hpp"

/**
 *      Definitions:
 * 
 * - Instructions are the lowest level of abstraction for the bot to control the player.
 *   Instructions can be generated either by tasks or by the event manager.
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

    class Bot : public LuaClass<"LuaBot"> {
    public:
        Bot();
        ~Bot();

        void Start();
        void Stop();
        bool Join();
        bool Running();

        void Exec(std::string script, bool ignoreReturns = true);
        void Exec(std::filesystem::path file, bool ignoreReturns = true);
        size_t InstructionCount();

        inline const FactorioInstance &GetFactorioInstance() const {
            return m_instance;
        }

    private:
        Instruction *GetInstruction(); // The calling function should lock m_instructionMutex
        void PopInstruction();
        void ClearInstructions();

        Task &QueueTask();
        void PopTask();

        void Loop();

        void QueueMineEntities(Task &task, const std::vector<Entity> &entities);

        static int QueueBuildBurnerCity(lua_State*);

        std::mutex m_instructionMutex;
        std::condition_variable m_instructionCond;
        std::thread m_loopThread;
        bool m_exit;

        lua_State *m_lua;
        EventManager m_eventManager;
        MapData m_mapData;

        std::deque<Task> m_tasks;

        FactorioInstance m_instance;

        LUA_CFUNCTIONS(
            std::make_tuple("QueueBuildBurnerCity", &Bot::QueueBuildBurnerCity)
        );
    };
}