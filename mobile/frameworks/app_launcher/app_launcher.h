/*
 * Mobile App Launcher Framework
 * Scans .desktop files, manages app metadata, launches apps via zygote
 */

#ifndef _APP_LAUNCHER_H_
#define _APP_LAUNCHER_H_

#include <sys/types.h>
#include <stdlib.h>

#define AL_PATH_MAX      512
#define AL_NAME_MAX      128
#define AL_ID_MAX        128
#define AL_EXEC_MAX      256
#define AL_ICON_MAX      128
#define AL_CAT_MAX       64
#define AL_CAT_COUNT_MAX 16
#define AL_MIME_MAX      (AL_CAT_COUNT_MAX * 32)
#define AL_MAX_APPS      1024
#define AL_APP_DIR       "/mobile/share/applications"
#define AL_DESKTOP_EXT   ".desktop"
#define AL_SEARCH_DIRS_MAX 8

typedef enum {
    APP_TYPE_APPLICATION = 0,
    APP_TYPE_LINK,
    APP_TYPE_DIRECTORY
} app_type_t;

typedef enum {
    PERM_NONE       = 0,
    PERM_CAMERA     = 1 << 0,
    PERM_MICROPHONE = 1 << 1,
    PERM_LOCATION   = 1 << 2,
    PERM_CONTACTS   = 1 << 3,
    PERM_SMS        = 1 << 4,
    PERM_STORAGE    = 1 << 5,
    PERM_INTERNET   = 1 << 6,
    PERM_BLUETOOTH  = 1 << 7,
    PERM_NFC        = 1 << 8,
    PERM_PHONE      = 1 << 9,
    PERM_SYSTEM     = 1 << 10,
    PERM_ACCESSIBILITY = 1 << 11,
} app_perms_t;

typedef struct app_info {
    char id[AL_ID_MAX];
    char name[AL_NAME_MAX];
    char exec[AL_EXEC_MAX];
    char icon[AL_ICON_MAX];
    char categories[AL_CAT_COUNT_MAX][AL_CAT_MAX];
    int  cat_count;
    char mime_types[AL_MIME_MAX];
    app_type_t type;
    app_perms_t permissions;
    int  launched_count;
} app_info_t;

typedef struct al_filter_query {
    const char *query;
    const char *category;
    app_perms_t required_perms;
} al_filter_query_t;

/* Filter callback context */
typedef struct al_ctx {
    app_info_t apps[AL_MAX_APPS];
    int count;
} al_ctx_t;

/* Initialize launcher framework, scan for .desktop files */
int al_init(void);

/* Shutdown launcher framework */
void al_shutdown(void);

/* Load and parse a single .desktop file */
int al_load_desktop_file(const char *path, app_info_t *out_info);

/* Get all registered applications */
int al_get_all_apps(app_info_t **out_apps, int *out_count);

/* Get application by ID */
app_info_t *al_get_app_by_id(const char *app_id);

/* Get applications in a category */
int al_get_category(const char *category, app_info_t **out_apps, int *out_count);

/* Filter applications by query (fuzzy match on name/id/category) */
int al_filter(const al_filter_query_t *query, app_info_t **out_apps, int *out_count);

/* Launch an application by ID via zygote */
int al_launch(const char *app_id);

/* Get application count */
int al_get_app_count(void);

/* Free results from get_all_apps, get_category, filter */
void al_free_apps(app_info_t *apps);

#endif /* _APP_LAUNCHER_H_ */
