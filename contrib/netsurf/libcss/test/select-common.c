#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcss/libcss.h>
#include <libcss/computed.h>
#include <libcss/select.h>
#include <libcss/stylesheet.h>

#include "utils/utils.h"

#include "dump_computed.h"
#include "testutils.h"

typedef struct attribute {
	lwc_string *name;
	lwc_string *value;
} attribute;

typedef struct node {
	lwc_string *name;

	uint32_t n_classes;
	lwc_string **classes;

	uint32_t n_attrs;
	attribute *attrs;

	void *libcss_node_data;

	struct node *parent;
	struct node *next;
	struct node *prev;
	struct node *children;
	struct node *last_child;
} node;

typedef struct sheet_ctx {
	css_stylesheet *sheet;
	css_origin origin;
	uint64_t media;
} sheet_ctx;

typedef struct line_ctx {
	size_t explen;
	size_t expused;
	char *exp;

	bool intree;
	bool insheet;
	bool inerrors;
	bool inexp;

	node *tree;
	node *current;
	uint32_t depth;

	uint32_t n_sheets;
	sheet_ctx *sheets;

	uint64_t media;
	uint32_t pseudo_element;
	node *target;
	
	lwc_string *attr_class;
	lwc_string *attr_id;
} line_ctx;




static bool handle_line(const char *data, size_t datalen, void *pw);
static void css__parse_tree(line_ctx *ctx, const char *data, size_t len);
static void css__parse_tree_data(line_ctx *ctx, const char *data, size_t len);
static void css__parse_sheet(line_ctx *ctx, const char *data, size_t len);
static void css__parse_media_list(const char **data, size_t *len, uint64_t *media);
static void css__parse_pseudo_list(const char **data, size_t *len, 
		uint32_t *element);
static void css__parse_expected(line_ctx *ctx, const char *data, size_t len);
static void run_test(line_ctx *ctx, const char *exp, size_t explen);
static void destroy_tree(node *root);

static css_error node_name(void *pw, void *node,
		css_qname *qname);
static css_error node_classes(void *pw, void *n,
		lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node,
		lwc_string **id);
static css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname,
		void **ancestor);
static css_error named_parent_node(void *pw, void *node,
		const css_qname *qname,
		void **parent);
static css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node, 
		const css_qname *qname, 
		bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname,
		bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_is_root(void *pw, void *node, bool *match);
static css_error node_count_siblings(void *pw, void *node,
		bool same_name, bool after, int32_t *count);
static css_error node_is_empty(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_visited(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
static css_error node_is_enabled(void *pw, void *node, bool *match);
static css_error node_is_disabled(void *pw, void *node, bool *match);
static css_error node_is_checked(void *pw, void *node, bool *match);
static css_error node_is_target(void *pw, void *node, bool *match);
static css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match);
static css_error node_presentational_hint(void *pw, void *node,
		uint32_t property, css_hint *hint);
static css_error ua_default_for_property(void *pw, uint32_t property,
		css_hint *hint);
static css_error compute_font_size(void *pw, const css_hint *parent,
		css_hint *size);
static css_error set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data);
static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data);

static css_select_handler select_handler = {
	CSS_SELECT_HANDLER_VERSION_1,

	node_name,
	node_classes,
	node_id,
	named_ancestor_node,
	named_parent_node,
	named_sibling_node,
	named_generic_sibling_node,
	parent_node,
	sibling_node,
	node_has_name,
	node_has_class,
	node_has_id,
	node_has_attribute,
	node_has_attribute_equal,
	node_has_attribute_dashmatch,
	node_has_attribute_includes,
	node_has_attribute_prefix,
	node_has_attribute_suffix,
	node_has_attribute_substring,
	node_is_root,
	node_count_siblings,
	node_is_empty,
	node_is_link,
	node_is_visited,
	node_is_hover,
	node_is_active,
	node_is_focus,
	node_is_enabled,
	node_is_disabled,
	node_is_checked,
	node_is_target,
	node_is_lang,
	node_presentational_hint,
	ua_default_for_property,
	compute_font_size,
	set_libcss_node_data,
	get_libcss_node_data
};

static css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs)
{
	UNUSED(pw);
	UNUSED(base);

	/* About as useless as possible */
	*abs = lwc_string_ref(rel);

	return CSS_OK;
}

