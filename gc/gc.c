#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "ptr_list.h"
#include "gc.h"

const int BIN_SIZES[N_BINS] = {0, 2, 4, 6, 8, 16, 32, 64, 128, 256};

static inline void arena_bitmap_set(arena_bitmap_t* bitmap, cell_index_t index) {
    bitmap->arr[(index / 64)] |= (1ULL << (index % 64));
}

static void arena_bitmap_set_range(arena_bitmap_t* bitmap, cell_index_t start_bit, cell_index_t len) {
    // set the start cell
    uint16_t start_cell = start_bit / 64;
    uint16_t start_bit_within_cell = start_bit % 64;
    uint16_t start_bits_to_set = min(len, 63);
    bitmap->arr[start_cell] |= (((1ULL << start_bits_to_set) - 1) << start_bit_within_cell);

    uint16_t end_cell = (start_bit + len) / 64;
    uint16_t n_cells = end_cell - start_cell;
    if (likely(n_cells > 1)) {
        // set the end cell
        uint16_t bits_to_preserve = 64 - (len % 64);
        uint16_t bits_to_fill = 64 - bits_to_preserve;
        bitmap->arr[end_cell] |= (1ULL << bits_to_fill) - 1;

        // memset anything in the interior chunks
        if (n_cells > 2) {
            memset(bitmap->arr + start_cell + 1, 0xFF, (n_cells - 1) * sizeof(uint64_t));
        }
    }
}

static inline void arena_bitmap_clear(arena_bitmap_t* bitmap, cell_index_t index) {
    bitmap->arr[(index / 64)] &= ~(1ULL << (index % 64));
}

static void arena_bitmap_clear_range(arena_bitmap_t* bitmap, cell_index_t start_bit, cell_index_t len) {
    // set the start cell
    uint16_t start_cell = start_bit / 64;
    uint16_t start_bit_within_cell = start_bit % 64;
    uint16_t start_bits_to_set = min(len, 63);
    bitmap->arr[start_cell] &= ~(((1ULL << start_bits_to_set) - 1) << start_bit_within_cell);

    uint16_t end_cell = (start_bit + len) / 64;
    uint16_t n_cells = end_cell - start_cell;
    if (likely(n_cells > 1)) {
        // set the end cell
        uint16_t bits_to_preserve = 64 - (len % 64);
        uint16_t bits_to_fill = 64 - bits_to_preserve;
        bitmap->arr[end_cell] >>= bits_to_fill;

        // memset anything in the interior chunks
        if (n_cells > 2) {
            // memset(bitmap->arr + start_cell + 1, 0x0, (n_cells - 1) * sizeof(uint64_t));
        }
    }
}

static inline bool arena_bitmap_get(arena_bitmap_t* bitmap, cell_index_t index) {
    return bitmap->arr[(index / 64)] & (1ULL << (index % 64));
}

static void arena_bitmap_gray_stack_push(arena_bitmap_t* bitmap, cell_index_t index) {
    // Store the gray stack length in the first element
    if (unlikely(bitmap->metadata.gray_stack == NULL)) {
        bitmap->metadata.gray_stack = calloc(ARENA_N_CELLS + 1, sizeof(uint16_t));
    }

    cell_index_t** gray_stack = &bitmap->metadata.gray_stack;
    const uint16_t len = *gray_stack[0];

    if (unlikely(gray_stack[len] == 0)) {
        const uint16_t new_len = len * 2;
        cell_index_t* new_gray_stack = realloc(*gray_stack, (size_t)new_len * sizeof(uint16_t));
        verify(new_gray_stack != NULL);
        *gray_stack = new_gray_stack;
        memset(*gray_stack + len, 0, new_len * sizeof(uint16_t));
    }

    *gray_stack[len] = index;
    *gray_stack[0] += 1;
}

static inline uint32_t arena_bitmap_gray_stack_len(arena_bitmap_t* bitmap) {
    if (bitmap->metadata.gray_stack != NULL) {
        return bitmap->metadata.gray_stack[0];
    }

    return 0;
}

