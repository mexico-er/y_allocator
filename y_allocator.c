#include "y_allocator.h"

// Use `#define INITIAL_PAGES amount` for the amount of initial pages. The default is 4

#ifndef Y_INITIAL_PAGES
#define Y_INITIAL_PAGES 4 // Set default initial page amount if one wasn't already set by the user
#endif

// Declare a global instance of yinfo_t to keep track of the memory allocator's state
struct yinfo_t yinfo_instance;
struct yinfo_t *yinfo = &yinfo_instance;

// Function to initialize the memory allocator
int init_yallocator() {
    // Calculate the initial size of memory to allocate
    size_t initial_size = getpagesize() * Y_INITIAL_PAGES;

    // Allocate memory using mmap
    void *start = mmap(NULL, initial_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap"); // Print error message if mmap fails
        return Y_FAILURE;
    }

    // Initialize the first memory chunk
    struct ychunk_t *first = (struct ychunk_t*)start;
    first->size = initial_size - sizeof(struct ychunk_t);
    first->inUse = Y_FALSE;
    first->next = NULL;

    // Set the start and first chunk pointers in yinfo and update available memory
    yinfo->start = first;
    yinfo->first = first;
    yinfo->available_mem = first->size;

    return Y_SUCCESS; // Return success
}

// Function to expand memory by allocating more pages
void *expand_memory(size_t size) {
    // Calculate the size of additional memory to allocate
    size_t additional_size = getpagesize() * ((size + sizeof(struct ychunk_t)) / getpagesize() + 1);

    // Allocate memory using mmap
    void *start = mmap(NULL, additional_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (start == MAP_FAILED) {
        perror("mmap"); // Print error message if mmap fails
        return (void *)Y_FAILURE;
    }

    // Initialize the new memory chunk
    struct ychunk_t *new_chunk = (struct ychunk_t *)start;
    new_chunk->size = additional_size - sizeof(struct ychunk_t);
    new_chunk->inUse = Y_FALSE;
    new_chunk->next = NULL;

    // Find the last chunk in the linked list and append the new chunk
    struct ychunk_t *chunk = yinfo->first;
    while (chunk->next != NULL) {
        chunk = chunk->next;
    }
    chunk->next = new_chunk;

    // Update available memory
    yinfo->available_mem += new_chunk->size;

    return (void *)Y_SUCCESS; // Return success
}

// Function to allocate memory of the requested size
void *yalloc(size_t size) {
    if (size == 0) {
        printf("[ERROR] requested size is zero\n");
        return (void *)Y_FAILURE;
    }

    struct ychunk_t *chunk = yinfo->first;

    // Traverse the linked list to find a suitable free chunk
    while (chunk != NULL) {
        if (!atomic_load(&chunk->inUse) && chunk->size >= size) {
            size_t remaining_size = chunk->size - size - sizeof(struct ychunk_t);

            // Split the chunk if the remaining size is enough for a new chunk
            if (remaining_size > sizeof(struct ychunk_t)) {
                struct ychunk_t *new_chunk = (struct ychunk_t *)((char *)chunk + sizeof(struct ychunk_t) + size);
                new_chunk->size = remaining_size;
                new_chunk->inUse = Y_FALSE;
                new_chunk->next = chunk->next;

                chunk->size = size;
                chunk->next = new_chunk;
            }

            // Mark the chunk as in use and update available memory
            atomic_store(&chunk->inUse, Y_TRUE);
            yinfo->available_mem -= (chunk->size + sizeof(struct ychunk_t));

            return (void *)((char *)chunk + sizeof(struct ychunk_t)); // Return the allocated memory
        }
        chunk = chunk->next;
    }

    // No suitable chunk found, expand memory and retry allocation
    if (expand_memory(size) == (void *)Y_FAILURE) {
        printf("[ERROR] memory expansion failed\n");
        return (void *)Y_FAILURE;
    }

    // Retry allocation after expansion
    return yalloc(size);
}

// Function to free allocated memory
void yfree(void *__ptr) {
    if (__ptr == NULL) {
        return;
    }

    // Get the chunk header from the pointer
    struct ychunk_t *chunk = (struct ychunk_t *)((char *)__ptr - sizeof(struct ychunk_t));
    if (atomic_load(&chunk->inUse)) {
        atomic_store(&chunk->inUse, Y_FALSE); // Mark the chunk as free
        yinfo->available_mem += (chunk->size + sizeof(struct ychunk_t)); // Update available memory

        // Merge with the next chunk if it's free
        if (chunk->next != NULL && !atomic_load(&chunk->next->inUse)) {
            chunk->size += sizeof(struct ychunk_t) + chunk->next->size;
            chunk->next = chunk->next->next;
        }

        // Merge with the previous chunk if it's free
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
