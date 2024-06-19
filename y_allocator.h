#ifndef Y_ALLOCATOR_H
#define Y_ALLOCATOR_H

#include <stdatomic.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

#define Y_FAILURE -1
#define Y_SUCCESS 0

#define Y_FALSE 0
#define Y_TRUE 1

struct ychunk_t {
    size_t size;
    atomic_bool inUse;
    struct ychunk_t *next;
};

struct yinfo_t {
    struct ychunk_t *start;
    struct ychunk_t *first;
    size_t available_mem;
};

int init_yallocator();

void *expand_memory(size_t size);

void *yalloc(size_t size);
void yfree(void *__ptr);

#endif
