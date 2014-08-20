/*
 * Copyright 2014 Vincent Sanders <vince@netsurf-browser.org>
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
 * Interface to platform-specific gui operation tables.
 */

#ifndef _NETSURF_DESKTOP_GUI_H_
#define _NETSURF_DESKTOP_GUI_H_

#include <stddef.h>

#include "utils/types.h"
#include "utils/errors.h"
#include "desktop/plot_style.h"
#include "desktop/mouse.h"

typedef enum {
	GUI_SAVE_SOURCE,
	GUI_SAVE_DRAW,
	GUI_SAVE_PDF,
	GUI_SAVE_TEXT,
	GUI_SAVE_COMPLETE,
	GUI_SAVE_OBJECT_ORIG,
	GUI_SAVE_OBJECT_NATIVE,
	GUI_SAVE_LINK_URI,
	GUI_SAVE_LINK_URL,
	GUI_SAVE_LINK_TEXT,
	GUI_SAVE_HOTLIST_EXPORT_HTML,
	GUI_SAVE_HISTORY_EXPORT_HTML,
	GUI_SAVE_TEXT_SELECTION,
	GUI_SAVE_CLIPBOARD_CONTENTS
} gui_save_type;

typedef enum {
	GDRAGGING_NONE,
	GDRAGGING_SCROLLBAR,
	GDRAGGING_SELECTION,
	GDRAGGING_OTHER
} gui_drag_type;

typedef enum {
	GW_CREATE_NONE		= 0,		/* New window */
	GW_CREATE_CLONE		= (1 << 0),	/* Clone existing window */
	GW_CREATE_TAB		= (1 << 1)	/* In same window as existing */
} gui_window_create_flags;

struct gui_window;
struct gui_download_window;
struct browser_window;
struct form_control;
struct ssl_cert_info;
struct hlcache_handle;
struct download_context;
struct nsurl;

typedef struct nsnsclipboard_styles {
	size_t start;			/**< Start of run */

	plot_font_style_t style;	/**< Style to give text run */
} nsclipboard_styles;

/**
 * Graphical user interface window function table.
 *
 * function table implementing window operations
 */
struct gui_window_table {

	/* Mandantory entries */

	/**
	 * Create and open a gui window for a browsing context.
	 *
	 * \param bw		bw to create gui_window for
	 * \param existing	an existing gui_window, may be NULL
	 * \param flags		flags for gui window creation
	 * \return gui window, or NULL on error
	 *
	 * If GW_CREATE_CLONE or GW_CREATE_TAB flags are set, existing is
	 * non-NULL.
	 *
	 * Front end's gui_window must include a reference to the
	 * browser window passed in the bw param.
	 */
	struct gui_window *(*create)(struct browser_window *bw,
			struct gui_window *existing,
			gui_window_create_flags flags);

	/** destroy previously created gui window */
	void (*destroy)(struct gui_window *g);

	/**
	 * Force a redraw of the entire contents of a window.
	 *
	 * @todo this API should be merged with update.
	 *
	 * \param g gui_window to redraw
	 */
	void (*redraw)(struct gui_window *g);

	/**
	 * Redraw an area of a window.
	 *
	 * \param g gui_window
	 * \param rect area to redraw
	 */
	void (*update)(struct gui_window *g, const struct rect *rect);

	/**
	 * Get the scroll position of a browser window.
	 *
	 * \param  g   gui_window
	 * \param  sx  receives x ordinate of point at top-left of window
	 * \param  sy  receives y ordinate of point at top-left of window
	 * \return true iff successful
	 */
	bool (*get_scroll)(struct gui_window *g, int *sx, int *sy);

	/**
	 * Set the scroll position of a browser window.
	 *
	 * \param  g   gui_window to scroll
	 * \param  sx  point to place at top-left of window
	 * \param  sy  point to place at top-left of window
	 */
	void (*set_scroll)(struct gui_window *g, int sx, int sy);