static bool fail_because_lwc_leaked = false;

static void
printing_lwc_iterator(lwc_string *str, void *pw)
{
	UNUSED(pw);
	
	printf(" DICT: %*s\n", (int)(lwc_string_length(str)), lwc_string_data(str));
	fail_because_lwc_leaked = true;
}

int main(int argc, char **argv)
{
	line_ctx ctx;
	
	if (argc != 2) {
		printf("Usage: %s <filename>\n", argv[0]);
		return 1;
	}

	memset(&ctx, 0, sizeof(ctx));


	lwc_intern_string("class", SLEN("class"), &ctx.attr_class);
	lwc_intern_string("id", SLEN("id"), &ctx.attr_id);
	
	assert(css__parse_testfile(argv[1], handle_line, &ctx) == true);
	
	/* and run final test */
	if (ctx.tree != NULL)
		run_test(&ctx, ctx.exp, ctx.expused);

	free(ctx.exp);
	
	lwc_string_unref(ctx.attr_class);
	lwc_string_unref(ctx.attr_id);
	
	lwc_iterate_strings(printing_lwc_iterator, NULL);
	
	assert(fail_because_lwc_leaked == false);
	
	printf("PASS\n");
	return 0;
}

bool handle_line(const char *data, size_t datalen, void *pw)
{
	line_ctx *ctx = (line_ctx *) pw;
	css_error error;

	if (data[0] == '#') {
		if (ctx->intree) {
			if (strncasecmp(data+1, "errors", 6) == 0) {
				ctx->intree = false;
				ctx->insheet = false;
				ctx->inerrors = true;
				ctx->inexp = false;
			} else {
				/* Assume start of stylesheet */
				css__parse_sheet(ctx, data + 1, datalen - 1);

				ctx->intree = false;
				ctx->insheet = true;
				ctx->inerrors = false;
				ctx->inexp = false;
			}
		} else if (ctx->insheet) {
			if (strncasecmp(data+1, "errors", 6) == 0) {
				assert(css_stylesheet_data_done(
					ctx->sheets[ctx->n_sheets - 1].sheet) 
					== CSS_OK);

				ctx->intree = false;
				ctx->insheet = false;
				ctx->inerrors = true;
				ctx->inexp = false;
			} else if (strncasecmp(data+1, "ua", 2) == 0 ||
					strncasecmp(data+1, "user", 4) == 0 ||
					strncasecmp(data+1, "author", 6) == 0) {
				assert(css_stylesheet_data_done(
					ctx->sheets[ctx->n_sheets - 1].sheet)
					== CSS_OK);

				css__parse_sheet(ctx, data + 1, datalen - 1);
			} else {
				error = css_stylesheet_append_data(
					ctx->sheets[ctx->n_sheets - 1].sheet, 
					(const uint8_t *) data, 
					datalen);
				assert(error == CSS_OK || 
						error == CSS_NEEDDATA);
			}
		} else if (ctx->inerrors) {
			ctx->intree = false;
			ctx->insheet = false;
			ctx->inerrors = false;
			ctx->inexp = true;
		} else if (ctx->inexp) {
			/* This marks end of testcase, so run it */
			run_test(ctx, ctx->exp, ctx->expused);

			ctx->expused = 0;

			ctx->intree = false;
			ctx->insheet = false;
			ctx->inerrors = false;
			ctx->inexp = false;
		} else {
			/* Start state */
			if (strncasecmp(data+1, "tree", 4) == 0) {
				css__parse_tree(ctx, data + 5, datalen - 5);

				ctx->intree = true;
				ctx->insheet = false;
				ctx->inerrors = false;
				ctx->inexp = false;
			}
		}
	} else {
		if (ctx->intree) {
			/* Not interested in the '|' */
			css__parse_tree_data(ctx, data + 1, datalen - 1);
		} else if (ctx->insheet) {
			error = css_stylesheet_append_data(
					ctx->sheets[ctx->n_sheets - 1].sheet, 
					(const uint8_t *) data, datalen);
			assert(error == CSS_OK || error == CSS_NEEDDATA);
		} else if (ctx->inexp) {
			css__parse_expected(ctx, data, datalen);
		}
	}

	return true;
}

