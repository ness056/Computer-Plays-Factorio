#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

#include "../utils/base64.h"
#include "../utils/include-reflectcpp.hpp"
#include <zlib.h>

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

    class FactorioInstance;

    enum class Direction {
        NORTH = 0,
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
        NORTH_NORTH_WEST,
        END
    };

    constexpr Direction &operator+=(Direction &lhs, const Direction &rhs) {
        lhs = Direction(((int)lhs + (int)rhs) % (int)Direction::END);
        return lhs;
    }

    constexpr Direction operator+(Direction lhs, const Direction &rhs) {
        lhs += rhs;
        return lhs;
    }

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

        constexpr std::string ToString() const {
            return "(" + std::to_string(x) + "; " + std::to_string(y) + ")";
        }

        constexpr MapPosition Abs() const {
            return MapPosition(std::abs(x), std::abs(y));
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

        // NORTH direction is an angle of 0, EAST is pi/2 clockwise
        // Only supports the north, south, east and west. Others will return a copy.
        constexpr MapPosition Rotate(Direction direction) {
            switch (direction) {
            case Direction::SOUTH: return MapPosition(-x, -y);
            case Direction::WEST: return MapPosition(y, -x);
            case Direction::EAST: return MapPosition(-y, x);
            case Direction::NORTH:
            default:
                return MapPosition(x, y);
            }
        }

        constexpr MapPosition Round() const {
            return MapPosition(std::round(x), std::round(y));
        }

        static constexpr double SqDistance(const MapPosition &lhs, const MapPosition &rhs) {
            auto d = lhs - rhs;
            return d.x*d.x + d.y*d.y;
        }

        static inline double Distance(const MapPosition &lhs, const MapPosition &rhs) {
            return std::sqrt(SqDistance(lhs, rhs));
        }

        double x;
        double y;
    };

    struct Area {
        constexpr Area() = default;
        constexpr Area(MapPosition left_top_, MapPosition right_bottom_) :
            left_top(left_top_), right_bottom(right_bottom_) {}

        constexpr MapPosition GetLeftBottom() const {
            return MapPosition(left_top.x, right_bottom.y);
        }

        constexpr MapPosition GetRightTop() const {
            return MapPosition(right_bottom.x, left_top.y);
        }

        constexpr Area &operator+=(const MapPosition &pos) {
            left_top += pos;
            right_bottom += pos;
            return *this;
        }

        friend constexpr Area operator+(Area area, const MapPosition &pos) {
            area += pos;
            return area;
        }

        constexpr Area &operator-=(const MapPosition &pos) {
            left_top -= pos;
            right_bottom -= pos;
            return *this;
        }

        friend constexpr Area operator-(Area area, const MapPosition &pos) {
            area -= pos;
            return area;
        }

        constexpr double Distance(const MapPosition &point) const {
            auto x = std::max({left_top.x - point.x, 0., point.x - right_bottom.x});
            auto y = std::max({left_top.y - point.y, 0., point.y - right_bottom.y});

            auto s = x + y;
            if (s == x || s == y) return s;
            else return std::sqrt(x * x + y * y);
        }

        constexpr bool Collides(const MapPosition &point) const {
            return Distance(point) == 0;
        }

        constexpr bool Collides(const Area &other) const {
            return Collides(other.left_top) || Collides(other.right_bottom) ||
                Collides(other.GetLeftBottom()) || Collides(other.GetRightTop());
        }

        static constexpr Area FromChunkPosition(const MapPosition &chunk) {
            auto chunk_left_top = chunk * 32;
            return Area(chunk_left_top, chunk_left_top + MapPosition(32, 32));
        }

        MapPosition left_top;
        MapPosition right_bottom;
    };

    using Path = std::vector<MapPosition>;

    struct EntitySearchFilters {
        Area area;
        std::vector<std::string> name;
        std::vector<std::string> type;
    };

    enum class TileType {
        NORMAL,
        WATER
    };

    struct Entity {
        std::string type;
        std::string name;
        MapPosition position;
        Direction direction = Direction::NORTH;
        bool mirror = false;
        bool valid = true;

        std::optional<Area> bounding_box;

        // std::optional<std::string> recipe;
        // std::optional<std::string> type;            // type of underground "input" or "output"
        // std::optional<std::string> input_priority;  // "left" or "right"
        // std::optional<std::string> output_priority; // "left" or "right"
    };
    using UEntity = std::unique_ptr<Entity>;
    using SEntity = std::shared_ptr<Entity>;

    struct Blueprint {
        std::vector<Entity> entities;
        // tiles
        // schedules
    };

    Blueprint DecodeBlueprint(const std::string &str);
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
            ComputerPlaysFactorio::MapPosition left_top, right_bottom;
        };

        static ComputerPlaysFactorio::Area to(const ReflType &area) noexcept {
            return { area.left_top, area.right_bottom };
        }

        static ReflType from(const ComputerPlaysFactorio::Area &area) {
            return { area.left_top, area.right_bottom };
        }
    };
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};