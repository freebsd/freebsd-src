/*
 * App Lifecycle Management - Implementation
 * State machine: CREATED -> RESUMED <-> PAUSED -> STOPPED -> DESTROYING
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>

#include "lifecycle.h"

static SLIST_HEAD(lc_head, lc_entry) g_lifecycles;
static pthread_mutex_t               g_lc_lock = PTHREAD_MUTEX_INITIALIZER;
static int                            g_lc_init = 0;

lifecycle_t *
lc_register(const char *app_id, uid_t uid, const lc_handlers_t *handlers, void *ctx)
{
    if (!app_id || !handlers)
        return NULL;

    lc_entry_t *entry = malloc(sizeof(*entry));
    if (!entry) return NULL;

    memset(entry, 0, sizeof(*entry));
    strlcpy(entry->lc.app_id, app_id, sizeof(entry->lc.app_id));
    entry->lc.uid = uid;
    entry->lc.ctx = ctx;
    entry->lc.state = LC_STATE_CREATED;
    entry->handlers = *handlers;

    pthread_mutex_lock(&g_lc_lock);
    SLIST_INSERT_HEAD(&g_lifecycles, entry, link);
    pthread_mutex_unlock(&g_lc_lock);

    mkdir(LC_STATE_DIR, 0755);

    if (handlers->on_create)
        handlers->on_create(ctx);

    return &entry->lc;
}

int
lc_transition(lifecycle_t *lc, lc_state_t new_state, lc_transition_t transition)
{
    if (!lc)
        return -1;

    lc_state_t old = lc->state;
    lc->state = new_state;

    pthread_mutex_lock(&g_lc_lock);
    lc_entry_t *entry;
    SLIST_FOREACH(entry, &g_lifecycles, link) {
        if (&entry->lc == lc) {
            switch (new_state) {
            case LC_STATE_RESUMED:
                if (entry->handlers.on_resume)
                    entry->handlers.on_resume(lc->ctx);
                break;
            case LC_STATE_PAUSED:
                if (entry->handlers.on_pause)
                    entry->handlers.on_pause(lc->ctx);
                break;
            case LC_STATE_STOPPED:
                if (entry->handlers.on_stop)
                    entry->handlers.on_stop(lc->ctx);
                break;
            case LC_STATE_DESTROYING:
                lc_clear_state(lc);
                if (entry->handlers.on_destroy)
                    entry->handlers.on_destroy(lc->ctx);
                break;
            case LC_STATE_CREATED:
                break;
            }
            break;
        }
    }
    pthread_mutex_unlock(&g_lc_lock);

    (void)old;
    (void)transition;
    return 0;
}

lc_state_t
lc_get_state(const lifecycle_t *lc)
{
    if (!lc)
        return LC_STATE_STOPPED;
    return lc->state;
}

int
lc_save_state(const lifecycle_t *lc, const void *data, uint32_t data_len)
{
    if (!lc || !data || data_len == 0)
        return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.state", LC_STATE_DIR, lc->app_id);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;

    ssize_t written = write(fd, data, data_len);
    close(fd);
    return (written == (ssize_t)data_len) ? 0 : -1;
}

int
lc_restore_state(const lifecycle_t *lc, void *out_data, uint32_t max_len)
{
    if (!lc || !out_data || max_len == 0)
        return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.state", LC_STATE_DIR, lc->app_id);

    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;

    ssize_t n = read(fd, out_data, max_len);
    close(fd);
    return (n > 0) ? 0 : -1;
}

int
lc_clear_state(const lifecycle_t *lc)
{
    if (!lc)
        return -1;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.state", LC_STATE_DIR, lc->app_id);
    unlink(path);
    return 0;
}

int
lc_on_low_memory(const char *app_id)
{
    lifecycle_t *lc = NULL;

    pthread_mutex_lock(&g_lc_lock);
    lc_entry_t *entry;
    SLIST_FOREACH(entry, &g_lifecycles, link) {
        if (strcmp(entry->lc.app_id, app_id) == 0) {
            lc = &entry->lc;
            break;
        }
    }
    pthread_mutex_unlock(&g_lc_lock);

    if (!lc)
        return -1;

    pthread_mutex_lock(&g_lc_lock);
    SLIST_FOREACH(entry, &g_lifecycles, link) {
        if (&entry->lc == lc && entry->handlers.on_low_memory) {
            entry->handlers.on_low_memory(lc->ctx);
            break;
        }
    }
    pthread_mutex_unlock(&g_lc_lock);

    return 0;
}
