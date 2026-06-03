/*
 * Compositor Core - Layer management and composition
 * BSD-licensed
 */

#ifndef _COMPOSITOR_CORE_H_
#define _COMPOSITOR_CORE_H_

#include <stdint.h>
#include "framebuffer.h"

#define LAYER_BACKGROUND  0
#define LAYER_WINDOW      1
#define LAYER_OVERLAY     2
#define LAYER_CURSOR      3
#define MAX_LAYERS        64

typedef struct {
    int x, y;
    int width, height;
    int zorder;
    uint32_t *pixel_data;
    int dirty;
    int visible;
} surface_t;

typedef struct {
    int layer_id;
    int type;
    surface_t *surfaces;
    int surface_count;
    int surface_capacity;
} layer_t;

typedef struct {
    layer_t layers[MAX_LAYERS];
    int layer_count;
    surface_t cursor_surface;
    int cursor_visible;
    int initialized;
} compositor_t;

int comp_init(void);
void comp_shutdown(void);
layer_t *comp_create_layer(int type);
void comp_destroy_layer(layer_t *layer);
surface_t *comp_add_surface(layer_t *layer, int x, int y, int width, int height);
void comp_remove_surface(layer_t *layer, surface_t *surface);
void comp_commit(void);
void comp_frame(void);
void comp_set_cursor(int x, int y, uint32_t *data);
void comp_show_cursor(void);
void comp_hide_cursor(void);
layer_t *comp_get_layer(int type);

#endif /* _COMPOSITOR_CORE_H_ */