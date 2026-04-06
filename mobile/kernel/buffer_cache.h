/*
 * Buffer Cache for Block Devices
 * uOS(m) - User OS Mobile
 */

#ifndef _BUFFER_CACHE_H_
#define _BUFFER_CACHE_H_

#include <stdint.h>
#include <stddef.h>

#define BUFFER_CACHE_SIZE 64
#define BLOCK_SIZE 512

/* Buffer cache entry */
typedef struct {
    uint32_t block_num;
    uint8_t data[BLOCK_SIZE];
    int dirty;
    int valid;
    int ref_count;
} buffer_t;

/* Initialize buffer cache */
int buffer_cache_init(void);

/* Get a buffer for a block */
buffer_t *buffer_get(uint32_t block_num);

/* Release a buffer */
void buffer_put(buffer_t *buf);

/* Flush all dirty buffers */
int buffer_flush(void);

#endif /* _BUFFER_CACHE_H_ */