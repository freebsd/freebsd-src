/*
 * Copyright 2006 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2005 Richard Wilson <info@tinct.net>
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
 * Browser history tree (implementation).
 */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "content/urldb.h"
#include "css/css.h"
#include "desktop/browser_history.h"
#include "desktop/browser_private.h"
#include "desktop/plotters.h"
#include "desktop/thumbnail.h"
#include "image/bitmap.h"
#include "render/font.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utils.h"


#define WIDTH 100
#define HEIGHT 86
#define RIGHT_MARGIN 50
#define BOTTOM_MARGIN 30

struct history_page {
	nsurl *url;    /**< Page URL, never 0. */
	lwc_string *frag_id; /** Fragment identifier, or 0. */
	char *title;  /**< Page title, never 0. */
};

/** A node in the history tree. */
struct history_entry {
  	struct history_page page;
	struct history_entry *back;  /**< Parent. */
	struct history_entry *next;  /**< Next sibling. */
	struct history_entry *forward;  /**< First child. */
	struct history_entry *forward_pref;  /**< Child in direction of
	                                          current entry. */
	struct history_entry *forward_last;  /**< Last child. */
	unsigned int children;  /**< Number of children. */
	int x;  /**< Position of node. */
	int y;  /**< Position of node. */
	struct bitmap *bitmap;  /**< Thumbnail bitmap, or 0. */
};

/** History tree for a window. */
struct history {
	/** First page in tree (page that window opened with). */
	struct history_entry *start;
	/** Current position in tree. */
	struct history_entry *current;
	/** Width of layout. */
	int width;
	/** Height of layout. */
	int height;
};


/**
 * Clone a history entry
 *
 * \param  history  opaque history structure, as returned by history_create()
 * \param  start    entry to clone
 *
 * \return  a cloned history entry, or 0 on error
 */

static struct history_entry *browser_window_history__clone_entry(
		struct history *history,
		struct history_entry *entry)
{
	struct history_entry *child;
	struct history_entry *new_child;
	struct history_entry *prev = NULL;
	struct history_entry *new_entry;

	assert(entry->page.url);
	assert(entry->page.title);

	/* clone the entry */
	new_entry = malloc(sizeof *entry);
	if (!new_entry)
		return NULL;

	memcpy(new_entry, entry, sizeof *entry);
	new_entry->page.url = nsurl_ref(entry->page.url);
	if (entry->page.frag_id)
		new_entry->page.frag_id = lwc_string_ref(entry->page.frag_id);

	new_entry->page.title = strdup(entry->page.title);
	if (!new_entry->page.url || !new_entry->page.title ||
			((entry->page.frag_id) && (!new_entry->page.frag_id))) {
		nsurl_unref(new_entry->page.url);
		if (new_entry->page.frag_id)
			lwc_string_unref(new_entry->page.frag_id);
		free(new_entry->page.title);
		free(new_entry);
		return NULL;
	}

	/* update references */
	if (history->current == entry)
		history->current = new_entry;

	/* recurse for all children */
	for (child = new_entry->forward; child; child = child->next) {
		new_child = browser_window_history__clone_entry(history, child);
		if (new_child) {
			new_child->back = new_entry;
		} else {
			nsurl_unref(new_entry->page.url);
			if (new_entry->page.frag_id)
				lwc_string_unref(new_entry->page.frag_id);
			free(new_entry->page.title);
			free(new_entry);
			return NULL;
		}
		if (prev)
			prev->next = new_child;
		if (new_entry->forward == child)
			new_entry->forward = new_child;
		if (new_entry->forward_pref == child)
			new_entry->forward_pref = new_child;
		if (new_entry->forward_last == child)
			new_entry->forward_last = new_child;
		prev = new_child;
	}
	return new_entry;
}


/**
 * Free an entry in the tree recursively.
 */

