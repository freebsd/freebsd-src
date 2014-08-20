/*
 * Copyright 2012 Vincent Sanders <vince@netsurf-browser.org>
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
 * libdom utilities (implementation).
 */

#include <assert.h>
#include <dom/dom.h>

#include "utils/config.h"
#include "utils/log.h"

#include "utils/libdom.h"

/* exported interface documented in libdom.h */
bool libdom_treewalk(dom_node *root,
		bool (*callback)(dom_node *node, dom_string *name, void *ctx),
		void *ctx)
{
	dom_node *node;
	bool result = true;

	node = dom_node_ref(root); /* tree root */

	while (node != NULL) {
		dom_node *next = NULL;
		dom_node_type type;
		dom_string *name;
		dom_exception exc;

		exc = dom_node_get_first_child(node, &next);
		if (exc != DOM_NO_ERR) {
			dom_node_unref(node);
			break;
		}

		if (next != NULL) {
			/* 1. Got children */
			dom_node_unref(node);
			node = next;
		} else {
			/* No children; siblings & ancestor's siblings */
			while (node != NULL) {
				exc = dom_node_get_next_sibling(node, &next);
				if (exc != DOM_NO_ERR) {
					dom_node_unref(node);
					node = NULL;
					break;
				}

				if (next != NULL) {
					/* 2. Got sibling */
					break;
				}

				exc = dom_node_get_parent_node(node, &next);
				if (exc != DOM_NO_ERR) {
					dom_node_unref(node);
					node = NULL;
					break;
				}

				/* 3. Try parent */
				dom_node_unref(node);
				node = next;
			}

			if (node == NULL)
				break;

			dom_node_unref(node);
			node = next;
		}

		assert(node != NULL);

		exc = dom_node_get_node_type(node, &type);
		if ((exc != DOM_NO_ERR) || (type != DOM_ELEMENT_NODE))
			continue;

		exc = dom_node_get_node_name(node, &name);
		if (exc != DOM_NO_ERR)
			continue;

		result = callback(node, name, ctx);

		dom_string_unref(name);

		if (result == false) {
			break; /* callback caused early termination */
		}

	}
	return result;
}


/* libdom_treewalk context for libdom_find_element */
struct find_element_ctx {
	lwc_string *search;
	dom_node *found;
};

/* libdom_treewalk callback for libdom_find_element */
static bool libdom_find_element_callback(dom_node *node, dom_string *name,
		void *ctx)
{
	struct find_element_ctx *data = ctx;

	if (dom_string_caseless_lwc_isequal(name, data->search)) {
		/* Found element */
		data->found = node;
		return false; /* Discontinue search */
	}

	return true; /* Continue search */
}


/* exported interface documented in libdom.h */
dom_node *libdom_find_element(dom_node *node, lwc_string *element_name)
{
	struct find_element_ctx data;

	assert(element_name != NULL);

	if (node == NULL)
		return NULL;

	data.search = element_name;
	data.found = NULL;

	libdom_treewalk(node, libdom_find_element_callback, &data);

	return data.found;
}


/* exported interface documented in libdom.h */
dom_node *libdom_find_first_element(dom_node *parent, lwc_string *element_name)
{
	dom_node *element;
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type node_type;
	dom_node *next_node;

	exc = dom_node_get_first_child(parent, &element);
	if ((exc != DOM_NO_ERR) || (element == NULL)) {
		return NULL;
	}

	/* find first node thats a element */
	do {
		exc = dom_node_get_node_type(element, &node_type);

		if ((exc == DOM_NO_ERR) && (node_type == DOM_ELEMENT_NODE)) {
			exc = dom_node_get_node_name(element, &node_name);
			if ((exc == DOM_NO_ERR) && (node_name != NULL)) {
				if (dom_string_caseless_lwc_isequal(node_name,
						     element_name)) {
					dom_string_unref(node_name);
					break;
				}
				dom_string_unref(node_name);
			}
		}

		exc = dom_node_get_next_sibling(element, &next_node);
		dom_node_unref(element);
		if (exc == DOM_NO_ERR) {
			element = next_node;
		} else {
			element = NULL;
		}
	} while (element != NULL);

	return element;
}

