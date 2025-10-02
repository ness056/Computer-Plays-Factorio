#pragma once

#include "../factorio-API/types.hpp"

namespace ComputerPlaysFactorio {
    template <class T>
    double TSPTotalLength(
        const std::vector<T> &points,
        std::function<const MapPosition&(const T&)> position_getter,
        bool fixed_end_point
    ) {
        size_t n = points.size();
        double length = fixed_end_point ? 0 : MapPosition::Distance(
            position_getter(points[0]),
            position_getter(points[n - 1])
        );

        for (int i = 0; i < n - 1; i++) {
            length += MapPosition::Distance(position_getter(points[i]), position_getter(points[i + 1]));
        }
        return length;
    }

    template <class T>
    void TSPSwap(
        std::vector<T> &points,
        std::function<const MapPosition&(const T&)> position_getter,
        int i, int j
    ) {
        i++;
        while (i < j) {
            auto temp = std::move(points[i]);
            points[i] = std::move(points[j]);
            points[j] = std::move(temp);
            i++;
            j--;
        }
    }

    // The first point is fixed, meaning that it is guaranteed that it will stay the same when the function returns.
    // The last point may also be fixed.
    template <class T>
    void TSP(
        std::vector<T> &points,
        std::function<const MapPosition&(const T&)> position_getter,
        bool fixed_end_point
    ) {
        DEBUG2(TSPTotalLength(points, position_getter, fixed_end_point));

        size_t n = points.size();
        bool found_improvement = true;
        while (found_improvement) {
            found_improvement = false;

            for (int i = 0; i < n - 1; i++) {
                const MapPosition &i_pos = position_getter(points[i]);
                const MapPosition &i2_pos = position_getter(points[i + 1]);
                for (int j = i + 2; j < n; j++) {
                    const MapPosition &j_pos = position_getter(points[j]);
                    const MapPosition &j2_pos = position_getter(points[(j + 1) % n]);
                    double length_delta =
                        -MapPosition::Distance(i_pos, i2_pos) - MapPosition::Distance(j_pos, j2_pos) +
                        MapPosition::Distance(i_pos, j_pos) + MapPosition::Distance(i2_pos, j2_pos);

                    if (length_delta < -0.1) {
                        TSPSwap<T>(points, position_getter, i, j);
                        found_improvement = true;
                    }
                }
            }
            DEBUG2(TSPTotalLength(points, position_getter, fixed_end_point));
        }

        DEBUG2(TSPTotalLength(points, position_getter, fixed_end_point));
    }
}