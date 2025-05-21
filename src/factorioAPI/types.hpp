#pragma once

#include <string>
#include <cstdint>

#include "../utils/serializer.hpp"

namespace ComputerPlaysFactorio {

    enum Direction {
        North,
        Northeast,
        East,
        Southeast,
        South,
        Southwest,
        West,
        Northwest
    };

    struct MapPosition {
        constexpr MapPosition() : x(0), y(0) {}
        constexpr MapPosition(double x_, double y_) : x(x_), y(y_) {}

        inline std::string ToString() {
            return "(" + std::to_string(x) + "; " + std::to_string(y) + ")";
        }

        constexpr MapPosition &operator+=(const MapPosition &rhs) {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        friend constexpr MapPosition &operator+(MapPosition lhs, const MapPosition &rhs) {
            lhs += rhs;
            return lhs;
        }

        friend constexpr bool operator==(const MapPosition &lhs, const MapPosition &rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        friend constexpr bool operator!=(const MapPosition &lhs, const MapPosition &rhs) {
            return !(lhs == rhs);
        }

        constexpr MapPosition ChunkPosition() {
            return MapPosition(std::floor(x / 32), std::floor(y / 32));
        }

        double x;
        double y;

        SERIALIZABLE(MapPosition, x, y)
    };

    struct RequestDataless {
        uint32_t id;
        std::string name;

        SERIALIZABLE(RequestDataless, id, name)
    };

    enum RequestError {
        BUSY = 1,
        NO_PATH_FOUND = 2,
        EMPTY_PATH = 3,
        ENTITY_DOESNT_EXIST = 4,
        ITEM_DOESNT_EXIST = 5,
        NOT_ENOUGH_ITEM = 6,
        NOT_ENOUGH_ROOM = 7,
        NOT_IN_RANGE = 8,
        ITEM_CANNOT_BE_PLACED = 9
    };

    struct ResponseDataless {
        uint32_t id;
        bool success;
        RequestError error;

        SERIALIZABLE(ResponseDataless, id, success, error)
    };

    template<class T>
    struct Response : public ResponseDataless {
        T data;

        SERIALIZABLE(Response<T>, id, success, error, data)
    };

    struct EventDataless {
        uint32_t id;
        std::string name;
    };

    template<class T>
    struct Event : public EventDataless {
        T data;
    };

    // https://lua-api.factorio.com/stable/concepts/PathfinderWaypoint.html
    struct PathfinderWaypoint {
        MapPosition pos;
        bool needsDestroyToReach;

        SERIALIZABLE_CUSTOM_NAMES(PathfinderWaypoint, "position", pos, "needs_destroy_to_reach", needsDestroyToReach);
    };

    using Path = std::vector<PathfinderWaypoint>;

    struct RequestPathData {
        MapPosition start;
        MapPosition goal;

        SERIALIZABLE(RequestPathData, start, goal);
    };

    struct CraftData {
        std::string recipe;
        uint32_t amount;
        bool force;

        SERIALIZABLE(CraftData, recipe, amount, force);
    };

    struct BuildData {
        MapPosition position;
        std::string item;
        Direction direction;

        SERIALIZABLE(BuildData, position, item, direction);
    };

    struct RotateData {
        MapPosition position;
        std::string entity;
        bool reversed;

        SERIALIZABLE(RotateData, position, entity, reversed);
    };
    
    struct PutTakeData {
        MapPosition position;
        std::string entity;
        std::string item;
        int32_t amount;
        uint8_t playerInventory;
        uint8_t entityInventory;
        bool force;

        SERIALIZABLE(PutTakeData, position, entity, item, amount, playerInventory, entityInventory, force);
    };
}