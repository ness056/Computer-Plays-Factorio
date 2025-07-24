#pragma once

#include <unordered_set>
#include <tuple>

#include "../factorioAPI/types.hpp"

namespace ComputerPlaysFactorio {

    struct PathfinderData {
        std::unordered_set<MapPosition> entity;
        std::unordered_set<MapPosition> chunk;
    };

    // Returns an empty path if no path were found
    Path FindPath(const PathfinderData&, MapPosition start, MapPosition goal, double radius);

    /**
     * Finds a chain of paths that starts from start and
     * goes through all the points and ends at one of them.
     * One usage of this function can be to build a blueprint:
     *  - You first find a list of points where all the entities
     *  of the blueprint are in range of at least 1 point.
     *  - Then you generate a path that goes through all those points.
     *  - By construction every entities in the blueprint must be in
     *  building range at least once when following that path.
     * 
     * Be careful with this function, it becomes really slow
     * compared to FindPath if the points are too far from each others.
     */
    std::vector<std::tuple<MapPosition, Path>> FindMultiPath(
        const PathfinderData&, MapPosition start, std::vector<std::tuple<MapPosition, double>> points);
}