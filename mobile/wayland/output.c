/*
 * UOS Compositor - Display Output Implementation
 * BSD-licensed
 */

#include "output.h"
#include "compositor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static output_hotplug_cb g_hotplug_cb = NULL;
static void *g_hotplug_user_data = NULL;

uos_output_t *output_init(const char *connector, const char *crtc, const char *mode)
{
    uos_output_t *output;

    output = calloc(1, sizeof(*output));
    if (!output) return NULL;

    wl_list_init(&output->link);
    output->x = 0;
    output->y = 0;
    output->scale = 1;
    output->transform = OUTPUT_TRANSFORM_NORMAL;
    output->connected = false;
    output->enabled = false;

    /* Use DRM backend to find and configure output */
    if (g_compositor && g_compositor->backend.u.drm.connector) {
        struct drm_connector *drm_conn = g_compositor->backend.u.drm.connector;
        struct drm_mode *drm_mode = g_compositor->backend.u.drm.mode;

        output->connector_id = drm_conn->connector_id;
        output->width = drm_mode->hdisplay;
        output->height = drm_mode->vdisplay;
        output->refresh = drm_mode->vrefresh;
        output->connected = drm_conn->connected;

        if (output->connected) {
            output->enabled = true;
        }
    }

    return output;
}

void output_deinit(uos_output_t *output)
{
    if (!output) return;
    wl_list_remove(&output->link);
    free(output);
}

void output_set_mode(uos_output_t *output, int32_t width, int32_t height, int32_t refresh)
{
    if (!output) return;

    output->width = width;
    output->height = height;
    output->refresh = refresh;

    /* Reconfigure CRTC with new mode */
    if (g_compositor && g_compositor->backend.u.drm.drm) {
        struct drm_device *drm = g_compositor->backend.u.drm.drm;
        /* Mode setting is done through DRM */
    }
}

int32_t output_get_refresh(uos_output_t *output)
{
    return output ? output->refresh : 0;
}

void output_scale(uos_output_t *output, int32_t scale_factor)
{
    if (!output) return;
    output->scale = scale_factor > 0 ? scale_factor : 1;
}

int32_t output_get_scale(uos_output_t *output)
{
    return output ? output->scale : 1;
}

void output_transform(uos_output_t *output, int32_t transform)
{
    if (!output) return;

    if (transform >= OUTPUT_TRANSFORM_NORMAL && transform <= OUTPUT_TRANSFORM_FLIPPED_270) {
        output->transform = transform;
    }
}

int32_t output_get_transform(uos_output_t *output)
{
    return output ? output->transform : OUTPUT_TRANSFORM_NORMAL;
}

bool output_is_connected(uos_output_t *output)
{
    return output ? output->connected : false;
}

bool output_is_enabled(uos_output_t *output)
{
    return output ? output->enabled : false;
}

void output_enable(uos_output_t *output)
{
    if (!output) return;
    output->enabled = true;
}

void output_disable(uos_output_t *output)
{
    if (!output) return;
    output->enabled = false;
}

void output_set_hotplug_callback(output_hotplug_cb cb, void *user_data)
{
    g_hotplug_cb = cb;
    g_hotplug_user_data = user_data;
}

int output_scan_for_outputs(void)
{
    int count = 0;

    if (!g_compositor || !g_compositor->backend.u.drm.drm) return 0;

    struct drm_device *drm = g_compositor->backend.u.drm.drm;

    for (size_t i = 0; i < drm->count_connectors; i++) {
        struct drm_connector *conn = drm->connectors[i];
        bool was_connected = conn->connected;

        /* Check connector status - in real impl would query kernel */
        conn->connected = true; /* Simplified */

        if (g_hotplug_cb) {
            uos_output_t output = {0};
            output.connector_id = conn->connector_id;
            g_hotplug_cb(&output, conn->connected, g_hotplug_user_data);
        }

        if (conn->connected) count++;
    }

    return count;
}