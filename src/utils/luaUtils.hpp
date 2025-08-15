#pragma once

#include <lua.hpp>
#include <new>
#include <functional>
#include <tuple>

#include "../../external/includeReflectcpp.hpp"
#include "logging.hpp"

#define LUA_CFUNCTIONS \
private: \
    template<class T> \
    friend void ::ComputerPlaysFactorio::LuaSetmetatable(lua_State *L, int idx); \
    constexpr static auto __s_luaCFunctions__ = std::make_tuple

namespace ComputerPlaysFactorio {

    template<class T, class = int>
    struct HasLuaCFunctions : std::false_type {};

    template<class T>
    struct HasLuaCFunctions<T, decltype(T::__s_luaCFunctions__, 0)> : std::true_type {};

    // Throws an error if the type of the value at idx is not equal to every t
    inline void CheckLuaType(lua_State *L, int idx, int t) {
        int topType = lua_type(L, idx);
        if (topType != t) {
            throw RuntimeErrorFormat("Tried to convert Lua value to {} but top is {}", lua_typename(L, t), luaL_typename(L, idx));
        }
    }

    template<class T>
    constexpr int TemplateToLuaType() {
        if constexpr (std::is_same_v<T, bool>) {
            return LUA_TBOOLEAN;
        } else if constexpr (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>) {
            return LUA_TNUMBER;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return LUA_TSTRING;
        } else if constexpr (is_vector<T>::value || is_string_key_map<T>::value) {
            return LUA_TTABLE;
        }
    }

    // Equivalent to lua_pushsometype with templates
    inline void LuaPushValue(lua_State *L, const bool &v);
    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
    inline void LuaPushValue(lua_State *L, const T &v);
    inline void LuaPushValue(lua_State *L, const std::string &v);
    template<class T>
    void LuaPushValue(lua_State *L, const std::vector<T> &v);
    template<class T>
    void LuaPushValue(lua_State *L, const std::map<std::string, T> &v);
    // Pushes any class onto the stack as a table
    template<class T>
    void LuaPushValue(lua_State *L, const T &v);

    // Equivalent to lua_tosometype with templates
    inline bool LuaGetValue(lua_State *L, int idx);
    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
    inline T LuaGetValue(lua_State *L, int idx);
    template<class T> requires (std::is_same_v<T, std::string>)
    inline T LuaGetValue(lua_State *L, int idx);
    template<class T> requires (is_vector<T>::value)
    T LuaGetValue(lua_State *L, int idx);
    template<class T> requires (is_string_key_map<T>::value)
    T LuaGetValue(lua_State *L, int idx);
    template<class T>
    T LuaGetValue(lua_State *L, int idx);

    template<class T>
    void LuaSetmetatable(lua_State *L, int idx);

    template<class T>
    void LuaPushUserdata(lua_State *L);
    // Requires copy constructor
    template<class T>
    void LuaPushUserdata(lua_State *L, const T &v);
    template<class T>
    void LuaPushLightUserData(lua_State *L, T *v);

    // Works for both userdata and light userdata
    template<class T>
    T *LuaGetUserdata(lua_State *L, int idx);

    inline void LuaPushValue(lua_State *L, const bool &v) {
        lua_pushboolean(L, v);
    }

    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
    inline void LuaPushValue(lua_State *L, const T &v) {
        lua_pushnumber(L, v);
    }

    inline void LuaPushValue(lua_State *L, const std::string &v) {
        lua_pushstring(L, v.c_str());
    }

    template<class T>
    void LuaPushValue(lua_State *L, const std::vector<T> &v) {
        lua_createtable(L, (int)v.size(), 0);
        for (int i = 1; i <= v.size(); i++) {
            LuaPushValue(L, v[i - 1]);
            lua_rawseti(L, -2, i);
        }
    }

    template<class T>
    void LuaPushValue(lua_State *L, const std::map<std::string, T> &v) {
        lua_createtable(L, 0, v.size());
        for (const auto &[k, v_] : v) {
            LuaPushValue(L, v_);
            lua_setfield(L, -2, k.c_str());
        }
    }

    template<class T>
    void LuaPushValue(lua_State *L, const T &v) {
        const auto view = rfl::to_view(v);
        lua_createtable(L, 0, (int)view.num_fields());

        view.apply([&](const auto &f) {
            const auto name = f.name();
            lua_pushlstring(L, name.data(), name.size());
            LuaPushValue(L, *f.value());
            lua_settable(L, -3);
        });
    }

