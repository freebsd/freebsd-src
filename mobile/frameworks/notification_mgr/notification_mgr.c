/*
 * Notification Manager - Implementation
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/queue.h>

#include "notification_mgr.h"

static notification_t g_notifs[NM_MAX_NOTIFICATIONS];
static int             g_notif_count = 0;
static int             g_init = 0;

int
nm_init(void)
{
    if (g_init)
        return 0;

    memset(g_notifs, 0, sizeof(g_notifs));
    g_notif_count = 0;
    g_init = 1;

    mkdir("/var/lib/notifications", 0755);
    return 0;
}

void nm_shutdown(void)
{
    memset(g_notifs, 0, sizeof(g_notifs));
    g_notif_count = 0;
    g_init = 0;
}

uint32_t
nm_notify(const char *app_id, const char *tag,
          const char *title, const char *body,
          const char *icon, nm_channel_t channel,
          nm_importance_t importance)
{
    if (!app_id)
        return 0;

    if (g_notif_count >= NM_MAX_NOTIFICATIONS)
        g_notif_count = 0;

    notification_t *n = &g_notifs[g_notif_count];
    n->id = (uint32_t)(time(NULL) & 0xFFFFFFFF);
    strlcpy(n->app_id, app_id, sizeof(n->app_id));
    if (tag) strlcpy(n->tag, tag, sizeof(n->tag));
    if (title) strlcpy(n->title, title, sizeof(n->title));
    if (body) strlcpy(n->body, body, sizeof(n->body));
    if (icon) strlcpy(n->icon, icon, sizeof(n->icon));
    n->channel = channel;
    n->importance = importance;
    n->when = time(NULL);
    n->ongoing = 0;
    n->auto_cancel = 1;
    n->dismissed = 0;

    uint32_t id = n->id;
    g_notif_count++;

    return id;
}

int
nm_cancel(uint32_t id)
{
    for (int i = 0; i < g_notif_count; i++) {
        if (g_notifs[i].id == id) {
            g_notifs[i].dismissed = 1;
            return 0;
        }
    }
    return -1;
}

int
nm_cancel_all(const char *app_id)
{
    for (int i = 0; i < g_notif_count; i++) {
        if (strcmp(g_notifs[i].app_id, app_id) == 0)
            g_notifs[i].dismissed = 1;
    }
    return 0;
}

int
nm_cancel_tag(const char *app_id, const char *tag)
{
    if (!app_id || !tag)
        return -1;

    for (int i = 0; i < g_notif_count; i++) {
        if (strcmp(g_notifs[i].app_id, app_id) == 0 &&
            strcmp(g_notifs[i].tag, tag) == 0) {
            g_notifs[i].dismissed = 1;
        }
    }
    return 0;
}

int
nm_set_channel_importance(const char *app_id, nm_channel_t channel, nm_importance_t importance)
{
    (void)app_id; (void)channel; (void)importance;
    return 0;
}

int
nm_get_notification(uint32_t id, notification_t *out_notif)
{
    if (!out_notif)
        return -1;
    for (int i = 0; i < g_notif_count; i++) {
        if (g_notifs[i].id == id) {
            *out_notif = g_notifs[i];
            return 0;
        }
    }
    return -1;
}

int
nm_get_active(const char *app_id, notification_t **out, int *out_count)
{
    if (!app_id || !out || !out_count)
        return -1;

    static notification_t results[NM_MAX_NOTIFICATIONS];
    int count = 0;

    for (int i = 0; i < g_notif_count && count < NM_MAX_NOTIFICATIONS; i++) {
        if (strcmp(g_notifs[i].app_id, app_id) == 0 && !g_notifs[i].dismissed) {
            results[count++] = g_notifs[i];
        }
    }

    *out = results;
    *out_count = count;
    return 0;
}
