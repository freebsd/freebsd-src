/*
 * Copyright 2012 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2004-2007 James Bursa <bursa@users.sourceforge.net>
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
 * Save HTML document with dependencies (implementation).
 */

#include "utils/config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include <dom/dom.h>

#include "content/content.h"
#include "content/hlcache.h"
#include "css/css.h"
#include "desktop/save_complete.h"
#include "desktop/gui_factory.h"
#include "render/box.h"
#include "render/html.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utf8.h"
#include "utils/utils.h"

regex_t save_complete_import_re;

/** An entry in save_complete_list. */
typedef struct save_complete_entry {
	hlcache_handle *content;
	struct save_complete_entry *next; /**< Next entry in list */
} save_complete_entry;

typedef struct save_complete_ctx {
    const char *path;
    save_complete_entry *list;
    save_complete_set_type_cb set_type;

    nsurl *base;
    FILE *fp;
    enum { STATE_NORMAL, STATE_IN_STYLE } iter_state;
} save_complete_ctx;

typedef enum {
	EVENT_ENTER,
	EVENT_LEAVE
} save_complete_event_type;


static bool save_complete_save_html(save_complete_ctx *ctx, hlcache_handle *c,
		bool index);
static bool save_complete_save_imported_sheets(save_complete_ctx *ctx,
		struct nscss_import *imports, uint32_t import_count);


static void save_complete_ctx_initialise(save_complete_ctx *ctx,
		const char *path, save_complete_set_type_cb set_type)
{
	ctx->path = path;
	ctx->list = NULL;
	ctx->set_type = set_type;
}

static void save_complete_ctx_finalise(save_complete_ctx *ctx)
{
	save_complete_entry *list = ctx->list;

	while (list != NULL) {
		save_complete_entry *next = list->next;
		free(list);
		list = next;
	}
}

static bool save_complete_ctx_add_content(save_complete_ctx *ctx,
		hlcache_handle *content)
{
	save_complete_entry *entry;

	entry = malloc(sizeof (*entry));
	if (entry == NULL)
		return false;

	entry->content = content;
	entry->next = ctx->list;
	ctx->list = entry;

	return true;
}


static hlcache_handle *save_complete_ctx_find_content(save_complete_ctx *ctx,
		const nsurl *url)
{
	save_complete_entry *entry;

	for (entry = ctx->list; entry != NULL; entry = entry->next)
		if (nsurl_compare(url,
				hlcache_handle_get_url(entry->content),
				NSURL_COMPLETE))
			return entry->content;

	return NULL;
}


static bool save_complete_ctx_has_content(save_complete_ctx *ctx,
		hlcache_handle *content)
{
	save_complete_entry *entry;

	for (entry = ctx->list; entry != NULL; entry = entry->next)
		if (entry->content == content)
			return true;

	return false;
}

static bool save_complete_save_buffer(save_complete_ctx *ctx,
		const char *leafname, const char *data, size_t data_len,
		lwc_string *mime_type)
{
	FILE *fp;
	bool error;
	char fullpath[PATH_MAX];

	strncpy(fullpath, ctx->path, sizeof fullpath);
	error = guit->fetch->path_add_part(fullpath, sizeof fullpath, leafname);
	if (error == false) {
		warn_user("NoMemory", NULL);
		return false;
	}

	fp = fopen(fullpath, "wb");
	if (fp == NULL) {
		LOG(("fopen(): errno = %i", errno));
		warn_user("SaveError", strerror(errno));
		return false;
	}

	fwrite(data, sizeof(*data), data_len, fp);

	fclose(fp);

	if (ctx->set_type != NULL)
		ctx->set_type(fullpath, mime_type);

	return true;
}

/**
 * Rewrite stylesheet \@import rules for save complete.
 *
 * \param  source  stylesheet source
 * \param  size    size of source
 * \param  base    url of stylesheet
 * \param  osize   updated with the size of the result
 * \return  converted source, or NULL on out of memory
 */

