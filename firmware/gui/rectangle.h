#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "element_pool.h"

struct gui_rectangle_data {
    uint8_t pos_y;
    uint8_t width;
    uint8_t height;
    uint16_t color;
};

bool gui_rectangle_draw(struct gui_item* data, uint16_t *buffer, uint8_t x);
struct gui_item* gui_make_rectangle(uint8_t posX, uint8_t posY, uint8_t z_index, uint8_t width, uint8_t height, uint16_t color);
