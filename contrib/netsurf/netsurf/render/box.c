/*
 * Copyright 2005-2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
 * Box tree manipulation (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <dom/dom.h>
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "css/utils.h"
#include "css/dump.h"
#include "desktop/scrollbar.h"
#include "utils/nsoption.h"
#include "render/box.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "utils/log.h"
#include "utils/talloc.h"
#include "utils/utils.h"

static bool box_contains_point(struct box *box, int x, int y, bool *physically);
static bool box_nearer_text_box(struct box *box, int bx, int by,
		int x, int y, int dir, struct box **nearest, int *tx, int *ty,
		int *nr_xd, int *nr_yd);
static bool box_nearest_text_box(struct box *box, int bx, int by,
		int fx, int fy, int x, int y, int dir, struct box **nearest,
		int *tx, int *ty, int *nr_xd, int *nr_yd);

#define box_is_float(box) (box->type == BOX_FLOAT_LEFT || \
		box->type == BOX_FLOAT_RIGHT)

/**
 * Destructor for box nodes which own styles
 *
 * \param b The box being destroyed.
 * \return 0 to allow talloc to continue destroying the tree.
 */
static int box_talloc_destructor(struct box *b)
{
	struct html_scrollbar_data *data;

	if ((b->flags & STYLE_OWNED) && b->style != NULL) {
		css_computed_style_destroy(b->style);
		b->style = NULL;
	}
	
	if (b->styles != NULL) {
		css_select_results_destroy(b->styles);
		b->styles = NULL;
	}

	if (b->href != NULL)
		nsurl_unref(b->href);

	if (b->id != NULL) {
		lwc_string_unref(b->id);
	}

	if (b->node != NULL) {
		dom_node_unref(b->node);
	}

	if (b->scroll_x != NULL) {
		data = scrollbar_get_data(b->scroll_x);
		scrollbar_destroy(b->scroll_x);
		free(data);
	}

	if (b->scroll_y != NULL) {
		data = scrollbar_get_data(b->scroll_y);
		scrollbar_destroy(b->scroll_y);
		free(data);
	}

	return 0;
}

/**
 * Create a box tree node.
 *
 * \param  styles       selection results for the box, or NULL
 * \param  style        computed style for the box (not copied), or 0
 * \param  style_owned  whether style is owned by this box
 * \param  href         href for the box (copied), or 0
 * \param  target       target for the box (not copied), or 0
 * \param  title        title for the box (not copied), or 0
 * \param  id           id for the box (not copied), or 0
 * \param  context      context for allocations
 * \return  allocated and initialised box, or 0 on memory exhaustion
 *
 * styles is always owned by the box, if it is set.
 * style is only owned by the box in the case of implied boxes.
 */

struct box * box_create(css_select_results *styles, css_computed_style *style,
		bool style_owned, nsurl *href, const char *target, 
		const char *title, lwc_string *id, void *context)
{
	unsigned int i;
	struct box *box;

	box = talloc(context, struct box);
	if (!box) {
		return 0;
	}

	talloc_set_destructor(box, box_talloc_destructor);

	box->type = BOX_INLINE;
	box->flags = 0;
	box->flags = style_owned ? (box->flags | STYLE_OWNED) : box->flags;
	box->styles = styles;
	box->style = style;
	box->x = box->y = 0;
	box->width = UNKNOWN_WIDTH;
	box->height = 0;
	box->descendant_x0 = box->descendant_y0 = 0;
	box->descendant_x1 = box->descendant_y1 = 0;
	for (i = 0; i != 4; i++)
		box->margin[i] = box->padding[i] = box->border[i].width = 0;
	box->scroll_x = box->scroll_y = NULL;
	box->min_width = 0;
	box->max_width = UNKNOWN_MAX_WIDTH;
	box->byte_offset = 0;
	box->text = NULL;
	box->length = 0;
	box->space = 0;
	box->href = (href == NULL) ? NULL : nsurl_ref(href);
	box->target = target;
	box->title = title;
	box->columns = 1;
	box->rows = 1;
	box->start_column = 0;
	box->next = NULL;
	box->prev = NULL;
	box->children = NULL;
	box->last = NULL;
	box->parent = NULL;
	box->inline_end = NULL;
	box->float_children = NULL;
	box->float_container = NULL;
	box->next_float = NULL;
	box->list_marker = NULL;
	box->col = NULL;
	box->gadget = NULL;
	box->usemap = NULL;
	box->id = id;
	box->background = NULL;
	box->object = NULL;
	box->object_params = NULL;
	box->iframe = NULL;
	box->node = NULL;

	return box;
}

