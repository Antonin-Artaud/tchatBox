//
// Created by warydra on 05/09/2020.
//

#include <stdlib.h>
#include <string.h>

void *memory_util_deep_copy(void *mem, int size) {
    void *alloc = malloc(size);
    memcpy(alloc, mem, size);
    return alloc;
}