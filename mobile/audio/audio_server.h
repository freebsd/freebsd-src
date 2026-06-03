#ifndef _AUDIO_SERVER_H_
#define _AUDIO_SERVER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audio server commands (ASRPC protocol)
 */
typedef enum {
    CMD_NONE = 0,
    CMD_VOL_GET,
    CMD_VOL_SET,
    CMD_ROUTE_GET,
    CMD_ROUTE_SET,
    CMD_MUTE_GET,
    CMD_MUTE_SET,
    CMD_EQ_GET,
    CMD_EQ_SET,
    CMD_MAX
} asrpc_cmd_t;

/**
 * Volume structure for IPC
 */
typedef struct {
    int channel; // mixer_channel_t
    int volume;  // 0-100
} asrpc_volume_t;

/**
 * Route structure for IPC
 */
typedef struct {
    int route; // audio_route_t
} asrpc_route_t;

/**
 * Mute structure for IPC
 */
typedef struct {
    int channel; // mixer_channel_t
    int muted;   // boolean
} asrpc_mute_t;

/**
 * EQ structure for IPC
 */
typedef struct {
    int band_idx;
    int freq;
    int gain;
    int q; // Q factor * 100 for integer representation
} asrpc_eq_t;

/**
 * Union of all command arguments
 */
typedef union {
    asrpc_volume_t volume;
    asrpc_route_t route;
    asrpc_mute_t mute;
    asrpc_eq_t eq;
} asrpc_arg_t;

/**
 * ASRPC message structure
 */
typedef struct {
    asrpc_cmd_t cmd;
    asrpc_arg_t arg;
} asrpc_msg_t;

/**
 * Main entry point for audio server
 * @param argc argument count
 * @param argv argument vector
 * @return exit code
 */
int audiod_main(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif // _AUDIO_SERVER_H_