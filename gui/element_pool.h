#pragma once

#include <stdint.h>

#define GUI_POOL_NUM_ELEMENTS 10
#define GUI_ELEMENT_DATA_SIZE 100

// Structure for the memory pool
struct gui_pool {
    struct gui_item* head;
};

// Structure for a item
struct gui_item {
    struct gui_item* prev;
    struct gui_item* next;
    union {
        struct  {
            uint8_t z_index;
            uint8_t pos_x;
        } data;

        uint16_t sort_value;
    } sort_data;

    uint32_t type;
    uint8_t raw_data[GUI_ELEMENT_DATA_SIZE];
};

void gui_init_memory_pool();
struct gui_item* gui_allocate_item();
void gui_deallocate_item(struct gui_item* item);

void gui_pool_add(struct gui_pool* pool, struct gui_item* item);
void gui_pool_remove(struct gui_pool* pool, struct gui_item* item);
void gui_pool_move(struct gui_pool *from, struct gui_pool *to, struct gui_item* item);
