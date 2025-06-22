#include "pathFinder.hpp"

#include <algorithm>
#include <iostream>

namespace ComputerPlaysFactorio {
    
    struct Node {
        Node(MapPosition pos_, Node *parent_ = nullptr) : pos(pos_), parent(parent_) {}
        inline uint32_t Score() { return G + H; }

        uint32_t G = 0, H = 0;
        MapPosition pos;
        Node *parent;
        Node *child = nullptr;
    };

    using NodeSet = std::vector<Node*>;

    static bool DetectCollision(const PathfinderData &data, MapPosition pos) {
        if (data.entity.contains(pos) || data.chunk.contains(pos)) return true;
        return false;
    }

    static Node *FindNode(const NodeSet &nodes, MapPosition pos) {
        for (auto node : nodes) {
            if (node->pos == pos) return node;
        }
        return nullptr;
    }

    static void ReleaseNodes(NodeSet &nodes) {
        for (auto node : nodes) {
            delete node;
        }
    }

    const size_t c_directionsCount = 8;
    const MapPosition c_directions[8] = {
        { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 },
        { -1, -1 }, { 1, 1 }, { -1, 1 }, { 1, -1 }
    };

    static float SqDistance(MapPosition p1, MapPosition p2) {
        MapPosition delta = (p1 - p2).Abs();
        return delta.x * delta.x + delta.y * delta.y;
    }

    // Octagonal heuristic
    static uint32_t Heuristic(MapPosition p1, MapPosition p2) {
        MapPosition delta = (p1 - p2).Abs();
        return 10 * (delta.x + delta.y) - 6 * std::min(delta.x, delta.y);
    }

    std::vector<std::tuple<MapPosition, Path>> FindMultiPath(
        const PathfinderData& data, MapPosition start, std::vector<std::tuple<MapPosition, float>> points) {

        for (auto &point : points) {
            auto &radius = std::get<float>(point);
            radius *= radius;
        }

        MapPosition currentStartingPos = start;
        std::vector<std::tuple<MapPosition, Path>> pathChain;
        while (!points.empty()) {
            Node *current = nullptr;
            NodeSet openSet, closedSet;
            openSet.reserve(100);
            closedSet.reserve(100);
            openSet.push_back(new Node(currentStartingPos));

            auto point_it = points.end();
            while (true) {
                if (openSet.empty()) break;

                auto current_it = openSet.begin();
                current = *current_it;

                for (auto it = openSet.begin(); it != openSet.end(); it++) {
                    auto node = *it;
                    if (node->Score() <= current->Score()) {
                        current = node;
                        current_it = it;
                    }
                }

                bool pointReached = false;
                for (auto it = points.begin(); it != points.end(); it++) {
                    if (SqDistance(current->pos, std::get<MapPosition>(*it)) < std::get<float>(*it)) {
                        currentStartingPos = current->pos;
                        point_it = it;
                        pointReached = true;
                    }
                }
                if (pointReached) break;

                closedSet.push_back(current);
                openSet.erase(current_it);

                for (size_t i = 0; i < c_directionsCount; i++) {
                    MapPosition newPos(current->pos + c_directions[i]);
                    if (DetectCollision(data, newPos) || FindNode(closedSet, newPos)) {
                        continue;
                    }

                    uint totalCost = current->G + ((i < 4) ? 10 : 14);

                    Node *successor = FindNode(openSet, newPos);
                    if (successor == nullptr) {
                        successor = new Node(newPos, current);
                        successor->G = totalCost;
                        successor->H = 0;
                        openSet.push_back(successor);
                    }
                    else if (totalCost < successor->G) {
                        successor->parent = current;
                        successor->G = totalCost;
                    }
                }
            }

            if (point_it == points.end()) break;

            auto &tuple = pathChain.emplace_back();
            auto &path = std::get<Path>(tuple);

            size_t nbWaypoints = 1;
            while (current->parent != nullptr) {
                Node *previous = current;
                current = current->parent;
                current->child = previous;
                nbWaypoints++;
            }

            path.reserve(nbWaypoints);
            std::cout << "Point: " << std::get<MapPosition>(*point_it).ToString() << std::endl;
            while (current != nullptr) {
                std::cout << current->pos.ToString() << std::endl;
                path.emplace_back(current->pos);
                current = current->child;
            }
            
            ReleaseNodes(openSet);
            ReleaseNodes(closedSet);
            points.erase(point_it);
        }

        return pathChain;
    }

    Path FindPath(const PathfinderData &data, MapPosition start, MapPosition goal, float radius) {
        const float SqRadius = radius * radius;
        Node *current = nullptr;
        NodeSet openSet, closedSet;
        openSet.reserve(100);
        closedSet.reserve(100);
        openSet.push_back(new Node(start));

        bool pathFound = false;
        while (true) {
            if (openSet.empty()) break;

            auto current_it = openSet.begin();
            current = *current_it;

            for (auto it = openSet.begin(); it != openSet.end(); it++) {
                auto node = *it;
                if (node->Score() <= current->Score()) {
                    current = node;
                    current_it = it;
                }
            }

            if (SqDistance(current->pos, goal) < SqRadius) {
                pathFound = true;
                break;
            }

            closedSet.push_back(current);
            openSet.erase(current_it);

            for (size_t i = 0; i < c_directionsCount; i++) {
                MapPosition newPos(current->pos + c_directions[i]);
                if (DetectCollision(data, newPos) || FindNode(closedSet, newPos)) {
                    continue;
                }

                uint totalCost = current->G + ((i < 4) ? 10 : 14);

                Node *successor = FindNode(openSet, newPos);
                if (successor == nullptr) {
                    successor = new Node(newPos, current);
                    successor->G = totalCost;
                    successor->H = Heuristic(successor->pos, goal);
                    openSet.push_back(successor);
                }
                else if (totalCost < successor->G) {
                    successor->parent = current;
                    successor->G = totalCost;
                }
            }
        }

        Path path;
        if (pathFound) {
            size_t nbWaypoints = 1;
            while (current->parent != nullptr) {
                Node *previous = current;
                current = current->parent;
                current->child = previous;
                nbWaypoints++;
            }

            path.reserve(nbWaypoints);
            while (current != nullptr) {
                path.emplace_back(current->pos);
                current = current->child;
            }
        }

        ReleaseNodes(openSet);
        ReleaseNodes(closedSet);

        return path;
    }
}