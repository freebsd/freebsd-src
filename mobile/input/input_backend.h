#ifndef INPUT_BACKEND_H
#define INPUT_BACKEND_H

#include <stdint.h>
#include <linux/input.h>

#ifdef __cplusplus
extern "C" {
#endif

#define INPUT_BACKEND_MAX_DEVICES 32

typedef enum {
    INPUT_DEVICE_TYPE_UNKNOWN,
    INPUT_DEVICE_TYPE_KEYBOARD,
    INPUT_DEVICE_TYPE_MOUSE,
    INPUT_DEVICE_TYPE_TOUCHSCREEN,
    INPUT_DEVICE_TYPE_TRACKPAD
} input_device_type_t;

typedef struct {
    char name[256];
    char phys[256];
    char devnode[256];
    input_device_type_t type;
    int fd;
    uint32_t key_bitmap[(KEY_MAX+31)/32];
    uint32_t rel_bitmap[(REL_MAX+31)/32];
    uint32_t abs_bitmap[(ABS_MAX+31)/32];
    uint32_t msc_bitmap[(MSC_MAX+31)/32];
} input_device_t;

typedef struct {
    input_device_t devices[INPUT_BACKEND_MAX_DEVICES];
    int count;
} input_backend_t;

int backend_init(input_backend_t *backend);
void backend_deinit(input_backend_t *backend);
int backend_poll(input_backend_t *backend, struct input_event **events, int *nevents, int timeout_ms);
const input_device_t *backend_device_list(const input_backend_t *backend, int *count);
void backend_free_events(struct input_event *events);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_BACKEND_H */