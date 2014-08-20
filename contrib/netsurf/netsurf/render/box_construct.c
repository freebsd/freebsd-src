/*
 * Copyright 2005 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2003 Phil Mellor <monkeyson@users.sourceforge.net>
 * Copyright 2005 John M Bell <jmb202@ecs.soton.ac.uk>
 * Copyright 2006 Richard Wilson <info@tinct.net>
 * Copyright 2008 Michael Drake <tlsa@netsurf-browser.org>
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
 * Conversion of XML tree to box tree (implementation).
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "utils/config.h"
#include "content/content_protected.h"
#include "css/css.h"
#include "css/utils.h"
#include "css/select.h"
#include "desktop/gui_factory.h"
#include "utils/nsoption.h"
#include "utils/corestrings.h"
#include "utils/locale.h"
#include "utils/log.h"
#include "utils/messages.h"
#include "utils/talloc.h"
#include "utils/url.h"
#include "utils/utils.h"

#include "render/box.h"
#include "render/box_textarea.h"
#include "render/form.h"
#include "render/html_internal.h"

/**
 * Context for box tree construction
 */
struct box_construct_ctx {
	html_content *content;		/**< Content we're constructing for */

	dom_node *n;			/**< Current node to process */

	struct box *root_box;		/**< Root box in the tree */

	box_construct_complete_cb cb;	/**< Callback to invoke on completion */

	int *bctx;                      /**< talloc context */
};

/**
 * Transient properties for construction of current node
 */
struct box_construct_props {
	/** Style from which to inherit, or NULL if none */
	const css_computed_style *parent_style;
	/** Current link target, or NULL if none */
	nsurl *href;
	/** Current frame target, or NULL if none */
	const char *target;
	/** Current title attribute, or NULL if none */
	const char *title;
	/** Identity of the current block-level container */
	struct box *containing_block;
	/** Current container for inlines, or NULL if none
	 * \note If non-NULL, will be the last child of containing_block */
	struct box *inline_container;
	/** Whether the current node is the root of the DOM tree */
	bool node_is_root;
};

static const content_type image_types = CONTENT_IMAGE;

/* the strings are not important, since we just compare the pointers */
const char *TARGET_SELF = "_self";
const char *TARGET_PARENT = "_parent";
const char *TARGET_TOP = "_top";
const char *TARGET_BLANK = "_blank";

static void convert_xml_to_box(struct box_construct_ctx *ctx);
static bool box_construct_element(struct box_construct_ctx *ctx,
		bool *convert_children);
static void box_construct_element_after(dom_node *n, html_content *content);
static bool box_construct_text(struct box_construct_ctx *ctx);
static css_select_results * box_get_style(html_content *c,
		const css_computed_style *parent_style, dom_node *n);
static void box_text_transform(char *s, unsigned int len,
		enum css_text_transform_e tt);
#define BOX_SPECIAL_PARAMS dom_node *n, html_content *content, \
		struct box *box, bool *convert_children
static bool box_a(BOX_SPECIAL_PARAMS);
static bool box_body(BOX_SPECIAL_PARAMS);
static bool box_br(BOX_SPECIAL_PARAMS);
static bool box_image(BOX_SPECIAL_PARAMS);
static bool box_textarea(BOX_SPECIAL_PARAMS);
static bool box_select(BOX_SPECIAL_PARAMS);
static bool box_input(BOX_SPECIAL_PARAMS);
static bool box_button(BOX_SPECIAL_PARAMS);
static bool box_frameset(BOX_SPECIAL_PARAMS);
static bool box_create_frameset(struct content_html_frames *f, dom_node *n,
		html_content *content);
static bool box_select_add_option(struct form_control *control, dom_node *n);
static bool box_noscript(BOX_SPECIAL_PARAMS);
static bool box_object(BOX_SPECIAL_PARAMS);
static bool box_embed(BOX_SPECIAL_PARAMS);
static bool box_pre(BOX_SPECIAL_PARAMS);
static bool box_iframe(BOX_SPECIAL_PARAMS);
static bool box_get_attribute(dom_node *n, const char *attribute,
		void *context, char **value);
static struct frame_dimension *box_parse_multi_lengths(const char *s,
		unsigned int *count);

/* element_table must be sorted by name */
struct element_entry {
	char name[10];	 /* element type */
	bool (*convert)(BOX_SPECIAL_PARAMS);
};
static const struct element_entry element_table[] = {
	{"a", box_a},
	{"body", box_body},
	{"br", box_br},
	{"button", box_button},
	{"embed", box_embed},
	{"frameset", box_frameset},
	{"iframe", box_iframe},
	{"image", box_image},
	{"img", box_image},
	{"input", box_input},
	{"noscript", box_noscript},
	{"object", box_object},
	{"pre", box_pre},
	{"select", box_select},
	{"textarea", box_textarea}
};
#define ELEMENT_TABLE_COUNT (sizeof(element_table) / sizeof(element_table[0]))

/**
 * Construct a box tree from an xml tree and stylesheets.
 *
 * \param n   xml tree
 * \param c   content of type CONTENT_HTML to construct box tree in
 * \param cb  callback to report conversion completion
 * \return    netsurf error code indicating status of call
 */

nserror dom_to_box(dom_node *n, html_content *c, box_construct_complete_cb cb)
{
	struct box_construct_ctx *ctx;

	if (c->bctx == NULL) {
		/* create a context allocation for this box tree */
		c->bctx = talloc_zero(0, int);
		if (c->bctx == NULL) {
			return NSERROR_NOMEM;
		}
	}

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL) {
		return NSERROR_NOMEM;
	}

	ctx->content = c;
	ctx->n = dom_node_ref(n);
	ctx->root_box = NULL;
	ctx->cb = cb;
	ctx->bctx = c->bctx;

	return guit->browser->schedule(0, (void *)convert_xml_to_box, ctx);
}

/* mapping from CSS display to box type
 * this table must be in sync with libcss' css_display enum */
static const box_type box_map[] = {
	0, /*CSS_DISPLAY_INHERIT,*/
	BOX_INLINE, /*CSS_DISPLAY_INLINE,*/
	BOX_BLOCK, /*CSS_DISPLAY_BLOCK,*/
	BOX_BLOCK, /*CSS_DISPLAY_LIST_ITEM,*/
	BOX_INLINE, /*CSS_DISPLAY_RUN_IN,*/
	BOX_INLINE_BLOCK, /*CSS_DISPLAY_INLINE_BLOCK,*/
	BOX_TABLE, /*CSS_DISPLAY_TABLE,*/
	BOX_TABLE, /*CSS_DISPLAY_INLINE_TABLE,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_ROW_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_HEADER_GROUP,*/
	BOX_TABLE_ROW_GROUP, /*CSS_DISPLAY_TABLE_FOOTER_GROUP,*/
	BOX_TABLE_ROW, /*CSS_DISPLAY_TABLE_ROW,*/
	BOX_NONE, /*CSS_DISPLAY_TABLE_COLUMN_GROUP,*/
	BOX_NONE, /*CSS_DISPLAY_TABLE_COLUMN,*/
	BOX_TABLE_CELL, /*CSS_DISPLAY_TABLE_CELL,*/
	BOX_INLINE, /*CSS_DISPLAY_TABLE_CAPTION,*/
	BOX_NONE /*CSS_DISPLAY_NONE*/
};

static inline struct box *box_for_node(dom_node *n)
{
	struct box *box = NULL;
	dom_exception err;

	err = dom_node_get_user_data(n, corestring_dom___ns_key_box_node_data,
			(void *) &box);
	if (err != DOM_NO_ERR)
		return NULL;

	return box;
}

static inline bool box_is_root(dom_node *n)
{
	dom_node *parent;
	dom_node_type type;
	dom_exception err;

	err = dom_node_get_parent_node(n, &parent);
	if (err != DOM_NO_ERR)
		return false;

	if (parent != NULL) {
		err = dom_node_get_node_type(parent, &type);

		dom_node_unref(parent);

		if (err != DOM_NO_ERR)
			return false;

		if (type != DOM_DOCUMENT_NODE)
			return false;
	}

	return true;
}

/**
 * Find the next node in the DOM tree, completing 
 * element construction where appropriate.
 *
 * \param n                 Current node
 * \param content           Containing content
 * \param convert_children  Whether to consider children of \a n
 * \return Next node to process, or NULL if complete
 *
 * \note \a n will be unreferenced
 */
static dom_node *next_node(dom_node *n, html_content *content,
		bool convert_children)
{
	dom_node *next = NULL;
	bool has_children;
	dom_exception err;

	err = dom_node_has_child_nodes(n, &has_children);
	if (err != DOM_NO_ERR) {
		dom_node_unref(n);
		return NULL;
	}

	if (convert_children && has_children) {
		err = dom_node_get_first_child(n, &next);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return NULL;
		}
		dom_node_unref(n);
	} else {
		err = dom_node_get_next_sibling(n, &next);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return NULL;
		}

		if (next != NULL) {
			if (box_for_node(n) != NULL)
				box_construct_element_after(n, content);
			dom_node_unref(n);
		} else {
			if (box_for_node(n) != NULL)
				box_construct_element_after(n, content);

			while (box_is_root(n) == false) {
				dom_node *parent = NULL;
				dom_node *parent_next = NULL;

				err = dom_node_get_parent_node(n, &parent);
				if (err != DOM_NO_ERR) {
					dom_node_unref(n);
					return NULL;
				}

				assert(parent != NULL);

				err = dom_node_get_next_sibling(parent, 
						&parent_next);
				if (err != DOM_NO_ERR) {
					dom_node_unref(parent);
					dom_node_unref(n);
					return NULL;
				}

				if (parent_next != NULL) {
					dom_node_unref(parent_next);
					dom_node_unref(parent);
					break;
				}

				dom_node_unref(n);
				n = parent;
				parent = NULL;

				if (box_for_node(n) != NULL) {
					box_construct_element_after(
							n, content);
				}
			}

			if (box_is_root(n) == false) {
				dom_node *parent = NULL;

				err = dom_node_get_parent_node(n, &parent);
				if (err != DOM_NO_ERR) {
					dom_node_unref(n);
					return NULL;
				}

				assert(parent != NULL);

				err = dom_node_get_next_sibling(parent, &next);
				if (err != DOM_NO_ERR) {
					dom_node_unref(parent);
					dom_node_unref(n);
					return NULL;
				}

				if (box_for_node(parent) != NULL) {
					box_construct_element_after(parent, 
							content);
				}

				dom_node_unref(parent);
			}

			dom_node_unref(n);
		}
	}

	return next;
}

