/*
 * Mobile App Framework API
 */

#ifndef _MOBILE_APP_H_
#define _MOBILE_APP_H_

struct mobile_app;

/* App manager lifecycle */
int mobile_app_mgr_start(void);
void mobile_app_mgr_stop(void);

/* App management */
struct mobile_app *mobile_app_install(const char *name, const char *package_id);
void mobile_app_uninstall(struct mobile_app *app);
int mobile_app_start(struct mobile_app *app);
int mobile_app_kill(struct mobile_app *app);
struct mobile_app *mobile_app_get(const char *package_id);

#endif /* _MOBILE_APP_H_ */