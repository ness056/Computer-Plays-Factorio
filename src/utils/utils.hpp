#pragma once

#define _USE_MATH_DEFINES
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <future>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#error "Only Windows is supported for now"
#endif

namespace ComputerPlaysFactorio {
    enum Result {
        SUCCESS,
        INVALID_FACTORIO_PATH,
        UNSUPPORTED_FACTORIO_VERSION,
        INSTANCE_ALREADY_RUNNING,
        INSTANCE_NOT_RUNNING,
        RCON_NOT_READY,
        RCON_AUTH_FAILED,
        PATH_FINDER_FAILED
    };

    constexpr std::string Result2String(Result result) {
        switch (result) {
        case SUCCESS: return "SUCCESS";
        case INVALID_FACTORIO_PATH: return "INVALID_FACTORIO_PATH";
        case UNSUPPORTED_FACTORIO_VERSION: return "UNSUPPORTED_FACTORIO_VERSION";
        case INSTANCE_ALREADY_RUNNING: return "INSTANCE_ALREADY_RUNNING";
        case INSTANCE_NOT_RUNNING: return "INSTANCE_NOT_RUNNING";
        case RCON_NOT_READY: return "RCON_NOT_READY";
        case RCON_AUTH_FAILED: return "RCON_AUTH_FAILED";
        case PATH_FINDER_FAILED: return "PATH_FINDER_FAILED";
        }

        throw;
    }

    inline double HalfFloor(double x) {
        return std::floor(x * 2) / 2;
    }

    inline double HalfCeil(double x) {
        return std::ceil(x * 2) / 2;
    }

    inline double HalfRound(double x) {
        return std::round(x * 2) / 2;
    }

    extern const std::chrono::high_resolution_clock::time_point g_start_time;

    std::filesystem::path GetRootPath();
    inline std::filesystem::path GetTempDirectory() { return GetRootPath() / "temp"; }
    void CreateTempDirectory();
    void ClearTempDirectory();
    inline std::filesystem::path GetDataPath() { return GetRootPath() / "data"; }
    inline std::filesystem::path GetModsPath() { return GetDataPath() / "mods"; }
    inline std::filesystem::path GetConfigPath() { return GetDataPath() / "config.json"; }

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
    // Diagonal directions (NORTH_WEST, NORTH_EAST, SOUTH_WEST and SOUTH_EAST) return the first cardinal direction when rotating clockwise
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

        throw std::runtime_error("Invalid direction value.");
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
            constexpr double s = 3 * M_PI_2;
            constexpr double f = M_2_PI * 4;
            double i = (Angle() + s) * f;
            // Angle returns 0 for east but the direction enum starts with north, so we need to rotate by pi/2
            return (Direction)((int)std::round(i) % (int)Direction::END);
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

            if (x == 0 || y == 0) return x + y;
            else return std::sqrt(x * x + y * y);
        }

        constexpr bool Collides(const MapPosition &point) const {
            return Distance(point) == 0;
        }

        constexpr bool Collides(const Area &other) const {
            return left_top.x < other.right_bottom.x &&
                   right_bottom.x > other.left_top.x &&
                   left_top.y < other.right_bottom.y &&
                   right_bottom.y > other.left_top.y;
        }

        // Returns the intersection points between the area and the line [a, b].
        constexpr std::vector<MapPosition> IntersectionPoints(const MapPosition &a, const MapPosition &b) const {
            std::vector<MapPosition> res;
            const auto lt = left_top;
            const auto rb = right_bottom;
            const auto lb = GetLeftBottom();
            const auto rt = GetRightTop();

            const std::array<std::pair<MapPosition, MapPosition>, 4> edges = {{
                {lt, rt},
                {rt, rb},
                {rb, lb},
                {lb, lt}
            }};

            constexpr double EPS = 1e-9;
            for (const auto &edge : edges) {
                if (auto ip = MapPosition::IntersectionPoint(a, b, edge.first, edge.second)) {
                    bool duplicate = false;
                    for (const auto &p : res) {
                        if (MapPosition::SqDistance(p, *ip) <= EPS * EPS) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) res.push_back(*ip);
                }
            }

            return res;
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

    template<class T>
    void WaitAll(const std::vector<std::future<T>> &futures) {
        for (const auto &future : futures) {
            future.wait();
        }
    }
}

template<>
struct std::hash<ComputerPlaysFactorio::MapPosition> {
    size_t operator()(const ComputerPlaysFactorio::MapPosition &position) const {
        return std::hash<double>()(position.x) ^ (std::hash<double>()(position.y) << 1);
    }
};