/**
 * Convert an ELEMENT node to a box tree fragment, 
 * then schedule conversion of the next ELEMENT node
 */
void convert_xml_to_box(struct box_construct_ctx *ctx)
{
	dom_node *next;
	bool convert_children;
	uint32_t num_processed = 0;
	const uint32_t max_processed_before_yield = 10;

	do {
		convert_children = true;

		assert(ctx->n != NULL);

		if (box_construct_element(ctx, &convert_children) == false) {
			ctx->cb(ctx->content, false);
			dom_node_unref(ctx->n);
			free(ctx);
			return;
		}

		/* Find next element to process, converting text nodes as we go */
		next = next_node(ctx->n, ctx->content, convert_children);
		while (next != NULL) {
			dom_node_type type;
			dom_exception err;

			err = dom_node_get_node_type(next, &type);
			if (err != DOM_NO_ERR) {
				ctx->cb(ctx->content, false);
				dom_node_unref(next);
				free(ctx);
				return;
			}

			if (type == DOM_ELEMENT_NODE)
				break;

			if (type == DOM_TEXT_NODE) {
				ctx->n = next;
				if (box_construct_text(ctx) == false) {
					ctx->cb(ctx->content, false);
					dom_node_unref(ctx->n);
					free(ctx);
					return;
				}
			}

			next = next_node(next, ctx->content, true);
		}

		ctx->n = next;

		if (next == NULL) {
			/* Conversion complete */
			struct box root;

			memset(&root, 0, sizeof(root));

			root.type = BOX_BLOCK;
			root.children = root.last = ctx->root_box;
			root.children->parent = &root;

			/** \todo Remove box_normalise_block */
			if (box_normalise_block(&root, ctx->content) == false) {
				ctx->cb(ctx->content, false);
			} else {
				ctx->content->layout = root.children;
				ctx->content->layout->parent = NULL;

				ctx->cb(ctx->content, true);
			}

			assert(ctx->n == NULL);

			free(ctx);
			return;
		}
	} while (++num_processed < max_processed_before_yield);

	/* More work to do: schedule a continuation */
	guit->browser->schedule(0, (void *)convert_xml_to_box, ctx);
}

/**
 * Construct a list marker box
 *
 * \param box      Box to attach marker to
 * \param title    Current title attribute
 * \param content  Containing content
 * \param parent   Current block-level container
 * \return True on success, false on memory exhaustion
 */
static bool box_construct_marker(struct box *box, const char *title, 
		struct box_construct_ctx *ctx, struct box *parent)
{
	lwc_string *image_uri;
	struct box *marker;

	marker = box_create(NULL, box->style, false, NULL, NULL, title, 
			NULL, ctx->bctx);
	if (marker == false)
		return false;

	marker->type = BOX_BLOCK;

	/** \todo marker content (list-style-type) */
	switch (css_computed_list_style_type(box->style)) {
	case CSS_LIST_STYLE_TYPE_DISC:
		/* 2022 BULLET */
		marker->text = (char *) "\342\200\242";
		marker->length = 3;
		break;
	case CSS_LIST_STYLE_TYPE_CIRCLE:
		/* 25CB WHITE CIRCLE */
		marker->text = (char *) "\342\227\213";
		marker->length = 3;
		break;
	case CSS_LIST_STYLE_TYPE_SQUARE:
		/* 25AA BLACK SMALL SQUARE */
		marker->text = (char *) "\342\226\252";
		marker->length = 3;
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL:
	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
	default:
		if (parent->last) {
			struct box *last = parent->last;

			/* Drill down into last child of parent
			 * to find the list marker (if any)
			 *
			 * Floated list boxes end up as:
			 *
			 * parent
			 *   BOX_INLINE_CONTAINER
			 *     BOX_FLOAT_{LEFT,RIGHT}
			 *       BOX_BLOCK <-- list box
			 *        ...
			 */
			while (last != NULL) {
				if (last->list_marker != NULL)
					break;

				last = last->last;
			}

			if (last && last->list_marker) {
				marker->rows = last->list_marker->rows + 1;
			}
		}

		marker->text = talloc_array(ctx->bctx, char, 20);
		if (marker->text == NULL)
			return false;

		snprintf(marker->text, 20, "%u.", marker->rows);
		marker->length = strlen(marker->text);
		break;
	case CSS_LIST_STYLE_TYPE_NONE:
		marker->text = 0;
		marker->length = 0;
		break;
	}

	if (css_computed_list_style_image(box->style, &image_uri) == CSS_LIST_STYLE_IMAGE_URI && 
	    (image_uri != NULL) &&
	    (nsoption_bool(foreground_images) == true)) {
		nsurl *url;
		nserror error;

		/* TODO: we get a url out of libcss as a lwc string, but
		 *       earlier we already had it as a nsurl after we
		 *       nsurl_joined it.  Can this be improved?
		 *       For now, just making another nsurl. */
		error = nsurl_create(lwc_string_data(image_uri), &url);
		if (error != NSERROR_OK)
			return false;

		if (html_fetch_object(ctx->content, url, marker, image_types,
				ctx->content->base.available_width, 1000, false) ==
				false) {
			nsurl_unref(url);
			return false;
		}
		nsurl_unref(url);
	}

	box->list_marker = marker;
	marker->parent = box;

	return true;
}

/**
 * Construct the box required for a generated element.
 *
 * \param n        XML node of type XML_ELEMENT_NODE
 * \param content  Content of type CONTENT_HTML that is being processed
 * \param box      Box which may have generated content
 * \param style    Complete computed style for pseudo element, or NULL
 *
 * TODO:
 * This is currently incomplete. It just does enough to support the clearfix
 * hack. ( http://www.positioniseverything.net/easyclearing.html )
 */
static void box_construct_generate(dom_node *n, html_content *content,
		struct box *box, const css_computed_style *style)
{
	struct box *gen = NULL;
	const css_computed_content_item *c_item;

	/* Nothing to generate if the parent box is not a block */
	if (box->type != BOX_BLOCK)
		return;

	/* To determine if an element has a pseudo element, we select 
	 * for it and test to see if the returned style's content 
	 * property is set to normal. */
	if (style == NULL ||
			css_computed_content(style, &c_item) ==
			CSS_CONTENT_NORMAL) {
		/* No pseudo element */
		return;
	}

	/* create box for this element */
	if (css_computed_display(style, box_is_root(n)) == CSS_DISPLAY_BLOCK) {
		/* currently only support block level elements */

		/** \todo Not wise to drop const from the computed style */ 
		gen = box_create(NULL, (css_computed_style *) style,
				false, NULL, NULL, NULL, NULL, content->bctx);
		if (gen == NULL) {
			return;
		}

		/* set box type from computed display */
		gen->type = box_map[css_computed_display(
				style, box_is_root(n))];

		box_add_child(box, gen);
	}
}

/**
 * Extract transient construction properties
 *
 * \param n      Current DOM node to convert
 * \param props  Property object to populate
 */
static void box_extract_properties(dom_node *n, 
		struct box_construct_props *props)
{
	memset(props, 0, sizeof(*props));

	props->node_is_root = box_is_root(n);

	/* Extract properties from containing DOM node */
	if (props->node_is_root == false) {
		dom_node *current_node = n;
		dom_node *parent_node = NULL;
		struct box *parent_box;
		dom_exception err;

		/* Find ancestor node containing parent box */
		while (true) {
			err = dom_node_get_parent_node(current_node,
					&parent_node);
			if (err != DOM_NO_ERR || parent_node == NULL)
				break;

			parent_box = box_for_node(parent_node);

			if (parent_box != NULL) {
				props->parent_style = parent_box->style;
				props->href = parent_box->href;
				props->target = parent_box->target;
				props->title = parent_box->title;

				dom_node_unref(parent_node);
				break;
			} else {
				if (current_node != n)
					dom_node_unref(current_node);
				current_node = parent_node;
				parent_node = NULL;
			}
		}
			
		/* Find containing block (may be parent) */
		while (true) {
			struct box *b;

			err = dom_node_get_parent_node(current_node,
					&parent_node);
			if (err != DOM_NO_ERR || parent_node == NULL) {
				if (current_node != n)
					dom_node_unref(current_node);
				break;
			}

			if (current_node != n)
				dom_node_unref(current_node);

			b = box_for_node(parent_node);

			/* Children of nodes that created an inline box
			 * will generate boxes which are attached as
			 * _siblings_ of the box generated for their 
			 * parent node. Note, however, that we'll still
			 * use the parent node's styling as the parent
			 * style, above. */
			if (b != NULL && b->type != BOX_INLINE && 
					b->type != BOX_BR) {
				props->containing_block = b;

				dom_node_unref(parent_node);
				break;
			} else {
				current_node = parent_node;
				parent_node = NULL;
			}
		}
	}

	/* Compute current inline container, if any */
	if (props->containing_block != NULL && 
			props->containing_block->last != NULL &&
			props->containing_block->last->type == 
				BOX_INLINE_CONTAINER)
		props->inline_container = props->containing_block->last;
}

/**
 * Construct the box tree for an XML element.
 *
 * \param ctx               Tree construction context
 * \param convert_children  Whether to convert children
 * \return  true on success, false on memory exhaustion
 */

