/*
 * Copyright 2007 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2010 Michael Drake <tlsa@netsurf-browser.org>
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
 * Content for text/html (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "utils/config.h"
#include "utils/corestrings.h"
#include "utils/http.h"
#include "utils/libdom.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utf8.h"
#include "utils/utils.h"
#include "utils/nsoption.h"
#include "content/content_protected.h"
#include "content/fetch.h"
#include "content/hlcache.h"
#include "desktop/selection.h"
#include "desktop/scrollbar.h"
#include "desktop/textarea.h"
#include "image/bitmap.h"
#include "javascript/js.h"
#include "desktop/gui_factory.h"

#include "render/box.h"
#include "render/font.h"
#include "render/form.h"
#include "render/html_internal.h"
#include "render/imagemap.h"
#include "render/layout.h"
#include "render/search.h"

#define CHUNK 4096

/* Change these to 1 to cause a dump to stderr of the frameset or box
 * when the trees have been built.
 */
#define ALWAYS_DUMP_FRAMESET 0
#define ALWAYS_DUMP_BOX 0

static const char *html_types[] = {
	"application/xhtml+xml",
	"text/html"
};


/**
 * Perform post-box-creation conversion of a document
 *
 * \param c        HTML content to complete conversion of
 * \param success  Whether box tree construction was successful
 */
static void html_box_convert_done(html_content *c, bool success)
{
	nserror err;
	dom_exception exc; /* returned by libdom functions */
	dom_node *html;

	LOG(("Done XML to box (%p)", c));

	/* Clean up and report error if unsuccessful or aborted */
	if ((success == false) || (c->aborted)) {
		html_object_free_objects(c);

		if (success == false) {
			content_broadcast_errorcode(&c->base, NSERROR_BOX_CONVERT);
		} else {
			content_broadcast_errorcode(&c->base, NSERROR_STOPPED);
		}

		content_set_error(&c->base);
		return;
	}


#if ALWAYS_DUMP_BOX
	box_dump(stderr, c->layout->children, 0);
#endif
#if ALWAYS_DUMP_FRAMESET
	if (c->frameset)
		html_dump_frameset(c->frameset, 0);
#endif

	exc = dom_document_get_document_element(c->document, (void *) &html);
	if ((exc != DOM_NO_ERR) || (html == NULL)) {
		/** @todo should this call html_object_free_objects(c);
		 * like the other error paths 
		 */
		LOG(("error retrieving html element from dom"));
		content_broadcast_errorcode(&c->base, NSERROR_DOM);
		content_set_error(&c->base);
		return;
	}

	/* extract image maps - can't do this sensibly in dom_to_box */
	err = imagemap_extract(c);
	if (err != NSERROR_OK) {
		LOG(("imagemap extraction failed"));
		html_object_free_objects(c);
		content_broadcast_errorcode(&c->base, err);
		content_set_error(&c->base);
		dom_node_unref(html);
		return;
	}
	/*imagemap_dump(c);*/

	/* Destroy the parser binding */
	dom_hubbub_parser_destroy(c->parser);
	c->parser = NULL;

	content_set_ready(&c->base);

	if (c->base.active == 0) {
		content_set_done(&c->base);
	}

	html_set_status(c, "");
	dom_node_unref(html);
}


/** process link node */
static bool html_process_link(html_content *c, dom_node *node)
{
	struct content_rfc5988_link link; /* the link added to the content */
	dom_exception exc; /* returned by libdom functions */
	dom_string *atr_string;
	nserror error;

	memset(&link, 0, sizeof(struct content_rfc5988_link));

	/* check that the relation exists - w3c spec says must be present */
	exc = dom_element_get_attribute(node, corestring_dom_rel, &atr_string);
	if ((exc != DOM_NO_ERR) || (atr_string == NULL)) {
		return false;
	}
	/* get a lwc string containing the link relation */
	exc = dom_string_intern(atr_string, &link.rel);
	dom_string_unref(atr_string);
	if (exc != DOM_NO_ERR) {
		return false;
	}

	/* check that the href exists - w3c spec says must be present */
	exc = dom_element_get_attribute(node, corestring_dom_href, &atr_string);
	if ((exc != DOM_NO_ERR) || (atr_string == NULL)) {
		lwc_string_unref(link.rel);
		return false;
	}

	/* get nsurl */
	error = nsurl_join(c->base_url, dom_string_data(atr_string),
			&link.href);
	dom_string_unref(atr_string);
	if (error != NSERROR_OK) {
		lwc_string_unref(link.rel);
		return false;
	}

	/* look for optional properties -- we don't care if internment fails */

	exc = dom_element_get_attribute(node,
			corestring_dom_hreflang, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		/* get a lwc string containing the href lang */
		exc = dom_string_intern(atr_string, &link.hreflang);
		dom_string_unref(atr_string);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_type, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		/* get a lwc string containing the type */
		exc = dom_string_intern(atr_string, &link.type);
		dom_string_unref(atr_string);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_media, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		/* get a lwc string containing the media */
		exc = dom_string_intern(atr_string, &link.media);
		dom_string_unref(atr_string);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_sizes, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		/* get a lwc string containing the sizes */
		exc = dom_string_intern(atr_string, &link.sizes);
		dom_string_unref(atr_string);
	}

	/* add to content */
	content__add_rfc5988_link(&c->base, &link);

	if (link.sizes != NULL)
		lwc_string_unref(link.sizes);
	if (link.media != NULL)
		lwc_string_unref(link.media);
	if (link.type != NULL)
		lwc_string_unref(link.type);
	if (link.hreflang != NULL)
		lwc_string_unref(link.hreflang);

	nsurl_unref(link.href);
	lwc_string_unref(link.rel);

	return true;
}

/** process title node */
static bool html_process_title(html_content *c, dom_node *node)
{
	dom_exception exc; /* returned by libdom functions */
	dom_string *title;
	char *title_str;
	bool success;

	exc = dom_node_get_text_content(node, &title);
	if ((exc != DOM_NO_ERR) || (title == NULL)) {
		return false;
	}

	title_str = squash_whitespace(dom_string_data(title));
	dom_string_unref(title);

	if (title_str == NULL) {
		return false;
	}

	success = content__set_title(&c->base, title_str);

	free(title_str);

	return success;
}

static bool html_process_base(html_content *c, dom_node *node)
{
	dom_exception exc; /* returned by libdom functions */
	dom_string *atr_string;

	/* get href attribute if present */
	exc = dom_element_get_attribute(node,
			corestring_dom_href, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		nsurl *url;
		nserror error;

		/* get url from string */
		error = nsurl_create(dom_string_data(atr_string), &url);
		dom_string_unref(atr_string);
		if (error == NSERROR_OK) {
			if (c->base_url != NULL)
				nsurl_unref(c->base_url);
			c->base_url = url;
		}
	}


	/* get target attribute if present and not already set */
	if (c->base_target != NULL) {
		return true;
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_target, &atr_string);
	if ((exc == DOM_NO_ERR) && (atr_string != NULL)) {
		/* Validation rules from the HTML5 spec for the base element:
		 *  The target must be one of _blank, _self, _parent, or
		 *  _top or any identifier which does not begin with an
		 *  underscore
		 */
		if (*dom_string_data(atr_string) != '_' ||
				dom_string_caseless_lwc_isequal(atr_string,
						corestring_lwc__blank) ||
				dom_string_caseless_lwc_isequal(atr_string,
						corestring_lwc__self) ||
				dom_string_caseless_lwc_isequal(atr_string,
						corestring_lwc__parent) ||
				dom_string_caseless_lwc_isequal(atr_string,
						corestring_lwc__top)) {
			c->base_target = strdup(dom_string_data(atr_string));
		}
		dom_string_unref(atr_string);
	}

	return true;
}