void css__parse_tree(line_ctx *ctx, const char *data, size_t len)
{
	const char *p = data;
	const char *end = data + len;
	size_t left;

	/* [ <media_list> <pseudo>? ] ? */

	ctx->media = CSS_MEDIA_ALL;
	ctx->pseudo_element = CSS_PSEUDO_ELEMENT_NONE;

	/* Consume any leading whitespace */
	while (p < end && isspace(*p))
		p++;

	if (p < end) {
		left = end - p;

		css__parse_media_list(&p, &left, &ctx->media);

		end = p + left;
	}

	if (p < end) {
		left = end - p;

		css__parse_pseudo_list(&p, &left, &ctx->pseudo_element);
	}
}

void css__parse_tree_data(line_ctx *ctx, const char *data, size_t len)
{
	const char *p = data;
	const char *end = data + len;
	const char *name = NULL;
	const char *value = NULL;
	size_t namelen = 0;
	size_t valuelen = 0;
	uint32_t depth = 0;
	bool target = false;

	/* ' '{depth+1} [ <element> '*'? | <attr> ]
	 * 
	 * <element> ::= [^=*[:space:]]+
	 * <attr>    ::= [^=*[:space:]]+ '=' [^[:space:]]*
	 */

	while (p < end && isspace(*p)) {
		depth++;
		p++;
	}
	depth--;

	/* Get element/attribute name */
	name = p;
	while (p < end && *p != '=' && *p != '*' && isspace(*p) == false) {
		namelen++;
		p++;
	}

	/* Skip whitespace */
	while (p < end && isspace(*p))
		p++;

	if (p < end && *p == '=') {
		/* Attribute value */
		p++;

		value = p;

		while (p < end && isspace(*p) == false) {
			valuelen++;
			p++;
		}
	} else if (p < end && *p == '*') {
		/* Element is target node */
		target = true;
	}

	if (value == NULL) {
		/* We have an element, so create it */
		node *n = malloc(sizeof(node));
		assert(n != NULL);

		memset(n, 0, sizeof(node));
		
		lwc_intern_string(name, namelen, &n->name);

		/* Insert it into tree */
		if (ctx->tree == NULL) {
			ctx->tree = n;
		} else {
			assert(depth > 0);
			assert(depth <= ctx->depth + 1);

			/* Find node to insert into */
			while (depth <= ctx->depth) {
				ctx->depth--;
				ctx->current = ctx->current->parent;
			}

			/* Insert into current node */
			if (ctx->current->children == NULL) {
				ctx->current->children = n;
				ctx->current->last_child = n;
			} else {
				ctx->current->last_child->next = n;
				n->prev = ctx->current->last_child;

				ctx->current->last_child = n;
			}
			n->parent = ctx->current;
		}

		ctx->current = n;
		ctx->depth = depth;

		/* Mark the target, if it's us */
		if (target)
			ctx->target = n;
	} else {
		/* New attribute */
		bool amatch = false;
		attribute *attr;
		node *n = ctx->current;

		attribute *temp = realloc(n->attrs,
				(n->n_attrs + 1) * sizeof(attribute));
		assert(temp != NULL);

		n->attrs = temp;

		attr = &n->attrs[n->n_attrs];
		
		lwc_intern_string(name, namelen, &attr->name);
		lwc_intern_string(value, valuelen, &attr->value);

		assert(lwc_string_caseless_isequal(
				n->attrs[n->n_attrs].name,
				ctx->attr_class, &amatch) == lwc_error_ok);
		if (amatch == true) {
			n->classes = realloc(NULL, sizeof(lwc_string **));
			assert(n->classes != NULL);

			n->classes[0] = lwc_string_ref(
					n->attrs[n->n_attrs].
					value);
			n->n_classes = 1;
		}

		n->n_attrs++;
	}
}

