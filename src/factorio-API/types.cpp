#include "prototypes.hpp"
#include "types.hpp"

namespace ComputerPlaysFactorio {

    Blueprint DecodeBlueprint(const std::string &str) {
        struct DecodeHelper {
            std::optional<Blueprint> blueprint;
        };

        auto compressed = base64_decode(str.substr(1), true);

        Blueprint blueprint;
        uLongf buff_size = (uLongf)compressed.size() * 2;
        while (true) {
            char *buffer = new char[buff_size];
            uLongf buff_size_ = buff_size;
            int r = uncompress((Bytef*)buffer, &buff_size_, 
                (const Bytef*)compressed.c_str(), (uLongf)compressed.size() + 1);

            switch (r) {
            case Z_BUF_ERROR:
                buff_size *= 2;
                break;

            case Z_DATA_ERROR:
                throw RuntimeErrorF("Invalid blueprint string.");

            case Z_OK: {
                auto json = std::string(buffer, buff_size_);
                auto decode = rfl::json::read<DecodeHelper, rfl::DefaultIfMissing>(json);
                if (!decode) {
                    throw RuntimeErrorF("Invalid blueprint string. error: {}, string: {}", decode.error().what(), json);
                }
                auto helper = decode.value();
                if (!helper.blueprint) {
                    throw RuntimeErrorF("Invalid blueprint string. Make sure that it is an actual blueprint and not a book.");
                }
                blueprint = helper.blueprint.value();
                goto Found;
            }

            default:
                throw RuntimeErrorF("Unhandled zlib error: {}", r);
            }
        }

        Found:

        for (auto &e : blueprint.entities) {
            auto &p = g_prototypes.GetEntity(e.name);
            e.type = p["type"];
        }

        return blueprint;
    }
}