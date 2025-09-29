#include "prototypes.hpp"
#include "types.hpp"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace ComputerPlaysFactorio {

    void to_json(json &j, const MapPosition &pos) {
        j["x"] = pos.x;
        j["y"] = pos.y;
    }

    void from_json(const json &j, MapPosition &pos) {
        if (j.contains("x")) {
            pos.x = j.at("x").get<double>();
        } else {
            pos.x = j.at(0).get<double>();
        }

        if (j.contains("y")) {
            pos.y = j.at("y").get<double>();
        } else {
            pos.y = j.at(1).get<double>();
        }
    }

    void to_json(json &j, const Area &area) {
        j["left_top"] = area.left_top;
        j["right_bottom"] = area.right_bottom;
    }
    
    void from_json(const json &j, Area &area) {
        if (j.contains("left_top")) {
            area.left_top = j.at("left_top").get<MapPosition>();
        } else {
            area.left_top = j.at(0).get<MapPosition>();
        }

        if (j.contains("right_bottom")) {
            area.right_bottom = j.at("right_bottom").get<MapPosition>();
        } else {
            area.right_bottom = j.at(1).get<MapPosition>();
        }
    }

    Blueprint DecodeBlueprintStr(const std::string &str) {
        auto compressed = base64_decode(str.substr(1), true);

        json blueprint_json;
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
                ErrorAndExit("Invalid blueprint string.");

            case Z_OK: {
                auto j_str = std::string(buffer, buff_size_);
                json j;
                try {
                    j = json::parse(j_str);
                } catch (...) {
                    ErrorAndExit("Invalid blueprint string");
                }

                if (!j.contains("blueprint")) {
                    Debug("Blueprint json: {}", j_str);
                    ErrorAndExit("Invalid blueprint string. Make sure that it is an actual blueprint and not a book.");
                }
                blueprint_json = std::move(j["blueprint"]);
                goto Found;
            }

            default:
                throw RuntimeErrorF("Unhandled zlib error: {}", r);
            }
        }

        Found:

        if (!blueprint_json.contains("entities") || !blueprint_json["entities"].is_array()) return {};

        size_t n_entity = blueprint_json["entities"].size();
        Blueprint blueprint;
        blueprint.center = {0, 0};
        blueprint.entities.reserve(n_entity);

        for (auto &e : blueprint_json["entities"]) {
            if (!e.contains("name") || !e.contains("position")) {
                ErrorAndExit("Invalid blueprint string.");
            }

            auto &p = g_prototypes.GetEntity(e["name"]);
            auto pos = e["position"].get<MapPosition>();
            blueprint.center += pos;

            blueprint.entities.emplace_back(
                p["type"],
                e["name"].get<std::string>(),
                pos,
                e.contains("direction") ? e["direction"].get<Direction>() : Direction::NORTH,
                e.contains("mirror") ? e["mirror"].get<bool>() : false,
                true,
                p.contains("collision_box") ? p["collision_box"].get<Area>() + pos : Area{},
                e.contains("recipe") ? e["recipe"].get<std::string>() : "",
                e.contains("type") ? e["type"].get<std::string>() : "",
                e.contains("input_priority") ? e["input_priority"].get<std::string>() : "",
                e.contains("output_priority") ? e["output_priority"].get<std::string>() : ""
            );
        }

        blueprint.center /= (double)n_entity;

        return blueprint;
    }

    Blueprint DecodeBlueprintFile(const std::filesystem::path &path) {
        auto size = std::filesystem::file_size(path);
        std::string bp(size, '\0');
        std::ifstream in(path);
        in.read(&bp[0], size);
        return DecodeBlueprintStr(bp);
    }
}