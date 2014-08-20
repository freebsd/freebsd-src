/*
 * LibCSS - example1.c
 *
 * Compile this using a command such as:
 *  gcc -g -W -Wall -o example1 example1.c `pkg-config --cflags --libs libcss`
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The entire API is available through this header. */
#include <libcss/libcss.h>


/* This macro is used to silence compiler warnings about unused function
 * arguments. */
#define UNUSED(x) ((x) = (x))


/* Function declarations. */
static css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs);
static void die(const char *text, css_error code);

static css_error node_name(void *pw, void *node,
		css_qname *qname);
static css_error node_classes(void *pw, void *node,
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

/* Table of function pointers for the LibCSS Select API. */
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


int main(int argc, char **argv)
{
	css_error code;
	css_stylesheet *sheet;
	size_t size;
	const char data[] = "h1 { color: red } "
		"h4 { color: #321; } "
		"h4, h5 { color: #123456; }";
	css_select_ctx *select_ctx;
	uint32_t count;
	unsigned int hh;
	css_stylesheet_params params;

	UNUSED(argc);
	UNUSED(argv);

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

	/* create a stylesheet */
	code = css_stylesheet_create(&params, &sheet);
	if (code != CSS_OK)
		die("css_stylesheet_create", code);
	code = css_stylesheet_size(sheet, &size);
	if (code != CSS_OK)
		die("css_stylesheet_size", code);
	printf("created stylesheet, size %zu\n", size);


	/* parse some CSS source */
	code = css_stylesheet_append_data(sheet, (const uint8_t *) data,
			sizeof data);
	if (code != CSS_OK && code != CSS_NEEDDATA)
		die("css_stylesheet_append_data", code);
	code = css_stylesheet_data_done(sheet);
	if (code != CSS_OK)
		die("css_stylesheet_data_done", code);
	code = css_stylesheet_size(sheet, &size);
	if (code != CSS_OK)
		die("css_stylesheet_size", code);
	printf("appended data, size now %zu\n", size);


	/* prepare a selection context containing the stylesheet */
	code = css_select_ctx_create(&select_ctx);
	if (code != CSS_OK)
		die("css_select_ctx_create", code);
	code = css_select_ctx_append_sheet(select_ctx, sheet, CSS_ORIGIN_AUTHOR,
			CSS_MEDIA_ALL);
	if (code != CSS_OK)
		die("css_select_ctx_append_sheet", code);
	code = css_select_ctx_count_sheets(select_ctx, &count);
	if (code != CSS_OK)
		die("css_select_ctx_count_sheets", code);
	printf("created selection context with %i sheets\n", count);


	/* select style for each of h1 to h6 */
	for (hh = 1; hh != 7; hh++) {
		css_select_results *style;
		char element[20];
		lwc_string *element_name;
		uint8_t color_type;
		css_color color_shade;

		/* in this very simple example our "document tree" is just one
		 * node and is in fact a libwapcaplet string containing the
		 * element name */
		snprintf(element, sizeof element, "h%i", hh);
		lwc_intern_string(element, strlen(element), &element_name);

		code = css_select_style(select_ctx, element_name,
				CSS_MEDIA_SCREEN, NULL,
				&select_handler, 0,
				&style);
		if (code != CSS_OK)
			die("css_select_style", code);

		lwc_string_unref(element_name);

		color_type = css_computed_color(
				style->styles[CSS_PSEUDO_ELEMENT_NONE],
				&color_shade);
		if (color_type == CSS_COLOR_INHERIT)
			printf("color of h%i is 'inherit'\n", hh);
		else
			printf("color of h%i is %x\n", hh, color_shade);

		code = css_select_results_destroy(style);
		if (code != CSS_OK)
			die("css_computed_style_destroy", code);
	}


	/* free everything and shut down libcss */
	code = css_select_ctx_destroy(select_ctx);
	if (code != CSS_OK)
		die("css_select_ctx_destroy", code);
	code = css_stylesheet_destroy(sheet);
	if (code != CSS_OK)
		die("css_stylesheet_destroy", code);

	return 0;
}


css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs)
{
	UNUSED(pw);
	UNUSED(base);

	/* About as useless as possible */
	*abs = lwc_string_ref(rel);

	return CSS_OK;
}


void die(const char *text, css_error code)
{
	fprintf(stderr, "ERROR: %s: %i: %s\n",
			text, code, css_error_to_string(code));
	exit(EXIT_FAILURE);
}



/* Select handlers. Our "document tree" is actually just a single node, which is
 * a libwapcaplet string containing the element name. Therefore all the
 * functions below except those getting or testing the element name return empty
 * data or false attributes. */
css_error node_name(void *pw, void *n, css_qname *qname)
{
	lwc_string *node = n;

	UNUSED(pw);
	
	qname->name = lwc_string_ref(node);
	
	return CSS_OK;
}

css_error node_classes(void *pw, void *n,
		lwc_string ***classes, uint32_t *n_classes)
{
	UNUSED(pw);
	UNUSED(n);
	*classes = NULL;
	*n_classes = 0;
	return CSS_OK;
}

css_error node_id(void *pw, void *n, lwc_string **id)
{
	UNUSED(pw);
	UNUSED(n);
	*id = NULL;
	return CSS_OK;
}

css_error named_ancestor_node(void *pw, void *n,
		const css_qname *qname,
		void **ancestor)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*ancestor = NULL;
	return CSS_OK;
}

css_error named_parent_node(void *pw, void *n,
		const css_qname *qname,
		void **parent)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*parent = NULL;
	return CSS_OK;
}

css_error named_generic_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*sibling = NULL;
	return CSS_OK;
}

css_error named_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*sibling = NULL;
	return CSS_OK;
}

css_error parent_node(void *pw, void *n, void **parent)
{
	UNUSED(pw);
	UNUSED(n);
	*parent = NULL;
	return CSS_OK;
}

css_error sibling_node(void *pw, void *n, void **sibling)
{
	UNUSED(pw);
	UNUSED(n);
	*sibling = NULL;
	return CSS_OK;
}

css_error node_has_name(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	lwc_string *node = n;
	UNUSED(pw);
	assert(lwc_string_caseless_isequal(node, qname->name, match) ==
			lwc_error_ok);
	return CSS_OK;
}

css_error node_has_class(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(name);
	*match = false;
	return CSS_OK;
}

css_error node_has_id(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(name);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_equal(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_dashmatch(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_includes(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_prefix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_suffix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_has_attribute_substring(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(qname);
	UNUSED(value);
	*match = false;
	return CSS_OK;
}

css_error node_is_first_child(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_root(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_count_siblings(void *pw, void *n,
		bool same_name, bool after, int32_t *count)
{
	UNUSED(pw);
	UNUSED(n);
	UNUSED(same_name);
	UNUSED(after);
	*count = 1;
	return CSS_OK;
}

css_error node_is_empty(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_link(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_visited(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_hover(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_active(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_focus(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_enabled(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_disabled(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_checked(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_target(void *pw, void *n, bool *match)
{
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}


css_error node_is_lang(void *pw, void *n,
		lwc_string *lang,
		bool *match)
{
	UNUSED(pw);
	UNUSED(n);
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
		hint->data.color = 0x00000000;
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
	UNUSED(pw);
	UNUSED(n);

	/* Since we're not storing it, ensure node data gets deleted */
	css_libcss_node_data_handler(&select_handler, CSS_NODE_DELETED,
			pw, n, NULL, libcss_node_data);

	return CSS_OK;
}

static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data)
{
	UNUSED(pw);
	UNUSED(n);
	*libcss_node_data = NULL;

	return CSS_OK;
}