void css__parse_sheet(line_ctx *ctx, const char *data, size_t len)
{
	css_stylesheet_params params;
	const char *p;
	const char *end = data + len;
	css_origin origin = CSS_ORIGIN_AUTHOR;
	uint64_t media = CSS_MEDIA_ALL;
	css_stylesheet *sheet;
	sheet_ctx *temp;

	/* <origin> <media_list>? */

	/* Find end of origin */
	for (p = data; p < end; p++) {
		if (isspace(*p))
			break;
	}

	if (p - data == 6 && strncasecmp(data, "author", 6) == 0)
		origin = CSS_ORIGIN_AUTHOR;
	else if (p - data == 4 && strncasecmp(data, "user", 4) == 0)
		origin = CSS_ORIGIN_USER;
	else if (p - data == 2 && strncasecmp(data, "ua", 2) == 0)
		origin = CSS_ORIGIN_UA;
	else
		assert(0 && "Unknown stylesheet origin");

	/* Skip any whitespace */
	while (p < end && isspace(*p))
		p++;

	if (p < end) {
		size_t ignored = end - p;

		css__parse_media_list(&p, &ignored, &media);
	}

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = "foo";
	params.title = "foo";
	params.allow_quirks = false;
	params.inline_style = false;
	params.resolve = resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = NULL;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	/** \todo How are we going to handle @import? */
	assert(css_stylesheet_create(&params, &sheet) == CSS_OK);

	/* Extend array of sheets and append new sheet to it */
	temp = realloc(ctx->sheets, 
			(ctx->n_sheets + 1) * sizeof(sheet_ctx));
	assert(temp != NULL);

	ctx->sheets = temp;

	ctx->sheets[ctx->n_sheets].sheet = sheet;
	ctx->sheets[ctx->n_sheets].origin = origin;
	ctx->sheets[ctx->n_sheets].media = media;

	ctx->n_sheets++;
}

void css__parse_media_list(const char **data, size_t *len, uint64_t *media)
{
	const char *p = *data;
	const char *end = p + *len;
	uint64_t result = 0;

	/* <medium> [ ',' <medium> ]* */

	while (p < end) {
		const char *start = p;

		/* consume a medium */
		while (isspace(*p) == false && *p != ',')
			p++;

		if (p - start == 10 && 
				strncasecmp(start, "projection", 10) == 0)
			result |= CSS_MEDIA_PROJECTION;
		else if (p - start == 8 &&
				strncasecmp(start, "handheld", 8) == 0)
			result |= CSS_MEDIA_HANDHELD;
		else if (p - start == 8 &&
				strncasecmp(start, "embossed", 8) == 0)
			result |= CSS_MEDIA_EMBOSSED;
		else if (p - start == 7 &&
				strncasecmp(start, "braille", 7) == 0)
			result |= CSS_MEDIA_BRAILLE;
		else if (p - start == 6 &&
				strncasecmp(start, "speech", 6) == 0)
			result |= CSS_MEDIA_SPEECH;
		else if (p - start == 6 &&
				strncasecmp(start, "screen", 6) == 0)
			result |= CSS_MEDIA_SCREEN;
		else if (p - start == 5 &&
				strncasecmp(start, "print", 5) == 0)
			result |= CSS_MEDIA_PRINT;
		else if (p - start == 5 &&
				strncasecmp(start, "aural", 5) == 0)
			result |= CSS_MEDIA_AURAL;
		else if (p - start == 3 &&
				strncasecmp(start, "tty", 3) == 0)
			result |= CSS_MEDIA_TTY;
		else if (p - start == 3 &&
				strncasecmp(start, "all", 3) == 0)
			result |= CSS_MEDIA_ALL;
		else if (p - start == 2 &&
				strncasecmp(start, "tv", 2) == 0)
			result |= CSS_MEDIA_TV;
		else
			assert(0 && "Unknown media type");

		/* Consume whitespace */
		while (p < end && isspace(*p))
			p++;

		/* Stop if we've reached the end */
		if (p == end || *p != ',')
			break;

		/* Consume comma */
		p++;

		/* Consume whitespace */
		while (p < end && isspace(*p))
			p++;
	}

	*media = result;

	*data = p;
	*len = end - p;
}