bool box_construct_element(struct box_construct_ctx *ctx,
		bool *convert_children)
{
	dom_string *title0, *s;
	lwc_string *id = NULL;
	struct box *box = NULL, *old_box;
	css_select_results *styles = NULL;
	struct element_entry *element;
	lwc_string *bgimage_uri;
	dom_exception err;
	struct box_construct_props props;

	assert(ctx->n != NULL);

	box_extract_properties(ctx->n, &props);

	if (props.containing_block != NULL) {
		/* In case the containing block is a pre block, we clear 
 		 * the PRE_STRIP flag since it is not used if we follow 
 		 * the pre with a tag */
		props.containing_block->flags &= ~PRE_STRIP;
	}

	styles = box_get_style(ctx->content, props.parent_style, ctx->n);
	if (styles == NULL)
		return false;

	/* Extract title attribute, if present */
	err = dom_element_get_attribute(ctx->n, corestring_dom_title, &title0);
	if (err != DOM_NO_ERR)
		return false;

	if (title0 != NULL) {
		char *t = squash_whitespace(dom_string_data(title0));

		dom_string_unref(title0);

		if (t == NULL)
			return false;

		props.title = talloc_strdup(ctx->bctx, t);

		free(t);

		if (props.title == NULL)
			return false;
	}

	/* Extract id attribute, if present */
	err = dom_element_get_attribute(ctx->n, corestring_dom_id, &s);
	if (err != DOM_NO_ERR)
		return false;

	if (s != NULL) {
		err = dom_string_intern(s, &id);
		if (err != DOM_NO_ERR)
			id = NULL;

		dom_string_unref(s);
	}

	box = box_create(styles, styles->styles[CSS_PSEUDO_ELEMENT_NONE], false,
			props.href, props.target, props.title, id,
			ctx->bctx);
	if (box == NULL)
		return false;

	/* If this is the root box, add it to the context */
	if (props.node_is_root)
		ctx->root_box = box;

	/* Deal with colspan/rowspan */
	err = dom_element_get_attribute(ctx->n, corestring_dom_colspan, &s);
	if (err != DOM_NO_ERR)
		return false;

	if (s != NULL) {
		const char *val = dom_string_data(s);

		if ('0' <= val[0] && val[0] <= '9')
			box->columns = strtol(val, NULL, 10);

		dom_string_unref(s);
	}

	err = dom_element_get_attribute(ctx->n, corestring_dom_rowspan, &s);
	if (err != DOM_NO_ERR)
		return false;

	if (s != NULL) {
		const char *val = dom_string_data(s);

		if ('0' <= val[0] && val[0] <= '9')
			box->rows = strtol(val, NULL, 10);

		dom_string_unref(s);
	}

	/* Set box type from computed display */
	if ((css_computed_position(box->style) == CSS_POSITION_ABSOLUTE ||
			css_computed_position(box->style) ==
					CSS_POSITION_FIXED) &&
			(css_computed_display_static(box->style) ==
					CSS_DISPLAY_INLINE ||
			 css_computed_display_static(box->style) ==
					CSS_DISPLAY_INLINE_BLOCK ||
			 css_computed_display_static(box->style) ==
					CSS_DISPLAY_INLINE_TABLE)) {
		/* Special case for absolute positioning: make absolute inlines
		 * into inline block so that the boxes are constructed in an
		 * inline container as if they were not absolutely positioned.
		 * Layout expects and handles this. */
		box->type = box_map[CSS_DISPLAY_INLINE_BLOCK];
	} else if (props.node_is_root) {
		/* Special case for root element: force it to BLOCK, or the
		 * rest of the layout will break. */
		box->type = BOX_BLOCK;
	} else {
		/* Normal mapping */
		box->type = box_map[css_computed_display(box->style, 
				props.node_is_root)];
	}

	/* Handle the :before pseudo element */
	box_construct_generate(ctx->n, ctx->content, box,
			box->styles->styles[CSS_PSEUDO_ELEMENT_BEFORE]);

	err = dom_node_get_node_name(ctx->n, &s);
	if (err != DOM_NO_ERR || s == NULL)
		return false;

	/* Special elements */
	element = bsearch(dom_string_data(s), element_table,
			ELEMENT_TABLE_COUNT, sizeof(element_table[0]),
			(int (*)(const void *, const void *)) strcasecmp);

	dom_string_unref(s);

	if (element != NULL) {
		/* A special convert function exists for this element */
		if (element->convert(ctx->n, ctx->content, box, 
				convert_children) == false)
			return false;
	}

	if (box->type == BOX_NONE || (css_computed_display(box->style,
			props.node_is_root) == CSS_DISPLAY_NONE &&
			props.node_is_root == false)) {
		css_select_results_destroy(styles);
		box->styles = NULL;
		box->style = NULL;

		/* Invalidate associated gadget, if any */
		if (box->gadget != NULL) {
			box->gadget->box = NULL;
			box->gadget = NULL;
		}

		/* Can't do this, because the lifetimes of boxes and gadgets
		 * are inextricably linked. Fortunately, talloc will save us
		 * (for now) */
		/* box_free_box(box); */

		*convert_children = false;

		return true;
	}

	/* Attach DOM node to box */
	err = dom_node_set_user_data(ctx->n,
			corestring_dom___ns_key_box_node_data, box, NULL, 
			(void *) &old_box);
	if (err != DOM_NO_ERR)
		return false;

	/* Attach box to DOM node */
	box->node = dom_node_ref(ctx->n);

	if (props.inline_container == NULL &&
			(box->type == BOX_INLINE ||
			 box->type == BOX_BR ||
			 box->type == BOX_INLINE_BLOCK ||
			 css_computed_float(box->style) == CSS_FLOAT_LEFT ||
			 css_computed_float(box->style) == CSS_FLOAT_RIGHT) &&
			props.node_is_root == false) {
		/* Found an inline child of a block without a current container
		 * (i.e. this box is the first child of its parent, or was
		 * preceded by block-level siblings) */
		assert(props.containing_block != NULL && 
				"Box must have containing block.");

		props.inline_container = box_create(NULL, NULL, false, NULL, 
				NULL, NULL, NULL, ctx->bctx);
		if (props.inline_container == NULL)
			return false;

		props.inline_container->type = BOX_INLINE_CONTAINER;

		box_add_child(props.containing_block, props.inline_container);
	}

	/* Kick off fetch for any background image */
	if (css_computed_background_image(box->style, &bgimage_uri) == 
			CSS_BACKGROUND_IMAGE_IMAGE && bgimage_uri != NULL &&
	    nsoption_bool(background_images) == true) {
		nsurl *url;
		nserror error;

		/* TODO: we get a url out of libcss as a lwc string, but
		 *       earlier we already had it as a nsurl after we
		 *       nsurl_joined it.  Can this be improved?
		 *       For now, just making another nsurl. */
		error = nsurl_create(lwc_string_data(bgimage_uri), &url);
		if (error != NSERROR_OK)
			return false;

		if (html_fetch_object(ctx->content, url, box, image_types,
				ctx->content->base.available_width, 1000,
				true) == false) {
			nsurl_unref(url);
			return false;
		}
		nsurl_unref(url);
	}

	if (*convert_children)
		box->flags |= CONVERT_CHILDREN;

	if (box->type == BOX_INLINE || box->type == BOX_BR || 
			box->type == BOX_INLINE_BLOCK) {
		/* Inline container must exist, as we'll have 
		 * created it above if it didn't */
		assert(props.inline_container != NULL);

		box_add_child(props.inline_container, box);
	} else {
		if (css_computed_display(box->style, props.node_is_root) ==
				CSS_DISPLAY_LIST_ITEM) {
			/* List item: compute marker */
			if (box_construct_marker(box, props.title, ctx,
					props.containing_block) == false)
				return false;
		}

		if (props.node_is_root == false &&
				(css_computed_float(box->style) ==
				CSS_FLOAT_LEFT ||
				css_computed_float(box->style) == 
				CSS_FLOAT_RIGHT)) {
			/* Float: insert a float between the parent and box. */
			struct box *flt = box_create(NULL, NULL, false,
					props.href, props.target, props.title, 
					NULL, ctx->bctx);
			if (flt == NULL)
				return false;

			if (css_computed_float(box->style) == CSS_FLOAT_LEFT)
				flt->type = BOX_FLOAT_LEFT;
			else
				flt->type = BOX_FLOAT_RIGHT;

			box_add_child(props.inline_container, flt);
			box_add_child(flt, box);
		} else {
			/* Non-floated block-level box: add to containing block
			 * if there is one. If we're the root box, then there
			 * won't be. */
			if (props.containing_block != NULL)
				box_add_child(props.containing_block, box);
		}
	}

	return true;
}

/**
 * Complete construction of the box tree for an element.
 *
 * \param n        DOM node to construct for
 * \param content  Containing document
 *
 * This will be called after all children of an element have been processed
 */
void box_construct_element_after(dom_node *n, html_content *content)
{
	struct box_construct_props props;
	struct box *box = box_for_node(n);

	assert(box != NULL);

	box_extract_properties(n, &props);

	if (box->type == BOX_INLINE || box->type == BOX_BR) {
		/* Insert INLINE_END into containing block */
		struct box *inline_end;
		bool has_children;
		dom_exception err;

		err = dom_node_has_child_nodes(n, &has_children);
		if (err != DOM_NO_ERR)
			return;

		if (has_children == false || 
				(box->flags & CONVERT_CHILDREN) == 0) {
			/* No children, or didn't want children converted */
			return;
		}

		if (props.inline_container == NULL) {
			/* Create inline container if we don't have one */
			props.inline_container = box_create(NULL, NULL, false, 
					NULL, NULL, NULL, NULL, content->bctx);
			if (props.inline_container == NULL)
				return;

			props.inline_container->type = BOX_INLINE_CONTAINER;

			box_add_child(props.containing_block, 
					props.inline_container);
		}

		inline_end = box_create(NULL, box->style, false,
				box->href, box->target, box->title, 
				box->id == NULL ? NULL :
				lwc_string_ref(box->id), content->bctx);
		if (inline_end != NULL) {
			inline_end->type = BOX_INLINE_END;

			assert(props.inline_container != NULL);

			box_add_child(props.inline_container, inline_end);

			box->inline_end = inline_end;
			inline_end->inline_end = box;
		}
	} else {
		/* Handle the :after pseudo element */
		box_construct_generate(n, content, box,
				box->styles->styles[CSS_PSEUDO_ELEMENT_AFTER]);
	}
}

/**
 * Construct the box tree for an XML text node.
 *
 * \param  ctx  Tree construction context
 * \return  true on success, false on memory exhaustion
 */

