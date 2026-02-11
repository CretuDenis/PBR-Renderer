#include "Arena.h"
#include <stdbool.h>
#include <stdint.h>

bool arena_create(Arena* arena,uint32_t capacity) {
    arena->storage = malloc(capacity);
    if(!arena->storage) return false;
    arena->capacity = capacity; 
    arena->base = 0;
    return true;
}

void arena_free(Arena* arena) {
    free(arena->storage);
}

void* arena_alloc_(Arena* arena, uint32_t element_size,uint32_t count) {
    void* mem = (uint8_t*)arena->storage + arena->base;
    arena->base += count*element_size;
    return mem;
}
