#include "gui/gui.hpp"

using namespace ComputerPlaysFactorio;

int main() {
    PathfinderData data;
    for (double x = -10; x < 10; x++) {
        data.chunk.emplace(x, -10);
        data.chunk.emplace(x, 9);
    }
    for (double y = -9; y < 9; y++) {
        data.chunk.emplace(-10, y);
        data.chunk.emplace(9, y);
    }

    for (double x = -3; x < 3; x++) {
        for (double y = -3; y < 3; y++) {
            data.entity.emplace(x, y);
        }
    }

    for (double y = -11; y < 11; y++) {
        for (double x = -11; x < 11; x++) {
            if (data.chunk.contains({x, y})) {
                std::cout << "c";
            } else if (data.entity.contains({x, y})) {
                std::cout << "e";
            } else {
                std::cout << ".";
            }
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
    
    auto path = FindPath(data, {-5, -5}, {2, 4}, 1);
    for (double y = -11; y < 11; y++) {
        for (double x = -11; x < 11; x++) {
            MapPosition pos(x, y);
            bool waypoint = std::find(path.begin(), path.end(), pos) != path.end();

            if ((data.chunk.contains(pos) || data.entity.contains(pos)) && waypoint) {
                std::cout << "x";
            } else if (waypoint) {
                std::cout << "w";
            } else if (data.chunk.contains(pos)) {
                std::cout << "c";
            } else if (data.entity.contains(pos)) {
                std::cout << "e";
            } else {
                std::cout << ".";
            }
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
    
    std::vector<std::tuple<MapPosition, float>> points = {
        { {4, 4}, 1 },
        { {-1, 7}, 1 },
        { {-5, -3}, 1 }
    };
    auto multiPath = FindMultiPath(data, {-5, -5}, points);
    
    int w = 0;
    std::unordered_map<MapPosition, int> waypoints;
    for (const auto &p : multiPath) {
        for (const auto &waypoint : std::get<Path>(p)) {
            waypoints[waypoint] = w++;
            if (w >= 10) w = 0;
        }
    }

    for (double y = -11; y < 11; y++) {
        for (double x = -11; x < 11; x++) {
            MapPosition pos(x, y);
            bool waypoint = waypoints.contains(pos);

            if ((data.chunk.contains(pos) || data.entity.contains(pos)) && waypoint) {
                std::cout << "x";
            } else if (waypoint) {
                std::cout << waypoints[pos];
            } else if (data.chunk.contains(pos)) {
                std::cout << "c";
            } else if (data.entity.contains(pos)) {
                std::cout << "e";
            } else {
                std::cout << ".";
            }
        }
        std::cout << std::endl;
    }
}

// int main(int argc, char **argv) {
//     QApplication app(argc, argv);
//     MainWindow w;
//     w.show();
//     return app.exec();
// }