bool box_construct_text(struct box_construct_ctx *ctx)
{
	struct box_construct_props props;
	struct box *box = NULL;
	dom_string *content;
	dom_exception err;

	assert(ctx->n != NULL);

	box_extract_properties(ctx->n, &props);

	assert(props.containing_block != NULL);

	err = dom_characterdata_get_data(ctx->n, &content);
	if (err != DOM_NO_ERR || content == NULL)
		return false;

	if (css_computed_white_space(props.parent_style) == 
			CSS_WHITE_SPACE_NORMAL ||
			css_computed_white_space(props.parent_style) == 
			CSS_WHITE_SPACE_NOWRAP) {
		char *text;

		text = squash_whitespace(dom_string_data(content));

		dom_string_unref(content);

		if (text == NULL)
			return false;

		/* if the text is just a space, combine it with the preceding
		 * text node, if any */
		if (text[0] == ' ' && text[1] == 0) {
			if (props.inline_container != NULL) {
				assert(props.inline_container->last != NULL);

				props.inline_container->last->space =
						UNKNOWN_WIDTH;
			}

			free(text);

			return true;
		}

		if (props.inline_container == NULL) {
			/* Child of a block without a current container
			 * (i.e. this box is the first child of its parent, or 
			 * was preceded by block-level siblings) */
			props.inline_container = box_create(NULL, NULL, false, 
					NULL, NULL, NULL, NULL, ctx->bctx);
			if (props.inline_container == NULL) {
				free(text);
				return false;
			}

			props.inline_container->type = BOX_INLINE_CONTAINER;

			box_add_child(props.containing_block, 
					props.inline_container);
		}

		/** \todo Dropping const here is not clever */ 
		box = box_create(NULL, 
				(css_computed_style *) props.parent_style,
				false, props.href, props.target, props.title, 
				NULL, ctx->bctx);
		if (box == NULL) {
			free(text);
			return false;
		}

		box->type = BOX_TEXT;

		box->text = talloc_strdup(ctx->bctx, text);
		free(text);
		if (box->text == NULL)
			return false;

		box->length = strlen(box->text);

		/* strip ending space char off */
		if (box->length > 1 && box->text[box->length - 1] == ' ') {
			box->space = UNKNOWN_WIDTH;
			box->length--;
		}

		if (css_computed_text_transform(props.parent_style) != 
				CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(box->text, box->length,
				css_computed_text_transform(
					props.parent_style));

		box_add_child(props.inline_container, box);

		if (box->text[0] == ' ') {
			box->length--;

			memmove(box->text, &box->text[1], box->length);

			if (box->prev != NULL)
				box->prev->space = UNKNOWN_WIDTH;
		}
	} else {
		/* white-space: pre */
		char *text;
		size_t text_len = dom_string_byte_length(content);
		size_t i;
		char *current;
		enum css_white_space_e white_space =
				css_computed_white_space(props.parent_style);

		/* note: pre-wrap/pre-line are unimplemented */
		assert(white_space == CSS_WHITE_SPACE_PRE ||
				white_space == CSS_WHITE_SPACE_PRE_LINE ||
				white_space == CSS_WHITE_SPACE_PRE_WRAP);

		text = malloc(text_len + 1);
		dom_string_unref(content);

		if (text == NULL)
			return false;

		memcpy(text, dom_string_data(content), text_len);
		text[text_len] = '\0';

		/* TODO: Handle tabs properly */
		for (i = 0; i < text_len; i++)
			if (text[i] == '\t')
				text[i] = ' ';

		if (css_computed_text_transform(props.parent_style) != 
				CSS_TEXT_TRANSFORM_NONE)
			box_text_transform(text, strlen(text),
				css_computed_text_transform(
						props.parent_style));

		current = text;

		/* swallow a single leading new line */
		if (props.containing_block->flags & PRE_STRIP) {
			switch (*current) {
			case '\n':
				current++;
				break;
			case '\r':
				current++;
				if (*current == '\n')
					current++;
				break;
			}
			props.containing_block->flags &= ~PRE_STRIP;
		}

		do {
			size_t len = strcspn(current, "\r\n");

			char old = current[len];

			current[len] = 0;

			if (props.inline_container == NULL) {
				/* Child of a block without a current container
				 * (i.e. this box is the first child of its 
				 * parent, or was preceded by block-level 
				 * siblings) */
				props.inline_container = box_create(NULL, NULL,
						false, NULL, NULL, NULL, NULL, 
						ctx->bctx);
				if (props.inline_container == NULL) {
					free(text);
					return false;
				}

				props.inline_container->type = 
						BOX_INLINE_CONTAINER;

				box_add_child(props.containing_block, 
						props.inline_container);
			}

			/** \todo Dropping const isn't clever */
			box = box_create(NULL,
				(css_computed_style *) props.parent_style,
				false, props.href, props.target, props.title, 
				NULL, ctx->bctx);
			if (box == NULL) {
				free(text);
				return false;
			}

			box->type = BOX_TEXT;

			box->text = talloc_strdup(ctx->bctx, current);
			if (box->text == NULL) {
				free(text);
				return false;
			}

			box->length = strlen(box->text);

			box_add_child(props.inline_container, box);

			current[len] = old;

			current += len;

			if (current[0] != '\0') {
				/* Linebreak: create new inline container */
				props.inline_container = box_create(NULL, NULL,
						false, NULL, NULL, NULL, NULL, 
						ctx->bctx);
				if (props.inline_container == NULL) {
					free(text);
					return false;
				}

				props.inline_container->type = 
						BOX_INLINE_CONTAINER;

				box_add_child(props.containing_block, 
						props.inline_container);

				if (current[0] == '\r' && current[1] == '\n')
					current += 2;
				else
					current++;
			}
		} while (*current);

		free(text);
	}

	return true;
}

/**
 * Get the style for an element.
 *
 * \param  c		   content of type CONTENT_HTML that is being processed
 * \param  parent_style    style at this point in xml tree, or NULL for root
 * \param  n		   node in xml tree
 * \return  the new style, or NULL on memory exhaustion
 */
css_select_results *box_get_style(html_content *c,
		const css_computed_style *parent_style, dom_node *n)
{
	dom_string *s;
	dom_exception err;
	css_stylesheet *inline_style = NULL;
	css_select_results *styles;
	nscss_select_ctx ctx;

	/* Firstly, construct inline stylesheet, if any */
	err = dom_element_get_attribute(n, corestring_dom_style, &s);
	if (err != DOM_NO_ERR)
		return NULL;

	if (s != NULL) {
		inline_style = nscss_create_inline_style(
				(const uint8_t *) dom_string_data(s),
				dom_string_byte_length(s),
				c->encoding,
				nsurl_access(content_get_url(&c->base)), 
				c->quirks != DOM_DOCUMENT_QUIRKS_MODE_NONE);

		dom_string_unref(s);

		if (inline_style == NULL)
			return NULL;
	}

	/* Populate selection context */
	ctx.ctx = c->select_ctx;
	ctx.quirks = (c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
	ctx.base_url = c->base_url;
	ctx.universal = c->universal;
	ctx.parent_style = parent_style;

	/* Select style for element */
	styles = nscss_get_style(&ctx, n, CSS_MEDIA_SCREEN, inline_style);

	/* No longer need inline style */
	if (inline_style != NULL)
		css_stylesheet_destroy(inline_style);

	return styles;
}


/**
 * Apply the CSS text-transform property to given text for its ASCII chars.
 *
 * \param  s	string to transform
 * \param  len  length of s
 * \param  tt	transform type
 */

void box_text_transform(char *s, unsigned int len, enum css_text_transform_e tt)
{
	unsigned int i;
	if (len == 0)
		return;
	switch (tt) {
		case CSS_TEXT_TRANSFORM_UPPERCASE:
			for (i = 0; i < len; ++i)
				if ((unsigned char) s[i] < 0x80)
					s[i] = ls_toupper(s[i]);
			break;
		case CSS_TEXT_TRANSFORM_LOWERCASE:
			for (i = 0; i < len; ++i)
				if ((unsigned char) s[i] < 0x80)
					s[i] = ls_tolower(s[i]);
			break;
		case CSS_TEXT_TRANSFORM_CAPITALIZE:
			if ((unsigned char) s[0] < 0x80)
				s[0] = ls_toupper(s[0]);
			for (i = 1; i < len; ++i)
				if ((unsigned char) s[i] < 0x80 &&
						ls_isspace(s[i - 1]))
					s[i] = ls_toupper(s[i]);
			break;
		default:
			break;
	}
}


/**
 * \name  Special case element handlers
 *
 * These functions are called by box_construct_element() when an element is
 * being converted, according to the entries in element_table.
 *
 * The parameters are the xmlNode, the content for the document, and a partly
 * filled in box structure for the element.
 *
 * Return true on success, false on memory exhaustion. Set *convert_children
 * to false if children of this element in the XML tree should be skipped (for
 * example, if they have been processed in some special way already).
 *
 * Elements ordered as in the HTML 4.01 specification. Section numbers in
 * brackets [] refer to the spec.
 *
 * \{
 */

/**
 * Document body [7.5.1].
 */

bool box_body(BOX_SPECIAL_PARAMS)
{
	css_color color;

	css_computed_background_color(box->style, &color);
	if (nscss_color_is_transparent(color))
		content->background_colour = NS_TRANSPARENT;
	else
		content->background_colour = nscss_color_to_ns(color);

	return true;
}


/**
 * Forced line break [9.3.2].
 */

bool box_br(BOX_SPECIAL_PARAMS)
{
	box->type = BOX_BR;
	return true;
}

/**
 * Preformatted text [9.3.4].
 */

bool box_pre(BOX_SPECIAL_PARAMS)
{
	box->flags |= PRE_STRIP;
	return true;
}

/**
 * Anchor [12.2].
 */

bool box_a(BOX_SPECIAL_PARAMS)
{
	bool ok;
	nsurl *url;
	dom_string *s;
	dom_exception err;

	err = dom_element_get_attribute(n, corestring_dom_href, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		ok = box_extract_link(dom_string_data(s),
				content->base_url, &url);
		dom_string_unref(s);
		if (!ok)
			return false;
		if (url) {
			if (box->href != NULL)
				nsurl_unref(box->href);
			box->href = url;
		}
	}

	/* name and id share the same namespace */
	err = dom_element_get_attribute(n, corestring_dom_name, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		lwc_string *lwc_name;

		err = dom_string_intern(s, &lwc_name);

		dom_string_unref(s);

		if (err == DOM_NO_ERR) {
			/* name replaces existing id
			 * TODO: really? */
			if (box->id != NULL)
				lwc_string_unref(box->id);

			box->id = lwc_name;
		}
	}

	/* target frame [16.3] */
	err = dom_element_get_attribute(n, corestring_dom_target, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc__blank))
			box->target = TARGET_BLANK;
		else if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc__top))
			box->target = TARGET_TOP;
		else if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc__parent))
			box->target = TARGET_PARENT;
		else if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc__self))
			/* the default may have been overridden by a
			 * <base target=...>, so this is different to 0 */
			box->target = TARGET_SELF;
		else {
			/* 6.16 says that frame names must begin with [a-zA-Z]
			 * This doesn't match reality, so just take anything */
			box->target = talloc_strdup(content->bctx, 
					dom_string_data(s));
			if (!box->target) {
				dom_string_unref(s);
				return false;
			}
		}
		dom_string_unref(s);
	}

	return true;
}