void css__parse_pseudo_list(const char **data, size_t *len, uint32_t *element)
{
	const char *p = *data;
	const char *end = p + *len;

	/* <pseudo> [ ',' <pseudo> ]* */

	*element = CSS_PSEUDO_ELEMENT_NONE;

	while (p < end) {
		const char *start = p;

		/* consume a pseudo */
		while (isspace(*p) == false && *p != ',')
			p++;

		/* Pseudo elements */
		if (p - start == 12 &&
				strncasecmp(start, "first-letter", 12) == 0)
			*element = CSS_PSEUDO_ELEMENT_FIRST_LETTER;
		else if (p - start == 10 &&
				strncasecmp(start, "first-line", 10) == 0)
			*element = CSS_PSEUDO_ELEMENT_FIRST_LINE;
		else if (p - start == 6 &&
				strncasecmp(start, "before", 6) == 0)
			*element = CSS_PSEUDO_ELEMENT_BEFORE;
		else if (p - start == 5 &&
				strncasecmp(start, "after", 5) == 0)
			*element = CSS_PSEUDO_ELEMENT_AFTER;
		else
			assert(0 && "Unknown pseudo");

		/* Consume whitespace */
		while (p < end && isspace(*p))
			p++;

		/* Stop if we've reached the end */
		if (p == end || *p != ',')
			break;

		/* Consume comma */
		p++;

		/* Consume whitespace */
		while (p < end && isspace(*p))
			p++;
	}

	*data = p;
	*len = end - p;
}

void css__parse_expected(line_ctx *ctx, const char *data, size_t len)
{
	while (ctx->expused + len >= ctx->explen) {
		size_t required = ctx->explen == 0 ? 64 : ctx->explen * 2;
		char *temp = realloc(ctx->exp, required);
		if (temp == NULL) {
			assert(0 && "No memory for expected output");
		}

		ctx->exp = temp;
		ctx->explen = required;
	}

	memcpy(ctx->exp + ctx->expused, data, len);

	ctx->expused += len;
}

void run_test(line_ctx *ctx, const char *exp, size_t explen)
{
	css_select_ctx *select;
	css_select_results *results;
	uint32_t i;
	char *buf;
	size_t buflen;
	static int testnum;

	UNUSED(exp);

	buf = malloc(8192);
	if (buf == NULL) {
		assert(0 && "No memory for result data");
	}
	buflen = 8192;

	assert(css_select_ctx_create(&select) == CSS_OK);

	for (i = 0; i < ctx->n_sheets; i++) {
		assert(css_select_ctx_append_sheet(select, 
				ctx->sheets[i].sheet, ctx->sheets[i].origin,
				ctx->sheets[i].media) == CSS_OK);
	}

	testnum++;

	assert(css_select_style(select, ctx->target, ctx->media, NULL, 
			&select_handler, ctx, &results) == CSS_OK);

	assert(results->styles[ctx->pseudo_element] != NULL);

	dump_computed_style(results->styles[ctx->pseudo_element], buf, &buflen);

	if (8192 - buflen != explen || memcmp(buf, exp, explen) != 0) {
		printf("Expected (%u):\n%.*s\n", 
				(int) explen, (int) explen, exp);
		printf("Result (%u):\n%.*s\n", (int) (8192 - buflen),
			(int) (8192 - buflen), buf);
		assert(0 && "Result doesn't match expected");
	}

	/* Clean up */
	css_select_results_destroy(results);
	css_select_ctx_destroy(select);

	destroy_tree(ctx->tree);

	for (i = 0; i < ctx->n_sheets; i++) {
		css_stylesheet_destroy(ctx->sheets[i].sheet);
	}

	ctx->tree = NULL;
	ctx->current = NULL;
	ctx->depth = 0;
	ctx->n_sheets = 0;
	free(ctx->sheets);
	ctx->sheets = NULL;
	ctx->target = NULL;

	free(buf);

	printf("Test %d: PASS\n", testnum);
}

void destroy_tree(node *root)
{
	node *n, *p;
	uint32_t i;

	for (n = root->children; n != NULL; n = p) {
		p = n->next;

		destroy_tree(n);
	}
	
	for (i = 0; i < root->n_attrs; ++i) {
		lwc_string_unref(root->attrs[i].name);
		lwc_string_unref(root->attrs[i].value);
	}
	free(root->attrs);

	if (root->classes != NULL) {
		for (i = 0; i < root->n_classes; ++i) {
			lwc_string_unref(root->classes[i]);
		}
		free(root->classes);
	}

	if (root->libcss_node_data != NULL) {
		css_libcss_node_data_handler(&select_handler, CSS_NODE_DELETED,
				NULL, root, NULL, root->libcss_node_data);
	}

	lwc_string_unref(root->name);
	free(root);
}


css_error node_name(void *pw, void *n, css_qname *qname)
{
	node *node = n;

	UNUSED(pw);
	
	qname->name = lwc_string_ref(node->name);
	
	return CSS_OK;
}

