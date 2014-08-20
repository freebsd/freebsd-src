/*
 * Copyright 2004 Richard Wilson <not_ginger_matt@users.sourceforge.net>
 * Copyright 2009 Paul Blokus <paul_pl@users.sourceforge.net> 
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \file
 * Generic tree handling (implementation).
 */

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "desktop/tree.h"
#include "desktop/plotters.h"
#include "gtk/compat.h"
#include "gtk/gui.h"
#include "gtk/plotters.h"
#include "gtk/treeview.h"
#include "utils/log.h"
#include "utils/utf8.h"
#include "utils/utils.h"

struct nsgtk_treeview {
	GtkWindow *window;
	GtkScrolledWindow *scrolled;
	GtkDrawingArea *drawing_area;
	GtkIMContext *input_method;
	bool mouse_pressed;
	int mouse_pressed_x;
	int mouse_pressed_y;
	int last_x, last_y;
	browser_mouse_state mouse_state;
	struct tree *tree;
};

void nsgtk_treeview_destroy(struct nsgtk_treeview *tv)
{
	tree_delete(tv->tree);
	g_object_unref(tv->input_method);
	gtk_widget_destroy(GTK_WIDGET(tv->window));
	free(tv);
}

struct tree *nsgtk_treeview_get_tree(struct nsgtk_treeview *tv)
{
	return tv->tree;
}

static void nsgtk_tree_redraw_request(int x, int y, int width, int height, void *data)
{
	struct nsgtk_treeview *tw = data;
	
	gtk_widget_queue_draw_area(GTK_WIDGET(tw->drawing_area),
			x, y, width, height);
}


/**
 * Updates the tree owner following a tree resize
 *
 * \param tree  the tree to update the owner of
 */
static void nsgtk_tree_resized(struct tree *tree, int width, int height, void *data)
{
	struct nsgtk_treeview *tw = data;
	
	gtk_widget_set_size_request(GTK_WIDGET(tw->drawing_area),
			width, height);
	return;	
}

/**
 * Scrolls the tree to make an element visible
 *
 * \param y		Y coordinate of the element
 * \param height	height of the element
 * \param data		user data assigned to the tree on tree creation
 */
static void nsgtk_tree_scroll_visible(int y, int height, void *data)
{
	int y0, y1;
	gdouble page;
	struct nsgtk_treeview *tw = data;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(tw->scrolled);

	assert(vadj);
	
	g_object_get(vadj, "page-size", &page, NULL);
	
	y0 = (int)(gtk_adjustment_get_value(vadj));
	y1 = y0 + page;
	
	if ((y >= y0) && (y + height <= y1))
		return;
	if (y + height > y1)
		y0 = y0 + (y + height - y1); 
	if (y < y0)
		y0 = y;
	gtk_adjustment_set_value(vadj, y0);
}


/**
 * Retrieves the dimensions of the window with the tree
 *
 * \param data		user data assigned to the tree on tree creation
 * \param width		will be updated to window width if not NULL
 * \param height	will be updated to window height if not NULL
 */
static void nsgtk_tree_get_window_dimensions(int *width, int *height, void *data)
{
	struct nsgtk_treeview *tw = data;
	GtkAdjustment *vadj;
	GtkAdjustment *hadj;
	gdouble page;
	
	if (width != NULL) {
		hadj = gtk_scrolled_window_get_hadjustment(tw->scrolled);
		g_object_get(hadj, "page-size", &page, NULL);
		*width = page;
	}
	
	if (height != NULL) {
		vadj = gtk_scrolled_window_get_vadjustment(tw->scrolled);
		g_object_get(vadj, "page-size", &page, NULL);
		*height = page;
	}
}

#if GTK_CHECK_VERSION(3,0,0)

