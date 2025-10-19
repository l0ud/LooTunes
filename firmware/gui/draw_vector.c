#include <stdint.h>
#include <stddef.h>
#include "element_pool.h"

#include "draw_vector.h"

struct draw_vector draw_vector_inst;

void draw_vector_insert(struct gui_item *item)
{

    // elements with higher zindex need to go to the right!
    // lower zindex - left (drawn first!)

    if (draw_vector_inst.size >= DRAW_VECTOR_NUM_ELEMENTS)
    {
        while (1) {
            // too many elements in list!
        }
    }

    // easy case, adding new item with higher or equal zindex - just append
    if (draw_vector_inst.size == 0 || item->sort_data.data.z_index >= draw_vector_inst.items[draw_vector_inst.size-1]->sort_data.data.z_index)
    {
        draw_vector_inst.items[draw_vector_inst.size++] = item;
        return;
    }

    size_t i=0;
    // element we're adding has lower zindex than last
    // move right while our index is higher than current item
    while (i < draw_vector_inst.size && item->sort_data.data.z_index >= draw_vector_inst.items[i]->sort_data.data.z_index)
    {
        i++;
    }

    // ok, now all elements from i up are supposed to go right by 1
    for (size_t current = draw_vector_inst.size; current>i; current--)
    {
        draw_vector_inst.items[current] = draw_vector_inst.items[current-1];
    }
    
    draw_vector_inst.items[i] = item; // insert our element
    
    draw_vector_inst.size++;

}

void draw_vector_remove(size_t index)
{
    draw_vector_inst.size--;
    for (size_t current = index; current<draw_vector_inst.size; current++)
    {
        draw_vector_inst.items[current] = draw_vector_inst.items[current+1];
    }

}
