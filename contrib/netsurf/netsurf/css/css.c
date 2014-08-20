/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
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

#include <assert.h>
#include <libwapcaplet/libwapcaplet.h>
#include <dom/dom.h>

#include "content/content_protected.h"
#include "content/fetch.h"
#include "content/hlcache.h"
#include "desktop/system_colour.h"
#include "utils/corestrings.h"
#include "utils/utils.h"
#include "utils/http.h"
#include "utils/log.h"
#include "utils/messages.h"

#include "css/css.h"
#include "css/internal.h"

/* Define to trace import fetches */
#undef NSCSS_IMPORT_TRACE

struct content_css_data;

/**
 * Type of callback called when a CSS object has finished
 *
 * \param css  CSS object that has completed
 * \param pw   Client-specific data
 */
typedef void (*nscss_done_callback)(struct content_css_data *css, void *pw);

/**
 * CSS content data
 */
struct content_css_data
{
	css_stylesheet *sheet;		/**< Stylesheet object */
	char *charset;			/**< Character set of stylesheet */
	struct nscss_import *imports;	/**< Array of imported sheets */
	uint32_t import_count;		/**< Number of sheets imported */
	uint32_t next_to_register;	/**< Index of next import to register */
	nscss_done_callback done;	/**< Completion callback */
	void *pw;			/**< Client data */
};

/**
 * CSS content data
 */
typedef struct nscss_content
{
	struct content base;		/**< Underlying content object */

	struct content_css_data data;	/**< CSS data */
} nscss_content;

/**
 * Context for import fetches
 */
typedef struct {
	struct content_css_data *css;		/**< Object containing import */
	uint32_t index;				/**< Index into parent sheet's 
						 *   imports array */
} nscss_import_ctx;

static nserror nscss_create(const content_handler *handler, 
		lwc_string *imime_type,	const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c);
static bool nscss_process_data(struct content *c, const char *data, 
		unsigned int size);
static bool nscss_convert(struct content *c);
static void nscss_destroy(struct content *c);
static nserror nscss_clone(const struct content *old, struct content **newc);
static bool nscss_matches_quirks(const struct content *c, bool quirks);
static content_type nscss_content_type(void);

static nserror nscss_create_css_data(struct content_css_data *c,
		const char *url, const char *charset, bool quirks,
		nscss_done_callback done, void *pw);
static css_error nscss_process_css_data(struct content_css_data *c, const char *data, 
		unsigned int size);
static css_error nscss_convert_css_data(struct content_css_data *c);
static void nscss_destroy_css_data(struct content_css_data *c);

static void nscss_content_done(struct content_css_data *css, void *pw);
static css_error nscss_handle_import(void *pw, css_stylesheet *parent, 
		lwc_string *url, uint64_t media);
static nserror nscss_import(hlcache_handle *handle,
		const hlcache_event *event, void *pw);
static css_error nscss_import_complete(nscss_import_ctx *ctx);

static css_error nscss_register_imports(struct content_css_data *c);
static css_error nscss_register_import(struct content_css_data *c,
		const hlcache_handle *import);


static css_stylesheet *blank_import;


/**
 * Initialise a CSS content
 *
 * \param c       Content to initialise
 * \param params  Content-Type parameters
 * \return true on success, false on failure
 */
nserror nscss_create(const content_handler *handler, 
		lwc_string *imime_type,	const http_parameter *params,
		llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	nscss_content *result;
	const char *charset = NULL;
	const char *xnsbase = NULL;
	lwc_string *charset_value = NULL;
	union content_msg_data msg_data;
	nserror error;

	result = calloc(1, sizeof(nscss_content));
	if (result == NULL)
		return NSERROR_NOMEM;

	error = content__init(&result->base, handler, imime_type,
			params, llcache, fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(result);
		return error;
	}

	/* Find charset specified on HTTP layer, if any */
	error = http_parameter_list_find_item(params, corestring_lwc_charset, 
			&charset_value);
	if (error != NSERROR_OK || lwc_string_length(charset_value) == 0) {
		/* No charset specified, use fallback, if any */
		/** \todo libcss will take this as gospel, which is wrong */
		charset = fallback_charset;
	} else {
		charset = lwc_string_data(charset_value);
	}

	/* Compute base URL for stylesheet */
	xnsbase = llcache_handle_get_header(llcache, "X-NS-Base");
	if (xnsbase == NULL) {
		xnsbase = nsurl_access(content_get_url(&result->base));
	}

	error = nscss_create_css_data(&result->data, 
			xnsbase, charset, result->base.quirks, 
			nscss_content_done, result);
	if (error != NSERROR_OK) {
		msg_data.error = messages_get("NoMemory");
		content_broadcast(&result->base, CONTENT_MSG_ERROR, msg_data);
		if (charset_value != NULL)
			lwc_string_unref(charset_value);
		free(result);
		return error;
	}

	if (charset_value != NULL)
		lwc_string_unref(charset_value);

	*c = (struct content *) result;

	return NSERROR_OK;
}

