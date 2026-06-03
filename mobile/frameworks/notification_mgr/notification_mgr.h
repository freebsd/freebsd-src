/*
 * Notification Manager
 * Channels: ALERT, MEDIA, SYSTEM, CALL, MESSAGE
 * Importance: MIN, LOW, DEFAULT, HIGH, CRITICAL
 */

#ifndef _NOTIFICATION_MGR_H_
#define _NOTIFICATION_MGR_H_

#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>

#define NM_MAX_NOTIFICATIONS 1024
#define NM_MAX_TAG           64
#define NM_MAX_TITLE         128
#define NM_MAX_BODY          1024
#define NM_PANEL_PATH        "/var/run/nm_panel.sock"

typedef enum {
    NM_CHANNEL_ALERT   = 0,
    NM_CHANNEL_MEDIA   = 1,
    NM_CHANNEL_SYSTEM  = 2,
    NM_CHANNEL_CALL    = 3,
    NM_CHANNEL_MESSAGE = 4,
} nm_channel_t;

typedef enum {
    NM_IMPORTANCE_MIN      = 0,
    NM_IMPORTANCE_LOW      = 1,
    NM_IMPORTANCE_DEFAULT  = 2,
    NM_IMPORTANCE_HIGH     = 3,
    NM_IMPORTANCE_CRITICAL = 4,
} nm_importance_t;

typedef struct notification {
    uint32_t      id;
    char          app_id[128];
    char          tag[NM_MAX_TAG];
    char          title[NM_MAX_TITLE];
    char          body[NM_MAX_BODY];
    char          icon[128];
    nm_channel_t  channel;
    nm_importance_t importance;
    time_t        when;
    int           ongoing;
    int           auto_cancel;
    int           dismissed;
} notification_t;

int nm_init(void);
void nm_shutdown(void);

uint32_t nm_notify(const char *app_id, const char *tag,
                   const char *title, const char *body,
                   const char *icon, nm_channel_t channel,
                   nm_importance_t importance);

int      nm_cancel(uint32_t id);
int      nm_cancel_all(const char *app_id);
int      nm_cancel_tag(const char *app_id, const char *tag);

int      nm_set_channel_importance(const char *app_id, nm_channel_t channel,
                                   nm_importance_t importance);
int      nm_get_notification(uint32_t id, notification_t *out_notif);
int      nm_get_active(const char *app_id, notification_t **out, int *out_count);

#endif /* _NOTIFICATION_MGR_H_ */
