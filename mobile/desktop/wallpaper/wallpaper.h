/*
 * Wallpaper Engine for uOS(m) Desktop
 * Handles static, gradient, solid, and animated wallpapers
 * Per-monitor support with fade transitions
 */

#ifndef _WALLPAPER_H_
#define _WALLPAPER_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"

#define WP_MAX_COLORS    16
#define WP_MAX_MONITORS  4
#define WP_FADE_DURATION 500

typedef enum {
    WP_TYPE_SOLID,
    WP_TYPE_GRADIENT,
    WP_TYPE_IMAGE,
    WP_TYPE_ANIMATED,
    WP_TYPE_PARTICLES,
    WP_TYPE_WAVES
} wp_type_t;

typedef enum {
    WP_ANIM_NONE,
    WP_ANIM_PARTICLES,
    WP_ANIM_WAVES,
    WP_ANIM_GRADIENT_SHIFT
} wp_anim_type_t;

typedef struct {
    uint8_t r, g, b, a;
    float position;
} wp_color_stop_t;

typedef struct {
    wp_type_t type;
    wp_anim_type_t anim_type;
    wp_color_stop_t colors[WP_MAX_COLORS];
    int color_count;
    char image_path[256];
    uint32_t *image_data;
    int image_width;
    int image_height;
    int anim_running;
    uint64_t anim_start_time;
    float gradient_offset;
    float particles[64][3];
    int particle_count;
    float wave_phase;
    int fade_active;
    uint32_t fade_from_color;
    uint32_t fade_to_color;
    uint64_t fade_start;
    int fade_complete;
} wallpaper_t;

typedef struct {
    wallpaper_t wallpapers[WP_MAX_MONITORS];
    int monitor_count;
    uint32_t output_width;
    uint32_t output_height;
    int initialized;
} wp_engine_t;

wp_engine_t *wp_init(compositor_t *comp);
void wp_shutdown(void);
int wp_set_image(const char *path, int monitor_idx);
int wp_set_gradient(wp_color_stop_t *colors, int num_stops, int monitor_idx);
int wp_set_solid(uint32_t color, int monitor_idx);
int wp_set_animated(wp_anim_type_t anim_type, int monitor_idx);
void wp_render(layer_t *bg_layer, int monitor_idx);
void wp_render_all(void);
void wp_update_animations(void);
int wp_fade_to(wp_type_t type, void *data, int monitor_idx);
wallpaper_t *wp_get_current(int monitor_idx);

#endif /* _WALLPAPER_H_ */