/**
 * Embedded image [13.2].
 */

bool box_image(BOX_SPECIAL_PARAMS)
{
	bool ok;
	dom_string *s;
	dom_exception err;
	nsurl *url;
	enum css_width_e wtype;
	enum css_height_e htype;
	css_fixed value = 0;
	css_unit wunit = CSS_UNIT_PX;
	css_unit hunit = CSS_UNIT_PX;

	if (box->style && css_computed_display(box->style, 
			box_is_root(n)) == CSS_DISPLAY_NONE)
		return true;

	/* handle alt text */
	err = dom_element_get_attribute(n, corestring_dom_alt, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		char *alt = squash_whitespace(dom_string_data(s));
		dom_string_unref(s);
		if (alt == NULL)
			return false;
		box->text = talloc_strdup(content->bctx, alt);
		free(alt);
		if (box->text == NULL)
			return false;
		box->length = strlen(box->text);
	}

	if (nsoption_bool(foreground_images) == false) {
		return true;
	}

	/* imagemap associated with this image */
	if (!box_get_attribute(n, "usemap", content->bctx, &box->usemap))
		return false;
	if (box->usemap && box->usemap[0] == '#')
		box->usemap++;

	/* get image URL */
	err = dom_element_get_attribute(n, corestring_dom_src, &s);
	if (err != DOM_NO_ERR || s == NULL)
		return true;

	if (box_extract_link(dom_string_data(s), content->base_url, 
			&url) == false) {
		dom_string_unref(s);
		return false;
	}

	dom_string_unref(s);

	if (url == NULL)
		return true;

	/* start fetch */
	ok = html_fetch_object(content, url, box, image_types,
			content->base.available_width, 1000, false);
	nsurl_unref(url);

	wtype = css_computed_width(box->style, &value, &wunit);
	htype = css_computed_height(box->style, &value, &hunit);

	if (wtype == CSS_WIDTH_SET && wunit != CSS_UNIT_PCT &&
			htype == CSS_HEIGHT_SET && hunit != CSS_UNIT_PCT) {
		/* We know the dimensions the image will be shown at before it's
		 * fetched. */
		box->flags |= REPLACE_DIM;
	}

	return ok;
}


/**
 * Noscript element
 */

bool box_noscript(BOX_SPECIAL_PARAMS)
{
	/* If scripting is enabled, do not display the contents of noscript */
	if (nsoption_bool(enable_javascript))
		*convert_children = false;

	return true;
}


/**
 * Destructor for object_params, for <object> elements
 *
 * \param b	The object params being destroyed.
 * \return 0 to allow talloc to continue destroying the tree.
 */
static int box_object_talloc_destructor(struct object_params *o)
{
	if (o->codebase != NULL)
		nsurl_unref(o->codebase);
	if (o->classid != NULL)
		nsurl_unref(o->classid);
	if (o->data != NULL)
		nsurl_unref(o->data);
	
	return 0;
}

/**
 * Generic embedded object [13.3].
 */

bool box_object(BOX_SPECIAL_PARAMS)
{
	struct object_params *params;
	struct object_param *param;
	dom_string *codebase, *classid, *data;
	dom_node *c;
	dom_exception err;

	if (box->style && css_computed_display(box->style, 
			box_is_root(n)) == CSS_DISPLAY_NONE)
		return true;

	if (box_get_attribute(n, "usemap", content->bctx, &box->usemap) ==
			false)
		return false;
	if (box->usemap && box->usemap[0] == '#')
		box->usemap++;

	params = talloc(content->bctx, struct object_params);
	if (params == NULL)
		return false;

	talloc_set_destructor(params, box_object_talloc_destructor);

	params->data = NULL;
	params->type = NULL;
	params->codetype = NULL;
	params->codebase = NULL;
	params->classid = NULL;
	params->params = NULL;

	/* codebase, classid, and data are URLs
	 * (codebase is the base for the other two) */
	err = dom_element_get_attribute(n, corestring_dom_codebase, &codebase);
	if (err == DOM_NO_ERR && codebase != NULL) {
		if (box_extract_link(dom_string_data(codebase), 
				content->base_url, 
				&params->codebase) == false) {
			dom_string_unref(codebase);
			return false;
		}
		dom_string_unref(codebase);
	}
	if (params->codebase == NULL)
		params->codebase = nsurl_ref(content->base_url);

	err = dom_element_get_attribute(n, corestring_dom_classid, &classid);
	if (err == DOM_NO_ERR && classid != NULL) {
		if (box_extract_link(dom_string_data(classid), params->codebase,
				&params->classid) == false) {
			dom_string_unref(classid);
			return false;
		}
		dom_string_unref(classid);
	}

	err = dom_element_get_attribute(n, corestring_dom_data, &data);
	if (err == DOM_NO_ERR && data != NULL) {
		if (box_extract_link(dom_string_data(data), params->codebase,
				&params->data) == false) {
			dom_string_unref(data);
			return false;
		}
		dom_string_unref(data);
	}

	if (params->classid == NULL && params->data == NULL)
		/* nothing to embed; ignore */
		return true;

	/* Don't include ourself */
	if (params->classid != NULL && nsurl_compare(content->base_url,
			params->classid, NSURL_COMPLETE))
		return true;

	if (params->data != NULL && nsurl_compare(content->base_url,
			params->data, NSURL_COMPLETE))
		return true;

	/* codetype and type are MIME types */
	if (box_get_attribute(n, "codetype", params, 
			&params->codetype) == false)
		return false;
	if (box_get_attribute(n, "type", params, &params->type) == false)
		return false;

	/* classid && !data => classid is used (consult codetype)
	 * (classid || !classid) && data => data is used (consult type)
	 * !classid && !data => invalid; ignored */

	if (params->classid != NULL && params->data == NULL && 
			params->codetype != NULL) {
		lwc_string *icodetype;
		lwc_error lerror;

		lerror = lwc_intern_string(params->codetype, 
				strlen(params->codetype), &icodetype);
		if (lerror != lwc_error_ok)
			return false;

		if (content_factory_type_from_mime_type(icodetype) ==
				CONTENT_NONE) {
			/* can't handle this MIME type */
			lwc_string_unref(icodetype);
			return true;
		}

		lwc_string_unref(icodetype);
	}

	if (params->data != NULL && params->type != NULL) {
		lwc_string *itype;
		lwc_error lerror;

		lerror = lwc_intern_string(params->type, strlen(params->type),
				&itype);
		if (lerror != lwc_error_ok)
			return false;

		if (content_factory_type_from_mime_type(itype) == 
				CONTENT_NONE) {
			/* can't handle this MIME type */
			lwc_string_unref(itype);
			return true;
		}

		lwc_string_unref(itype);
	}

	/* add parameters to linked list */
	err = dom_node_get_first_child(n, &c);
	if (err != DOM_NO_ERR)
		return false;

	while (c != NULL) {
		dom_node *next;
		dom_node_type type;

		err = dom_node_get_node_type(c, &type);
		if (err != DOM_NO_ERR) {
			dom_node_unref(c);
			return false;
		}

		if (type == DOM_ELEMENT_NODE) {
			dom_string *name;

			err = dom_node_get_node_name(c, &name);
			if (err != DOM_NO_ERR) {
				dom_node_unref(c);
				return false;
			}

			if (!dom_string_caseless_lwc_isequal(name,
					corestring_lwc_param)) {
				/* The first non-param child is the start of 
				 * the alt html. Therefore, we should break 
				 * out of this loop. */
				dom_node_unref(c);
				break;
			}

			param = talloc(params, struct object_param);
			if (param == NULL) {
				dom_node_unref(c);
				return false;
			}
			param->name = NULL;
			param->value = NULL;
			param->type = NULL;
			param->valuetype = NULL;
			param->next = NULL;

			if (box_get_attribute(c, "name", param, 
					&param->name) == false) {
				dom_node_unref(c);
				return false;
			}

			if (box_get_attribute(c, "value", param, 
					&param->value) == false) {
				dom_node_unref(c);
				return false;
			}

			if (box_get_attribute(c, "type", param, 
					&param->type) == false) {
				dom_node_unref(c);
				return false;
			}

			if (box_get_attribute(c, "valuetype", param,
					&param->valuetype) == false) {
				dom_node_unref(c);
				return false;
			}

			if (param->valuetype == NULL) {
				param->valuetype = talloc_strdup(param, "data");
				if (param->valuetype == NULL) {
					dom_node_unref(c);
					return false;
				}
			}

			param->next = params->params;
			params->params = param;
		}

		err = dom_node_get_next_sibling(c, &next);
		if (err != DOM_NO_ERR) {
			dom_node_unref(c);
			return false;
		}

		dom_node_unref(c);
		c = next;
	}

	box->object_params = params;

	/* start fetch (MIME type is ok or not specified) */
	if (!html_fetch_object(content,
			params->data ? params->data : params->classid,
			box, CONTENT_ANY, content->base.available_width, 1000,
			false))
		return false;

	*convert_children = false;
	return true;
}


/**
 * Window subdivision [16.2.1].
 */

bool box_frameset(BOX_SPECIAL_PARAMS)
{
	bool ok;

	if (content->frameset) {
		LOG(("Error: multiple framesets in document."));
		/* Don't convert children */
		if (convert_children)
			*convert_children = false;
		/* And ignore this spurious frameset */
		box->type = BOX_NONE;
		return true;
	}

	content->frameset = talloc_zero(content->bctx, struct content_html_frames);
	if (!content->frameset)
		return false;

	ok = box_create_frameset(content->frameset, n, content);
	if (ok)
		box->type = BOX_NONE;

	if (convert_children)
		*convert_children = false;
	return ok;
}


