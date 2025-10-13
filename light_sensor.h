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

class LIGHT {
    public:
    static void init();
    static void set_thresholds(uint16_t low, uint16_t high);
    static void start();
    static void stop();
    
};

void LightSensorCallback(uint16_t value);
