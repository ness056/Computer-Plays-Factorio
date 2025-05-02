#include "serializer.hpp"

namespace ComputerPlaysFactorio {

template<class Class, class T>
QJsonValue SerializerProperty<Class, T>::GetJson() const {
    if (std::is_base_of_v<SerializableBase, Class>) {
        return *m_member.ToJson();
    } else {
        return QJsonValue(*m_member);
    }
}

template<class Class, class T>
bool SerializerProperty<Class, T>::SetJson(const QJsonValue &json) {
    if (std::is_base_of_v<SerializableBase, Class>) {
        *m_member = Class::FromJson(json);
        return true;
    } else {
        if ()
    }
}

template <typename T, const char *name>
QJsonObject Serializable<T, name>::ToJson() const {
    QJsonObject json;
    auto properties = GetProperties();

    for (auto &p : properties) {
        json[p->GetName()] = p->GetJson();
    }

    return json;
}

template <typename T, const char *name>
T Serializable<T, name>::FromJson(const QJsonObject &json) {
    T object;
    const std::vector<std::unique_ptr<SerializerPropertyBase>> &properties = object.GetProperties();

    for (auto &p : properties) {
        if (const QJsonValue v = json[p->GetName()]) {
            p->SetJson(v);
        }
    }

    return object;
}

void SerializableFactory::Register(const std::size_t id, Descriptor descriptor) {
    assert(s_descriptors.find(id) == s_descriptors.end() && "Double factory registration");
    s_descriptors[id] = descriptor;
}

std::unique_ptr<SerializableBase> SerializableFactory::Instantiate(const std::size_t id) {
    if (auto it = s_descriptors.find(id); it != s_descriptors.end()) {
        return it->second.Instantiate();
    }
    return nullptr;
}
}