/**
 * Destructor for content_html_frames, for <frame> elements
 *
 * \param b	The frame params being destroyed.
 * \return 0 to allow talloc to continue destroying the tree.
 */
static int box_frames_talloc_destructor(struct content_html_frames *f)
{
	if (f->url != NULL) {
		nsurl_unref(f->url);
		f->url = NULL;
	}
	
	return 0;
}

bool box_create_frameset(struct content_html_frames *f, dom_node *n,
		html_content *content) {
	unsigned int row, col, index, i;
	unsigned int rows = 1, cols = 1;
	dom_string *s;
	dom_exception err;
	nsurl *url;
	struct frame_dimension *row_height = 0, *col_width = 0;
	dom_node *c, *next;
	struct content_html_frames *frame;
	bool default_border = true;
	colour default_border_colour = 0x000000;

	/* parse rows and columns */
	err = dom_element_get_attribute(n, corestring_dom_rows, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		row_height = box_parse_multi_lengths(dom_string_data(s), &rows);
		dom_string_unref(s);
		if (row_height == NULL)
			return false;
	} else {
		row_height = calloc(1, sizeof(struct frame_dimension));
		if (row_height == NULL)
			return false;
		row_height->value = 100;
		row_height->unit = FRAME_DIMENSION_PERCENT;
	}

	err = dom_element_get_attribute(n, corestring_dom_cols, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		col_width = box_parse_multi_lengths(dom_string_data(s), &cols);
		dom_string_unref(s);
		if (col_width == NULL) {
			free(row_height);
			return false;
		}
	} else {
		col_width = calloc(1, sizeof(struct frame_dimension));
		if (col_width == NULL) {
			free(row_height);
			return false;
		}
		col_width->value = 100;
		col_width->unit = FRAME_DIMENSION_PERCENT;
	}

	/* common extension: border="0|1" to control all children */
	err = dom_element_get_attribute(n, corestring_dom_border, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		if ((dom_string_data(s)[0] == '0') && 
				(dom_string_data(s)[1] == '\0'))
			default_border = false;
		dom_string_unref(s);
	}

	/* common extension: frameborder="yes|no" to control all children */
	err = dom_element_get_attribute(n, corestring_dom_frameborder, &s);
	if (err == DOM_NO_ERR && s != NULL) {
	  	if (dom_string_caseless_lwc_isequal(s,
	  			corestring_lwc_no) == 0)
	  		default_border = false;
		dom_string_unref(s);
	}

	/* common extension: bordercolor="#RRGGBB|<named colour>" to control
	 *all children */
	err = dom_element_get_attribute(n, corestring_dom_bordercolor, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		css_color color;

		if (nscss_parse_colour(dom_string_data(s), &color))
			default_border_colour = nscss_color_to_ns(color);

		dom_string_unref(s);
	}

	/* update frameset and create default children */
	f->cols = cols;
	f->rows = rows;
	f->scrolling = SCROLLING_NO;
	f->children = talloc_array(content->bctx, struct content_html_frames,
								(rows * cols));

	talloc_set_destructor(f->children, box_frames_talloc_destructor);

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			index = (row * cols) + col;
			frame = &f->children[index];
			frame->cols = 0;
			frame->rows = 0;
			frame->width = col_width[col];
			frame->height = row_height[row];
			frame->margin_width = 0;
			frame->margin_height = 0;
			frame->name = NULL;
			frame->url = NULL;
			frame->no_resize = false;
			frame->scrolling = SCROLLING_AUTO;
			frame->border = default_border;
			frame->border_colour = default_border_colour;
			frame->children = NULL;
		}
	}
	free(col_width);
	free(row_height);

	/* create the frameset windows */
	err = dom_node_get_first_child(n, &c);
	if (err != DOM_NO_ERR)
		return false;

	for (row = 0; c != NULL && row < rows; row++) {
		for (col = 0; c != NULL && col < cols; col++) {
			while (c != NULL) {
				dom_node_type type;
				dom_string *name;

				err = dom_node_get_node_type(c, &type);
				if (err != DOM_NO_ERR) {
					dom_node_unref(c);
					return false;
				}

				err = dom_node_get_node_name(c, &name);
				if (err != DOM_NO_ERR) {
					dom_node_unref(c);
					return false;
				}

				if (type != DOM_ELEMENT_NODE ||
					(!dom_string_caseless_lwc_isequal(
							name,
							corestring_lwc_frame) &&
					!dom_string_caseless_lwc_isequal(
							name,
							corestring_lwc_frameset
							))) {
					err = dom_node_get_next_sibling(c, 
							&next);
					if (err != DOM_NO_ERR) {
						dom_string_unref(name);
						dom_node_unref(c);
						return false;
					}

					dom_string_unref(name);
					dom_node_unref(c);
					c = next;
				} else {
					/* Got a FRAME or FRAMESET element */
					dom_string_unref(name);
					break;
				}
			}

			if (c == NULL)
				break;

			/* get current frame */
			index = (row * cols) + col;
			frame = &f->children[index];

			/* nest framesets */
			err = dom_node_get_node_name(c, &s);
			if (err != DOM_NO_ERR) {
				dom_node_unref(c);
				return false;
			}

			if (dom_string_caseless_lwc_isequal(s,
					corestring_lwc_frameset)) {
				dom_string_unref(s);
				frame->border = 0;
				if (box_create_frameset(frame, c, 
						content) == false) {
					dom_node_unref(c);
					return false;
				}

				err = dom_node_get_next_sibling(c, &next);
				if (err != DOM_NO_ERR) {
					dom_node_unref(c);
					return false;
				}

				dom_node_unref(c);
				c = next;
				continue;
			}

			dom_string_unref(s);

			/* get frame URL (not required) */
			url = NULL;
			err = dom_element_get_attribute(c, corestring_dom_src, &s);
			if (err == DOM_NO_ERR && s != NULL) {
				box_extract_link(dom_string_data(s), 
						content->base_url, &url);
				dom_string_unref(s);
			}

			/* copy url */
			if (url != NULL) {
			  	/* no self-references */
			  	if (nsurl_compare(content->base_url, url,
						NSURL_COMPLETE) == false)
					frame->url = url;
				url = NULL;
			}

			/* fill in specified values */
			err = dom_element_get_attribute(c, corestring_dom_name, &s);
			if (err == DOM_NO_ERR && s != NULL) {
				frame->name = talloc_strdup(content->bctx, 
						dom_string_data(s));
				dom_string_unref(s);
			}

			dom_element_has_attribute(c, corestring_dom_noresize, 
					&frame->no_resize);

			err = dom_element_get_attribute(c, corestring_dom_frameborder, 
					&s);
			if (err == DOM_NO_ERR && s != NULL) {
				i = atoi(dom_string_data(s));
				frame->border = (i != 0);
				dom_string_unref(s);
			}

			err = dom_element_get_attribute(c, corestring_dom_scrolling, &s);
			if (err == DOM_NO_ERR && s != NULL) {
				if (dom_string_caseless_lwc_isequal(s,
						corestring_lwc_yes))
					frame->scrolling = SCROLLING_YES;
				else if (dom_string_caseless_lwc_isequal(s,
						corestring_lwc_no))
					frame->scrolling = SCROLLING_NO;
				dom_string_unref(s);
			}

			err = dom_element_get_attribute(c, corestring_dom_marginwidth, 
					&s);
			if (err == DOM_NO_ERR && s != NULL) {
				frame->margin_width = atoi(dom_string_data(s));
				dom_string_unref(s);
			}

			err = dom_element_get_attribute(c, corestring_dom_marginheight,
					&s);
			if (err == DOM_NO_ERR && s != NULL) {
				frame->margin_height = atoi(dom_string_data(s));
				dom_string_unref(s);
			}

			err = dom_element_get_attribute(c, corestring_dom_bordercolor, 
					&s);
			if (err == DOM_NO_ERR && s != NULL) {
				css_color color;

				if (nscss_parse_colour(dom_string_data(s), 
						&color))
					frame->border_colour =
						nscss_color_to_ns(color);

				dom_string_unref(s);
			}

			/* advance */
			err = dom_node_get_next_sibling(c, &next);
			if (err != DOM_NO_ERR) {
				dom_node_unref(c);
				return false;
			}

			dom_node_unref(c);
			c = next;
		}
	}

	return true;
}


/**
 * Destructor for content_html_iframe, for <iframe> elements
 *
 * \param b	The iframe params being destroyed.
 * \return 0 to allow talloc to continue destroying the tree.
 */
static int box_iframes_talloc_destructor(struct content_html_iframe *f)
{
	if (f->url != NULL) {
		nsurl_unref(f->url);
		f->url = NULL;
	}
	
	return 0;
}


/**
 * Inline subwindow [16.5].
 */