/**
 * Create a struct content_css_data, creating a stylesheet object
 *
 * \param c        Struct to populate
 * \param url      URL of stylesheet
 * \param charset  Stylesheet charset
 * \param quirks   Stylesheet quirks mode
 * \param done     Callback to call when content has completed
 * \param pw       Client data for \a done
 * \return NSERROR_OK on success, NSERROR_NOMEM on memory exhaustion
 */
static nserror nscss_create_css_data(struct content_css_data *c,
		const char *url, const char *charset, bool quirks,
		nscss_done_callback done, void *pw)
{
	css_error error;
	css_stylesheet_params params;

	c->pw = pw;
	c->done = done;
	c->next_to_register = (uint32_t) -1;
	c->import_count = 0;
	c->imports = NULL;
	if (charset != NULL)
		c->charset = strdup(charset);
	else
		c->charset = NULL;

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_DEFAULT;
	params.charset = charset;
	params.url = url;
	params.title = NULL;
	params.allow_quirks = quirks;
	params.inline_style = false;
	params.resolve = nscss_resolve_url;
	params.resolve_pw = NULL;
	params.import = nscss_handle_import;
	params.import_pw = c;
	params.color = ns_system_colour;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	error = css_stylesheet_create(&params, &c->sheet);
	if (error != CSS_OK) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

/**
 * Process CSS source data
 *
 * \param c     Content structure
 * \param data  Data to process
 * \param size  Number of bytes to process
 * \return true on success, false on failure
 */
bool nscss_process_data(struct content *c, const char *data, unsigned int size)
{
	nscss_content *css = (nscss_content *) c;
	union content_msg_data msg_data;
	css_error error;

	error = nscss_process_css_data(&css->data, data, size);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
	}

	return (error == CSS_OK || error == CSS_NEEDDATA);
}

/**
 * Process CSS data
 *
 * \param c     CSS content object
 * \param data  Data to process
 * \param size  Number of bytes to process
 * \return CSS_OK on success, appropriate error otherwise
 */
static css_error nscss_process_css_data(struct content_css_data *c,
		const char *data, unsigned int size)
{
	return css_stylesheet_append_data(c->sheet, 
			(const uint8_t *) data, size);
}

/**
 * Convert a CSS content ready for use
 *
 * \param c  Content to convert
 * \return true on success, false on failure
 */
bool nscss_convert(struct content *c)
{
	nscss_content *css = (nscss_content *) c;
	union content_msg_data msg_data;
	css_error error;

	error = nscss_convert_css_data(&css->data);
	if (error != CSS_OK) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		return false;
	}

	return true;
}

/**
 * Convert CSS data ready for use
 *
 * \param c  CSS data to convert
 * \return CSS error
 */
static css_error nscss_convert_css_data(struct content_css_data *c)
{
	css_error error;

	error = css_stylesheet_data_done(c->sheet);

	/* Process pending imports */
	if (error == CSS_IMPORTS_PENDING) {
		/* We must not have registered any imports yet */
		assert(c->next_to_register == (uint32_t) -1);

		/* Start registering, until we find one that 
		 * hasn't finished fetching */
		c->next_to_register = 0;
		error = nscss_register_imports(c);
	} else if (error == CSS_OK) {
		/* No imports, and no errors, so complete conversion */
		c->done(c, c->pw);
	} else {
		const char *url;

		if (css_stylesheet_get_url(c->sheet, &url) == CSS_OK) {
			LOG(("Failed converting %p %s (%d)", c, url, error));
		} else {
			LOG(("Failed converting %p (%d)", c, error));
		}
	}

	return error;
}

