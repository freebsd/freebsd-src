/*
 * Copyright 2008,2010 Vincent Sanders <vince@simtec.co.uk>
 *
 * Framebuffer windowing toolkit core.
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

#include <sys/types.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include <libnsfb.h>
#include <libnsfb_plot.h>
#include <libnsfb_plot_util.h>
#include <libnsfb_event.h>
#include <libnsfb_cursor.h>

#include "utils/utils.h"
#include "utils/log.h"
#include "css/css.h"
#include "desktop/browser.h"
#include "desktop/plotters.h"

#include "framebuffer/gui.h"
#include "framebuffer/fbtk.h"
#include "framebuffer/image_data.h"

#include "widget.h"

#ifdef FBTK_LOGGING

/* tree dump debug, also example of depth first tree walk */
static void
dump_tk_tree(fbtk_widget_t *widget)
{
	widget = fbtk_get_root_widget(widget);
	int indent = 0;

	while (widget != NULL) {
		LOG(("%*s%p", indent, "", widget));
		if (widget->first_child != NULL) {
			widget = widget->first_child;
			indent += 6;
		} else if (widget->next != NULL) {
			widget = widget->next;
		} else {
			while ((widget->parent != NULL) &&
			       (widget->parent->next == NULL)) {
				widget = widget->parent;
				indent -= 6;
			}
			if (widget->parent != NULL) {
				indent -= 6;
				widget = widget->parent->next;
			} else {
				widget = NULL;
			}
		}
	}
}

#endif 

/* exported function documented in fbtk.h */
void
fbtk_request_redraw(fbtk_widget_t *widget)
{
	fbtk_widget_t *cwidget;
	fbtk_widget_t *pwidget;

	assert(widget != NULL);

	/* if widget not mapped do not try to redraw it */
	pwidget = widget;
	while (pwidget != NULL) {
		if (pwidget->mapped == false)
			return;
		pwidget = pwidget->parent;
	}

	widget->redraw.needed = true;
	widget->redraw.x = 0;
	widget->redraw.y = 0;
	widget->redraw.width = widget->width;
	widget->redraw.height = widget->height;

#ifdef FBTK_LOGGING
	LOG(("redrawing %p %d,%d %d,%d",
	     widget,
	     widget->redraw.x,
	     widget->redraw.y,
	     widget->redraw.width,
	     widget->redraw.height));
#endif

	cwidget = widget->last_child;
	while (cwidget != NULL) {
		fbtk_request_redraw(cwidget);
		cwidget = cwidget->prev;
	}

	while (widget->parent != NULL) {
		widget = widget->parent;
		widget->redraw.child = true;
	}
}



/* exported function documented in fbtk.h */
int
fbtk_set_mapping(fbtk_widget_t *widget, bool map)
{
	LOG(("setting mapping on %p to %d", widget, map));
	widget->mapped = map;
	if (map) {
		fbtk_request_redraw(widget);
	} else {
		fbtk_request_redraw(widget->parent);
	}
	return 0;
}

/** swap the widget given with the next sibling.
 *
 * Swap a sibling widget with the next deepest in the hierachy
 */
static void
swap_siblings(fbtk_widget_t *lw)
{
	fbtk_widget_t *rw = lw->next; /* the widget to swap lw with */
	fbtk_widget_t *before;
	fbtk_widget_t *after;

	assert(rw != NULL);

	LOG(("Swapping %p with %p", lw, rw));
	before = lw->prev;
	after = rw->next;

	if (before == NULL) {
		/* left widget is currently the first child */
		lw->parent->first_child = rw;
	} else {
		before->next = rw;
	}
	rw->prev = before;
	rw->next = lw;

	if (after == NULL) {
		/* right widget is currently the last child */
		rw->parent->last_child = lw;
	} else {
		after->prev = lw;
	}
	lw->next = after;
	lw->prev = rw;
}



