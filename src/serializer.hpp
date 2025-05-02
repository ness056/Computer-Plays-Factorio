#pragma once

#include <QtCore>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <type_traits>

namespace ComputerPlaysFactorio {

struct SerializerPropertyBase {
public:
    SerializerPropertyBase() = delete;

    inline const char *GetName() const {
        return p_name;
    }

    virtual std::string GetValue() const = 0;
    virtual bool SetValue(const std::string&) = 0;

    virtual QJsonValue GetJson() const = 0;
    virtual bool SetJson(const QJsonValue&) = 0;

protected:
    constexpr SerializerPropertyBase(const char *name_) : p_name(name_) {}
    const char *p_name;
};

template<class Class, class T>
struct SerializerProperty : SerializerPropertyBase {
    constexpr SerializerProperty(const char *name_, T Class::* member_) :
        SerializerPropertyBase(name_), m_member{member_} {}

    virtual QJsonValue GetJson() const override;
    virtual bool SetJson(const QJsonValue&) override;

    virtual std::string GetValue() const override;
    virtual bool SetValue(const std::string&) override;

private:
    T Class:: *m_member;
};

class SerializableFactory;

/**
 * This class should not be derived from.
 * To make a class serializable derive that class from Serializable<class>.
 */
class SerializableBase {
public:
    virtual void Test() = 0;
    virtual ~SerializableBase() = default;

    virtual QJsonObject ToJson() const = 0;

    virtual const std::vector<std::unique_ptr<SerializerPropertyBase>> &GetProperties() const = 0;

protected:
    SerializableBase() = default;
};

template <typename T, const char *name>
class Serializable : public SerializableBase {
public:
    virtual QJsonObject ToJson() const override;
    static T FromJson(const QJsonObject&);

protected:
    Serializable() = default;

private:
    struct Register {
        Register() {
            SerializableFactory::Register(s_id,
                {.name = name, .Instantiate = [] { return std::make_unique<T>(); }}
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