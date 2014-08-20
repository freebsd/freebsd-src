/*
 * Copyright 2010 Vincent Sanders <vince@simtec.co.uk>
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

#ifndef NETSURF_FB_FBTK_WIDGET_H
#define NETSURF_FB_FBTK_WIDGET_H

#include <stdbool.h>

enum fbtk_widgettype_e {
	FB_WIDGET_TYPE_ROOT = 0,
	FB_WIDGET_TYPE_WINDOW,
	FB_WIDGET_TYPE_BITMAP,
	FB_WIDGET_TYPE_FILL,
	FB_WIDGET_TYPE_TEXT,
	FB_WIDGET_TYPE_HSCROLL,
	FB_WIDGET_TYPE_VSCROLL,
	FB_WIDGET_TYPE_USER,
};


/** Widget description.
 *
 * A widget is an entry in a tree structure which represents a
 * rectangular area with co-ordinates relative to its parent widget.
 * This area has a distinct set of callback operations for handling
 * events which occour within its boundries. A widget may have an
 * arbitrary number of child widgets. The order within the tree
 * determines a widgets z order.
 *
 *                                         ---
 *                                          A
 *                                          |
 *                                  +----------+
 *                             +--->| Button 3 |
 *                             |    +----------+
 *                             |       |    A
 *                             |       V    |
 *                             |    +----------+
 *                             |    | Button 2 |
 *                             |    +----------+
 *                             |       |    A
 *                             |       V    |
 *                             |    +----------+
 *                             |    | Button 1 |
 *                             |    +----------+
 *                             |       |    A
 *                             |       V    |
 *                      ---    |    +----------+
 *                       A     | +->|  Filled  |
 *                       |     | |  +----------+
 *                +----------+ | |     |
 *          +---->|          |-+ |     V
 *          |     | Window 1 |   |    ---  ---
 *          |     |          |---+          A
 *          |     +----------+              |
 *          |        |    A         +----------+               ---
 *          |        |    |    +--->| Button 2 |                A
 *          |        |    |    |    +----------+                |
 *          |        |    |    |       |    A         +-------------+
 *          |        |    |    |       |    |    +--->|  Button Up  |
 *          |        |    |    |       |    |    |    +-------------+
 *          |        |    |    |       |    |    |        |     A
 *          |        |    |    |       |    |    |        V     |
 *          |        |    |    |       |    |    |    +-------------+
 *          |        |    |    |       |    |    |    | Button Down |
 *          |        |    |    |       |    |    |    +-------------+
 *          |        |    |    |       |    |    |        |     A
 *          |        |    |    |       |    |    |        V     |
 *          |        |    |    |       |    |    |    +-------------+
 *          |        |    |    |       |    |    | +->|   Scroller  |
 *          |        |    |    |       V    |    | |  +-------------+
 *          |        |    |    |    +----------+ | |      |
 *          |        |    |    |    |          |-+ |      V
 *          |        |    |    |    | V Scroll |   |     ---
 *          |        |    |    |    |          |---+
 *          |        |    |    |    +----------+
 *          |        |    |    |       |    A
 *          |        |    |    |       V    |
 *          |        |    |    |    +----------+
 *          |        |    |    | +->| Button 1 |
 *          |        |    |    | |  +----------+
 *          |     +----------+ | |     |
 *          |     |          |-+ |     V
 *          |     | Window 2 |   |    ---
 *          |     |          |---+
 *          |     +----------+
 *          |        |    A
 *          |        V    |
 *          |    +------------+
 *     ---  |    | Background |
 *      A   | +->|   Bitmap   |
 *      |   | |  +------------+
 * +------+ | |      |
 * |      |-+ |      V
 * | Root |   |     ---
 * |      |---+
 * +------+
 *   |
 *   V
 *  ---
 *
 * Every widget is contained within this generic wrapper. The
 * integrated union provides for data specific to a widget type.
 */
struct fbtk_widget_s {
	struct fbtk_widget_s *next; /* next lower z ordered widget in tree */
	struct fbtk_widget_s *prev; /* next higher z ordered widget in tree */