static gboolean 
nsgtk_tree_window_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	struct tree *tree = (struct tree *)data;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};
	double x1;
	double y1;
	double x2;
	double y2;
	
	current_widget = widget;
	current_cr = cr;
	
	cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

	tree_draw(tree, 0, 0, x1, y1, x2 - x1, y2 - y1, &ctx);
	
	current_widget = NULL;
	
	return FALSE;
}

#else

/* signal handler functions for a tree window */
static gboolean 
nsgtk_tree_window_draw_event(GtkWidget *widget, GdkEventExpose *event, gpointer g)
{
	struct tree *tree = (struct tree *) g;
	struct redraw_context ctx = {
		.interactive = true,
		.background_images = true,
		.plot = &nsgtk_plotters
	};
	int x, y, width, height;
	
	x = event->area.x;
	y = event->area.y;
	width = event->area.width;
	height = event->area.height;
	
	current_widget = widget;
	current_cr = gdk_cairo_create(nsgtk_widget_get_window(widget));
	
	tree_draw(tree, 0, 0, x, y, width, height, &ctx);
	
	current_widget = NULL;
	cairo_destroy(current_cr);
	
	return FALSE;
}

#endif

void nsgtk_tree_window_hide(GtkWidget *widget, gpointer g)
{
}

static gboolean
nsgtk_tree_window_button_press_event(GtkWidget *widget,
		GdkEventButton *event, gpointer g)
{	
	struct nsgtk_treeview *tw = g;
	struct tree *tree = tw->tree;
	
	gtk_im_context_reset(tw->input_method);
	gtk_widget_grab_focus(GTK_WIDGET(tw->drawing_area));

	tw->mouse_pressed = true;	
	tw->mouse_pressed_x = event->x;
	tw->mouse_pressed_y = event->y;

	if (event->type == GDK_2BUTTON_PRESS)
		tw->mouse_state = BROWSER_MOUSE_DOUBLE_CLICK;
	
	switch (event->button) {
		case 1: tw->mouse_state |= BROWSER_MOUSE_PRESS_1; break;
		case 2: tw->mouse_state |= BROWSER_MOUSE_PRESS_2; break;
	}
	/* Handle the modifiers too */
	if (event->state & GDK_SHIFT_MASK)
		tw->mouse_state |= BROWSER_MOUSE_MOD_1;
	if (event->state & GDK_CONTROL_MASK)
		tw->mouse_state |= BROWSER_MOUSE_MOD_2;
	if (event->state & GDK_MOD1_MASK)
		tw->mouse_state |= BROWSER_MOUSE_MOD_3;

	/* Record where we pressed, for use when determining whether to start
	 * a drag in motion notify events. */
	tw->last_x = event->x;
	tw->last_y = event->y;

	tree_mouse_action(tree, tw->mouse_state, event->x, event->y);
	
	return TRUE;
}

