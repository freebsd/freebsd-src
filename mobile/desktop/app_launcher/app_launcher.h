/*
 * Application Launcher for uOS(m)
 * .desktop file parser, categories, fuzzy search, launch via service_mgr
 */

#ifndef _APP_LAUNCHER_H_
#define _APP_LAUNCHER_H_

#include <stdint.h>
#include "../ui/framebuffer.h"
#include "../services/service_mgr.h"

#define LAUNCHER_MAX_APPS       512
#define LAUNCHER_MAX_CATEGORIES 32
#define LAUNCHER_NAME_MAX       128
#define LAUNCHER_EXEC_MAX       256
#define LAUNCHER_ICON_MAX       128
#define LAUNCHER_CAT_MAX        64
#define LAUNCHER_MIME_MAX       32

typedef struct {
    char id[128];
    char name[LAUNCHER_NAME_MAX];
    char exec[LAUNCHER_EXEC_MAX];
    char icon[LAUNCHER_ICON_MAX];
    char categories[LAUNCHER_MAX_CATEGORIES][LAUNCHER_CAT_MAX];
    int category_count;
    char mime_types[LAUNCHER_MIME_MAX][64];
    int mime_count;
    int terminal;
    int hidden;
    int no_display;
    char comment[256];
    char path[256];
} desktop_app_t;

typedef struct {
    desktop_app_t apps[LAUNCHER_MAX_APPS];
    int count;
    char apps_dir[256];
} launcher_t;

int launcher_init(const char *apps_dir);
void launcher_shutdown(void);
int launcher_scan_directory(const char *path);
int launcher_parse_desktop_file(const char *path, desktop_app_t *app);
char **launcher_get_categories(int *count);
desktop_app_t *launcher_search(const char *query);
desktop_app_t *launcher_get_by_id(const char *id);
int launcher_launch(const char *app_id);
int launcher_launch_app(desktop_app_t *app);
void launcher_refresh(void);
launcher_t *launcher_get_instance(void);

#endif /* _APP_LAUNCHER_H_ */
