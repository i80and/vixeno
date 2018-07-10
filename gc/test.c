#include <stdbool.h>
#include "ptr_list.h"
#include "gc.h"
#include "util.h"

void test_ptr_list(void) {
    ptr_list_t start;
    ptr_list_t* end = &start;
    ptr_list_init(&start);

    for(uint64_t i = 1; i < 100; i += 1) {
        end = ptr_list_append(end, (void*)i);
    }

    ptr_list_iter_t iter;
    ptr_list_iter_start(&iter, &start);

    void* el;
    uint64_t max_seen = 0;
    while((el = ptr_list_iter_next(&iter)) != NULL) {
        max_seen = (uint64_t)el;
    }
    verify((uintptr_t)max_seen == 99);
}

int test_gc(void) {
    gc_ctx gc;
    gc_ctx_init(&gc);
    for (int i = 0; i < 10000; i += 1) {
        int* ptr = gc_alloc(&gc, sizeof(int), false);
        *ptr = 10;
    }

    gc_cycle(&gc);
    return 0;
}

int main(void) {
    // test_ptr_list();
    // gc_test_bitmap();
    test_gc();
    return 0;
}

