#include "path-finder.hpp"

#include <algorithm>
#include <queue>

namespace ComputerPlaysFactorio {
    struct Node {
        Node(MapPosition pos_, std::shared_ptr<Node> parent_ = nullptr) : pos(pos_), parent(parent_) {}

        double g = 0, t = 0;    // t = g + heuristic
        MapPosition pos;
        std::shared_ptr<Node> parent;
        std::shared_ptr<Node> child = nullptr;
    };
    
    struct NodeComp {
        bool operator()(const std::shared_ptr<Node> &lhs, const std::shared_ptr<Node> &rhs) const {
            return lhs->t > rhs->t;
        }
    };

    const size_t c_directions_count = 8;
    const MapPosition c_directions[8] = {
        { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
        { -1, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }
    };

    // Octagonal heuristic
    static double Heuristic(MapPosition p1, MapPosition p2) {
        MapPosition delta = (p1 - p2).Abs();
        return (delta.x + delta.y) - 0.6 * std::min(delta.x, delta.y);
    }

    std::expected<Path, Result> FindPath(
        const MapData &data,
        const MapPosition &start,
        const MapPosition &goal,
        double radius,
        const std::unordered_set<MapPosition> *additional_colliders
    ) {
        const double sq_radius = radius * radius;
        std::shared_ptr<Node> current = nullptr;
        std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, NodeComp> open_queue;
        std::unordered_set<MapPosition> closed_set;
        open_queue.push(std::make_shared<Node>(start));

        while (true) {
            if (open_queue.empty()) return std::unexpected(Result::PATH_FINDER_FAILED);

            current = open_queue.top();
            open_queue.pop();

            if (MapPosition::SqDistance(current->pos, goal) <= sq_radius) break;

            if (closed_set.contains(current->pos)) continue;
            closed_set.insert(current->pos);

            for (size_t i = 0; i < c_directions_count; i++) {
                MapPosition new_pos(current->pos + c_directions[i] * 0.5);
                if (data.PathfinderCollides(new_pos) || closed_set.contains(new_pos) || 
                    (additional_colliders && additional_colliders->contains(new_pos))
                ) {
                    continue;
                }

                double new_g = current->g + ((i < 4) ? 1 : M_SQRT2);

                auto successor = std::make_shared<Node>(new_pos, current);
                successor->g = new_g;
                successor->t = new_g + Heuristic(successor->pos, goal);
                open_queue.push(successor);
            }
        }

        size_t nb_waypoints = 1;
        while (current->parent) {
            std::shared_ptr<Node> previous = current;
            current = current->parent;
            current->child = previous;
            nb_waypoints++;
        }

        Path path;
        path.reserve(nb_waypoints);
        while (current) {
            path.emplace_back(current->pos);
            current = current->child;
        }

        return path;
    }
}