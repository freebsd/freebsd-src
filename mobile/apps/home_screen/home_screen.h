/*
 * Home Screen App - Mobile OS Launcher
 * uOS(m) - User OS Mobile
 */

#ifndef _HOME_SCREEN_H_
#define _HOME_SCREEN_H_

#include <stdint.h>
#include <time.h>

#define MAX_APPS 32
#define MAX_PAGES 5

typedef struct {
    char name[32];
    char package_id[64];
    int icon_x, icon_y;
    int page;
} app_icon_t;

typedef struct {
    app_icon_t apps[MAX_APPS];
    int app_count;
    int current_page;
    int drawer_open;
    int pull_start_y;
    time_t last_pull;
} home_screen_t;

int home_screen_init(void);
void home_screen_deinit(void);
void home_screen_render(void);
void home_screen_handle_touch(int x, int y, int action);
void home_screen_open_app(const char *package_id);
void home_screen_open_drawer(void);
void home_screen_close_drawer(void);
void home_screen_add_app(const char *name, const char *package_id, int page);

#endif /* _HOME_SCREEN_H_ */