/**
 * Add a child to a box tree node.
 *
 * \param  parent  box giving birth
 * \param  child   box to link as last child of parent
 */

void box_add_child(struct box *parent, struct box *child)
{
	assert(parent);
	assert(child);

	if (parent->children != 0) {	/* has children already */
		parent->last->next = child;
		child->prev = parent->last;
	} else {			/* this is the first child */
		parent->children = child;
		child->prev = 0;
	}

	parent->last = child;
	child->parent = parent;
}


/**
 * Insert a new box as a sibling to a box in a tree.
 *
 * \param  box      box already in tree
 * \param  new_box  box to link into tree as next sibling
 */

void box_insert_sibling(struct box *box, struct box *new_box)
{
	new_box->parent = box->parent;
	new_box->prev = box;
	new_box->next = box->next;
	box->next = new_box;
	if (new_box->next)
		new_box->next->prev = new_box;
	else if (new_box->parent)
		new_box->parent->last = new_box;
}


/**
 * Unlink a box from the box tree and then free it recursively.
 *
 * \param  box  box to unlink and free recursively.
 */

void box_unlink_and_free(struct box *box)
{
	struct box *parent = box->parent;
	struct box *next = box->next;
	struct box *prev = box->prev;

	if (parent) {
		if (parent->children == box)
			parent->children = next;
		if (parent->last == box)
			parent->last = next ? next : prev;
	}

	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;

	box_free(box);
}


/**
 * Free a box tree recursively.
 *
 * \param  box  box to free recursively
 *
 * The box and all its children is freed.
 */

void box_free(struct box *box)
{
	struct box *child, *next;

	/* free children first */
	for (child = box->children; child; child = next) {
		next = child->next;
		box_free(child);
	}

	/* last this box */
	box_free_box(box);
}


/**
 * Free the data in a single box structure.
 *
 * \param  box  box to free
 */

void box_free_box(struct box *box)
{
	if (!(box->flags & CLONE)) {
		if (box->gadget)
			form_free_control(box->gadget);
		if (box->scroll_x != NULL)
			scrollbar_destroy(box->scroll_x);
		if (box->scroll_y != NULL)
			scrollbar_destroy(box->scroll_y);
		if (box->styles != NULL)
			css_select_results_destroy(box->styles);
	}

	talloc_free(box);
}


/**
 * Find the absolute coordinates of a box.
 *
 * \param  box  the box to calculate coordinates of
 * \param  x    updated to x coordinate
 * \param  y    updated to y coordinate
 */

void box_coords(struct box *box, int *x, int *y)
{
	*x = box->x;
	*y = box->y;
	while (box->parent) {
		if (box_is_float(box)) {
			do {
				box = box->parent;
			} while (!box->float_children);
		} else
			box = box->parent;
		*x += box->x - scrollbar_get_offset(box->scroll_x);
		*y += box->y - scrollbar_get_offset(box->scroll_y);
	}
}


/**
 * Find the bounds of a box.
 *
 * \param  box  the box to calculate bounds of
 * \param  r    receives bounds
 */

void box_bounds(struct box *box, struct rect *r)
{
	int width, height;

	box_coords(box, &r->x0, &r->y0);

	width = box->padding[LEFT] + box->width + box->padding[RIGHT];
	height = box->padding[TOP] + box->height + box->padding[BOTTOM];

	r->x1 = r->x0 + width;
	r->y1 = r->y0 + height;
}