static nserror html_meta_refresh_process_element(html_content *c, dom_node *n)
{
	union content_msg_data msg_data;
	const char *url, *end, *refresh = NULL;
	char *new_url;
	char quote = '\0';
	dom_string *equiv, *content;
	dom_exception exc;
	nsurl *nsurl;
	nserror error = NSERROR_OK;

	exc = dom_element_get_attribute(n, corestring_dom_http_equiv, &equiv);
	if (exc != DOM_NO_ERR) {
		return NSERROR_DOM;
	}

	if (equiv == NULL) {
		return NSERROR_OK;
	}

	if (!dom_string_caseless_lwc_isequal(equiv, corestring_lwc_refresh)) {
		dom_string_unref(equiv);
		return NSERROR_OK;
	}

	dom_string_unref(equiv);

	exc = dom_element_get_attribute(n, corestring_dom_content, &content);
	if (exc != DOM_NO_ERR) {
		return NSERROR_DOM;
	}

	if (content == NULL) {
		return NSERROR_OK;
	}

	end = dom_string_data(content) + dom_string_byte_length(content);

	/* content  := *LWS intpart fracpart? *LWS [';' *LWS *1url *LWS]
	 * intpart  := 1*DIGIT
	 * fracpart := 1*('.' | DIGIT)
	 * url      := "url" *LWS '=' *LWS (url-nq | url-sq | url-dq)
	 * url-nq   := *urlchar
	 * url-sq   := "'" *(urlchar | '"') "'"
	 * url-dq   := '"' *(urlchar | "'") '"'
	 * urlchar  := [#x9#x21#x23-#x26#x28-#x7E] | nonascii
	 * nonascii := [#x80-#xD7FF#xE000-#xFFFD#x10000-#x10FFFF]
	 */

	url = dom_string_data(content);

	/* *LWS */
	while (url < end && isspace(*url)) {
		url++;
	}

	/* intpart */
	if (url == end || (*url < '0' || '9' < *url)) {
		/* Empty content, or invalid timeval */
		dom_string_unref(content);
		return NSERROR_OK;
	}

	msg_data.delay = (int) strtol(url, &new_url, 10);
	/* a very small delay and self-referencing URL can cause a loop
	 * that grinds machines to a halt. To prevent this we set a
	 * minimum refresh delay of 1s. */
	if (msg_data.delay < 1) {
		msg_data.delay = 1;
	}

	url = new_url;

	/* fracpart? (ignored, as delay is integer only) */
	while (url < end && (('0' <= *url && *url <= '9') ||
			*url == '.')) {
		url++;
	}

	/* *LWS */
	while (url < end && isspace(*url)) {
		url++;
	}

	/* ';' */
	if (url < end && *url == ';')
		url++;

	/* *LWS */
	while (url < end && isspace(*url)) {
		url++;
	}

	if (url == end) {
		/* Just delay specified, so refresh current page */
		dom_string_unref(content);

		c->base.refresh = nsurl_ref(
				content_get_url(&c->base));

		content_broadcast(&c->base, CONTENT_MSG_REFRESH, msg_data);

		return NSERROR_OK;
	}

	/* "url" */
	if (url <= end - 3) {
		if (strncasecmp(url, "url", 3) == 0) {
			url += 3;
		} else {
			/* Unexpected input, ignore this header */
			dom_string_unref(content);
			return NSERROR_OK;
		}
	} else {
		/* Insufficient input, ignore this header */
		dom_string_unref(content);
		return NSERROR_OK;
	}

	/* *LWS */
	while (url < end && isspace(*url)) {
		url++;
	}

	/* '=' */
	if (url < end) {
		if (*url == '=') {
			url++;
		} else {
			/* Unexpected input, ignore this header */
			dom_string_unref(content);
			return NSERROR_OK;
		}
	} else {
		/* Insufficient input, ignore this header */
		dom_string_unref(content);
		return NSERROR_OK;
	}

	/* *LWS */
	while (url < end && isspace(*url)) {
		url++;
	}

	/* '"' or "'" */
	if (url < end && (*url == '"' || *url == '\'')) {
		quote = *url;
		url++;
	}

	/* Start of URL */
	refresh = url;

	if (quote != 0) {
		/* url-sq | url-dq */
		while (url < end && *url != quote)
			url++;
	} else {
		/* url-nq */
		while (url < end && !isspace(*url))
			url++;
	}

	/* '"' or "'" or *LWS (we don't care) */
	if (url > refresh) {
		/* There's a URL */
		new_url = strndup(refresh, url - refresh);
		if (new_url == NULL) {
			dom_string_unref(content);
			return NSERROR_NOMEM;
		}

		error = nsurl_join(c->base_url, new_url, &nsurl);
		if (error == NSERROR_OK) {
			/* broadcast valid refresh url */

			c->base.refresh = nsurl;

			content_broadcast(&c->base, CONTENT_MSG_REFRESH, msg_data);
			c->refresh = true;
		}

		free(new_url);

	}

	dom_string_unref(content);

	return error;
}

static bool html_process_img(html_content *c, dom_node *node)
{
	dom_string *src;
	nsurl *url;
	nserror err;
	dom_exception exc;
	bool success;

	/* Do nothing if foreground images are disabled */
	if (nsoption_bool(foreground_images) == false) {
		return true;
	}

	exc = dom_element_get_attribute(node, corestring_dom_src, &src);
	if (exc != DOM_NO_ERR || src == NULL) {
		return true;
	}

	err = nsurl_join(c->base_url, dom_string_data(src), &url);
	if (err != NSERROR_OK) {
		dom_string_unref(src);
		return false;
	}
	dom_string_unref(src);

	/* Speculatively fetch the image */
	success = html_fetch_object(c, url, NULL, CONTENT_IMAGE, 0, 0, false);
	nsurl_unref(url);

	return success;
}

/**
 * Complete conversion of an HTML document
 *
 * \param c  Content to convert
 */
