/*
 * Compositor Core Implementation
 * BSD-licensed
 */

#include "compositor_core.h"
#include <string.h>

static compositor_t g_compositor;

int comp_init(void) {
    memset(&g_compositor, 0, sizeof(g_compositor));
    g_compositor.layer_count = 0;
    g_compositor.cursor_visible = 0;
    
    for (int i = 0; i < MAX_LAYERS; i++) {
        g_compositor.layers[i].surfaces = NULL;
        g_compositor.layers[i].surface_count = 0;
    }
    
    g_compositor.initialized = 1;
    return 0;
}

void comp_shutdown(void) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        layer_t *layer = &g_compositor.layers[i];
        if (layer->surfaces) {
            for (int j = 0; j < layer->surface_count; j++) {
                if (layer->surfaces[j].pixel_data) {
                    free(layer->surfaces[j].pixel_data);
                }
            }
            free(layer->surfaces);
            layer->surfaces = NULL;
        }
        layer->surface_count = 0;
    }
    g_compositor.layer_count = 0;
    g_compositor.initialized = 0;
}

layer_t *comp_create_layer(int type) {
    if (g_compositor.layer_count >= MAX_LAYERS) return NULL;
    
    layer_t *layer = &g_compositor.layers[g_compositor.layer_count];
    layer->layer_id = g_compositor.layer_count;
    layer->type = type;
    layer->surfaces = NULL;
    layer->surface_count = 0;
    layer->surface_capacity = 0;
    
    return layer;
}

void comp_destroy_layer(layer_t *layer) {
    if (!layer) return;
    layer->type = -1;
}

surface_t *comp_add_surface(layer_t *layer, int x, int y, int width, int height) {
    if (!layer) return NULL;
    
    if (layer->surface_count >= layer->surface_capacity) {
        int new_cap = layer->surface_capacity ? layer->surface_capacity * 2 : 4;
        surface_t *new_surfaces = realloc(layer->surfaces, new_cap * sizeof(surface_t));
        if (!new_surfaces) return NULL;
        layer->surfaces = new_surfaces;
        layer->surface_capacity = new_cap;
    }
    
    surface_t *surf = &layer->surfaces[layer->surface_count];
    memset(surf, 0, sizeof(surface_t));
    surf->x = x;
    surf->y = y;
    surf->width = width;
    surf->height = height;
    surf->dirty = 1;
    
    size_t pixel_size = width * height * sizeof(uint32_t);
    surf->pixel_data = malloc(pixel_size);
    if (surf->pixel_data) {
        memset(surf->pixel_data, 0, pixel_size);
    }
    
    layer->surface_count++;
    return surf;
}

void comp_remove_surface(layer_t *layer, surface_t *surface) {
    (void)layer;
    (void)surface;
}

void comp_commit(void) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        layer_t *layer = &g_compositor.layers[i];
        for (int j = 0; j < layer->surface_count; j++) {
            layer->surfaces[j].dirty = 0;
        }
    }
}

static int compare_zorder(const void *a, const void *b) {
    const layer_t *la = a, *lb = b;
    return la->surfaces ? la->surfaces[0].zorder - lb->surfaces[0].zorder : 0;
}

void comp_frame(void) {
    static uint64_t frame_start = 0;
    frame_start++;
    
    if (frame_start % 60 == 0) {
    }
}

void comp_set_cursor(int x, int y, uint32_t *data) {
    g_compositor.cursor_surface.x = x;
    g_compositor.cursor_surface.y = y;
    g_compositor.cursor_visible = 1;
}

void comp_show_cursor(void) {
    g_compositor.cursor_visible = 1;
}

void comp_hide_cursor(void) {
    g_compositor.cursor_visible = 0;
}

layer_t *comp_get_layer(int type) {
    for (int i = 0; i < MAX_LAYERS; i++) {
        if (g_compositor.layers[i].type == type) {
            return &g_compositor.layers[i];
        }
    }
    return NULL;
}