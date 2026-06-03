/*
 * Audio Daemon - Implementation
 * BSD-style audio management service
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
#include "audiod.h"

static int audiod_running = 0;
static int audiod_socket_fd = -1;
static struct audio_volume volumes[4];
static int current_route = AUDIO_ROUTE_SPEAKER;
static struct audio_device pcm_devices[4];

static void
audiod_sigterm_handler(int sig __unused)
{
    audiod_running = 0;
}

static int
write_mixer(const char *control, int value)
{
    int fd;
    char path[256];
    char buf[16];
    int len;

    snprintf(path, sizeof(path), "/dev/mixer/%s", control);
    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -1;

    len = snprintf(buf, sizeof(buf), "%d\n", value);
    if (write(fd, buf, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

static int
read_mixer(const char *control, int *value)
{
    int fd;
    char path[256];
    char buf[16];
    int len;

    snprintf(path, sizeof(path), "/dev/mixer/%s", control);
    fd = open(path, O_RDONLY);
    if (fd == -1)
        return -1;

    len = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (len > 0) {
        buf[len] = '\0';
        *value = atoi(buf);
        return 0;
    }
    return -1;
}

int
audiod_set_volume(int type, int level)
{
    char control[32];

    if (type < 0 || type > 3)
        return -1;

    if (level < 0)
        level = 0;
    if (level > 100)
        level = 100;

    volumes[type].type = type;
    volumes[type].level = level;
    volumes[type].muted = 0;

    switch (type) {
    case AUDIO_TYPE_MASTER:
        snprintf(control, sizeof(control), "master");
        break;
    case AUDIO_TYPE_MEDIA:
        snprintf(control, sizeof(control), "pcm");
        break;
    case AUDIO_TYPE_CALL:
        snprintf(control, sizeof(control), "phone");
        break;
    case AUDIO_TYPE_NOTIFICATION:
        snprintf(control, sizeof(control), "beep");
        break;
    default:
        return -1;
    }

    write_mixer(control, level * 255 / 100);
    syslog(LOG_DEBUG, "Set volume %d to %d%%", type, level);
    return 0;
}

int
audiod_get_volume(int type)
{
    if (type >= 0 && type < 4)
        return volumes[type].level;
    return -1;
}

int
audiod_mute(int type)
{
    if (type >= 0 && type < 4) {
        volumes[type].muted = 1;
        return audiod_set_volume(type, 0);
    }
    return -1;
}

int
audiod_unmute(int type)
{
    if (type >= 0 && type < 4) {
        volumes[type].muted = 0;
        return 0;
    }
    return -1;
}

int
audiod_set_route(int route)
{
    if (route < AUDIO_ROUTE_SPEAKER || route > AUDIO_ROUTE_BLUETOOTH)
        return -1;

    current_route = route;

    switch (route) {
    case AUDIO_ROUTE_SPEAKER:
        write_mixer("speaker", 100);
        write_mixer("headphone", 0);
        break;
    case AUDIO_ROUTE_HEADPHONE:
        write_mixer("speaker", 0);
        write_mixer("headphone", 100);
        break;
    case AUDIO_ROUTE_BLUETOOTH:
        write_mixer("bluetooth", 100);
        break;
    }

    syslog(LOG_INFO, "Audio route set to %d", route);
    return 0;
}

int
audiod_pcm_open(int card, int device)
{
    if (card < 0 || card >= 4)
        return -1;

    pcm_devices[card].card = card;
    pcm_devices[card].device = device;
    pcm_devices[card].active = 1;

    snprintf(pcm_devices[card].name, sizeof(pcm_devices[card].name),
             "pcm%dD%d", card, device);

    syslog(LOG_INFO, "PCM device %s opened", pcm_devices[card].name);
    return 0;
}

int
audiod_pcm_close(int card, int device)
{
    if (card < 0 || card >= 4)
        return -1;

    if (pcm_devices[card].device != device)
        return -1;

    pcm_devices[card].active = 0;
    syslog(LOG_INFO, "PCM device pcm%dD%d closed", card, device);
    return 0;
}

int
audiod_pcm_write(int card, int device, const void *data, size_t len)
{
    int fd;
    char path[64];

    if (card < 0 || card >= 4 || !pcm_devices[card].active)
        return -1;

    snprintf(path, sizeof(path), "/dev/pcm%dD%d", card, device);
    fd = open(path, O_WRONLY);
    if (fd == -1)
        return -1;

    write(fd, data, len);
    close(fd);
    return 0;
}

int
audiod_init(void)
{
    struct sigaction sa;
    struct sockaddr_un addr;

    openlog("audiod", LOG_PID, LOG_DAEMON);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = audiod_sigterm_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* Initialize default volumes */
    audiod_set_volume(AUDIO_TYPE_MASTER, 80);
    audiod_set_volume(AUDIO_TYPE_MEDIA, 100);
    audiod_set_volume(AUDIO_TYPE_CALL, 100);
    audiod_set_volume(AUDIO_TYPE_NOTIFICATION, 70);

    /* Create control socket */
    audiod_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (audiod_socket_fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, AUDIOD_SOCKET_PATH, sizeof(addr.sun_path) - 1);
        unlink(AUDIOD_SOCKET_PATH);
        if (bind(audiod_socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            close(audiod_socket_fd);
            audiod_socket_fd = -1;
        }
    }

    audiod_running = 1;
    syslog(LOG_INFO, "Audio daemon initialized");
    return 0;
}

void
audiod_shutdown(void)
{
    int i;

    audiod_running = 0;

    /* Close PCM devices */
    for (i = 0; i < 4; i++) {
        if (pcm_devices[i].active) {
            audiod_pcm_close(i, pcm_devices[i].device);
        }
    }

    if (audiod_socket_fd >= 0) {
        close(audiod_socket_fd);
        unlink(AUDIOD_SOCKET_PATH);
    }

    closelog();
}

int
audiod_main(int argc __unused, char **argv __unused)
{
    struct pollfd pfd;

    if (audiod_init() < 0)
        return 1;

    pfd.fd = audiod_socket_fd;
    pfd.events = POLLIN;

    while (audiod_running) {
        poll(&pfd, 1, 5000);
        /* Handle volume changes, routing etc. */
    }

    audiod_shutdown();
    return 0;
}