void html_finish_conversion(html_content *c)
{
	union content_msg_data msg_data;
	dom_exception exc; /* returned by libdom functions */
	dom_node *html;
	nserror error;

	/* Bail out if we've been aborted */
	if (c->aborted) {
		content_broadcast_errorcode(&c->base, NSERROR_STOPPED);
		content_set_error(&c->base);
		return;
	}

	/* create new css selection context */
	error = html_css_new_selection_context(c, &c->select_ctx);
	if (error != NSERROR_OK) {
		content_broadcast_errorcode(&c->base, error);
		content_set_error(&c->base);
		return;
	}


	/* fire a simple event named load at the Document's Window
	 * object, but with its target set to the Document object (and
	 * the currentTarget set to the Window object)
	 */
	js_fire_event(c->jscontext, "load", c->document, NULL);

	/* convert dom tree to box tree */
	LOG(("DOM to box (%p)", c));
	content_set_status(&c->base, messages_get("Processing"));
	msg_data.explicit_status_text = NULL;
	content_broadcast(&c->base, CONTENT_MSG_STATUS, msg_data);

	exc = dom_document_get_document_element(c->document, (void *) &html);
	if ((exc != DOM_NO_ERR) || (html == NULL)) {
		LOG(("error retrieving html element from dom"));
		content_broadcast_errorcode(&c->base, NSERROR_DOM);
		content_set_error(&c->base);
		return;
	}

	error = dom_to_box(html, c, html_box_convert_done);
	if (error != NSERROR_OK) {
		LOG(("box conversion failed"));
		dom_node_unref(html);
		html_object_free_objects(c);
		content_broadcast_errorcode(&c->base, error);
		content_set_error(&c->base);
		return;
	}

	dom_node_unref(html);
}

/* callback for DOMNodeInserted end type */
static void
dom_default_action_DOMNodeInserted_cb(struct dom_event *evt, void *pw)
{
	dom_event_target *node;
	dom_node_type type;
	dom_string *name;
	dom_exception exc;
	html_content *htmlc = pw;

	exc = dom_event_get_target(evt, &node);
	if ((exc == DOM_NO_ERR) && (node != NULL)) {
		exc = dom_node_get_node_type(node, &type);
		if ((exc == DOM_NO_ERR) && (type == DOM_ELEMENT_NODE)) {
			/* an element node has been inserted */
			exc = dom_node_get_node_name(node, &name);
			if ((exc == DOM_NO_ERR) && (name != NULL)) {

				if (dom_string_caseless_isequal(name,
						corestring_dom_link)) {
					/* Handle stylesheet loading */
					html_css_process_link(htmlc,
							(dom_node *)node);
					/* Generic link handling */
					html_process_link(htmlc,
							(dom_node *)node);

				} else if (dom_string_caseless_lwc_isequal(name,
						corestring_lwc_meta) &&
						htmlc->refresh == false) {
					html_meta_refresh_process_element(htmlc,
							(dom_node *)node);
				} else if (dom_string_caseless_lwc_isequal(
						name, corestring_lwc_base)) {
					html_process_base(htmlc,
							(dom_node *)node);
				} else if (dom_string_caseless_lwc_isequal(
						name, corestring_lwc_title) &&
						htmlc->title == NULL) {
					htmlc->title = dom_node_ref(node);
				} else if (dom_string_caseless_lwc_isequal(
						name, corestring_lwc_img)) {
					html_process_img(htmlc,
							(dom_node *) node);
				}

				dom_string_unref(name);
			}
		}
		dom_node_unref(node);
	}
}

/* callback for DOMNodeInserted end type */
static void
dom_default_action_DOMSubtreeModified_cb(struct dom_event *evt, void *pw)
{
	dom_event_target *node;
	dom_node_type type;
	dom_string *name;
	dom_exception exc;
	html_content *htmlc = pw;

	exc = dom_event_get_target(evt, &node);
	if ((exc == DOM_NO_ERR) && (node != NULL)) {
		if (htmlc->title == (dom_node *)node) {
			/* Node is our title node */
			html_process_title(htmlc, (dom_node *)node);
			dom_node_unref(node);
			return;
		}

		exc = dom_node_get_node_type(node, &type);
		if ((exc == DOM_NO_ERR) && (type == DOM_ELEMENT_NODE)) {
			/* an element node has been modified */
			exc = dom_node_get_node_name(node, &name);
			if ((exc == DOM_NO_ERR) && (name != NULL)) {

				if (dom_string_caseless_isequal(name,
						corestring_dom_style)) {
					html_css_update_style(htmlc,
							(dom_node *)node);
				}

				dom_string_unref(name);
			}
		}
		dom_node_unref(node);
	}
}

/* callback function selector
 *
 * selects a callback function for libdom to call based on the type and phase.
 * dom_default_action_phase from events/document_event.h
 *
 * The principle events are:
 *   DOMSubtreeModified
 *   DOMAttrModified
 *   DOMNodeInserted
 *   DOMNodeInsertedIntoDocument
 *
 * @return callback function pointer or NULL for none
 */
static dom_default_action_callback
dom_event_fetcher(dom_string *type,
		  dom_default_action_phase phase,
		  void **pw)
{
	//LOG(("type:%s", dom_string_data(type)));

	if (phase == DOM_DEFAULT_ACTION_END) {
		if (dom_string_isequal(type, corestring_dom_DOMNodeInserted)) {
			return dom_default_action_DOMNodeInserted_cb;
		} else if (dom_string_isequal(type, corestring_dom_DOMSubtreeModified)) {
			return dom_default_action_DOMSubtreeModified_cb;
		}
	}
	return NULL;
}

static nserror
html_create_html_data(html_content *c, const http_parameter *params)
{
	lwc_string *charset;
	nserror nerror;
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;

	c->parser = NULL;
	c->parse_completed = false;
	c->document = NULL;
	c->quirks = DOM_DOCUMENT_QUIRKS_MODE_NONE;
	c->encoding = NULL;
	c->base_url = nsurl_ref(content_get_url(&c->base));
	c->base_target = NULL;
	c->aborted = false;
	c->refresh = false;
	c->title = NULL;
	c->bctx = NULL;
	c->layout = NULL;
	c->background_colour = NS_TRANSPARENT;
	c->stylesheet_count = 0;
	c->stylesheets = NULL;
	c->select_ctx = NULL;
	c->universal = NULL;
	c->num_objects = 0;
	c->object_list = NULL;
	c->forms = NULL;
	c->imagemaps = NULL;
	c->bw = NULL;
	c->frameset = NULL;
	c->iframe = NULL;
	c->page = NULL;
	c->font_func = &nsfont;
	c->drag_type = HTML_DRAG_NONE;
	c->drag_owner.no_owner = true;
	c->selection_type = HTML_SELECTION_NONE;
	c->selection_owner.none = true;
	c->focus_type = HTML_FOCUS_SELF;
	c->focus_owner.self = true;
	c->search = NULL;
	c->search_string = NULL;
	c->scripts_count = 0;
	c->scripts = NULL;
	c->jscontext = NULL;

	c->base.active = 1; /* The html content itself is active */

	if (lwc_intern_string("*", SLEN("*"), &c->universal) != lwc_error_ok) {
		return NSERROR_NOMEM;
	}

	selection_prepare(&c->sel, (struct content *)c, true);

	nerror = http_parameter_list_find_item(params, corestring_lwc_charset, &charset);
	if (nerror == NSERROR_OK) {
		c->encoding = strdup(lwc_string_data(charset));

		lwc_string_unref(charset);

		if (c->encoding == NULL) {
			lwc_string_unref(c->universal);
			c->universal = NULL;
			return NSERROR_NOMEM;

		}
		c->encoding_source = DOM_HUBBUB_ENCODING_SOURCE_HEADER;
	}

	/* Create the parser binding */
	parse_params.enc = c->encoding;
	parse_params.fix_enc = true;
	parse_params.enable_script = nsoption_bool(enable_javascript);
	parse_params.msg = NULL;
	parse_params.script = html_process_script;
	parse_params.ctx = c;
	parse_params.daf = dom_event_fetcher;

	error = dom_hubbub_parser_create(&parse_params,
					 &c->parser,
					 &c->document);
	if ((error != DOM_HUBBUB_OK) && (c->encoding != NULL)) {
		/* Ok, we don't support the declared encoding. Bailing out
		 * isn't exactly user-friendly, so fall back to autodetect */
		free(c->encoding);
		c->encoding = NULL;

		parse_params.enc = c->encoding;

		error = dom_hubbub_parser_create(&parse_params,
						 &c->parser,
						 &c->document);
	}
	if (error != DOM_HUBBUB_OK) {
		nsurl_unref(c->base_url);
		c->base_url = NULL;

		lwc_string_unref(c->universal);
		c->universal = NULL;

		return libdom_hubbub_error_to_nserror(error);
	}

	return NSERROR_OK;

}

