/*
 * UOS Compositor - Display Output
 * BSD-licensed
 */

#ifndef _UOS_OUTPUT_H_
#define _UOS_OUTPUT_H_

#include <stdint.h>
#include <stdbool.h>
#include "protocols.h"
#include "../gfx/drm.h"

/* Output transform flags */
#define OUTPUT_TRANSFORM_NORMAL        0
#define OUTPUT_TRANSFORM_90            1
#define OUTPUT_TRANSFORM_180           2
#define OUTPUT_TRANSFORM_270           3
#define OUTPUT_TRANSFORM_FLIPPED         4
#define OUTPUT_TRANSFORM_FLIPPED_90      5
#define OUTPUT_TRANSFORM_FLIPPED_180     6
#define OUTPUT_TRANSFORM_FLIPPED_270     7

/* Output */
typedef struct uos_output {
    struct wl_output wl_output;
    int32_t x, y;
    int32_t width, height;
    int32_t refresh;      /* in mHz */
    int32_t transform;
    int32_t scale;
    uint32_t crtc_id;
    uint32_t connector_id;
    uint32_t fb_id;
    bool connected;
    bool enabled;
    struct wl_list link;
} uos_output_t;

/* Output public API */
uos_output_t *output_init(const char *connector, const char *crtc, const char *mode);
void output_deinit(uos_output_t *output);

void output_set_mode(uos_output_t *output, int32_t width, int32_t height, int32_t refresh);
int32_t output_get_refresh(uos_output_t *output);

void output_scale(uos_output_t *output, int32_t scale_factor);
int32_t output_get_scale(uos_output_t *output);

void output_transform(uos_output_t *output, int32_t transform);
int32_t output_get_transform(uos_output_t *output);

bool output_is_connected(uos_output_t *output);
bool output_is_enabled(uos_output_t *output);

void output_enable(uos_output_t *output);
void output_disable(uos_output_t *output);

/* Hotplug detection */
typedef void (*output_hotplug_cb)(uos_output_t *output, bool connected, void *user_data);

void output_set_hotplug_callback(output_hotplug_cb cb, void *user_data);
int output_scan_for_outputs(void);

#endif /* _UOS_OUTPUT_H_ */