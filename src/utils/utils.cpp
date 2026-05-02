#include "utils.hpp"

#include <queue>
#include <unordered_set>

#include "logging.hpp"

namespace ComputerPlaysFactorio {

    const std::chrono::high_resolution_clock::time_point g_start_time =
        std::chrono::high_resolution_clock::now();

    std::filesystem::path GetRootPath() {
        char path[MAX_PATH + 1];
        GetModuleFileNameA(nullptr, path, MAX_PATH + 1);
        const auto root_path = std::filesystem::canonical(std::filesystem::path(path) / "../..");

        return root_path;
    }

    static HANDLE lock = INVALID_HANDLE_VALUE;

    void CreateTempDirectory() {
        auto path = GetTempDirectory();
        const auto lock_path = path / ".lock";

        if (!std::filesystem::exists(path)) std::filesystem::create_directory(path);

        if (lock == INVALID_HANDLE_VALUE) {
            lock = CreateFileA(lock_path.string().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (lock == INVALID_HANDLE_VALUE) {
                throw RuntimeErrorF("Failed to create lock. Make sure that no other instance of Computer Plays Factorio is running.");
            }
        }
    }

    static void RemoveAllTempDir() {
        const auto path = GetRootPath() / "temp";

        for (const auto &entry : std::filesystem::directory_iterator(path)) {
            std::filesystem::remove_all(entry.path());
        }
    }

    void ClearTempDirectory() {
        if (lock != INVALID_HANDLE_VALUE) {
            CloseHandle(lock);
            lock = INVALID_HANDLE_VALUE;
            RemoveAllTempDir();
        }
    }

    void ForEachCardinal(std::function<void(Direction)> func) {
        func(Direction::NORTH);
        func(Direction::EAST);
        func(Direction::SOUTH);
        func(Direction::WEST);
    }

    void ForEachDiagonal(std::function<void(Direction)> func) {
        func(Direction::NORTH_EAST);
        func(Direction::SOUTH_EAST);
        func(Direction::SOUTH_WEST);
        func(Direction::NORTH_WEST);
    }

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

    void IterateFromClosestPointCircle(
        const MapPosition &attraction_point,
        bool half_intergers,
        const MapPosition &center,
        double radius,
        std::function<bool(const MapPosition&)> callback,
        std::function<void()> finally
    ) {
        const double sq_radius = radius * radius;
        const double d = half_intergers ? 0.5 : 1.;

        const auto comp2 = [&attraction_point](const MapPosition &lhs, const MapPosition &rhs) {
            return MapPosition::SqDistance(attraction_point, lhs) > MapPosition::SqDistance(attraction_point, rhs);
        };
        std::priority_queue<MapPosition, std::vector<MapPosition>, decltype(comp2)> points(comp2);
        std::unordered_set<MapPosition> visited;

        MapPosition pos = attraction_point;
        if (MapPosition::SqDistance(pos, center) > sq_radius) {
            double angle = (pos - center).Angle();
            pos.x = center.x + radius * std::cos(angle);
            pos.y = center.y + radius * std::sin(angle);
        }
        if (half_intergers) pos = pos.HalfRound();
        else pos = pos.Round();

        if (MapPosition::SqDistance(pos, center) <= sq_radius && callback(pos)) return;
        while (true) {
            for (double dx = -d; dx <= d; dx += d) {
                for (double dy = -d; dy <= d; dy += d) {
                    if (dx == 0 && dy == 0) continue;
                    MapPosition neighbor = pos + MapPosition(dx, dy);
                    if (
                        !visited.contains(neighbor) &&
                        MapPosition::SqDistance(neighbor, center) <= sq_radius
                    ) {
                        visited.emplace(neighbor);
                        points.emplace(neighbor);
                    }
                }
            }

            if (points.empty()) break;
            pos = points.top();
            points.pop();
            
            if (callback(pos)) return;
        };

        if (finally) finally();
    }

    void IterateFromClosestPointArea(
        const MapPosition &attraction_point,
        const MapPosition &step_vector,
        const Area &area,
        std::function<bool(const MapPosition&)> callback,
        std::function<void()> finally
    ) {
        assert(step_vector.HalfRound() == step_vector);

        bool half_intergers = step_vector.Round() != step_vector;
        std::array<MapPosition, 4> vectors = {
            step_vector,
            step_vector.Rotate(Direction::EAST),
            step_vector.Rotate(Direction::SOUTH),
            step_vector.Rotate(Direction::WEST),
        };

        const auto comp2 = [&attraction_point](const MapPosition &lhs, const MapPosition &rhs) {
            return MapPosition::SqDistance(attraction_point, lhs) > MapPosition::SqDistance(attraction_point, rhs);
        };
        std::priority_queue<MapPosition, std::vector<MapPosition>, decltype(comp2)> points(comp2);
        std::unordered_set<MapPosition> visited;

        MapPosition pos = attraction_point;
        if (!area.Collides(pos)) {
            const MapPosition center = area.Center();
            const auto intersection_points = area.IntersectionPoints(center, pos);
            assert(intersection_points.size() == 1);

            pos = intersection_points[0];
        }
        if (half_intergers) pos = pos.HalfRound();
        else pos = pos.Round();

        if (area.Collides(pos) && callback(pos)) return;
        while (true) {
            for (const auto &vec : vectors) {
                MapPosition neighbor = pos + vec;
                if (!visited.contains(neighbor) && area.Collides(neighbor)) {
                    visited.emplace(neighbor);
                    points.emplace(neighbor);
                }
            }

            if (points.empty()) break;
            pos = points.top();
            points.pop();
            
            if (callback(pos)) return;
        };

        if (finally) finally();
    }
}