/**
 * Create a CONTENT_HTML.
 *
 * The content_html_data structure is initialized and the HTML parser is
 * created.
 */

static nserror
html_create(const content_handler *handler,
	    lwc_string *imime_type,
	    const http_parameter *params,
	    llcache_handle *llcache,
	    const char *fallback_charset,
	    bool quirks,
	    struct content **c)
{
	html_content *html;
	nserror error;

	html = calloc(1, sizeof(html_content));
	if (html == NULL)
		return NSERROR_NOMEM;

	error = content__init(&html->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(html);
		return error;
	}

	error = html_create_html_data(html, params);
	if (error != NSERROR_OK) {
		content_broadcast_errorcode(&html->base, error);
		free(html);
		return error;
	}

	error = html_css_new_stylesheets(html);
	if (error != NSERROR_OK) {
		content_broadcast_errorcode(&html->base, error);
		free(html);
		return error;
	}

	*c = (struct content *) html;

	return NSERROR_OK;
}



static nserror
html_process_encoding_change(struct content *c, 
			     const char *data, 
			     unsigned int size)
{
	html_content *html = (html_content *) c;
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	const char *encoding;
	const char *source_data;
	unsigned long source_size;

	/* Retrieve new encoding */
	encoding = dom_hubbub_parser_get_encoding(html->parser, 
						  &html->encoding_source);
	if (encoding == NULL) {
		return NSERROR_NOMEM;
	}

	if (html->encoding != NULL) {
		free(html->encoding);
		html->encoding = NULL;
	}

	html->encoding = strdup(encoding);
	if (html->encoding == NULL) {
		return NSERROR_NOMEM;
	}

	/* Destroy binding */
	dom_hubbub_parser_destroy(html->parser);
	html->parser = NULL;

	if (html->document != NULL) {
		dom_node_unref(html->document);
	}

	parse_params.enc = html->encoding;
	parse_params.fix_enc = true;
	parse_params.enable_script = nsoption_bool(enable_javascript);
	parse_params.msg = NULL;
	parse_params.script = html_process_script;
	parse_params.ctx = html;
	parse_params.daf = dom_event_fetcher;

	/* Create new binding, using the new encoding */
	error = dom_hubbub_parser_create(&parse_params,
					 &html->parser,
					 &html->document);
	if (error != DOM_HUBBUB_OK) {
		/* Ok, we don't support the declared encoding. Bailing out
		 * isn't exactly user-friendly, so fall back to Windows-1252 */
		free(html->encoding);
		html->encoding = strdup("Windows-1252");
		if (html->encoding == NULL) {
			return NSERROR_NOMEM;
		}
		parse_params.enc = html->encoding;

		error = dom_hubbub_parser_create(&parse_params,
						 &html->parser,
						 &html->document);

		if (error != DOM_HUBBUB_OK) {
			return libdom_hubbub_error_to_nserror(error);
		}

	}

	source_data = content__get_source_data(c, &source_size);

	/* Reprocess all the data.  This is safe because
	 * the encoding is now specified at parser start which means
	 * it cannot be changed again. 
	 */
	error = dom_hubbub_parser_parse_chunk(html->parser, 
					      (const uint8_t *)source_data, 
					      source_size);

	return libdom_hubbub_error_to_nserror(error);
}


/**
 * Process data for CONTENT_HTML.
 */

static bool
html_process_data(struct content *c, const char *data, unsigned int size)
{
	html_content *html = (html_content *) c;
	dom_hubbub_error dom_ret;
	nserror err = NSERROR_OK; /* assume its all going to be ok */

	dom_ret = dom_hubbub_parser_parse_chunk(html->parser, 
					      (const uint8_t *) data, 
					      size);

	err = libdom_hubbub_error_to_nserror(dom_ret);

	/* deal with encoding change */
	if (err == NSERROR_ENCODING_CHANGE) {
		 err = html_process_encoding_change(c, data, size);
	}

	/* broadcast the error if necessary */
	if (err != NSERROR_OK) {
		content_broadcast_errorcode(c, err);
		return false;
	}

	return true;	
}


/**
 * Convert a CONTENT_HTML for display.
 *
 * The following steps are carried out in order:
 *
 *  - parsing to an XML tree is completed
 *  - stylesheets are fetched
 *  - the XML tree is converted to a box tree and object fetches are started
 *
 * On exit, the content status will be either CONTENT_STATUS_DONE if the
 * document is completely loaded or CONTENT_STATUS_READY if objects are still
 * being fetched.
 */

static bool html_convert(struct content *c)
{
	html_content *htmlc = (html_content *) c;
	dom_exception exc; /* returned by libdom functions */

	/* The quirk check and associated stylesheet fetch is "safe"
	 * once the root node has been inserted into the document
	 * which must have happened by this point in the parse.
	 *
	 * faliure to retrive the quirk mode or to start the
	 * stylesheet fetch is non fatal as this "only" affects the
	 * render and it would annoy the user to fail the entire
	 * render for want of a quirks stylesheet.
	 */
	exc = dom_document_get_quirks_mode(htmlc->document, &htmlc->quirks);
	if (exc == DOM_NO_ERR) {
		html_css_quirks_stylesheets(htmlc);
		LOG(("quirks set to %d", htmlc->quirks));
	}

	htmlc->base.active--; /* the html fetch is no longer active */
	LOG(("%d fetches active", htmlc->base.active));

	/* The parse cannot be completed here because it may be paused
	 * untill all the resources being fetched have completed.
	 */

	/* if there are no active fetches in progress no scripts are
	 * being fetched or they completed already.
	 */
	if (html_can_begin_conversion(htmlc)) {
		return html_begin_conversion(htmlc);
	}
	return true;
}

/* Exported interface documented in html_internal.h */
bool html_can_begin_conversion(html_content *htmlc)
{
	unsigned int i;

	if (htmlc->base.active != 0)
		return false;

	for (i = 0; i != htmlc->stylesheet_count; i++) {
		if (htmlc->stylesheets[i].modified)
			return false;
	}

	return true;
}

