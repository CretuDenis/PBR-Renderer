#ifndef _ARENA_
#define _ARENA_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void* storage;
    uint32_t capacity; 
    uint32_t base;
}Arena;

#define MB(n) ((n) * 1024000)
#define GB(n) ((n) * 1024000000)

bool arena_create(Arena* arena,uint32_t capacity);
void arena_free(Arena* arena);
void* arena_alloc_(Arena* arena, uint32_t elemnt_size,uint32_t count);

#define arena_alloc(arena_ptr,T,count) (T*)arena_alloc_(arena_ptr,sizeof(T),count)

#endif