/**
 * Find the boxes at a point.
 *
 * \param  box      box to search children of
 * \param  x        point to find, in global document coordinates
 * \param  y        point to find, in global document coordinates
 * \param  box_x    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \param  box_y    position of box, in global document coordinates, updated
 *                  to position of returned box, if any
 * \return  box at given point, or 0 if none found
 *
 * To find all the boxes in the hierarchy at a certain point, use code like
 * this:
 * \code
 *	struct box *box = top_of_document_to_search;
 *	int box_x = 0, box_y = 0;
 *
 *	while ((box = box_at_point(box, x, y, &box_x, &box_y))) {
 *		// process box
 *	}
 * \endcode
 */

struct box *box_at_point(struct box *box, const int x, const int y,
		int *box_x, int *box_y)
{
	int bx = *box_x, by = *box_y;
	struct box *child, *sibling;
	bool physically;

	assert(box);

	/* consider floats first, since they will often overlap other boxes */
	for (child = box->float_children; child; child = child->next_float) {
		if (box_contains_point(child, x - bx, y - by, &physically)) {
			*box_x = bx + child->x -
					scrollbar_get_offset(child->scroll_x);
			*box_y = by + child->y -
					scrollbar_get_offset(child->scroll_y);

			if (physically)
				return child;
			else
				return box_at_point(child, x, y, box_x, box_y);
		}
	}

non_float_children:
	/* non-float children */
	for (child = box->children; child; child = child->next) {
		if (box_is_float(child))
			continue;
		if (box_contains_point(child, x - bx, y - by, &physically)) {
			*box_x = bx + child->x -
					scrollbar_get_offset(child->scroll_x);
			*box_y = by + child->y -
					scrollbar_get_offset(child->scroll_y);

			if (physically)
				return child;
			else
				return box_at_point(child, x, y, box_x, box_y);
		}
	}

	/* marker boxes */
	if (box->list_marker) {
		if (box_contains_point(box->list_marker, x - bx, y - by,
				&physically)) {
			*box_x = bx + box->list_marker->x;
			*box_y = by + box->list_marker->y;
			return box->list_marker;
		}
	}

	/* siblings and siblings of ancestors */
	while (box) {
		if (box_is_float(box)) {
			bx -= box->x - scrollbar_get_offset(box->scroll_x);
			by -= box->y - scrollbar_get_offset(box->scroll_y);
			for (sibling = box->next_float; sibling;
					sibling = sibling->next_float) {
				if (box_contains_point(sibling,
						x - bx, y - by, &physically)) {
					*box_x = bx + sibling->x -
							scrollbar_get_offset(
							sibling->scroll_x);
					*box_y = by + sibling->y -
							scrollbar_get_offset(
							sibling->scroll_y);

					if (physically)
						return sibling;
					else
						return box_at_point(sibling,
								x, y,
								box_x, box_y);
				}
			}
			/* ascend to float's parent */
			do {
				box = box->parent;
			} while (!box->float_children);
			/* process non-float children of float's parent */
			goto non_float_children;

		} else {
			bx -= box->x - scrollbar_get_offset(box->scroll_x);
			by -= box->y - scrollbar_get_offset(box->scroll_y);
			for (sibling = box->next; sibling;
					sibling = sibling->next) {
				if (box_is_float(sibling))
					continue;
				if (box_contains_point(sibling, x - bx, y - by,
						&physically)) {
					*box_x = bx + sibling->x -
							scrollbar_get_offset(
							sibling->scroll_x);
					*box_y = by + sibling->y -
							scrollbar_get_offset(
							sibling->scroll_y);

					if (physically)
						return sibling;
					else
						return box_at_point(sibling,
								x, y,
								box_x, box_y);
				}
			}
			box = box->parent;
		}
	}

	return 0;
}


/**
 * Determine if a point lies within a box.
 *
 * \param  box		box to consider
 * \param  x		coordinate relative to box parent
 * \param  y		coordinate relative to box parent
 * \param  physically	if function returning true, physically is set true if
 *			point is within the box's physical dimensions and false
 *			if the point is not within the box's physical dimensions
 *			but is in the area defined by the box's descendants.
 *			if function returning false, physically is undefined.
 * \return  true if the point is within the box or a descendant box
 *
 * This is a helper function for box_at_point().
 */

