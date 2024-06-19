#include "y_allocator.h"

#define INITIAL_PAGES 10  // Define how many pages you want to initially allocate

struct yinfo_t yinfo_instance;
struct yinfo_t *yinfo = &yinfo_instance;

int init_yallocator() {
    size_t initial_size = getpagesize() * INITIAL_PAGES;
    void *start = mmap(NULL, initial_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap");
        return Y_FAILURE;
    }

    struct ychunk_t *first = (struct ychunk_t*)start;
    first->size = initial_size - sizeof(struct ychunk_t);
    first->inUse = Y_FALSE;
    first->next = NULL;

    yinfo->start = first;
    yinfo->first = first;
    yinfo->available_mem = first->size;

    return Y_SUCCESS;
}

void *expand_memory(size_t size) {
    size_t additional_size = getpagesize() * ((size + sizeof(struct ychunk_t)) / getpagesize() + 1);
    void *start = mmap(NULL, additional_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap");
        return (void *)Y_FAILURE;
    }

    struct ychunk_t *new_chunk = (struct ychunk_t *)start;
    new_chunk->size = additional_size - sizeof(struct ychunk_t);
    new_chunk->inUse = Y_FALSE;
    new_chunk->next = NULL;

    struct ychunk_t *chunk = yinfo->first;
    while (chunk->next != NULL) {
        chunk = chunk->next;
    }
    chunk->next = new_chunk;
    yinfo->available_mem += new_chunk->size;

    return (void *)Y_SUCCESS;
}

void *yalloc(size_t size) {
    if (size == 0) {
        printf("[ERROR] requested size is zero\n");
        return (void *)Y_FAILURE;
    }

    struct ychunk_t *chunk = yinfo->first;
    while (chunk != NULL) {
        if (!atomic_load(&chunk->inUse) && chunk->size >= size) {
            size_t remaining_size = chunk->size - size - sizeof(struct ychunk_t);

            if (remaining_size > sizeof(struct ychunk_t)) {
                struct ychunk_t *new_chunk = (struct ychunk_t *)((char *)chunk + sizeof(struct ychunk_t) + size);
                new_chunk->size = remaining_size;
                new_chunk->inUse = Y_FALSE;
                new_chunk->next = chunk->next;

                chunk->size = size;
                chunk->next = new_chunk;
            }

            atomic_store(&chunk->inUse, Y_TRUE);
            yinfo->available_mem -= (chunk->size + sizeof(struct ychunk_t));
            return (void *)((char *)chunk + sizeof(struct ychunk_t));
        }
        chunk = chunk->next;
    }

    // No suitable chunk found, expand memory
    if (expand_memory(size) == (void *)Y_FAILURE) {
        printf("[ERROR] memory expansion failed\n");
        return (void *)Y_FAILURE;
    }

    // Retry allocation after expansion
    return yalloc(size);
}

void yfree(void *__ptr) {
    if (__ptr == NULL) {
        return;
    }

    struct ychunk_t *chunk = (struct ychunk_t *)((char *)__ptr - sizeof(struct ychunk_t));
    if (atomic_load(&chunk->inUse)) {
        atomic_store(&chunk->inUse, Y_FALSE);
        yinfo->available_mem += (chunk->size + sizeof(struct ychunk_t));

        if (chunk->next != NULL && !atomic_load(&chunk->next->inUse)) {
            chunk->size += sizeof(struct ychunk_t) + chunk->next->size;
            chunk->next = chunk->next->next;
        }

        struct ychunk_t *prev = yinfo->start;
        while (prev != NULL && prev->next != chunk) {
            prev = prev->next;
        }

        if (prev != NULL && !atomic_load(&prev->inUse)) {
            prev->size += sizeof(struct ychunk_t) + chunk->size;
            prev->next = chunk->next;
        }
    }
}