/* exported interface documented in libdom.h */
/* TODO: return appropriate errors */
nserror libdom_iterate_child_elements(dom_node *parent, 
		libdom_iterate_cb cb, void *ctx)
{
	dom_nodelist *children;
	uint32_t index, num_children;
	dom_exception error;

	error = dom_node_get_child_nodes(parent, &children);
	if (error != DOM_NO_ERR || children == NULL)
		return NSERROR_NOMEM;

	error = dom_nodelist_get_length(children, &num_children);
	if (error != DOM_NO_ERR) {
		dom_nodelist_unref(children);
		return NSERROR_NOMEM;
	}

	for (index = 0; index < num_children; index++) {
		dom_node *child;
		dom_node_type type;

		error = dom_nodelist_item(children, index, &child);
		if (error != DOM_NO_ERR) {
			dom_nodelist_unref(children);
			return NSERROR_NOMEM;
		}

		error = dom_node_get_node_type(child, &type);
		if (error == DOM_NO_ERR && type == DOM_ELEMENT_NODE) {
			nserror err = cb(child, ctx);
			if (err != NSERROR_OK) {
				dom_node_unref(child);
				dom_nodelist_unref(children);
				return err;
			}
		}

		dom_node_unref(child);
	}

	dom_nodelist_unref(children);

	return NSERROR_OK;
}

/* exported interface documented in libdom.h */
nserror libdom_hubbub_error_to_nserror(dom_hubbub_error error)
{
	switch (error) {

	/* HUBBUB_REPROCESS is not handled here because it can
	 * never occur outside the hubbub treebuilder
	 */

	case DOM_HUBBUB_OK:
		/* parsed ok */
		return NSERROR_OK;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_PAUSED):
		/* hubbub input paused */
		return NSERROR_OK;

	case DOM_HUBBUB_NOMEM:
		/* out of memory error from DOM */
		return NSERROR_NOMEM;

	case DOM_HUBBUB_BADPARM:
		/* Bad parameter passed to creation */
		return NSERROR_BAD_PARAMETER;

	case DOM_HUBBUB_DOM:
		/* DOM call returned error */
		return NSERROR_DOM;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_ENCODINGCHANGE):
		/* encoding changed */
		return NSERROR_ENCODING_CHANGE;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_NOMEM):
		/* out of memory error from parser */
		return NSERROR_NOMEM;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_BADPARM):
		return NSERROR_BAD_PARAMETER;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_INVALID):
		return NSERROR_INVALID;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_FILENOTFOUND):
		return NSERROR_NOT_FOUND;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_NEEDDATA):
		return NSERROR_NEED_DATA;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_BADENCODING):
		return NSERROR_BAD_ENCODING;

	case (DOM_HUBBUB_HUBBUB_ERR | HUBBUB_UNKNOWN):
		/* currently only generated by the libdom hubbub binding */
		return NSERROR_DOM;
	default:
		/* unknown error */
		/** @todo better error handling and reporting */
		return NSERROR_UNKNOWN;
	}
	return NSERROR_UNKNOWN;
}


static void ignore_dom_msg(uint32_t severity, void *ctx, const char *msg, ...)
{
}

/* exported interface documented in libdom.h */
nserror libdom_parse_file(const char *filename, const char *encoding, dom_document **doc)
{
	dom_hubbub_parser_params parse_params;
	dom_hubbub_error error;
	dom_hubbub_parser *parser;
	dom_document *document;
	FILE *fp = NULL;
#define BUF_SIZE 512
	uint8_t buf[BUF_SIZE];

	fp = fopen(filename, "r");
	if (fp == NULL) {
		return NSERROR_NOT_FOUND;
	}

	parse_params.enc = encoding;
	parse_params.fix_enc = false;
	parse_params.enable_script = false;
	parse_params.msg = ignore_dom_msg;
	parse_params.script = NULL;
	parse_params.ctx = NULL;
	parse_params.daf = NULL;

	error = dom_hubbub_parser_create(&parse_params, &parser, &document);
	if (error != DOM_HUBBUB_OK) {
		fclose(fp);
		return libdom_hubbub_error_to_nserror(error);
	}

	while (feof(fp) == 0) {
		size_t read = fread(buf, sizeof(buf[0]), BUF_SIZE, fp);

		error = dom_hubbub_parser_parse_chunk(parser, buf, read);
		if (error != DOM_HUBBUB_OK) {
			dom_node_unref(document);
			dom_hubbub_parser_destroy(parser);
			fclose(fp);
			return NSERROR_DOM;
		}
	}

	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		dom_node_unref(document);
		dom_hubbub_parser_destroy(parser);
		fclose(fp);
		return libdom_hubbub_error_to_nserror(error);
	}

	dom_hubbub_parser_destroy(parser);
	fclose(fp);

	*doc = document;
	return NSERROR_OK;
}
