/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

extern "C" {
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

class GPIO {
    public:
    static void init();

    static inline void led_on() {
        GPIOB->BSRR = GPIO_BSRR_BS2; // set PB2 high
    };
    static inline void led_off() {
        GPIOB->BSRR = GPIO_BSRR_BR2; // set PB2 low
    };
    static inline void usb_power_on() {
        GPIOA->BSRR = GPIO_BSRR_BS7; // set PA7 high
    };
    static inline void usb_power_off() {
        GPIOA->BSRR = GPIO_BSRR_BR7; // set PA7 low
    };
    static inline void sd_nss_set() {
        GPIOA->BSRR = GPIO_BSRR_BR4; // set PA4 low
    };
    static inline void sd_nss_reset() {
        GPIOA->BSRR = GPIO_BSRR_BS4; // set PA4 high
    };
};
