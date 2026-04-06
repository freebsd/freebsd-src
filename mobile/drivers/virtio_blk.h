/*
 * VirtIO Block Device Driver Header for uOS(m)
 */

#ifndef _VIRTIO_BLK_H_
#define _VIRTIO_BLK_H_

int virtio_blk_init(void);
int virtio_blk_read_block(uint32_t block, uint8_t *buf);
int virtio_blk_write_block(uint32_t block, const uint8_t *buf);
int virtio_blk_alloc_block(void);
uint32_t virtio_blk_block_size(void);
uint32_t virtio_blk_total_blocks(void);

#endif /* _VIRTIO_BLK_H_ */