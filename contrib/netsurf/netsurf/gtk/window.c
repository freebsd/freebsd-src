/*
 * Copyright 2006 Daniel Silverstone <dsilvers@digital-scurf.org>
 * Copyright 2006 Rob Kendrick <rjek@rjek.com>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <inttypes.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/nsoption.h"
#include "content/hlcache.h"
#include "gtk/window.h"
#include "gtk/selection.h"
#include "desktop/gui.h"
#include "desktop/browser_private.h"
#include "desktop/mouse.h"
#include "desktop/searchweb.h"
#include "desktop/textinput.h"

#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/scaffolding.h"
#include "gtk/plotters.h"
#include "gtk/schedule.h"
#include "gtk/tabs.h"
#include "gtk/bitmap.h"
#include "gtk/gdk.h"

/* helper macro to conenct signals to callbacks */
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))

extern const GdkPixdata menu_cursor_pixdata;

struct gui_window {
	/** The gtk scaffold object containing menu, buttons, url bar, [tabs],
	 * drawing area, etc that may contain one or more gui_windows.
	 */
	nsgtk_scaffolding *scaffold;

	/** The 'content' window that is rendered in the gui_window */
	struct browser_window	*bw;

	/** mouse state and events. */
	struct {
		struct gui_window *gui;

		gdouble pressed_x;
		gdouble pressed_y;
		gboolean waiting;
		browser_mouse_state state;
	} mouse;

	/** caret dimension and location for rendering */
	int caretx, carety, careth;

	/** caret shape for rendering */
	gui_pointer_shape current_pointer;

	/** previous event location */
	int last_x, last_y;

	/** The top level container (tabContents) */
	GtkWidget *container;

	/** display widget for this page or frame */
	GtkLayout *layout;

	/** handle to the the visible tab */
	GtkWidget *tab;

	/** statusbar */
	GtkLabel *status_bar;

	/** status pane */
	GtkPaned *paned;

	/** has the status pane had its first size operation yet? */
	bool paned_sized;

	/** to allow disactivation / resume of normal window behaviour */
	gulong signalhandler[NSGTK_WINDOW_SIGNAL_COUNT];

	/** The icon this window should have */
	GdkPixbuf *icon;

	/** The input method to use with this window */
	GtkIMContext *input_method;

	/** list for cleanup */
	struct gui_window *next, *prev;
};

struct gui_window *window_list = NULL;	/**< first entry in win list*/
int temp_open_background = -1;

nsgtk_scaffolding *nsgtk_get_scaffold(struct gui_window *g)
{
	return g->scaffold;
}

GdkPixbuf *nsgtk_get_icon(struct gui_window *gw)
{
	return gw->icon;
}

struct browser_window *nsgtk_get_browser_window(struct gui_window *g)
{
	return g->bw;
}

unsigned long nsgtk_window_get_signalhandler(struct gui_window *g, int i)
{
	return g->signalhandler[i];
}

GtkLayout *nsgtk_window_get_layout(struct gui_window *g)
{
	return g->layout;
}

GtkWidget *nsgtk_window_get_tab(struct gui_window *g)
{
	return g->tab;
}

void nsgtk_window_set_tab(struct gui_window *g, GtkWidget *w)
{
	g->tab = w;
}


struct gui_window *nsgtk_window_iterate(struct gui_window *g)
{
	return g->next;
}

float nsgtk_get_scale_for_gui(struct gui_window *g)
{
	return browser_window_get_scale(g->bw);
}

#if GTK_CHECK_VERSION(3,0,0)

static gboolean
nsgtk_window_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct gui_window *gw = data;
	struct gui_window *z;
	struct rect clip;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	double x1;
	double y1;
	double x2;
	double y2;

	assert(gw);
	assert(gw->bw);

	for (z = window_list; z && z != gw; z = z->next)
		continue;
	assert(z);
	assert(GTK_WIDGET(gw->layout) == widget);

	current_widget = (GtkWidget *)gw->layout;
	current_cr = cr;

	GtkAdjustment *vscroll = nsgtk_layout_get_vadjustment(gw->layout);
	GtkAdjustment *hscroll = nsgtk_layout_get_hadjustment(gw->layout);

	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

	clip.x0 = x1;
	clip.y0 = y1;
	clip.x1 = x2;
	clip.y1 = y2;

	browser_window_redraw(gw->bw,
			      -gtk_adjustment_get_value(hscroll),
			      -gtk_adjustment_get_value(vscroll),
			      &clip,
			      &ctx);

	if (gw->careth != 0) {
		nsgtk_plot_caret(gw->caretx, gw->carety, gw->careth);
	}

	current_widget = NULL;

	return FALSE;
}