bool box_iframe(BOX_SPECIAL_PARAMS)
{
	nsurl *url;
	dom_string *s;
	dom_exception err;
	struct content_html_iframe *iframe;
	int i;

	if (box->style && css_computed_display(box->style, 
			box_is_root(n)) == CSS_DISPLAY_NONE)
		return true;

	if (box->style && css_computed_visibility(box->style) ==
			CSS_VISIBILITY_HIDDEN)
		/* Don't create iframe discriptors for invisible iframes
		 * TODO: handle hidden iframes at browser_window generation
		 * time instead? */
		return true;

	/* get frame URL */
	err = dom_element_get_attribute(n, corestring_dom_src, &s);
	if (err != DOM_NO_ERR || s == NULL)
		return true;
	if (box_extract_link(dom_string_data(s), content->base_url, 
			&url) == false) {
		dom_string_unref(s);
		return false;
	}
	dom_string_unref(s);
	if (url == NULL)
		return true;

	/* don't include ourself */
	if (nsurl_compare(content->base_url, url, NSURL_COMPLETE)) {
		nsurl_unref(url);
		return true;
	}

	/* create a new iframe */
	iframe = talloc(content->bctx, struct content_html_iframe);
	if (iframe == NULL) {
		nsurl_unref(url);
		return false;
	}

	talloc_set_destructor(iframe, box_iframes_talloc_destructor);

	iframe->box = box;
	iframe->margin_width = 0;
	iframe->margin_height = 0;
	iframe->name = NULL;
	iframe->url = url;
	iframe->scrolling = SCROLLING_AUTO;
	iframe->border = true;

	/* Add this iframe to the linked list of iframes */
	iframe->next = content->iframe;
	content->iframe = iframe;

	/* fill in specified values */
	err = dom_element_get_attribute(n, corestring_dom_name, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		iframe->name = talloc_strdup(content->bctx, dom_string_data(s));
		dom_string_unref(s);
	}

	err = dom_element_get_attribute(n, corestring_dom_frameborder, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		i = atoi(dom_string_data(s));
		iframe->border = (i != 0);
		dom_string_unref(s);
	}

	err = dom_element_get_attribute(n, corestring_dom_bordercolor, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		css_color color;

		if (nscss_parse_colour(dom_string_data(s), &color))
			iframe->border_colour = nscss_color_to_ns(color);

		dom_string_unref(s);
	}

	err = dom_element_get_attribute(n, corestring_dom_scrolling, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc_yes))
			iframe->scrolling = SCROLLING_YES;
		else if (dom_string_caseless_lwc_isequal(s,
				corestring_lwc_no))
			iframe->scrolling = SCROLLING_NO;
		dom_string_unref(s);
	}

	err = dom_element_get_attribute(n, corestring_dom_marginwidth, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		iframe->margin_width = atoi(dom_string_data(s));
		dom_string_unref(s);
	}

	err = dom_element_get_attribute(n, corestring_dom_marginheight, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		iframe->margin_height = atoi(dom_string_data(s));
		dom_string_unref(s);
	}

	/* box */
	assert(box->style);
	box->flags |= IFRAME;

	/* Showing iframe, so don't show alternate content */
	if (convert_children)
		*convert_children = false;
	return true;
}


/**
 * Helper function for adding textarea widget to box.
 *
 * This is a load of hacks to ensure boxes replaced with textareas
 * can be handled by the layout code.
 */

static bool box_input_text(html_content *html, struct box *box,
		struct dom_node *node)
{
	struct box *inline_container, *inline_box;

	box->type = BOX_INLINE_BLOCK;

	inline_container = box_create(NULL, 0, false, 0, 0, 0, 0, html->bctx);
	if (!inline_container)
		return false;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(NULL, box->style, false, 0, 0, box->title, 0,
			html->bctx);
	if (!inline_box)
		return false;
	inline_box->type = BOX_TEXT;
	inline_box->text = talloc_strdup(html->bctx, "");

	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	return box_textarea_create_textarea(html, box, node);
}


/**
 * Form control [17.4].
 */

bool box_input(BOX_SPECIAL_PARAMS)
{
	struct form_control *gadget = NULL;
	dom_string *type = NULL;
	dom_exception err;
	nsurl *url;
	nserror error;

	dom_element_get_attribute(n, corestring_dom_type, &type);

	gadget = html_forms_get_control_for_node(content->forms, n);
	if (gadget == NULL)
		goto no_memory;
	box->gadget = gadget;
	gadget->box = box;
	gadget->html = content;

	if (type && dom_string_caseless_lwc_isequal(type,
			corestring_lwc_password)) {
		if (box_input_text(content, box, n) == false)
			goto no_memory;

	} else if (type && dom_string_caseless_lwc_isequal(type,
			corestring_lwc_file)) {
		box->type = BOX_INLINE_BLOCK;

	} else if (type && dom_string_caseless_lwc_isequal(type,
			corestring_lwc_hidden)) {
		/* no box for hidden inputs */
		box->type = BOX_NONE;

	} else if (type && 
			(dom_string_caseless_lwc_isequal(type,
				corestring_lwc_checkbox) ||
			dom_string_caseless_lwc_isequal(type,
				corestring_lwc_radio))) {

	} else if (type && 
			(dom_string_caseless_lwc_isequal(type,
				corestring_lwc_submit) ||
			dom_string_caseless_lwc_isequal(type,
				corestring_lwc_reset) ||
			dom_string_caseless_lwc_isequal(type,
				corestring_lwc_button))) {
		struct box *inline_container, *inline_box;

		if (box_button(n, content, box, 0) == false)
			goto no_memory;

		inline_container = box_create(NULL, 0, false, 0, 0, 0, 0,
				content->bctx);
		if (inline_container == NULL)
			goto no_memory;

		inline_container->type = BOX_INLINE_CONTAINER;

		inline_box = box_create(NULL, box->style, false, 0, 0,
				box->title, 0, content->bctx);
		if (inline_box == NULL)
			goto no_memory;

		inline_box->type = BOX_TEXT;

		if (box->gadget->value != NULL)
			inline_box->text = talloc_strdup(content->bctx,
					box->gadget->value);
		else if (box->gadget->type == GADGET_SUBMIT)
			inline_box->text = talloc_strdup(content->bctx,
					messages_get("Form_Submit"));
		else if (box->gadget->type == GADGET_RESET)
			inline_box->text = talloc_strdup(content->bctx,
					messages_get("Form_Reset"));
		else
			inline_box->text = talloc_strdup(content->bctx, 
							 "Button");

		if (inline_box->text == NULL)
			goto no_memory;

		inline_box->length = strlen(inline_box->text);

		box_add_child(inline_container, inline_box);

		box_add_child(box, inline_container);

	} else if (type && dom_string_caseless_lwc_isequal(type,
			corestring_lwc_image)) {
		gadget->type = GADGET_IMAGE;

		if (box->style && css_computed_display(box->style,
				box_is_root(n)) != CSS_DISPLAY_NONE &&
		    nsoption_bool(foreground_images) == true) {
			dom_string *s;

			err = dom_element_get_attribute(n, corestring_dom_src, &s);
			if (err == DOM_NO_ERR && s != NULL) {
				error = nsurl_join(content->base_url, 
						dom_string_data(s), &url);
				dom_string_unref(s);
				if (error != NSERROR_OK)
					goto no_memory;

				/* if url is equivalent to the parent's url,
				 * we've got infinite inclusion. stop it here
				 */
				if (nsurl_compare(url, content->base_url,
						NSURL_COMPLETE) == false) {
					if (!html_fetch_object(content, url,
							box, image_types,
							content->base.
							available_width,
							1000, false)) {
						nsurl_unref(url);
						goto no_memory;
					}
				}
				nsurl_unref(url);
			}
		}
	} else {
		/* the default type is "text" */
		if (box_input_text(content, box, n) == false)
			goto no_memory;
	}

	if (type)
		dom_string_unref(type);

	*convert_children = false;
	return true;

no_memory:
	if (type)
		dom_string_unref(type);

	return false;
}


/**
 * Push button [17.5].
 */

bool box_button(BOX_SPECIAL_PARAMS)
{
	struct form_control *gadget;

	gadget = html_forms_get_control_for_node(content->forms, n);
	if (!gadget)
		return false;

	gadget->html = content;
	box->gadget = gadget;
	gadget->box = box;

	box->type = BOX_INLINE_BLOCK;

	/* Just render the contents */

	return true;
}


/**
 * Option selector [17.6].
 */

bool box_select(BOX_SPECIAL_PARAMS)
{
	struct box *inline_container;
	struct box *inline_box;
	struct form_control *gadget;
	dom_node *c, *c2;
	dom_node *next, *next2;
	dom_exception err;

	gadget = html_forms_get_control_for_node(content->forms, n);
	if (gadget == NULL)
		return false;

	gadget->html = content;
	err = dom_node_get_first_child(n, &c);
	if (err != DOM_NO_ERR)
		return false;

	while (c != NULL) {
		dom_string *name;

		err = dom_node_get_node_name(c, &name);
		if (err != DOM_NO_ERR) {
			dom_node_unref(c);
			return false;
		}

		if (dom_string_caseless_lwc_isequal(name,
				corestring_lwc_option)) {
			dom_string_unref(name);

			if (box_select_add_option(gadget, c) == false) {
				dom_node_unref(c);
				goto no_memory;
			}
		} else if (dom_string_caseless_lwc_isequal(name,
				corestring_lwc_optgroup)) {
			dom_string_unref(name);

			err = dom_node_get_first_child(c, &c2);
			if (err != DOM_NO_ERR) {
				dom_node_unref(c);
				return false;
			}

			while (c2 != NULL) {
				dom_string *c2_name;

				err = dom_node_get_node_name(c2, &c2_name);
				if (err != DOM_NO_ERR) {
					dom_node_unref(c2);
					dom_node_unref(c);
					return false;
				}
				
				if (dom_string_caseless_lwc_isequal(c2_name,
						corestring_lwc_option)) {
					dom_string_unref(c2_name);

					if (box_select_add_option(gadget, 
							c2) == false) {
						dom_node_unref(c2);
						dom_node_unref(c);
						goto no_memory;
					}
				} else {
					dom_string_unref(c2_name);
				}

				err = dom_node_get_next_sibling(c2, &next2);
				if (err != DOM_NO_ERR) {
					dom_node_unref(c2);
					dom_node_unref(c);
					return false;
				}

				dom_node_unref(c2);
				c2 = next2;
			}
		} else {
			dom_string_unref(name);
		}
	
		err = dom_node_get_next_sibling(c, &next);
		if (err != DOM_NO_ERR) {
			dom_node_unref(c);
			return false;
		}

		dom_node_unref(c);
		c = next;
	}

	if (gadget->data.select.num_items == 0) {
		/* no options: ignore entire select */
		return true;
	}

	box->type = BOX_INLINE_BLOCK;
	box->gadget = gadget;
	gadget->box = box;

	inline_container = box_create(NULL, 0, false, 0, 0, 0, 0, content->bctx);
	if (inline_container == NULL)
		goto no_memory;
	inline_container->type = BOX_INLINE_CONTAINER;
	inline_box = box_create(NULL, box->style, false, 0, 0, box->title, 0,
			content->bctx);
	if (inline_box == NULL)
		goto no_memory;
	inline_box->type = BOX_TEXT;
	box_add_child(inline_container, inline_box);
	box_add_child(box, inline_container);

	if (gadget->data.select.multiple == false &&
			gadget->data.select.num_selected == 0) {
		gadget->data.select.current = gadget->data.select.items;
		gadget->data.select.current->initial_selected =
			gadget->data.select.current->selected = true;
		gadget->data.select.num_selected = 1;
		dom_html_option_element_set_selected(
				gadget->data.select.current->node, true);
	}

	if (gadget->data.select.num_selected == 0)
		inline_box->text = talloc_strdup(content->bctx,
				messages_get("Form_None"));
	else if (gadget->data.select.num_selected == 1)
		inline_box->text = talloc_strdup(content->bctx,
				gadget->data.select.current->text);
	else
		inline_box->text = talloc_strdup(content->bctx,
				messages_get("Form_Many"));
	if (inline_box->text == NULL)
		goto no_memory;

	inline_box->length = strlen(inline_box->text);

	*convert_children = false;
	return true;

no_memory:
	return false;
}