static css_error node_classes(void *pw, void *n,
		lwc_string ***classes, uint32_t *n_classes)
{
	unsigned int i;
	node *node = n;
	UNUSED(pw);

	*classes = node->classes;
	*n_classes = node->n_classes;

	for (i = 0; i < *n_classes; i++)
		(*classes)[i] = lwc_string_ref(node->classes[i]);

	return CSS_OK;

}

css_error node_id(void *pw, void *n,
		lwc_string **id)
{
	node *node = n;
	uint32_t i;
	line_ctx *lc = pw;

	for (i = 0; i < node->n_attrs; i++) {
		bool amatch = false;
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, lc->attr_id, &amatch) == 
				lwc_error_ok);
		if (amatch == true)
			break;
	}

	if (i != node->n_attrs)
		*id = lwc_string_ref(node->attrs[i].value);
	else
		*id = NULL;

	return CSS_OK;
}

css_error named_ancestor_node(void *pw, void *n,
		const css_qname *qname,
		void **ancestor)
{
	node *node = n;
	UNUSED(pw);

	for (node = node->parent; node != NULL; node = node->parent) {
		bool match = false;
		assert(lwc_string_caseless_isequal(
				qname->name, node->name, 
				&match) == lwc_error_ok);
		if (match == true)
			break;
	}

	*ancestor = (void *) node;

	return CSS_OK;
}

css_error named_parent_node(void *pw, void *n,
		const css_qname *qname,
		void **parent)
{
	node *node = n;
	UNUSED(pw);

	*parent = NULL;
	if (node->parent != NULL) {
		bool match = false;
		assert(lwc_string_caseless_isequal(
				qname->name, node->parent->name, &match) == 
				lwc_error_ok);
		if (match == true)
			*parent = (void *) node->parent;
	}

	return CSS_OK;
}

css_error named_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	node *node = n;
	UNUSED(pw);

	*sibling = NULL;
	if (node->prev != NULL) {
		bool match = false;
		assert(lwc_string_caseless_isequal(
				qname->name, node->prev->name, &match) == 
				lwc_error_ok);
		if (match == true)
			*sibling = (void *) node->prev;
	}

	return CSS_OK;
}

css_error named_generic_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	node *node = n;
	UNUSED(pw);

	for (node = node->prev; node != NULL; node = node->prev) {
		bool match = false;
		assert(lwc_string_caseless_isequal(
				qname->name, node->name, 
				&match) == lwc_error_ok);
		if (match == true)
			break;
	}

	*sibling = (void *) node;

	return CSS_OK;
}

css_error parent_node(void *pw, void *n, void **parent)
{
	node *node = n;

	UNUSED(pw);

	*parent = (void *) node->parent;

	return CSS_OK;
}

css_error sibling_node(void *pw, void *n, void **sibling)
{
	node *node = n;

	UNUSED(pw);

	*sibling = (void *) node->prev;

	return CSS_OK;
}

css_error node_has_name(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	node *node = n;
	UNUSED(pw);

	if (lwc_string_length(qname->name) == 1 &&
			lwc_string_data(qname->name)[0] == '*')
		*match = true;
	else
		assert(lwc_string_caseless_isequal(node->name, 
			qname->name, match) == lwc_error_ok);

	return CSS_OK;
}

css_error node_has_class(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	node *node = n;
	uint32_t i;
	line_ctx *ctx = pw;

	for (i = 0; i < node->n_attrs; i++) {
		bool amatch = false;
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, ctx->attr_class, 
				&amatch) == lwc_error_ok);
		if (amatch == true)
			break;
	}

	/* Classes are case-sensitive in HTML */
	if (i != node->n_attrs && name == node->attrs[i].value)
		*match = true;
	else
		*match = false;

	return CSS_OK;
}

css_error node_has_id(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	node *node = n;
	uint32_t i;
	line_ctx *ctx = pw;

	for (i = 0; i < node->n_attrs; i++) {
		bool amatch = false;
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, ctx->attr_id, &amatch) == 
				lwc_error_ok);
		if (amatch == true)
			break;
	}

	/* IDs are case-sensitive in HTML */
	if (i != node->n_attrs && name == node->attrs[i].value)
		*match = true;
	else
		*match = false;

	return CSS_OK;
}

css_error node_has_attribute(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	node *node = n;
	uint32_t i;
	UNUSED(pw);
	
	*match = false;
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}

	return CSS_OK;
}