#else

static gboolean
nsgtk_window_draw_event(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	struct gui_window *gw = data;
	struct gui_window *z;
	struct rect clip;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};

	assert(gw);
	assert(gw->bw);

	for (z = window_list; z && z != gw; z = z->next)
		continue;
	assert(z);
	assert(GTK_WIDGET(gw->layout) == widget);

	current_widget = (GtkWidget *)gw->layout;
	current_cr = gdk_cairo_create(nsgtk_layout_get_bin_window(gw->layout));

	clip.x0 = event->area.x;
	clip.y0 = event->area.y;
	clip.x1 = event->area.x + event->area.width;
	clip.y1 = event->area.y + event->area.height;

	browser_window_redraw(gw->bw, 0, 0, &clip, &ctx);

	if (gw->careth != 0) {
		nsgtk_plot_caret(gw->caretx, gw->carety, gw->careth);
	}

	cairo_destroy(current_cr);

	current_widget = NULL;

	return FALSE;
}

#endif

static gboolean nsgtk_window_motion_notify_event(GtkWidget *widget,
					  GdkEventMotion *event, gpointer data)
{
	struct gui_window *g = data;
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;

	if ((abs(event->x - g->last_x) < 5) &&
	    (abs(event->y - g->last_y) < 5)) {
		/* Mouse hasn't moved far enough from press coordinate for this
		 * to be considered a drag. */
		return FALSE;
	} else {
		/* This is a drag, ensure it's always treated as such, even if
		 * we drag back over the press location */
		g->last_x = INT_MIN;
		g->last_y = INT_MIN;
	}

	if (g->mouse.state & BROWSER_MOUSE_PRESS_1) {
		/* Start button 1 drag */
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_DRAG_1,
				g->mouse.pressed_x, g->mouse.pressed_y);

		/* Replace PRESS with HOLDING and declare drag in progress */
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_1 |
				BROWSER_MOUSE_HOLDING_1);
		g->mouse.state |= BROWSER_MOUSE_DRAG_ON;
	} else if (g->mouse.state & BROWSER_MOUSE_PRESS_2) {
		/* Start button 2 drag */
		browser_window_mouse_click(g->bw, BROWSER_MOUSE_DRAG_2,
				g->mouse.pressed_x, g->mouse.pressed_y);

		/* Replace PRESS with HOLDING and declare drag in progress */
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_2 |
				BROWSER_MOUSE_HOLDING_2);
		g->mouse.state |= BROWSER_MOUSE_DRAG_ON;
	}

	/* Handle modifiers being removed */
	if (g->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
		g->mouse.state ^= BROWSER_MOUSE_MOD_1;
	if (g->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
		g->mouse.state ^= BROWSER_MOUSE_MOD_2;

	browser_window_mouse_track(g->bw, g->mouse.state,
			event->x / browser_window_get_scale(g->bw),
			event->y / browser_window_get_scale(g->bw));

	return TRUE;
}

static gboolean nsgtk_window_button_press_event(GtkWidget *widget,
					 GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;

	gtk_im_context_reset(g->input_method);
	gtk_widget_grab_focus(GTK_WIDGET(g->layout));
	gtk_widget_hide(GTK_WIDGET(nsgtk_scaffolding_history_window(
			g->scaffold)->window));

	g->mouse.pressed_x = event->x / browser_window_get_scale(g->bw);
	g->mouse.pressed_y = event->y / browser_window_get_scale(g->bw);

	switch (event->button) {
	case 1:	/* Left button, usually. Pass to core as BUTTON 1. */
		g->mouse.state = BROWSER_MOUSE_PRESS_1;
		break;

	case 2:	/* Middle button, usually. Pass to core as BUTTON 2 */
		g->mouse.state = BROWSER_MOUSE_PRESS_2;
		break;

	case 3:	/* Right button, usually. Action button, context menu. */
		browser_window_remove_caret(g->bw, true);
		nsgtk_scaffolding_popup_menu(g->scaffold, g->mouse.pressed_x,
				g->mouse.pressed_y);
		return TRUE;

	default:
		return FALSE;
	}

	/* Modify for double & triple clicks */
	if (event->type == GDK_3BUTTON_PRESS)
		g->mouse.state |= BROWSER_MOUSE_TRIPLE_CLICK;
	else if (event->type == GDK_2BUTTON_PRESS)
		g->mouse.state |= BROWSER_MOUSE_DOUBLE_CLICK;

	/* Handle the modifiers too */
	if (event->state & GDK_SHIFT_MASK)
		g->mouse.state |= BROWSER_MOUSE_MOD_1;
	if (event->state & GDK_CONTROL_MASK)
		g->mouse.state |= BROWSER_MOUSE_MOD_2;

	/* Record where we pressed, for use when determining whether to start
	 * a drag in motion notify events. */
	g->last_x = event->x;
	g->last_y = event->y;

	browser_window_mouse_click(g->bw, g->mouse.state, g->mouse.pressed_x,
			g->mouse.pressed_y);

	return TRUE;
}

static gboolean nsgtk_window_button_release_event(GtkWidget *widget,
					 GdkEventButton *event, gpointer data)
{
	struct gui_window *g = data;
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;

	/* If the mouse state is PRESS then we are waiting for a release to emit
	 * a click event, otherwise just reset the state to nothing */
	if (g->mouse.state & BROWSER_MOUSE_PRESS_1)
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_1 | BROWSER_MOUSE_CLICK_1);
	else if (g->mouse.state & BROWSER_MOUSE_PRESS_2)
		g->mouse.state ^= (BROWSER_MOUSE_PRESS_2 | BROWSER_MOUSE_CLICK_2);

	/* Handle modifiers being removed */
	if (g->mouse.state & BROWSER_MOUSE_MOD_1 && !shift)
		g->mouse.state ^= BROWSER_MOUSE_MOD_1;
	if (g->mouse.state & BROWSER_MOUSE_MOD_2 && !ctrl)
		g->mouse.state ^= BROWSER_MOUSE_MOD_2;

	if (g->mouse.state & (BROWSER_MOUSE_CLICK_1 | BROWSER_MOUSE_CLICK_2)) {
		browser_window_mouse_click(g->bw, g->mouse.state,
				event->x / browser_window_get_scale(g->bw),
				event->y / browser_window_get_scale(g->bw));
	} else {
		browser_window_mouse_track(g->bw, 0,
				event->x / browser_window_get_scale(g->bw),
				event->y / browser_window_get_scale(g->bw));
	}

	g->mouse.state = 0;
	return TRUE;
}

