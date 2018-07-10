// This GC implementation follows http://wiki.luajit.org/New-Garbage-Collector
// to the best of my ability.

#ifndef __VIXENO_GC_H__
#define __VIXENO_GC_H__

#include <stddef.h>
#include <stdbool.h>

#define CELL_SIZE (16)
typedef uint16_t cell_index_t;
typedef struct {
    uint8_t data[CELL_SIZE];
} cell_t;

#define ARENA_SIZE_PARAMETER    20
_Static_assert(ARENA_SIZE_PARAMETER >= 16 && ARENA_SIZE_PARAMETER <= 20,
               "ARENA_SIZE_PARAMETER must be between 16 and 20 inclusive");

#define ARENA_SIZE_MASK         ((1 << (ARENA_SIZE_PARAMETER)) - 1)
#define ARENA_SIZE              (1 << (ARENA_SIZE_PARAMETER))
#define METADATA_SIZE           ((ARENA_SIZE) / 64)
#define BITMAP_SIZE             (((METADATA_SIZE) / 2 / sizeof(uint64_t)) - 2)
#define ARENA_N_CELLS           (((ARENA_SIZE) - (METADATA_SIZE)) / CELL_SIZE)
#define MAX_ARENA_ALLOCATION_THRESHOLD (((ARENA_SIZE) - (METADATA_SIZE)) / 4)

#define N_BINS 10

typedef struct {
    char traversal_function[128];
} class_t;

// Arena bitmap structure
typedef struct {
    union {
        cell_index_t* gray_stack;
        cell_index_t frontier;
    } metadata;

    char _padding[8];

    uint64_t arr[BITMAP_SIZE];
} arena_bitmap_t;

typedef struct {
    arena_bitmap_t block_bitmap;
    arena_bitmap_t mark_bitmap;
    cell_t cells[ARENA_N_CELLS];
} arena_t;
_Static_assert(sizeof(arena_t) == ARENA_SIZE, "Arena size seems weird");

typedef struct {
    ptr_list_t arenas;
    ptr_list_t* last_arena_list;
    arena_t* last_arena;
} arena_list_t;

typedef struct {
    uint32_t fragmentation;

    arena_list_t traversable_arenas;
    arena_list_t primitive_arenas;

    void* gray_queue;
    ptr_list_t bins[N_BINS];
} gc_ctx;

void gc_ctx_init(gc_ctx*);
void* gc_alloc(gc_ctx*, size_t, bool);
void gc_cycle(gc_ctx*);

#ifdef VIXENO_TEST
void gc_test_bitmap(void);
#endif

#endif
