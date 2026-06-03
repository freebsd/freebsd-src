#include "input_hal.h"
#include "evdev.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>

int hal_init(hal_context_t *ctx)
{
    if (!ctx)
        return -1;
    memset(ctx, 0, sizeof(*ctx));
    return hal_enumerate_devices(ctx);
}

void hal_deinit(hal_context_t *ctx)
{
    if (!ctx)
        return;
    for (int i = 0; i < ctx->count; i++) {
        /* close any open fds? we don't store fds in hal_device_info_t */
    }
    memset(ctx, 0, sizeof(*ctx));
}

int hal_enumerate_devices(hal_context_t *ctx)
{
    if (!ctx)
        return -1;

    ctx->count = 0;
    DIR *dir;
    struct dirent *ent;
    char path[256];
    char name[256] = {0};
    char phys[256] = {0};
    int fd;
    int ret;
    int i = 0;

    dir = opendir("/sys/class/input");
    if (!dir) {
        perror("opendir /sys/class/input");
        return -1;
    }

    while ((ent = readdir(dir)) != NULL && i < 32) {
        if (strncmp(ent->d_name, "event", 5) != 0)
            continue;

        snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", ent->d_name);
        int name_fd = open(path, O_RDONLY);
        if (name_fd >= 0) {
            ret = read(name_fd, name, sizeof(name) - 1);
            if (ret > 0) {
                name[ret] = '\0';
                char *newline = strchr(name, '\n');
                if (newline) *newline = '\0';
            }
            close(name_fd);
        } else {
            name[0] = '\0';
        }

        snprintf(path, sizeof(path), "/sys/class/input/%s/device/phys", ent->d_name);
        int phys_fd = open(path, O_RDONLY);
        if (phys_fd >= 0) {
            ret = read(phys_fd, phys, sizeof(phys) - 1);
            if (ret > 0) {
                phys[ret] = '\0';
                char *newline = strchr(phys, '\n');
                if (newline) *newline = '\0';
            }
            close(phys_fd);
        } else {
            phys[0] = '\0';
        }

        snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
        fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }

        /* Get capabilities */
        uint32_t key_bitmap[(KEY_MAX+31)/32] = {0};
        uint32_t rel_bitmap[(REL_MAX+31)/32] = {0};
        uint32_t abs_bitmap[(ABS_MAX+31)/32] = {0};
        uint32_t msc_bitmap[(MSC_MAX+31)/32] = {0};
        evdev_get_capabilities(fd, key_bitmap, rel_bitmap, abs_bitmap, msc_bitmap);

        /* Determine device type */
        input_type_t type = INPUT_TYPE_UNKNOWN;
        /* Check for touchscreen: ABS_MT_POSITION_X or ABS_X with BTN_TOUCH */
        if ((abs_bitmap[ABS_MT_POSITION_X/32] & (1 << (ABS_MT_POSITION_X % 32))) ||
            (abs_bitmap[ABS_X/32] & (1 << (ABS_X % 32)))) {
            if (key_bitmap[BTN_TOUCH/32] & (1 << (BTN_TOUCH % 32))) {
                type = INPUT_TYPE_TOUCHSCREEN;
            }
        }
        /* Check for mouse: REL_X, REL_Y and BTN_LEFT */
        if ((rel_bitmap[REL_X/32] & (1 << (REL_X % 32))) &&
            (rel_bitmap[REL_Y/32] & (1 << (REL_Y % 32))) &&
            (key_bitmap[BTN_LEFT/32] & (1 << (BTN_LEFT % 32)))) {
            type = INPUT_TYPE_MOUSE;
        }
        /* Check for keyboard: has keys */
        if (type == INPUT_TYPE_UNKNOWN) {
            int has_keys = 0;
            for (int b = 0; b < (KEY_MAX+31)/32; b++) {
                if (key_bitmap[b]) {
                    has_keys = 1;
                    break;
                }
            }
            if (has_keys) {
                type = INPUT_TYPE_KEYBOARD;
            }
        }
        /* Check for trackpad: could be touchpad with ABS_MT and no screen? */
        /* For simplicity, we'll treat touchpad as touchscreen for now */

        strncpy(ctx->devices[i].path, path, sizeof(ctx->devices[i].path)-1);
        strncpy(ctx->devices[i].name, name, sizeof(ctx->devices[i].name)-1);
        strncpy(ctx->devices[i].phys, phys, sizeof(ctx->devices[i].phys)-1);
        ctx->devices[i].type = type;
        memcpy(ctx->devices[i].key_bitmap, key_bitmap, sizeof(key_bitmap));
        memcpy(ctx->devices[i].rel_bitmap, rel_bitmap, sizeof(rel_bitmap));
        memcpy(ctx->devices[i].abs_bitmap, abs_bitmap, sizeof(abs_bitmap));
        memcpy(ctx->devices[i].msc_bitmap, msc_bitmap, sizeof(msc_bitmap));

        i++;
        close(fd);
    }

    closedir(dir);
    ctx->count = i;
    return 0;
}

int hal_open_device(hal_context_t *ctx, const char *path)
{
    /* We don't track open fds in hal_context_t, so just open and return fd */
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("hal_open_device");
    }
    return fd;
}

int hal_close_device(hal_context_t *ctx, int index)
{
    /* Not implemented */
    return 0;
}

const hal_device_info_t *hal_get_device_info(const hal_context_t *ctx, int index)
{
    if (!ctx || index < 0 || index >= ctx->count)
        return NULL;
    return &ctx->devices[index];
}

int hal_get_device_count(const hal_context_t *ctx)
{
    if (!ctx)
        return 0;
    return ctx->count;
}

int hal_monitor_hotplug(hal_context_t *ctx, int timeout_ms)
{
    /* Simple polling method: rescan after timeout */
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long long elapsed = (now.tv_sec - start.tv_sec) * 1000LL + (now.tv_nsec - start.tv_nsec) / 1000000LL;
        if (elapsed >= timeout_ms)
            break;
        usleep(100000); /* 100ms */
    }
    return hal_enumerate_devices(ctx);
}

void hal_set_monitor_callback(hal_context_t *ctx, void (*callback)(int index, int added))
{
    /* Not implemented */
}