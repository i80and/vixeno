#ifndef __VIXENO_UTIL_H__
#define __VIXENO_UTIL_H__

#include <stdint.h>
#include <stdlib.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

// Requires x > 0, y > 0
#define INT_DIV_CEIL(x, y) (1 + (((x) - 1) / (y)))

// The bottom 3 bits of each pointer on most 64-bit platforms can be
// repurposed for storing our own data. Nice. Don't store values larger
// than PTR_DATA_BITS.
#define PTR_DATA_BITS 7
static inline uint8_t ptr_get_data(void* ptr) { return (uintptr_t)ptr & PTR_DATA_BITS; }
static inline void* ptr_set_data(void* ptr, size_t val) { return (void*)(((uintptr_t)ptr & ~PTR_DATA_BITS) | val); }
static inline void* ptr_get_ptr(void* p) { return (void*)((uintptr_t)p & ~PTR_DATA_BITS); }
_Static_assert(sizeof(void*) >= 8, "Only 64-bit pointers are supported");

static inline uint64_t minu64(uint64_t a, uint64_t b) { return (a < b)? a : b; }
static inline uint32_t minu32(uint32_t a, uint32_t b) { return (a < b)? a : b; }
static inline uint16_t minu16(uint16_t a, uint32_t b) { return (a < b)? a : b; }
#define min(a, b) _Generic((a), uint64_t: minu64, \
                                uint32_t: minu32, \
                                uint16_t: minu16)(a, b)

void __fail(const char*, int, const char*);
#define verify(cond) if (unlikely(!(cond))) { __fail(__FILE__, __LINE__, #cond); }

#endif