static gboolean
nsgtk_window_scroll_event(GtkWidget *widget,
			  GdkEventScroll *event,
			  gpointer data)
{
	struct gui_window *g = data;
	double value;
	double deltax = 0;
	double deltay = 0;
	GtkAdjustment *vscroll = nsgtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hscroll = nsgtk_layout_get_hadjustment(g->layout);
	GtkAllocation alloc;

	switch (event->direction) {
	case GDK_SCROLL_LEFT:
		deltax = -1.0;
		break;

	case GDK_SCROLL_UP:
		deltay = -1.0;
		break;

	case GDK_SCROLL_RIGHT:
		deltax = 1.0;
		break;

	case GDK_SCROLL_DOWN:
		deltay = 1.0;
		break;

#if GTK_CHECK_VERSION(3,4,0)
	case GDK_SCROLL_SMOOTH:
		gdk_event_get_scroll_deltas((GdkEvent *)event, &deltax, &deltay);
		break;
#endif
	default:
		LOG(("Unhandled mouse scroll direction"));
		return TRUE;
	}

	deltax *= nsgtk_adjustment_get_step_increment(hscroll);
	deltay *= nsgtk_adjustment_get_step_increment(vscroll);

	if (browser_window_scroll_at_point(g->bw,
			event->x / browser_window_get_scale(g->bw),
			event->y / browser_window_get_scale(g->bw),
			deltax, deltay) != true) {

		/* core did not handle event so change adjustments */

		/* Horizontal */
		if (deltax != 0) {
			value = gtk_adjustment_get_value(hscroll) + deltax;

			/* @todo consider gtk_widget_get_allocated_width() */
			nsgtk_widget_get_allocation(GTK_WIDGET(g->layout), &alloc);

			if (value > nsgtk_adjustment_get_upper(hscroll) - alloc.width) {
				value = nsgtk_adjustment_get_upper(hscroll) - alloc.width;
			}
			if (value < nsgtk_adjustment_get_lower(hscroll)) {
				value = nsgtk_adjustment_get_lower(hscroll);
			}

			gtk_adjustment_set_value(hscroll, value);
		}

		/* Vertical */
		if (deltay != 0) {
			value = gtk_adjustment_get_value(vscroll) + deltay;

			/* @todo consider gtk_widget_get_allocated_height */
			nsgtk_widget_get_allocation(GTK_WIDGET(g->layout), &alloc);

			if (value > (nsgtk_adjustment_get_upper(vscroll) - alloc.height)) {
				value = nsgtk_adjustment_get_upper(vscroll) - alloc.height;
			}
			if (value < nsgtk_adjustment_get_lower(vscroll)) {
				value = nsgtk_adjustment_get_lower(vscroll);
			}

			gtk_adjustment_set_value(vscroll, value);
		}
	}

	return TRUE;
}