css_error node_has_attribute_equal(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}
	
	if (*match == true) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, value, match) == 
				lwc_error_ok);
	}
	
	return CSS_OK;
}

css_error node_has_attribute_includes(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	size_t vlen = lwc_string_length(value);
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}

	if (*match == true) {
		const char *p;
		const char *start = lwc_string_data(node->attrs[i].value);
		const char *end = start + 
				lwc_string_length(node->attrs[i].value);
		
		*match = false;
		
		for (p = start; p < end; p++) {
			if (*p == ' ') {
				if ((size_t) (p - start) == vlen && 
						strncasecmp(start,
							lwc_string_data(value),
							vlen) == 0) {
					*match = true;
					break;
				}

				start = p + 1;
			}
		}
	}

	return CSS_OK;
}

css_error node_has_attribute_dashmatch(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	size_t vlen = lwc_string_length(value);
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}

	if (*match == true) {
		const char *p;
		const char *start = lwc_string_data(node->attrs[i].value);
		const char *end = start + 
				lwc_string_length(node->attrs[i].value);
		
		*match = false;
		
		for (p = start; p < end; p++) {
			if (*p == '-') {
				if ((size_t) (p - start) == vlen && 
						strncasecmp(start,
							lwc_string_data(value), 
							vlen) == 0) {
					*match = true;
					break;
				}

				start = p + 1;
			}
		}
	}

	return CSS_OK;
}

css_error node_has_attribute_prefix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}
	
	if (*match == true) {
		size_t len = lwc_string_length(node->attrs[i].value);
		const char *data = lwc_string_data(node->attrs[i].value);

		size_t vlen = lwc_string_length(value);
		const char *vdata = lwc_string_data(value);

		if (len < vlen)
			*match = false;
		else
			*match = (strncasecmp(data, vdata, vlen) == 0);
	}
	
	return CSS_OK;
}

css_error node_has_attribute_suffix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}
	
	if (*match == true) {
		size_t len = lwc_string_length(node->attrs[i].value);
		const char *data = lwc_string_data(node->attrs[i].value);

		size_t vlen = lwc_string_length(value);
		const char *vdata = lwc_string_data(value);

		size_t suffix_start = len - vlen;

		if (len < vlen)
			*match = false;
		else {
			*match = (strncasecmp(data + suffix_start, 
					vdata, vlen) == 0);
		}
	}

	return CSS_OK;
}

css_error node_has_attribute_substring(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	node *node = n;
	uint32_t i;
	UNUSED(pw);

	*match = false;
	
	for (i = 0; i < node->n_attrs; i++) {
		assert(lwc_string_caseless_isequal(
				node->attrs[i].name, qname->name, match) == 
				lwc_error_ok);
		if (*match == true)
			break;
	}
	
	if (*match == true) {
		size_t len = lwc_string_length(node->attrs[i].value);
		const char *data = lwc_string_data(node->attrs[i].value);

		size_t vlen = lwc_string_length(value);
		const char *vdata = lwc_string_data(value);

		const char *last_start = data + len - vlen;

		if (len < vlen)
			*match = false;
		else {
			while (data <= last_start) {
				if (strncasecmp(data, vdata, vlen) == 0) {
					*match = true;
					break;
				}

				data++;
			}

			if (data > last_start)
				*match = false;
		}
	}

	return CSS_OK;
}

css_error node_is_root(void *pw, void *n, bool *match)
{
	node *node = n;
	UNUSED(pw);

	*match = (node->parent == NULL);

	return CSS_OK;
}

css_error node_count_siblings(void *pw, void *n,
		bool same_name, bool after, int32_t *count)
{
	int32_t cnt = 0;
	bool match = false;
	node *node = n;
	lwc_string *name = node->name;
	UNUSED(pw);

	if (after) {
		while (node->next != NULL) {
			if (same_name) {
				assert(lwc_string_caseless_isequal(
					name, node->next->name, &match) ==
					lwc_error_ok);

				if (match)
					cnt++;
			} else {
				cnt++;
			}

			node = node->next;
		}
	} else {
		while (node->prev != NULL) {
			if (same_name) {
				assert(lwc_string_caseless_isequal(
					name, node->prev->name, &match) ==
					lwc_error_ok);

				if (match)
					cnt++;
			} else {
				cnt++;
			}

			node = node->prev;
		}
	}

	*count = cnt;

	return CSS_OK;
}