static char *save_complete_rewrite_stylesheet_urls(save_complete_ctx *ctx,
		const char *source, unsigned long size, const nsurl *base,
		unsigned long *osize)
{
	char *rewritten;
	unsigned long offset = 0;
	unsigned int imports = 0;
	nserror error;

	/* count number occurrences of @import to (over)estimate result size */
	/* can't use strstr because source is not 0-terminated string */
	for (offset = 0; SLEN("@import") < size &&
			offset <= size - SLEN("@import"); offset++) {
		if (source[offset] == '@' &&
				tolower(source[offset + 1]) == 'i' &&
				tolower(source[offset + 2]) == 'm' &&
				tolower(source[offset + 3]) == 'p' &&
				tolower(source[offset + 4]) == 'o' &&
				tolower(source[offset + 5]) == 'r' &&
				tolower(source[offset + 6]) == 't')
			imports++;
	}

	rewritten = malloc(size + imports * 20);
	if (rewritten == NULL)
		return NULL;
	*osize = 0;

	offset = 0;
	while (offset < size) {
		const char *import_url = NULL;
		char *import_url_copy;
		int import_url_len = 0;
		nsurl *url = NULL;
		regmatch_t match[11];
		int m = regexec(&save_complete_import_re, source + offset,
				11, match, 0);
		if (m)
			break;

		if (match[2].rm_so != -1) {
			import_url = source + offset + match[2].rm_so;
			import_url_len = match[2].rm_eo - match[2].rm_so;
		} else if (match[4].rm_so != -1) {
			import_url = source + offset + match[4].rm_so;
			import_url_len = match[4].rm_eo - match[4].rm_so;
		} else if (match[6].rm_so != -1) {
			import_url = source + offset + match[6].rm_so;
			import_url_len = match[6].rm_eo - match[6].rm_so;
		} else if (match[8].rm_so != -1) {
			import_url = source + offset + match[8].rm_so;
			import_url_len = match[8].rm_eo - match[8].rm_so;
		} else if (match[10].rm_so != -1) {
			import_url = source + offset + match[10].rm_so;
			import_url_len = match[10].rm_eo - match[10].rm_so;
		}
		assert(import_url != NULL);

		import_url_copy = strndup(import_url, import_url_len);
		if (import_url_copy == NULL) {
			free(rewritten);
			return NULL;
		}

		error = nsurl_join(base, import_url_copy, &url);
		free(import_url_copy);
		if (error == NSERROR_NOMEM) {
			free(rewritten);
			return NULL;
		}

		/* copy data before match */
		memcpy(rewritten + *osize, source + offset, match[0].rm_so);
		*osize += match[0].rm_so;

		if (url != NULL) {
			hlcache_handle *content;
			content = save_complete_ctx_find_content(ctx, url);
			if (content != NULL) {
				/* replace import */
				char buf[64];
				snprintf(buf, sizeof buf, "@import '%p'",
						content);
				memcpy(rewritten + *osize, buf, strlen(buf));
				*osize += strlen(buf);
			} else {
				/* copy import */
				memcpy(rewritten + *osize,
					source + offset + match[0].rm_so,
					match[0].rm_eo - match[0].rm_so);
				*osize += match[0].rm_eo - match[0].rm_so;
			}
			nsurl_unref(url);
		} else {
			/* copy import */
			memcpy(rewritten + *osize,
				source + offset + match[0].rm_so,
				match[0].rm_eo - match[0].rm_so);
			*osize += match[0].rm_eo - match[0].rm_so;
		}

		assert(0 < match[0].rm_eo);
		offset += match[0].rm_eo;
	}

	/* copy rest of source */
	if (offset < size) {
		memcpy(rewritten + *osize, source + offset, size - offset);
		*osize += size - offset;
	}

	return rewritten;
}

