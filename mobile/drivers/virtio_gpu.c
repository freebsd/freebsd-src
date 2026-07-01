/*
 * VirtIO GPU Driver for QEMU VGA/virtio-gpu
 *
 * Uses the QEMU stdvga framebuffer at physical 0x40000000
 * (1280x720x32 BGRA). This is the simplest and most reliable way to get
 * graphical output working in QEMU RISC-V virt with -vga virtio.
 */

#include <stdint.h>

#define GPU_FB_PHYS 0x40000000UL
#define GPU_FB_SIZE (1280 * 720 * 4)

static volatile uint32_t *gpu_fb;

int gpu_init(void) {
	gpu_fb = (volatile uint32_t *)GPU_FB_PHYS;
	for (uint32_t i = 0; i < 1280 * 720; i++) {
		gpu_fb[i] = 0xFF0E0E1AUL;
	}
	return 0;
}

void *gpu_framebuffer(void) { return (void *)gpu_fb; }
uint32_t gpu_width(void) { return 1280; }
uint32_t gpu_height(void) { return 720; }
void gpu_flush_rect(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
