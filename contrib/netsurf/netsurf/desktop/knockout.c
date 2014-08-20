/*
 * Copyright 2006 Richard Wilson <info@tinct.net>
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
 * Knockout rendering (implementation).
 *
 * Knockout rendering is an optimisation which is particularly for
 * unaccelerated screen redraw. It tries to avoid plotting the same area more
 * than once.
 *
 * If the object is to plot two overlapping rectangles (one large, one small),
 * such as:
 *
 *   +-----------------+
 *   |#################|
 *   |####+-------+####|
 *   |####|:::::::|####|
 *   |####|:::::::|####|
 *   |####|:::::::|####|
 *   |####+-------+####|
 *   |#################|
 *   +-----------------+
 *
 * Without knockout rendering we plot the bottom rectangle and then the top one:
 *
 *   +-----------------+                 +-----------------+
 *   |#################|                 |#################|
 *   |#################|                 |####+-------+####|
 *   |#################|                 |####|:::::::|####|
 *   |#################|    and then,    |####|:::::::|####|
 *   |#################|                 |####|:::::::|####|
 *   |#################|                 |####+-------+####|
 *   |#################|                 |#################|
 *   +-----------------+                 +-----------------+
 *
 * With knockout rendering, the bottom rectangle is split up into smaller
 * ones and each pixel is just plotted once:
 *
 *   +-----------------+
 *   |#################|
 *   +----+-------+----+
 *   |####|:::::::|####|
 *   |####|:::::::|####|
 *   |####|:::::::|####|
 *   +----+-------+----+
 *   |#################|
 *   +-----------------+
 */

#include <assert.h>
#include <string.h>
#include "desktop/knockout.h"
#include "desktop/plotters.h"
#include "image/bitmap.h"
#include "utils/log.h"

/* Define to enable knockout debug */
#undef KNOCKOUT_DEBUG

#define KNOCKOUT_ENTRIES 3072	/* 40 bytes each */
#define KNOCKOUT_BOXES 768	/* 28 bytes each */
#define KNOCKOUT_POLYGONS 3072	/* 4 bytes each */

struct knockout_box;
struct knockout_entry;


static void knockout_calculate(int x0, int y0, int x1, int y1, struct knockout_box *box);
static bool knockout_plot_fill_recursive(struct knockout_box *box, plot_style_t *plot_style);
static bool knockout_plot_bitmap_recursive(struct knockout_box *box,
		struct knockout_entry *entry);

static bool knockout_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *pstyle);
static bool knockout_plot_polygon(const int *p, unsigned int n, const plot_style_t *pstyle);
static bool knockout_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *plot_style);
static bool knockout_plot_clip(const struct rect *clip);
static bool knockout_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle);
static bool knockout_plot_disc(int x, int y, int radius, const plot_style_t *pstyle);
static bool knockout_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *pstyle);
static bool knockout_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags);
static bool knockout_plot_flush(void);
static bool knockout_plot_group_start(const char *name);
static bool knockout_plot_group_end(void);
static bool knockout_plot_path(const float *p, unsigned int n, colour fill,
		float width, colour c, const float transform[6]);


const struct plotter_table knockout_plotters = {
	.rectangle = knockout_plot_rectangle,
	.line = knockout_plot_line,
	.polygon = knockout_plot_polygon,
	.clip = knockout_plot_clip,
	.text = knockout_plot_text,
	.disc = knockout_plot_disc,
	.arc = knockout_plot_arc,
	.bitmap = knockout_plot_bitmap,
	.group_start = knockout_plot_group_start,
	.group_end = knockout_plot_group_end,
	.flush = knockout_plot_flush,
	.path = knockout_plot_path,
	.option_knockout = true,
};


