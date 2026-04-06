/*
 * Framebuffer Graphics Implementation
 * uOS(m) - User OS Mobile
 */

#include "framebuffer.h"
#include "../kernel/memory.h"

/* Simple abs function */
static int abs(int x) {
    return x < 0 ? -x : x;
}

static framebuffer_t fb;

/* Simple 8x16 bitmap font (ASCII 32-127) */
static const uint8_t font_8x16[96][16] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ... (other characters would be defined here)
};

/* Initialize framebuffer */
int fb_init(void) {
    fb.width = FB_WIDTH;
    fb.height = FB_HEIGHT;
    fb.bpp = FB_BPP;

    /* Allocate framebuffer memory */
    fb.buffer = (uint32_t *)mem_alloc(FB_SIZE);
    if (!fb.buffer) {
        return -1;
    }

    /* Clear to black */
    fb_clear(COLOR_BLACK);

    return 0;
}

/* Get framebuffer instance */
framebuffer_t *fb_get(void) {
    return &fb;
}

/* Clear framebuffer */
void fb_clear(uint32_t color) {
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb.buffer[i] = color;
    }
}

/* Set pixel */
void fb_set_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        fb.buffer[y * FB_WIDTH + x] = color;
    }
}

/* Get pixel */
uint32_t fb_get_pixel(int x, int y) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        return fb.buffer[y * FB_WIDTH + x];
    }
    return 0;
}

/* Draw rectangle */
void fb_draw_rect(int x, int y, int width, int height, uint32_t color) {
    /* Top line */
    fb_draw_line(x, y, x + width - 1, y, color);
    /* Bottom line */
    fb_draw_line(x, y + height - 1, x + width - 1, y + height - 1, color);
    /* Left line */
    fb_draw_line(x, y, x, y + height - 1, color);
    /* Right line */
    fb_draw_line(x + width - 1, y, x + width - 1, y + height - 1, color);
}

/* Draw filled rectangle */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            fb_set_pixel(x + j, y + i, color);
        }
    }
}

/* Draw line using Bresenham's algorithm */
void fb_draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        fb_set_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

/* Draw circle using Bresenham's algorithm */
void fb_draw_circle(int x, int y, int radius, uint32_t color) {
    int x_pos = radius;
    int y_pos = 0;
    int err = 0;

    while (x_pos >= y_pos) {
        fb_set_pixel(x + x_pos, y + y_pos, color);
        fb_set_pixel(x + y_pos, y + x_pos, color);
        fb_set_pixel(x - y_pos, y + x_pos, color);
        fb_set_pixel(x - x_pos, y + y_pos, color);
        fb_set_pixel(x - x_pos, y - y_pos, color);
        fb_set_pixel(x - y_pos, y - x_pos, color);
        fb_set_pixel(x + y_pos, y - x_pos, color);
        fb_set_pixel(x + x_pos, y - y_pos, color);

        if (err <= 0) {
            y_pos += 1;
            err += 2 * y_pos + 1;
        }
        if (err > 0) {
            x_pos -= 1;
            err -= 2 * x_pos + 1;
        }
    }
}

/* Draw filled circle */
void fb_fill_circle(int x, int y, int radius, uint32_t color) {
    int x_pos = radius;
    int y_pos = 0;
    int err = 0;

    while (x_pos >= y_pos) {
        fb_draw_line(x - x_pos, y + y_pos, x + x_pos, y + y_pos, color);
        fb_draw_line(x - x_pos, y - y_pos, x + x_pos, y - y_pos, color);
        fb_draw_line(x - y_pos, y + x_pos, x + y_pos, y + x_pos, color);
        fb_draw_line(x - y_pos, y - x_pos, x + y_pos, y - x_pos, color);

        if (err <= 0) {
            y_pos += 1;
            err += 2 * y_pos + 1;
        }
        if (err > 0) {
            x_pos -= 1;
            err -= 2 * x_pos + 1;
        }
    }
}

/* Draw text using simple bitmap font */
void fb_draw_text(int x, int y, const char *text, uint32_t color, uint32_t bg_color) {
    int start_x = x;
    while (*text) {
        if (*text == '\n') {
            x = start_x;
            y += 16;
            text++;
            continue;
        }

        if (*text >= 32 && *text <= 127) {
            int char_index = *text - 32;
            for (int row = 0; row < 16; row++) {
                uint8_t row_data = font_8x16[char_index][row];
                for (int col = 0; col < 8; col++) {
                    if (row_data & (1 << (7 - col))) {
                        fb_set_pixel(x + col, y + row, color);
                    } else if (bg_color != COLOR_TRANSPARENT) {
                        fb_set_pixel(x + col, y + row, bg_color);
                    }
                }
            }
        }
        x += 8;
        text++;
    }
}

/* Flush framebuffer to display (placeholder for actual display hardware) */
void fb_flush(void) {
    /* In a real implementation, this would copy the framebuffer to the display */
    /* For now, it's a no-op */
}