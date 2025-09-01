#pragma once

#include <map>

#include "../utils/luaUtils.hpp"

namespace ComputerPlaysFactorio {

    class EventManager : public LuaClass<"LuaEventManager"> {

    public:
        enum BotEvent {
            NONE_EVENT,

            END_EVENT
        };

        template<typename T>
        void InvokeEvent(lua_State* L, BotEvent eventName, const T &data);

    private:
        static int On(lua_State*);

        std::map<BotEvent, LuaFunction<void()>> m_functions;

        LUA_CFUNCTIONS(
            std::make_tuple("On", &EventManager::On)
        );
    };

    template<typename T>
    void EventManager::InvokeEvent(lua_State* L, BotEvent eventName, const T &data) {
        if (m_functions.contains(eventName)) {
            m_functions[eventName].Call();
        }
    }
}