/* exported function documented in fbtk.h */
int
fbtk_set_zorder(fbtk_widget_t *widget, int z)
{
	while (z != 0) {
		if (z < 0) {
			if (widget->prev == NULL)
				break; /* cannot go any shallower */

			/* swap with previous entry */
			swap_siblings(widget->prev);

			z++;
		} else {
			if (widget->next == NULL)
				break; /* cannot go any deeper */

			/* swap with subsequent entry */
			swap_siblings(widget);

			z--;
		}
	}

	return z;
}


/* exported function documented in fbtk.h */
bool
fbtk_set_pos_and_size(fbtk_widget_t *widget,
		      int x, int y,
		      int width, int height)
{
	if ((widget->x != x) ||
	    (widget->y != y) ||
	    (widget->width != width) ||
	    (widget->height != height)) {
		widget->x = x;
		widget->y = y;
		widget->width = width;
		widget->height = height;
		/* @todo This should limit the redrawn area to the sum
		 * of the old and new widget dimensions, not redraw the lot.
		 */
		fbtk_request_redraw(widget->parent);
		return true;
	}
	return false;
}


/* exported function docuemnted in fbtk.h */
void
fbtk_set_caret(fbtk_widget_t *widget, bool set,
		int x, int y, int height,
		void (*remove_caret)(fbtk_widget_t *widget))
{
	fbtk_widget_t *root;

	assert(widget != NULL);
	root = fbtk_get_root_widget(widget);

	if (root->u.root.caret.owner != NULL &&
			root->u.root.caret.remove_cb != NULL)
		root->u.root.caret.remove_cb(widget);

	if (set) {
		assert(remove_caret != NULL);

		root->u.root.caret.owner = widget;
		root->u.root.caret.x = x;
		root->u.root.caret.y = y;
		root->u.root.caret.height = height;
		root->u.root.caret.remove_cb = remove_caret;

	} else {
		root->u.root.caret.owner = NULL;
		root->u.root.caret.remove_cb = NULL;
	}
}

/* exported function documented in fbtk.h */
int
fbtk_destroy_widget(fbtk_widget_t *widget)
{
	fbtk_widget_t *parent;
	int ret = 0;

	ret = fbtk_post_callback(widget, FBTK_CBT_DESTROY);

	while (widget->first_child != NULL) {
		fbtk_destroy_widget(widget->first_child);
	}

	parent = widget->parent;
	if (parent != NULL) {

		/* unlink from siblings */
		if (widget->prev != NULL) {
			widget->prev->next = widget->next;
		} else {
			/* must be the first widget, unlink from parent */
			parent->first_child = widget->next;
		}
		if (widget->next != NULL) {
			widget->next->prev = widget->prev;
		} else {
			/* must be the last widget, unlink from parent */
			parent->last_child = widget->prev;
		}

		free(widget);
	}

	return ret;
}

/* region coverage flags. */
enum {
	POINT_LEFTOF_REGION = 1,
	POINT_RIGHTOF_REGION = 2,
	POINT_ABOVE_REGION = 4,
	POINT_BELOW_REGION = 8,
};

/* Computes where a point lies in respect to an area. */
#define REGION(x,y,cx1,cx2,cy1,cy2)			\
	(( (y) > (cy2) ? POINT_BELOW_REGION : 0) |	\
	 ( (y) < (cy1) ? POINT_ABOVE_REGION : 0) |	\
	 ( (x) > (cx2) ? POINT_RIGHTOF_REGION : 0) |	\
	 ( (x) < (cx1) ? POINT_LEFTOF_REGION : 0) )

/* swap two integers */
#define SWAP(a, b) do { int t; t=(a); (a)=(b); (b)=t;  } while(0)