static gboolean
nsgtk_tree_window_button_release_event(GtkWidget *widget,
		GdkEventButton *event, gpointer g)
{
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;
	bool alt = event->state & GDK_MOD1_MASK;
	struct nsgtk_treeview *tw = (struct nsgtk_treeview *) g;
	struct tree *tree = tw->tree;

	/* We consider only button 1 clicks as double clicks.
	* If the mouse state is PRESS then we are waiting for a release to emit
	* a click event, otherwise just reset the state to nothing*/
	if (tw->mouse_state & BROWSER_MOUSE_DOUBLE_CLICK) {
		
		if (tw->mouse_state & BROWSER_MOUSE_PRESS_1)
			tw->mouse_state ^= BROWSER_MOUSE_PRESS_1 |
					BROWSER_MOUSE_CLICK_1;
		else if (tw->mouse_state & BROWSER_MOUSE_PRESS_2)
			tw->mouse_state ^= (BROWSER_MOUSE_PRESS_2 |
					BROWSER_MOUSE_CLICK_2 |
					BROWSER_MOUSE_DOUBLE_CLICK);
		
	} else if (tw->mouse_state & BROWSER_MOUSE_PRESS_1) {
		tw->mouse_state ^= (BROWSER_MOUSE_PRESS_1 |
				    BROWSER_MOUSE_CLICK_1);
	} else if (tw->mouse_state & BROWSER_MOUSE_PRESS_2) {
		tw->mouse_state ^= (BROWSER_MOUSE_PRESS_2 |
				    BROWSER_MOUSE_CLICK_2);
	} else if (tw->mouse_state & BROWSER_MOUSE_HOLDING_1) {
		tw->mouse_state ^= (BROWSER_MOUSE_HOLDING_1 |
				    BROWSER_MOUSE_DRAG_ON);
	} else if (tw->mouse_state & BROWSER_MOUSE_HOLDING_2) {
		tw->mouse_state ^= (BROWSER_MOUSE_HOLDING_2 |
				    BROWSER_MOUSE_DRAG_ON);
	}
	
	/* Handle modifiers being removed */
	if (tw->mouse_state & BROWSER_MOUSE_MOD_1 && !shift)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_1;
	if (tw->mouse_state & BROWSER_MOUSE_MOD_2 && !ctrl)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_2;
	if (tw->mouse_state & BROWSER_MOUSE_MOD_3 && !alt)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_3;


	if (tw->mouse_state &
			~(BROWSER_MOUSE_MOD_1 |
			  BROWSER_MOUSE_MOD_2 |
			  BROWSER_MOUSE_MOD_3)) {
		tree_mouse_action(tree, tw->mouse_state,
				event->x, event->y);
	} else {
		tree_drag_end(tree, tw->mouse_state,
				tw->mouse_pressed_x,
				tw->mouse_pressed_y,
				event->x, event->y);
	}

	tw->mouse_state = 0;
	tw->mouse_pressed = false;
				
	return TRUE;	
}

static gboolean
nsgtk_tree_window_motion_notify_event(GtkWidget *widget,
		GdkEventMotion *event, gpointer g)
{
	bool shift = event->state & GDK_SHIFT_MASK;
	bool ctrl = event->state & GDK_CONTROL_MASK;
	bool alt = event->state & GDK_MOD1_MASK;
	struct nsgtk_treeview *tw = (struct nsgtk_treeview *) g;
	struct tree *tree = tw->tree;

	if (tw->mouse_pressed == false)
		return TRUE;

	if ((abs(event->x - tw->last_x) < 5) &&
			(abs(event->y - tw->last_y) < 5)) {
		/* Mouse hasn't moved far enough from press coordinate for this
		 * to be considered a drag. */
		return FALSE;
	} else {
		/* This is a drag, ensure it's always treated as such, even if
		 * we drag back over the press location */
		tw->last_x = INT_MIN;
		tw->last_y = INT_MIN;
	}
	
	if (tw->mouse_state & BROWSER_MOUSE_PRESS_1) {
		/* Start button 1 drag */
		tree_mouse_action(tree, BROWSER_MOUSE_DRAG_1,
				  tw->mouse_pressed_x, tw->mouse_pressed_y);
		/* Replace PRESS with HOLDING and declare drag in progress */
		tw->mouse_state ^= (BROWSER_MOUSE_PRESS_1 |
				BROWSER_MOUSE_HOLDING_1);
		tw->mouse_state |= BROWSER_MOUSE_DRAG_ON;
		return TRUE;
	}
	else if (tw->mouse_state & BROWSER_MOUSE_PRESS_2){
		/* Start button 2s drag */
		tree_mouse_action(tree, BROWSER_MOUSE_DRAG_2,
				  tw->mouse_pressed_x, tw->mouse_pressed_y);
		/* Replace PRESS with HOLDING and declare drag in progress */
		tw->mouse_state ^= (BROWSER_MOUSE_PRESS_2 |
				BROWSER_MOUSE_HOLDING_2);
		tw->mouse_state |= BROWSER_MOUSE_DRAG_ON;
		return TRUE;
	}

	/* Handle modifiers being removed */
	if (tw->mouse_state & BROWSER_MOUSE_MOD_1 && !shift)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_1;
	if (tw->mouse_state & BROWSER_MOUSE_MOD_2 && !ctrl)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_2;
	if (tw->mouse_state & BROWSER_MOUSE_MOD_3 && !alt)
		tw->mouse_state ^= BROWSER_MOUSE_MOD_3;
	
	if (tw->mouse_state & (BROWSER_MOUSE_HOLDING_1 |
			BROWSER_MOUSE_HOLDING_2))
		tree_mouse_action(tree, tw->mouse_state, event->x,
				event->y);
	
	return TRUE;
}


