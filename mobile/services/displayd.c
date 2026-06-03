/*
 * Display Daemon - Implementation
 * BSD-style display management service
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include "displayd.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "../ui/input.h"

static int displayd_running = 0;
static int displayd_blank_timer = 0;
static int displayd_brightness = BRIGHTNESS_DEFAULT;
static int displayd_dpms_state = DISPLAY_STATE_ON;
static int display_fd = -1;

static void
displayd_sigterm_handler(int sig __unused)
{
    displayd_running = 0;
}

static int
write_sysfs(const char *path, const char *value)
{
    int fd;
    int len;

    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -1;

    len = strlen(value);
    if (write(fd, value, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int
read_sysfs(const char *path, char *buf, size_t bufsz)
{
    int fd;
    int len;

    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;

    len = read(fd, buf, bufsz - 1);
    if (len > 0)
        buf[len] = '\0';
    close(fd);
    return len;
}

int
displayd_init(void)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("displayd", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = displayd_sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Initialize framebuffer */
    if (fb_init() < 0) {
        syslog(LOG_ERR, "Failed to initialize framebuffer");
        return -1;
    }

    /* Initialize compositor */
    if (comp_init() < 0) {
        syslog(LOG_ERR, "Failed to initialize compositor");
        return -1;
    }

    /* Initialize input system */
    if (input_init() < 0) {
        syslog(LOG_ERR, "Failed to initialize input");
        return -1;
    }

    /* Create control socket */
    display_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (display_fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, DISPLAYD_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(DISPLAYD_SOCKET_PATH);
        if (bind(display_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(display_fd);
            display_fd = -1;
        }
    }

    displayd_running = 1;
    syslog(LOG_INFO, "Display daemon initialized");
    return 0;
}

void
displayd_shutdown(void)
{
    displayd_running = 0;

    if (display_fd >= 0) {
        close(display_fd);
        unlink(DISPLAYD_SOCKET_PATH);
    }

    input_shutdown();
    comp_shutdown();

    closelog();
}

int
displayd_set_brightness(int level)
{
    char buf[16];

    if (level < 0)
        level = 0;
    if (level > BRIGHTNESS_MAX)
        level = BRIGHTNESS_MAX;

    displayd_brightness = level;

    snprintf(buf, sizeof(buf), "%d", displayd_brightness);
    if (write_sysfs("/sys/class/backlight/panel/brightness", buf) < 0) {
        /* Fallback to direct write */
        syslog(LOG_DEBUG, "Could not write brightness to sysfs");
    }

    return 0;
}

int
displayd_blank(void)
{
    if (displayd_dpms_state != DISPLAY_STATE_ON)
        return 0;

    fb_clear(COLOR_BLACK);
    displayd_dpms_state = DISPLAY_STATE_BLANK;
    displayd_blank_timer = 0;

    syslog(LOG_INFO, "Display blanked");
    return 0;
}

int
displayd_unblank(void)
{
    if (displayd_dpms_state == DISPLAY_STATE_ON)
        return 0;

    displayd_dpms_state = DISPLAY_STATE_ON;
    displayd_blank_timer = 0;

    syslog(LOG_INFO, "Display unblanked");
    return 0;
}

void
displayd_hotplug_check(void)
{
    char buf[256];
    static int last_connected = -1;
    int connected = 0;

    /* Check for display hotplug via sysfs */
    if (read_sysfs("/sys/class/drm/card0-HDMI-A-1/status", buf, sizeof(buf)) > 0) {
        if (strncmp(buf, "connected", 9) == 0)
            connected = 1;
    }

    if (connected != last_connected) {
        syslog(LOG_INFO, "Display hotplug: %s", connected ? "connected" : "disconnected");
        last_connected = connected;
        /* Trigger reconfiguration */
        comp_commit();
    }
}

void
displayd_suspend(void)
{
    syslog(LOG_INFO, "Suspending display");
    displayd_dpms_state = DISPLAY_STATE_SUSPEND;
    write_sysfs("/sys/class/graphics/fb0/blank", "4");
}

void
displayd_resume(void)
{
    syslog(LOG_INFO, "Resuming display");
    displayd_dpms_state = DISPLAY_STATE_ON;
    write_sysfs("/sys/class/graphics/fb0/blank", "0");
    comp_commit();
}

int
displayd_main(int argc __unused, char **argv __unused)
{
    struct pollfd pfd[2];
    int input_event_fd = 0;
    int timeout_ms = 1000; /* 1 second for blank timer */

    if (displayd_init() < 0)
        return 1;

    pfd[0].fd = display_fd;
    pfd[0].events = POLLIN;
    pfd[1].fd = input_event_fd;
    pfd[1].events = POLLIN;

    while (displayd_running) {
        int n;

        /* Handle blank timeout */
        displayd_blank_timer++;
        if (displayd_blank_timer >= 60 && displayd_dpms_state == DISPLAY_STATE_ON) {
            displayd_blank();
        }

        /* Poll for input and control events */
        n = poll(pfd, 2, timeout_ms);

        if (n > 0) {
            if (pfd[1].revents & POLLIN) {
                input_poll();
                displayd_blank_timer = 0;
                if (displayd_dpms_state == DISPLAY_STATE_BLANK) {
                    displayd_unblank();
                }
            }
        }

        /* Compositor frame */
        comp_frame();
    }

    displayd_shutdown();
    return 0;
}