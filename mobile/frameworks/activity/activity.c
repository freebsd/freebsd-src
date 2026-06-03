/*
 * Activity System - Implementation
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/queue.h>

#include "activity.h"
#include "../app_launcher/app_launcher.h"
#include "../lifecycle/lifecycle.h"
#include "../ipc/ipc.h"

static activity_t *g_stack[ACT_MAX_STACK];
static int         g_stack_count = 0;
static activity_t *g_current = NULL;
static intent_filter_t g_filters[ACT_MAX_INTENT_FILTERS];
static int         g_filter_count = 0;

activity_t *
act_create(const char *class_name, struct app_info *app, const intent_t *intent)
{
    if (!class_name || !app)
        return NULL;

    activity_t *act = malloc(sizeof(*act));
    if (!act) return NULL;

    memset(act, 0, sizeof(*act));
    strlcpy(act->app_id, app->id, sizeof(act->app_id));
    strlcpy(act->class_name, class_name, sizeof(act->class_name));
    act->pid = getpid();

    if (intent && intent->action[0]) {
        act->flags |= ACT_LAUNCH_MASK;
        if (intent->data_len > 0 && intent->data_len < ACT_MAX_DATA && intent->data) {
            act->result_data = malloc(intent->data_len);
            if (act->result_data) {
                memcpy(act->result_data, intent->data, intent->data_len);
                act->result_data_len = intent->data_len;
            }
        }
    }

    /* Register lifecycle */
    lc_handlers_t lc_handlers;

    static void on_create_wrapper(void *ctx) { (void)ctx; }
    lc_handlers.on_create = on_create_wrapper;

    lc_register(act->app_id, getuid(), &lc_handlers, act_ref);

    return act;
}

int
act_start(activity_t *act)
{
    if (!act)
        return -1;

    if (g_stack_count < ACT_MAX_STACK) {
        if (g_current)
            g_current->next = act;
        act->prev = g_current;
        g_stack[g_stack_count++] = act;
        g_current = act;
    }

    return 0;
}

int
act_finish(activity_t *act)
{
    if (!act)
        return -1;

    if (act->result_data) {
        free(act->result_data);
        act->result_data = NULL;
    }

    act->state = LC_STATE_DESTROYING;

    if (g_current == act) {
        g_current = act->prev;
    }

    free(act);
    return 0;
}

int
act_set_result(activity_t *act, int code, const void *data, uint32_t data_len)
{
    if (!act)
        return -1;

    act->result_code = code;
    if (act->result_data) {
        free(act->result_data);
        act->result_data = NULL;
    }
    if (data && data_len > 0 && data_len < ACT_MAX_DATA) {
        act->result_data = malloc(data_len);
        if (!act->result_data)
            return -1;
        memcpy(act->result_data, data, data_len);
        act->result_data_len = data_len;
    }
    return 0;
}

int
act_send_intent(const char *action, const void *data, uint32_t data_len)
{
    if (!action)
        return -1;

    /* Find matching activity via intent filters */
    for (int i = 0; i < g_filter_count; i++) {
        if (strcmp(g_filters[i].action, action) == 0) {
            intent_t intent;
            memset(&intent, 0, sizeof(intent));
            strlcpy(intent.action, action, sizeof(intent.action));
            if (data && data_len > 0) {
                intent.data = (void *)data;
                intent.data_len = data_len;
            }

            lifecycle_t *lc = lc_find(g_filters[i].class_name);
            if (lc) {
                lc_transition(lc, LC_STATE_RESUMED, LC_TRANSITION_NORMAL);
                return 0;
            }
        }
    }
    return -1;
}

activity_t *
act_get_current(void)
{
    return g_current;
}

activity_t *
act_pop_activity(void)
{
    if (g_stack_count == 0)
        return NULL;

    activity_t *top = g_stack[--g_stack_count];
    g_current = (g_stack_count > 0) ? g_stack[g_stack_count - 1] : NULL;
    return top;
}

int
act_on_back_pressed(void)
{
    activity_t *act = act_pop_activity();
    if (act) {
        act_finish(act);
        return 0;
    }
    return -1;
}

void
act_declare_filters(struct app_info *app, intent_filter_t *out_filters, int *out_count)
{
    if (!app || !out_filters || !out_count)
        return;

    int count = 0;
    for (int c = 0; c < app->cat_count && count < ACT_MAX_INTENT_FILTERS; c++) {
        if (strcmp(app->categories[c], "Browser") == 0 ||
            strstr(app->mime_types, "text/html") != NULL) {
            strlcpy(out_filters[count].action, "android.intent.action.VIEW", 128);
            strlcpy(out_filters[count].mime_type, "text/html", 64);
            strlcpy(out_filters[count].category, "BROWSABLE", 64);
            strlcpy(out_filters[count].class_name, app->id, 128);
            count++;
        }
        if (strcmp(app->categories[c], "Messaging") == 0) {
            strlcpy(out_filters[count].action, "android.intent.action.SEND", 128);
            strlcpy(out_filters[count].mime_type, "*/*", 64);
            strlcpy(out_filters[count].category, "DEFAULT", 64);
            strlcpy(out_filters[count].class_name, app->id, 128);
            count++;
        }
    }

    *out_count = count;
    g_filter_count = count;
    memcpy(g_filters, out_filters, sizeof(g_filters[0]) * (size_t)count);
}