/**
 * Clean up a CSS content
 *
 * \param c  Content to clean up
 */
void nscss_destroy(struct content *c)
{
	nscss_content *css = (nscss_content *) c;

	nscss_destroy_css_data(&css->data);
}

/**
 * Clean up CSS data
 *
 * \param c  CSS data to clean up
 */
static void nscss_destroy_css_data(struct content_css_data *c)
{
	uint32_t i;

	for (i = 0; i < c->import_count; i++) {
		if (c->imports[i].c != NULL) {
			hlcache_handle_release(c->imports[i].c);
		}
		c->imports[i].c = NULL;
	}

	free(c->imports);

	if (c->sheet != NULL) {
		css_stylesheet_destroy(c->sheet);
		c->sheet = NULL;
	}

	free(c->charset);
}

nserror nscss_clone(const struct content *old, struct content **newc)
{
	const nscss_content *old_css = (const nscss_content *) old;
	nscss_content *new_css;
	const char *data;
	unsigned long size;
	nserror error;

	new_css = calloc(1, sizeof(nscss_content));
	if (new_css == NULL)
		return NSERROR_NOMEM;

	/* Clone content */
	error = content__clone(old, &new_css->base);
	if (error != NSERROR_OK) {
		content_destroy(&new_css->base);
		return error;
	}

	/* Simply replay create/process/convert */
	error = nscss_create_css_data(&new_css->data,
			nsurl_access(content_get_url(&new_css->base)),
			old_css->data.charset, 
			new_css->base.quirks,
			nscss_content_done, new_css);
	if (error != NSERROR_OK) {
		content_destroy(&new_css->base);
		return error;
	}

	data = content__get_source_data(&new_css->base, &size);
	if (size > 0) {
		if (nscss_process_data(&new_css->base, data, size) == false) {
			content_destroy(&new_css->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	if (old->status == CONTENT_STATUS_READY ||
			old->status == CONTENT_STATUS_DONE) {
		if (nscss_convert(&new_css->base) == false) {
			content_destroy(&new_css->base);
			return NSERROR_CLONE_FAILED;
		}
	}

	*newc = (struct content *) new_css;

	return NSERROR_OK;
}

bool nscss_matches_quirks(const struct content *c, bool quirks)
{
	return c->quirks == quirks;
}

/**
 * Retrieve the stylesheet object associated with a CSS content
 *
 * \param h  Stylesheet content
 * \return Pointer to stylesheet object
 */
css_stylesheet *nscss_get_stylesheet(struct hlcache_handle *h)
{
	nscss_content *c = (nscss_content *) hlcache_handle_get_content(h);

	assert(c != NULL);

	return c->data.sheet;
}

/**
 * Retrieve imported stylesheets
 *
 * \param h  Stylesheet containing imports
 * \param n  Pointer to location to receive number of imports
 * \return Pointer to array of imported stylesheets
 */
struct nscss_import *nscss_get_imports(hlcache_handle *h, uint32_t *n)
{
	nscss_content *c = (nscss_content *) hlcache_handle_get_content(h);

	assert(c != NULL);
	assert(n != NULL);

	*n = c->data.import_count;

	return c->data.imports;
}

/**
 * Compute the type of a content
 *
 * \return CONTENT_CSS
 */
content_type nscss_content_type(void)
{
	return CONTENT_CSS;
}

/*****************************************************************************
 * Object completion                                                         *
 *****************************************************************************/

/**
 * Handle notification that a CSS object is done
 *
 * \param css  CSS object
 * \param pw   Private data
 */
void nscss_content_done(struct content_css_data *css, void *pw)
{
	union content_msg_data msg_data;
	struct content *c = pw;
	uint32_t i;
	size_t size;
	css_error error;

	/* Retrieve the size of this sheet */
	error = css_stylesheet_size(css->sheet, &size);
	if (error != CSS_OK) {
		msg_data.error = "?";
		content_broadcast(c, CONTENT_MSG_ERROR, msg_data);
		content_set_error(c);
		return;
	}
	c->size += size;

	/* Add on the size of the imported sheets */
	for (i = 0; i < css->import_count; i++) {
		if (css->imports[i].c != NULL) {
			struct content *import = hlcache_handle_get_content(
					css->imports[i].c);

			if (import != NULL) {
				c->size += import->size;
			}
		}
	}

	/* Finally, catch the content's users up with reality */
	content_set_ready(c);
	content_set_done(c);
}

/*****************************************************************************
 * Import handling                                                           *
 *****************************************************************************/

/**
 * Handle notification of the need for an imported stylesheet
 *
 * \param pw      CSS object requesting the import
 * \param parent  Stylesheet requesting the import
 * \param url     URL of the imported sheet
 * \param media   Applicable media for the imported sheet
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error nscss_handle_import(void *pw, css_stylesheet *parent,
		lwc_string *url, uint64_t media)
{
	content_type accept = CONTENT_CSS;
	struct content_css_data *c = pw;
	nscss_import_ctx *ctx;
	hlcache_child_context child;
	struct nscss_import *imports;
	const char *referer;
	css_error error;
	nserror nerror;

	nsurl *ns_url;
	nsurl *ns_ref;

	assert(parent == c->sheet);

	error = css_stylesheet_get_url(c->sheet, &referer);
	if (error != CSS_OK) {
		return error;
	}

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return CSS_NOMEM;

	ctx->css = c;
	ctx->index = c->import_count;

	/* Increase space in table */
	imports = realloc(c->imports, (c->import_count + 1) * 
			sizeof(struct nscss_import));
	if (imports == NULL) {
		free(ctx);
		return CSS_NOMEM;
	}
	c->imports = imports;

	/** \todo fallback charset */
	child.charset = NULL;
	error = css_stylesheet_quirks_allowed(c->sheet, &child.quirks);
	if (error != CSS_OK) {
		free(ctx);
		return error;
	}

	/* Create content */
	c->imports[c->import_count].media = media;

	/* TODO: Why aren't we getting a relative url part, to join? */
	nerror = nsurl_create(lwc_string_data(url), &ns_url);
	if (nerror != NSERROR_OK) {
		free(ctx);
		return CSS_NOMEM;
	}

	/* TODO: Constructing nsurl for referer here is silly, avoid */
	nerror = nsurl_create(referer, &ns_ref);
	if (nerror != NSERROR_OK) {
		nsurl_unref(ns_url);
		free(ctx);
		return CSS_NOMEM;
	}

	/* Avoid importing ourself */
	if (nsurl_compare(ns_url, ns_ref, NSURL_COMPLETE)) {
		c->imports[c->import_count].c = NULL;
		/* No longer require context as we're not fetching anything */
		free(ctx);
		ctx = NULL;
	} else {
		nerror = hlcache_handle_retrieve(ns_url,
				0, ns_ref, NULL, nscss_import, ctx,
				&child, accept,
				&c->imports[c->import_count].c);
		if (nerror != NSERROR_OK) {
			free(ctx);
			return CSS_NOMEM;
		}
	}

	nsurl_unref(ns_url);
	nsurl_unref(ns_ref);

#ifdef NSCSS_IMPORT_TRACE
	LOG(("Import %d '%s' -> (handle: %p ctx: %p)", 
			c->import_count, lwc_string_data(url), 
			c->imports[c->import_count].c, ctx));
#endif

	c->import_count++;

	return CSS_OK;
}

/**
 * Handler for imported stylesheet events
 *
 * \param handle  Handle for stylesheet
 * \param event   Event object
 * \param pw      Callback context
 * \return NSERROR_OK on success, appropriate error otherwise
 */
nserror nscss_import(hlcache_handle *handle,
		const hlcache_event *event, void *pw)
{
	nscss_import_ctx *ctx = pw;
	css_error error = CSS_OK;

#ifdef NSCSS_IMPORT_TRACE
	LOG(("Event %d for %p (%p)", event->type, handle, ctx));
#endif

	assert(ctx->css->imports[ctx->index].c == handle);

	switch (event->type) {
	case CONTENT_MSG_DONE:
		error = nscss_import_complete(ctx);
		break;

	case CONTENT_MSG_ERROR:
		hlcache_handle_release(handle);
		ctx->css->imports[ctx->index].c = NULL;

		error = nscss_import_complete(ctx);
		/* Already released handle */
		break;

	default:
		break;
	}

	/* Preserve out-of-memory. Anything else is OK */
	return error == CSS_NOMEM ? NSERROR_NOMEM : NSERROR_OK;
}

/**
 * Handle an imported stylesheet completing
 *
 * \param ctx  Import context
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error nscss_import_complete(nscss_import_ctx *ctx)
{
	css_error error = CSS_OK;

	/* If this import is the next to be registered, do so */
	if (ctx->css->next_to_register == ctx->index)
		error = nscss_register_imports(ctx->css);

#ifdef NSCSS_IMPORT_TRACE
	LOG(("Destroying import context %p for %d", ctx, ctx->index));
#endif

	/* No longer need import context */
	free(ctx);

	return error;
}

/*****************************************************************************
 * Import registration                                                       *
 *****************************************************************************/

/**
 * Register imports with a stylesheet
 *
 * \param c  CSS object containing the imports
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error nscss_register_imports(struct content_css_data *c)
{
	uint32_t index;
	css_error error;

	assert(c->next_to_register != (uint32_t) -1);
	assert(c->next_to_register < c->import_count);

	/* Register imported sheets */
	for (index = c->next_to_register; index < c->import_count; index++) {
		/* Stop registering if we encounter one whose fetch hasn't 
		 * completed yet. We'll resume at this point when it has 
		 * completed.
		 */
		if (c->imports[index].c != NULL && 
			content_get_status(c->imports[index].c) != 
				CONTENT_STATUS_DONE) {
			break;
		}

		error = nscss_register_import(c, c->imports[index].c);
		if (error != CSS_OK)
			return error;
	}

	/* Record identity of the next import to register */
	c->next_to_register = (uint32_t) index;

	if (c->next_to_register == c->import_count) {
		/* No more imports: notify parent that we're DONE */
		c->done(c, c->pw);
	}

	return CSS_OK;
}


