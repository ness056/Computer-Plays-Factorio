#pragma once

#include "../factorioAPI/types.hpp"

namespace ComputerPlaysFactorio {

    using PathfinderData = std::unordered_set<MapPosition>;

    // Returns an empty path if no path were found
    Path FindPath(const PathfinderData&, MapPosition start, MapPosition goal, float radius);

    // Finds a chain of paths that starts from start and goes to all points 1 time.
    std::vector<std::tuple<MapPosition, Path>> FindStepPath(
        const PathfinderData&, MapPosition start, std::vector<std::tuple<MapPosition, float>> points);
}