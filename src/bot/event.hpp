#pragma once

#include <QObject>
#include <lua.hpp>
#include <map>

namespace ComputerPlaysFactorio {

    class EventManager : public QObject {
        Q_OBJECT

    public:
        enum BotEvent {
            NONE_EVENT,

            END_EVENT
        };

        void SetLuaState(lua_State *L);

        template<typename T>
        void InvokeEvent(BotEvent eventName, const T &data);

    private:
        static int On(lua_State*);

        lua_State *m_lua;
        int m_handlerTableIdx;
    };

    template<typename T>
    void EventManager::InvokeEvent(BotEvent eventName, const T &data) {
        
    }
}