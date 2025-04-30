#pragma once

#include <QtCore>
#include <map>
#include <list>
#include <functional>
#include <string>

namespace ComputerPlaysFactorio {

template<class Class, class T>
struct SerializerProperty {
    constexpr SerializerProperty(const char *name_, T Class::* member_) : name(name_), member{member_} {}
    const char *name;
    T Class:: *member;
};

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

class SerializableFactory;

/**
 * This class should not be derived from.
 * To make a class serializable derive that class from Serializable<class>.
 */
class SerializableBase {
public:
    virtual void Test() = 0;
    virtual ~SerializableBase() = default;

protected:
    SerializableBase() = default;

    virtual std::string GetName() const { return "unamed"; }
};

template <typename T>
class Serializable : public SerializableBase {
protected:
    Serializable() = default;

private:
    struct Register {
        Register() {
            SerializableFactory::Register(s_id,
                {.name = T().GetName(), .Instantiate = [] { return std::make_unique<T>(); }}
            );
        }
    };
    static inline const Register s_register = Register();

public:
    static inline const std::size_t s_id = reinterpret_cast<std::size_t>(&s_register);
};

/**
 * This is mainly used for manual request sending for debuging
 */
class SerializableFactory {
public:
    using InstantiationMethod = std::function<std::unique_ptr<SerializableBase>()>;

    struct Descriptor {
        std::string name;
        InstantiationMethod Instantiate;
    };

    SerializableFactory() = delete;

    static void Register(const std::size_t id, Descriptor descriptor);
    static std::unique_ptr<SerializableBase> Instantiate(const std::size_t id);
    static inline const std::unordered_map<std::size_t, Descriptor> &GetAllClasses() {
        return s_descriptors;
    }

private:
    static inline std::unordered_map<std::size_t, Descriptor> s_descriptors;
};
}

#define SerializerPropertiesBegin(name) \
    public: virtual std::string VGetName() const override { return "C3"; } \
    constexpr static auto properties = std::make_tuple(