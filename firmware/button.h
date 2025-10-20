/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>

class BTN {

public:
    enum class ID : uint32_t {
        POWER = 0, // left button, short press
        NEXT_DIR, // left button, long press
        NEXT, // right button, short press
        PREV // right button, long press
    };

    static constexpr uint32_t BUTTON_DEBOUNCE_PERIOD = 50;
    static constexpr uint32_t BUTTON_HOLD_PERIOD = 400;

public:
    static void init();
    static void on_ext_interrupt();
    static void on_short_timer_interrupt();
    static void on_timer_interrupt();
};

void ButtonPressCallback(BTN::ID button_id);