static gboolean nsgtk_window_keypress_event(GtkWidget *widget,
				GdkEventKey *event, gpointer data)
{
	struct gui_window *g = data;
	uint32_t nskey;
	
	if (gtk_im_context_filter_keypress(g->input_method, event))
		return TRUE;

	nskey = gtk_gui_gdkkey_to_nskey(event);

	if (browser_window_key_press(g->bw, nskey))
		return TRUE;

	if ((event->state & 0x7) != 0)
		return TRUE;

	double value;
	GtkAdjustment *vscroll = nsgtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hscroll = nsgtk_layout_get_hadjustment(g->layout);
	GtkAllocation alloc;

	/* @todo consider gtk_widget_get_allocated_width() */
	nsgtk_widget_get_allocation(GTK_WIDGET(g->layout), &alloc);

	switch (event->keyval) {

	case GDK_KEY(Home):
	case GDK_KEY(KP_Home):
		value = nsgtk_adjustment_get_lower(vscroll);
		gtk_adjustment_set_value(vscroll, value);
		break;

	case GDK_KEY(End):
	case GDK_KEY(KP_End):
		value = nsgtk_adjustment_get_upper(vscroll) - alloc.height;

		if (value < nsgtk_adjustment_get_lower(vscroll))
			value = nsgtk_adjustment_get_lower(vscroll);

		gtk_adjustment_set_value(vscroll, value);
		break;

	case GDK_KEY(Left):
	case GDK_KEY(KP_Left):
		value = gtk_adjustment_get_value(hscroll) -
			nsgtk_adjustment_get_step_increment(hscroll);

		if (value < nsgtk_adjustment_get_lower(hscroll))
			value = nsgtk_adjustment_get_lower(hscroll);

		gtk_adjustment_set_value(hscroll, value);
		break;

	case GDK_KEY(Up):
	case GDK_KEY(KP_Up):
		value = gtk_adjustment_get_value(vscroll) -
			nsgtk_adjustment_get_step_increment(vscroll);

		if (value < nsgtk_adjustment_get_lower(vscroll))
			value = nsgtk_adjustment_get_lower(vscroll);

		gtk_adjustment_set_value(vscroll, value);
		break;

	case GDK_KEY(Right):
	case GDK_KEY(KP_Right):
		value = gtk_adjustment_get_value(hscroll) +
			nsgtk_adjustment_get_step_increment(hscroll);

		if (value > nsgtk_adjustment_get_upper(hscroll) - alloc.width)
			value = nsgtk_adjustment_get_upper(hscroll) - alloc.width;

		gtk_adjustment_set_value(hscroll, value);
		break;

	case GDK_KEY(Down):
	case GDK_KEY(KP_Down):
		value = gtk_adjustment_get_value(vscroll) +
			nsgtk_adjustment_get_step_increment(vscroll);

		if (value > nsgtk_adjustment_get_upper(vscroll) - alloc.height)
			value = nsgtk_adjustment_get_upper(vscroll) - alloc.height;

		gtk_adjustment_set_value(vscroll, value);
		break;

	case GDK_KEY(Page_Up):
	case GDK_KEY(KP_Page_Up):
		value = gtk_adjustment_get_value(vscroll) -
			nsgtk_adjustment_get_page_increment(vscroll);

		if (value < nsgtk_adjustment_get_lower(vscroll))
			value = nsgtk_adjustment_get_lower(vscroll);

		gtk_adjustment_set_value(vscroll, value);
		break;

	case GDK_KEY(Page_Down):
	case GDK_KEY(KP_Page_Down):
		value = gtk_adjustment_get_value(vscroll) +
			nsgtk_adjustment_get_page_increment(vscroll);

		if (value > nsgtk_adjustment_get_upper(vscroll) - alloc.height)
			value = nsgtk_adjustment_get_upper(vscroll) - alloc.height;

		gtk_adjustment_set_value(vscroll, value);
		break;

	default:
		break;

	}

	return TRUE;
}

static gboolean nsgtk_window_keyrelease_event(GtkWidget *widget,
				GdkEventKey *event, gpointer data)
{
	struct gui_window *g = data;
	
	return gtk_im_context_filter_keypress(g->input_method, event);
}