static gboolean
nsgtk_tree_window_keypress_event(GtkWidget *widget, GdkEventKey *event,
		gpointer g)
{
	struct nsgtk_treeview *tw = (struct nsgtk_treeview *) g;
	struct tree *tree = tw->tree;
	uint32_t nskey;
	double value;
	GtkAdjustment *vscroll;
	GtkAdjustment *hscroll;
	GtkAdjustment *scroll = NULL;
	gdouble hpage, vpage;
	
	if (gtk_im_context_filter_keypress(tw->input_method, event))
		return TRUE;

	nskey = gtk_gui_gdkkey_to_nskey(event);

	if (tree_keypress(tree, nskey) == true)
		return TRUE;
			
	vscroll = gtk_scrolled_window_get_vadjustment(tw->scrolled);
	hscroll =  gtk_scrolled_window_get_hadjustment(tw->scrolled);
	g_object_get(vscroll, "page-size", &vpage, NULL);
	g_object_get(hscroll, "page-size", &hpage, NULL);

	switch (event->keyval) {
	case GDK_KEY(Home):
	case GDK_KEY(KP_Home):
			scroll = vscroll;
			value = nsgtk_adjustment_get_lower(scroll);
			break;

	case GDK_KEY(End):
	case GDK_KEY(KP_End):		
			scroll = vscroll;
			value = nsgtk_adjustment_get_upper(scroll) - vpage;
			if (value < nsgtk_adjustment_get_lower(scroll))
				value = nsgtk_adjustment_get_lower(scroll);
			break;

	case GDK_KEY(Left):
	case GDK_KEY(KP_Left):		
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) -
				nsgtk_adjustment_get_step_increment(scroll);
			if (value < nsgtk_adjustment_get_lower(scroll))
				value = nsgtk_adjustment_get_lower(scroll);
			break;

	case GDK_KEY(Up):
	case GDK_KEY(KP_Up):
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
				nsgtk_adjustment_get_step_increment(scroll);
			if (value < nsgtk_adjustment_get_lower(scroll))
				value = nsgtk_adjustment_get_lower(scroll);
			break;

	case GDK_KEY(Right):
	case GDK_KEY(KP_Right):
			scroll = hscroll;
			value = gtk_adjustment_get_value(scroll) +
				nsgtk_adjustment_get_step_increment(scroll);
			if (value > nsgtk_adjustment_get_upper(scroll) - hpage)
				value = nsgtk_adjustment_get_upper(scroll) - hpage;
			break;

	case GDK_KEY(Down):
	case GDK_KEY(KP_Down):
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
				nsgtk_adjustment_get_step_increment(scroll);
			if (value > nsgtk_adjustment_get_upper(scroll) - vpage)
				value = nsgtk_adjustment_get_upper(scroll) - vpage;
			break;

	case GDK_KEY(Page_Up):
	case GDK_KEY(KP_Page_Up):
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) -
				nsgtk_adjustment_get_page_increment(scroll);

			if (value < nsgtk_adjustment_get_lower(scroll))
				value = nsgtk_adjustment_get_lower(scroll);

			break;

	case GDK_KEY(Page_Down):
	case GDK_KEY(KP_Page_Down):
			scroll = vscroll;
			value = gtk_adjustment_get_value(scroll) +
				nsgtk_adjustment_get_page_increment(scroll);

			if (value > nsgtk_adjustment_get_upper(scroll) - vpage)
				value = nsgtk_adjustment_get_upper(scroll) - vpage;
			break;			

	default:
			break;
	}	
	
	if (scroll != NULL)
		gtk_adjustment_set_value(scroll, value);
	
	return TRUE;
}	

