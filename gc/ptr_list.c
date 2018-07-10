#include <stdlib.h>
#include <string.h>
#include "ptr_list.h"
#include "util.h"

void ptr_list_init(ptr_list_t* list) {
    list->next = NULL;
}

ptr_list_t* ptr_list_append(ptr_list_t* list, void* ptr) {
    const uint8_t len = ptr_get_data(list->next);
    if (likely(len < 7)) {
        list->elements[len] = ptr;
        list->next = ptr_set_data(list->next, len + 1);
        return list;
    }

    ptr_list_t* new_node = calloc(1, sizeof(ptr_list_t));
    verify(new_node != NULL);
    new_node->elements[0] = ptr;
    new_node->next = (void*)1;
    list->next = ptr_set_data(new_node, len);
    return new_node;
}

void ptr_list_iter_start(ptr_list_iter_t* iter, ptr_list_t* node) {
    iter->node = node;
    iter->element = 0;
}

void* ptr_list_iter_next(ptr_list_iter_t* iter) {
    if (unlikely(iter->element >= PTR_DATA_BITS)) {
        iter->element = 0;
        iter->node = ptr_get_ptr(iter->node->next);
        if (unlikely(iter->node == NULL)) {
            return NULL;
        }
    }

    const uint8_t len = ptr_get_data(iter->node->next);
    if (unlikely(iter->element > len)) {
        return NULL;
    }

    const uint8_t i = iter->element;
    iter->element += 1;
    return iter->node->elements[i];
}