bool box_contains_point(struct box *box, int x, int y, bool *physically)
{
	css_computed_clip_rect css_rect;

	if (box->style != NULL &&
			css_computed_position(box->style) ==
					CSS_POSITION_ABSOLUTE &&
			css_computed_clip(box->style, &css_rect) ==
					CSS_CLIP_RECT) {
		/* We have an absolutly positioned box with a clip rect */
		struct rect r = {
			.x0 = box->x - box->border[LEFT].width,
			.y0 = box->y - box->border[TOP].width,
			.x1 = box->x + box->padding[LEFT] + box->width +
					box->border[RIGHT].width +
					box->padding[RIGHT],
			.y1 = box->y + box->padding[TOP] + box->height +
					box->border[BOTTOM].width +
					box->padding[BOTTOM]
		};
		if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1)
			*physically = true;
		else
			*physically = false;

		/* Adjust rect to css clip region */
		if (css_rect.left_auto == false) {
			r.x0 += FIXTOINT(nscss_len2px(
					css_rect.left, css_rect.lunit,
					box->style));
		}
		if (css_rect.top_auto == false) {
			r.y0 += FIXTOINT(nscss_len2px(
					css_rect.top, css_rect.tunit,
					box->style));
		}
		if (css_rect.right_auto == false) {
			r.x1 = box->x - box->border[LEFT].width +
					FIXTOINT(nscss_len2px(
							css_rect.right,
							css_rect.runit,
							box->style));
		}
		if (css_rect.bottom_auto == false) {
			r.y1 = box->y - box->border[TOP].width +
					FIXTOINT(nscss_len2px(
							css_rect.bottom,
							css_rect.bunit,
							box->style));
		}

		/* Test if point is in clipped box */
		if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) {
			/* inside clip area */
			return true;
		}

		/* Not inside clip area */
		return false;
	}
	if (box->x <= x + box->border[LEFT].width &&
			x < box->x + box->padding[LEFT] + box->width +
			box->border[RIGHT].width + box->padding[RIGHT] &&
			box->y <= y + box->border[TOP].width &&
			y < box->y + box->padding[TOP] + box->height +
			box->border[BOTTOM].width + box->padding[BOTTOM]) {
		*physically = true;
		return true;
	}
	if (box->list_marker && box->list_marker->x <= x +
			box->list_marker->border[LEFT].width &&
			x < box->list_marker->x +
			box->list_marker->padding[LEFT] +
			box->list_marker->width +
			box->list_marker->border[RIGHT].width +
			box->list_marker->padding[RIGHT] &&
			box->list_marker->y <= y +
			box->list_marker->border[TOP].width &&
			y < box->list_marker->y +
			box->list_marker->padding[TOP] +
			box->list_marker->height +
			box->list_marker->border[BOTTOM].width +
			box->list_marker->padding[BOTTOM]) {
		*physically = true;
		return true;
	}
	if ((box->style && css_computed_overflow(box->style) == 
			CSS_OVERFLOW_VISIBLE) || !box->style) {
		if (box->x + box->descendant_x0 <= x &&
				x < box->x + box->descendant_x1 &&
				box->y + box->descendant_y0 <= y &&
				y < box->y + box->descendant_y1) {
			*physically = false;
			return true;
		}
	}
	return false;
}


/**
 * Check whether box is nearer mouse coordinates than current nearest box
 *
 * \param  box      box to test
 * \param  bx	    position of box, in global document coordinates
 * \param  by	    position of box, in global document coordinates
 * \param  x	    mouse point, in global document coordinates
 * \param  y	    mouse point, in global document coordinates
 * \param  dir      direction in which to search (-1 = above-left,
 *						  +1 = below-right)
 * \param  nearest  nearest text box found, or NULL if none
 *		    updated if box is nearer than existing nearest
 * \param  tx	    position of text_box, in global document coordinates
 *		    updated if box is nearer than existing nearest
 * \param  ty	    position of text_box, in global document coordinates
 *		    updated if box is nearer than existing nearest
 * \param  nr_xd    distance to nearest text box found
 *		    updated if box is nearer than existing nearest
 * \param  ny_yd    distance to nearest text box found
 *		    updated if box is nearer than existing nearest
 * \return true if mouse point is inside box
 */

