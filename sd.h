#pragma once

#include "spi.h"

class SD {
public:
    static bool init();
    static void stream_sectors(uint32_t starting, uint32_t ending);
private:
    static uint8_t send_command(uint8_t cmd, uint32_t arg, uint8_t crc);
    static void cs_set();
    static void cs_reset();
};
