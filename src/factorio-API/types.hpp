#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <expected>

#include "../utils/base64.h"
#include <zlib.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../utils/logging.hpp"

namespace ComputerPlaysFactorio {

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

    // Returns the closest cardinal direction.
    // Diagonal directions (which are at equidistance of 2 cardinal direction) return the first cardinal direction when rotating clockwise
    constexpr Direction CardinalDirection(Direction direction) {
        switch (direction) {
            case Direction::NORTH: return Direction::NORTH;
            case Direction::SOUTH: return Direction::SOUTH;
            case Direction::WEST:  return Direction::WEST;
            case Direction::EAST:  return Direction::EAST;

            case Direction::NORTH_WEST: return Direction::NORTH;
            case Direction::NORTH_EAST: return Direction::EAST;
            case Direction::SOUTH_WEST: return Direction::WEST;
            case Direction::SOUTH_EAST: return Direction::SOUTH;

            case Direction::NORTH_NORTH_WEST: return Direction::NORTH;
            case Direction::WEST_NORTH_WEST:  return Direction::WEST;
            case Direction::NORTH_NORTH_EAST: return Direction::NORTH;
            case Direction::EAST_NORTH_EAST:  return Direction::EAST;

            case Direction::SOUTH_SOUTH_WEST: return Direction::SOUTH;
            case Direction::WEST_SOUTH_WEST:  return Direction::WEST;
            case Direction::SOUTH_SOUTH_EAST: return Direction::SOUTH;
            case Direction::EAST_SOUTH_EAST:  return Direction::EAST;
        }

        assert(false);
        return Direction::NORTH;
    }

    void ForEachCardinal(std::function<void(Direction)> callback);
    void ForEachDiagonal(std::function<void(Direction)> callback);

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