bool box_nearer_text_box(struct box *box, int bx, int by,
		int x, int y, int dir, struct box **nearest, int *tx, int *ty,
		int *nr_xd, int *nr_yd)
{
	int w = box->padding[LEFT] + box->width + box->padding[RIGHT];
	int h = box->padding[TOP] + box->height + box->padding[BOTTOM];
	int y1 = by + h;
	int x1 = bx + w;
	int yd = INT_MAX;
	int xd = INT_MAX;

	if (x >= bx && x1 > x && y >= by && y1 > y) {
		*nearest = box;
		*tx = bx;
		*ty = by;
		return true;
	}

	if (box->parent->list_marker != box) {
		if (dir < 0) {
			/* consider only those children (partly) above-left */
			if (by <= y && bx < x) {
				yd = y <= y1 ? 0 : y - y1;
				xd = x <= x1 ? 0 : x - x1;
			}
		} else {
			/* consider only those children (partly) below-right */
			if (y1 > y && x1 > x) {
				yd = y > by ? 0 : by - y;
				xd = x > bx ? 0 : bx - x;
			}
		}

		/* give y displacement precedence over x */
		if (yd < *nr_yd || (yd == *nr_yd && xd <= *nr_xd)) {
			*nr_yd = yd;
			*nr_xd = xd;
			*nearest = box;
			*tx = bx;
			*ty = by;
		}
	}
	return false;
}


/**
 * Pick the text box child of 'box' that is closest to and above-left
 * (dir -ve) or below-right (dir +ve) of the point 'x,y'
 *
 * \param  box      parent box
 * \param  bx	    position of box, in global document coordinates
 * \param  by	    position of box, in global document coordinates
 * \param  fx	    position of float parent, in global document coordinates
 * \param  fy	    position of float parent, in global document coordinates
 * \param  x	    mouse point, in global document coordinates
 * \param  y	    mouse point, in global document coordinates
 * \param  dir      direction in which to search (-1 = above-left,
 *						  +1 = below-right)
 * \param  nearest  nearest text box found, or NULL if none
 *		    updated if a descendant of box is nearer than old nearest
 * \param  tx	    position of nearest, in global document coordinates
 *		    updated if a descendant of box is nearer than old nearest
 * \param  ty	    position of nearest, in global document coordinates
 *		    updated if a descendant of box is nearer than old nearest
 * \param  nr_xd    distance to nearest text box found
 *		    updated if a descendant of box is nearer than old nearest
 * \param  ny_yd    distance to nearest text box found
 *		    updated if a descendant of box is nearer than old nearest
 * \return true if mouse point is inside text_box
 */

bool box_nearest_text_box(struct box *box, int bx, int by,
		int fx, int fy, int x, int y, int dir, struct box **nearest,
		int *tx, int *ty, int *nr_xd, int *nr_yd)
{
	struct box *child = box->children;
	int c_bx, c_by;
	int c_fx, c_fy;
	bool in_box = false;

	if (*nearest == NULL) {
		*nr_xd = INT_MAX / 2; /* displacement of 'nearest so far' */
		*nr_yd = INT_MAX / 2;
	}
	if (box->type == BOX_INLINE_CONTAINER) {
		int bw = box->padding[LEFT] + box->width + box->padding[RIGHT];
		int bh = box->padding[TOP] + box->height + box->padding[BOTTOM];
		int b_y1 = by + bh;
		int b_x1 = bx + bw;
		if (x >= bx && b_x1 > x && y >= by && b_y1 > y) {
			in_box = true;
		}
	}

	while (child) {
		if (child->type == BOX_FLOAT_LEFT ||
				child->type == BOX_FLOAT_RIGHT) {
			c_bx = fx + child->x -
					scrollbar_get_offset(child->scroll_x);
			c_by = fy + child->y -
					scrollbar_get_offset(child->scroll_y);
		} else {
			c_bx = bx + child->x -
					scrollbar_get_offset(child->scroll_x);
			c_by = by + child->y -
					scrollbar_get_offset(child->scroll_y);
		}
		if (child->float_children) {
			c_fx = c_bx;
			c_fy = c_by;
		} else {
			c_fx = fx;
			c_fy = fy;
		}
		if (in_box && child->text && !child->object) {
			if (box_nearer_text_box(child,
					c_bx, c_by, x, y, dir, nearest,
					tx, ty, nr_xd, nr_yd))
				return true;
		} else {
			if (child->list_marker) {
				if (box_nearer_text_box(
						child->list_marker,
						c_bx + child->list_marker->x,
						c_by + child->list_marker->y,
						x, y, dir, nearest,
						tx, ty, nr_xd, nr_yd))
					return true;
			}
			if (box_nearest_text_box(child, c_bx, c_by,
					c_fx, c_fy, x, y, dir, nearest, tx, ty,
					nr_xd, nr_yd))
				return true;
		}
		child = child->next;
	}

	return false;
}


