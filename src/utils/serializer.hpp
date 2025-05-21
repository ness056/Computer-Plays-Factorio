#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <cstring>
#include <cstdint>
#include <QtCore>

/**
 * Used to serialize data sent between C++ and Factorio.
 * 
 * Supports booleans, all numeric values, enums, strings, C arrays, vectors and maps with string as keys.
 * Custom classes can also be serialize if they have a public member called
 *  properties that specifies what members are to be included in the serialization.
 * Does not support pointers.
 * 
 * Example:
 * 
 *  using namespace ComputerPlaysFactorio;
 *
 *  template<class T>
 *  struct Test {
 *      // included in the serialization
 *      int32_t a;
 *      bool b;
 *      MyEnum c;
 *      SomeOtherSerializableClass d;
 *      std::string e;
 *      std::vector<T> f;
 *      std::map<std::string, uint64_t> g;
 * 
 *      // not included in the serialization
 *      uint8_t h;
 *      bool i;
 *      std::string j;
 * 
 *  private:
 *      // also included but with a modified name in order to remove the m_
 *      bool m_k;   // will become "k": true/false in JSON
 *      int m_l;
 *  
 *      SERIALIZABLE_BEGIN
 *          PROPERTIES(Test<T>, a, b, c, d, e, f, g),    // !!!! Do NOT forget the comma at the end !!!!
 *          CUSTOM_NAME_PROPERTIES(Test<T>, "k", somethingWeDontWant_k, "l", somethingWeDontWant_l)
 *      SERIALIZABLE_END
 * 
 *      // Additionally, in case all the members we want to include can be named automatically,
 *      // we can use the following macro instead of the 4 lines above:
 *      SERIALIZABLE(a, b, c, d, e, f, g)
 *  };
 * 
 *  int main() {
 *      Test<std::string> data;
 *      QJsonObject json = ToJson(data);
 *  
 *      Test<std::string> deserialized;
 *      FromJson(json, deserialized);
 * 
 *      return 0;
 *  }
 */

namespace ComputerPlaysFactorio {

    template<class Class, class T>
    struct SerializerProperty {
        constexpr SerializerProperty(const char *name_, T Class::* member_) : name(name_), member{member_} {}

        const char *name;
        T Class:: *member;
    };

#define SERIALIZABLE_BEGIN \
private:\
    template<class __T__>\
    friend QJsonObject ToJson(const __T__ &v);\
    template<class __T__>\
    friend void FromJson(const QJsonValue &json, __T__ &out);\
    constexpr static auto s_properties = std::make_tuple(

#define SERIALIZABLE_END );

#define PARENS ()
#define COMMA ,

#define EXPAND(...) EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define PROPERTIES(className, ...) __VA_OPT__(EXPAND(PROPERTIES_HELPER(className, __VA_ARGS__)))
#define PROPERTIES_HELPER(className, v, ...) \
    SerializerProperty(#v, &className::v) __VA_OPT__(COMMA PROPERTIES_AGAIN PARENS (className, __VA_ARGS__))
#define PROPERTIES_AGAIN() PROPERTIES_HELPER

#define CUSTOM_NAME_PROPERTIES(className, ...) __VA_OPT__(EXPAND(CUSTOM_NAME_PROPERTIES_HELPER(className, __VA_ARGS__)))
#define CUSTOM_NAME_PROPERTIES_HELPER(className, n, v, ...) \
    SerializerProperty(n, &className::v) __VA_OPT__(COMMA CUSTOM_NAME_PROPERTIES_AGAIN PARENS (className, __VA_ARGS__))
#define CUSTOM_NAME_PROPERTIES_AGAIN() CUSTOM_NAME_PROPERTIES_HELPER

#define SERIALIZABLE(className, ...) \
    SERIALIZABLE_BEGIN \
    PROPERTIES(className, __VA_ARGS__) \
    SERIALIZABLE_END

#define SERIALIZABLE_CUSTOM_NAMES(className, ...) \
    SERIALIZABLE_BEGIN \
    CUSTOM_NAME_PROPERTIES(className, __VA_ARGS__) \
    SERIALIZABLE_END

    inline QJsonValue ToJson(const bool &v) {
        return QJsonValue(v);
    }

    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T>)
    inline QJsonValue ToJson(const T &v) {
        return QJsonValue(static_cast<qint64>(v));
    }

    template<class T> requires (std::is_floating_point_v<T>)
    inline QJsonValue ToJson(const T &v) {
        return QJsonValue(static_cast<double>(v));
    }

    inline QJsonValue ToJson(const std::string &v) {
        return QJsonValue(v.c_str());
    }

    template<class T>
    QJsonArray ToJson(const std::vector<T> &v) {
        QJsonArray r;
        for (const auto &v_ : v) {
            r.push_back(ToJson(v_));
        }
        return r;
    }

    template<class T>
    QJsonObject ToJson(const std::map<std::string, T> &v) {
        QJsonObject r;
        for (const auto &[k, v_] : v) {
            r[k.c_str()] = ToJson(v_);
        }
        return r;
    }

    inline void FromJson(const QJsonValue &json, bool &out) {
        out = json.toBool();
    }

    template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T>)
    inline void FromJson(const QJsonValue &json, T &out) {
        out = static_cast<T>(json.toInteger());
    }

    template<class T> requires (std::is_floating_point_v<T>)
    inline void FromJson(const QJsonValue &json, T &out) {
        out = static_cast<T>(json.toDouble());
    }

    inline void FromJson(const QJsonValue &json, std::string &out) {
        out = json.toString().toStdString();
    }

    template<class T>
    void FromJson(const QJsonValue &json, std::vector<T> &out) {
        if (json.isArray()) {
            auto array = json.toArray();
            out.clear();
            out.resize(array.size());
            auto it = out.begin();
            for (const auto &v : array) {
                FromJson(v, *it);
                it++;
            }
        }
    }

    template<class T>
    void FromJson(const QJsonValue &json, std::map<std::string, T> &out) {
        if (json.isObject()) {
            auto object = json.toObject();
            out.clear();
            for (auto it = object.begin(); it != object.end(); it++) {
                T v_;
                FromJson(it.value(), v_);
                out[it.key().toStdString()] = v_;
            }
        }
    }

    template <class T, T... S, class F>
    constexpr void ForProperties(std::integer_sequence<T, S...>, F&& f) {
        (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
    }

    template<class T>
    QJsonObject ToJson(const T &v) {
        constexpr auto nbProperties = std::tuple_size<decltype(T::s_properties)>::value;
        QJsonObject r;

        ForProperties(std::make_index_sequence<nbProperties>{}, [&](auto j) {
            constexpr auto property = std::get<j>(T::s_properties);

            r[property.name] = ToJson(v.*(property.member));
        });

        return r;
    }

    template<class T>
    void FromJson(const QJsonValue &json, T &out) {
        constexpr auto nbProperties = std::tuple_size<decltype(T::s_properties)>::value;

        ForProperties(std::make_index_sequence<nbProperties>{}, [&](auto j) {
            constexpr auto property = std::get<j>(T::s_properties);
            if (const QJsonValue v = json[property.name]; !v.isUndefined()) {
                FromJson(v, out.*(property.member));
            }
        });
    }
}