static bool save_complete_save_stylesheet(save_complete_ctx *ctx,
		hlcache_handle *css)
{
	const char *css_data;
	unsigned long css_size;
	char *source;
	unsigned long source_len;
	struct nscss_import *imports;
	uint32_t import_count;
	lwc_string *type;
	char filename[32];
	bool result;

	if (save_complete_ctx_has_content(ctx, css))
		return true;

	if (save_complete_ctx_add_content(ctx, css) == false) {
		warn_user("NoMemory", 0);
		return false;
	}

	imports = nscss_get_imports(css, &import_count);
	if (save_complete_save_imported_sheets(ctx,
			imports, import_count) == false)
		return false;

	css_data = content_get_source_data(css, &css_size);
	source = save_complete_rewrite_stylesheet_urls(ctx, css_data, css_size,
			hlcache_handle_get_url(css), &source_len);
	if (source == NULL) {
		warn_user("NoMemory", 0);
		return false;
	}

	type = content_get_mime_type(css);
	if (type == NULL) {
		free(source);
		return false;
	}

	snprintf(filename, sizeof filename, "%p", css);

	result = save_complete_save_buffer(ctx, filename,
			source, source_len, type);

	lwc_string_unref(type);
	free(source);

	return result;
}

static bool save_complete_save_imported_sheets(save_complete_ctx *ctx,
		struct nscss_import *imports, uint32_t import_count)
{
	uint32_t i;

	for (i = 0; i < import_count; i++) {
		if (save_complete_save_stylesheet(ctx, imports[i].c) == false)
			return false;
	}

	return true;
}

static bool save_complete_save_html_stylesheet(save_complete_ctx *ctx,
		struct html_stylesheet *sheet)
{
	if (sheet->sheet == NULL)
		return true;

	return save_complete_save_stylesheet(ctx, sheet->sheet);
}

static bool save_complete_save_html_stylesheets(save_complete_ctx *ctx,
		hlcache_handle *c)
{
	struct html_stylesheet *sheets;
	unsigned int i, count;

	sheets = html_get_stylesheets(c, &count);

	for (i = STYLESHEET_START; i != count; i++) {
		if (save_complete_save_html_stylesheet(ctx,
				&sheets[i]) == false)
			return false;
	}

	return true;
}

static bool save_complete_save_html_object(save_complete_ctx *ctx,
		hlcache_handle *obj)
{
	const char *obj_data;
	unsigned long obj_size;
	lwc_string *type;
	bool result;
	char filename[32];

	if (content_get_type(obj) == CONTENT_NONE)
		return true;

	obj_data = content_get_source_data(obj, &obj_size);
	if (obj_data == NULL)
		return true;

	if (save_complete_ctx_has_content(ctx, obj))
		return true;

	if (save_complete_ctx_add_content(ctx, obj) == false) {
		warn_user("NoMemory", 0);
		return false;
	}

	if (content_get_type(obj) == CONTENT_HTML) {
		return save_complete_save_html(ctx, obj, false);
	}

	snprintf(filename, sizeof filename, "%p", obj);

	type = content_get_mime_type(obj);
	if (type == NULL)
		return false;

	result = save_complete_save_buffer(ctx, filename, 
			obj_data, obj_size, type);

	lwc_string_unref(type);

	return result;
}

static bool save_complete_save_html_objects(save_complete_ctx *ctx,
		hlcache_handle *c)
{
	struct content_html_object *object;
	unsigned int count;

	object = html_get_objects(c, &count);

	for (; object != NULL; object = object->next) {
		if ((object->content != NULL) && (object->box != NULL)) {
			if (save_complete_save_html_object(ctx,
					object->content) == false)
				return false;
		}
	}

	return true;
}

