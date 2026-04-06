/*
 * Buffer Cache Implementation
 * uOS(m) - User OS Mobile
 */

#include "buffer_cache.h"
#include "memory.h"
#include "../drivers/virtio_blk.h"
#include "memory_utils.h"

static buffer_t buffer_cache[BUFFER_CACHE_SIZE];
static int buffer_lru[BUFFER_CACHE_SIZE];
static int lru_head = 0;

extern void uart_puts(const char *s);

/* Initialize buffer cache */
int buffer_cache_init(void) {
    uart_puts("Buffer cache initializing...\n");
    
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++) {
        buffer_cache[i].block_num = 0;
        buffer_cache[i].dirty = 0;
        buffer_cache[i].valid = 0;
        buffer_cache[i].ref_count = 0;
        buffer_lru[i] = i;
    }
    
    uart_puts("Buffer cache ready\n");
    return 0;
}

/* Find a buffer in cache */
static buffer_t *buffer_find(uint32_t block_num) {
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++) {
        if (buffer_cache[i].valid && buffer_cache[i].block_num == block_num) {
            return &buffer_cache[i];
        }
    }
    return NULL;
}

/* Evict a buffer (LRU) */
static buffer_t *buffer_evict(void) {
    int idx = buffer_lru[lru_head];
    buffer_t *buf = &buffer_cache[idx];
    
    if (buf->dirty) {
        /* Write back */
        virtio_blk_write_block(buf->block_num, buf->data);
        buf->dirty = 0;
    }
    
    buf->valid = 0;
    buf->ref_count = 0;
    return buf;
}

/* Get a buffer for a block */
buffer_t *buffer_get(uint32_t block_num) {
    buffer_t *buf = buffer_find(block_num);
    if (buf) {
        buf->ref_count++;
        return buf;
    }
    
    /* Not in cache, find free slot */
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++) {
        if (buffer_cache[i].ref_count == 0) {
            buf = &buffer_cache[i];
            break;
        }
    }
    
    if (!buf) {
        /* Evict one */
        buf = buffer_evict();
    }
    
    /* Load from disk */
    if (virtio_blk_read_block(block_num, buf->data) == 0) {
        buf->block_num = block_num;
        buf->valid = 1;
        buf->dirty = 0;
        buf->ref_count = 1;
        return buf;
    }
    
    return NULL;
}

/* Release a buffer */
void buffer_put(buffer_t *buf) {
    if (buf && buf->ref_count > 0) {
        buf->ref_count--;
    }
}

/* Flush all dirty buffers */
int buffer_flush(void) {
    for (int i = 0; i < BUFFER_CACHE_SIZE; i++) {
        if (buffer_cache[i].valid && buffer_cache[i].dirty) {
            if (virtio_blk_write_block(buffer_cache[i].block_num, buffer_cache[i].data) != 0) {
                return -1;
            }
            buffer_cache[i].dirty = 0;
        }
    }
    return 0;
}