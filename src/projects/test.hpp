#pragma once

#include "../bot/bot.hpp"

namespace ComputerPlaysFactorio {

    class Test : public Bot {
        REGISTER_BOT("Test", Test);

        void OnReady() override;
        /** OnBiterAttack {
        } */
    };
}