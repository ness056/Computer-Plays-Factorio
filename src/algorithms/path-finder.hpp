#pragma once

#include <expected>

#include "../bot/map-data.hpp"

namespace ComputerPlaysFactorio {

    // Finds a path using the A* algorithm
    std::expected<Path, Result> FindPath(
        const MapData&,
        const MapPosition &start,
        const MapPosition &goal,
        double radius,
        const std::unordered_set<MapPosition> *additional_colliders = nullptr
    );
}