static void browser_window_history__free_entry(struct history_entry *entry)
{
	if (!entry)
		return;
	browser_window_history__free_entry(entry->forward);
	browser_window_history__free_entry(entry->next);
	nsurl_unref(entry->page.url);
	if (entry->page.frag_id)
		lwc_string_unref(entry->page.frag_id);
	free(entry->page.title);
	free(entry);
}


/**
 * Recursively position a subtree.
 *
 * \param  history  history being laid out
 * \param  entry    subtree to position
 * \param  x        x position for entry
 * \param  y        smallest available y
 * \param  shuffle  shuffle layout
 * \return  greatest y used by subtree
 */

static int browser_window_history__layout_subtree(struct history *history,
		struct history_entry *entry, int x, int y, bool shuffle)
{
	struct history_entry *child;
	int y1 = y;

	if (history->width < x + WIDTH)
		history->width = x + WIDTH;

	if (!entry->forward) {
		entry->x = x;
		entry->y = y;
		if (shuffle) {
			entry->x = rand() % 600;
			entry->y = rand() % 400;
		}
		return y + HEIGHT;
	}

	/* layout child subtrees below each other */
	for (child = entry->forward; child; child = child->next) {
		y1 = browser_window_history__layout_subtree(history, child,
				x + WIDTH + RIGHT_MARGIN, y1, shuffle);
		if (child->next)
			y1 += BOTTOM_MARGIN;
	}

	/* place ourselves in the middle */
	entry->x = x;
	entry->y = (y + y1) / 2 - HEIGHT / 2;
	if (shuffle) {
		entry->x = rand() % 600;
		entry->y = rand() % 400;
	}

	return y1;
}


/**
 * Compute node positions.
 *
 * \param  history  history to layout
 *
 * Each node's x and y are filled in.
 */

static void browser_window_history__layout(struct history *history)
{
	time_t t = time(0);
	struct tm *tp = localtime(&t);
	bool shuffle = tp->tm_mon == 3 && tp->tm_mday == 1;

	if (!history)
		return;

	history->width = 0;
	if (history->start)
		history->height = browser_window_history__layout_subtree(
				history, history->start,
				RIGHT_MARGIN / 2, BOTTOM_MARGIN / 2,
				shuffle);
	else
		history->height = 0;
	if (shuffle) {
		history->width = 600 + WIDTH;
		history->height = 400 + HEIGHT;
	}
	history->width += RIGHT_MARGIN / 2;
	history->height += BOTTOM_MARGIN / 2;
}

/**
 * Recursively redraw a history_entry.
 *
 * \param  history        history containing the entry
 * \param  history_entry  entry to render
 * \param ctx		  current redraw context
 */