/* exported function documented in fbtk.h */
bool
fbtk_clip_rect(const bbox_t * restrict clip, bbox_t * restrict box)
{
	uint8_t region1;
	uint8_t region2;

	/* ensure co-ordinates are in ascending order */
	if (box->x1 < box->x0) 
		SWAP(box->x0, box->x1);
	if (box->y1 < box->y0) 
		SWAP(box->y0, box->y1);

	region1 = REGION(box->x0, box->y0, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);
	region2 = REGION(box->x1, box->y1, clip->x0, clip->x1 - 1, clip->y0, clip->y1 - 1);

	/* area lies entirely outside the clipping rectangle */
	if ((region1 | region2) && (region1 & region2))
		return false;

	if (box->x0 < clip->x0)
		box->x0 = clip->x0;
	if (box->x0 > clip->x1)
		box->x0 = clip->x1;

	if (box->x1 < clip->x0)
		box->x1 = clip->x0;
	if (box->x1 > clip->x1)
		box->x1 = clip->x1;

	if (box->y0 < clip->y0)
		box->y0 = clip->y0;
	if (box->y0 > clip->y1)
		box->y0 = clip->y1;

	if (box->y1 < clip->y0)
		box->y1 = clip->y0;
	if (box->y1 > clip->y1)
		box->y1 = clip->y1;

	return true;
}

/* exported function documented in fbtk.h */
bool
fbtk_clip_to_widget(fbtk_widget_t *widget, bbox_t * restrict box)
{
	bbox_t wbox;
	wbox.x0 = 0;
	wbox.y0 = 0;
	wbox.x1 = widget->width;
	wbox.y1 = widget->height;
	return fbtk_clip_rect(&wbox, box);
}



/* internally exported function documented in widget.h */
int
fbtk_set_ptr(fbtk_widget_t *widget, fbtk_callback_info *cbi)
{
	fbtk_widget_t *root = fbtk_get_root_widget(widget);
	struct fbtk_bitmap *bm = cbi->context;

	nsfb_cursor_set(root->u.root.fb,
			(nsfb_colour_t *)bm->pixdata,
			bm->width,
			bm->height,
			bm->width,
			bm->hot_x,
			bm->hot_y);

	return 0;
}



/* internally exported function documented in widget.h */
fbtk_widget_t *
fbtk_get_root_widget(fbtk_widget_t *widget)
{
	while (widget->parent != NULL)
		widget = widget->parent;

	/* check root widget was found */
	if (widget->type != FB_WIDGET_TYPE_ROOT) {
		LOG(("Widget with null parent that is not the root widget!"));
		return NULL;
	}

	return widget;
}


/* exported function documented in fbtk.h */
int
fbtk_get_absx(fbtk_widget_t *widget)
{
	int x = widget->x;

	while (widget->parent != NULL) {
		widget = widget->parent;
		x += widget->x;
	}

	return x;
}

/* exported function documented in fbtk.h */
int
fbtk_get_absy(fbtk_widget_t *widget)
{
	int y = widget->y;

	while (widget->parent != NULL) {
		widget = widget->parent;
		y += widget->y;
	}

	return y;
}

/* exported function documented in fbtk.h */
int
fbtk_get_height(fbtk_widget_t *widget)
{
	return widget->height;
}

/* exported function documented in fbtk.h */
int
fbtk_get_width(fbtk_widget_t *widget)
{
	return widget->width;
}

/* exported function documented in fbtk.h */
bool
fbtk_get_bbox(fbtk_widget_t *widget, nsfb_bbox_t *bbox)
{
	bbox->x0 = widget->x;
	bbox->y0 = widget->y;
	bbox->x1 = widget->x + widget->width;
	bbox->y1 = widget->y + widget->height;

	widget = widget->parent;
	while (widget != NULL) {
		bbox->x0 += widget->x;
		bbox->y0 += widget->y;
		bbox->x1 += widget->x;
		bbox->y1 += widget->y;
		widget = widget->parent;
	}

	return true;
}

bool
fbtk_get_caret(fbtk_widget_t *widget, int *x, int *y, int *height)
{
	fbtk_widget_t *root = fbtk_get_root_widget(widget);

	if (root->u.root.caret.owner == widget) {
		*x = root->u.root.caret.x;
		*y = root->u.root.caret.y;
		*height = root->u.root.caret.height;

		return true;

	} else {
		*x = 0;
		*y = 0;
		*height = 0;

		return false;
	}
}