bool
html_begin_conversion(html_content *htmlc)
{
	dom_node *html;
	nserror ns_error;
	struct form *f;
	dom_exception exc; /* returned by libdom functions */
	dom_string *node_name = NULL;
	dom_hubbub_error error;

	/* The act of completing the parse can result in additional data 
	 * being flushed through the parser. This may result in new style or 
	 * script nodes, upon which the conversion depends. Thus, once we 
	 * have completed the parse, we must check again to see if we can 
	 * begin the conversion. If we can't, we must stop and wait for the 
	 * new styles/scripts to be processed. Once they have been processed,
	 * we will be called again to begin the conversion for real. Thus, 
	 * we must also ensure that we don't attempt to complete the parse 
	 * multiple times, so store a flag to indicate that parsing is
	 * complete to avoid repeating the completion pointlessly.
	 */
	if (htmlc->parse_completed == false) {
		LOG(("Completing parse"));
		/* complete parsing */
		error = dom_hubbub_parser_completed(htmlc->parser);
		if (error != DOM_HUBBUB_OK) {
			LOG(("Parsing failed"));
	
			content_broadcast_errorcode(&htmlc->base, 
						    libdom_hubbub_error_to_nserror(error));

			return false;
		}
		htmlc->parse_completed = true;
	}

	if (html_can_begin_conversion(htmlc) == false) {
		/* We can't proceed (see commentary above) */
		return true;
	}

	/* Give up processing if we've been aborted */
	if (htmlc->aborted) {
		content_broadcast_errorcode(&htmlc->base, NSERROR_STOPPED);
		return false;
	}

	/* complete script execution */
	html_scripts_exec(htmlc);

	/* fire a simple event that bubbles named DOMContentLoaded at
	 * the Document.
	 */

	/* get encoding */
	if (htmlc->encoding == NULL) {
		const char *encoding;

		encoding = dom_hubbub_parser_get_encoding(htmlc->parser,
					&htmlc->encoding_source);
		if (encoding == NULL) {
			content_broadcast_errorcode(&htmlc->base, 
						    NSERROR_NOMEM);
			return false;
		}

		htmlc->encoding = strdup(encoding);
		if (htmlc->encoding == NULL) {
			content_broadcast_errorcode(&htmlc->base, 
						    NSERROR_NOMEM);
			return false;
		}
	}

	/* locate root element and ensure it is html */
	exc = dom_document_get_document_element(htmlc->document, (void *) &html);
	if ((exc != DOM_NO_ERR) || (html == NULL)) {
		LOG(("error retrieving html element from dom"));
		content_broadcast_errorcode(&htmlc->base, NSERROR_DOM);
		return false;
	}

	exc = dom_node_get_node_name(html, &node_name);
	if ((exc != DOM_NO_ERR) ||
	    (node_name == NULL) ||
	    (!dom_string_caseless_lwc_isequal(node_name,
	    		corestring_lwc_html))) {
		LOG(("root element not html"));
		content_broadcast_errorcode(&htmlc->base, NSERROR_DOM);
		dom_node_unref(html);
		return false;
	}
	dom_string_unref(node_name);

	/* Retrieve forms from parser */
	htmlc->forms = html_forms_get_forms(htmlc->encoding,
			(dom_html_document *) htmlc->document);
	for (f = htmlc->forms; f != NULL; f = f->prev) {
		nsurl *action;

		/* Make all actions absolute */
		if (f->action == NULL || f->action[0] == '\0') {
			/* HTML5 4.10.22.3 step 9 */
			nsurl *doc_addr = content_get_url(&htmlc->base);
			ns_error = nsurl_join(htmlc->base_url,
					      nsurl_access(doc_addr), 
					      &action);
		} else {
			ns_error = nsurl_join(htmlc->base_url, 
					      f->action, 
					      &action);
		}

		if (ns_error != NSERROR_OK) {
			content_broadcast_errorcode(&htmlc->base, ns_error);

			dom_node_unref(html);
			return false;
		}

		free(f->action);
		f->action = strdup(nsurl_access(action));
		nsurl_unref(action);
		if (f->action == NULL) {
			content_broadcast_errorcode(&htmlc->base, 
						    NSERROR_NOMEM);

			dom_node_unref(html);
			return false;
		}

		/* Ensure each form has a document encoding */
		if (f->document_charset == NULL) {
			f->document_charset = strdup(htmlc->encoding);
			if (f->document_charset == NULL) {
				content_broadcast_errorcode(&htmlc->base, 
							    NSERROR_NOMEM);
				dom_node_unref(html);
				return false;
			}
		}
	}

	dom_node_unref(html);

	if (htmlc->base.active == 0) {
		html_finish_conversion(htmlc);
	}

	return true;
}


/**
 * Stop loading a CONTENT_HTML.
 */

static void html_stop(struct content *c)
{
	html_content *htmlc = (html_content *) c;

	switch (c->status) {
	case CONTENT_STATUS_LOADING:
		/* Still loading; simply flag that we've been aborted
		 * html_convert/html_finish_conversion will do the rest */
		htmlc->aborted = true;
		break;

	case CONTENT_STATUS_READY:
		html_object_abort_objects(htmlc);

		/* If there are no further active fetches and we're still
 		 * in the READY state, transition to the DONE state. */
		if (c->status == CONTENT_STATUS_READY && c->active == 0) {
			html_set_status(htmlc, "");
			content_set_done(c);
		}

		break;

	case CONTENT_STATUS_DONE:
		/* Nothing to do */
		break;

	default:
		LOG(("Unexpected status %d", c->status));
		assert(0);
	}
}


/**
 * Reformat a CONTENT_HTML to a new width.
 */

static void html_reformat(struct content *c, int width, int height)
{
	html_content *htmlc = (html_content *) c;
	struct box *layout;
	unsigned int time_before, time_taken;

	time_before = wallclock();

	layout_document(htmlc, width, height);
	layout = htmlc->layout;

	/* width and height are at least margin box of document */
	c->width = layout->x + layout->padding[LEFT] + layout->width +
			layout->padding[RIGHT] + layout->border[RIGHT].width +
			layout->margin[RIGHT];
	c->height = layout->y + layout->padding[TOP] + layout->height +
			layout->padding[BOTTOM] + layout->border[BOTTOM].width +
			layout->margin[BOTTOM];

	/* if boxes overflow right or bottom edge, expand to contain it */
	if (c->width < layout->x + layout->descendant_x1)
		c->width = layout->x + layout->descendant_x1;
	if (c->height < layout->y + layout->descendant_y1)
		c->height = layout->y + layout->descendant_y1;

	selection_reinit(&htmlc->sel, htmlc->layout);

	time_taken = wallclock() - time_before;
	c->reformat_time = wallclock() +
		((time_taken * 3 < nsoption_uint(min_reflow_period) ?
		  nsoption_uint(min_reflow_period) : time_taken * 3));
}


/**
 * Redraw a box.
 *
 * \param  h	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void html_redraw_a_box(hlcache_handle *h, struct box *box)
{
	int x, y;

	box_coords(box, &x, &y);

	content_request_redraw(h, x, y,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM]);
}


/**
 * Redraw a box.
 *
 * \param  h	content containing the box, of type CONTENT_HTML
 * \param  box  box to redraw
 */

void html__redraw_a_box(struct html_content *html, struct box *box)
{
	int x, y;

	box_coords(box, &x, &y);

	content__request_redraw((struct content *)html, x, y,
			box->padding[LEFT] + box->width + box->padding[RIGHT],
			box->padding[TOP] + box->height + box->padding[BOTTOM]);
}

