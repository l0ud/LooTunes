/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include "spi.h"

class SD {
public:
    static bool init();
    static uint8_t send_command(uint8_t cmd, uint32_t arg, uint8_t crc);
    static void cs_set();
    static void cs_reset();

private:
};
