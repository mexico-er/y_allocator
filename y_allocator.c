#include "y_allocator.h"
#include <stdio.h>
#include <sys/mman.h>

#ifndef Y_INITIAL_PAGES
#define Y_INITIAL_PAGES 4
#endif

#define Y_BATCH_PAGES 16

struct yinfo_t yinfo_instance = {PTHREAD_MUTEX_INITIALIZER, NULL, 0};
struct yinfo_t *yinfo = &yinfo_instance;

static int y_ready = 0;

static inline void *expand_memory(size_t size) {
    size_t additional_size = getpagesize() * ((size + sizeof(struct ychunk_t)) / getpagesize() + 1);

    void *start = mmap(NULL, additional_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }

    struct ychunk_t *new_chunk = (struct ychunk_t *)start;
    new_chunk->size = additional_size - sizeof(struct ychunk_t);
    new_chunk->inUse = 0;
    new_chunk->next = NULL;

    return new_chunk;
}

int init_yallocator() {
    size_t initial_size = getpagesize() * Y_INITIAL_PAGES;

    void *start = mmap(NULL, initial_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap");
        return Y_FAILURE;
    }

    struct ychunk_t *first = (struct ychunk_t *)start;
    first->size = initial_size - sizeof(struct ychunk_t);
    first->inUse = 0;
    first->next = NULL;

    yinfo->free_list = first;
    yinfo->available_mem = first->size;

    y_ready = 1;

    return Y_SUCCESS;
}

int isYallocatorReady() {
    return y_ready;
}

void *yalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    size = Y_ALIGN(size);

    pthread_mutex_lock(&yinfo->lock);

    struct ychunk_t *prev = NULL;
    struct ychunk_t *chunk = yinfo->free_list;

    while (chunk != NULL) {
        if (!chunk->inUse && chunk->size >= size) {
            size_t remaining_size = chunk->size - size - sizeof(struct ychunk_t);

            if (remaining_size > sizeof(struct ychunk_t)) {
                struct ychunk_t *new_chunk = (struct ychunk_t *)((char *)chunk + sizeof(struct ychunk_t) + size);
                new_chunk->size = remaining_size;
                new_chunk->inUse = 0;
                new_chunk->next = chunk->next;

                chunk->size = size;
                chunk->next = new_chunk;
            }

            chunk->inUse = 1;

            if (prev) {
                prev->next = chunk->next;
            } else {
                yinfo->free_list = chunk->next;
            }

            yinfo->available_mem -= (chunk->size + sizeof(struct ychunk_t));
            pthread_mutex_unlock(&yinfo->lock);

            return (void *)((char *)chunk + sizeof(struct ychunk_t));
        }
        prev = chunk;
        chunk = chunk->next;
    }

    chunk = expand_memory(size);
    if (!chunk) {
        pthread_mutex_unlock(&yinfo->lock);
        return NULL;
    }

    chunk->inUse = 1;
    yinfo->available_mem += chunk->size;

    pthread_mutex_unlock(&yinfo->lock);
    return (void *)((char *)chunk + sizeof(struct ychunk_t));
}

void yfree(void *__ptr) {
    if (__ptr == NULL) {
        return;
    }

    struct ychunk_t *chunk = (struct ychunk_t *)((char *)__ptr - sizeof(struct ychunk_t));

    pthread_mutex_lock(&yinfo->lock);

    chunk->inUse = 0;
    yinfo->available_mem += (chunk->size + sizeof(struct ychunk_t));

    if (chunk->next != NULL && !chunk->next->inUse) {
        chunk->size += sizeof(struct ychunk_t) + chunk->next->size;
        chunk->next = chunk->next->next;
    }

    struct ychunk_t *prev = yinfo->free_list;
    while (prev != NULL && prev->next != chunk) {
        prev = prev->next;
    }

    if (prev != NULL && !prev->inUse) {
        prev->size += sizeof(struct ychunk_t) + chunk->size;
        prev->next = chunk->next;
    } else {
        chunk->next = yinfo->free_list;
        yinfo->free_list = chunk;
    }

    pthread_mutex_unlock(&yinfo->lock);
}