    inline bool LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TBOOLEAN);
        return lua_toboolean(L, idx);
    }

    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
    inline T LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TNUMBER);
        return static_cast<T>(lua_tonumber(L, idx));
    }

    template<class T> requires (std::is_same_v<T, std::string>)
    inline T LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TSTRING);
        return std::string(lua_tostring(L, idx));
    }

    template<class T> requires (is_vector<T>::value)
    T LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TTABLE);
        T v;
        int size = lua_rawlen(L, idx);
        v.reserve(size);

        for (int i = 1; i <= size; i++) {
            lua_rawgeti(L, idx, i);
            v.push_back(LuaGetValue<typename T::value_type>(L, -1));
            lua_pop(L, 1);
        }

        return v;
    }

    template<class T> requires (is_string_key_map<T>::value)
    T LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TTABLE);
        T v;

        lua_pushnil(L);
        if (idx < 0) idx--;
        while (lua_next(L, idx) != 0) {
            v[LuaGetValue<std::string>(L, -2)] = LuaGetValue<typename T::mapped_type>(L, -1);
            lua_pop(L, 1);
        }
        
        return v;
    }

    template<class T>
    T LuaGetValue(lua_State *L, int idx) {
        CheckLuaType(L, idx, LUA_TTABLE);
        T v;
        const auto view = rfl::to_view(v);

        if (idx < 0) idx--;
        view.apply([&](auto &f) {
            using FieldType = std::remove_reference_t<decltype(*f.value())>;

            const auto name = f.name();
            lua_pushlstring(L, name.data(), name.size());
            lua_rawget(L, idx);
            CheckLuaType(L, -1, TemplateToLuaType<FieldType>());
            *f.value() = LuaGetValue<FieldType>(L, -1);
            lua_pop(L, 1);
        });

        return v;
    }

    template <class T, T... S, class F>
    constexpr void ForSequence(std::integer_sequence<T, S...>, F&& f) {
        (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
    }

    template<class T>
    void LuaSetmetatable(lua_State *L, int idx) {
        static_assert(HasLuaCFunctions<T>::value);
        constexpr auto nbCFunctions = std::tuple_size_v<decltype(T::__s_luaCFunctions__)>;

        const auto name = TypeName<T>();
        if (luaL_newmetatable(L, name.c_str())) {
            ForSequence(std::make_index_sequence<nbCFunctions>{}, [&](auto i) {
                constexpr auto f = std::get<i>(T::__s_luaCFunctions__);
                lua_pushcfunction(L, std::get<1>(f));
                lua_setfield(L, -2, std::get<0>(f));
            });

            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");

            lua_pushcfunction(L, [](lua_State *L) -> int {
                luaL_error(L, "Cannot add new field to userdata.");
                return 0;
            });
            lua_setfield(L, -2, "__newindex");

            lua_pushcfunction(L, [](lua_State *L) -> int {
                auto ud = LuaGetUserdata<T>(L, -1);
                ud->~T();
                return 0;
            });
            lua_setfield(L, -2, "__gc");
        }
        
        lua_setmetatable(L, idx < 0 ? idx - 1 : idx);
    }

    template<class T>
    void LuaPushUserdata(lua_State *L) {
        void *p = lua_newuserdata(L, sizeof(T));
        new (p) T();
        
        LuaSetmetatable<T>(L, -1);
    }

    template<class T>
    void LuaPushUserdata(lua_State *L, const T &v) {
        void *p = lua_newuserdata(L, sizeof(T));
        new (p) T(v);
        
        LuaSetmetatable<T>(L, -1);
    }

    template<class T>
    void LuaPushLightUserData(lua_State *L, T *v) {
        lua_pushlightuserdata(L, (void*)v);

        LuaSetmetatable<T>(L, -1);
    }

    template<class T>
    inline T *LuaGetUserdata(lua_State *L, int idx) {
        const auto name = TypeName<T>();
        void *p = luaL_testudata(L, idx, name.c_str());
        if (p == NULL) {
            throw RuntimeErrorFormat("Top is not a userdata of type {} but is {}", name, luaL_typename(L, idx));
        }

        return (T*)p;
    }
}