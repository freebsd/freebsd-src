/*
 * Camera Mock - Software test pattern generator
 * For testing camera stack without hardware
 */

#include "camera.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t hsv_to_rgb(float h, float s, float v) {
    (void)h; (void)s; (void)v;
    return 0xFF000000;
}

static void draw_color_bars(uint32_t *buf, int w, int h) {
    int bar_w = w / 8;
    uint32_t colors[] = {
        0xFFC0C0C0, 0xFFC0FF00, 0xFFC000FF, 0xFF00FFFF,
        0xFF0000FF, 0xFF00FF00, 0xFF0000C0, 0xFF000000
    };
    for (int bar = 0; bar < 8; bar++) {
        for (int y = 0; y < h; y++) {
            for (int x = bar * bar_w; x < (bar + 1) * bar_w && x < w; x++) {
                buf[y * w + x] = colors[bar];
            }
        }
    }
}

int cam_mock_generate_frame(cam_handle_t *handle, cam_frame_t *frame) {
    if (!handle || !frame || !handle->mock_buf) return -1;
    uint32_t *buf = (uint32_t*)handle->mock_buf;
    int w = handle->width;
    int h = handle->height;

    if (handle->mock_mode == CAM_MOCK_BARS) {
        draw_color_bars(buf, w, h);
    } else {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                buf[y * w + x] = ((x * 255 / w) << 16) | ((y * 255 / h) << 8) | 0x80;
            }
        }
    }

    frame->data = handle->mock_buf;
    frame->width = w;
    frame->height = h;
    frame->format = CAM_FORMAT_ARGB8888;
    frame->timestamp_ns = (uint64_t)time(NULL) * 1000000000ULL;
    frame->sequence = handle->frame_count++;
    return 0;
}

int cam_mock_init(cam_handle_t *handle) {
    if (!handle) return -1;
    handle->mock_mode = CAM_MOCK_BARS;
    handle->mock_buf = malloc(handle->width * handle->height * 4);
    if (!handle->mock_buf) return -1;
    handle->frame_count = 0;
    return 0;
}

void cam_mock_cleanup(cam_handle_t *handle) {
    if (handle && handle->mock_buf) {
        free(handle->mock_buf);
        handle->mock_buf = NULL;
    }
}
