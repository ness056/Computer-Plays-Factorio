#include "test.hpp"

namespace ComputerPlaysFactorio {
    void Test::OnReady() {
        PickUpCrashSiteItems();

        BuildBurnerCity(15, 6, 20, 4);
    }
}