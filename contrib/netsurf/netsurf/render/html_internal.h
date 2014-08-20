/*
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
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
 * Content for text/html (private data).
 */

#ifndef NETSURF_RENDER_HTML_INTERNAL_H_
#define NETSURF_RENDER_HTML_INTERNAL_H_

#include "content/content_protected.h"
#include "desktop/selection.h"
#include "render/html.h"

typedef enum {
	HTML_DRAG_NONE,			/** No drag */
	HTML_DRAG_SELECTION,		/** Own; Text selection */
	HTML_DRAG_SCROLLBAR,		/** Not own; drag in scrollbar widget */
	HTML_DRAG_TEXTAREA_SELECTION,	/** Not own; drag in textarea widget */
	HTML_DRAG_TEXTAREA_SCROLLBAR,	/** Not own; drag in textarea widget */
	HTML_DRAG_CONTENT_SELECTION,	/** Not own; drag in child content */
	HTML_DRAG_CONTENT_SCROLL	/** Not own; drag in child content */
} html_drag_type;
union html_drag_owner {
	bool no_owner;
	struct box *content;
	struct scrollbar *scrollbar;
	struct box *textarea;
}; /**< For drags we don't own */

typedef enum {
	HTML_SELECTION_NONE,		/** No selection */
	HTML_SELECTION_TEXTAREA,	/** Selection in one of our textareas */
	HTML_SELECTION_SELF,		/** Selection in this html content */
	HTML_SELECTION_CONTENT		/** Selection in child content */
} html_selection_type;
union html_selection_owner {
	bool none;
	struct box *textarea;
	struct box *content;
}; /**< For getting at selections in this content or things in this content */

typedef enum {
	HTML_FOCUS_SELF,		/** Focus is our own */
	HTML_FOCUS_CONTENT,		/** Focus belongs to child content */
	HTML_FOCUS_TEXTAREA		/** Focus belongs to textarea */
} html_focus_type;
union html_focus_owner {
	bool self;
	struct box *textarea;
	struct box *content;
}; /**< For directing input */

/** Data specific to CONTENT_HTML. */
typedef struct html_content {
	struct content base;

	dom_hubbub_parser *parser; /**< Parser object handle */
	bool parse_completed; /**< Whether the parse has been completed */

	/** Document tree */
	dom_document *document;
	/** Quirkyness of document */
	dom_document_quirks_mode quirks;

	/** Encoding of source, NULL if unknown. */
	char *encoding;
	/** Source of encoding information. */
	dom_hubbub_encoding_source encoding_source;

	/** Base URL (may be a copy of content->url). */
	nsurl *base_url;
	/** Base target */
	char *base_target;

	/** Content has been aborted in the LOADING state */
	bool aborted;

	/** Whether a meta refresh has been handled */
	bool refresh;

	/* Title element node */
	dom_node *title;

        /** A talloc context purely for the render box tree */
	int *bctx;
	/** Box tree, or NULL. */
	struct box *layout;
	/** Document background colour. */
	colour background_colour;
	/** Font callback table */
	const struct font_functions *font_func;

	/** Number of entries in scripts */
	unsigned int scripts_count;
	/** Scripts */
	struct html_script *scripts;
	/** javascript context */
	struct jscontext *jscontext;

	/** Number of entries in stylesheet_content. */
	unsigned int stylesheet_count;
	/** Stylesheets. Each may be NULL. */
	struct html_stylesheet *stylesheets;
	/**< Style selection context */
	css_select_ctx *select_ctx;
	/**< Universal selector */
	lwc_string *universal;

	/** Number of entries in object_list. */
	unsigned int num_objects;
	/** List of objects. */
	struct content_html_object *object_list;
	/** Forms, in reverse order to document. */
	struct form *forms;
	/** Hash table of imagemaps. */
	struct imagemap **imagemaps;

	/** Browser window containing this document, or NULL if not open. */
	struct browser_window *bw;

	/** Frameset information */
	struct content_html_frames *frameset;

	/** Inline frame information */
	struct content_html_iframe *iframe;

	/** Content of type CONTENT_HTML containing this, or NULL if not an 
	 * object within a page. */
	struct html_content *page;

	/** Current drag type */
	html_drag_type drag_type;
	/** Widget capturing all mouse events */
	union html_drag_owner drag_owner;

	/** Current selection state */
	html_selection_type selection_type;
	/** Current selection owner */
	union html_selection_owner selection_owner;

	/** Current input focus target type */
	html_focus_type focus_type;
	/** Current input focus target */
	union html_focus_owner focus_owner;

	/** HTML content's own text selection object */
	struct selection sel;

	/** Open core-handled form SELECT menu,
	 *  or NULL if none currently open. */
	struct form_control *visible_select_menu;

	/** Context for free text search, or NULL if none */
	struct search_context *search;
	/** Search string or NULL */
	char *search_string;

} html_content;



void html_set_status(html_content *c, const char *extra);

void html__redraw_a_box(html_content *html, struct box *box);

/**
 * Set our drag status, and inform whatever owns the content
 *
 * \param html		HTML content
 * \param drag_type	Type of drag
 * \param drag_owner	What owns the drag
 * \param rect		Pointer movement bounds
 */
