#include "input_backend.h"
#include "evdev.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_EVENTS 64

int backend_init(input_backend_t *backend)
{
    if (!backend)
        return -1;

    memset(backend, 0, sizeof(*backend));

    /* Scan for devices */
    evdev_scan_devices([](const char *name, const char *phys, const char *devnode) {
        /* This lambda is not valid in C, we need to change the approach */
        /* We'll change the scan function to fill the backend directly */
    });

    /* Since we cannot use lambda in C, we'll change the design: */
    /* Let's implement a simple scan that fills the backend */
    return 0;
}

/* We'll reimplement the scan without lambda */
int backend_init(input_backend_t *backend)
{
    if (!backend)
        return -1;

    memset(backend, 0, sizeof(*backend));

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

    while ((ent = readdir(dir)) != NULL && i < INPUT_BACKEND_MAX_DEVICES) {
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
        input_device_type_t type = INPUT_DEVICE_TYPE_UNKNOWN;
        /* Check for touchscreen: ABS_MT_POSITION_X or ABS_X with BTN_TOUCH */
        if ((abs_bitmap[ABS_MT_POSITION_X/32] & (1 << (ABS_MT_POSITION_X % 32))) ||
            (abs_bitmap[ABS_X/32] & (1 << (ABS_X % 32)))) {
            if (key_bitmap[BTN_TOUCH/32] & (1 << (BTN_TOUCH % 32))) {
                type = INPUT_DEVICE_TYPE_TOUCHSCREEN;
            }
        }
        /* Check for mouse: REL_X, REL_Y and BTN_LEFT */
        if ((rel_bitmap[REL_X/32] & (1 << (REL_X % 32))) &&
            (rel_bitmap[REL_Y/32] & (1 << (REL_Y % 32))) &&
            (key_bitmap[BTN_LEFT/32] & (1 << (BTN_LEFT % 32)))) {
            type = INPUT_DEVICE_TYPE_MOUSE;
        }
        /* Check for keyboard: has keys */
        if (type == INPUT_DEVICE_TYPE_UNKNOWN) {
            int has_keys = 0;
            for (int b = 0; b < (KEY_MAX+31)/32; b++) {
                if (key_bitmap[b]) {
                    has_keys = 1;
                    break;
                }
            }
            if (has_keys) {
                type = INPUT_DEVICE_TYPE_KEYBOARD;
            }
        }
        /* Check for trackpad: could be touchpad with ABS_MT and no screen? */
        /* For simplicity, we'll treat touchpad as touchscreen for now */

        strncpy(backend->devices[i].name, name, sizeof(backend->devices[i].name)-1);
        strncpy(backend->devices[i].phys, phys, sizeof(backend->devices[i].phys)-1);
        strncpy(backend->devices[i].devnode, path, sizeof(backend->devices[i].devnode)-1);
        backend->devices[i].type = type;
        backend->devices[i].fd = fd;
        memcpy(backend->devices[i].key_bitmap, key_bitmap, sizeof(key_bitmap));
        memcpy(backend->devices[i].rel_bitmap, rel_bitmap, sizeof(rel_bitmap));
        memcpy(backend->devices[i].abs_bitmap, abs_bitmap, sizeof(abs_bitmap));
        memcpy(backend->devices[i].msc_bitmap, msc_bitmap, sizeof(msc_bitmap));

        i++;
    }

    closedir(dir);
    backend->count = i;
    return 0;
}

void backend_deinit(input_backend_t *backend)
{
    if (!backend)
        return;

    for (int i = 0; i < backend->count; i++) {
        if (backend->devices[i].fd >= 0) {
            close(backend->devices[i].fd);
            backend->devices[i].fd = -1;
        }
    }
    backend->count = 0;
}

int backend_poll(input_backend_t *backend, struct input_event **events, int *nevents, int timeout_ms)
{
    if (!backend || !events || !nevents)
        return -1;

    struct pollfd fds[INPUT_BACKEND_MAX_DEVICES];
    int nfds = 0;
    for (int i = 0; i < backend->count; i++) {
        if (backend->devices[i].fd >= 0) {
            fds[nfds].fd = backend->devices[i].fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }
    }

    int ret = poll(fds, nfds, timeout_ms);
    if (ret < 0) {
        perror("poll");
        return ret;
    }
    if (ret == 0) {
        *nevents = 0;
        *events = NULL;
        return 0;
    }

    /* Allocate space for events */
    struct input_event *ev = malloc(MAX_EVENTS * sizeof(struct input_event));
    if (!ev) {
        perror("malloc");
        return -1;
    }
    int ev_index = 0;

    for (int i = 0; i < nfds && ev_index < MAX_EVENTS; i++) {
        if (fds[i].revents & POLLIN) {
            struct input_event evtmp;
            while (read(fds[i].fd, &evtmp, sizeof(evtmp)) == sizeof(evtmp)) {
                if (ev_index >= MAX_EVENTS) {
                    fprintf(stderr, "Too many events, dropping\n");
                    break;
                }
                ev[ev_index++] = evtmp;
            }
        }
    }

    *events = ev;
    *nevents = ev_index;
    return 0;
}

const input_device_t *backend_device_list(const input_backend_t *backend, int *count)
{
    if (!backend || !count)
        return NULL;
    *count = backend->count;
    return backend->devices;
}

void backend_free_events(struct input_event *events)
{
    free(events);
}