#include "serializer.hpp"

namespace ComputerPlaysFactorio {

void Serialize(const std::string &data, std::string &out) {
    Serialize(data.size(), out);
    out.append(data);
}

bool Deserialize(const std::string &data, size_t &i, std::string &out) {
    size_t size;
    if (!Deserialize(data, i, size)) return false;
    if (data.size() - i < size) return false;
    out = data.substr(i, size);
    i += size;
    return true;
}
}