static bool browser_window_history__redraw_entry(struct history *history,
		struct history_entry *entry,
		int x0, int y0, int x1, int y1,
		int x, int y, bool clip,
		const struct redraw_context *ctx)
{
	const struct plotter_table *plot = ctx->plot;
	size_t char_offset;
	int actual_x;
	struct history_entry *child;
	colour c = entry == history->current ?
			HISTORY_COLOUR_SELECTED : HISTORY_COLOUR_FOREGROUND;
	int tailsize = 5;
	int xoffset = x - x0;
	int yoffset = y - y0;
        plot_style_t pstyle_history_rect = { 
            .stroke_type = PLOT_OP_TYPE_SOLID,
            .stroke_colour = c,
            .stroke_width = entry == history->current ? 3 : 1,
        };
	plot_font_style_t fstyle = *plot_style_font;

	if (clip) {
		struct rect rect;
		rect.x0 = x0 + xoffset;
		rect.y0 = y0 + yoffset;
		rect.x1 = x1 + xoffset;
		rect.y1 = y1 + yoffset;
		if(!plot->clip(&rect))
			return false;
	}

	if (!plot->bitmap(entry->x + xoffset, entry->y + yoffset, WIDTH, HEIGHT,
			entry->bitmap, 0xffffff, 0))
		return false;
	if (!plot->rectangle(entry->x - 1 + xoffset, 
                            entry->y - 1 + yoffset,
                            entry->x + xoffset + WIDTH, 
                            entry->y + yoffset + HEIGHT,
                            &pstyle_history_rect))
		return false;

	if (!nsfont.font_position_in_string(plot_style_font, entry->page.title,
			strlen(entry->page.title), WIDTH,
			&char_offset, &actual_x))
		return false;

	fstyle.background = HISTORY_COLOUR_BACKGROUND;
	fstyle.foreground = c;
	fstyle.weight = entry == history->current ? 900 : 400;

	if (!plot->text(entry->x + xoffset, entry->y + HEIGHT + 12 + yoffset,
			entry->page.title, char_offset, &fstyle))
		return false;

	for (child = entry->forward; child; child = child->next) {
		if (!plot->line(entry->x + WIDTH + xoffset,
				entry->y + HEIGHT / 2 + yoffset,
		      	entry->x + WIDTH + tailsize + xoffset,
				entry->y + HEIGHT / 2 + yoffset, 
			       plot_style_stroke_history))
			return false;
		if (!plot->line(entry->x + WIDTH + tailsize + xoffset,
			       entry->y + HEIGHT / 2 + yoffset,
			       child->x - tailsize +xoffset,
			       child->y + HEIGHT / 2 + yoffset,
			       plot_style_stroke_history))
			return false;
		if (!plot->line(child->x - tailsize + xoffset,
			       child->y + HEIGHT / 2 + yoffset,
			       child->x + xoffset, child->y +
			       			HEIGHT / 2 + yoffset,
			       plot_style_stroke_history))
			return false;
		if (!browser_window_history__redraw_entry(history, child,
				x0, y0, x1, y1, x, y, clip, ctx))
			return false;
	}

	return true;
}


/**
 * Find the history entry at a position.
 *
 * \param  entry  entry to search from
 * \param  x      coordinate
 * \param  y      coordinate
 * \return  an entry if found, 0 if none
 */

static struct history_entry *browser_window_history__find_position(
		struct history_entry *entry, int x, int y)
{
	struct history_entry *child;
	struct history_entry *found;

	if (!entry)
		return 0;

	if (entry->x <= x && x <= entry->x + WIDTH &&
			entry->y <= y && y <= entry->y + HEIGHT)
		return entry;

	for (child = entry->forward; child; child = child->next) {
		found = browser_window_history__find_position(child, x, y);
		if (found)
			return found;
	}

	return 0;
}

/**
 * Enumerate subentries in history
 * See also history_enumerate()
 *
 * \param	bw		The browser window to enumerate history of
 * \param	entry		entry to start enumeration at
 * \param	cb			callback function
 * \param	ud			context pointer passed to cb
 * \return	true to continue enumeration, false to cancel
 */
static bool browser_window_history__enumerate_entry(
		const struct browser_window *bw,
		const struct history_entry *entry,
		browser_window_history_enumerate_cb cb,
		void *ud)
{
	const struct history_entry *child;

	if (!cb(bw, entry->x, entry->y,
			entry->x + WIDTH, entry->y + HEIGHT,
			entry, ud))
		return false;

	for (child = entry->forward; child; child = child->next) {
		if (!browser_window_history__enumerate_entry(bw, child,
				cb, ud))
			return false;
	}

	return true;
}


/* -------------------------------------------------------------------------- */


/**
 * Create a new history tree for a browser window window.
 *
 * \param  bw	browser window to create history for.
 *
 * \return  NSERROR_OK or appropriate error otherwise
 */

nserror browser_window_history_create(struct browser_window *bw)
{
	struct history *history;

	bw->history = NULL;

	history = calloc(1, sizeof *history);
	if (!history) {
		warn_user("NoMemory", 0);
		return NSERROR_NOMEM;
	}
	history->width = RIGHT_MARGIN / 2;
	history->height = BOTTOM_MARGIN / 2;

	bw->history = history;
	return NSERROR_OK;
}