/* exported function documented in fbtk.h */
fbtk_widget_t *
fbtk_get_widget_at(fbtk_widget_t *nwid, int x, int y)
{
	fbtk_widget_t *widget = NULL; /* found widget */

	/* require the root widget to start */
	nwid = fbtk_get_root_widget(nwid);

	while (nwid != NULL) {
		if ((nwid->mapped) &&
		    (x >= nwid->x) &&
		    (y >= nwid->y) &&
		    (x < (nwid->x + nwid->width)) &&
		    (y < (nwid->y + nwid->height))) {
			widget = nwid;
			x -= nwid->x;
			y -= nwid->y;
			nwid = nwid->first_child;
		} else {
			nwid = nwid->next;
		}
	}

	return widget;
}




/* internally exported function documented in widget.h */
fbtk_widget_t *
fbtk_widget_new(fbtk_widget_t *parent,
		enum fbtk_widgettype_e type,
		int x,
		int y,
		int width,
		int height)
{
	fbtk_widget_t *neww; /* new widget */

	if (parent == NULL)
		return NULL;

	neww = calloc(1, sizeof(fbtk_widget_t));
	if (neww == NULL)
		return NULL;

#ifdef FBTK_LOGGING
	LOG(("creating %p %d,%d %d,%d", neww, x, y, width, height));
#endif

	/* make new window fit inside parent */
	if (width == 0) {
		width = parent->width - x;
	} else if (width < 0) {
		width = parent->width + width - x;
	}
	if ((width + x) > parent->width) {
		width = parent->width - x;
	}

	if (height == 0) {
		height = parent->height - y;
	} else if (height < 0) {
		height = parent->height + height - y;
	}
	if ((height + y) > parent->height) {
		height = parent->height - y;
	}

#ifdef FBTK_LOGGING
	LOG(("using %p %d,%d %d,%d", neww, x, y, width, height));
#endif
	/* set values */
	neww->type = type;
	neww->x = x;
	neww->y = y;
	neww->width = width;
	neww->height = height;

	/* insert into widget heiarchy */

	neww->parent = parent;

	if (parent->first_child == NULL) {
		/* no child widgets yet */
		parent->last_child = neww;
	} else {
		/* add new widget to front of sibling chain */
		neww->next = parent->first_child;
		neww->next->prev = neww;
	}
	parent->first_child = neww;

	return neww;
}

/* exported function documented in fbtk.h */
bool
fbtk_get_redraw_pending(fbtk_widget_t *widget)
{
	fbtk_widget_t *root;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	return root->redraw.needed | root->redraw.child;
}

/** Perform a depth-first tree-walk, calling the redraw callback of the widgets in turn.
 *
 * This function makes no decisions of its own and simply walks the
 * widget tree depth first calling widgets redraw callbacks if flagged
 * to do so.
 * The tree search is optimised with a flag to indicate wether the
 * children of a node should be considered.
 */
static int
do_redraw(nsfb_t *nsfb, fbtk_widget_t *widget)
{
	nsfb_bbox_t plot_ctx;
	fbtk_widget_t *cwidget; /* child widget */

	/* check if the widget requires redrawing */
	if (widget->redraw.needed == true) {
		plot_ctx.x0 = fbtk_get_absx(widget) + widget->redraw.x;
		plot_ctx.y0 = fbtk_get_absy(widget) + widget->redraw.y;
		plot_ctx.x1 = plot_ctx.x0 + widget->redraw.width;
		plot_ctx.y1 = plot_ctx.y0 + widget->redraw.height;

#ifdef FBTK_LOGGING
		LOG(("clipping %p %d,%d %d,%d",
		     widget, plot_ctx.x0, plot_ctx.y0,
		     plot_ctx.x1, plot_ctx.y1));
#endif
		if (nsfb_plot_set_clip(nsfb, &plot_ctx) == true) {
			fbtk_post_callback(widget, FBTK_CBT_REDRAW);
		}
		widget->redraw.needed = false;
	}

	/* walk the widgets children if child flag is set */
	if (widget->redraw.child) {
		cwidget = widget->last_child;
		while (cwidget != NULL) {
			do_redraw(nsfb, cwidget);
			cwidget = cwidget->prev;
		}
		widget->redraw.child = false;
	}

	return 1;
}