static void html_destroy_frameset(struct content_html_frames *frameset)
{
	int i;

	if (frameset->name) {
		talloc_free(frameset->name);
		frameset->name = NULL;
	}
	if (frameset->url) {
		talloc_free(frameset->url);
		frameset->url = NULL;
	}
	if (frameset->children) {
		for (i = 0; i < (frameset->rows * frameset->cols); i++) {
			if (frameset->children[i].name) {
				talloc_free(frameset->children[i].name);
				frameset->children[i].name = NULL;
			}
			if (frameset->children[i].url) {
				nsurl_unref(frameset->children[i].url);
				frameset->children[i].url = NULL;
			}
		  	if (frameset->children[i].children)
		  		html_destroy_frameset(&frameset->children[i]);
		}
		talloc_free(frameset->children);
		frameset->children = NULL;
	}
}

static void html_destroy_iframe(struct content_html_iframe *iframe)
{
	struct content_html_iframe *next;
	next = iframe;
	while ((iframe = next) != NULL) {
		next = iframe->next;
		if (iframe->name)
			talloc_free(iframe->name);
		if (iframe->url) {
			nsurl_unref(iframe->url);
			iframe->url = NULL;
		}
		talloc_free(iframe);
	}
}


static void html_free_layout(html_content *htmlc)
{
	if (htmlc->bctx != NULL) {
		/* freeing talloc context should let the entire box
		 * set be destroyed 
		 */
		talloc_free(htmlc->bctx);
	}
}

/**
 * Destroy a CONTENT_HTML and free all resources it owns.
 */

static void html_destroy(struct content *c)
{
	html_content *html = (html_content *) c;
	struct form *f, *g;

	LOG(("content %p", c));

	/* Destroy forms */
	for (f = html->forms; f != NULL; f = g) {
		g = f->prev;

		form_free(f);
	}

	imagemap_destroy(html);

	if (c->refresh)
		nsurl_unref(c->refresh);

	if (html->base_url)
		nsurl_unref(html->base_url);

	if (html->parser != NULL) {
		dom_hubbub_parser_destroy(html->parser);
		html->parser = NULL;
	}

	if (html->document != NULL) {
		dom_node_unref(html->document);
		html->document = NULL;
	}

	if (html->title != NULL) {
		dom_node_unref(html->title);
		html->title = NULL;
	}

	/* Free encoding */
	if (html->encoding != NULL) {
		free(html->encoding);
		html->encoding = NULL;
	}

	/* Free base target */
	if (html->base_target != NULL) {
	 	free(html->base_target);
	 	html->base_target = NULL;
	}

	/* Free frameset */
	if (html->frameset != NULL) {
		html_destroy_frameset(html->frameset);
		talloc_free(html->frameset);
		html->frameset = NULL;
	}

	/* Free iframes */
	if (html->iframe != NULL) {
		html_destroy_iframe(html->iframe);
		html->iframe = NULL;
	}

	/* Destroy selection context */
	if (html->select_ctx != NULL) {
		css_select_ctx_destroy(html->select_ctx);
		html->select_ctx = NULL;
	}

	if (html->universal != NULL) {
		lwc_string_unref(html->universal);
		html->universal = NULL;
	}

	/* Free stylesheets */
	html_css_free_stylesheets(html);

	/* Free scripts */
	html_free_scripts(html);

	/* Free objects */
	html_object_free_objects(html);

	/* free layout */
	html_free_layout(html);
}


static nserror html_clone(const struct content *old, struct content **newc)
{
	/** \todo Clone HTML specifics */

	/* In the meantime, we should never be called, as HTML contents
	 * cannot be shared and we're not intending to fix printing's
	 * cloning of documents. */
	assert(0 && "html_clone should never be called");

	return true;
}

/**
 * Set the content status.
 */

void html_set_status(html_content *c, const char *extra)
{
	content_set_status(&c->base, extra);
}


/**
 * Handle a window containing a CONTENT_HTML being opened.
 */

static void
html_open(struct content *c,
	  struct browser_window *bw,
	  struct content *page,
	  struct object_params *params)
{
	html_content *html = (html_content *) c;

	html->bw = bw;
	html->page = (html_content *) page;

	html->drag_type = HTML_DRAG_NONE;
	html->drag_owner.no_owner = true;

	/* text selection */
	selection_init(&html->sel, html->layout);
	html->selection_type = HTML_SELECTION_NONE;
	html->selection_owner.none = true;

	html_object_open_objects(html, bw);
}


/**
 * Handle a window containing a CONTENT_HTML being closed.
 */

static void html_close(struct content *c)
{
	html_content *html = (html_content *) c;

	selection_clear(&html->sel, false);

	if (html->search != NULL)
		search_destroy_context(html->search);

	html->bw = NULL;

	html_object_close_objects(html);
}


/**
 * Return an HTML content's selection context
 */

static void html_clear_selection(struct content *c)
{
	html_content *html = (html_content *) c;

	switch (html->selection_type) {
	case HTML_SELECTION_NONE:
		/* Nothing to do */
		assert(html->selection_owner.none == true);
		break;
	case HTML_SELECTION_TEXTAREA:
		textarea_clear_selection(html->selection_owner.textarea->
				gadget->data.text.ta);
		break;
	case HTML_SELECTION_SELF:
		assert(html->selection_owner.none == false);
		selection_clear(&html->sel, true);
		break;
	case HTML_SELECTION_CONTENT:
		content_clear_selection(html->selection_owner.content->object);
		break;
	default:
		break;
	}

	/* There is no selection now. */
	html->selection_type = HTML_SELECTION_NONE;
	html->selection_owner.none = true;
}


/**
 * Return an HTML content's selection context
 */

static char *html_get_selection(struct content *c)
{
	html_content *html = (html_content *) c;

	switch (html->selection_type) {
	case HTML_SELECTION_TEXTAREA:
		return textarea_get_selection(html->selection_owner.textarea->
				gadget->data.text.ta);
	case HTML_SELECTION_SELF:
		assert(html->selection_owner.none == false);
		return selection_get_copy(&html->sel);
	case HTML_SELECTION_CONTENT:
		return content_get_selection(
				html->selection_owner.content->object);
	case HTML_SELECTION_NONE:
		/* Nothing to do */
		assert(html->selection_owner.none == true);
		break;
	default:
		break;
	}

	return NULL;
}


/**
 * Get access to any content, link URLs and objects (images) currently
 * at the given (x, y) coordinates.
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param data	pointer to contextual_content struct.  Its fields are updated
 *		with pointers to any relevent content, or set to NULL if none.
 */
static void
html_get_contextual_content(struct content *c,
			    int x,
			    int y,
			    struct contextual_content *data)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	int box_x = 0, box_y = 0;

	while ((next = box_at_point(box, x, y, &box_x, &box_y)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) ==
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->iframe)
			browser_window_get_contextual_content(box->iframe,
					x - box_x, y - box_y, data);

		if (box->object)
			content_get_contextual_content(box->object,
					x - box_x, y - box_y, data);

		if (box->object)
			data->object = box->object;

		if (box->href)
			data->link_url = nsurl_access(box->href);

		if (box->usemap) {
			const char *target = NULL;
			nsurl *url = imagemap_get(html, box->usemap, box_x,
					box_y, x, y, &target);
			/* Box might have imagemap, but no actual link area
			 * at point */
			if (url != NULL)
				data->link_url = nsurl_access(url);
		}
		if (box->gadget) {
			switch (box->gadget->type) {
			case GADGET_TEXTBOX:
			case GADGET_TEXTAREA:
			case GADGET_PASSWORD:
				data->form_features = CTX_FORM_TEXT;
				break;

			case GADGET_FILE:
				data->form_features = CTX_FORM_FILE;
				break;

			default:
				data->form_features = CTX_FORM_NONE;
				break;
			}
		}
	}
}


