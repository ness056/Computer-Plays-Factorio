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
 *      constexpr static auto properties = std::make_tuple(
 *          SerializerProperty("a", &Test<T>::a),
 *          SerializerProperty("b", &Test<T>::b),
 *          SerializerProperty("c", &Test<T>::c),
 *          SerializerProperty("d", &Test<T>::d),
 *          SerializerProperty("e", &Test<T>::e),
 *          SerializerProperty("f", &Test<T>::f),
 *          SerializerProperty("g", &Test<T>::g)
 *      );
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

void FromJson(const QJsonValue &json, bool &out) {
    out = json.toBool();
}

template<class T> requires (std::is_integral_v<T> || std::is_enum_v<T>)
void FromJson(const QJsonValue &json, T &out) {
    out = static_cast<T>(json.toInteger());
}

template<class T> requires (std::is_floating_point_v<T>)
void FromJson(const QJsonValue &json, T &out) {
    out = static_cast<T>(json.toDouble());
}

void FromJson(const QJsonValue &json, std::string &out) {
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
constexpr void ForSequence(std::integer_sequence<T, S...>, F&& f) {
    (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
}

template<class T>
QJsonObject ToJson(const T &v) {
    constexpr auto nbProperties = std::tuple_size<decltype(T::properties)>::value;
    QJsonObject r;

    ForSequence(std::make_index_sequence<nbProperties>{}, [&](auto j) {
        constexpr auto property = std::get<j>(T::properties);

        r[property.name] = ToJson(v.*(property.member));
    });

    return r;
}

template<class T>
void FromJson(const QJsonValue &json, T &out) {
    constexpr auto nbProperties = std::tuple_size<decltype(T::properties)>::value;

    ForSequence(std::make_index_sequence<nbProperties>{}, [&](auto j) {
        constexpr auto property = std::get<j>(T::properties);
        if (const QJsonValue v = json[property.name]; !v.isUndefined()) {
            FromJson(v, out.*(property.member));
        }
    });
}
}