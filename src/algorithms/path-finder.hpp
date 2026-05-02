#pragma once

#include <expected>

#include "../bot/map-data.hpp"

namespace ComputerPlaysFactorio {

    using Path = std::vector<MapPosition>;

    // Finds a path using the A* algorithm
    std::expected<Path, Result> FindPath(
        const MapData&,
        MapData::Branch,
        MapPosition start,
        MapPosition goal,
        double radius
    );
}