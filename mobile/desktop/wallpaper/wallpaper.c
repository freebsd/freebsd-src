/*
 * Wallpaper Engine Implementation
 */

#include "wallpaper.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static wp_engine_t *g_wp = NULL;

static uint32_t wp_gradient_color_at(const wp_color_stop_t *colors, int count, float t) {
    if (count == 0) return 0xFF000000;
    if (count == 1) {
        return (colors[0].r << 24) | (colors[0].g << 16) | (colors[0].b << 8) | colors[0].a;
    }
    if (t <= colors[0].position) {
        return (colors[0].r << 24) | (colors[0].g << 16) | (colors[0].b << 8) | colors[0].a;
    }
    if (t >= colors[count - 1].position) {
        return (colors[count - 1].r << 24) | (colors[count - 1].g << 16) | (colors[count - 1].b << 8) | colors[count - 1].a;
    }
    for (int i = 0; i < count - 1; i++) {
        if (t >= colors[i].position && t <= colors[i + 1].position) {
            float local = (t - colors[i].position) / (colors[i + 1].position - colors[i].position);
            float e = local * local * (3 - 2 * local);
            uint8_t r = (uint8_t)(colors[i].r + (colors[i + 1].r - colors[i].r) * e);
            uint8_t g = (uint8_t)(colors[i].g + (colors[i + 1].g - colors[i].g) * e);
            uint8_t b = (uint8_t)(colors[i].b + (colors[i + 1].b - colors[i].b) * e);
            uint8_t a = (uint8_t)(colors[i].a + (colors[i + 1].a - colors[i].a) * e);
            return (r << 24) | (g << 16) | (b << 8) | a;
        }
    }
    return 0xFF000000;
}

static void wp_init_particles(wallpaper_t *wp) {
    wp->particle_count = 64;
    for (int i = 0; i < wp->particle_count; i++) {
        wp->particles[i][0] = (float)(rand() % FB_WIDTH);
        wp->particles[i][1] = (float)(rand() % FB_HEIGHT);
        wp->particles[i][2] = 0.5f + ((float)(rand() % 100) / 100.0f) * 2.0f;
    }
}

wp_engine_t *wp_init(compositor_t *comp) {
    (void)comp;
    if (!g_wp) {
        g_wp = calloc(1, sizeof(wp_engine_t));
        if (g_wp) {
            g_wp->initialized = 1;
            g_wp->output_width = FB_WIDTH;
            g_wp->output_height = FB_HEIGHT;
            g_wp->monitor_count = 1;
            wallpaper_t *wp = &g_wp->wallpapers[0];
            wp->type = WP_TYPE_GRADIENT;
            wp->color_count = 3;
            wp->colors[0] = (wp_color_stop_t){0x0A, 0x0A, 0x2A, 0xFF, 0.0f};
            wp->colors[1] = (wp_color_stop_t){0x1A, 0x1A, 0x4A, 0xFF, 0.5f};
            wp->colors[2] = (wp_color_stop_t){0x2A, 0x0A, 0x3A, 0xFF, 1.0f};
            wp->anim_type = WP_ANIM_NONE;
            wp_init_particles(wp);
        }
    }
    return g_wp;
}

void wp_shutdown(void) {
    if (g_wp) {
        for (int i = 0; i < WP_MAX_MONITORS; i++) {
            wallpaper_t *wp = &g_wp->wallpapers[i];
            if (wp->image_data) free(wp->image_data);
        }
        free(g_wp);
        g_wp = NULL;
    }
}

int wp_set_image(const char *path, int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS) return -1;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    if (wp->image_data) { free(wp->image_data); wp->image_data = NULL; }
    wp->type = WP_TYPE_IMAGE;
    strncpy(wp->image_path, path, sizeof(wp->image_path) - 1);
    wp->anim_type = WP_ANIM_NONE;
    wp->fade_active = 1;
    wp->fade_start = 0;
    wp->fade_complete = 0;
    return 0;
}

int wp_set_gradient(wp_color_stop_t *colors, int num_stops, int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS || num_stops > WP_MAX_COLORS) return -1;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    memcpy(wp->colors, colors, num_stops * sizeof(wp_color_stop_t));
    wp->color_count = num_stops;
    wp->type = WP_TYPE_GRADIENT;
    wp->anim_type = WP_ANIM_GRADIENT_SHIFT;
    wp->fade_active = 1;
    wp->fade_start = 0;
    wp->fade_complete = 0;
    return 0;
}

int wp_set_solid(uint32_t color, int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS) return -1;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    wp->colors[0].r = (color >> 24) & 0xFF;
    wp->colors[0].g = (color >> 16) & 0xFF;
    wp->colors[0].b = (color >> 8) & 0xFF;
    wp->colors[0].a = color & 0xFF;
    wp->colors[0].position = 0.0f;
    wp->color_count = 1;
    wp->type = WP_TYPE_SOLID;
    wp->anim_type = WP_ANIM_NONE;
    wp->fade_active = 1;
    wp->fade_complete = 0;
    return 0;
}

