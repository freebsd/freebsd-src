#ifndef _VIRTIO_GPU_H_
#define _VIRTIO_GPU_H_

#include <stdint.h>

int gpu_init(void);
void *gpu_framebuffer(void);
uint32_t gpu_width(void);
uint32_t gpu_height(void);
void gpu_flush_rect(int x, int y, int w, int h);

#endif