static bool save_complete_libdom_treewalk(dom_node *root,
		bool (*callback)(dom_node *node,
				save_complete_event_type event_type, void *ctx),
		void *ctx)
{
	dom_node *node;

	node = dom_node_ref(root); /* tree root */

	while (node != NULL) {
		dom_node *next = NULL;
		dom_exception exc;

		exc = dom_node_get_first_child(node, &next);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(node);
			break;
		}

		if (next != NULL) {  /* 1. children */
			dom_node_unref(node);
			node = next;
		} else {
			exc = dom_node_get_next_sibling(node, &next);
			if (exc != DOM_NO_ERR) {
				dom_node_unref(node);
				break;
			}

			if (next != NULL) {  /* 2. siblings */
				if (callback(node, EVENT_LEAVE, ctx) == false) {
					return false;
				}
				dom_node_unref(node);
				node = next;
			} else {  /* 3. ancestor siblings */
				while (node != NULL) {
					exc = dom_node_get_next_sibling(node,
							&next);
					if (exc != DOM_NO_ERR) {
						dom_node_unref(node);
						node = NULL;
						break;
					}

					if (next != NULL) {
						dom_node_unref(next);
						break;
					}

					exc = dom_node_get_parent_node(node,
							&next);
					if (exc != DOM_NO_ERR) {
						dom_node_unref(node);
						node = NULL;
						break;
					}

					if (callback(node, EVENT_LEAVE,
							ctx) == false) {
						return false;
					}
					dom_node_unref(node);
					node = next;
				}

				if (node == NULL)
					break;

				exc = dom_node_get_next_sibling(node, &next);
				if (exc != DOM_NO_ERR) {
					dom_node_unref(node);
					break;
				}

				if (callback(node, EVENT_LEAVE, ctx) == false) {
					return false;
				}
				dom_node_unref(node);
				node = next;
			}
		}

		assert(node != NULL);

		if (callback(node, EVENT_ENTER, ctx) == false) {
			return false; /* callback caused early termination */
		}

	}

	return true;
}

static bool save_complete_rewrite_url_value(save_complete_ctx *ctx,
		const char *value, size_t value_len)
{
	nsurl *url;
	hlcache_handle *content;
	char *escaped;
	nserror error;

	error = nsurl_join(ctx->base, value, &url);
	if (error == NSERROR_NOMEM)
		return false;

	if (url != NULL) {
		content = save_complete_ctx_find_content(ctx, url);
		if (content != NULL) {
			/* found a match */
			nsurl_unref(url);

			fprintf(ctx->fp, "\"%p\"", content);
		} else {
			/* no match found */
			error = utf8_to_html(nsurl_access(url), "UTF-8",
					nsurl_length(url), &escaped);
			nsurl_unref(url);

			if (error != NSERROR_OK)
				return false;

			fprintf(ctx->fp, "\"%s\"", escaped);

			free(escaped);
		}
	} else {
		error = utf8_to_html(value, "UTF-8", value_len, &escaped);
		if (error != NSERROR_OK)
			return false;

		fprintf(ctx->fp, "\"%s\"", escaped);

		free(escaped);
	}

	return true;
}

static bool save_complete_write_value(save_complete_ctx *ctx,
		const char *value, size_t value_len)
{
	char *escaped;
	nserror ret;

	ret = utf8_to_html(value, "UTF-8", value_len, &escaped);
	if (ret != NSERROR_OK)
		return false;

	fprintf(ctx->fp, "\"%s\"", escaped);

	free(escaped);

	return true;
}

