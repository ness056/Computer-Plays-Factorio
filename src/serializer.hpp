#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <vector>
#include <tuple>
#include <cstring>
#include <cstdint>

namespace ComputerPlaysFactorio {

template<class T> requires std::is_arithmetic_v<T> || std::is_enum_v<T>
void Serialize(const T &data, std::string &out) {
    const char *p = reinterpret_cast<const char*>(&data);
    out.append(p, sizeof(data));
}

template<class T> requires std::is_bounded_array_v<T>
void Serialize(const T &data, std::string &out) {
    const char *p = reinterpret_cast<const char*>(data);
    out.append(p, sizeof(data));
}

void Serialize(const std::string &data, std::string &out);

template<class T>
void Serialize(const std::vector<T> &data, std::string &out) {
    Serialize(data.size(), out);
    for (auto &e : data) {
        Serialize(e, out);
    }
}

template<class T> requires std::is_arithmetic_v<T> || std::is_enum_v<T>
bool Deserialize(const std::string &data, size_t &i, T &out) {
    if (data.size() - i < sizeof(out)) return false;
    out = *reinterpret_cast<const T*>(&data[i]);
    i += sizeof(out);
    return true;
}

template<class T> requires std::is_bounded_array_v<T>
bool Deserialize(const std::string &data, size_t &i, T &out) {
    if (data.size() - i < sizeof(out)) return false;
    std::memcpy(out, &data[i], sizeof(out));
    i += sizeof(out);
    return true;
}

bool Deserialize(const std::string &data, size_t &i, std::string &out);

template<class T>
bool Deserialize(const std::string &data, size_t &i, std::vector<T> &out) {
    size_t size;
    if (!Deserialize(data, i, size)) return false;
    out.reserve(size * sizeof(T));
    for (size_t i = 0; i < size; i++) {
        if (!Deserialize(data, i, out[i])) return false;
    }
}

template<class Class, class T>
struct SerializerProperty {
    constexpr SerializerProperty(T Class::*member_) : member{member_} {}
    T Class::*member;
};

template<class Class, class... T>
constexpr auto MakeSerializerProperties(T Class::*... members) {
    return std::make_tuple(SerializerProperty(members)...);
}

template <class T, T... S, class F>
constexpr void ForSequence(std::integer_sequence<T, S...>, F&& f) {
    (static_cast<void>(f(std::integral_constant<T, S>{})), ...);
}

template<class T>
void Serialize(const T &object, std::string &out) {
    constexpr auto nbProperties = std::tuple_size<decltype(T::properties)>::value;

    ForSequence(std::make_index_sequence<nbProperties>{}, [&](auto j) {
        constexpr auto property = std::get<j>(T::properties);
        Serialize(object.*(property.member), out);
    });
}

template<class T>
bool Deserialize(const std::string &data, size_t &i, T &out) {
    constexpr auto nbProperties = std::tuple_size<decltype(T::properties)>::value;
    bool success = true;

    ForSequence(std::make_index_sequence<nbProperties>{}, [&](auto j) {
        constexpr auto property = std::get<j>(T::properties);
        if (!Deserialize(data, i, out.*(property.member))) success = false;
    });

    return success;
}
}