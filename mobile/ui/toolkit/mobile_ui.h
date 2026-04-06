/*
 * Mobile UI Toolkit API
 */

#ifndef _MOBILE_UI_H_
#define _MOBILE_UI_H_

struct mobile_widget;

/* UI lifecycle */
int mobile_ui_start(void);
void mobile_ui_stop(void);

/* Rendering */
void mobile_ui_do_render(void);

/* Input handling */
void mobile_ui_handle_touch(int x, int y, int action);

/* Widgets */
struct mobile_widget *mobile_widget_button(int x, int y, int width, int height);
void mobile_widget_remove(struct mobile_widget *widget);

#endif /* _MOBILE_UI_H_ */