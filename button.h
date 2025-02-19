#pragma once

#include <cstdint>

class Button {

    static constexpr uint32_t BUTTON_DEBOUNCE_PERIOD = 50;
    static constexpr uint32_t BUTTON_HOLD_PERIOD = 400;

public:
    Button() : _pressed(false) { };
    void init();

    void onExtInterrupt();
    void onShortTimerInterrupt();
    void onTimerInterrupt();

private:
    bool _pressed;
};

void ShortButtonPressCallback();
void LongButtonPressCallback();

extern Button BTN;