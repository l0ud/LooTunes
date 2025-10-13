/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

extern "C" {
#include "defines.h"
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

class WDT {
    public:
    static inline void init() {
        // Enable the Independent Watchdog (IWDG)
        IWDG->KR = 0x5555; // Enable write access to IWDG_PR and IWDG_RLR
        while((IWDG->SR & IWDG_SR_PVU) || (IWDG->SR & IWDG_SR_RVU)) {
            // Wait until the prescaler and reload values are updated
        }
        IWDG->PR = 0x0002; // Set prescaler
        IWDG->RLR = 0x333; // Set reload value

        IWDG->KR = 0xAAAA; // Reload the counter with the value defined in the reload register
        IWDG->KR = 0xCCCC; // Start the watchdog
    }

    static inline void feed() {
        IWDG->KR = 0xAAAA; // Feed the watchdog
    };
};