	struct fbtk_widget_s *parent; /* parent widget */

	struct fbtk_widget_s *first_child; /* first child widget */
	struct fbtk_widget_s *last_child; /* last child widget */

	/* flags */
	bool mapped; /**< The widget is mapped/visible . */

	/* Generic properties */
	int x;
	int y;
	int width;
	int height;
	colour bg;
	colour fg;

	/* event callback handlers */
	fbtk_callback callback[FBTK_CBT_END];
	void *callback_context[FBTK_CBT_END];

	/* widget redraw */
	struct {
		bool child; /* A child of this widget requires redrawing */
		bool needed; /* the widget requires redrawing */
		int x;
		int y;
		int width;
		int height;
	} redraw;

	enum fbtk_widgettype_e type; /**< The type of the widget */


	union {
		/* toolkit base handle */
		struct {
			nsfb_t *fb;
			struct fbtk_widget_s *prev; /* previous widget pointer wasin */
			struct fbtk_widget_s *grabbed; /* widget that has grabbed pointer movement. */
			struct fbtk_widget_s *input;

			/* caret */
			struct {
				struct fbtk_widget_s *owner; /* widget / NULL */
				int x; /* relative to owner */
				int y; /* relative to owner */
				int height;
				void (*remove_cb)(fbtk_widget_t *widget);
			} caret;
		} root;

		/* bitmap */
		struct {
			struct fbtk_bitmap *bitmap;
		} bitmap;

		/* text */
		struct {
			char* text;
			bool outline;
			fbtk_enter_t enter;
			void *pw;
			int idx; /* caret pos in text */
			int len; /* text length */
			int width; /* text width in px */
			int idx_offset; /* caret pos in pixels */
		} text;

		/* application driven widget */
		struct {
			void *pw; /* private data for user widget */
		} user;

		struct {
			int minimum; /* lowest value of scrollbar */
			int maximum; /* highest value of scrollbar */
			int thumb; /* size of bar representing a page */
			int page; /* amount to page document */
			int position; /* position of bar */
			int drag; /* offset to start of drag */
			int drag_position; /* indicator bar pos at drag start */
			struct fbtk_widget_s *btnul; /* scroll button up/left */
			struct fbtk_widget_s *btndr; /* scroll button down/right*/
		} scroll;

	} u;
};


/* These functions are not considered part of the public API but are
 * not static as they are used by the higher level widget provision
 * routines
 */


/** creates a new widget and insert it into to hierachy.
 *
 * The widget is set to defaults of false, 0 or NULL.
 *
 * @param parent The parent widget. The new widget will be added with
 *               the shallowest z order relative to its siblings.
 * @param type The type of the widget.
 * @param x The x co-ordinate relative to the parent widget.
 * @param y The y co-ordinate relative to the parent widget.
 * @param width the widgets width. This will be clipped to the parent, if
 *          the value is 0 the largest extent which can fit within the parent
 *          is used, if the value is negative the largest value that will fit
 *          within the parent less the value given will be used.
 * @param height the widgets width. This will be clipped to the parent, if
 *          the value is 0 the largest extent which can fit within the parent
 *          is used, if the value is negative the largest value that will fit
 *          within the parent less the value given will be used.
 */
fbtk_widget_t *fbtk_widget_new(fbtk_widget_t *parent, enum fbtk_widgettype_e type, int x, int y, int width, int height);

/** find the root widget from any widget in the toolkit hierarchy.
 *
 * @param widget Any widget.
 * @return The root widget or NULL if \a widget was not valid.
 */
fbtk_widget_t *fbtk_get_root_widget(fbtk_widget_t *widget);

/** set pointer to bitmap in context.
 *
 * widget helper callback to set cursor image to the bitmap passed in
 * the callbacks private data.
 */
int fbtk_set_ptr(fbtk_widget_t *widget, fbtk_callback_info *cbi);

#endif

/*
 * Local Variables:
 * c-basic-offset:8
 * End:
 */