static bool save_complete_handle_attr_value(save_complete_ctx *ctx,
		dom_string *node_name, dom_string *attr_name,
		dom_string *attr_value)
{
	const char *node_data = dom_string_data(node_name);
	size_t node_len = dom_string_byte_length(node_name);
	const char *name_data = dom_string_data(attr_name);
	size_t name_len = dom_string_byte_length(attr_name);
	const char *value_data = dom_string_data(attr_value);
	size_t value_len = dom_string_byte_length(attr_value);

	/**
	 * We only need to consider the following cases:
	 *
	 * Attribute:      Elements:
	 *
	 * 1)   data         <object>
	 * 2)   href         <a> <area> <link>
	 * 3)   src          <script> <input> <frame> <iframe> <img>
	 * 4)   background   any (except those above)
	 */
	/* 1 */
	if (name_len == SLEN("data") && 
			strncasecmp(name_data, "data", name_len) == 0) {
		if (node_len == SLEN("object") &&
				strncasecmp(node_data,
						"object", node_len) == 0) {
			return save_complete_rewrite_url_value(ctx,
					value_data, value_len);
		} else {
			return save_complete_write_value(ctx,
					value_data, value_len);
		}
	}
	/* 2 */
	else if (name_len == SLEN("href") &&
			strncasecmp(name_data, "href", name_len) == 0) {
		if ((node_len == SLEN("a") && 
				strncasecmp(node_data, "a", node_len) == 0) ||
			(node_len == SLEN("area") &&
				strncasecmp(node_data, "area", 
					node_len) == 0) ||
			(node_len == SLEN("link") && 
				strncasecmp(node_data, "link", 
					node_len) == 0)) {
			return save_complete_rewrite_url_value(ctx,
					value_data, value_len);
		} else {
			return save_complete_write_value(ctx,
					value_data, value_len);
		}
	} 
	/* 3 */
	else if (name_len == SLEN("src") &&
			strncasecmp(name_data, "src", name_len) == 0) {
		if ((node_len == SLEN("frame") &&
				strncasecmp(node_data, "frame",
					node_len) == 0) ||
			(node_len == SLEN("iframe") &&
				strncasecmp(node_data, "iframe",
					node_len) == 0) ||
			(node_len == SLEN("input") &&
				strncasecmp(node_data, "input",
					node_len) == 0) ||
			(node_len == SLEN("img") &&
				strncasecmp(node_data, "img",
					node_len) == 0) ||
			(node_len == SLEN("script") &&
				strncasecmp(node_data, "script",
					node_len) == 0)) {
			return save_complete_rewrite_url_value(ctx,
					value_data, value_len);
		} else {
			return save_complete_write_value(ctx,
					value_data, value_len);
		}
	}
	/* 4 */
	else if (name_len == SLEN("background") &&
			strncasecmp(name_data, "background", name_len) == 0) {
		return save_complete_rewrite_url_value(ctx,
				value_data, value_len);
	} else {
		return save_complete_write_value(ctx,
				value_data, value_len);
	}
}

static bool save_complete_handle_attr(save_complete_ctx *ctx,
		dom_string *node_name, dom_attr *attr)
{
	dom_string *name;
	const char *name_data;
	size_t name_len;
	dom_string *value;
	dom_exception error;

	error = dom_attr_get_name(attr, &name);
	if (error != DOM_NO_ERR)
		return false;

	if (name == NULL)
		return true;

	error = dom_attr_get_value(attr, &value);
	if (error != DOM_NO_ERR) {
		dom_string_unref(name);
		return false;
	}

	name_data = dom_string_data(name);
	name_len = dom_string_byte_length(name);

	fputc(' ', ctx->fp);
	fwrite(name_data, sizeof(*name_data), name_len, ctx->fp);

	if (value != NULL) {
		fputc('=', ctx->fp);
		if (save_complete_handle_attr_value(ctx, node_name, 
				name, value) == false) {
			dom_string_unref(value);
			dom_string_unref(name);
			return false;
		}
	}

	dom_string_unref(name);

	return true;
}

static bool save_complete_handle_attrs(save_complete_ctx *ctx,
		dom_string *node_name, dom_namednodemap *attrs)
{
	uint32_t length, i;
	dom_exception error;

	error = dom_namednodemap_get_length(attrs, &length);
	if (error != DOM_NO_ERR)
		return false;

	for (i = 0; i < length; i++) {
		dom_attr *attr;

		error = dom_namednodemap_item(attrs, i, (void *) &attr);
		if (error != DOM_NO_ERR)
			return false;

		if (attr == NULL)
			continue;

		if (save_complete_handle_attr(ctx, node_name, attr) == false) {
			dom_node_unref(attr);
			return false;
		}

		dom_node_unref(attr);
	}

	return true;
}