	/**
	 * Find the current dimensions of a browser window's content area.
	 *
	 * @todo The implementations of this are buggy and its only
	 * used from frames code.
	 *
	 * \param g	 gui_window to measure
	 * \param width	 receives width of window
	 * \param height receives height of window
	 * \param scaled whether to return scaled values
	 */
	void (*get_dimensions)(struct gui_window *g, int *width, int *height, bool scaled);

	/**
	 * Update the extent of the inside of a browser window to that of the
	 * current content.
	 *
	 * @todo this is used to update scroll bars does it need
	 * renaming? some frontends (windows) do not even implement it.
	 *
	 * \param  g gui_window to update the extent of
	 */
	void (*update_extent)(struct gui_window *g);



	/* Optional entries */

	/**
	 * Set the title of a window.
	 *
	 * \param  g	  window to update
	 * \param  title  new window title
	 */
	void (*set_title)(struct gui_window *g, const char *title);

	/**
	 * set the navigation url.
	 */
	void (*set_url)(struct gui_window *g, const char *url);

	/** set favicon */
	void (*set_icon)(struct gui_window *g, struct hlcache_handle *icon);

	/**
	 * Set the status bar of a browser window.
	 *
	 * \param  g	 gui_window to update
	 * \param  text  new status text
	 */
	void (*set_status)(struct gui_window *g, const char *text);

	/**
	 * Change mouse pointer shape
	 */
	void (*set_pointer)(struct gui_window *g, gui_pointer_shape shape);

	/**
	 * Place the caret in a browser window.
	 *
	 * \param  g	   window with caret
	 * \param  x	   coordinates of caret
	 * \param  y	   coordinates of caret
	 * \param  height  height of caret
	 * \param  clip	   clip rectangle, or NULL if none
	 */
	void (*place_caret)(struct gui_window *g, int x, int y, int height, const struct rect *clip);

	/**
	 * Remove the caret, if present.
	 *
	 * \param g window with caret
	 */
	void (*remove_caret)(struct gui_window *g);

	/** start the navigation throbber. */
	void (*start_throbber)(struct gui_window *g);

	/** stop the navigation throbber. */
	void (*stop_throbber)(struct gui_window *g);

	/** start a drag operation within a window */
	bool (*drag_start)(struct gui_window *g, gui_drag_type type, const struct rect *rect);

	/** save link operation */
	void (*save_link)(struct gui_window *g, const char *url, const char *title);

	/**
	 * Scrolls the specified area of a browser window into view.
	 *
	 * @todo investigate if this can be merged with set_scroll
	 * which is what the default implementation used by most
	 * toolkits uses.
	 *
	 * \param  g   gui_window to scroll
	 * \param  x0  left point to ensure visible
	 * \param  y0  bottom point to ensure visible
	 * \param  x1  right point to ensure visible
	 * \param  y1  top point to ensure visible
	 */
	void (*scroll_visible)(struct gui_window *g, int x0, int y0, int x1, int y1);

	/**
	 * Starts drag scrolling of a browser window
	 *
	 * \param g the window to scroll
	 */
	bool (*scroll_start)(struct gui_window *g);

	/**
	 * Called when the gui_window has new content.
	 *
	 * \param  g  the gui_window that has new content
	 */
	void (*new_content)(struct gui_window *g);

	/**
	 * Called when file chooser gadget is activated
	 */
	void (*file_gadget_open)(struct gui_window *g, struct hlcache_handle *hl, struct form_control *gadget);

	/** object dragged to window*/
	void (*drag_save_object)(struct gui_window *g, struct hlcache_handle *c, gui_save_type type);

	/** drag selection save */
	void (*drag_save_selection)(struct gui_window *g, const char *selection);

	/** selection started */
	void (*start_selection)(struct gui_window *g);
};


/**
 * function table for download windows.
 */
struct gui_download_table {
	struct gui_download_window *(*create)(struct download_context *ctx, struct gui_window *parent);

	nserror (*data)(struct gui_download_window *dw,	const char *data, unsigned int size);