/**
 * Clone a bw's history tree for new bw
 *
 * \param  existing	browser window with history to clone.
 * \param  clone	browser window to make cloned history for.
 *
 * \return  NSERROR_OK or appropriate error otherwise
 */

nserror browser_window_history_clone(const struct browser_window *existing,
		struct browser_window *clone)
{
	struct history *new_history;

	clone->history = NULL;

	if (existing == NULL || existing->history == NULL ||
			existing->history->start == NULL)
		/* Nothing to clone, create new history for clone window */
		return browser_window_history_create(clone);

	/* Make cloned history */
	new_history = malloc(sizeof *new_history);
	if (!new_history)
		return NSERROR_NOMEM;

	clone->history = new_history;
	memcpy(new_history, existing->history, sizeof *new_history);

	new_history->start = browser_window_history__clone_entry(new_history,
			new_history->start);
	if (!new_history->start) {
		LOG(("Insufficient memory to clone history"));
		warn_user("NoMemory", 0);
		browser_window_history_destroy(clone);
		clone->history = NULL;
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}


/**
 * Insert a url into the history tree.
 *
 * \param  bw       browser window with history object
 * \param  content  content to add to history
 * \param  frag_id  fragment identifier, or NULL.
 *
 * The page is added after the current entry and becomes current.
 */


void browser_window_history_add(struct browser_window *bw,
		struct hlcache_handle *content, lwc_string *frag_id)
{
	struct history *history;
	struct history_entry *entry;
	nsurl *nsurl = hlcache_handle_get_url(content);
	char *title;
	struct bitmap *bitmap;

	assert(bw);
	assert(bw->history);
	assert(content);

	history = bw->history;

	/* allocate space */
	entry = malloc(sizeof *entry);
	if (entry == NULL)
		return;

	title = strdup(content_get_title(content));
	if (title == NULL) {
		warn_user("NoMemory", 0);
		free(entry);
		return;
	}

	entry->page.url = nsurl_ref(nsurl);
	entry->page.frag_id = frag_id ? lwc_string_ref(frag_id) : 0;

	entry->page.title = title;
	entry->back = history->current;
	entry->next = 0;
	entry->forward = entry->forward_pref = entry->forward_last = 0;
	entry->children = 0;
	entry->bitmap = 0;
	if (history->current) {
		if (history->current->forward_last)
			history->current->forward_last->next = entry;
		else
			history->current->forward = entry;
		history->current->forward_pref = entry;
		history->current->forward_last = entry;
		history->current->children++;
	} else {
		history->start = entry;
	}
	history->current = entry;

	/* if we have a thumbnail, don't update until the page has finished
	 * loading */
	bitmap = urldb_get_thumbnail(nsurl);
	if (!bitmap) {
		LOG(("Creating thumbnail for %s", nsurl_access(nsurl)));
		bitmap = bitmap_create(WIDTH, HEIGHT,
				BITMAP_NEW | BITMAP_CLEAR_MEMORY |
				BITMAP_OPAQUE);
		if (!bitmap) {
			warn_user("NoMemory", 0);
			return;
		}
		if (thumbnail_create(content, bitmap, nsurl) == false) {
			/* Thumbnailing failed. Ignore it silently */
			bitmap_destroy(bitmap);
			bitmap = NULL;
		}
	}
	entry->bitmap = bitmap;

	browser_window_history__layout(history);
}


/**
 * Update the thumbnail for the current entry.
 *
 * \param  history  opaque history structure, as returned by history_create()
 * \param  content  content for current entry
 */

void browser_window_history_update(struct browser_window *bw,
		struct hlcache_handle *content)
{
	struct history *history;
	char *title;

	assert(bw != NULL);

	history = bw->history;