static void nsgtk_window_input_method_commit(GtkIMContext *ctx,
				const gchar *str, gpointer data)
{
	struct gui_window *g = data;
	size_t len = strlen(str), offset = 0;

	while (offset < len) {
		uint32_t nskey = utf8_to_ucs4(str + offset, len - offset);

		browser_window_key_press(g->bw, nskey);

		offset = utf8_next(str, len, offset);
	}
}


static gboolean nsgtk_window_size_allocate_event(GtkWidget *widget,
		GtkAllocation *allocation, gpointer data)
{
	struct gui_window *g = data;

	g->bw->reformat_pending = true;
	browser_reformat_pending = true;


	return TRUE;
}


/**  when the pane position is changed update the user option
 *
 * The slightly awkward implementation with the first allocation flag
 * is necessary because the initial window creation does not cause an
 * allocate-event signal so the position value in the pane is incorrect
 * and we cannot know what it should be until after the allocation
 * (which did not generate a signal) is done as the user position is a
 * percentage of pane total width not an absolute value.
 */
static void
nsgtk_paned_notify__position(GObject *gobject, GParamSpec *pspec, gpointer data)
{
	struct gui_window *g = data;
	GtkAllocation pane_alloc;

	gtk_widget_get_allocation(GTK_WIDGET(g->paned), &pane_alloc);

	if (g->paned_sized == false)
	{
		g->paned_sized = true;
		gtk_paned_set_position(g->paned,
		(nsoption_int(toolbar_status_size) * pane_alloc.width) / 10000);
		return;
	}

	nsoption_set_int(toolbar_status_size,
	 ((gtk_paned_get_position(g->paned) * 10000) / (pane_alloc.width - 1)));
}

/** Set status bar / scroll bar proportion according to user option
 * when pane is resized.
 */
static gboolean nsgtk_paned_size_allocate_event(GtkWidget *widget,
		GtkAllocation *allocation, gpointer data)
{
	gtk_paned_set_position(GTK_PANED(widget),
	       (nsoption_int(toolbar_status_size) * allocation->width) / 10000);

	return TRUE;
}

/* destroy the browsing context as there is nothing to display it now */
static void window_destroy(GtkWidget *widget, gpointer data)
{
	struct gui_window *gw = data;

	browser_window_destroy(gw->bw);

	g_object_unref(gw->input_method);
}