typedef enum {
	KNOCKOUT_PLOT_RECTANGLE,
	KNOCKOUT_PLOT_LINE,
	KNOCKOUT_PLOT_POLYGON,
	KNOCKOUT_PLOT_FILL,		/* knockout, knocked out */
	KNOCKOUT_PLOT_CLIP,
	KNOCKOUT_PLOT_TEXT,
	KNOCKOUT_PLOT_DISC,
	KNOCKOUT_PLOT_ARC,
	KNOCKOUT_PLOT_BITMAP,		/* knockout, knocked out */
	KNOCKOUT_PLOT_GROUP_START,
	KNOCKOUT_PLOT_GROUP_END,
} knockout_type;


struct knockout_box {
	struct rect bbox;
	bool deleted;			/* box has been deleted, ignore */
	struct knockout_box *child;
	struct knockout_box *next;
};


struct knockout_entry {
	knockout_type type;
	struct knockout_box *box;	/* relating series of knockout clips */
	union {
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
			plot_style_t plot_style;
		} rectangle;
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
			plot_style_t plot_style;
		} line;
		struct {
			int *p;
			unsigned int n;
			plot_style_t plot_style;
		} polygon;
		struct {
			int x0;
			int y0;
			int x1;
			int y1;
			plot_style_t plot_style;
		} fill;
		struct rect clip;
		struct {
			int x;
			int y;
			const char *text;
			size_t length;
			plot_font_style_t font_style;
		} text;
		struct {
			int x;
			int y;
			int radius;
			plot_style_t plot_style;
		} disc;
		struct {
			int x;
			int y;
			int radius;
			int angle1;
			int angle2;
			plot_style_t plot_style;
		} arc;
		struct {
			int x;
			int y;
			int width;
			int height;
			struct bitmap *bitmap;
			colour bg;
			bitmap_flags_t flags;
		} bitmap;
		struct {
			const char *name;
		} group_start;
	} data;
};


static struct knockout_entry knockout_entries[KNOCKOUT_ENTRIES];
static struct knockout_box knockout_boxes[KNOCKOUT_BOXES];
static int knockout_polygons[KNOCKOUT_POLYGONS];
static int knockout_entry_cur = 0;
static int knockout_box_cur = 0;
static int knockout_polygon_cur = 0;
static struct knockout_box *knockout_list = NULL;

static struct plotter_table real_plot;

static struct rect clip_cur;
static int nested_depth = 0;

/**
 * Start a knockout plotting session
 *
 * \param ctx		the redraw context with real plotter table
 * \param knk_ctx	updated to copy of ctx, with plotter table replaced
 * \return  true on success, false otherwise
 */
bool knockout_plot_start(const struct redraw_context *ctx,
		struct redraw_context *knk_ctx)
{
	/* check if we're recursing */
	if (nested_depth++ > 0) {
		/* we should already have the knockout renderer as default */
		assert(ctx->plot->rectangle == knockout_plotters.rectangle);
		*knk_ctx = *ctx;
		return true;
	}

	/* end any previous sessions */
	if (knockout_entry_cur > 0)
		knockout_plot_end();

	/* get copy of real plotter table */
	real_plot = *(ctx->plot);

	/* set up knockout rendering context */
	*knk_ctx = *ctx;
	knk_ctx->plot = &knockout_plotters;
	return true;
}


/**
 * End a knockout plotting session
 *
 * \return  true on success, false otherwise
 */
bool knockout_plot_end(void)
{
	/* only output when we've finished any nesting */
	if (--nested_depth == 0)
		return knockout_plot_flush();

	assert(nested_depth > 0);
	return true;
}


/**
 * Flush the current knockout session to empty the buffers
 *
 * \return  true on success, false otherwise
 */
