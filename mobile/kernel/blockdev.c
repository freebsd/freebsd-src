/*
 * Block Device Abstraction for uOS(m) - VirtIO Backend with Buffer Cache
 */

#include "blockdev.h"
#include "buffer_cache.h"
#include "../drivers/virtio_blk.h"
#include <stdint.h>
#include "memory_utils.h"

int blockdev_init(void) {
    int ret = virtio_blk_init();
    if (ret == 0) {
        buffer_cache_init();
    }
    return ret;
}

int blockdev_read_block(uint32_t block, uint8_t *buf) {
    buffer_t *cache_buf = buffer_get(block);
    if (cache_buf) {
        memcpy(buf, cache_buf->data, BLOCK_SIZE);
        buffer_put(cache_buf);
        return 0;
    }
    return virtio_blk_read_block(block, buf);
}

int blockdev_write_block(uint32_t block, const uint8_t *buf) {
    buffer_t *cache_buf = buffer_get(block);
    if (cache_buf) {
        memcpy(cache_buf->data, buf, BLOCK_SIZE);
        cache_buf->dirty = 1;
        buffer_put(cache_buf);
        return 0;
    }
    return virtio_blk_write_block(block, buf);
}

int blockdev_alloc_block(void) {
    return virtio_blk_alloc_block();
}

uint32_t blockdev_block_size(void) {
    return virtio_blk_block_size();
}

uint32_t blockdev_total_blocks(void) {
    return virtio_blk_total_blocks();
}

int blockdev_flush(void) {
    return buffer_flush();
}