/* Core interface documented in desktop/gui.h to create a gui_window */
static struct gui_window *
gui_window_create(struct browser_window *bw,
		struct gui_window *existing,
		gui_window_create_flags flags)
{
	struct gui_window *g; /**< what we're creating to return */
	GError* error = NULL;
	bool tempback;
	GtkBuilder* xml;

	/* open builder file first to ease error handling on faliure */
	xml = gtk_builder_new();
	if (!gtk_builder_add_from_file(xml,
				       glade_file_location->tabcontents,
				       &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free(error);
		return NULL;
	}

	g = calloc(1, sizeof(*g));
	if (!g) {
		warn_user("NoMemory", 0);
		g_object_unref(xml);
		return NULL;
	}

	LOG(("Creating gui window %p for browser window %p", g, bw));

	g->bw = bw;
	g->mouse.state = 0;
	g->current_pointer = GUI_POINTER_DEFAULT;
	if (flags & GW_CREATE_CLONE) {
		assert(existing != NULL);
		bw->scale = existing->bw->scale;
	} else {
		bw->scale = nsoption_int(scale) / 100;
	}

	/* attach scaffold */
	if (flags & GW_CREATE_TAB) {
		assert(existing != NULL);
		g->scaffold = existing->scaffold;
	} else {
		/* Now construct and attach a scaffold */
		g->scaffold = nsgtk_new_scaffolding(g);
	}
	if (g->scaffold == NULL) {
		warn_user("NoMemory", 0);
		free(g);
		g_object_unref(xml);
		return NULL;
	}

	/* Construct our primary elements */
	g->container = GTK_WIDGET(gtk_builder_get_object(xml, "tabContents"));
	g->layout = GTK_LAYOUT(gtk_builder_get_object(xml, "layout"));
	g->status_bar = GTK_LABEL(gtk_builder_get_object(xml, "status_bar"));
	g->paned = GTK_PANED(gtk_builder_get_object(xml, "hpaned1"));
	g->input_method = gtk_im_multicontext_new();


	/* add new gui window to global list (push_top) */
	if (window_list) {
		window_list->prev = g;
	}
	g->next = window_list;
	g->prev = NULL;
	window_list = g;

	/* set the events we're interested in receiving from the browser's
	 * drawing area.
	 */
	gtk_widget_add_events(GTK_WIDGET(g->layout),
				GDK_EXPOSURE_MASK |
				GDK_LEAVE_NOTIFY_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_POINTER_MOTION_HINT_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK |
				GDK_SCROLL_MASK);
	nsgtk_widget_set_can_focus(GTK_WIDGET(g->layout), TRUE);

	/* set the default background colour of the drawing area to white. */
	nsgtk_widget_override_background_color(GTK_WIDGET(g->layout),
					       GTK_STATE_NORMAL,
					       0, 0xffff, 0xffff, 0xffff);

	nsgtk_connect_draw_event(GTK_WIDGET(g->layout),
				 G_CALLBACK(nsgtk_window_draw_event), g);

	/* layout signals */
	CONNECT(g->layout, "motion-notify-event",
			nsgtk_window_motion_notify_event, g);
	g->signalhandler[NSGTK_WINDOW_SIGNAL_CLICK] =
			CONNECT(g->layout, "button-press-event",
			nsgtk_window_button_press_event, g);
	CONNECT(g->layout, "button-release-event",
			nsgtk_window_button_release_event, g);
	CONNECT(g->layout, "key-press-event",
			nsgtk_window_keypress_event, g);
	CONNECT(g->layout, "key-release-event",
			nsgtk_window_keyrelease_event, g);
	CONNECT(g->layout, "size-allocate",
			nsgtk_window_size_allocate_event, g);
	CONNECT(g->layout, "scroll-event",
			nsgtk_window_scroll_event, g);

	/* status pane signals */
	CONNECT(g->paned, "size-allocate",
		nsgtk_paned_size_allocate_event, g);

	CONNECT(g->paned, "notify::position",
		nsgtk_paned_notify__position, g);

	/* gtk container destructor */
	CONNECT(g->container, "destroy",
		window_destroy, g);

	/* input method */
	gtk_im_context_set_client_window(g->input_method,
			nsgtk_layout_get_bin_window(g->layout));
	gtk_im_context_set_use_preedit(g->input_method, FALSE);
	/* input method signals */
	CONNECT(g->input_method, "commit",
		nsgtk_window_input_method_commit, g);

	/* add the tab container to the scaffold notebook */
	switch (temp_open_background) {
	case -1:
		tempback = !(nsoption_bool(focus_new));
		break;
	case 0:
		tempback = false;
		break;
	default:
		tempback = true;
		break;
	}
	nsgtk_tab_add(g, g->container, tempback);

	/* safe to drop the reference to the xml as the container is
	 * referenced by the notebook now.
	 */
	g_object_unref(xml);

	return g;
}



void nsgtk_reflow_all_windows(void)
{
	for (struct gui_window *g = window_list; g; g = g->next) {
		nsgtk_tab_options_changed(
				nsgtk_scaffolding_notebook(g->scaffold));
		g->bw->reformat_pending = true;
	}

	browser_reformat_pending = true;
}


/**
 * Process pending reformats
 */

void nsgtk_window_process_reformats(void)
{
	struct gui_window *g;
	GtkAllocation alloc;

	browser_reformat_pending = false;
	for (g = window_list; g; g = g->next) {
		if (!g->bw->reformat_pending)
			continue;

		g->bw->reformat_pending = false;

		/* @todo consider gtk_widget_get_allocated_width() */
		nsgtk_widget_get_allocation(GTK_WIDGET(g->layout), &alloc);

		browser_window_reformat(g->bw, false, alloc.width, alloc.height);
	}
}

void nsgtk_window_destroy_browser(struct gui_window *gw)
{
	/* remove tab */
	gtk_widget_destroy(gw->container);
}

static void gui_window_destroy(struct gui_window *g)
{
	LOG(("gui_window: %p", g));
	assert(g != NULL);
	assert(g->bw != NULL);
	LOG(("scaffolding: %p", g->scaffold));

	if (g->prev) {
		g->prev->next = g->next;
	} else {
		window_list = g->next;
	}

	if (g->next) {
		g->next->prev = g->prev;
	}

	LOG(("window list head: %p", window_list));
}

/**
 * set favicon
 */
static void gui_window_set_icon(struct gui_window *gw, hlcache_handle *icon)
{
	struct bitmap *icon_bitmap = NULL;

	/* free any existing icon */
	if (gw->icon != NULL) {
		g_object_unref(gw->icon);
		gw->icon = NULL;
	}

	if (icon != NULL) {
		icon_bitmap = content_get_bitmap(icon);
		if (icon_bitmap != NULL) {
			LOG(("Using %p bitmap", icon_bitmap));
			gw->icon = nsgdk_pixbuf_get_from_surface(icon_bitmap->surface, 16, 16);
		}
	}

	if (gw->icon == NULL) {
		LOG(("Using default favicon"));
		g_object_ref(favicon_pixbuf);
		gw->icon = favicon_pixbuf;
	}

	nsgtk_scaffolding_set_icon(gw);
}

static bool gui_window_get_scroll(struct gui_window *g, int *sx, int *sy)
{
	GtkAdjustment *vadj = nsgtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hadj = nsgtk_layout_get_hadjustment(g->layout);

	assert(vadj);
	assert(hadj);

	*sy = (int)(gtk_adjustment_get_value(vadj));
	*sx = (int)(gtk_adjustment_get_value(hadj));

	return true;
}

static void nsgtk_redraw_caret(struct gui_window *g)
{
	int sx, sy;

	if (g->careth == 0)
		return;

	gui_window_get_scroll(g, &sx, &sy);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
			g->caretx - sx, g->carety - sy, 1, g->careth + 1);

}

