/*
 * Mobile Compositor API
 */

#ifndef _MOBILE_COMPOSITOR_H_
#define _MOBILE_COMPOSITOR_H_

struct mobile_window;

/* Compositor lifecycle */
int mobile_compositor_start(void);
void mobile_compositor_stop(void);

/* Window management */
struct mobile_window *mobile_window_create(int width, int height);
void mobile_window_destroy(struct mobile_window *window);

/* Rendering */
void mobile_compositor_do_render(void);

/* Input handling */
void mobile_compositor_touch_event(int x, int y, int action);

#endif /* _MOBILE_COMPOSITOR_H_ */