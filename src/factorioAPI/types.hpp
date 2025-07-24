#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

#include "../../external/includeReflectcpp.hpp"

namespace ComputerPlaysFactorio {

    class FactorioInstance;

    enum class Direction {
        NORTH,
        NORTH_NORTH_EAST,
        NORTH_EAST,
        EAST_NORTH_EAST,
        EAST,
        EAST_SOUTH_EAST,
        SOUTH_EAST,
        SOUTH_SOUTH_EAST,
        SOUTH,
        SOUTH_SOUTH_WEST,
        SOUTH_WEST,
        WEST_SOUTH_WEST,
        WEST,
        WEST_NORTH_WEST,
        NORTH_WEST,
        NORTH_NORTH_WEST
    };

    struct MapPosition {
        constexpr MapPosition() : x(0), y(0) {}
        constexpr MapPosition(double x_, double y_) : x(x_), y(y_) {}
        constexpr MapPosition(Direction direction) {
            switch (direction) {
                case Direction::NORTH: x =  0; y = -1; break;
                case Direction::SOUTH: x =  0; y =  1; break;
                case Direction::WEST:  x = -1; y =  0; break;
                case Direction::EAST:  x =  1; y =  0; break;

                case Direction::NORTH_WEST: x = -1; y = -1; break;
                case Direction::NORTH_EAST: x =  1; y = -1; break;
                case Direction::SOUTH_WEST: x = -1; y =  1; break;
                case Direction::SOUTH_EAST: x =  1; y =  1; break;

                case Direction::NORTH_NORTH_WEST: x = -0.5; y = -1;   break;
                case Direction::WEST_NORTH_WEST:  x = -1;   y = -0.5; break;
                case Direction::NORTH_NORTH_EAST: x =  0.5; y = -1;   break;
                case Direction::EAST_NORTH_EAST:  x =  1;   y = -0.5; break;

                case Direction::SOUTH_SOUTH_WEST: x = -0.5; y =  1;   break;
                case Direction::WEST_SOUTH_WEST:  x = -1;   y =  0.5; break;
                case Direction::SOUTH_SOUTH_EAST: x =  0.5; y =  1;   break;
                case Direction::EAST_SOUTH_EAST:  x =  1;   y =  0.5; break;
            }
        }

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

        constexpr MapPosition &operator*=(double rhs) {
            x *= rhs;
            y *= rhs;
            return *this;
        }

        friend constexpr MapPosition operator*(MapPosition lhs, double rhs) {
            lhs *= rhs;
            return lhs;
        }

        constexpr MapPosition &operator/=(double rhs) {
            x /= rhs;
            y /= rhs;
            return *this;
        }

        friend constexpr MapPosition operator/(MapPosition lhs, double rhs) {
            lhs /= rhs;
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

        static constexpr double Dot(const MapPosition &lhs, const MapPosition &rhs) {
            return lhs.x * rhs.x + lhs.y * rhs.y;
        }

        constexpr MapPosition Rotate(double angle) const {
            return MapPosition(
                std::cos(angle) * x - std::sin(angle) * y,
                std::sin(angle) * x + std::cos(angle) * y
            );
        }

        double x;
        double y;
    };

    struct Area {
        constexpr Area() = default;
        constexpr Area(MapPosition leftTop_, MapPosition rightBottom_) :
            leftTop(leftTop_), rightBottom(rightBottom_) {}

        constexpr MapPosition GetLeftBottom() const {
            return MapPosition(leftTop.x, rightBottom.y);
        }

        constexpr MapPosition GetRightTop() const {
            return MapPosition(rightBottom.x, leftTop.y);
        }

        constexpr double Distance(const MapPosition &point) const {
            auto x = std::max({leftTop.x - point.x, 0., point.x - rightBottom.x});
            auto y = std::max({leftTop.y - point.y, 0., point.y - rightBottom.y});

            auto s = x + y;
            if (s == x || s == y) return s;
            else return std::sqrt(x * x + y * y);
        }

        constexpr bool Collides(const MapPosition &point) const {
            return Distance(point) == 0;
        }

        constexpr bool Collides(const Area &other) const {
            return Collides(other.leftTop) || Collides(other.rightBottom) ||
                Collides(other.GetLeftBottom()) || Collides(other.GetRightTop());
        }

        static constexpr Area FromChunkPosition(const MapPosition &chunk) {
            auto leftTop = chunk * 32;
            return Area(leftTop, leftTop + MapPosition(31, 31));
        }

        MapPosition leftTop;
        MapPosition rightBottom;
    };

    using Path = std::vector<MapPosition>;

    struct Entity {
        std::string name;
        MapPosition position;
        Area boundingBox;
        bool collidesWithPlayer;
        bool valid;
    };

    struct EntitySearchFilters {
        Area area;
        std::vector<std::string> name;
        std::vector<std::string> type;
    };
}

namespace rfl {
    template<>
    struct Reflector<ComputerPlaysFactorio::MapPosition> {
        struct ReflType {
            double x, y;
        };

        static ComputerPlaysFactorio::MapPosition to(const ReflType &pos) noexcept {
            return { pos.x, pos.y };
        }

        static ReflType from(const ComputerPlaysFactorio::MapPosition &pos) {
            return { pos.x, pos.y };
        }
    };

    template<>
    struct Reflector<ComputerPlaysFactorio::Area> {
        struct ReflType {
            ComputerPlaysFactorio::MapPosition leftTop, rightBottom;
        };

        static ComputerPlaysFactorio::Area to(const ReflType &area) noexcept {
            return { area.leftTop, area.rightBottom };
        }

        static ReflType from(const ComputerPlaysFactorio::Area &area) {
            return { area.leftTop, area.rightBottom };
        }
    };
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};