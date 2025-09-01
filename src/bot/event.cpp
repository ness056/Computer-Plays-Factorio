#include "event.hpp"

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    int EventManager::On(lua_State *L) {
        auto parameters = LuaGetParameters<EventManager*, int, LuaFunction<void()>>(L);
        auto &functions = std::get<0>(parameters)->m_functions;

        auto id = (BotEvent)std::get<1>(parameters);
        if (id < EventManager::NONE_EVENT || id >= EventManager::END_EVENT) {
            luaL_error(L, "Event with id %d does not exist", id);
        }

        if (functions.contains(id)) {
            functions.erase(id);
        }

        functions.emplace(id, std::get<2>(parameters));
        functions.at(id).Register();
        return 0;
    }
}