int wp_set_animated(wp_anim_type_t anim_type, int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS) return -1;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    wp->anim_type = anim_type;
    wp->anim_running = 1;
    wp->anim_start_time = 0;
    if (anim_type == WP_ANIM_PARTICLES) wp_init_particles(wp);
    wp->type = WP_ANIM_NONE != anim_type ? WP_TYPE_ANIMATED : wp->type;
    wp->fade_active = 1;
    wp->fade_complete = 0;
    return 0;
}

void wp_render(layer_t *bg_layer, int monitor_idx) {
    if (!g_wp || !bg_layer || monitor_idx >= WP_MAX_MONITORS) return;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    int w = g_wp->output_width;
    int h = g_wp->output_height;
    if (w <= 0 || h <= 0) return;
    if (bg_layer->surface_count == 0) {
        comp_add_surface(bg_layer, 0, 0, w, h);
    }
    if (bg_layer->surfaces[0].pixel_data) {
        if (wp->type == WP_TYPE_IMAGE && wp->image_data) {
            memcpy(bg_layer->surfaces[0].pixel_data, wp->image_data, w * h * sizeof(uint32_t));
        } else if (wp->type == WP_TYPE_SOLID && wp->color_count > 0) {
            uint32_t c = (wp->colors[0].r << 24) | (wp->colors[0].g << 16) | (wp->colors[0].b << 8) | wp->colors[0].a;
            for (int i = 0; i < w * h; i++) bg_layer->surfaces[0].pixel_data[i] = c;
        } else if (wp->type == WP_TYPE_GRADIENT || wp->type == WP_TYPE_ANIMATED) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    float t;
                    if (wp->type == WP_TYPE_GRADIENT) t = (float)x / (float)w;
                    else t = (float)(x + y) / (float)(w + h);
                    t += wp->gradient_offset;
                    if (t > 1.0f) t -= 1.0f;
                    bg_layer->surfaces[0].pixel_data[y * w + x] = wp_gradient_color_at(wp->colors, wp->color_count, t);
                }
            }
        }
        if (wp->anim_type == WP_ANIM_PARTICLES && wp->anim_running) {
            for (int i = 0; i < wp->particle_count; i++) {
                int px = (int)wp->particles[i][0];
                int py = (int)wp->particles[i][1];
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    int r = 2;
                    for (int dy = -r; dy <= r; dy++) {
                        for (int dx = -r; dx <= r; dx++) {
                            int nx = px + dx, ny = py + dy;
                            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                                bg_layer->surfaces[0].pixel_data[ny * w + nx] = 0x55FFFFFF;
                            }
                        }
                    }
                }
                wp->particles[i][1] -= wp->particles[i][2];
                if (wp->particles[i][1] < 0) {
                    wp->particles[i][0] = (float)(rand() % w);
                    wp->particles[i][1] = (float)h;
                    wp->particles[i][2] = 0.5f + ((float)(rand() % 100) / 100.0f) * 2.0f;
                }
            }
        }
        if (wp->anim_type == WP_ANIM_WAVES && wp->wave_phase < 6.28f) wp->wave_phase += 0.01f;
        else if (wp->anim_type == WP_ANIM_GRADIENT_SHIFT) wp->gradient_offset += 0.0005f;
        bg_layer->surfaces[0].dirty = 1;
    }
}

void wp_render_all(void) {
    for (int i = 0; i < g_wp->monitor_count; i++) wp_render(comp_get_layer(LAYER_BACKGROUND), i);
}

void wp_update_animations(void) {
    if (!g_wp) return;
    for (int i = 0; i < WP_MAX_MONITORS; i++) {
        wallpaper_t *wp = &g_wp->wallpapers[i];
        if (wp->anim_running && wp->anim_type != WP_ANIM_NONE) {
            wp_render(comp_get_layer(LAYER_BACKGROUND), i);
        }
    }
}

int wp_fade_to(wp_type_t type, void *data, int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS) return -1;
    wallpaper_t *wp = &g_wp->wallpapers[monitor_idx];
    wp->fade_active = 1;
    wp->fade_start = 0;
    wp->fade_complete = 0;
    if (type == WP_TYPE_SOLID) {
        uint32_t *c = data;
        wp->fade_to_color = *c;
    } else if (type == WP_TYPE_IMAGE) {
        wp->fade_active = 0;
        wp->fade_complete = 1;
    } else if (type == WP_TYPE_GRADIENT && data) {
        wp_color_stop_t *colors = data;
        memcpy(wp->colors, colors, wp->color_count * sizeof(wp_color_stop_t));
        wp->type = WP_TYPE_GRADIENT;
    }
    return 0;
}

wallpaper_t *wp_get_current(int monitor_idx) {
    if (!g_wp || monitor_idx >= WP_MAX_MONITORS) return NULL;
    return &g_wp->wallpapers[monitor_idx];
}