static bool save_complete_handle_element(save_complete_ctx *ctx,
		dom_node *node, save_complete_event_type event_type)
{
	dom_string *name;
	dom_namednodemap *attrs;
	const char *name_data;
	size_t name_len;
	bool process = true;
	dom_exception error;

	ctx->iter_state = STATE_NORMAL;

	error = dom_node_get_node_name(node, &name);
	if (error != DOM_NO_ERR)
		return false;

	if (name == NULL)
		return true;

	name_data = dom_string_data(name);
	name_len = dom_string_byte_length(name);

	if (name_len == SLEN("base") &&
			strncasecmp(name_data, "base", name_len) == 0) {
		/* Elide BASE elements from the output */
		process = false;
	} else if (name_len == SLEN("meta") && 
			strncasecmp(name_data, "meta", name_len) == 0) {
		/* Don't emit close tags for META elements */
		if (event_type == EVENT_LEAVE) {
			process = false;
		} else {
			/* Elide meta charsets */
			dom_string *value;
			error = dom_element_get_attribute(node,
					corestring_dom_http_equiv, &value);
			if (error != DOM_NO_ERR) {
				dom_string_unref(name);
				return false;
			}

			if (value != NULL) {
				if (dom_string_length(value) ==
					SLEN("Content-Type") &&
					strncasecmp(dom_string_data(value),
						"Content-Type",
						SLEN("Content-Type")) == 0)
					process = false;

				dom_string_unref(value);
			} else {
				bool yes;

				error = dom_element_has_attribute(node,
						corestring_dom_charset, &yes);
				if (error != DOM_NO_ERR) {
					dom_string_unref(name);
					return false;
				}

				if (yes)
					process = false;
			}
		}
	} else if (event_type == EVENT_LEAVE && 
			((name_len == SLEN("link") && 
			strncasecmp(name_data, "link", name_len) == 0))) {
		/* Don't emit close tags for void elements */
		process = false;
	}

	if (process == false) {
		dom_string_unref(name);
		return true;
	}

	fputc('<', ctx->fp);
	if (event_type == EVENT_LEAVE)
		fputc('/', ctx->fp);
	fwrite(name_data, sizeof(*name_data), name_len, ctx->fp);

	if (event_type == EVENT_ENTER) {
		error = dom_node_get_attributes(node, &attrs);
		if (error != DOM_NO_ERR) {
			dom_string_unref(name);
			return false;
		}

		if (save_complete_handle_attrs(ctx, name, attrs) == false) {
			dom_namednodemap_unref(attrs);
			dom_string_unref(name);
			return false;
		}

		dom_namednodemap_unref(attrs);
	}

	fputc('>', ctx->fp);

	/* Rewrite contents of style elements */
	if (event_type == EVENT_ENTER && name_len == SLEN("style") &&
			strncasecmp(name_data, "style", name_len) == 0) {
		dom_string *content;

		error = dom_node_get_text_content(node, &content);
		if (error != DOM_NO_ERR) {
			dom_string_unref(name);
			return false;
		}

		if (content != NULL) {
			char *rewritten;
			unsigned long len;

			/* Rewrite @import rules */
			rewritten = save_complete_rewrite_stylesheet_urls(
					ctx,
					dom_string_data(content),
					dom_string_byte_length(content),
					ctx->base,
					&len);
			if (rewritten == NULL) {
				dom_string_unref(content);
				dom_string_unref(name);
				return false;
			}

			dom_string_unref(content);

			fwrite(rewritten, sizeof(*rewritten), len, ctx->fp);

			free(rewritten);
		}

		ctx->iter_state = STATE_IN_STYLE;
	} else if (event_type == EVENT_ENTER && name_len == SLEN("head") &&
			strncasecmp(name_data, "head", name_len) == 0) {
		/* If this is a HEAD element, insert a meta charset */
		fputs("<META http-equiv=\"Content-Type\" "
				"content=\"text/html; charset=utf-8\">",
				ctx->fp);
	}

	dom_string_unref(name);

	return true;
}