css_error node_is_empty(void *pw, void *n, bool *match)
{
	node *node = n;
	UNUSED(pw);

	*match = (node->children == NULL);

	return CSS_OK;
}

css_error node_is_link(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_visited(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_hover(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_active(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_focus(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_enabled(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_disabled(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_checked(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_target(void *pw, void *n, bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);

	*match = false;

	return CSS_OK;
}

css_error node_is_lang(void *pw, void *n,
		lwc_string *lang,
		bool *match)
{
	node *node = n;

	UNUSED(pw);
	UNUSED(node);
	UNUSED(lang);

	*match = false;

	return CSS_OK;
}

css_error node_presentational_hint(void *pw, void *node,
		uint32_t property, css_hint *hint)
{
	UNUSED(pw);
	UNUSED(node);
	UNUSED(property);
	UNUSED(hint);

	return CSS_PROPERTY_NOT_SET;
}

css_error ua_default_for_property(void *pw, uint32_t property, css_hint *hint)
{
	UNUSED(pw);

	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0xff000000;
		hint->status = CSS_COLOR_COLOR;
	} else if (property == CSS_PROP_FONT_FAMILY) {
		hint->data.strings = NULL;
		hint->status = CSS_FONT_FAMILY_SANS_SERIF;
	} else if (property == CSS_PROP_QUOTES) {
		/* Not exactly useful :) */
		hint->data.strings = NULL;
		hint->status = CSS_QUOTES_NONE;
	} else if (property == CSS_PROP_VOICE_FAMILY) {
		/** \todo Fix this when we have voice-family done */
		hint->data.strings = NULL;
		hint->status = 0;
	} else {
		return CSS_INVALID;
	}

	return CSS_OK;
}

css_error compute_font_size(void *pw, const css_hint *parent, css_hint *size)
{
	static css_hint_length sizes[] = {
		{ FLTTOFIX(6.75), CSS_UNIT_PT },
		{ FLTTOFIX(7.50), CSS_UNIT_PT },
		{ FLTTOFIX(9.75), CSS_UNIT_PT },
		{ FLTTOFIX(12.0), CSS_UNIT_PT },
		{ FLTTOFIX(13.5), CSS_UNIT_PT },
		{ FLTTOFIX(18.0), CSS_UNIT_PT },
		{ FLTTOFIX(24.0), CSS_UNIT_PT }
	};
	const css_hint_length *parent_size;

	UNUSED(pw);

	/* Grab parent size, defaulting to medium if none */
	if (parent == NULL) {
		parent_size = &sizes[CSS_FONT_SIZE_MEDIUM - 1];
	} else {
		assert(parent->status == CSS_FONT_SIZE_DIMENSION);
		assert(parent->data.length.unit != CSS_UNIT_EM);
		assert(parent->data.length.unit != CSS_UNIT_EX);
		parent_size = &parent->data.length;
	}

	assert(size->status != CSS_FONT_SIZE_INHERIT);

	if (size->status < CSS_FONT_SIZE_LARGER) {
		/* Keyword -- simple */
		size->data.length = sizes[size->status - 1];
	} else if (size->status == CSS_FONT_SIZE_LARGER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value = 
				FMUL(parent_size->value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size->unit;
	} else if (size->status == CSS_FONT_SIZE_SMALLER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value = 
				FMUL(parent_size->value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size->unit;
	} else if (size->data.length.unit == CSS_UNIT_EM ||
			size->data.length.unit == CSS_UNIT_EX) {
		size->data.length.value = 
			FMUL(size->data.length.value, parent_size->value);

		if (size->data.length.unit == CSS_UNIT_EX) {
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.6));
		}

		size->data.length.unit = parent_size->unit;
	} else if (size->data.length.unit == CSS_UNIT_PCT) {
		size->data.length.value = FDIV(FMUL(size->data.length.value,
				parent_size->value), FLTTOFIX(100));
		size->data.length.unit = parent_size->unit;
	}

	size->status = CSS_FONT_SIZE_DIMENSION;

	return CSS_OK;
}

static css_error set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data)
{
	node *node = n;
	UNUSED(pw);

	node->libcss_node_data = libcss_node_data;

	return CSS_OK;
}


