/*
 * App Lifecycle Management
 * Manages CREATED, PAUSED, RESUMED, STOPPED, DESTROYING state transitions
 * State preservation to /var/lib/app-state/{app-id}/
 */

#ifndef _LIFECYCLE_H_
#define _LIFECYCLE_H_

#include <sys/types.h>
#include <stdlib.h>

#define LC_STATE_DIR   "/var/lib/app-state"
#define LC_STATE_MAX   65536

typedef enum {
    LC_STATE_CREATED     = 0,
    LC_STATE_PAUSED      = 1,
    LC_STATE_RESUMED     = 2,
    LC_STATE_STOPPED     = 3,
    LC_STATE_DESTROYING  = 4,
} lc_state_t;

typedef enum {
    LC_TRANSITION_NORMAL    = 0,
    LC_TRANSITION_CONFIG    = 1,
    LC_TRANSITION_LOW_MEM   = 2,
    LC_TRANSITION_USER_LEAVE = 3,
} lc_transition_t;

typedef struct lifecycle {
    char       app_id[128];
    lc_state_t state;
    lc_state_t target_state;
    pid_t      pid;
    uid_t      uid;
    void      *ctx;
} lifecycle_t;

typedef struct lc_handlers {
    void (*on_create)(void *ctx);
    void (*on_resume)(void *ctx);
    void (*on_pause)(void *ctx);
    void (*on_stop)(void *ctx);
    void (*on_destroy)(void *ctx);
    void (*on_config_change)(void *ctx, int width, int height, int orientation);
    void (*on_low_memory)(void *ctx);
} lc_handlers_t;

SLIST_ENTRY(lc_entry) link;
} lc_entry_t;

static inline lc_entry_t *
lc_find(const char *app_id)
{
    (void)app_id;
    return NULL;
}

/* Register lifecycle callbacks for an app */
lifecycle_t *lc_register(const char *app_id, uid_t uid, const lc_handlers_t *handlers, void *ctx);

/* Transition through lifecycle */
int lc_transition(lifecycle_t *lc, lc_state_t new_state, lc_transition_t transition);

/* Get current state */
lc_state_t lc_get_state(const lifecycle_t *lc);

/* Save state to disk */
int lc_save_state(const lifecycle_t *lc, const void *data, uint32_t data_len);

/* Restore state from disk */
int lc_restore_state(const lifecycle_t *lc, void *out_data, uint32_t max_len);

/* Clear persisted state */
int lc_clear_state(const lifecycle_t *lc);

/* Called by system when memory pressure is high */
int lc_on_low_memory(const char *app_id);

#endif /* _LIFECYCLE_H_ */
