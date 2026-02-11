#ifndef VECTOR_H
#define VECTOR_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define INITIAL_CAPACITY 256

#define vector(T) struct { T* data; uint32_t capacity; uint32_t size; }
#define vector_create(vec,T) { \
    vec.data = malloc(sizeof(T) * INITIAL_CAPACITY);\
    vec.capacity = INITIAL_CAPACITY;\
    vec.size = 0;\
}

#define vector_reserve(vec,T,capacity) {\
    vec.data = realloc(vec.data,capacity * sizeof(T));\
    vec.capacity = capacity;\
}

#define vector_resize(vec,T,new_size) {\
    if(new_size >= vec.capacity) {\
        vec.capacity = (new_size * 2);\
        vec.data = realloc(vec.data,vec.capacity * sizeof(T));\
    }\
    vec.size = new_size;\
}

#define vector_push(vec,T,elem) {\
    if(vec.size == vec.capacity) {\
        vec.capacity *= 2;\
        vec.data = realloc(vec.data,vec.capacity * sizeof(T));\
    }\
    memcpy(&vec.data[vec.size],&elem,sizeof(T));\
    ++vec.size;\
}

#define vector_push_const(vec,T,elem) {\
    if(vec.size == vec.capacity) {\
        vec.capacity *= 2;\
        vec.data = realloc(vec.data,vec.capacity * sizeof(T));\
    }\
    vec.data[vec.size++] = (elem);\
}

#define vector_push_array(vec,T,array_ptr,count) {\
    if(vec.size + count >= vec.capacity) {\
        vec.capacity = (vec.size + (count)) * 2;\
        vec.data = realloc(vec.data,vec.capacity * sizeof(T));\
    }\
    memcpy(&vec.data[vec.size],array_ptr,(count) * sizeof(T));\
    vec.size += (count);\
}

#define vector_at(vec,index) vec.data[(index)]

#define vector_pop(vec) {\
    assert(vec.size != 0 && "Pop on empty vector!");\
    --vec.size;\
}

#define vector_remove(vec,T,index,removed_element_ptr) {\
    assert((index) >= 0 && (index) < vec.size && "Trying to delete invalid index");\
    T aux = vec.data[vec.size - 1];\
    memcpy((removed_element_ptr),&vec.data[index],sizeof(T));\
    vec.data[index] = aux;\
    --vec.size;\
}




#endif
