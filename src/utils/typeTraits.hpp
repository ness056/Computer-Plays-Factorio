#pragma once

#include <type_traits>
#include <vector>
#include <map>
#include <string>
#include <optional>

namespace ComputerPlaysFactorio {

    template<class>
    struct is_vector : std::false_type {};

    template<class T, class A>
    struct is_vector<std::vector<T, A>> : std::true_type {};

    template<class T>
    constexpr auto is_vector_v = is_vector<T>::value;

    template<class>
    struct is_string_key_map : std::false_type {};

    template<class T, class C, class A>
    struct is_string_key_map<std::map<std::string, T, C, A>> : std::true_type {};

    template<class T>
    constexpr auto is_string_key_map_v = is_string_key_map<T>::value;

    template<class>
    struct is_optional : std::false_type {};

    template<class T>
    struct is_optional<std::optional<T>> : std::true_type {};

    template<class T>
    constexpr auto is_optional_v = is_optional<T>::value;

    template<class T> struct remove_optional { typedef T type; };
    template<class T> struct remove_optional<std::optional<T>> { typedef T type; };

    template<class T>
    using remove_optional_t = remove_optional<T>::type;
}