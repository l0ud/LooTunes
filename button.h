#pragma once

#include <cstdint>

class BTN {

public:
    enum class ID : uint32_t {
        POWER = 0, // left button, short press
        TODO, // left button, long press
        NEXT, // right button, short press
        PREV // right button, long press
    };

    static constexpr uint32_t BUTTON_DEBOUNCE_PERIOD = 50;
    static constexpr uint32_t BUTTON_HOLD_PERIOD = 400;

public:
    static void init();
    static void onExtInterrupt();
    static void onShortTimerInterrupt();
    static void onTimerInterrupt();
};

void ButtonPressCallback(BTN::ID button_id);
