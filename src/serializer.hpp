#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <vector>
#include <map>
#include <tuple>
#include <cstring>
#include <cstdint>

#include <iostream>

/**
 * Used to serialize data sent between C++ and Factorio.
 * 
 * Supports all numeric values, enums, strings, C arrays, vectors and maps.
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
 *      unsigned short d[40];
 *      std::string e;
 *      std::vector<T> f;
 *      std::map<std::string, uint64_t> g;
 * 
 *      // not included in the serialization
 *      uint8_t h;
 *      bool i;
 *      std::string j;
 *  
 *      constexpr static auto properties = MakeSerializerProperties(
 *          &Test<T>::a, &Test<T>::b, &Test<T>::c, &Test<T>::d,
 *          &Test<T>::e, &Test<T>::f, &Test<T>::g
 *      );
 *  };
 * 
 *  int main() {
 *      Test<std::string> data;
 *      std::string serialize;
 *      Serialize(data, serialize);
 *  
 *      Test<std::string> deserialize;
 *      size_t i = 0; // The index of the begining of the data in the string
 *      bool success = Deserialize(serialize, i, deserialize);
 *      // i is now the index of the end of the data in the string.
 *      // If the string data was invalid, some members of deserialize may become
 *      // filled with trash stuff.
 * 
 *      return 0;
 *  }
 */

namespace ComputerPlaysFactorio {

template<class T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
void Serialize(const T &data, std::string &out) {
    const char *p = reinterpret_cast<const char*>(&data);
    out.append(p, sizeof(data));
}

template<class T> requires (std::is_bounded_array_v<T>)
void Serialize(const T &data, std::string &out) {
    const char *p = reinterpret_cast<const char*>(data);
    out.append(p, sizeof(data));
}

void Serialize(const std::string &data, std::string &out);

template<class T>
void Serialize(const std::vector<T> &data, std::string &out) {
    Serialize(data.size(), out);
    for (auto const &e : data) {
        Serialize(e, out);
    }
}

template<class K, class T>
void Serialize(const std::map<K, T> &data, std::string &out) {
    Serialize(data.size(), out);
    for (auto const &[k, v] : data) {
        Serialize(k, out);
        Serialize(v, out);
    }
}

template<class T> requires (std::is_arithmetic_v<T> || std::is_enum_v<T>)
bool Deserialize(const std::string &data, size_t &i, T &out) {
    if (data.size() - i < sizeof(out)) return false;
    out = *reinterpret_cast<const T*>(&data[i]);
    i += sizeof(out);
    return true;
}

template<class T> requires (std::is_bounded_array_v<T>)
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
    out.resize(size);
    for (size_t j = 0; j < size; j++) {
        if (!Deserialize(data, i, out[j])) return false;
    }
    return true;
}

template<class K, class T>
bool Deserialize(const std::string &data, size_t &i, std::map<K, T> &out) {
    size_t size;
    if (!Deserialize(data, i, size)) return false;
    out.clear();
    for (size_t j = 0; j < size; j++) {
        K k; T v;
        if (!Deserialize(data, i, k)) return false;
        if (!Deserialize(data, i, v)) return false;
        out[k] = v;
    }
    return true;
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