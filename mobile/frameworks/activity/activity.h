/*
 * Activity System
 * Activity = one screen in an app (Android-like)
 * Intent-based navigation, activity stack, back button handling
 */

#ifndef _ACTIVITY_H_
#define _ACTIVITY_H_

#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>

#define ACT_MAX_STACK        32
#define ACT_MAX_INTENT_FILTERS 32
#define ACT_MAX_DATA         8192
#define ACT_STACK_PATH       "/tmp/act_stack"

typedef enum {
    ACT_RESULT_OK       = -1,
    ACT_RESULT_CANCELED = 0,
    ACT_RESULT_FIRST_USER = 1,
} act_result_t;

struct app_info;

typedef struct activity {
    char       app_id[128];
    char       class_name[128];
    uint32_t   state;
    pid_t      pid;
    void      *root_view;
    int        result_code;
    void      *result_data;
    uint32_t   result_data_len;
    uint32_t   flags;
    struct activity *next;     /* stack pointer */
    struct activity *prev;     /* back stack */
} activity_t;

typedef struct intent {
    char action[128];
    char type[64];
    void *data;
    uint32_t data_len;
    uint32_t flags;
} intent_t;

typedef struct intent_filter {
    char     action[128];
    char     category[64];
    char     mime_type[64];
    char     class_name[128];
} intent_filter_t;

typedef struct activity_callbacks {
    void (*on_create)(activity_t *act, intent_t *intent);
    void (*on_start)(activity_t *act);
    void (*on_resume)(activity_t *act);
    void (*on_pause)(activity_t *act);
    void (*on_stop)(activity_t *act);
    void (*on_destroy)(activity_t *act);
    void (*on_result)(activity_t *act, int result_code, void *data, uint32_t data_len);
    void (*on_config_change)(activity_t *act, int width, int height);
    void (*on_new_intent)(activity_t *act, intent_t *intent);
} activity_callbacks_t;

activity_t *act_create(const char *class_name, struct app_info *app, const intent_t *intent);
int         act_start(activity_t *act);
int         act_finish(activity_t *act);
int         act_set_result(activity_t *act, int code, const void *data, uint32_t data_len);
int         act_send_intent(const char *action, const void *data, uint32_t data_len);
activity_t *act_get_current(void);
activity_t *act_pop_activity(void);
int         act_on_back_pressed(void);
void        act_declare_filters(struct app_info *app, intent_filter_t *out_filters, int *out_count);

#endif /* _ACTIVITY_H_ */