        constexpr MapPosition operator-() const {
            return MapPosition(-x, -y);
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

        constexpr MapPosition &operator*=(MapPosition rhs) {
            x *= rhs.x;
            y *= rhs.y;
            return *this;
        }

        friend constexpr MapPosition operator*(MapPosition lhs, MapPosition rhs) {
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

        constexpr MapPosition &operator/=(MapPosition rhs) {
            x /= rhs.x;
            y /= rhs.y;
            return *this;
        }

        friend constexpr MapPosition operator/(MapPosition lhs, MapPosition rhs) {
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
        constexpr MapPosition Rotate(Direction direction) const {
            switch (direction) {
            case Direction::SOUTH: return MapPosition(-x, -y);
            case Direction::WEST: return MapPosition(y, -x);
            case Direction::EAST: return MapPosition(-y, x);
            case Direction::NORTH:
            default:
                return *this;
            }
        }

        constexpr MapPosition Floor() const {
            return MapPosition(std::floor(x), std::floor(y));
        }

        constexpr MapPosition HalfFloor() const {
            return MapPosition(ComputerPlaysFactorio::HalfFloor(x), ComputerPlaysFactorio::HalfFloor(y));
        }

        constexpr MapPosition Ceil() const {
            return MapPosition(std::ceil(x), std::ceil(y));
        }

        constexpr MapPosition HalfCeil() const {
            return MapPosition(ComputerPlaysFactorio::HalfCeil(x), ComputerPlaysFactorio::HalfCeil(y));
        }

        constexpr MapPosition Round() const {
            return MapPosition(std::round(x), std::round(y));
        }

        constexpr MapPosition HalfRound() const {
            return MapPosition(ComputerPlaysFactorio::HalfRound(x), ComputerPlaysFactorio::HalfRound(y));
        }

        constexpr MapPosition Trunc() const {
            return MapPosition(std::trunc(x), std::trunc(y));
        }

        inline double Angle() const {
            return std::atan2(y, x);
        }

        inline Direction ToDirection() const {
            constexpr double f = ((double)Direction::END - 1) / (2 * M_PI);
            constexpr double s = M_PI * 5 / 2 + 2 * M_PI / (double)Direction::END;
            double angle = Angle() + s;

            return (Direction)((int)std::round(angle * f) % (int)Direction::END);
        }

        static constexpr double SqDistance(const MapPosition &lhs, const MapPosition &rhs) {
            auto d = lhs - rhs;
            return d.x*d.x + d.y*d.y;
        }

        static inline double Distance(const MapPosition &lhs, const MapPosition &rhs) {
            return std::sqrt(SqDistance(lhs, rhs));
        }

        static constexpr double Det(const MapPosition &lhs, const MapPosition &rhs) {
            return lhs.x * rhs.y - lhs.y * rhs.x;
        }

        // Returns the intersection point between the line (a1, a2) and (b1, b2)
        static constexpr std::optional<MapPosition> IntersectionPoint(
            const MapPosition &a1, const MapPosition &a2, const MapPosition &b1, const MapPosition &b2
        ) {
            MapPosition a = a2 - a1;
            MapPosition b = b2 - b1;
            double det = Det(a, b);

            if (det == 0) return std::nullopt;

            MapPosition c = b1 - a1;
            double det2 = Det(c, b) / det;
            if (det2 < 0 || det2 > 1) return std::nullopt;

            double det3 = Det(c, a) / det;
            if (det3 < 0 || det3 > 1) return std::nullopt;

            return a1 + a * det2;
        }

        double x;
        double y;
    };

    void to_json(json &j, const MapPosition &pos);
    void from_json(const json &j, MapPosition &pos);

    struct Area {
        constexpr Area() = default;
        constexpr Area(MapPosition left_top_, MapPosition right_bottom_) :
            left_top(left_top_), right_bottom(right_bottom_) {}
        constexpr Area(MapPosition center, double radius) {
            MapPosition shift(radius, radius);
            left_top = center - shift;
            right_bottom = center + shift;
        }
        constexpr Area(double x1, double y1, double x2, double y2) :
            Area(MapPosition(x1, y1), MapPosition(x2, y2)) {}

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

        constexpr MapPosition Center() const {
            return (left_top + right_bottom) / 2;
        }

        // NORTH direction is an angle of 0, EAST is pi/2 clockwise
        // Only supports the north, south, east and west. Others will return a copy.
        constexpr Area Rotate(Direction direction) const {
            const double &x1 = left_top.x, &y1 = left_top.y, &x2 = right_bottom.x, &y2 = right_bottom.y;
            switch (direction) {
            case Direction::SOUTH: return Area(-x2, -y2, -x1, -y1);
            case Direction::WEST: return Area(y1, -x2, y2, -x1);
            case Direction::EAST: return Area(-y2, x1, -y1, x2);
            case Direction::NORTH:
            default:
                return *this;
            }
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

    void to_json(json &j, const Area &area);
    void from_json(const json &j, Area &area);

    // A helper function to iterate through all the points in a circle from the closest
    // to attraction_point to the furthest. callback may return true to break the loop early.
    // finally is called only if loop end is reached but callback never returned true.
    void IterateFromClosestPointCircle(
        const MapPosition &attraction_point,
        bool half_intergers,
        const MapPosition &center,
        double radius,
        std::function<bool(const MapPosition&)> callback,
        std::function<void()> finally = nullptr
    );

    // A helper function to iterate through all the points in a rectangle from the closest
    // to attraction_point to the furthest. callback may return true to break the loop early.
    // finally is called only if loop end is reached but callback never returned true.
    void IterateFromClosestPointArea(
        const MapPosition &attraction_point,
        const MapPosition &step_vector,
        const Area &area,
        std::function<bool(const MapPosition&)> callback,
        std::function<void()> finally = nullptr
    );

    using Path = std::vector<MapPosition>;

    enum class TileType {
        NORMAL,
        WATER
    };

    struct Blueprint;

    class Entity {
    public:
        friend constexpr bool operator==(const Entity &lhs, const Entity &rhs) {
            return lhs.m_type == rhs.m_type &&
                lhs.m_name == rhs.m_name &&
                lhs.m_position == rhs.m_position &&
                lhs.m_direction == rhs.m_direction &&
                lhs.m_mirror == rhs.m_mirror &&
                lhs.m_recipe == rhs.m_recipe &&
                lhs.m_underground_type == rhs.m_underground_type &&
                lhs.m_input_priority == rhs.m_input_priority &&
                lhs.m_resource_amount == rhs.m_resource_amount &&
                lhs.m_output_priority == rhs.m_output_priority;
        }

        inline const auto &GetType() const { return m_type; }
        inline void SetType(const std::string &type) { m_type = type; }

        inline const auto &GetName() const { return m_name; }
        void SetName(const std::string &name);

        inline const auto &GetPosition() const { return m_position; }
        inline void SetPosition(const MapPosition &position) {
            m_position = position;
            UpdateBoundingBox();
        }

        inline const auto &GetDirection() const { return m_direction; }
        inline void SetDirection(Direction direction) {
            m_direction = direction;
            UpdateBoundingBox();
        }

        inline const auto &GetMirror() const { return m_mirror; }
        inline void SetMirror(bool mirror) { m_mirror = mirror; }

        inline const auto &GetRecipe() const { return m_recipe; }
        inline void SetRecipe(const std::string &recipe) { m_recipe = recipe; }

        inline const auto &GetUndergroundType() const { return m_underground_type; }
        inline void SetUndergroundType(const std::string &underground_type) { m_underground_type = underground_type; }

        inline const auto &GetInputPriority() const { return m_input_priority; }
        inline void SetInputPriority(const std::string &input_priority) { m_input_priority = input_priority; }

        inline const auto &GetOutputPriority() const { return m_output_priority; }
        inline void SetOutputPriority(const std::string &output_priority) { m_output_priority = output_priority; }

        inline const auto &GetResourceAmount() const { return m_resource_amount; }
        inline void SetResourceAmount(int resource_amount) { m_resource_amount = resource_amount; }

        inline const auto &GetBoundingBox() const { return m_bounding_box; }
        inline const auto &GetPrototype() const { return *m_prototype; }

    private:
        friend void to_json(json &j, const Entity &e);
        friend void from_json(const json &j, Entity &e);
        friend struct Blueprint;

        void UpdateBoundingBox() {
            const auto &proto = *m_prototype;
            if (proto.contains("collision_box")) {
                m_bounding_box = proto["collision_box"].get<Area>().Rotate(m_direction) + m_position;
            }
        }

        const json *m_prototype = nullptr;
        Area m_bounding_box;

        std::string m_type;
        std::string m_name;
        MapPosition m_position;
        Direction m_direction = Direction::NORTH;
        bool m_mirror = false;

        std::string m_recipe;
        std::string m_underground_type;   // type of underground "input" or "output"
        std::string m_input_priority;     // "left" or "right"
        std::string m_output_priority;    // "left" or "right"
        int m_resource_amount = 0;
    };
    using UEntity = std::unique_ptr<Entity>;
    using SEntity = std::shared_ptr<Entity>;

    void to_json(json &j, const Entity &e);
    void from_json(const json &j, Entity &e);

    struct Blueprint {
        std::vector<Entity> entities;
        MapPosition center;

        void Shift(const MapPosition &vector);
        void Rotate(Direction direction);

        int CountEntityType(const std::string &type) const;
        int CountEntityName(const std::string &name) const;

        // Load should be preferred.
        static Blueprint LoadString(const std::string &str);

        // Decode the blueprint in the file at path data/blueprints/{path}.
        // The blueprints are cached, meaning that if a file is requested multiple times, it will only be loaded once.
        static const Blueprint &Load(const std::filesystem::path &path);
    };
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};