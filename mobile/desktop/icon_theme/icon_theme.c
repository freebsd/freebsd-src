/*
 * Icon Theme Implementation
 */

#include "icon_theme.h"
#include "../ui/framebuffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static icon_theme_t *g_theme = NULL;
static icon_handle_t g_default_icons[ICON_TYPE_COUNT][ICON_SIZE_COUNT];
static const int g_icon_sizes[ICON_SIZE_COUNT] = {
    ICON_SIZE_16, ICON_SIZE_22, ICON_SIZE_24, ICON_SIZE_32,
    ICON_SIZE_48, ICON_SIZE_64, ICON_SIZE_128, ICON_SIZE_256, 256
};

static uint32_t icon_hash(const char *name, int size, icon_type_t type) {
    uint32_t h = 2166136261u;
    while (*name) { h ^= (uint8_t)*name++; h *= 16777619u; }
    h ^= (uint32_t)size;
    h ^= (uint32_t)type * 2654435761u;
    return h % ICON_CACHE_SIZE;
}

static icon_handle_t *icon_make_fallback(const char *name, int size) {
    icon_t *ico = calloc(1, sizeof(icon_t));
    if (!ico) return NULL;
    strncpy(ico->name, name, sizeof(ico->name) - 1);
    ico->size = size;
    ico->width = size;
    ico->height = size;
    ico->pixel_data = calloc(size * size, sizeof(uint32_t));
    if (!ico->pixel_data) { free(ico); return NULL; }
    int r = (name[0] ? ((unsigned char)name[0]) : 65) % 6;
    uint32_t palette[6] = {0xFF4A90D9, 0xFF50C878, 0xFFFF6B6B, 0xFFFFA500, 0xFF9B59B6, 0xFF1ABC9C};
    uint32_t bg = palette[r];
    uint32_t fg = 0xFFFFFFFF;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int dx = x - size / 2, dy = y - size / 2;
            int dist = (int)sqrtf((float)(dx * dx + dy * dy));
            int radius = size / 2 - 2;
            if (dist > radius) ico->pixel_data[y * size + x] = 0x00000000;
            else ico->pixel_data[y * size + x] = bg;
        }
    }
    for (int y = size / 3; y < 2 * size / 3; y++) {
        for (int x = size / 4; x < 3 * size / 4; x++) {
            int dx = x - size / 2, dy = y - size / 2;
            int dist = (int)sqrtf((float)(dx * dx + dy * dy));
            int radius = size / 3;
            if (dist <= radius) ico->pixel_data[y * size + x] = fg;
        }
    }
    ico->avg_color = bg;
    ico->first_letter = name[0] ? name[0] : '?';
    ico->ref_count = 1;
    ico->last_used = 0;
    uint32_t hsh = icon_hash(name, size, ICON_TYPE_APP);
    if (g_theme) {
        if (g_theme->cache[hsh]) icon_cache_evict_lru();
        g_theme->cache[hsh] = ico;
    }
    icon_handle_t *handle = malloc(sizeof(icon_handle_t));
    if (!handle) { free(ico->pixel_data); free(ico); return NULL; }
    handle->pixels = ico->pixel_data;
    handle->width = ico->width;
    handle->height = ico->height;
    handle->loaded = 1;
    return handle;
}

int icon_theme_init(const char *theme_name) {
    if (!g_theme) {
        g_theme = calloc(1, sizeof(icon_theme_t));
        if (!g_theme) return -1;
        icon_cache_init(ICON_CACHE_SIZE);
    }
    strncpy(g_theme->theme_name, theme_name ? theme_name : "hicolor", sizeof(g_theme->theme_name) - 1);
    g_theme->hicolor_only = 1;
    g_theme->loaded = 1;
    for (int t = 0; t < ICON_TYPE_COUNT; t++) {
        for (int s = 0; s < ICON_SIZE_COUNT; s++) {
            icon_handle_t *h = icon_load_fallback("", g_icon_sizes[s]);
            if (h) g_default_icons[t][s] = *h;
        }
    }
    return 0;
}

void icon_theme_shutdown(void) {
    if (!g_theme) return;
    icon_cache_clear();
    free(g_theme);
    g_theme = NULL;
}

