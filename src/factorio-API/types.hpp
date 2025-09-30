#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

#include "../utils/base64.h"
#include <zlib.h>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

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
        constexpr MapPosition Rotate(Direction direction) const {
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

        constexpr MapPosition HalfRound() const {
            return MapPosition(ComputerPlaysFactorio::HalfRound(x), ComputerPlaysFactorio::HalfRound(y));
        }

        double Angle() const {
            return std::atan2(y, x);
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

        // NORTH direction is an angle of 0, EAST is pi/2 clockwise
        // Only supports the north, south, east and west. Others will return a copy.
        constexpr Area Rotate(Direction direction) const {
            return Area(left_top.Rotate(direction), right_bottom.Rotate(direction));
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

    using Path = std::vector<MapPosition>;

    enum class TileType {
        NORMAL,
        WATER
    };

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
                lhs.m_output_priority == rhs.m_output_priority;
        }

        inline const auto &GetType() const { return m_type; }
        inline void SetType(const std::string &type_) { m_type = type_; }

        inline const auto &GetName() const { return m_name; }
        void SetName(const std::string &name_);

        inline const auto &GetPosition() const { return m_position; }
        inline void SetPosition(const MapPosition &position_) {
            m_position = position_;
            UpdateBoundingBox();
        }

        inline const auto &GetDirection() const { return m_direction; }
        inline void SetDirection(Direction direction_) {
            m_direction = direction_;
            UpdateBoundingBox();
        }

        inline const auto &GetMirror() const { return m_mirror; }
        inline void SetMirror(bool mirror_) { m_mirror = mirror_; }

        inline const auto &GetRecipe() const { return m_recipe; }
        inline void SetRecipe(const std::string &recipe_) { m_recipe = recipe_; }

        inline const auto &GetUndergroundType() const { return m_underground_type; }
        inline void SetUndergroundType(const std::string &underground_type_) { m_underground_type = underground_type_; }

        inline const auto &GetInputPriority() const { return m_input_priority; }
        inline void SetInputPriority(const std::string &input_priority_) { m_input_priority = input_priority_; }

        inline const auto &GetOutputPriority() const { return m_output_priority; }
        inline void SetOutputPriority(const std::string &output_priority_) { m_output_priority = output_priority_; }

        inline const auto &GetBoundingBox() const { return m_bounding_box; }
        inline const auto &GetPrototype() const { return *m_prototype; }

    private:
        friend void to_json(json &j, const Entity &e);
        friend void from_json(const json &j, Entity &e);

        void UpdateBoundingBox() {
            m_bounding_box = (*m_prototype)["collision_box"].get<Area>().Rotate(m_direction) + m_position;
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
    };
    using UEntity = std::unique_ptr<Entity>;
    using SEntity = std::shared_ptr<Entity>;

    void to_json(json &j, const Entity &e);
    void from_json(const json &j, Entity &e);

    struct Blueprint {
        std::vector<Entity> entities;
        MapPosition center;
    };

    // returns the json of the blueprint as described here: https://wiki.factorio.com/Blueprint_string_format.
    // In addition the entity objects have the additional field prototype_type that contains the entity type.
    Blueprint DecodeBlueprintStr(const std::string &str);

    Blueprint DecodeBlueprintFile(const std::filesystem::path &path);
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};