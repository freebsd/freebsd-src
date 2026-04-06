/*
 * Framebuffer Graphics System for uOS(m)
 * uOS(m) - User OS Mobile
 */

#ifndef _FRAMEBUFFER_H_
#define _FRAMEBUFFER_H_

#include <stdint.h>

/* Framebuffer configuration */
#define FB_WIDTH        1080
#define FB_HEIGHT       1920
#define FB_BPP          32
#define FB_SIZE         (FB_WIDTH * FB_HEIGHT * (FB_BPP / 8))

/* Color definitions (RGBA) */
#define COLOR_BLACK     0xFF000000
#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_RED       0xFFFF0000
#define COLOR_GREEN     0xFF00FF00
#define COLOR_BLUE      0xFF0000FF
#define COLOR_GRAY      0xFF808080
#define COLOR_TRANSPARENT 0x00000000

/* Framebuffer structure */
typedef struct {
    uint32_t *buffer;
    int width;
    int height;
    int bpp;
} framebuffer_t;

/* Initialize framebuffer */
int fb_init(void);

/* Get framebuffer instance */
framebuffer_t *fb_get(void);

/* Clear framebuffer */
void fb_clear(uint32_t color);

/* Set pixel */
void fb_set_pixel(int x, int y, uint32_t color);

/* Get pixel */
uint32_t fb_get_pixel(int x, int y);

/* Draw rectangle */
void fb_draw_rect(int x, int y, int width, int height, uint32_t color);

/* Draw filled rectangle */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);

/* Draw line */
void fb_draw_line(int x1, int y1, int x2, int y2, uint32_t color);

/* Draw circle */
void fb_draw_circle(int x, int y, int radius, uint32_t color);

/* Draw filled circle */
void fb_fill_circle(int x, int y, int radius, uint32_t color);

/* Draw text (simple bitmap font) */
void fb_draw_text(int x, int y, const char *text, uint32_t color, uint32_t bg_color);

/* Flush framebuffer to display */
void fb_flush(void);

#endif /* _FRAMEBUFFER_H_ */