/**
 * Peform pick text on browser window contents to locate the box under
 * the mouse pointer, or nearest in the given direction if the pointer is
 * not over a text box.
 *
 * \param html	an HTML content
 * \param x	coordinate of mouse
 * \param y	coordinate of mouse
 * \param dir	direction to search (-1 = above-left, +1 = below-right)
 * \param dx	receives x ordinate of mouse relative to text box
 * \param dy	receives y ordinate of mouse relative to text box
 */

struct box *box_pick_text_box(struct html_content *html,
		int x, int y, int dir, int *dx, int *dy)
{
	struct box *text_box = NULL;
	struct box *box;
	int nr_xd, nr_yd;
	int bx, by;
	int fx, fy;
	int tx, ty;

	if (html == NULL)
		return NULL;

	box = html->layout;
	bx = box->margin[LEFT];
	by = box->margin[TOP];
	fx = bx;
	fy = by;

	if (!box_nearest_text_box(box, bx, by, fx, fy, x, y,
			dir, &text_box, &tx, &ty, &nr_xd, &nr_yd)) {
		if (text_box && text_box->text && !text_box->object) {
			int w = (text_box->padding[LEFT] +
					text_box->width +
					text_box->padding[RIGHT]);
			int h = (text_box->padding[TOP] +
					text_box->height +
					text_box->padding[BOTTOM]);
			int x1, y1;

			y1 = ty + h;
			x1 = tx + w;

			/* ensure point lies within the text box */
			if (x < tx) x = tx;
			if (y < ty) y = ty;
			if (y > y1) y = y1;
			if (x > x1) x = x1;
		}
	}

	/* return coordinates relative to box */
	*dx = x - tx;
	*dy = y - ty;

	return text_box;
}


/**
 * Find a box based upon its id attribute.
 *
 * \param  box  box tree to search
 * \param  id   id to look for
 * \return  the box or 0 if not found
 */

struct box *box_find_by_id(struct box *box, lwc_string *id)
{
	struct box *a, *b;
	bool m;

	if (box->id != NULL &&
			lwc_string_isequal(id, box->id, &m) == lwc_error_ok &&
			m == true)
		return box;

	for (a = box->children; a; a = a->next) {
		if ((b = box_find_by_id(a, id)) != NULL)
			return b;
	}

	return NULL;
}


/**
 * Determine if a box is visible when the tree is rendered.
 *
 * \param  box  box to check
 * \return  true iff the box is rendered
 */

bool box_visible(struct box *box)
{
	/* visibility: hidden */
	if (box->style && css_computed_visibility(box->style) == 
			CSS_VISIBILITY_HIDDEN)
		return false;

	return true;
}


/**
 * Print a box tree to a file.
 */