static inline arena_t* ptr_get_arena(void* ptr) {
    return (arena_t*)(((uintptr_t)ptr >> ARENA_SIZE_PARAMETER) << ARENA_SIZE_PARAMETER);
}

static inline cell_index_t cell_get_index(void* start_of_cell) {
    return (uintptr_t)start_of_cell & ARENA_SIZE_MASK >> 4;
}

static void* allocate_gc_aligned(size_t size) {
    void* ptr = NULL;
    if (posix_memalign(&ptr, sizeof(arena_t), size) != 0) {
        abort();
    }

    return ptr;
}

static arena_t* allocate_new_arena(arena_list_t* arena_list) {
    arena_t* arena = allocate_gc_aligned(sizeof(arena_t));
    arena->block_bitmap.metadata.gray_stack = NULL;
    arena->mark_bitmap.metadata.frontier = 1;
    arena_bitmap_clear_range(&arena->block_bitmap, 0, ARENA_N_CELLS);
    arena_bitmap_set_range(&arena->mark_bitmap, 0, ARENA_N_CELLS);

    arena_list->last_arena_list = ptr_list_append(arena_list->last_arena_list, arena);
    arena_list->last_arena = arena;

    return arena;
}

static inline cell_index_t* arena_get_frontier(arena_t* arena) {
    return &arena->mark_bitmap.metadata.frontier;
}

static void* arena_bump_allocate(arena_t* arena, uint32_t n_cells) {
    cell_index_t* frontier = arena_get_frontier(arena);
    cell_index_t old_frontier = *frontier;
    uint32_t new_frontier = old_frontier + n_cells;
    if (unlikely(new_frontier > ARENA_N_CELLS)) {
        return NULL;
    }

    arena_bitmap_set(&arena->block_bitmap, old_frontier);
    arena_bitmap_clear_range(&arena->block_bitmap, old_frontier + 1, n_cells - 1);
    arena_bitmap_clear_range(&arena->mark_bitmap, old_frontier, n_cells);
    *frontier = new_frontier;
    return &arena->cells[old_frontier];
}

static void* bump_allocate(arena_list_t* arena_list, uint32_t n_cells) {
    arena_t* last_arena = arena_list->last_arena;

    void* block = arena_bump_allocate(last_arena, n_cells);
    if (unlikely(block == NULL)) {
        // Allocate a new arena
        arena_t* new_arena = allocate_new_arena(arena_list);
        return arena_bump_allocate(new_arena, n_cells);
    }

    return block;
}

static inline uint8_t get_bin(cell_index_t n_cells) {
    return 0;
}

static void* fit_allocate(size_t size) {
    // TODO: Fit Allocator
    abort();
}

static void arena_list_init(arena_list_t* arena_list) {
    ptr_list_init(&arena_list->arenas);
    arena_list->last_arena_list = &arena_list->arenas;
    arena_list->last_arena = NULL;
    allocate_new_arena(arena_list);
}

void gc_ctx_init(gc_ctx* gc) {
    gc->fragmentation = 0;
    gc->gray_queue = NULL;

    arena_list_init(&gc->traversable_arenas);
    arena_list_init(&gc->primitive_arenas);
}

void* gc_alloc(gc_ctx* gc, size_t bytes, bool traversable) {
    if (unlikely(bytes == 0)) {
        abort();
    }

    if (unlikely(bytes > MAX_ARENA_ALLOCATION_THRESHOLD)) {
        // TODO: Huge block allocator must store metadata
        return allocate_gc_aligned(bytes);
    }

    arena_list_t* restrict arena_list = traversable ? &gc->traversable_arenas : &gc->primitive_arenas;
    size_t n_cells = INT_DIV_CEIL(bytes, CELL_SIZE);


    if (unlikely(gc->fragmentation > 100)) {
        return fit_allocate(n_cells);
    }

    return bump_allocate(arena_list, n_cells);
}

