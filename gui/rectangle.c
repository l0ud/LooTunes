#include "rectangle.h"
#include <string.h>


bool gui_rectangle_draw(struct gui_item* data, uint16_t *buffer, uint8_t x)
{
    struct gui_rectangle_data *this = (struct gui_rectangle_data *)data->raw_data;
    if (x > data->sort_data.data.pos_x + this->width)
    {
        return false;
    }

    if (x >= data->sort_data.data.pos_x) {
        for (uint8_t i=this->pos_y; i<this->height + this->pos_y; i++) {
            buffer[i] = this->color;
        }
    }

    return true;
}

struct gui_item* gui_make_rectangle(uint8_t posX, uint8_t posY, uint8_t z_index, uint8_t width, uint8_t height, uint16_t color)
{
    struct gui_item* this = gui_allocate_item();
    struct gui_rectangle_data* data = (struct gui_rectangle_data*)this->raw_data;
    this->type = 0;
    this->sort_data.data.z_index = z_index;
    this->sort_data.data.pos_x = posX;
    data->pos_y = posY;
    data->width = width;
    data->height = height;
    data->color = color;
    return this;
}
