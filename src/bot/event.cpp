#include "event.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    void EventManager::SetLuaState(lua_State *L) {
        static constexpr luaL_Reg eventLib[] = {
            { "On", EventManager::On },
            { nullptr, nullptr }
        };

        luaL_newlibtable(L, eventLib);
        lua_pushlightuserdata(L, this);
        luaL_setfuncs(L, eventLib, 1);
        lua_setglobal(L, "event");

        lua_newtable(L);
        m_handlerTableIdx = luaL_ref(L, LUA_REGISTRYINDEX);

        m_lua = L;
    }

    int EventManager::On(lua_State *L) {
        auto event = (EventManager*)lua_touserdata(L, lua_upvalueindex(1));

        auto top = lua_gettop(L);
        if (top < 2) {
            luaL_error(L, "EventManager::On requires 2 arguments but got {}", top);
        } else if (lua_isnumber(L, -2)) {
            luaL_error(L, "EventManager::On first argument should be a number but got {}", luaL_typename(L, -2));
        } else if (lua_isfunction(L, -1)) {
            luaL_error(L, "EventManager::On first argument should be a function but got {}", luaL_typename(L, -1));
        }

        auto name = (BotEvent)lua_tonumber(L, -2);
        if (name < EventManager::NONE_EVENT || name >= EventManager::END_EVENT) {
            luaL_error(L, "Event with name {} does not exist", name);
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, event->m_handlerTableIdx);
        lua_pushvalue(L, -2);
        lua_rawseti(L, -2, name);

        lua_pop(L, 3);
        return 0;
    }
}