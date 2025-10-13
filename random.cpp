/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "random.h"

#include "light_sensor.h"

extern "C" {
#include "defines.h"
#include "py32f0xx.h"
#include "py32f0xx_hal.h"
}

namespace RAND {
    volatile static uint32_t state = 0;

    void init() {
        __HAL_RCC_TIM14_CLK_ENABLE();
        TIM14->CR1 = TIM_CR1_CEN; // enable timer14

        __HAL_RCC_CRC_CLK_ENABLE();
    }

    uint32_t next() {
        // use timer14 counter as entropy source
        state ^= TIM14->CNT;
        // mix it with CRC hardware
        CRC->DR = state;
        return CRC->DR;
    }
}