bool knockout_plot_flush(void)
{
	int i;
	bool success = true;
	struct knockout_box *box;

	/* debugging information */
#ifdef KNOCKOUT_DEBUG
	LOG(("Entries are %i/%i, %i/%i, %i/%i",
			knockout_entry_cur, KNOCKOUT_ENTRIES,
			knockout_box_cur, KNOCKOUT_BOXES,
			knockout_polygon_cur, KNOCKOUT_POLYGONS));
#endif

	for (i = 0; i < knockout_entry_cur; i++) {
		switch (knockout_entries[i].type) {
		case KNOCKOUT_PLOT_RECTANGLE:
                        success &= real_plot.rectangle(
                                knockout_entries[i].data.rectangle.x0,
                                knockout_entries[i].data.rectangle.y0,
                                knockout_entries[i].data.rectangle.x1,
                                knockout_entries[i].data.rectangle.y1,
                                &knockout_entries[i].data.rectangle.plot_style);
			break;
		case KNOCKOUT_PLOT_LINE:
			success &= real_plot.line(
					knockout_entries[i].data.line.x0,
					knockout_entries[i].data.line.y0,
					knockout_entries[i].data.line.x1,
					knockout_entries[i].data.line.y1,
					&knockout_entries[i].data.line.plot_style);
			break;
		case KNOCKOUT_PLOT_POLYGON:
			success &= real_plot.polygon(
					knockout_entries[i].data.polygon.p,
					knockout_entries[i].data.polygon.n,
					&knockout_entries[i].data.polygon.plot_style);
			break;
		case KNOCKOUT_PLOT_FILL:
			box = knockout_entries[i].box->child;
			if (box)
				success &= knockout_plot_fill_recursive(box,
						&knockout_entries[i].data.fill.plot_style);
			else if (!knockout_entries[i].box->deleted)
				success &= real_plot.rectangle(
						knockout_entries[i].data.fill.x0,
						knockout_entries[i].data.fill.y0,
						knockout_entries[i].data.fill.x1,
						knockout_entries[i].data.fill.y1,
						&knockout_entries[i].data.fill.plot_style);
			break;
		case KNOCKOUT_PLOT_CLIP:
			success &= real_plot.clip(
					&knockout_entries[i].data.clip);
			break;
		case KNOCKOUT_PLOT_TEXT:
			success &= real_plot.text(
					knockout_entries[i].data.text.x,
					knockout_entries[i].data.text.y,
					knockout_entries[i].data.text.text,
					knockout_entries[i].data.text.length,
					&knockout_entries[i].data.text.font_style);
			break;
		case KNOCKOUT_PLOT_DISC:
			success &= real_plot.disc(
					knockout_entries[i].data.disc.x,
					knockout_entries[i].data.disc.y,
					knockout_entries[i].data.disc.radius,
					&knockout_entries[i].data.disc.plot_style);
			break;
		case KNOCKOUT_PLOT_ARC:
			success &= real_plot.arc(
					knockout_entries[i].data.arc.x,
					knockout_entries[i].data.arc.y,
					knockout_entries[i].data.arc.radius,
					knockout_entries[i].data.arc.angle1,
					knockout_entries[i].data.arc.angle2,
					&knockout_entries[i].data.arc.plot_style);
			break;
		case KNOCKOUT_PLOT_BITMAP:
			box = knockout_entries[i].box->child;
			if (box) {
				success &= knockout_plot_bitmap_recursive(box,
						&knockout_entries[i]);
			} else if (!knockout_entries[i].box->deleted) {
				success &= real_plot.bitmap(
						knockout_entries[i].data.
								bitmap.x,
						knockout_entries[i].data.
								bitmap.y,
						knockout_entries[i].data.
								bitmap.width,
						knockout_entries[i].data.
								bitmap.height,
						knockout_entries[i].data.
								bitmap.bitmap,
						knockout_entries[i].data.
								bitmap.bg,
						knockout_entries[i].data.
								bitmap.flags);
			}
			break;
		case KNOCKOUT_PLOT_GROUP_START:
			success &= real_plot.group_start(
					knockout_entries[i].data.group_start.name);
			break;
		case KNOCKOUT_PLOT_GROUP_END:
			success &= real_plot.group_end();
			break;
		}
	}

	knockout_entry_cur = 0;
	knockout_box_cur = 0;
	knockout_polygon_cur = 0;
	knockout_list = NULL;

	return success;
}