	if (!history || !history->current || !history->current->bitmap)
		return;

	assert(history->current->page.url);
	assert(history->current->page.title);

	title = strdup(content_get_title(content));
	if (!title) {
		warn_user("NoMemory", 0);
		return;
	}

	assert(title);
	free(history->current->page.title);
	history->current->page.title = title;

	thumbnail_create(content, history->current->bitmap, NULL);
}


/**
 * Free a history structure.
 *
 * \param  history  opaque history structure, as returned by history_create()
 */

void browser_window_history_destroy(struct browser_window *bw)
{
	assert(bw != NULL);

	if (bw->history == NULL)
		return;

	browser_window_history__free_entry(bw->history->start);
	free(bw->history);

	bw->history = NULL;
}


/**
 * Go back in the history.
 *
 * \param  bw       browser window
 * \param  history  history of the window
 * \param  new_window  whether to open in new window
 */

void browser_window_history_back(struct browser_window *bw, bool new_window)
{
	if (!bw || !bw->history || !bw->history->current ||
			!bw->history->current->back)
		return;
	browser_window_history_go(bw, bw->history->current->back, new_window);
}


/**
 * Go forward in the history.
 *
 * \param  bw       browser window
 * \param  history  history of the window
 * \param  new_window  whether to open in new window
 */

void browser_window_history_forward(struct browser_window *bw, bool new_window)
{
	if (!bw || !bw->history || !bw->history->current ||
			!bw->history->current->forward_pref)
		return;
	browser_window_history_go(bw, bw->history->current->forward_pref,
			new_window);
}


/**
 * Check whether it is pssible to go back in the history.
 *
 * \param  history  history of the window
 * \return true if the history can go back, false otherwise
 */

bool browser_window_history_back_available(struct browser_window *bw)
{
	return (bw && bw->history && bw->history->current &&
			bw->history->current->back);
}


/**
 * Check whether it is pssible to go forwards in the history.
 *
 * \param  history  history of the window
 * \return true if the history can go forwards, false otherwise
 */

bool browser_window_history_forward_available(struct browser_window *bw)
{
	return (bw && bw->history && bw->history->current &&
			bw->history->current->forward_pref);
}


/* Documented in local_history.h */
void browser_window_history_go(struct browser_window *bw,
		struct history_entry *entry, bool new_window)
{
	struct history *history;
	nsurl *url;
	struct history_entry *current;
	nserror error;

	assert(bw != NULL);
	history = bw->history;

	if (entry->page.frag_id) {
		error = nsurl_refragment(entry->page.url,
				entry->page.frag_id, &url);

		if (error != NSERROR_OK) {
			warn_user("NoMemory", 0);
			return;
		}
	} else {
		url = nsurl_ref(entry->page.url);
	}

	if (new_window) {
		current = history->current;
		history->current = entry;

		error = browser_window_create(BW_CREATE_CLONE,
				url, NULL, bw, NULL);
		history->current = current;
		if (error != NSERROR_OK) {
			nsurl_unref(url);
			return;
		}
	} else {
		history->current = entry;
		browser_window_navigate(bw, url, NULL,
				BW_NAVIGATE_NONE, NULL, NULL, NULL);
	}