static bool save_complete_node_handler(dom_node *node,
		save_complete_event_type event_type, void *ctxin)
{
	save_complete_ctx *ctx = ctxin;
	dom_node_type type;
	dom_exception error;
	nserror ret;

	error = dom_node_get_node_type(node, &type);
	if (error != DOM_NO_ERR)
		return false;

	if (type == DOM_ELEMENT_NODE) {
		return save_complete_handle_element(ctx, node, event_type);
	} else if (type == DOM_TEXT_NODE || type == DOM_COMMENT_NODE) {
		if (event_type != EVENT_ENTER)
			return true;

		if (ctx->iter_state != STATE_IN_STYLE) {
			/* Emit text content */
			dom_string *text;
			const char *text_data;
			size_t text_len;

			error = dom_characterdata_get_data(node, &text);
			if (error != DOM_NO_ERR) {
				return false;
			}

			if (type == DOM_COMMENT_NODE)
				fwrite("<!--", 1, sizeof("<!--") - 1, ctx->fp);

			if (text != NULL) {
				char *escaped;

				text_data = dom_string_data(text);
				text_len = dom_string_byte_length(text);

				ret = utf8_to_html(text_data, "UTF-8",
						text_len, &escaped);
				if (ret != NSERROR_OK)
					return false;

				fwrite(escaped, sizeof(*escaped), 
						strlen(escaped), ctx->fp);

				free(escaped);

				dom_string_unref(text);
			}

			if (type == DOM_COMMENT_NODE) {
				fwrite("-->", 1, sizeof("-->") - 1, ctx->fp);
			}
		}

	} else if (type == DOM_DOCUMENT_TYPE_NODE) {
		dom_string *name;
		const char *name_data;
		size_t name_len;

		if (event_type != EVENT_ENTER)
			return true;

		error = dom_document_type_get_name(node, &name);
		if (error != DOM_NO_ERR)
			return false;

		if (name == NULL)
			return true;

		name_data = dom_string_data(name);
		name_len = dom_string_byte_length(name);

		fputs("<!DOCTYPE ", ctx->fp);
		fwrite(name_data, sizeof(*name_data), name_len, ctx->fp);

		dom_string_unref(name);

		error = dom_document_type_get_public_id(node, &name);
		if (error != DOM_NO_ERR)
			return false;

		if (name != NULL) {
			name_data = dom_string_data(name);
			name_len = dom_string_byte_length(name);

			if (name_len > 0)
				fprintf(ctx->fp, " PUBLIC \"%.*s\"",
						(int) name_len, name_data);

			dom_string_unref(name);
		}

		error = dom_document_type_get_system_id(node, &name);
		if (error != DOM_NO_ERR)
			return false;

		if (name != NULL) {
			name_data = dom_string_data(name);
			name_len = dom_string_byte_length(name);

			if (name_len > 0)
				fprintf(ctx->fp, " \"%.*s\"",
						(int) name_len, name_data);

			dom_string_unref(name);
		}

		fputc('>', ctx->fp);
	} else if (type == DOM_DOCUMENT_NODE) {
		/* Do nothing */
	} else {
		LOG(("Unhandled node type: %d", type));
	}

	return true;
}

static bool save_complete_save_html_document(save_complete_ctx *ctx,
		hlcache_handle *c, bool index)
{
	bool error;
	FILE *fp;
	dom_document *doc;
	lwc_string *mime_type;
	char filename[32];
	char fullpath[PATH_MAX];

	strncpy(fullpath, ctx->path, sizeof fullpath);

	if (index)
		snprintf(filename, sizeof filename, "index");
	else 
		snprintf(filename, sizeof filename, "%p", c);

	error = guit->fetch->path_add_part(fullpath, sizeof fullpath, filename);
	if (error == false) {
		warn_user("NoMemory", NULL);
		return false;
	}

	fp = fopen(fullpath, "wb");
	if (fp == NULL) {
		warn_user("NoMemory", NULL);
		return false;
	}

	ctx->base = html_get_base_url(c);
	ctx->fp = fp;
	ctx->iter_state = STATE_NORMAL;

	doc = html_get_document(c);

	if (save_complete_libdom_treewalk((dom_node *) doc,
			save_complete_node_handler, ctx) == false) {
		warn_user("NoMemory", 0);
		fclose(fp);
		return false;
	}

	fclose(fp);

	mime_type = content_get_mime_type(c);
	if (mime_type != NULL) {
		if (ctx->set_type != NULL)
			ctx->set_type(fullpath, mime_type);

		lwc_string_unref(mime_type);
	}

	return true;
}

