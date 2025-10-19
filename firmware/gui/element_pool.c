
#include <stddef.h>
#include "element_pool.h"

struct gui_pool gui_memory_pool;
struct gui_item gui_memory_pool_data[GUI_POOL_NUM_ELEMENTS];

// Function to initialize the memory pool
void gui_init_memory_pool() {
    // Allocate memory for the entire pool
    
    // Create the linked list of items within the pool
    struct gui_item* current = gui_memory_pool_data;
    struct gui_item* prev = NULL;
    for (int i = 0; i < GUI_POOL_NUM_ELEMENTS - 1; ++i) {
        current->prev = prev;
        current->next = (struct gui_item*)((uint8_t*)current + sizeof(struct gui_item));
        prev = current;
        current = current->next;
    }
    current->next = NULL;

    // Set the head of the pool
    gui_memory_pool.head = gui_memory_pool_data;
}

// Function to allocate a item from the memory pool
struct gui_item* gui_allocate_item() {
    if (gui_memory_pool.head == NULL) {
        while(1) {
            // out of memory, freeze
        }
        return NULL;
    }

    struct gui_item* allocated_gui_item = gui_memory_pool.head;
    gui_memory_pool.head = gui_memory_pool.head->next;
    gui_memory_pool.head->prev = NULL;

    allocated_gui_item->next = NULL;
    allocated_gui_item->prev = NULL;
    return allocated_gui_item;
}

// Function to deallocate a item and return it to the memory pool
void gui_deallocate_item(struct gui_item* item) {
    gui_memory_pool.head->prev = item;
    item->prev = NULL;
    item->next = gui_memory_pool.head;
    gui_memory_pool.head = item;
}

void gui_pool_add(struct gui_pool* pool, struct gui_item* new_node) {
    const uint16_t new_node_value = new_node->sort_data.sort_value;
    struct gui_item* head = pool->head;
    if (head == NULL)
    {
        pool->head = new_node;
    }
    else if (new_node_value < head->sort_data.sort_value) {
        // NEW NODE <-> HEAD <-> ...
        new_node->next = head;
        head->prev = new_node;
        pool->head = new_node;
    }
    else {
        struct gui_item* current = head;
        while (current->next != NULL && current->next->sort_data.sort_value <= new_node_value) {
            current = current->next;
        }

        // HEAD <-> CURRENT <-> NEW NODE <-> ...
        new_node->next = current->next;
        new_node->prev = current;

        if (current->next != NULL) {
            current->next->prev = new_node;
        }

        current->next = new_node;
    }
}

void gui_pool_remove(struct gui_pool* pool, struct gui_item* item) {
    struct gui_item* prev_node = item->prev;
    struct gui_item* next_node = item->next;

    if (prev_node == NULL) {
        // If the node to be deleted is the head
        pool->head = next_node;
        if (next_node != NULL) {
            next_node->prev = NULL;
        }
    }
    else {
        prev_node->next = next_node;
        if (next_node != NULL) {
            next_node->prev = prev_node;
        }
    }
    item->prev = item->next = NULL;
}

void gui_pool_move(struct gui_pool *from, struct gui_pool *to, struct gui_item* item)
{
    gui_pool_remove(from, item);
    gui_pool_add(to, item);
}
