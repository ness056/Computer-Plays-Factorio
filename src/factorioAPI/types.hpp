#pragma once

#include <string>
#include <cstdint>

#include "../utils/serializer.hpp"

namespace ComputerPlaysFactorio {

    class FactorioInstance;

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

        inline MapPosition Abs() {
            return MapPosition(std::abs(x), std::abs(y));
        }

        constexpr MapPosition &operator+=(const MapPosition &rhs) {
            x += rhs.x;
            y += rhs.y;
            return *this;
        }

        friend constexpr MapPosition operator+(MapPosition lhs, const MapPosition &rhs) {
            lhs += rhs;
            return lhs;
        }

        constexpr MapPosition &operator-=(const MapPosition &rhs) {
            x -= rhs.x;
            y -= rhs.y;
            return *this;
        }

        friend constexpr MapPosition operator-(MapPosition lhs, const MapPosition &rhs) {
            lhs -= rhs;
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

    struct Area {
        constexpr Area() = default;
        constexpr Area(MapPosition topLeft_, MapPosition bottomRight_) :
            topLeft(topLeft_), bottomRight(bottomRight_) {}

        MapPosition topLeft;
        MapPosition bottomRight;

        SERIALIZABLE_CUSTOM_NAMES(Area, "top_left", topLeft, "bottom_right", bottomRight);
    };

    // https://lua-api.factorio.com/stable/concepts/PathfinderWaypoint.html
    struct PathfinderWaypoint {
        PathfinderWaypoint(MapPosition pos_, bool needsDestroyToReach_) :
            pos(pos_), needsDestroyToReach(needsDestroyToReach_) {}

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

    struct Entity {
        std::string name;
        MapPosition position;
        Area boundingBox;

        SERIALIZABLE(Entity, name, position, boundingBox);
    };

    struct EntitySearchFilters {
        Area area;
        MapPosition position;
        double radius;
        std::vector<std::string> name;
        std::vector<std::string> type;

        SERIALIZABLE(EntitySearchFilters, area, position, radius, name, type);
    };
}