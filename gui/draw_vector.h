#pragma once

#define DRAW_VECTOR_NUM_ELEMENTS 10

struct draw_vector
{
    uint8_t size;
    struct gui_item* items[DRAW_VECTOR_NUM_ELEMENTS];
};

extern struct draw_vector draw_vector_inst;
void draw_vector_insert(struct gui_item *item);
void draw_vector_remove(size_t index);