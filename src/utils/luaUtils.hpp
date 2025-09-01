#pragma once

#include <lua.hpp>
#include <new>
#include <functional>
#include <tuple>
#include <optional>
#include <expected>

#include "includeReflectcpp.hpp"
#include "typeTraits.hpp"
#include "logging.hpp"

#define LUA_CFUNCTIONS \
private: \
    template<class T> \
    friend void ::ComputerPlaysFactorio::LuaSetmetatable(lua_State *L, int idx, bool lightUserdata); \
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
        } else if constexpr (is_vector_v<T> || is_string_key_map_v<T>) {
            return LUA_TTABLE;
        }
    }

    inline void LuaPushValue(lua_State *L, const bool &v) {
        lua_pushboolean(L, v);
    }

    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T> || std::is_floating_point_v<T>)
    inline void LuaPushValue(lua_State *L, const T &v) {
        lua_pushnumber(L, v);
    }

    inline void LuaPushValue(lua_State *L, const char *v) {
        lua_pushstring(L, v);
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

    template<class T> requires (is_vector_v<T>)
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

    template<class T> requires (is_string_key_map_v<T>)
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
        static_assert(std::is_base_of<LuaClassBase, T>);
        constexpr auto nbCFunctions = std::tuple_size_v<decltype(T::__s_luaCFunctions__)>;

        const auto &name = T::s_luaName;
        if (luaL_newmetatable(L, name.c_str())) {
            ForSequence(std::make_index_sequence<nbCFunctions>{}, [&](auto i) {
                constexpr auto f = std::get<i>(T::__s_luaCFunctions__);
                const auto funcName = std::get<0>(f);
                LuaPushValue(L, funcName);
                lua_pushcclosure(L, std::get<1>(f), 1);
                lua_setfield(L, -2, funcName);
            });

            lua_pushvalue(L, -1);
            lua_setfield(L, -2, "__index");

            LuaPushValue(L, "__newindex");
            lua_pushcclosure(L, [](lua_State *L) -> int {
                luaL_error(L, "Cannot add new field to userdata.");
                return 0;
            }, 1);
            lua_setfield(L, -2, "__newindex");

            LuaPushValue(L, "__gc");
            lua_pushcclosure(L, [](lua_State *L) -> int {
                auto ud = LuaGetUserdata<T>(L, -1);
                ud->~T();
                return 0;
            }, 1);
            lua_setfield(L, -2, "__gc");
        }

        lua_setmetatable(L, idx < 0 ? idx - 1 : idx);
    }

    template<class T>
    T *LuaPushUserdata(lua_State *L) {
        void *p = lua_newuserdata(L, sizeof(T));
        new (p) T();
        
        LuaSetmetatable<T>(L, -1);
        return (T*)p;
    }

    template<class T>
    T *LuaPushUserdata(lua_State *L, const T &v) {
        void *p = lua_newuserdata(L, sizeof(T));
        new (p) T(v);
        
        LuaSetmetatable<T>(L, -1);
        return (T*)p;
    }

    template<class T>
    void LuaPushLightUserdata(lua_State *L, T *v) {
        lua_pushlightuserdata(L, (void*)v);
    }

    template<class T>
    T *LuaGetUserdata(lua_State *L, int idx) {
        static_assert(std::is_base_of<LuaClassBase, T>);

        const auto &name = T::s_luaName;
        void *p = luaL_testudata(L, idx, name.c_str());
        if (p == NULL) {
            throw RuntimeErrorFormat("Top is not a userdata of type {} but is {}", name, luaL_typename(L, idx));
        }

        return (T*)p;
    }

    template<class Return, class ...Args>
    class LuaFunction {
    public:
        ~LuaFunction() {
            Unregister();
        }

        void Set(lua_State *L, int idx) {
            if (!lua_isfunction(L, idx) || idx < 0) {
                throw std::runtime_error();
            }

            m_L = L;
            m_idx = idx;
            m_valid = true;
        }

        inline bool IsValid() {
            return m_valid;
        }

        std::expected<Return, std::string> Call(const Args &...args) {
            if (!m_valid) {
                return std::unexpected("Invalid");
            }

            auto top = lua_gettop(m_L);

            if (m_isRegistered) {
                lua_rawgeti(m_L, LUA_REGISTRYINDEX, s_tableIdx);
                lua_rawgeti(m_L, -1, m_registeredIdx);
            } else {
                lua_pushvalue(m_L, m_idx);
            }

            for (auto &arg : args) {
                LuaPushValue(m_L, arg);
            }

            if (lua_pcall(m_L, sizeof...(Args), m_hasReturnValue ? 1 : 0, 0)) {
                auto msg = LuaGetValue<std::string>(m_L, -1);
                lua_settop(top);
                return std::unexpected(msg);
            } else if (m_hasReturnValue) {
                auto value = LuaGetValue<Return>(m_L, -1);
                lua_settop(top);
                return value;
            }
        }

        void Register() {
            if (m_isRegistered) return;
            StaticInit(m_L);

            lua_rawgeti(m_L, LUA_REGISTRYINDEX, s_tableIdx);
            lua_pushvalue(m_L, m_idx);
            m_registeredIdx = luaL_ref(m_L, -2);
            lua_pop(m_L, 1);
        }

        void Unregister() {
            if (!m_isRegistered) return;

            lua_rawgeti(m_L, LUA_REGISTRYINDEX, s_tableIdx);
            luaL_unref(m_L, -1, m_isRegistered);
            lua_pop(m_L, 1);
        }

    private:
        static void StaticInit(lua_State *L) {
            if (s_staticInit) return;

            lua_newtable(L);
            s_tableIdx = luaL_ref(L, LUA_REGISTRYINDEX);
        }

        const bool m_hasReturnValue = std::is_same_v<Return, void> ? false : true;
        lua_State *m_L;
        int m_idx;
        bool m_valid = false;

        static inline int s_tableIdx;
        static inline bool s_staticInit = false;

        bool m_isRegistered = false;
        int m_registeredIdx;
    };

    template<class>
    struct is_luaFunction : std::false_type {};

    template<class Return, class ...Args>
    struct is_luaFunction<LuaFunction<Return(Args...)>> : std::true_type {};

    template<class T>
    constexpr auto is_luaFunction_v = is_luaFunction<T>::value;

    template<class ...Args>
    std::tuple<Args...> LuaGetParameters(lua_State *L) {
        auto name = lua_tostring(L, lua_upvalueindex(1));

        constexpr size_t nbArgs = sizeof...(Args);
        constexpr size_t nbOptionals = ((is_optional_v<Args> ? 1 : 0) + ...);
        constexpr size_t nbMandatory = nbArgs - nbOptionals;

        int top = lua_gettop(L);
        if (top < nbMandatory || top > nbArgs) {
            luaL_error(L, "Wrong count of arguments for function %s: expected between %d and %d, got %d",
                name, nbOptionals, nbArgs, top);
        }

        int i = 0;
        return std::make_tuple([&] {
            using NoOptionalArg = remove_optional_t<Args>;

            i++;
            try {
                if constexpr (is_luaFunction_v<NoOptionalArg>) {
                    NoOptionalArg value;
                    value.Set(L, i);
                    return value;
                } else if constexpr (std::is_pointer_v<NoOptionalArg>) {
                    static_assert(std::is_base_of<LuaClassBase, T>);
                    return LuaGetUserdata<std::remove_pointer_t<NoOptionalArg>>(L, i);
                } else {
                    return LuaGetValue<NoOptionalArg>(L, i);
                }
            } catch (std::runtime_error e) {
                if constexpr (is_optional_v<Args>) {
                    return std::nullopt;
                } else if constexpr (is_luaFunction_v<NoOptionalArg>) {
                    luaL_error(L, "Function expected for argument #%d of function %s, got %s",
                        i, name, luaL_typename(L, i));
                } else if constexpr (std::is_pointer_v<NoOptionalArg>) {
                    luaL_error(L, "%s expected for argument #%d of function %s, got %s",
                        NoOptionalArg::s_luaName.c_str(), i, name, luaL_typename(L, i));
                } else {
                    luaL_error(L, "%s expected for argument #%d of function %s, got %s",
                        TypeName<NoOptionalArg>(), i, name, luaL_typename(L, i));
                }
            }

            // Without this the compiler thinks there is a error as it looks like
            // not all paths return although they do.
            // (luaL_error either calls abort or does a long jump)
            throw RuntimeExceptionFormat("How did we get here?");
        }()...);
    }

    class LuaRegistry {
    public:
        template<class T>
        static void Register() {
            static_assert(std::is_base_of<LuaClassBase, T>);
            static_assert(HasLuaCFunction<T>::value);
            const auto &name = T::s_luaName;

            if (!s_functions.contains[name]) {
                auto &m = s_functions.emplace(name);

                constexpr auto nbCFunctions = std::tuple_size_v<decltype(T::__s_luaCFunctions__)>;
                ForSequence(std::make_index_sequence<nbCFunctions>{}, [&](auto i) {
                    constexpr auto f = std::get<i>(T::__s_luaCFunctions__);
                    m[std::string(std::get<0>(f))] = std::get<1>(f);
                });
            }
        }

        static inline std::optional<lua_CFunction> GetFunction(std::string className, std::string functionName) {
            if (!s_functions.contains(className)) {
                return;
            }

            auto &m = s_functions[className];
            if (!m.contains(functionName)) {
                return;
            }

            return m[functionName];
        }

    private:
        static inline std::map<std::string, std::map<std::string, lua_CFunction>> s_functions;
    };

    class LuaClassBase {
    public:
        constexpr LuaClassBase(const std::string &luaName) : c_luaName(luaName) {}

        const std::string c_luaName;
    };

    template<char const *luaName>
    class LuaClass : LuaClassBase {
    public:
        constexpr LuaClass() : LuaClassBase(luaName) {}

        static constexpr std::string s_luaName = luaName;
    };

    template<class T>
    class LuaRegisterer {
    public:
        LuaRegisterer() {
            LuaRegistry::Register<T>();
        }
    };

    // Must be called before using LuaPushLightUserdata
    void LuaCreateLightUDataMeta(lua_State *L) {
        lua_pushlightuserdata(L, nullptr);

        if (luaL_newmetatable(L, "LightUserdataMeta")) {
            LuaPushValue(L, "__index");
            lua_pushcclosure(L, [](lua_State *L) -> int {
                auto args = LuaGetParameters<LuaClassBase*, std::string>(L);
                const auto &name = std::get<0>(args)->c_luaName;
                const auto &fieldName = std::get<1>(args);
                
                const auto fn = LuaRegistry::GetFunction(name, fieldName);
                if (!fn) {
                    luaL_error(L, "Field %s doesn't exist in class %s", name.c_str(), fieldName.c_str());
                }

                LuaPushValue(L, fieldName.c_str());
                lua_pushcclosure(L, fn.value(), 1);
                return 1;
            }, 1);
            lua_setfield(L, -2, "__index");

            LuaPushValue(L, "__newindex");
            lua_pushcclosure(L, [](lua_State *L) -> int {
                luaL_error(L, "Cannot add new field to userdata.");
                return 0;
            }, 1);
            lua_setfield(L, -2, "__newindex");
        } else {
            throw RuntimeErrorFormat("LightUserdataMeta already exists.");
        }

        lua_setmetatable(L, -2);
        lua_pop(L, 1);
    }
}