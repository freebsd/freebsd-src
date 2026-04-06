/*
 * Block Device Abstraction for uOS(m)
 */

#ifndef _BLOCKDEV_H_
#define _BLOCKDEV_H_

#include <stdint.h>

int blockdev_init(void);
int blockdev_read_block(uint32_t block, uint8_t *buf);
int blockdev_write_block(uint32_t block, const uint8_t *buf);
int blockdev_alloc_block(void);
uint32_t blockdev_block_size(void);
uint32_t blockdev_total_blocks(void);
int blockdev_flush(void);

#endif /* _BLOCKDEV_H_ */