/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
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
 * Box tree construction and manipulation (interface).
 *
 * This stage of rendering converts a tree of dom_nodes (produced by libdom)
 * to a tree of struct box. The box tree represents the structure of the
 * document as given by the CSS display and float properties.
 *
 * For example, consider the following HTML:
 * \code
 *   <h1>Example Heading</h1>
 *   <p>Example paragraph <em>with emphasised text</em> etc.</p>       \endcode
 *
 * This would produce approximately the following box tree with default CSS
 * rules:
 * \code
 *   BOX_BLOCK (corresponds to h1)
 *     BOX_INLINE_CONTAINER
 *       BOX_INLINE "Example Heading"
 *   BOX_BLOCK (p)
 *     BOX_INLINE_CONTAINER
 *       BOX_INLINE "Example paragraph "
 *       BOX_INLINE "with emphasised text" (em)
 *       BOX_INLINE "etc."                                             \endcode
 *
 * Note that the em has been collapsed into the INLINE_CONTAINER.
 *
 * If these CSS rules were applied:
 * \code
 *   h1 { display: table-cell }
 *   p { display: table-cell }
 *   em { float: left; width: 5em }                                    \endcode
 *
 * then the box tree would instead look like this:
 * \code
 *   BOX_TABLE
 *     BOX_TABLE_ROW_GROUP
 *       BOX_TABLE_ROW
 *         BOX_TABLE_CELL (h1)
 *           BOX_INLINE_CONTAINER
 *             BOX_INLINE "Example Heading"
 *         BOX_TABLE_CELL (p)
 *           BOX_INLINE_CONTAINER
 *             BOX_INLINE "Example paragraph "
 *             BOX_FLOAT_LEFT (em)
 *               BOX_BLOCK
 *                 BOX_INLINE_CONTAINER
 *                   BOX_INLINE "with emphasised text"
 *             BOX_INLINE "etc."                                       \endcode
 *
 * Here implied boxes have been added and a float is present.
 *
 * A box tree is "normalized" if the following is satisfied:
 * \code
 * parent               permitted child nodes
 * BLOCK, INLINE_BLOCK  BLOCK, INLINE_CONTAINER, TABLE
 * INLINE_CONTAINER     INLINE, INLINE_BLOCK, FLOAT_LEFT, FLOAT_RIGHT, BR, TEXT,
 *                      INLINE_END
 * INLINE               none
 * TABLE                at least 1 TABLE_ROW_GROUP
 * TABLE_ROW_GROUP      at least 1 TABLE_ROW
 * TABLE_ROW            at least 1 TABLE_CELL
 * TABLE_CELL           BLOCK, INLINE_CONTAINER, TABLE (same as BLOCK)
 * FLOAT_(LEFT|RIGHT)   exactly 1 BLOCK or TABLE
 * \endcode
 */

#ifndef _NETSURF_RENDER_BOX_H_
#define _NETSURF_RENDER_BOX_H_

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>

#include "css/css.h"
#include "utils/nsurl.h"
#include "utils/types.h"

struct content;
struct box;
struct browser_window;
struct column;
struct object_params;
struct object_param;
struct html_content;

struct dom_node;

#define UNKNOWN_WIDTH INT_MAX
#define UNKNOWN_MAX_WIDTH INT_MAX

typedef void (*box_construct_complete_cb)(struct html_content *c, bool success);

/** Type of a struct box. */
typedef enum {
	BOX_BLOCK, BOX_INLINE_CONTAINER, BOX_INLINE,
	BOX_TABLE, BOX_TABLE_ROW, BOX_TABLE_CELL,
	BOX_TABLE_ROW_GROUP,
	BOX_FLOAT_LEFT, BOX_FLOAT_RIGHT,
	BOX_INLINE_BLOCK, BOX_BR, BOX_TEXT,
	BOX_INLINE_END, BOX_NONE
} box_type;