	void (*error)(struct gui_download_window *dw, const char *error_msg);

	void (*done)(struct gui_download_window *dw);
};


/**
 * function table for clipboard operations.
 */
struct gui_clipboard_table {
	/**
	 * Core asks front end for clipboard contents.
	 *
	 * \param  buffer  UTF-8 text, allocated by front end, ownership yeilded to core
	 * \param  length  Byte length of UTF-8 text in buffer
	 */
	void (*get)(char **buffer, size_t *length);

	/**
	 * Core tells front end to put given text in clipboard
	 *
	 * \param  buffer    UTF-8 text, owned by core
	 * \param  length    Byte length of UTF-8 text in buffer
	 * \param  styles    Array of styles given to text runs, owned by core, or NULL
	 * \param  n_styles  Number of text run styles in array
	 */
	void (*set)(const char *buffer, size_t length, nsclipboard_styles styles[], int n_styles);
};


/**
 * function table for fetcher operations
 */
struct gui_fetch_table {
	/* Mandantory entries */

	/**
	 * Return the filename part of a full path
	 *
	 * @note used in curl fetcher
	 *
	 * \param path full path and filename
	 * \return filename (will be freed with free())
	 */
	char *(*filename_from_path)(char *path);

	/**
	 * Add a path component/filename to an existing path
	 *
	 * @note used in save complete and file fetcher
	 *
	 * \param path buffer containing path + free space
	 * \param length length of buffer "path"
	 * \param newpart string containing path component to add to path
	 * \return true on success
	 */
	bool (*path_add_part)(char *path, int length, const char *newpart);

	/**
	 * Determine the MIME type of a local file.
	 *
	 * @note used in file fetcher
	 *
	 * \param unix_path Unix style path to file on disk
	 * \return Pointer to MIME type string (should not be freed) -
	 *	   invalidated on next call to fetch_filetype.
	 */
	const char *(*filetype)(const char *unix_path);

	/**
	 * Convert a pathname to a file: URL.
	 *
	 * \param  path  pathname
	 * \return  URL, allocated on heap, or NULL on failure
	 */
	char *(*path_to_url)(const char *path);

	/**
	 * Convert a file: URL to a pathname.
	 *
	 * \param  url  a file: URL
	 * \return  pathname, allocated on heap, or NULL on failure
	 */
	char *(*url_to_path)(const char *url);


	/* Optional entries */

	/**
	 * Callback to translate resource to full url.
	 *
	 * @note used in resource fetcher
	 *
	 * Transforms a resource: path into a full URL. The returned URL
	 * is used as the target for a redirect. The caller takes ownership of
	 * the returned nsurl including unrefing it when finished with it.
	 *
	 * \param path The path of the resource to locate.
	 * \return A string containing the full URL of the target object or
	 *         NULL if no suitable resource can be found.
	 */
	struct nsurl* (*get_resource_url)(const char *path);

	/**
	 * Find a MIME type for a local file
	 *
	 * @note used in file fetcher
	 *
	 * \param ro_path RISC OS style path to file on disk
	 * \return MIME type string (on heap, caller should free), or NULL
	 */
	char *(*mimetype)(const char *ro_path);

};


/**
 * User interface utf8 characterset conversion routines.
 */
struct gui_utf8_table {
	/**
	 * Convert a UTF-8 encoded string into the system local encoding
	 *
	 * \param string The string to convert
	 * \param len The length (in bytes) of the string, or 0
	 * \param result Pointer to location in which to store result
	 * \return An nserror code
	 */
	nserror (*utf8_to_local)(const char *string, size_t len, char **result);

	/**
	 * Convert a string encoded in the system local encoding to UTF-8
	 *
	 * \param string The string to convert
	 * \param len The length (in bytes) of the string, or 0
	 * \param result Pointer to location in which to store result
	 * \return An nserror code
	 */
	nserror (*local_to_utf8)(const char *string, size_t len, char **result);
};