void box_dump(FILE *stream, struct box *box, unsigned int depth)
{
	unsigned int i;
	struct box *c, *prev;

	for (i = 0; i != depth; i++)
		fprintf(stream, "  ");

	fprintf(stream, "%p ", box);
	fprintf(stream, "x%i y%i w%i h%i ", box->x, box->y,
			box->width, box->height);
	if (box->max_width != UNKNOWN_MAX_WIDTH)
		fprintf(stream, "min%i max%i ", box->min_width, box->max_width);
	fprintf(stream, "(%i %i %i %i) ",
			box->descendant_x0, box->descendant_y0,
			box->descendant_x1, box->descendant_y1);

	fprintf(stream, "m(%i %i %i %i) ",
			box->margin[TOP], box->margin[LEFT],
			box->margin[BOTTOM], box->margin[RIGHT]);

	switch (box->type) {
	case BOX_BLOCK:            fprintf(stream, "BLOCK "); break;
	case BOX_INLINE_CONTAINER: fprintf(stream, "INLINE_CONTAINER "); break;
	case BOX_INLINE:           fprintf(stream, "INLINE "); break;
	case BOX_INLINE_END:       fprintf(stream, "INLINE_END "); break;
	case BOX_INLINE_BLOCK:     fprintf(stream, "INLINE_BLOCK "); break;
	case BOX_TABLE:            fprintf(stream, "TABLE [columns %i] ",
					   box->columns); break;
	case BOX_TABLE_ROW:        fprintf(stream, "TABLE_ROW "); break;
	case BOX_TABLE_CELL:       fprintf(stream, "TABLE_CELL [columns %i, "
					   "start %i, rows %i] ", box->columns,
					   box->start_column, box->rows); break;
	case BOX_TABLE_ROW_GROUP:  fprintf(stream, "TABLE_ROW_GROUP "); break;
	case BOX_FLOAT_LEFT:       fprintf(stream, "FLOAT_LEFT "); break;
	case BOX_FLOAT_RIGHT:      fprintf(stream, "FLOAT_RIGHT "); break;
	case BOX_BR:               fprintf(stream, "BR "); break;
	case BOX_TEXT:             fprintf(stream, "TEXT "); break;
	default:                   fprintf(stream, "Unknown box type ");
	}

	if (box->text)
		fprintf(stream, "%li '%.*s' ", (unsigned long) box->byte_offset,
				(int) box->length, box->text);
	if (box->space)
		fprintf(stream, "space ");
	if (box->object) {
		fprintf(stream, "(object '%s') ", 
				nsurl_access(hlcache_handle_get_url(box->object)));
	}
	if (box->iframe) {
		fprintf(stream, "(iframe) ");
	}
	if (box->gadget)
		fprintf(stream, "(gadget) ");
	if (box->style)
		nscss_dump_computed_style(stream, box->style);
	if (box->href)
		fprintf(stream, " -> '%s'", nsurl_access(box->href));
	if (box->target)
		fprintf(stream, " |%s|", box->target);
	if (box->title)
		fprintf(stream, " [%s]", box->title);
	if (box->id)
		fprintf(stream, " <%s>", lwc_string_data(box->id));
	if (box->type == BOX_INLINE || box->type == BOX_INLINE_END)
		fprintf(stream, " inline_end %p", box->inline_end);
	if (box->float_children)
		fprintf(stream, " float_children %p", box->float_children);
	if (box->next_float)
		fprintf(stream, " next_float %p", box->next_float);
	if (box->col) {
		fprintf(stream, " (columns");
		for (i = 0; i != box->columns; i++)
			fprintf(stream, " (%s %s %i %i %i)",
					((const char *[]) {"UNKNOWN", "FIXED",
					"AUTO", "PERCENT", "RELATIVE"})
					[box->col[i].type],
					((const char *[]) {"normal",
					"positioned"})
					[box->col[i].positioned],
					box->col[i].width,
					box->col[i].min, box->col[i].max);
		fprintf(stream, ")");
	}
	fprintf(stream, "\n");

	if (box->list_marker) {
		for (i = 0; i != depth; i++)
			fprintf(stream, "  ");
		fprintf(stream, "list_marker:\n");
		box_dump(stream, box->list_marker, depth + 1);
	}

	for (c = box->children; c && c->next; c = c->next)
		;
	if (box->last != c)
		fprintf(stream, "warning: box->last %p (should be %p) "
				"(box %p)\n", box->last, c, box);
	for (prev = 0, c = box->children; c; prev = c, c = c->next) {
		if (c->parent != box)
			fprintf(stream, "warning: box->parent %p (should be "
					"%p) (box on next line)\n",
					c->parent, box);
		if (c->prev != prev)
			fprintf(stream, "warning: box->prev %p (should be "
					"%p) (box on next line)\n",
					c->prev, prev);
		box_dump(stream, c, depth + 1);
	}
}