void html_set_drag_type(html_content *html, html_drag_type drag_type,
		union html_drag_owner drag_owner, const struct rect *rect);

/**
 * Set our selection status, and inform whatever owns the content
 *
 * \param html			HTML content
 * \param selection_type	Type of selection
 * \param selection_owner	What owns the selection
 * \param read_only		True iff selection is read only
 */
void html_set_selection(html_content *html, html_selection_type selection_type,
		union html_selection_owner selection_owner, bool read_only);

/**
 * Set our input focus, and inform whatever owns the content
 *
 * \param html			HTML content
 * \param focus_type		Type of input focus
 * \param focus_owner		What owns the focus
 * \param hide_caret		True iff caret to be hidden
 * \param x			Carret x-coord rel to owner
 * \param y			Carret y-coord rel to owner
 * \param height		Carret height
 * \param clip			Carret clip rect
 */
void html_set_focus(html_content *html, html_focus_type focus_type,
		union html_focus_owner focus_owner, bool hide_caret,
		int x, int y, int height, const struct rect *clip);


struct browser_window *html_get_browser_window(struct content *c);

/**
 * Complete conversion of an HTML document
 *
 * \param htmlc  Content to convert
 */
void html_finish_conversion(html_content *htmlc);

/**
 * Test if an HTML content conversion can begin
 *
 * \param htmlc		html content to test
 * \return true iff the html content conversion can begin
 */
bool html_can_begin_conversion(html_content *htmlc);

/**
 * Begin conversion of an HTML document
 *
 * \param htmlc Content to convert
 */
bool html_begin_conversion(html_content *htmlc);

/* in render/html_redraw.c */
bool html_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx);

/* in render/html_interaction.c */
void html_mouse_track(struct content *c, struct browser_window *bw,
			browser_mouse_state mouse, int x, int y);
void html_mouse_action(struct content *c, struct browser_window *bw,
			browser_mouse_state mouse, int x, int y);
bool html_keypress(struct content *c, uint32_t key);
void html_overflow_scroll_callback(void *client_data,
		struct scrollbar_msg_data *scrollbar_data);
void html_search(struct content *c, void *context,
		search_flags_t flags, const char *string);
void html_search_clear(struct content *c);


/* in render/html_script.c */
dom_hubbub_error  html_process_script(void *ctx, dom_node *node);
void html_free_scripts(html_content *html);
bool html_scripts_exec(html_content *c);

/* in render/html_forms.c */
struct form *html_forms_get_forms(const char *docenc, dom_html_document *doc);
struct form_control *html_forms_get_control_for_node(struct form *forms,
		dom_node *node);

/* in render/html_css.c */
nserror html_css_init(void);
void html_css_fini(void);

/**
 * Initialise core stylesheets for a content
 *
 * \param c content structure to update
 * \return nserror
 */
nserror html_css_new_stylesheets(html_content *c);
nserror html_css_quirks_stylesheets(html_content *c);
nserror html_css_free_stylesheets(html_content *html);

bool html_css_process_link(html_content *htmlc, dom_node *node);
bool html_css_update_style(html_content *c, dom_node *style);

nserror html_css_new_selection_context(html_content *c,
		css_select_ctx **ret_select_ctx);

/* in render/html_css_fetcher.c */
void html_css_fetcher_register(void);
nserror html_css_fetcher_add_item(dom_string *data, nsurl *base_url,
		uint32_t *key);

/* in render/html_object.c */

/**
 * Start a fetch for an object required by a page.
 *
 * \param  c                 content of type CONTENT_HTML
 * \param  url               URL of object to fetch (copied)
 * \param  box               box that will contain the object
 * \param  permitted_types   bitmap of acceptable types
 * \param  available_width   estimate of width of object
 * \param  available_height  estimate of height of object
 * \param  background        this is a background image
 * \return  true on success, false on memory exhaustion
 */
bool html_fetch_object(html_content *c, nsurl *url, struct box *box,
		content_type permitted_types,
		int available_width, int available_height,
		bool background);

nserror html_object_free_objects(html_content *html);
nserror html_object_close_objects(html_content *html);
nserror html_object_open_objects(html_content *html, struct browser_window *bw);
nserror html_object_abort_objects(html_content *html);

/* Useful dom_string pointers */
struct dom_string;

extern struct dom_string *html_dom_string_map;
extern struct dom_string *html_dom_string_id;
extern struct dom_string *html_dom_string_name;
extern struct dom_string *html_dom_string_area;
extern struct dom_string *html_dom_string_a;
extern struct dom_string *html_dom_string_nohref;
extern struct dom_string *html_dom_string_href;
extern struct dom_string *html_dom_string_target;
extern struct dom_string *html_dom_string_shape;
extern struct dom_string *html_dom_string_default;
extern struct dom_string *html_dom_string_rect;
extern struct dom_string *html_dom_string_rectangle;
extern struct dom_string *html_dom_string_coords;
extern struct dom_string *html_dom_string_circle;
extern struct dom_string *html_dom_string_poly;
extern struct dom_string *html_dom_string_polygon;
extern struct dom_string *html_dom_string_text_javascript;
extern struct dom_string *html_dom_string_type;
extern struct dom_string *html_dom_string_src;

#endif