/**
 * function table for page text search.
 */
struct gui_search_table {
	/**
	 * Change the displayed search status.
	 *
	 * \param found  search pattern matched in text
	 * \param p gui private data pointer provided with search callbacks
	 */
	void (*status)(bool found, void *p);

	/**
	 * display hourglass while searching.
	 *
	 * \param active start/stop indicator
	 * \param p gui private data pointer provided with search callbacks
	 */
	void (*hourglass)(bool active, void *p);

	/**
	 * add search string to recent searches list
	 * front has full liberty how to implement the bare notification;
	 * core gives no guarantee of the integrity of the string
	 *
	 * \param string search pattern
	 * \param p gui private data pointer provided with search callbacks
	 */
	void (*add_recent)(const char *string, void *p);

	/**
	 * activate search forwards button in gui
	 *
	 * \param active activate/inactivate
	 * \param p gui private data pointer provided with search callbacks
	 */
	void (*forward_state)(bool active, void *p);

	/**
	 * activate search back button in gui
	 *
	 * \param active activate/inactivate
	 * \param p gui private data pointer provided with search callbacks
	 */
	void (*back_state)(bool active, void *p);
};

/**
 * Graphical user interface browser misc function table.
 *
 * function table implementing GUI interface to miscelaneous browser
 * functionality
 */
struct gui_browser_table {
	/* Mandantory entries */

	/**
	 * called to let the frontend update its state and run any
	 * I/O operations.
	 */
	void (*poll)(bool active);

	/**
	 * Schedule a callback.
	 *
	 * \param t interval before the callback should be made in ms or
	 *          negative value to remove any existing callback.
	 * \param callback callback function
	 * \param p user parameter passed to callback function
	 * \return NSERROR_OK on sucess or appropriate error on faliure
	 *
	 * The callback function will be called as soon as possible
	 * after the timeout has elapsed.
	 *
	 * Additional calls with the same callback and user parameter will
	 * reset the callback time to the newly specified value.
	 *
	 */
	nserror (*schedule)(int t, void (*callback)(void *p), void *p);

	/* Optional entries */

	/** called to allow the gui to cleanup */
	void (*quit)(void);

	/**
	 * set gui display of a retrieved favicon representing the
	 * search provider
	 *
	 * \param ico may be NULL for local calls; then access current
	 * cache from search_web_ico()
	 */
	void (*set_search_ico)(struct hlcache_handle *ico);

	/**
	 * core has no fetcher for url
	 */
	void (*launch_url)(const char *url);

	/**
	 * create a form select menu
	 */
	void (*create_form_select_menu)(struct browser_window *bw, struct form_control *control);

	/**
	 * verify certificate
	 */
	void (*cert_verify)(struct nsurl *url, const struct ssl_cert_info *certs, unsigned long num, nserror (*cb)(bool proceed, void *pw), void *cbpw);

	/**
	 * Prompt user for login
	 */
	void (*login)(struct nsurl *url, const char *realm,
			nserror (*cb)(bool proceed, void *pw), void *cbpw);

};


/** Graphical user interface function table
 *
 * function table implementing GUI interface to browser core
 */
struct gui_table {

	/**
	 * Browser table.
	 *
	 * Provides miscellaneous browser functionality. The table
	 * is mandantory and must be provided.
	 */
	struct gui_browser_table *browser;

	/** Window table */
	struct gui_window_table *window;

	/** Download table */
	struct gui_download_table *download;

	/** Clipboard table */
	struct gui_clipboard_table *clipboard;

	/** Fetcher table */
	struct gui_fetch_table *fetch;

	/**
	 * UTF8 table.
	 *
	 * Provides for conversion between the gui local character
	 * encoding and utf8. The table optional and may be NULL which
	 * implies the local encoding is utf8.
	 */
	struct gui_utf8_table *utf8;

	/**
	 *
	 * Page search table.
	 *
	 * Provides routines for the interactive text search on a page.
	 */
	struct gui_search_table *search;
};


#endif
