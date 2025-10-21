#pragma once

#include <expected>

#include "../bot/map-data.hpp"

namespace ComputerPlaysFactorio {

    // Finds a path using the A* algorithm
    std::expected<Path, Result> FindPath(
        const MapData&,
        MapData::Branch,
        const MapPosition &start,
        const MapPosition &goal,
        double radius
    );
}