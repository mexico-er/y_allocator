#ifndef Y_ALLOCATOR_H
#define Y_ALLOCATOR_H

#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define Y_FAILURE -1
#define Y_SUCCESS 0

#define Y_ALIGNMENT 16
#define Y_ALIGN(size) (((size) + (Y_ALIGNMENT - 1)) & ~(Y_ALIGNMENT - 1))

struct ychunk_t {
    size_t size;
    int inUse;
    struct ychunk_t *next;
};

struct yinfo_t {
    pthread_mutex_t lock;
    struct ychunk_t *free_list;
    size_t available_mem;
};

int init_yallocator();
void *yalloc(size_t size);
void yfree(void *__ptr);

int isYallocatorReady();

#endif
