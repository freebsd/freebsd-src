#include "audio_server.h"
#include "mixer.h"
#include "route.h"
#include "dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>

#define SOCKET_PATH "/var/run/audio.sock"
#define BACKLOG 5

static mixer_t *g_mixer = NULL;
static audio_route_t *g_route = NULL;
static dsp_t *g_dsp = NULL;

/**
 * Handle volume get command
 */
static int handle_vol_get(int client_fd, const asrpc_msg_t *msg) {
    asrpc_volume_t resp;
    resp.channel = msg->arg.volume.channel;
    resp.volume = mixer_get_volume(g_mixer, (mixer_channel_t)resp.channel);
    write(client_fd, &resp, sizeof(resp));
    return 0;
}

/**
 * Handle volume set command
 */
static int handle_vol_set(int client_fd, const asrpc_msg_t *msg) {
    int ret = mixer_set_volume(g_mixer, 
                              (mixer_channel_t)msg->arg.volume.channel,
                              msg->arg.volume.volume);
    asrpc_volume_t resp;
    resp.channel = msg->arg.volume.channel;
    resp.volume = msg->arg.volume.volume;
    write(client_fd, &resp, sizeof(resp));
    return ret;
}

/**
 * Handle route get command
 */
static int handle_route_get(int client_fd, const asrpc_msg_t *msg) {
    asrpc_route_t resp;
    resp.route = route_get_active(g_route);
    write(client_fd, &resp, sizeof(resp));
    return 0;
}

/**
 * Handle route set command
 */
static int handle_route_set(int client_fd, const asrpc_msg_t *msg) {
    int ret = route_set(g_route, (audio_route_t)msg->arg.route.route);
    asrpc_route_t resp;
    resp.route = msg->arg.route.route;
    write(client_fd, &resp, sizeof(resp));
    return ret;
}

/**
 * Handle mute get command
 */
static int handle_mute_get(int client_fd, const asrpc_msg_t *msg) {
    asrpc_mute_t resp;
    resp.channel = msg->arg.mute.channel;
    resp.muted = mixer_get_mute(g_mixer, (mixer_channel_t)resp.channel);
    write(client_fd, &resp, sizeof(resp));
    return 0;
}

/**
 * Handle mute set command
 */
static int handle_mute_set(int client_fd, const asrpc_msg_t *msg) {
    int ret = mixer_set_mute(g_mixer,
                            (mixer_channel_t)msg->arg.mute.channel,
                            msg->arg.mute.muted);
    asrpc_mute_t resp;
    resp.channel = msg->arg.mute.channel;
    resp.muted = msg->arg.mute.muted;
    write(client_fd, &resp, sizeof(resp));
    return ret;
}

/**
 * Handle EQ get command
 */
static int handle_eq_get(int client_fd, const asrpc_msg_t *msg) {
    asrpc_eq_t resp;
    resp.band_idx = msg->arg.eq.band_idx;
    // In a real implementation, we would get from dsp
    resp.freq = 1000; // dummy
    resp.gain = 0;
    resp.q = 100; // 1.00
    write(client_fd, &resp, sizeof(resp));
    return 0;
}

/**
 * Handle EQ set command
 */
static int handle_eq_set(int client_fd, const asrpc_msg_t *msg) {
    int ret = dsp_eq_band_freq(g_dsp,
                              msg->arg.eq.band_idx,
                              msg->arg.eq.freq,
                              msg->arg.eq.gain,
                              msg->arg.eq.q / 100.0f);
    asrpc_eq_t resp;
    resp = msg->arg.eq;
    write(client_fd, &resp, sizeof(resp));
    return ret;
}

/**
 * Main client handling loop
 */
static void handle_client(int client_fd) {
    asrpc_msg_t msg;
    ssize_t n;

    while ((n = read(client_fd, &msg, sizeof(msg))) > 0) {
        switch (msg.cmd) {
            case CMD_VOL_GET:
                handle_vol_get(client_fd, &msg);
                break;
            case CMD_VOL_SET:
                handle_vol_set(client_fd, &msg);
                break;
            case CMD_ROUTE_GET:
                handle_route_get(client_fd, &msg);
                break;
            case CMD_ROUTE_SET:
                handle_route_set(client_fd, &msg);
                break;
            case CMD_MUTE_GET:
                handle_mute_get(client_fd, &msg);
                break;
            case CMD_MUTE_SET:
                handle_mute_set(client_fd, &msg);
                break;
            case CMD_EQ_GET:
                handle_eq_get(client_fd, &msg);
                break;
            case CMD_EQ_SET:
                handle_eq_set(client_fd, &msg);
                break;
            default:
                // Unknown command
                break;
        }
    }

    close(client_fd);
}

/**
 * Main audio server daemon
 */
int audiod_main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    // Initialize subsystems
    g_mixer = mixer_init();
    g_route = route_init();
    g_dsp = dsp_init();

    if (!g_mixer || !g_route || !g_dsp) {
        fprintf(stderr, "Failed to initialize audio subsystems\n");
        return 1;
    }

    // Create UNIX socket
    int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Remove any existing socket
    unlink(SOCKET_PATH);

    if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return 1;
    }

    if (listen(sock_fd, BACKLOG) < 0) {
        perror("listen");
        close(sock_fd);
        unlink(SOCKET_PATH);
        return 1;
    }

    printf("Audio server listening on %s\n", SOCKET_PATH);

    // Main loop
    while (1) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        handle_client(client_fd);
    }

    // Cleanup (unreachable)
    close(sock_fd);
    unlink(SOCKET_PATH);
    mixer_free(g_mixer);
    route_free(g_route);
    dsp_free(g_dsp);

    return 0;
}