static gboolean
nsgtk_tree_window_keyrelease_event(GtkWidget *widget, GdkEventKey *event,
		gpointer g)
{
	struct nsgtk_treeview *tw = (struct nsgtk_treeview *) g;
	
	return gtk_im_context_filter_keypress(tw->input_method, event);
}

static void
nsgtk_tree_window_input_method_commit(GtkIMContext *ctx,
		const gchar *str, gpointer data)
{
	struct nsgtk_treeview *tw = (struct nsgtk_treeview *) data;
	size_t len = strlen(str), offset = 0;

	while (offset < len) {
		uint32_t nskey = utf8_to_ucs4(str + offset, len - offset);

		tree_keypress(tw->tree, nskey);

		offset = utf8_next(str, len, offset);
	}
}


static const struct treeview_table nsgtk_tree_callbacks = {
	.redraw_request = nsgtk_tree_redraw_request,
	.resized = nsgtk_tree_resized,
	.scroll_visible = nsgtk_tree_scroll_visible,
	.get_window_dimensions = nsgtk_tree_get_window_dimensions
};

struct nsgtk_treeview *nsgtk_treeview_create(unsigned int flags,
		GtkWindow *window, GtkScrolledWindow *scrolled,
 		GtkDrawingArea *drawing_area)
{
	struct nsgtk_treeview *tv;	
	
	assert(drawing_area != NULL);

	tv = malloc(sizeof(struct nsgtk_treeview));
	if (tv == NULL) {
		LOG(("malloc failed"));
		warn_user("NoMemory", 0);
		return NULL;
	}
	
	tv->window = window;
	tv->scrolled = scrolled;
	tv->drawing_area = drawing_area;
	tv->input_method = gtk_im_multicontext_new();
	tv->tree = tree_create(flags, &nsgtk_tree_callbacks, tv);
	tv->mouse_state = 0;
	tv->mouse_pressed = false;
	
	nsgtk_widget_override_background_color(GTK_WIDGET(drawing_area), 
					       GTK_STATE_NORMAL,
					       0, 0xffff, 0xffff, 0xffff);

	nsgtk_connect_draw_event(GTK_WIDGET(drawing_area), G_CALLBACK(nsgtk_tree_window_draw_event), tv->tree);
	
#define CONNECT(obj, sig, callback, ptr) \
	g_signal_connect(G_OBJECT(obj), (sig), G_CALLBACK(callback), (ptr))
	CONNECT(drawing_area, "button-press-event",
			nsgtk_tree_window_button_press_event,
			tv);
	CONNECT(drawing_area, "button-release-event",
			nsgtk_tree_window_button_release_event,
  			tv);
	CONNECT(drawing_area, "motion-notify-event",
			nsgtk_tree_window_motion_notify_event,
  			tv);
	CONNECT(drawing_area, "key-press-event",
			nsgtk_tree_window_keypress_event,
  			tv);
	CONNECT(drawing_area, "key-release-event",
			nsgtk_tree_window_keyrelease_event,
			tv);


	/* input method */
	gtk_im_context_set_client_window(tv->input_method,
			nsgtk_widget_get_window(GTK_WIDGET(tv->window)));
	gtk_im_context_set_use_preedit(tv->input_method, FALSE);
	/* input method signals */
	CONNECT(tv->input_method, "commit",
			nsgtk_tree_window_input_method_commit,
			tv);

	return tv;
}