/**
 * Applies the given scroll setup to a box. This includes scroll
 * creation/deletion as well as scroll dimension updates.
 *
 * \param c		content in which the box is located
 * \param box		the box to handle the scrolls for
 * \param bottom	whether the horizontal scrollbar should be present
 * \param right		whether the vertical scrollbar should be present
 * \return		true on success false otherwise
 */
bool box_handle_scrollbars(struct content *c, struct box *box,
		bool bottom, bool right)
{
	struct html_scrollbar_data *data;
	int visible_width, visible_height;
	int full_width, full_height;

	if (!bottom && box->scroll_x != NULL) {
		data = scrollbar_get_data(box->scroll_x);
		scrollbar_destroy(box->scroll_x);
		free(data);
		box->scroll_x = NULL;
	}

	if (!right && box->scroll_y != NULL) {
		data = scrollbar_get_data(box->scroll_y);
		scrollbar_destroy(box->scroll_y);
		free(data);
		box->scroll_y = NULL;
	}

	if (!bottom && !right)
		return true;

	visible_width = box->width + box->padding[RIGHT] + box->padding[LEFT];
	visible_height = box->height + box->padding[TOP] + box->padding[BOTTOM];

	full_width = ((box->descendant_x1 - box->border[RIGHT].width) >
			visible_width) ?
			box->descendant_x1 + box->padding[RIGHT] :
			visible_width;
	full_height = ((box->descendant_y1 - box->border[BOTTOM].width) >
			visible_height) ?
			box->descendant_y1 + box->padding[BOTTOM] :
			visible_height;

	if (right) {
		if (box->scroll_y == NULL) {
			data = malloc(sizeof(struct html_scrollbar_data));
			if (data == NULL) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				return false;
			}
			data->c = c;
			data->box = box;
			if (!scrollbar_create(false, visible_height,
					full_height, visible_height,
					data, html_overflow_scroll_callback,
					&(box->scroll_y)))
				return false;
		} else  {
			scrollbar_set_extents(box->scroll_y, visible_height,
					visible_height, full_height);
		}
	}
	if (bottom) {
		if (box->scroll_x == NULL) {
			data = malloc(sizeof(struct html_scrollbar_data));
			if (data == NULL) {
				LOG(("malloc failed"));
				warn_user("NoMemory", 0);
				return false;
			}
			data->c = c;
			data->box = box;
			if (!scrollbar_create(true,
					visible_width -
					(right ? SCROLLBAR_WIDTH : 0),
					full_width, visible_width,
					data, html_overflow_scroll_callback,
					&box->scroll_x))
				return false;
		} else {
			scrollbar_set_extents(box->scroll_x,
					visible_width -
					(right ? SCROLLBAR_WIDTH : 0),
					visible_width, full_width);
		}
	}
	
	if (right && bottom)
		scrollbar_make_pair(box->scroll_x, box->scroll_y);
	
	return true;
}

/**
 * Determine if a box has a vertical scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a vertical scrollbar
 */

bool box_vscrollbar_present(const struct box * const box)
{
	return box->padding[TOP] + box->height + box->padding[BOTTOM] +
			box->border[BOTTOM].width < box->descendant_y1;
}


/**
 * Determine if a box has a horizontal scrollbar.
 *
 * \param  box  scrolling box
 * \return the box has a horizontal scrollbar
 */

bool box_hscrollbar_present(const struct box * const box)
{
	return box->padding[LEFT] + box->width + box->padding[RIGHT] +
			box->border[RIGHT].width < box->descendant_x1;
}