/**
 * Knockout a section of previous rendering
 *
 * \param  x0	the left edge of the removal box
 * \param  y0	the bottom edge of the removal box
 * \param  x1	the right edge of the removal box
 * \param  y1	the top edge of the removal box
 * \param  box  the parent box set to consider, or NULL for top level
*/
void knockout_calculate(int x0, int y0, int x1, int y1, struct knockout_box *owner)
{
	struct knockout_box *box;
	struct knockout_box *parent;
	struct knockout_box *prev = NULL;
	int nx0, ny0, nx1, ny1;

	if (owner == NULL)
		box = knockout_list;
	else
		box = owner->child;

	for (parent = box; parent; parent = parent->next) {
		/* permanently delink deleted nodes */
		if (parent->deleted) {
			if (prev) {
				/* not the first valid element: just skip future */
				prev->next = parent->next;
			} else {
				if (owner) {
					/* first valid element: update child reference */
					owner->child = parent->next;
					/* have we deleted all child nodes? */
					if (!owner->child)
						owner->deleted = true;
				} else {
					/* we are the head of the list */
					knockout_list = parent->next;
				}
			}
			continue;
		} else {
			prev = parent;
		}

		/* get the parent dimensions */
		nx0 = parent->bbox.x0;
		ny0 = parent->bbox.y0;
		nx1 = parent->bbox.x1;
		ny1 = parent->bbox.y1;

		/* reject non-overlapping boxes */
		if ((nx0 >= x1) || (nx1 <= x0) || (ny0 >= y1) || (ny1 <= y0))
			continue;

		/* check for a total knockout */
		if ((x0 <= nx0) && (x1 >= nx1) && (y0 <= ny0) && (y1 >= ny1)) {
			parent->deleted = true;
			continue;
		}

		/* has the box been replaced by children? */
		if (parent->child) {
			knockout_calculate(x0, y0, x1, y1, parent);
		} else {
			/* we need a maximum of 4 child boxes */
			if (knockout_box_cur + 4 >= KNOCKOUT_BOXES) {
				knockout_plot_flush();
				return;
			}

			/* clip top */
			if (y1 < ny1) {
				knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
				knockout_boxes[knockout_box_cur].bbox.y0 = y1;
				knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
				knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
				knockout_boxes[knockout_box_cur].deleted = false;
				knockout_boxes[knockout_box_cur].child = NULL;
				knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				ny1 = y1;
			}
			/* clip bottom */
			if (y0 > ny0) {
				knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
				knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
				knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
				knockout_boxes[knockout_box_cur].bbox.y1 = y0;
				knockout_boxes[knockout_box_cur].deleted = false;
				knockout_boxes[knockout_box_cur].child = NULL;
				knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				ny0 = y0;
			}
			/* clip right */
			if (x1 < nx1) {
				knockout_boxes[knockout_box_cur].bbox.x0 = x1;
				knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
				knockout_boxes[knockout_box_cur].bbox.x1 = nx1;
				knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
				knockout_boxes[knockout_box_cur].deleted = false;
				knockout_boxes[knockout_box_cur].child = NULL;
				knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				/* nx1 isn't used again, but if it was it would
				 * need to be updated to x1 here. */
			}
			/* clip left */
			if (x0 > nx0) {
				knockout_boxes[knockout_box_cur].bbox.x0 = nx0;
				knockout_boxes[knockout_box_cur].bbox.y0 = ny0;
				knockout_boxes[knockout_box_cur].bbox.x1 = x0;
				knockout_boxes[knockout_box_cur].bbox.y1 = ny1;
				knockout_boxes[knockout_box_cur].deleted = false;
				knockout_boxes[knockout_box_cur].child = NULL;
				knockout_boxes[knockout_box_cur].next = parent->child;
				parent->child = &knockout_boxes[knockout_box_cur++];
				/* nx0 isn't used again, but if it was it would
				 * need to be updated to x0 here. */
			}
		}
	}
}