	nsurl_unref(url);
}


/**
 * Get the dimensions of a history.
 *
 * \param  history  history to measure
 * \param  width    updated to width
 * \param  height   updated to height
 */

void browser_window_history_size(struct browser_window *bw,
		int *width, int *height)
{
	assert(bw != NULL);
	assert(bw->history != NULL);

	*width = bw->history->width;
	*height = bw->history->height;
}


/**
 * Redraw a history.
 *
 * \param history	history to render
 * \param ctx		current redraw context
 */

bool browser_window_history_redraw(struct browser_window *bw,
		const struct redraw_context *ctx)
{
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	if (!history->start)
		return true;
	return browser_window_history__redraw_entry(history, history->start,
			0, 0, 0, 0, 0, 0, false, ctx);
}

/**
 * Redraw part of a history.
 *
 * \param  history	history to render
 * \param  x0		left X co-ordinate of redraw area
 * \param  y0		top Y co-ordinate of redraw area
 * \param  x1		right X co-ordinate of redraw area
 * \param  y1		lower Y co-ordinate of redraw area
 * \param  x		start X co-ordinate on plot canvas
 * \param  y		start Y co-ordinate on plot canvas
 * \param ctx		current redraw context
 */

bool browser_window_history_redraw_rectangle(struct browser_window *bw,
	int x0, int y0, int x1, int y1,
	int x, int y, const struct redraw_context *ctx)
{
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	if (!history->start)
		return true;
	return browser_window_history__redraw_entry(history, history->start,
		x0, y0, x1, y1, x, y, true, ctx);
}


/**
 * Handle a mouse click in a history.
 *
 * \param  bw          browser window containing history
 * \param  x           click coordinate
 * \param  y           click coordinate
 * \param  new_window  open a new window instead of using bw
 * \return  true if action was taken, false if click was not on an entry
 */

bool browser_window_history_click(struct browser_window *bw,
		int x, int y, bool new_window)
{
	struct history_entry *entry;
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	entry = browser_window_history__find_position(history->start, x, y);
	if (!entry)
		return false;
	if (entry == history->current)
		return false;

	browser_window_history_go(bw, entry, new_window);

	return true;
}


/**
 * Determine the URL of the entry at a position.
 *
 * \param  history  history to search
 * \param  x        coordinate
 * \param  y        coordinate
 * \return  URL, or 0 if no entry at (x, y)
 */

const char *browser_window_history_position_url(struct browser_window *bw,
		int x, int y)
{
	struct history_entry *entry;
	struct history *history;

	assert(bw != NULL);
	history = bw->history;

	entry = browser_window_history__find_position(history->start, x, y);
	if (!entry)
		return 0;

	return nsurl_access(entry->page.url);
}

/* Documented in local_history.h */
void browser_window_history_enumerate_forward(const struct browser_window *bw, 
		browser_window_history_enumerate_cb cb, void *user_data)
{
	struct history_entry *e;

	if (bw == NULL || bw->history == NULL || bw->history->current == NULL)
		return;

	e = bw->history->current->forward_pref;
	for (; e != NULL; e = e->forward_pref) {
		if (!cb(bw, e->x, e->y, e->x + WIDTH, e->y + HEIGHT,
				e, user_data))
			break;
	}
}

/* Documented in local_history.h */
void browser_window_history_enumerate_back(const struct browser_window *bw, 
		browser_window_history_enumerate_cb cb, void *user_data)
{
	struct history_entry *e;

	if (bw == NULL || bw->history == NULL || bw->history->current == NULL)
		return;

	for (e = bw->history->current->back; e != NULL; e = e->back) {
		if (!cb(bw, e->x, e->y, e->x + WIDTH, e->y + HEIGHT,
				e, user_data))
			break;
	}
}

/* Documented in local_history.h */
void browser_window_history_enumerate(const struct browser_window *bw,
		browser_window_history_enumerate_cb cb, void *user_data)
{
	if (bw == NULL || bw->history == NULL)
		return;
	browser_window_history__enumerate_entry(bw,
			bw->history->start, cb, user_data);
}

/* Documented in local_history.h */
const char *browser_window_history_entry_get_url(
		const struct history_entry *entry)
{
	return nsurl_access(entry->page.url);
}

/* Documented in local_history.h */
const char *browser_window_history_entry_get_fragment_id(
		const struct history_entry *entry)
{
	return (entry->page.frag_id) ? lwc_string_data(entry->page.frag_id) : 0;
}

/* Documented in local_history.h */
const char *browser_window_history_entry_get_title(
		const struct history_entry *entry)
{
	return entry->page.title;
}