/**
 * Save an HTML page with all dependencies, recursing through imported pages.
 *
 * \param  ctx    Save complete context
 * \param  c      Content to save
 * \param  index  true to save as "index"
 * \return  true on success, false on error and error reported
 */
static bool save_complete_save_html(save_complete_ctx *ctx, hlcache_handle *c,
		bool index)
{
	if (content_get_type(c) != CONTENT_HTML)
		return false;

	if (save_complete_ctx_has_content(ctx, c))
		return true;

	if (save_complete_save_html_stylesheets(ctx, c) == false)
		return false;

	if (save_complete_save_html_objects(ctx, c) == false)
		return false;

	return save_complete_save_html_document(ctx, c, index);
}


/**
 * Create the inventory file listing original URLs.
 */

static bool save_complete_inventory(save_complete_ctx *ctx)
{
	FILE *fp;
	bool error;
	save_complete_entry *entry;
	char fullpath[PATH_MAX];

	strncpy(fullpath, ctx->path, sizeof fullpath);
	error = guit->fetch->path_add_part(fullpath, sizeof fullpath, "Inventory");
	if (error == false) {
		warn_user("NoMemory", NULL);
		return false;
	}

	fp = fopen(fullpath, "w");
	if (fp == NULL) {
		LOG(("fopen(): errno = %i", errno));
		warn_user("SaveError", strerror(errno));
		return false;
	}

	for (entry = ctx->list; entry != NULL; entry = entry->next) {
		fprintf(fp, "%p %s\n", entry->content, 
				nsurl_access(hlcache_handle_get_url(
						entry->content)));
	}

	fclose(fp);

	return true;
}

/* Documented in save_complete.h */
void save_complete_init(void)
{
	/* Match an @import rule - see CSS 2.1 G.1. */
	regcomp_wrapper(&save_complete_import_re,
			"@import"		/* IMPORT_SYM */
			"[ \t\r\n\f]*"		/* S* */
			/* 1 */
			"("			/* [ */
			/* 2 3 */
			"\"(([^\"]|[\\]\")*)\""	/* STRING (approximated) */
			"|"
			/* 4 5 */
			"'(([^']|[\\]')*)'"
			"|"			/* | */
			"url\\([ \t\r\n\f]*"	/* URI (approximated) */
			     /* 6 7 */
			     "\"(([^\"]|[\\]\")*)\""
			     "[ \t\r\n\f]*\\)"
			"|"
			"url\\([ \t\r\n\f]*"
			    /* 8 9 */
			     "'(([^']|[\\]')*)'"
			     "[ \t\r\n\f]*\\)"
			"|"
			"url\\([ \t\r\n\f]*"
			   /* 10 */
			     "([^) \t\r\n\f]*)"
			     "[ \t\r\n\f]*\\)"
			")",			/* ] */
			REG_EXTENDED | REG_ICASE);
}

/* Documented in save_complete.h */
bool save_complete(hlcache_handle *c, const char *path,
		save_complete_set_type_cb set_type)
{
	bool result;
	save_complete_ctx ctx;

	save_complete_ctx_initialise(&ctx, path, set_type);
	
	result = save_complete_save_html(&ctx, c, true);

	if (result)
		result = save_complete_inventory(&ctx);

	save_complete_ctx_finalise(&ctx);

	return result;
}