/* exported function documented in fbtk.h */
int
fbtk_redraw(fbtk_widget_t *widget)
{
	fbtk_widget_t *root;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	return do_redraw(root->u.root.fb, root);
}

/* exported function documented in fbtk.h */
fbtk_callback
fbtk_get_handler(fbtk_widget_t *widget, fbtk_callback_type cbt)
{
	if ((cbt <= FBTK_CBT_START) || (cbt >= FBTK_CBT_END)) {
		/* type out of range, no way to report error so return NULL */
		return NULL;
	}

	return widget->callback[cbt];
}

/* exported function documented in fbtk.h */
fbtk_callback
fbtk_set_handler(fbtk_widget_t *widget,
		 fbtk_callback_type cbt,
		 fbtk_callback cb,
		 void *context)
{
	fbtk_callback prevcb;

	if ((cbt <= FBTK_CBT_START) || (cbt >= FBTK_CBT_END)) {
		/* type out of range, no way to report error so return NULL */
		return NULL;
	}

	prevcb = widget->callback[cbt];

	widget->callback[cbt] = cb;
	widget->callback_context[cbt] = context;

	return prevcb;
}

/* exported function docuemnted in fbtk.h */
int
fbtk_post_callback(fbtk_widget_t *widget, fbtk_callback_type cbt, ...)
{
	fbtk_callback_info cbi;
	int ret = 0;
	va_list ap;

	if (widget == NULL)
		return -1;
	/* if the widget is not mapped do not attempt to post any
	 * events to it
	 */
	if (widget->mapped == false)
		return ret;

	if (widget->callback[cbt] != NULL) {
		cbi.type = cbt;
		cbi.context = widget->callback_context[cbt];

		va_start(ap, cbt);

		switch (cbt) {
		case FBTK_CBT_SCROLLX:
			cbi.x = va_arg(ap,int);
			break;

		case FBTK_CBT_SCROLLY:
			cbi.y = va_arg(ap,int);
			break;

		case FBTK_CBT_CLICK:
			cbi.event = va_arg(ap, void *);
			cbi.x = va_arg(ap, int);
			cbi.y = va_arg(ap, int);
			break;

		case FBTK_CBT_INPUT:
			cbi.event = va_arg(ap, void *);
			break;

		case FBTK_CBT_POINTERMOVE:
			cbi.x = va_arg(ap, int);
			cbi.y = va_arg(ap, int);
			break;

		case FBTK_CBT_REDRAW:
			break;

		case FBTK_CBT_USER:
			break;

		case FBTK_CBT_STRIP_FOCUS:
			break;

		default:
			break;
		}
		va_end(ap);

		ret = (widget->callback[cbt])(widget, &cbi);
	}

	return ret;
}

/* exported function docuemnted in fbtk.h */
void
fbtk_set_focus(fbtk_widget_t *widget)
{
	fbtk_widget_t *root;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	if (root->u.root.input != NULL &&
			root->u.root.input != widget) {
		/* inform previous holder of focus that it's being stripped
		 * of focus */
		fbtk_post_callback(root->u.root.input, FBTK_CBT_STRIP_FOCUS);
	}

	root->u.root.input = widget;
}



/* exported function docuemnted in fbtk.h */
nsfb_t *
fbtk_get_nsfb(fbtk_widget_t *widget)
{
	fbtk_widget_t *root;

	/* ensure we have the root widget */
	root = fbtk_get_root_widget(widget);

	return root->u.root.fb;
}

/* exported function docuemnted in fbtk.h */
fbtk_widget_t *
fbtk_init(nsfb_t *fb)
{
	fbtk_widget_t *root;

	/* create and configure root widget */
	root = calloc(1, sizeof(fbtk_widget_t));
	if (root == NULL)
		return NULL;

	root->type = FB_WIDGET_TYPE_ROOT;
	root->u.root.fb = fb;
	root->u.root.caret.owner = NULL;

	nsfb_get_geometry(fb, &root->width, &root->height, NULL);

	root->width = 720;	/* XXX Hack for HDMI monitor */

	root->mapped = true;

	return root;
}

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