/**
 * Add an option to a form select control (helper function for box_select()).
 *
 * \param  control  select containing the option
 * \param  n	    xml element node for <option>
 * \return  true on success, false on memory exhaustion
 */

bool box_select_add_option(struct form_control *control, dom_node *n)
{
	char *value = NULL;
	char *text = NULL;
	char *text_nowrap = NULL;
	bool selected;
	dom_string *content, *s;
	dom_exception err;

	err = dom_node_get_text_content(n, &content);
	if (err != DOM_NO_ERR)
		return false;

	if (content != NULL) {
		text = squash_whitespace(dom_string_data(content));
		dom_string_unref(content);
	} else {
		text = strdup("");
	}

	if (text == NULL)
		goto no_memory;

	err = dom_element_get_attribute(n, corestring_dom_value, &s);
	if (err == DOM_NO_ERR && s != NULL) {
		value = strdup(dom_string_data(s));
		dom_string_unref(s);
	} else {
		value = strdup(text);
	}

	if (value == NULL)
		goto no_memory;

	dom_element_has_attribute(n, corestring_dom_selected, &selected);

	/* replace spaces/TABs with hard spaces to prevent line wrapping */
	text_nowrap = cnv_space2nbsp(text);
	if (text_nowrap == NULL)
		goto no_memory;

	if (form_add_option(control, value, text_nowrap, selected, n) == false)
		goto no_memory;

	free(text);

	return true;

no_memory:
	free(value);
	free(text);
	free(text_nowrap);
	return false;
}


/**
 * Multi-line text field [17.7].
 */

bool box_textarea(BOX_SPECIAL_PARAMS)
{
	/* Get the form_control for the DOM node */
	box->gadget = html_forms_get_control_for_node(content->forms, n);
	if (box->gadget == NULL)
		return false;

	box->gadget->html = content;
	box->gadget->box = box;

	if (!box_input_text(content, box, n))
		return false;

	*convert_children = false;
	return true;
}


/**
 * Embedded object (not in any HTML specification:
 * see http://wp.netscape.com/assist/net_sites/new_html3_prop.html )
 */

bool box_embed(BOX_SPECIAL_PARAMS)
{
	struct object_params *params;
	struct object_param *param;
	dom_namednodemap *attrs;
	unsigned long idx;
	uint32_t num_attrs;
	dom_string *src;
	dom_exception err;

	if (box->style && css_computed_display(box->style,
			box_is_root(n)) == CSS_DISPLAY_NONE)
		return true;

	params = talloc(content->bctx, struct object_params);
	if (params == NULL)
		return false;

	talloc_set_destructor(params, box_object_talloc_destructor);

	params->data = NULL;
	params->type = NULL;
	params->codetype = NULL;
	params->codebase = NULL;
	params->classid = NULL;
	params->params = NULL;

	/* src is a URL */
	err = dom_element_get_attribute(n, corestring_dom_src, &src);
	if (err != DOM_NO_ERR || src == NULL)
		return true;
	if (box_extract_link(dom_string_data(src), content->base_url, 
			&params->data) == false) {
		dom_string_unref(src);
		return false;
	}

	dom_string_unref(src);

	if (params->data == NULL)
		return true;

	/* Don't include ourself */
	if (nsurl_compare(content->base_url, params->data, NSURL_COMPLETE))
		return true;

	/* add attributes as parameters to linked list */
	err = dom_node_get_attributes(n, &attrs);
	if (err != DOM_NO_ERR)
		return false;

	err = dom_namednodemap_get_length(attrs, &num_attrs);
	if (err != DOM_NO_ERR) {
		dom_namednodemap_unref(attrs);
		return false;
	}

	for (idx = 0; idx < num_attrs; idx++) {
		dom_attr *attr;
		dom_string *name, *value;

		err = dom_namednodemap_item(attrs, idx, (void *) &attr);
		if (err != DOM_NO_ERR) {
			dom_namednodemap_unref(attrs);
			return false;
		}

		err = dom_attr_get_name(attr, &name);
		if (err != DOM_NO_ERR) {
			dom_namednodemap_unref(attrs);
			return false;
		}

		if (dom_string_caseless_lwc_isequal(name, corestring_lwc_src)) {
			dom_string_unref(name);
			continue;
		}

		err = dom_attr_get_value(attr, &value);
		if (err != DOM_NO_ERR) {
			dom_string_unref(name);
			dom_namednodemap_unref(attrs);
			return false;
		}

		param = talloc(content->bctx, struct object_param);
		if (param == NULL) {
			dom_string_unref(value);
			dom_string_unref(name);
			dom_namednodemap_unref(attrs);
			return false;
		}

		param->name = talloc_strdup(content->bctx, dom_string_data(name));
		param->value = talloc_strdup(content->bctx, dom_string_data(value));
		param->type = NULL;
		param->valuetype = talloc_strdup(content->bctx, "data");
		param->next = NULL;

		dom_string_unref(value);
		dom_string_unref(name);

		if (param->name == NULL || param->value == NULL || 
				param->valuetype == NULL) {
			dom_namednodemap_unref(attrs);
			return false;
		}

		param->next = params->params;
		params->params = param;
	}

	dom_namednodemap_unref(attrs);

	box->object_params = params;

	/* start fetch */
	return html_fetch_object(content, params->data, box, CONTENT_ANY,
			content->base.available_width, 1000, false);
}

/**
 * \}
 */


/**
 * Get the value of an XML element's attribute.
 *
 * \param  n	      xmlNode, of type XML_ELEMENT_NODE
 * \param  attribute  name of attribute
 * \param  context    talloc context for result buffer
 * \param  value      updated to value, if the attribute is present
 * \return  true on success, false if attribute present but memory exhausted
 *
 * Note that returning true does not imply that the attribute was found. If the
 * attribute was not found, *value will be unchanged.
 */

bool box_get_attribute(dom_node *n, const char *attribute,
		void *context, char **value)
{
	char *result;
	dom_string *attr, *attr_name;
	dom_exception err;

	err = dom_string_create_interned((const uint8_t *) attribute, 
			strlen(attribute), &attr_name);
	if (err != DOM_NO_ERR)
		return false;

	err = dom_element_get_attribute(n, attr_name, &attr);
	if (err != DOM_NO_ERR) {
		dom_string_unref(attr_name);
		return false;
	}

	dom_string_unref(attr_name);

	if (attr != NULL) {
		result = talloc_strdup(context, dom_string_data(attr));

		dom_string_unref(attr);
	
		if (result == NULL)
			return false;

		*value = result;
	}

	return true;
}


/**
 * Extract a URL from a relative link, handling junk like whitespace and
 * attempting to read a real URL from "javascript:" links.
 *
 * \param  rel	   relative URL taken from page
 * \param  base	   base for relative URLs
 * \param  result  updated to target URL on heap, unchanged if extract failed
 * \return  true on success, false on memory exhaustion
 */

bool box_extract_link(const char *rel, nsurl *base, nsurl **result)
{
	char *s, *s1, *apos0 = 0, *apos1 = 0, *quot0 = 0, *quot1 = 0;
	unsigned int i, j, end;
	nserror error;

	s1 = s = malloc(3 * strlen(rel) + 1);
	if (!s)
		return false;

	/* copy to s, removing white space and control characters */
	for (i = 0; rel[i] && isspace(rel[i]); i++)
		;
	for (end = strlen(rel); end != i && isspace(rel[end - 1]); end--)
		;
	for (j = 0; i != end; i++) {
		if ((unsigned char) rel[i] < 0x20) {
			; /* skip control characters */
		} else if (rel[i] == ' ') {
			s[j++] = '%';
			s[j++] = '2';
			s[j++] = '0';
		} else {
			s[j++] = rel[i];
		}
	}
	s[j] = 0;

	if (nsoption_bool(enable_javascript) == false) {
		/* extract first quoted string out of "javascript:" link */
		if (strncmp(s, "javascript:", 11) == 0) {
			apos0 = strchr(s, '\'');
			if (apos0)
				apos1 = strchr(apos0 + 1, '\'');
			quot0 = strchr(s, '"');
			if (quot0)
				quot1 = strchr(quot0 + 1, '"');
			if (apos0 && apos1 && 
					(!quot0 || !quot1 || apos0 < quot0)) {
				*apos1 = 0;
				s1 = apos0 + 1;
			} else if (quot0 && quot1) {
				*quot1 = 0;
				s1 = quot0 + 1;
			}
		}
	}

	/* construct absolute URL */
	error = nsurl_join(base, s1, result);
	free(s);
	if (error != NSERROR_OK) {
		*result = NULL;
		return false;
	}

	return true;
}


/**
 * Parse a multi-length-list, as defined by HTML 4.01.
 *
 * \param  s	    string to parse
 * \param  count    updated to number of entries
 * \return  array of struct box_multi_length, or 0 on memory exhaustion
 */

struct frame_dimension *box_parse_multi_lengths(const char *s,
		unsigned int *count)
{
	char *end;
	unsigned int i, n;
	struct frame_dimension *length;

	for (i = 0, n = 1; s[i]; i++)
		if (s[i] == ',')
			n++;

	length = calloc(n, sizeof(struct frame_dimension));
	if (!length)
		return NULL;

	for (i = 0; i != n; i++) {
		while (isspace(*s))
			s++;
		length[i].value = strtof(s, &end);
		if (length[i].value <= 0)
			length[i].value = 1;
		s = end;
		switch (*s) {
			case '%':
				length[i].unit = FRAME_DIMENSION_PERCENT;
				break;
			case '*':
				length[i].unit = FRAME_DIMENSION_RELATIVE;
				break;
			default:
				length[i].unit = FRAME_DIMENSION_PIXELS;
				break;
		}
		while (*s && *s != ',')
			s++;
		if (*s == ',')
			s++;
	}

	*count = n;
	return length;
}

