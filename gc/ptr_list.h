#ifndef __VIXENO_PTR_LIST_H__
#define __VIXENO_PTR_LIST_H__

#include <stdint.h>

// Unrolled linked list of pointers

typedef struct ptr_list {
    struct ptr_list* next;
    void* elements[7];
} ptr_list_t;

typedef struct {
    ptr_list_t* node;
    uint8_t element;
} ptr_list_iter_t;

void ptr_list_init(ptr_list_t*);
ptr_list_t* ptr_list_append(ptr_list_t*, void*);
void ptr_list_iter_start(ptr_list_iter_t*, ptr_list_t*);
void* ptr_list_iter_next(ptr_list_iter_t*);

#endif