/**
 * Scroll deepest thing within the content which can be scrolled at given point
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param scrx	number of px try to scroll something in x direction
 * \param scry	number of px try to scroll something in y direction
 * \return true iff scroll was consumed by something in the content
 */
static bool
html_scroll_at_point(struct content *c, int x, int y, int scrx, int scry)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	int box_x = 0, box_y = 0;
	bool handled_scroll = false;

	/* TODO: invert order; visit deepest box first */

	while ((next = box_at_point(box, x, y, &box_x, &box_y)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) ==
				CSS_VISIBILITY_HIDDEN)
			continue;

		/* Pass into iframe */
		if (box->iframe && browser_window_scroll_at_point(box->iframe,
				x - box_x, y - box_y, scrx, scry) == true)
			return true;

		/* Pass into textarea widget */
		if (box->gadget && (box->gadget->type == GADGET_TEXTAREA ||
				box->gadget->type == GADGET_PASSWORD ||
				box->gadget->type == GADGET_TEXTBOX) &&
				textarea_scroll(box->gadget->data.text.ta,
						scrx, scry) == true)
			return true;

		/* Pass into object */
		if (box->object != NULL && content_scroll_at_point(
				box->object, x - box_x, y - box_y,
				scrx, scry) == true)
			return true;

		/* Handle box scrollbars */
		if (box->scroll_y && scrollbar_scroll(box->scroll_y, scry))
			handled_scroll = true;

		if (box->scroll_x && scrollbar_scroll(box->scroll_x, scrx))
			handled_scroll = true;

		if (handled_scroll == true)
			return true;
	}

	return false;
}

/** Helper for file gadgets to store their filename unencoded on the
 * dom node associated with the gadget.
 *
 * \todo Get rid of this crap eventually
 */
static void html__dom_user_data_handler(dom_node_operation operation,
		dom_string *key, void *_data, struct dom_node *src,
		struct dom_node *dst)
{
	char *oldfile;
	char *data = (char *)_data;

	if (!dom_string_isequal(corestring_dom___ns_key_file_name_node_data,
				key) || data == NULL) {
		return;
	}

	switch (operation) {
	case DOM_NODE_CLONED:
		if (dom_node_set_user_data(dst,
					   corestring_dom___ns_key_file_name_node_data,
					   strdup(data), html__dom_user_data_handler,
					   &oldfile) == DOM_NO_ERR) {
			if (oldfile != NULL)
				free(oldfile);
		}
		break;

	case DOM_NODE_RENAMED:
	case DOM_NODE_IMPORTED:
	case DOM_NODE_ADOPTED:
		break;

	case DOM_NODE_DELETED:
		free(data);
		break;
	default:
		LOG(("User data operation not handled."));
		assert(0);
	}
}

static void html__set_file_gadget_filename(struct content *c,
	struct form_control *gadget, const char *fn)
{
	nserror ret;
	char *utf8_fn, *oldfile = NULL;
	html_content *html = (html_content *)c;
	struct box *file_box = gadget->box;

	ret = guit->utf8->local_to_utf8(fn, 0, &utf8_fn);
	if (ret != NSERROR_OK) {
		assert(ret != NSERROR_BAD_ENCODING);
		LOG(("utf8 to local encoding conversion failed"));
		/* Load was for us - just no memory */
		return;		
	}
	
	form_gadget_update_value(gadget, utf8_fn);

	/* corestring_dom___ns_key_file_name_node_data */
	if (dom_node_set_user_data((dom_node *)file_box->gadget->node,
				   corestring_dom___ns_key_file_name_node_data,
				   strdup(fn), html__dom_user_data_handler,
				   &oldfile) == DOM_NO_ERR) {
		if (oldfile != NULL)
			free(oldfile);
	}

	/* Redraw box. */
	html__redraw_a_box(html, file_box);		
}

void html_set_file_gadget_filename(struct hlcache_handle *hl,
	struct form_control *gadget, const char *fn)
{
	return html__set_file_gadget_filename(hlcache_handle_get_content(hl),
		gadget, fn);
}

/**
 * Drop a file onto a content at a particular point, or determine if a file
 * may be dropped onto the content at given point.
 *
 * \param c	html content to look inside
 * \param x	x-coordinate of point of interest
 * \param y	y-coordinate of point of interest
 * \param file	path to file to be dropped, or NULL to know if drop allowed
 * \return true iff file drop has been handled, or if drop possible (NULL file)
 */
static bool html_drop_file_at_point(struct content *c, int x, int y, char *file)
{
	html_content *html = (html_content *) c;

	struct box *box = html->layout;
	struct box *next;
	struct box *file_box = NULL;
	struct box *text_box = NULL;
	int box_x = 0, box_y = 0;

	/* Scan box tree for boxes that can handle drop */
	while ((next = box_at_point(box, x, y, &box_x, &box_y)) != NULL) {
		box = next;

		if (box->style && css_computed_visibility(box->style) ==
				CSS_VISIBILITY_HIDDEN)
			continue;

		if (box->iframe)
			return browser_window_drop_file_at_point(box->iframe,
					x - box_x, y - box_y, file);

		if (box->object && content_drop_file_at_point(box->object,
					x - box_x, y - box_y, file) == true)
			return true;

		if (box->gadget) {
			switch (box->gadget->type) {
				case GADGET_FILE:
					file_box = box;
				break;

				case GADGET_TEXTBOX:
				case GADGET_TEXTAREA:
				case GADGET_PASSWORD:
					text_box = box;
					break;

				default:	/* appease compiler */
					break;
			}
		}
	}

	if (!file_box && !text_box)
		/* No box capable of handling drop */
		return false;

	if (file == NULL)
		/* There is a box capable of handling drop here */
		return true;

	/* Handle the drop */
	if (file_box) {
		/* File dropped on file input */
		html__set_file_gadget_filename(c, file_box->gadget, file);

	} else {
		/* File dropped on text input */

		size_t file_len;
		FILE *fp = NULL;
		char *buffer;
		char *utf8_buff;
		nserror ret;
		unsigned int size;
		int bx, by;

		/* Open file */
		fp = fopen(file, "rb");
		if (fp == NULL) {
			/* Couldn't open file, but drop was for us */
			return true;
		}

		/* Get filesize */
		fseek(fp, 0, SEEK_END);
		file_len = ftell(fp);
		fseek(fp, 0, SEEK_SET);

		/* Allocate buffer for file data */
		buffer = malloc(file_len + 1);
		if (buffer == NULL) {
			/* No memory, but drop was for us */
			fclose(fp);
			return true;
		}

		/* Stick file into buffer */
		if (file_len != fread(buffer, 1, file_len, fp)) {
			/* Failed, but drop was for us */
			free(buffer);
			fclose(fp);
			return true;
		}

		/* Done with file */
		fclose(fp);

		/* Ensure buffer's string termination */
		buffer[file_len] = '\0';

		/* TODO: Sniff for text? */

		/* Convert to UTF-8 */
		ret = guit->utf8->local_to_utf8(buffer, file_len, &utf8_buff);
		if (ret != NSERROR_OK) {
			/* bad encoding shouldn't happen */
			assert(ret != NSERROR_BAD_ENCODING);
			LOG(("local to utf8 encoding failed"));
			free(buffer);
			warn_user("NoMemory", NULL);
			return true;
		}

		/* Done with buffer */
		free(buffer);

		/* Get new length */
		size = strlen(utf8_buff);

		/* Simulate a click over the input box, to place caret */
		box_coords(text_box, &bx, &by);
		textarea_mouse_action(text_box->gadget->data.text.ta,
				BROWSER_MOUSE_PRESS_1, x - bx, y - by);

		/* Paste the file as text */
		textarea_drop_text(text_box->gadget->data.text.ta,
				utf8_buff, size);

		free(utf8_buff);
	}

	return true;
}


