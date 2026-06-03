/*
 * Notification System for uOS(m) Desktop
 * Slide-in popups, urgency levels, actions, history, DND
 */

#ifndef _NOTIFICATIONS_H_
#define _NOTIFICATIONS_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../ui/compositor/compositor_core.h"
#include "icon_theme.h"

#define NOTIF_MAX_NOTIFICATIONS  8
#define NOTIF_MAX_TITLE          128
#define NOTIF_MAX_BODY           512
#define NOTIF_MAX_ACTIONS        4
#define NOTIF_HISTORY_SIZE       100
#define NOTIF_POPUP_WIDTH        360
#define NOTIF_POPUP_HEIGHT      96
#define NOTIF_SPACING           12

typedef enum {
    NOTIF_URGENCY_LOW,
    NOTIF_URGENCY_NORMAL,
    NOTIF_URGENCY_CRITICAL
} notif_urgency_t;

typedef struct {
    char id[32];
    char title[NOTIF_MAX_TITLE];
    char body[NOTIF_MAX_BODY];
    icon_handle_t *icon;
    notif_urgency_t urgency;
    uint64_t timestamp;
    uint32_t timeout_ms;
    int dismissible;
    char actions[NOTIF_MAX_ACTIONS][64];
    int action_count;
    int visible;
} notification_t;

typedef struct {
    notification_t notifications[NOTIF_MAX_NOTIFICATIONS];
    int count;
    int dnd_enabled;
    int history_count;
    notification_t history[NOTIF_HISTORY_SIZE];
    uint32_t next_id;
    int popup_x, popup_y;
    uint64_t last_popup_time;
} notif_engine_t;

int notif_init(void);
void notif_shutdown(void);
int notif_send(const char *title, const char *body, const char *icon_name, notif_urgency_t urgency);
int notif_send_with_actions(const char *title, const char *body, const char *icon_name, notif_urgency_t urgency, const char **actions, int action_count);
void notif_dismiss(int notif_id);
void notif_dismiss_all(void);
void notif_set_dnd(int enabled);
int notif_get_dnd(void);
void notif_render(void);
void notif_update(void);
void notif_handle_action(int notif_id, int action_idx);
void notif_show_history(void);
void notif_clear_history(void);
notif_engine_t *notif_get_instance(void);

#endif /* _NOTIFICATIONS_H_ */
