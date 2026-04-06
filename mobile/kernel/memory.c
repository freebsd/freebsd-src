/*
 * Memory Management Implementation
 * uOS(m) - User OS Mobile
 */

#include "memory.h"
#include <stdint.h>

#define KERNEL_HEAP_SIZE (1024 * 1024)  /* 1MB */
#define HEAP_ALIGN 16

typedef struct heap_block {
    uint64_t size;
    int free;
    struct heap_block *next;
} heap_block_t;

#define HEAP_HEADER_SIZE ((sizeof(heap_block_t) + (HEAP_ALIGN - 1)) & ~(HEAP_ALIGN - 1))

static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static heap_block_t *heap_free_list;
static uint64_t heap_used;

extern void uart_puts(const char *s);
extern void uart_putc(char c);

static uint64_t align_up(uint64_t value, uint64_t align) {
    return (value + align - 1) & ~(align - 1);
}

static heap_block_t *find_free_block(uint64_t size) {
    heap_block_t *block = heap_free_list;
    while (block) {
        if (block->free && block->size >= size) {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

static void split_block(heap_block_t *block, uint64_t size) {
    uint8_t *base = (uint8_t *)block;
    uint8_t *next_block_addr = base + HEAP_HEADER_SIZE + size;
    heap_block_t *next_block = (heap_block_t *)next_block_addr;
    next_block->size = block->size - size - HEAP_HEADER_SIZE;
    next_block->free = 1;
    next_block->next = block->next;

    block->size = size;
    block->next = next_block;
}

static void coalesce_free_blocks(void) {
    heap_block_t *block = heap_free_list;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += HEAP_HEADER_SIZE + block->next->size;
            block->next = block->next->next;
            continue;
        }
        block = block->next;
    }
}

/* Initialize memory management */
int mem_init(void) {
    uart_puts("Memory management initializing...\n");
    heap_free_list = (heap_block_t *)kernel_heap;
    heap_free_list->size = KERNEL_HEAP_SIZE - HEAP_HEADER_SIZE;
    heap_free_list->free = 1;
    heap_free_list->next = NULL;
    heap_used = 0;
    uart_puts("Memory ready\n");
    return 0;
}

/* Allocate memory from the kernel heap */
void *mem_alloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    uint64_t aligned_size = align_up(size, HEAP_ALIGN);
    heap_block_t *block = find_free_block(aligned_size);
    if (!block) {
        uart_puts("Heap exhausted\n");
        return NULL;
    }

    if (block->size >= aligned_size + HEAP_HEADER_SIZE + HEAP_ALIGN) {
        split_block(block, aligned_size);
    }

    block->free = 0;
    heap_used += block->size;

    uint8_t *payload = (uint8_t *)block + HEAP_HEADER_SIZE;
    for (uint64_t i = 0; i < aligned_size; i++) {
        payload[i] = 0;
    }

    return payload;
}

/* Free memory back to the kernel heap */
void mem_free(void *ptr) {
    if (!ptr) {
        return;
    }

    heap_block_t *block = (heap_block_t *)((uint8_t *)ptr - HEAP_HEADER_SIZE);
    block->free = 1;
    heap_used -= block->size;
    coalesce_free_blocks();
}

/* Map a virtual address region */
int mem_mmap(uint64_t addr, size_t len, int prot, int flags, uint32_t pid) {
    (void)addr;
    (void)prot;
    (void)flags;
    (void)pid;

    if (len == 0) {
        return -1;
    }

    void *ptr = mem_alloc(len);
    return ptr ? 0 : -1;
}

/* Unmap virtual address region */
int mem_munmap(uint64_t addr, size_t len, uint32_t pid) {
    (void)len;
    (void)pid;
    mem_free((void *)(uintptr_t)addr);
    return 0;
}

/* Break (heap expansion) */
int mem_brk(uint64_t addr) {
    uint8_t *heap_base = kernel_heap;
    uint8_t *heap_end = kernel_heap + KERNEL_HEAP_SIZE;
    uint8_t *candidate = (uint8_t *)(uintptr_t)addr;

    if (addr == 0) {
        return (int)(uintptr_t)heap_base;
    }

    if (candidate >= heap_base && candidate <= heap_end) {
        return 0;
    }

    return -1;
}

/* Kernel-space memory utilities */