bool knockout_plot_fill_recursive(struct knockout_box *box, plot_style_t *plot_style)
{
	bool success = true;
	struct knockout_box *parent;

	for (parent = box; parent; parent = parent->next) {
		if (parent->deleted)
			continue;
		if (parent->child)
			knockout_plot_fill_recursive(parent->child, plot_style);
		else
			success &= real_plot.rectangle(parent->bbox.x0,
                                                  parent->bbox.y0,
                                                  parent->bbox.x1,
                                                  parent->bbox.y1,
                                                  plot_style);
	}
	return success;
}


bool knockout_plot_bitmap_recursive(struct knockout_box *box,
		struct knockout_entry *entry)
{
	bool success = true;
	struct knockout_box *parent;

	for (parent = box; parent; parent = parent->next) {
		if (parent->deleted)
			continue;
		if (parent->child)
			knockout_plot_bitmap_recursive(parent->child, entry);
		else {
			success &= real_plot.clip(&parent->bbox);
			success &= real_plot.bitmap(entry->data.bitmap.x,
					entry->data.bitmap.y,
					entry->data.bitmap.width,
					entry->data.bitmap.height,
					entry->data.bitmap.bitmap,
					entry->data.bitmap.bg,
					entry->data.bitmap.flags);
		}
	}
	return success;
}

bool knockout_plot_rectangle(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	int kx0, ky0, kx1, ky1;

        if (pstyle->fill_type != PLOT_OP_TYPE_NONE) {
		/* filled draw */

		/* get our bounds */
		kx0 = (x0 > clip_cur.x0) ? x0 : clip_cur.x0;
		ky0 = (y0 > clip_cur.y0) ? y0 : clip_cur.y0;
		kx1 = (x1 < clip_cur.x1) ? x1 : clip_cur.x1;
		ky1 = (y1 < clip_cur.y1) ? y1 : clip_cur.y1;
		if ((kx0 > clip_cur.x1) || (kx1 < clip_cur.x0) ||
		    (ky0 > clip_cur.y1) || (ky1 < clip_cur.y0))
			return true;

		/* fills both knock out and get knocked out */
		knockout_calculate(kx0, ky0, kx1, ky1, NULL);
		knockout_boxes[knockout_box_cur].bbox.x0 = x0;
		knockout_boxes[knockout_box_cur].bbox.y0 = y0;
		knockout_boxes[knockout_box_cur].bbox.x1 = x1;
		knockout_boxes[knockout_box_cur].bbox.y1 = y1;
		knockout_boxes[knockout_box_cur].deleted = false;
		knockout_boxes[knockout_box_cur].child = NULL;
		knockout_boxes[knockout_box_cur].next = knockout_list;
		knockout_list = &knockout_boxes[knockout_box_cur];
		knockout_entries[knockout_entry_cur].box = &knockout_boxes[knockout_box_cur];
		knockout_entries[knockout_entry_cur].data.fill.x0 = x0;
		knockout_entries[knockout_entry_cur].data.fill.y0 = y0;
		knockout_entries[knockout_entry_cur].data.fill.x1 = x1;
		knockout_entries[knockout_entry_cur].data.fill.y1 = y1;
		knockout_entries[knockout_entry_cur].data.fill.plot_style = *pstyle;
		knockout_entries[knockout_entry_cur].data.fill.plot_style.stroke_type = PLOT_OP_TYPE_NONE; /* ensure we only plot the fill */
		knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_FILL;
		if ((++knockout_entry_cur >= KNOCKOUT_ENTRIES) ||
		    (++knockout_box_cur >= KNOCKOUT_BOXES))
			knockout_plot_flush();
        } 

	if (pstyle->stroke_type != PLOT_OP_TYPE_NONE) {
		/* draw outline */

		knockout_entries[knockout_entry_cur].data.rectangle.x0 = x0;
		knockout_entries[knockout_entry_cur].data.rectangle.y0 = y0;
		knockout_entries[knockout_entry_cur].data.rectangle.x1 = x1;
		knockout_entries[knockout_entry_cur].data.rectangle.y1 = y1;
		knockout_entries[knockout_entry_cur].data.fill.plot_style = *pstyle;
		knockout_entries[knockout_entry_cur].data.fill.plot_style.fill_type = PLOT_OP_TYPE_NONE; /* ensure we only plot the outline */
		knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_RECTANGLE;
		if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
			knockout_plot_flush();
        }
	return true;
}