static void minor_sweep(ptr_list_iter_t* iter) {
    arena_t* arena;
    while ((arena = ptr_list_iter_next(iter)) != NULL) {
        uint64_t* restrict block_bitmap = arena->block_bitmap.arr;
        uint64_t* restrict mark_bitmap = arena->mark_bitmap.arr;
        for (cell_index_t i = 0; i < ARENA_N_CELLS; i += 1) {
            uint64_t block = block_bitmap[i];
            uint64_t mark = mark_bitmap[i];
            block_bitmap[i] = block & mark;
            mark_bitmap[i] = block | mark;
        }
    }
}

static void gc_minor_sweep(gc_ctx* gc) {
    // Free white blocks, but keeps black blocks
    // block' = block & mark; mark' = block | mark
    ptr_list_iter_t iter;
    ptr_list_iter_start(&iter, &gc->traversable_arenas.arenas);
    minor_sweep(&iter);

    ptr_list_iter_start(&iter, &gc->primitive_arenas.arenas);
    minor_sweep(&iter);
}

static void major_sweep(ptr_list_iter_t* iter) {
    arena_t* arena;
    while ((arena = ptr_list_iter_next(iter)) != NULL) {
        uint64_t* restrict block_bitmap = arena->block_bitmap.arr;
        uint64_t* restrict mark_bitmap = arena->mark_bitmap.arr;
        for (cell_index_t i = 0; i < ARENA_N_CELLS; i += 1) {
            uint64_t block = block_bitmap[i];
            uint64_t mark = mark_bitmap[i];
            block_bitmap[i] = block & mark;
            mark_bitmap[i] = block ^ mark;
        }
    }
}

static void gc_major_sweep(gc_ctx* gc) {
    // Free white blocks and turn black blocks into white blocks
    // block' = block & mark; mark' = block ^ mark
    ptr_list_iter_t iter;
    ptr_list_iter_start(&iter, &gc->traversable_arenas.arenas);
    major_sweep(&iter);

    ptr_list_iter_start(&iter, &gc->primitive_arenas.arenas);
    major_sweep(&iter);
}

static void gc_mark(gc_ctx* gc) {
    ptr_list_iter_t iter;
    ptr_list_iter_start(&iter, &gc->traversable_arenas.arenas);
    arena_t* arena;
    while ((arena = ptr_list_iter_next(&iter)) != NULL) {
    }
}

void gc_cycle(gc_ctx* gc) {
    gc_mark(gc);
    gc_major_sweep(gc);
}

#ifdef VIXENO_TEST
void gc_test_bitmap(void) {
    arena_bitmap_t bitmap;
    arena_bitmap_clear_range(&bitmap, 0, ARENA_N_CELLS);
    arena_bitmap_set(&bitmap, 3);
    arena_bitmap_set(&bitmap, 1);
    arena_bitmap_set(&bitmap, 9);
    verify(bitmap.arr[0] == 0x020A);
    verify(arena_bitmap_get(&bitmap, 3) == true);
    verify(arena_bitmap_get(&bitmap, 2) == false);

    // Test range setting
    arena_bitmap_set_range(&bitmap, 3, 194);
    verify(arena_bitmap_get(&bitmap, 3) == true);
    verify(arena_bitmap_get(&bitmap, 2) == false);
    verify(bitmap.arr[0] == 0xFFFFFFFFFFFFFFFA);
    verify(bitmap.arr[1] == UINT64_MAX);
    verify(bitmap.arr[2] == UINT64_MAX);
    verify(bitmap.arr[3] == 0x03);
    verify(bitmap.arr[4] == 0);

    verify(arena_bitmap_get(&bitmap, 192) == true);
    verify(arena_bitmap_get(&bitmap, 193) == true);
    verify(arena_bitmap_get(&bitmap, 194) == false);

    // Test range clearing
    arena_bitmap_clear_range(&bitmap, 4, 192);
    verify(arena_bitmap_get(&bitmap, 2) == false);
    verify(arena_bitmap_get(&bitmap, 3) == true);
    verify(arena_bitmap_get(&bitmap, 4) == false);
    verify(bitmap.arr[1] == 0);
    verify(bitmap.arr[2] == 0);
    verify(arena_bitmap_get(&bitmap, 191) == false);
    verify(arena_bitmap_get(&bitmap, 192) == true);
}
#endif