/**
 * Dump debug info concerning the html_content
 *
 * \param  bw    The browser window
 * \param  bw    The file to dump to
 */
static void html_debug_dump(struct content *c, FILE *f)
{
	html_content *html = (html_content *) c;

	assert(html != NULL);
	assert(html->layout != NULL);

	box_dump(f, html->layout, 0);
}


#if ALWAYS_DUMP_FRAMESET
/**
 * Print a frameset tree to stderr.
 */

static void
html_dump_frameset(struct content_html_frames *frame, unsigned int depth)
{
	unsigned int i;
	int row, col, index;
	const char *unit[] = {"px", "%", "*"};
	const char *scrolling[] = {"auto", "yes", "no"};

	assert(frame);

	fprintf(stderr, "%p ", frame);

	fprintf(stderr, "(%i %i) ", frame->rows, frame->cols);

	fprintf(stderr, "w%g%s ", frame->width.value, unit[frame->width.unit]);
	fprintf(stderr, "h%g%s ", frame->height.value,unit[frame->height.unit]);
	fprintf(stderr, "(margin w%i h%i) ",
			frame->margin_width, frame->margin_height);

	if (frame->name)
		fprintf(stderr, "'%s' ", frame->name);
	if (frame->url)
		fprintf(stderr, "<%s> ", frame->url);

	if (frame->no_resize)
		fprintf(stderr, "noresize ");
	fprintf(stderr, "(scrolling %s) ", scrolling[frame->scrolling]);
	if (frame->border)
		fprintf(stderr, "border %x ",
				(unsigned int) frame->border_colour);

	fprintf(stderr, "\n");

	if (frame->children) {
		for (row = 0; row != frame->rows; row++) {
			for (col = 0; col != frame->cols; col++) {
				for (i = 0; i != depth; i++)
					fprintf(stderr, "  ");
				fprintf(stderr, "(%i %i): ", row, col);
				index = (row * frame->cols) + col;
				html_dump_frameset(&frame->children[index],
						depth + 1);
			}
		}
	}
}

#endif

/**
 * Retrieve HTML document tree
 *
 * \param h  HTML content to retrieve document tree from
 * \return Pointer to document tree
 */
dom_document *html_get_document(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->document;
}

/**
 * Retrieve box tree
 *
 * \param h  HTML content to retrieve tree from
 * \return Pointer to box tree
 *
 * \todo This API must die, as must all use of the box tree outside render/
 */
struct box *html_get_box_tree(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->layout;
}

/**
 * Retrieve the charset of an HTML document
 *
 * \param h  Content to retrieve charset from
 * \return Pointer to charset, or NULL
 */
const char *html_get_encoding(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->encoding;
}

/**
 * Retrieve the charset of an HTML document
 *
 * \param h  Content to retrieve charset from
 * \return Pointer to charset, or NULL
 */
dom_hubbub_encoding_source html_get_encoding_source(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->encoding_source;
}

/**
 * Retrieve framesets used in an HTML document
 *
 * \param h  Content to inspect
 * \return Pointer to framesets, or NULL if none
 */
struct content_html_frames *html_get_frameset(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->frameset;
}

/**
 * Retrieve iframes used in an HTML document
 *
 * \param h  Content to inspect
 * \return Pointer to iframes, or NULL if none
 */
struct content_html_iframe *html_get_iframe(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->iframe;
}

/**
 * Retrieve an HTML content's base URL
 *
 * \param h  Content to retrieve base target from
 * \return Pointer to URL
 */
nsurl *html_get_base_url(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->base_url;
}

/**
 * Retrieve an HTML content's base target
 *
 * \param h  Content to retrieve base target from
 * \return Pointer to target, or NULL if none
 */
const char *html_get_base_target(hlcache_handle *h)
{
	html_content *c = (html_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->base_target;
}


/**
 * Retrieve layout coordinates of box with given id
 *
 * \param h        HTML document to search
 * \param frag_id  String containing an element id
 * \param x        Updated to global x coord iff id found
 * \param y        Updated to global y coord iff id found
 * \return  true iff id found
 */
bool html_get_id_offset(hlcache_handle *h, lwc_string *frag_id, int *x, int *y)
{
	struct box *pos;
	struct box *layout;

	if (content_get_type(h) != CONTENT_HTML)
		return false;

	layout = html_get_box_tree(h);

	if ((pos = box_find_by_id(layout, frag_id)) != 0) {
		box_coords(pos, x, y);
		return true;
	}
	return false;
}

/**
 * Compute the type of a content
 *
 * \return CONTENT_HTML
 */
static content_type html_content_type(void)
{
	return CONTENT_HTML;
}


static void html_fini(void)
{
	html_css_fini();
}

static const content_handler html_content_handler = {
	.fini = html_fini,
	.create = html_create,
	.process_data = html_process_data,
	.data_complete = html_convert,
	.reformat = html_reformat,
	.destroy = html_destroy,
	.stop = html_stop,
	.mouse_track = html_mouse_track,
	.mouse_action = html_mouse_action,
	.keypress = html_keypress,
	.redraw = html_redraw,
	.open = html_open,
	.close = html_close,
	.get_selection = html_get_selection,
	.clear_selection = html_clear_selection,
	.get_contextual_content = html_get_contextual_content,
	.scroll_at_point = html_scroll_at_point,
	.drop_file_at_point = html_drop_file_at_point,
	.search = html_search,
	.search_clear = html_search_clear,
	.debug_dump = html_debug_dump,
	.clone = html_clone,
	.type = html_content_type,
	.no_share = true,
};

nserror html_init(void)
{
	uint32_t i;
	nserror error;

	error = html_css_init();
	if (error != NSERROR_OK)
		goto error;

	for (i = 0; i < NOF_ELEMENTS(html_types); i++) {
		error = content_factory_register_handler(html_types[i],
				&html_content_handler);
		if (error != NSERROR_OK)
			goto error;
	}

	return NSERROR_OK;

error:
	html_fini();

	return error;
}

/**
 * Get the browser window containing an HTML content
 *
 * \param  c	HTML content
 * \return the browser window
 */
struct browser_window *html_get_browser_window(struct content *c)
{
	html_content *html = (html_content *) c;

	assert(c != NULL);
	assert(c->handler == &html_content_handler);

	return html->bw;
}