icon_handle_t *icon_load(const char *name, int size, icon_type_t type) {
    if (!g_theme || !name || !name[0]) return icon_load_fallback("?", size);
    icon_t *ico = calloc(1, sizeof(icon_t));
    if (!ico) return NULL;
    strncpy(ico->name, name, sizeof(ico->name) - 1);
    ico->size = size;
    ico->type = type;
    ico->pixel_data = calloc(size * size, sizeof(uint32_t));
    if (!ico->pixel_data) { free(ico); return NULL; }
    if ((int)strlen(name) == 1) {
        int r = ((unsigned char)name[0]) % 6;
        uint32_t palette[6] = {0xFF4A90D9, 0xFF50C878, 0xFFFF6B6B, 0xFFFFA500, 0xFF9B59B6, 0xFF1ABC9C};
        uint32_t bg = palette[r];
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                int dx = x - size / 2, dy = y - size / 2;
                if ((int)sqrtf((float)(dx * dx + dy * dy)) > size / 2 - 2)
                    ico->pixel_data[y * size + x] = 0x00000000;
                else ico->pixel_data[y * size + x] = bg;
            }
        }
        if (size >= 24) {
            uint32_t fg = 0xFFFFFFFF;
            for (int y = size / 3; y < 2 * size / 3; y++) {
                for (int x = size / 4; x < 3 * size / 4; x++) {
                    int dx = x - size / 2, dy = y - size / 2;
                    if ((int)sqrtf((float)(dx * dx + dy * dy)) <= size / 3)
                        ico->pixel_data[y * size + x] = fg;
                }
            }
        }
        ico->avg_color = bg;
        ico->first_letter = name[0];
    } else {
        for (int i = 0; i < size * size; i++) ico->pixel_data[i] = 0x00000000;
        ico->avg_color = 0xFF808080;
        ico->first_letter = name[0];
    }
    ico->width = size;
    ico->height = size;
    ico->ref_count = 1;
    ico->last_used = 0;
    uint32_t hsh = icon_hash(name, size, type);
    if (g_theme->cache[hsh]) icon_cache_evict_lru();
    g_theme->cache[hsh] = ico;
    icon_handle_t *handle = malloc(sizeof(icon_handle_t));
    if (!handle) { free(ico->pixel_data); free(ico); return NULL; }
    handle->pixels = ico->pixel_data;
    handle->width = size;
    handle->height = size;
    handle->loaded = 1;
    return handle;
}

void icon_unload(icon_handle_t *handle) {
    if (!handle) return;
    free(handle);
}

void icon_cache_init(int cache_size) {
    if (g_theme) memset(g_theme->cache, 0, sizeof(g_theme->cache));
}

void icon_cache_clear(void) {
    if (!g_theme) return;
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (g_theme->cache[i]) {
            if (g_theme->cache[i]->pixel_data) free(g_theme->cache[i]->pixel_data);
            free(g_theme->cache[i]);
            g_theme->cache[i] = NULL;
        }
    }
    g_theme->cache_count = 0;
}

icon_handle_t *icon_load_fallback(const char *name, int size) {
    return icon_load(name, size, ICON_TYPE_APP);
}

int icon_get_avg_color(icon_t *icon) {
    return icon ? icon->avg_color : 0xFF808080;
}

icon_handle_t *icon_theme_get_default(icon_type_t type, int size) {
    (void)type;
    int idx = 0;
    for (int i = 0; i < ICON_SIZE_COUNT; i++) {
        if (g_icon_sizes[i] == size) { idx = i; break; }
    }
    return &g_default_icons[0][idx];
}

void icon_cache_evict_lru(void) {
    if (!g_theme) return;
    uint64_t oldest = (uint64_t)-1;
    int oldest_idx = -1;
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (g_theme->cache[i] && g_theme->cache[i]->last_used < oldest) {
            oldest = g_theme->cache[i]->last_used;
            oldest_idx = i;
        }
    }
    if (oldest_idx >= 0 && g_theme->cache[oldest_idx]) {
        icon_t *ico = g_theme->cache[oldest_idx];
        if (ico->pixel_data) free(ico->pixel_data);
        free(ico);
        g_theme->cache[oldest_idx] = NULL;
    }
}
