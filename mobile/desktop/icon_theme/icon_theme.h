/*
 * Icon Theme System for uOS(m) Desktop
 * Loads icons from theme directories with LRU cache
 * Fallback: generated colored icon with first letter
 */

#ifndef _ICON_THEME_H_
#define _ICON_THEME_H_

#include <stdint.h>
#include "../ui/framebuffer.h"

#define ICON_SIZE_COUNT     9
#define ICON_TYPE_COUNT     6
#define ICON_CACHE_SIZE     256
#define ICON_PATH_MAX       512

#define ICON_SIZE_16        16
#define ICON_SIZE_22        22
#define ICON_SIZE_24        24
#define ICON_SIZE_32        32
#define ICON_SIZE_48        48
#define ICON_SIZE_64        64
#define ICON_SIZE_128       128
#define ICON_SIZE_256       256

#define ICON_CONTEXT_APPS           "apps"
#define ICON_CONTEXT_MIMETYPES      "mimetypes"
#define ICON_CONTEXT_DEVICES        "devices"
#define ICON_CONTEXT_ACTIONS        "actions"
#define ICON_CONTEXT_PANEL          "panel"
#define ICON_CONTEXT_STATUS         "status"

typedef enum {
    ICON_TYPE_APP,
    ICON_TYPE_MIMETYPE,
    ICON_TYPE_DEVICE,
    ICON_TYPE_ACTION,
    ICON_TYPE_PANEL,
    ICON_TYPE_STATUS
} icon_type_t;

typedef struct icon {
    char name[128];
    int size;
    icon_type_t type;
    uint32_t *pixel_data;
    int width;
    int height;
    uint32_t avg_color;
    char first_letter;
    uint32_t ref_count;
    uint64_t last_used;
    struct icon *next;
} icon_t;

typedef struct {
    icon_t *cache[ICON_CACHE_SIZE];
    int cache_count;
    uint64_t access_counter;
    int hicolor_only;
    char theme_name[64];
} icon_theme_t;

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int loaded;
} icon_handle_t;

int icon_theme_init(const char *theme_name);
void icon_theme_shutdown(void);
icon_handle_t *icon_load(const char *name, int size, icon_type_t type);
void icon_unload(icon_handle_t *handle);
void icon_cache_init(int cache_size);
void icon_cache_clear(void);
icon_handle_t *icon_load_fallback(const char *name, int size);
int icon_get_avg_color(icon_t *icon);
icon_handle_t *icon_theme_get_default(icon_type_t type, int size);
void icon_cache_evict_lru(void);

#endif /* _ICON_THEME_H_ */
