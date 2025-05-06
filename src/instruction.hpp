#pragma once

#include <string>
#include <cstdint>

#include "serializer.hpp"

namespace ComputerPlaysFactorio {
    
    /**
     * Instructions are a special type requests that is used to control the player's character.
     * When an instruction is created using QueueSomeInstruction(...), it wont be executed immediately,
     *  but is instead added into a queue and will be sent when all the previous instructions are done.
     */
    struct Instruction {
        std::string name;

        double x, y;
        uint64_t amount;
        std::string str, item;
        uint8_t inventory, orientation;
        // bit 0: Force, 1: Super force
        uint8_t other;

        QUICK_AUTO_NAMED_PROPERTIES(Instruction, x, y, amount, str, item, inventory, orientation, other)
    };

    struct WalkResponse {

    };

    struct CraftResponse {

    };

    struct CancelResponse {

    };

    struct TechResponse {

    };

    struct PickupResponse {

    };

    struct DropResponse {

    };

    struct MineResponse {

    };

    struct ShootResponse {

    };

    struct UseResponse {

    };

    struct EquipResponse {

    };

    struct TakeResponse {

    };

    struct PutResponse {

    };

    struct BuildResponse {

    };

    struct RecipeResponse {

    };

    struct LimitResponse {

    };

    struct FilterResponse {

    };

    struct PriorityResponse {

    };

    struct LaunchResponse {

    };

    struct RotateResponse {

    };

}