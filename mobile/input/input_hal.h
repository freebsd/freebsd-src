#ifndef INPUT_HAL_H
#define INPUT_HAL_H

#include <stdint.h>
#include <linux/input.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    INPUT_TYPE_UNKNOWN,
    INPUT_TYPE_TOUCHSCREEN,
    INPUT_TYPE_KEYBOARD,
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_TRACKPAD
} input_type_t;

typedef struct {
    char path[256];
    char name[256];
    char phys[256];
    input_type_t type;
    uint32_t key_bitmap[(KEY_MAX+31)/32];
    uint32_t rel_bitmap[(REL_MAX+31)/32];
    uint32_t abs_bitmap[(ABS_MAX+31)/32];
    uint32_t msc_bitmap[(MSC_MAX+31)/32];
} hal_device_info_t;

typedef struct {
    hal_device_info_t devices[32];
    int count;
} hal_context_t;

int hal_init(hal_context_t *ctx);
void hal_deinit(hal_context_t *ctx);
int hal_enumerate_devices(hal_context_t *ctx);
int hal_open_device(hal_context_t *ctx, const char *path);
int hal_close_device(hal_context_t *ctx, int index);
const hal_device_info_t *hal_get_device_info(const hal_context_t *ctx, int index);
int hal_get_device_count(const hal_context_t *ctx);
int hal_monitor_hotplug(hal_context_t *ctx, int timeout_ms); /* blocking, returns when device added/removed */
void hal_set_monitor_callback(hal_context_t *ctx, void (*callback)(int index, int added));

#ifdef __cplusplus
}
#endif

#endif /* INPUT_HAL_H */