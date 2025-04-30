#include "serializer.hpp"

namespace ComputerPlaysFactorio {

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