bool knockout_plot_line(int x0, int y0, int x1, int y1, const plot_style_t *pstyle)
{
	knockout_entries[knockout_entry_cur].data.line.x0 = x0;
	knockout_entries[knockout_entry_cur].data.line.y0 = y0;
	knockout_entries[knockout_entry_cur].data.line.x1 = x1;
	knockout_entries[knockout_entry_cur].data.line.y1 = y1;
	knockout_entries[knockout_entry_cur].data.line.plot_style = *pstyle;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_LINE;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}


bool knockout_plot_polygon(const int *p, unsigned int n, const plot_style_t *pstyle)
{
	bool success = true;
	int *dest;

	/* ensure we have sufficient room even when flushed */
	if (n * 2 >= KNOCKOUT_POLYGONS) {
		knockout_plot_flush();
		success = real_plot.polygon(p, n, pstyle);
		return success;
	}

	/* ensure we have enough room right now */
	if (knockout_polygon_cur + n * 2 >= KNOCKOUT_POLYGONS)
		knockout_plot_flush();

	/* copy our data */
	dest = &(knockout_polygons[knockout_polygon_cur]);
	memcpy(dest, p, n * 2 * sizeof(int));
	knockout_polygon_cur += n * 2;
	knockout_entries[knockout_entry_cur].data.polygon.p = dest;
	knockout_entries[knockout_entry_cur].data.polygon.n = n;
	knockout_entries[knockout_entry_cur].data.polygon.plot_style = *pstyle;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_POLYGON;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}


bool knockout_plot_path(const float *p, unsigned int n, colour fill,
		float width, colour c, const float transform[6])
{
	knockout_plot_flush();
	return real_plot.path(p, n, fill, width, c, transform);
}


bool knockout_plot_clip(const struct rect *clip)
{
	if (clip->x1 < clip->x0 || clip->y0 > clip->y1) {
#ifdef KNOCKOUT_DEBUG
		LOG(("bad clip rectangle %i %i %i %i",
				clip->x0, clip->y0, clip->x1, clip->y1));
#endif
		return false;
	}

	/* memorise clip for bitmap tiling */
	clip_cur = *clip;

	knockout_entries[knockout_entry_cur].data.clip = *clip;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_CLIP;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}


bool knockout_plot_text(int x, int y, const char *text, size_t length, 
		const plot_font_style_t *fstyle)
{
	knockout_entries[knockout_entry_cur].data.text.x = x;
	knockout_entries[knockout_entry_cur].data.text.y = y;
	knockout_entries[knockout_entry_cur].data.text.text = text;
	knockout_entries[knockout_entry_cur].data.text.length = length;
	knockout_entries[knockout_entry_cur].data.text.font_style = *fstyle;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_TEXT;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}


bool knockout_plot_disc(int x, int y, int radius, const plot_style_t *pstyle)
{
	knockout_entries[knockout_entry_cur].data.disc.x = x;
	knockout_entries[knockout_entry_cur].data.disc.y = y;
	knockout_entries[knockout_entry_cur].data.disc.radius = radius;
	knockout_entries[knockout_entry_cur].data.disc.plot_style = *pstyle;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_DISC;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}