/** Flags for a struct box. */
typedef enum {
	NEW_LINE    = 1 << 0,	/* first inline on a new line */
	STYLE_OWNED = 1 << 1,	/* style is owned by this box */
	PRINTED     = 1 << 2,	/* box has already been printed */
	PRE_STRIP   = 1 << 3,	/* PRE tag needing leading newline stripped */
	CLONE       = 1 << 4,	/* continuation of previous box from wrapping */
	MEASURED    = 1 << 5,	/* text box width has been measured */
	HAS_HEIGHT  = 1 << 6,	/* box has height (perhaps due to children) */
	MAKE_HEIGHT = 1 << 7,	/* box causes its own height */
	NEED_MIN    = 1 << 8,	/* minimum width is required for layout */
	REPLACE_DIM = 1 << 9,	/* replaced element has given dimensions */
	IFRAME      = 1 << 10,	/* box contains an iframe */
	CONVERT_CHILDREN = 1 << 11  /* wanted children converting */
} box_flags;

/* Sides of a box */
enum box_side { TOP, RIGHT, BOTTOM, LEFT };

/**
 * Container for box border details
 */
struct box_border {
	enum css_border_style_e style;	/**< border-style */
	css_color c;			/**< border-color value */
	int width;			/**< border-width (pixels) */
};

/** Node in box tree. All dimensions are in pixels. */
struct box {
	/** Type of box. */
	box_type type;

	/** Box flags */
	box_flags flags;

	/** Computed styles for elements and their pseudo elements.  NULL on
	 *  non-element boxes. */
	css_select_results *styles;

	/** Style for this box. 0 for INLINE_CONTAINER and FLOAT_*. Pointer into
	 *  a box's 'styles' select results, except for implied boxes, where it
	 *  is a pointer to an owned computed style. */
	css_computed_style *style;

	/** Coordinate of left padding edge relative to parent box, or relative
	 * to ancestor that contains this box in float_children for FLOAT_. */
	int x;
	/** Coordinate of top padding edge, relative as for x. */
	int y;

	int width;   /**< Width of content box (excluding padding etc.). */
	int height;  /**< Height of content box (excluding padding etc.). */

	/* These four variables determine the maximum extent of a box's
	 * descendants. They are relative to the x,y coordinates of the box.
	 *
	 * Their use depends on the overflow CSS property:
	 *
	 * Overflow:	Usage:
	 * visible	The content of the box is displayed within these
	 *		dimensions.
	 * hidden	These are ignored. Content is plotted within the box
	 *		dimensions.
	 * scroll	These are used to determine the extent of the
	 *		scrollable area.
	 * auto		As "scroll".
	 */
	int descendant_x0;  /**< left edge of descendants */
	int descendant_y0;  /**< top edge of descendants */
	int descendant_x1;  /**< right edge of descendants */
	int descendant_y1;  /**< bottom edge of descendants */

	int margin[4];   /**< Margin: TOP, RIGHT, BOTTOM, LEFT. */
	int padding[4];  /**< Padding: TOP, RIGHT, BOTTOM, LEFT. */
	struct box_border border[4];   /**< Border: TOP, RIGHT, BOTTOM, LEFT. */

	struct scrollbar *scroll_x;  /**< Horizontal scroll. */
	struct scrollbar *scroll_y;  /**< Vertical scroll. */

	/** Width of box taking all line breaks (including margins etc). Must
	 * be non-negative. */
	int min_width;
	/** Width that would be taken with no line breaks. Must be
	 * non-negative. */
	int max_width;

	/**< Byte offset within a textual representation of this content. */
	size_t byte_offset;

	char *text;     /**< Text, or 0 if none. Unterminated. */
	size_t length;  /**< Length of text. */

	/** Width of space after current text (depends on font and size). */
	int space;

	nsurl *href;   /**< Link, or 0. */
	const char *target;  /**< Link target, or 0. */
	const char *title;  /**< Title, or 0. */

	unsigned int columns;  /**< Number of columns for TABLE / TABLE_CELL. */
	unsigned int rows;     /**< Number of rows for TABLE only. */
	unsigned int start_column;  /**< Start column for TABLE_CELL only. */

