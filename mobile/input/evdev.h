#ifndef EVDEV_H
#define EVDEV_H

#include <linux/input.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int evdev_open(const char *path);
void evdev_close(int fd);
struct input_event evdev_read_event(int fd, int timeout_ms);
int evdev_get_capabilities(int fd, uint32_t *key_bitmap, uint32_t *rel_bitmap, uint32_t *abs_bitmap, uint32_t *msc_bitmap);
int evdev_get_abs_info(int fd, unsigned int code, struct input_absinfo *info);
const char *evdev_get_key_name(unsigned int code);
int evdev_scan_devices(void (**devices)(const char *name, const char *phys, const char *devnode));

#ifdef __cplusplus
}
#endif

#endif /* EVDEV_H */