bool knockout_plot_arc(int x, int y, int radius, int angle1, int angle2, const plot_style_t *pstyle)
{
	knockout_entries[knockout_entry_cur].data.arc.x = x;
	knockout_entries[knockout_entry_cur].data.arc.y = y;
	knockout_entries[knockout_entry_cur].data.arc.radius = radius;
	knockout_entries[knockout_entry_cur].data.arc.angle1 = angle1;
	knockout_entries[knockout_entry_cur].data.arc.angle2 = angle2;
	knockout_entries[knockout_entry_cur].data.arc.plot_style = *pstyle;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_ARC;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}



bool knockout_plot_bitmap(int x, int y, int width, int height,
		struct bitmap *bitmap, colour bg,
		bitmap_flags_t flags)
{
	int kx0, ky0, kx1, ky1;

	/* get our bounds */
	kx0 = clip_cur.x0;
	ky0 = clip_cur.y0;
	kx1 = clip_cur.x1;
	ky1 = clip_cur.y1;
	if (!(flags & BITMAPF_REPEAT_X)) {
		if (x > kx0)
			kx0 = x;
		if (x + width < kx1)
			kx1 = x + width;
		if ((kx0 > clip_cur.x1) || (kx1 < clip_cur.x0))
			return true;
	}
	if (!(flags & BITMAPF_REPEAT_Y)) {
		if (y > ky0)
			ky0 = y;
		if (y + height < ky1)
			ky1 = y + height;
		if ((ky0 > clip_cur.y1) || (ky1 < clip_cur.y0))
			return true;
	}

	/* tiled bitmaps both knock out and get knocked out */
	if (bitmap_get_opaque(bitmap))
		knockout_calculate(kx0, ky0, kx1, ky1, NULL);
	knockout_boxes[knockout_box_cur].bbox.x0 = kx0;
	knockout_boxes[knockout_box_cur].bbox.y0 = ky0;
	knockout_boxes[knockout_box_cur].bbox.x1 = kx1;
	knockout_boxes[knockout_box_cur].bbox.y1 = ky1;
	knockout_boxes[knockout_box_cur].deleted = false;
	knockout_boxes[knockout_box_cur].child = NULL;
	knockout_boxes[knockout_box_cur].next = knockout_list;
	knockout_list = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].box = &knockout_boxes[knockout_box_cur];
	knockout_entries[knockout_entry_cur].data.bitmap.x = x;
	knockout_entries[knockout_entry_cur].data.bitmap.y = y;
	knockout_entries[knockout_entry_cur].data.bitmap.width = width;
	knockout_entries[knockout_entry_cur].data.bitmap.height = height;
	knockout_entries[knockout_entry_cur].data.bitmap.bitmap = bitmap;
	knockout_entries[knockout_entry_cur].data.bitmap.bg = bg;
	knockout_entries[knockout_entry_cur].data.bitmap.flags = flags;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_BITMAP;
	if ((++knockout_entry_cur >= KNOCKOUT_ENTRIES) ||
			(++knockout_box_cur >= KNOCKOUT_BOXES))
		knockout_plot_flush();
	return knockout_plot_clip(&clip_cur);
}

bool knockout_plot_group_start(const char *name)
{
	if (real_plot.group_start == NULL) {
		return true;
	}

	knockout_entries[knockout_entry_cur].data.group_start.name = name;
	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_GROUP_START;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}

bool knockout_plot_group_end(void)
{
	if (real_plot.group_end == NULL) {
		return true;
	}

	knockout_entries[knockout_entry_cur].type = KNOCKOUT_PLOT_GROUP_END;
	if (++knockout_entry_cur >= KNOCKOUT_ENTRIES)
		knockout_plot_flush();
	return true;
}