static void gui_window_remove_caret(struct gui_window *g)
{
	int sx, sy;
	int oh = g->careth;

	if (oh == 0)
		return;

	g->careth = 0;

	gui_window_get_scroll(g, &sx, &sy);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
			g->caretx - sx, g->carety - sy, 1, oh + 1);

}

static void gui_window_redraw_window(struct gui_window *g)
{
	gtk_widget_queue_draw(GTK_WIDGET(g->layout));
}

static void gui_window_update_box(struct gui_window *g, const struct rect *rect)
{
	int sx, sy;
	float scale;

	if (!browser_window_has_content(g->bw))
		return;

	gui_window_get_scroll(g, &sx, &sy);
	scale = browser_window_get_scale(g->bw);

	gtk_widget_queue_draw_area(GTK_WIDGET(g->layout),
			rect->x0 * scale - sx,
			rect->y0 * scale - sy,
			(rect->x1 - rect->x0) * scale,
			(rect->y1 - rect->y0) * scale);
}

static void gui_window_set_status(struct gui_window *g, const char *text)
{
	assert(g);
	assert(g->status_bar);
	gtk_label_set_text(g->status_bar, text);
}


static void gui_window_set_scroll(struct gui_window *g, int sx, int sy)
{
	GtkAdjustment *vadj = nsgtk_layout_get_vadjustment(g->layout);
	GtkAdjustment *hadj = nsgtk_layout_get_hadjustment(g->layout);
	gdouble vlower, vpage, vupper, hlower, hpage, hupper, x = (double)sx, y = (double)sy;

	assert(vadj);
	assert(hadj);

	g_object_get(vadj, "page-size", &vpage, "lower", &vlower, "upper", &vupper, NULL);
	g_object_get(hadj, "page-size", &hpage, "lower", &hlower, "upper", &hupper, NULL);

	if (x < hlower)
		x = hlower;
	if (x > (hupper - hpage))
		x = hupper - hpage;
	if (y < vlower)
		y = vlower;
	if (y > (vupper - vpage))
		y = vupper - vpage;

	gtk_adjustment_set_value(vadj, y);
	gtk_adjustment_set_value(hadj, x);
}

static void gui_window_update_extent(struct gui_window *g)
{
	int w, h;

	if (browser_window_get_extents(g->bw, true, &w, &h) == NSERROR_OK) {
		gtk_layout_set_size(g->layout, w, h);
	}
}

static GdkCursor *nsgtk_create_menu_cursor(void)
{
	GdkCursor *cursor = NULL;
	GdkPixbuf *pixbuf;
	pixbuf = gdk_pixbuf_from_pixdata(&menu_cursor_pixdata, FALSE, NULL);
	cursor = gdk_cursor_new_from_pixbuf(gdk_display_get_default(), pixbuf, 0, 3);
	g_object_unref (pixbuf);

	return cursor;
}

