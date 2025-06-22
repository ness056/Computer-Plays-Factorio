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

        inline std::string ToString() const {
            return "(" + std::to_string(x) + "; " + std::to_string(y) + ")";
        }

        inline MapPosition Abs() const {
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

        constexpr MapPosition ChunkPosition() const {
            return MapPosition(std::floor(x / 32), std::floor(y / 32));
        }

        double x;
        double y;

        SERIALIZABLE(MapPosition, x, y)
    };

    struct Area {
        constexpr Area() = default;
        constexpr Area(MapPosition leftTop_, MapPosition rightBottom_) :
            leftTop(leftTop_), rightBottom(rightBottom_) {}

        MapPosition leftTop;
        MapPosition rightBottom;

        SERIALIZABLE_CUSTOM_NAMES(Area, "left_top", leftTop, "right_bottom", rightBottom);
    };

    using Path = std::vector<MapPosition>;

    struct Entity {
        std::string name;
        MapPosition position;
        Area boundingBox;

        SERIALIZABLE(Entity, name, position, boundingBox);
    };

    struct EntitySearchFilters {
        Area area;
        std::vector<std::string> name;
        std::vector<std::string> type;

        SERIALIZABLE(EntitySearchFilters, area, name, type);
    };
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};