	struct box *next;      /**< Next sibling box, or 0. */
	struct box *prev;      /**< Previous sibling box, or 0. */
	struct box *children;  /**< First child box, or 0. */
	struct box *last;      /**< Last child box, or 0. */
	struct box *parent;    /**< Parent box, or 0. */
	/** INLINE_END box corresponding to this INLINE box, or INLINE box
	 * corresponding to this INLINE_END box. */
	struct box *inline_end;

	/** First float child box, or 0. Float boxes are in the tree twice, in
	 * this list for the block box which defines the area for floats, and
	 * also in the standard tree given by children, next, prev, etc. */
	struct box *float_children;
	/** Next sibling float box. */
	struct box *next_float;
	/** If box is a float, points to box's containing block */
	struct box *float_container;
	/** Level below which subsequent floats must be cleared.
	 * This is used only for boxes with float_children */
	int clear_level;

	/** List marker box if this is a list-item, or 0. */
	struct box *list_marker;

	struct column *col;  /**< Array of table column data for TABLE only. */

	/** Form control data, or 0 if not a form control. */
	struct form_control* gadget;

	char *usemap; /** (Image)map to use with this object, or 0 if none */
	lwc_string *id; /**<  value of id attribute (or name for anchors) */

	/** Background image for this box, or 0 if none */
	struct hlcache_handle *background;

	/** Object in this box (usually an image), or 0 if none. */
	struct hlcache_handle* object;
	/** Parameters for the object, or 0. */
	struct object_params *object_params;

	/** Iframe's browser_window, or NULL if none */
	struct browser_window *iframe;

	struct dom_node *node; /**< DOM node that generated this box or NULL */
};

/** Table column data. */
struct column {
	/** Type of column. */
	enum { COLUMN_WIDTH_UNKNOWN, COLUMN_WIDTH_FIXED,
	       COLUMN_WIDTH_AUTO, COLUMN_WIDTH_PERCENT,
	       COLUMN_WIDTH_RELATIVE } type;
	/** Preferred width of column. Pixels for FIXED, percentage for PERCENT,
	 *  relative units for RELATIVE, unused for AUTO. */
	int width;
	/** Minimum width of content. */
	int min;
	/** Maximum width of content. */
	int max;
	/** Whether all of column's cells are css positioned. */
	bool positioned;
};

/** Parameters for object element and similar elements. */
struct object_params {
	nsurl *data;
	char *type;
	char *codetype;
	nsurl *codebase;
	nsurl *classid;
	struct object_param *params;
};

/** Linked list of object element parameters. */
struct object_param {
	char *name;
	char *value;
	char *type;
	char *valuetype;
	struct object_param *next;
};

/** Frame target names (constant pointers to save duplicating the strings many
 * times). We convert _blank to _top for user-friendliness. */
extern const char *TARGET_SELF;
extern const char *TARGET_PARENT;
extern const char *TARGET_TOP;
extern const char *TARGET_BLANK;



struct box * box_create(css_select_results *styles, css_computed_style *style,
		bool style_owned, nsurl *href, const char *target, 
		const char *title, lwc_string *id, void *context);
void box_add_child(struct box *parent, struct box *child);
void box_insert_sibling(struct box *box, struct box *new_box);
void box_unlink_and_free(struct box *box);
void box_free(struct box *box);
void box_free_box(struct box *box);
void box_bounds(struct box *box, struct rect *r);
void box_coords(struct box *box, int *x, int *y);
struct box *box_at_point(struct box *box, const int x, const int y,
		int *box_x, int *box_y);
struct box *box_pick_text_box(struct html_content *html,
		int x, int y, int dir, int *dx, int *dy);
struct box *box_find_by_id(struct box *box, lwc_string *id);
bool box_visible(struct box *box);
void box_dump(FILE *stream, struct box *box, unsigned int depth);
bool box_extract_link(const char *rel, nsurl *base, nsurl **result);

bool box_handle_scrollbars(struct content *c, struct box *box,
		bool bottom, bool right);
bool box_vscrollbar_present(const struct box *box);
bool box_hscrollbar_present(const struct box *box);

nserror dom_to_box(struct dom_node *n, struct html_content *c,
		box_construct_complete_cb cb);

bool box_normalise_block(struct box *block, struct html_content *c);

#endif