static void gui_window_set_pointer(struct gui_window *g,
				   gui_pointer_shape shape)
{
	GdkCursor *cursor = NULL;
	GdkCursorType cursortype;
	bool nullcursor = false;

	if (g->current_pointer == shape)
		return;

	g->current_pointer = shape;

	switch (shape) {
	case GUI_POINTER_POINT:
		cursortype = GDK_HAND2;
		break;
	case GUI_POINTER_CARET:
		cursortype = GDK_XTERM;
		break;
	case GUI_POINTER_UP:
		cursortype = GDK_TOP_SIDE;
		break;
	case GUI_POINTER_DOWN:
		cursortype = GDK_BOTTOM_SIDE;
		break;
	case GUI_POINTER_LEFT:
		cursortype = GDK_LEFT_SIDE;
		break;
	case GUI_POINTER_RIGHT:
		cursortype = GDK_RIGHT_SIDE;
		break;
	case GUI_POINTER_LD:
		cursortype = GDK_BOTTOM_LEFT_CORNER;
		break;
	case GUI_POINTER_RD:
		cursortype = GDK_BOTTOM_RIGHT_CORNER;
		break;
	case GUI_POINTER_LU:
		cursortype = GDK_TOP_LEFT_CORNER;
		break;
	case GUI_POINTER_RU:
		cursortype = GDK_TOP_RIGHT_CORNER;
		break;
	case GUI_POINTER_CROSS:
		cursortype = GDK_CROSS;
		break;
	case GUI_POINTER_MOVE:
		cursortype = GDK_FLEUR;
		break;
	case GUI_POINTER_WAIT:
		cursortype = GDK_WATCH;
		break;
	case GUI_POINTER_HELP:
		cursortype = GDK_QUESTION_ARROW;
		break;
	case GUI_POINTER_MENU:
		cursor = nsgtk_create_menu_cursor();
		nullcursor = true;
		break;
	case GUI_POINTER_PROGRESS:
		/* In reality, this needs to be the funky left_ptr_watch
		 * which we can't do easily yet.
		 */
		cursortype = GDK_WATCH;
		break;
	/* The following we're not sure about */
	case GUI_POINTER_NO_DROP:
	case GUI_POINTER_NOT_ALLOWED:
	case GUI_POINTER_DEFAULT:
	default:
	      nullcursor = true;
	}

	if (!nullcursor)
		cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(
					GTK_WIDGET(g->layout)),
					cursortype);
	gdk_window_set_cursor(nsgtk_widget_get_window(GTK_WIDGET(g->layout)),
			      cursor);

	if (!nullcursor)
		nsgdk_cursor_unref(cursor);
}


static void gui_window_place_caret(struct gui_window *g, int x, int y, int height,
		const struct rect *clip)
{
	nsgtk_redraw_caret(g);

	y += 1;
	height -= 1;

	if (y < clip->y0) {
		height -= clip->y0 - y;
		y = clip->y0;
	}

	if (y + height > clip->y1) {
		height = clip->y1 - y + 1;
	}

	g->caretx = x;
	g->carety = y;
	g->careth = height;

	nsgtk_redraw_caret(g);

	gtk_widget_grab_focus(GTK_WIDGET(g->layout));
}


static void gui_window_get_dimensions(struct gui_window *g, int *width, int *height,
			       bool scaled)
{
	GtkAllocation alloc;

	/* @todo consider gtk_widget_get_allocated_width() */
	nsgtk_widget_get_allocation(GTK_WIDGET(g->layout), &alloc);

	*width = alloc.width;
	*height = alloc.height;

	if (scaled) {
		float scale = browser_window_get_scale(g->bw);
		*width /= scale;
		*height /= scale;
	}
	LOG(("width: %i", *width));
	LOG(("height: %i", *height));
}

static void gui_window_start_selection(struct gui_window *g)
{
	gtk_widget_grab_focus(GTK_WIDGET(g->layout));
}

static void
gui_window_file_gadget_open(struct gui_window *g,
			    hlcache_handle *hl,
			    struct form_control *gadget)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new("Select File",
			nsgtk_scaffolding_window(g->scaffold),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

	LOG(("*** open dialog: %p", dialog));
			
	int ret = gtk_dialog_run(GTK_DIALOG(dialog));
	LOG(("*** return value: %d", ret));
	if (ret == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_filename(
			GTK_FILE_CHOOSER(dialog));
		
		browser_window_set_gadget_filename(g->bw, gadget, filename);
		
		g_free(filename);
	}

	gtk_widget_destroy(dialog);
}

static struct gui_window_table window_table = {
	.create = gui_window_create,
	.destroy = gui_window_destroy,
	.redraw = gui_window_redraw_window,
	.update = gui_window_update_box,
	.get_scroll = gui_window_get_scroll,
	.set_scroll = gui_window_set_scroll,
	.get_dimensions = gui_window_get_dimensions,
	.update_extent = gui_window_update_extent,

	.set_icon = gui_window_set_icon,
	.set_status = gui_window_set_status,
	.set_pointer = gui_window_set_pointer,
	.place_caret = gui_window_place_caret,
	.remove_caret = gui_window_remove_caret,
	.file_gadget_open = gui_window_file_gadget_open,
	.start_selection = gui_window_start_selection,

	/* from scaffold */
	.set_title = gui_window_set_title,
	.set_url = gui_window_set_url,
	.start_throbber = gui_window_start_throbber,
	.stop_throbber = gui_window_stop_throbber,
};

struct gui_window_table *nsgtk_window_table = &window_table;