/**
 * Register an import with a stylesheet
 *
 * \param c       CSS object that requested the import
 * \param import  Cache handle of import, or NULL for blank
 * \return CSS_OK on success, appropriate error otherwise
 */
css_error nscss_register_import(struct content_css_data *c,
		const hlcache_handle *import)
{
	css_stylesheet *sheet;
	css_error error;

	if (import != NULL) {
		nscss_content *s = 
			(nscss_content *) hlcache_handle_get_content(import);
		sheet = s->data.sheet;
	} else {
		/* Create a blank sheet if needed. */
		if (blank_import == NULL) {
			css_stylesheet_params params;

			params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
			params.level = CSS_LEVEL_DEFAULT;
			params.charset = NULL;
			params.url = "";
			params.title = NULL;
			params.allow_quirks = false;
			params.inline_style = false;
			params.resolve = nscss_resolve_url;
			params.resolve_pw = NULL;
			params.import = NULL;
			params.import_pw = NULL;
			params.color = ns_system_colour;
			params.color_pw = NULL;
			params.font = NULL;
			params.font_pw = NULL;

			error = css_stylesheet_create(&params, &blank_import);
			if (error != CSS_OK) {
				return error;
			}

			error = css_stylesheet_data_done(blank_import);
			if (error != CSS_OK) {
				css_stylesheet_destroy(blank_import);
				return error;
			}
		}

		sheet = blank_import;
	}

	error = css_stylesheet_register_import(c->sheet, sheet);
	if (error != CSS_OK) {
		return error;
	}

	return error;
}

/**
 * Clean up after the CSS content handler
 */
static void nscss_fini(void)
{
	if (blank_import != NULL) {
		css_stylesheet_destroy(blank_import);
		blank_import = NULL;
	}
}

static const content_handler css_content_handler = {
	.fini = nscss_fini,
	.create = nscss_create,
	.process_data = nscss_process_data,
	.data_complete = nscss_convert,
	.destroy = nscss_destroy,
	.clone = nscss_clone,
	.matches_quirks = nscss_matches_quirks,
	.type = nscss_content_type,
	.no_share = false,
};

/**
 * Initialise the CSS content handler
 */
nserror nscss_init(void)
{
	nserror error;

	error = content_factory_register_handler("text/css", 
			&css_content_handler);
	if (error != NSERROR_OK) 
		goto error;

	return NSERROR_OK;

error:
	nscss_fini();

	return error;
}
