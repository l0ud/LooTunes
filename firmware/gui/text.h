#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "element_pool.h"

struct gui_text_data {
    uint8_t pos_y;
    uint8_t scale;
    uint16_t color;
    const char* text;
};

bool gui_draw_text(struct gui_item* data, uint16_t *buffer, uint8_t x);
struct gui_item* gui_make_text(uint8_t posX, uint8_t posY, uint8_t z_index, uint16_t color, const char* text, uint8_t scale);
