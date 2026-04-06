/*
 * Mobile UI Toolkit
 *
 * Basic widget system for mobile applications with touch support
 * and smooth animations.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>

#include "../compositor/mobile_compositor.h"

struct mobile_widget {
	TAILQ_ENTRY(mobile_widget) link;
	uint32_t id;
	int x, y;
	int width, height;
	void (*draw)(struct mobile_widget *widget);
	void (*touch)(struct mobile_widget *widget, int x, int y, int action);
	void *user_data;
};

struct mobile_ui {
	struct mtx mtx;
	TAILQ_HEAD(, mobile_widget) widgets;
	struct mobile_window *window;
};

static struct mobile_ui *ui;

static int
mobile_ui_init(void)
{
	ui = malloc(sizeof(*ui), M_DEVBUF, M_WAITOK | M_ZERO);
	if (ui == NULL)
		return (ENOMEM);

	mtx_init(&ui->mtx, "mobile_ui", NULL, MTX_DEF);
	TAILQ_INIT(&ui->widgets);

	/* Create main window */
	ui->window = mobile_window_create(1080, 1920);
	if (ui->window == NULL) {
		mtx_destroy(&ui->mtx);
		free(ui, M_DEVBUF);
		return (ENOMEM);
	}

	printf("Mobile UI initialized\n");
	return (0);
}

static void
mobile_ui_fini(void)
{
	struct mobile_widget *widget, *tmp;

	mtx_lock(&ui->mtx);
	TAILQ_FOREACH_SAFE(widget, &ui->widgets, link, tmp) {
		TAILQ_REMOVE(&ui->widgets, widget, link);
		free(widget, M_DEVBUF);
	}
	mtx_unlock(&ui->mtx);

	if (ui->window)
		mobile_window_destroy(ui->window);

	mtx_destroy(&ui->mtx);
	free(ui, M_DEVBUF);
}

static struct mobile_widget *
mobile_widget_create(int x, int y, int width, int height,
    void (*draw_func)(struct mobile_widget *),
    void (*touch_func)(struct mobile_widget *, int, int, int))
{
	struct mobile_widget *widget;
	static uint32_t next_id = 1;

	widget = malloc(sizeof(*widget), M_DEVBUF, M_WAITOK | M_ZERO);
	if (widget == NULL)
		return (NULL);

	widget->id = next_id++;
	widget->x = x;
	widget->y = y;
	widget->width = width;
	widget->height = height;
	widget->draw = draw_func;
	widget->touch = touch_func;

	mtx_lock(&ui->mtx);
	TAILQ_INSERT_TAIL(&ui->widgets, widget, link);
	mtx_unlock(&ui->mtx);

	return (widget);
}

static void
mobile_widget_destroy(struct mobile_widget *widget)
{
	mtx_lock(&ui->mtx);
	TAILQ_REMOVE(&ui->widgets, widget, link);
	mtx_unlock(&ui->mtx);

	free(widget, M_DEVBUF);
}

static void
mobile_ui_render(void)
{
	struct mobile_widget *widget;

	mtx_lock(&ui->mtx);
	TAILQ_FOREACH(widget, &ui->widgets, link) {
		if (widget->draw)
			widget->draw(widget);
	}
	mtx_unlock(&ui->mtx);

	mobile_compositor_do_render();
}

static void
mobile_ui_touch(int x, int y, int action)
{
	struct mobile_widget *widget;

	mtx_lock(&ui->mtx);
	TAILQ_FOREACH(widget, &ui->widgets, link) {
		if (x >= widget->x && x < widget->x + widget->width &&
		    y >= widget->y && y < widget->y + widget->height) {
			if (widget->touch)
				widget->touch(widget, x, y, action);
			break;
		}
	}
	mtx_unlock(&ui->mtx);

	mobile_compositor_touch_event(x, y, action);
}

/* Example button widget */
static void
button_draw(struct mobile_widget *widget)
{
	printf("Drawing button %u at (%d,%d) %dx%d\n",
	    widget->id, widget->x, widget->y, widget->width, widget->height);
}

static void
button_touch(struct mobile_widget *widget, int x, int y, int action)
{
	if (action == 1) /* Touch down */
		printf("Button %u pressed\n", widget->id);
	else if (action == 0) /* Touch up */
		printf("Button %u released\n", widget->id);
}

struct mobile_widget *
mobile_button_create(int x, int y, int width, int height)
{
	return mobile_widget_create(x, y, width, height, button_draw, button_touch);
}

/* Public API */
int mobile_ui_start(void) { return mobile_ui_init(); }
void mobile_ui_stop(void) { mobile_ui_fini(); }
void mobile_ui_do_render(void) { mobile_ui_render(); }
void mobile_ui_handle_touch(int x, int y, int a) { mobile_ui_touch(x, y, a); }
struct mobile_widget *mobile_widget_button(int x, int y, int w, int h) { return mobile_button_create(x, y, w, h); }
void mobile_widget_remove(struct mobile_widget *w) { mobile_widget_destroy(w); }