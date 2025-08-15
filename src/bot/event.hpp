#pragma once

#include <lua.hpp>
#include <map>

namespace ComputerPlaysFactorio {

    class EventManager {

    public:
        enum BotEvent {
            NONE_EVENT,

            END_EVENT
        };

        void Init(lua_State *L);

        template<typename T>
        void InvokeEvent(lua_State* L, BotEvent eventName, const T &data);

    private:
        static int On(lua_State*);

        int m_handlerTableIdx;
    };

    template<typename T>
    void EventManager::InvokeEvent(lua_State* L, BotEvent eventName, const T &data) {
        
    }
}