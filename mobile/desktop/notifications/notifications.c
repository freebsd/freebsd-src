/*
 * Notification System Implementation
 */

#include "notifications.h"
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

static notif_engine_t *g_notif = NULL;

int notif_init(void) {
    if (g_notif) return 0;
    g_notif = calloc(1, sizeof(notif_engine_t));
    if (!g_notif) return -1;
    g_notif->popup_x = FB_WIDTH - NOTIF_POPUP_WIDTH - 16;
    g_notif->popup_y = 16;
    return 0;
}

void notif_shutdown(void) {
    if (!g_notif) return;
    for (int i = 0; i < g_notif->count; i++) {
        if (g_notif->notifications[i].icon) icon_unload(g_notif->notifications[i].icon);
    }
    free(g_notif);
    g_notif = NULL;
}

static int notif_find_slot(void) {
    if (g_notif->count < NOTIF_MAX_NOTIFICATIONS) {
        return g_notif->count++;
    }
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) {
        if (!g_notif->notifications[i].visible) return i;
    }
    return (g_notif->next_id++) % NOTIF_MAX_NOTIFICATIONS;
}

int notif_send(const char *title, const char *body, const char *icon_name, notif_urgency_t urgency) {
    if (!g_notif) return -1;
    int slot = notif_find_slot();
    notification_t *n = &g_notif->notifications[slot];
    memset(n, 0, sizeof(notification_t));
    snprintf(n->id, sizeof(n->id), "%d", (int)g_notif->next_id++);
    strncpy(n->title, title ? title : "", sizeof(n->title) - 1);
    strncpy(n->body, body ? body : "", sizeof(n->body) - 1);
    n->urgency = urgency;
    n->timestamp = (uint64_t)time(NULL);
    n->icon = icon_name ? icon_load(icon_name, 48, ICON_TYPE_ACTION) : icon_theme_get_default(ICON_TYPE_ACTION, 48);
    switch (urgency) {
        case NOTIF_URGENCY_LOW: n->timeout_ms = 5000; break;
        case NOTIF_URGENCY_NORMAL: n->timeout_ms = 8000; break;
        case NOTIF_URGENCY_CRITICAL: n->timeout_ms = 0; break;
    }
    n->dismissible = (urgency != NOTIF_URGENCY_CRITICAL);
    n->visible = 1;
    g_notif->last_popup_time = n->timestamp;
    if (g_notif->history_count < NOTIF_HISTORY_SIZE) {
        memcpy(&g_notif->history[g_notif->history_count++], n, sizeof(notification_t));
    }
    return atoi(n->id);
}

int notif_send_with_actions(const char *title, const char *body, const char *icon_name, notif_urgency_t urgency, const char **actions, int action_count) {
    int id = notif_send(title, body, icon_name, urgency);
    if (id < 0 || !g_notif) return id;
    for (int i = 0; i < action_count && i < NOTIF_MAX_ACTIONS; i++) {
        strncpy(g_notif->notifications[0].actions[i], actions[i], sizeof(g_notif->notifications[0].actions[0]) - 1);
        g_notif->notifications[0].action_count++;
    }
    return id;
}

void notif_dismiss(int notif_id) {
    if (!g_notif) return;
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) {
        if (atoi(g_notif->notifications[i].id) == notif_id) {
            if (g_notif->notifications[i].icon) icon_unload(g_notif->notifications[i].icon);
            memset(&g_notif->notifications[i], 0, sizeof(notification_t));
            break;
        }
    }
}

void notif_dismiss_all(void) {
    if (!g_notif) return;
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) notif_dismiss(atoi(g_notif->notifications[i].id));
}

void notif_set_dnd(int enabled) {
    if (!g_notif) return;
    g_notif->dnd_enabled = enabled;
}

int notif_get_dnd(void) {
    return g_notif ? g_notif->dnd_enabled : 0;
}

void notif_render(void) {
    if (!g_notif || g_notif->dnd_enabled) return;
    int y = g_notif->popup_y;
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) {
        notification_t *n = &g_notif->notifications[i];
        if (!n->visible || !n->title[0]) continue;
        uint32_t bg = (n->urgency == NOTIF_URGENCY_CRITICAL) ? 0xFF3A1A1A : 0xFF1E1E2E;
        int w = NOTIF_POPUP_WIDTH, h = NOTIF_POPUP_HEIGHT;
        for (int j = y; j < y + h && j < FB_HEIGHT; j++) {
            for (int k = g_notif->popup_x; k < g_notif->popup_x + w && k < FB_WIDTH; k++) {
                if (k >= 0 && j >= 0) fb_set_pixel(k, j, bg);
            }
        }
        fb_draw_text(g_notif->popup_x + 8, y + 8, n->title, 0xFFFFFFFF, bg);
        fb_draw_text(g_notif->popup_x + 8, y + 36, n->body, 0xAABBCCFF, bg);
        float elapsed = (float)((uint64_t)time(NULL) - n->timestamp);
        int bar_w = (int)((w - 16) * (elapsed / (n->timeout_ms / 1000.0f)));
        for (int k = g_notif->popup_x + 8; k < g_notif->popup_x + 8 + bar_w && k < FB_WIDTH; k++) fb_set_pixel(k, y + h - 6, 0xFF4A90D9);
        y += h + NOTIF_SPACING;
    }
}

void notif_update(void) {
    if (!g_notif) return;
    uint64_t now = (uint64_t)time(NULL);
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) {
        notification_t *n = &g_notif->notifications[i];
        if (!n->visible) continue;
        if (n->timeout_ms > 0 && (now - n->timestamp) * 1000 > n->timeout_ms) {
            notif_dismiss(atoi(n->id));
        }
    }
}

void notif_handle_action(int notif_id, int action_idx) {
    if (!g_notif) return;
    for (int i = 0; i < NOTIF_MAX_NOTIFICATIONS; i++) {
        if (atoi(g_notif->notifications[i].id) == notif_id && action_idx < g_notif->notifications[i].action_count) {
            notif_dismiss(notif_id);
            break;
        }
    }
}

void notif_show_history(void) {
}

void notif_clear_history(void) {
    if (!g_notif) return;
    g_notif->history_count = 0;
}

notif_engine_t *notif_get_instance(void) { return g_notif; }
