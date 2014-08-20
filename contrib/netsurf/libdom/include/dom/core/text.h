/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_core_text_h_
#define dom_core_text_h_

#include <stdbool.h>

#include <dom/core/exceptions.h>
#include <dom/core/characterdata.h>

struct dom_characterdata;

typedef struct dom_text dom_text;

typedef struct dom_text_vtable {
	struct dom_characterdata_vtable base;

	dom_exception (*dom_text_split_text)(struct dom_text *text,
			uint32_t offset, struct dom_text **result);
	dom_exception (*dom_text_get_is_element_content_whitespace)(
			struct dom_text *text, bool *result);
	dom_exception (*dom_text_get_whole_text)(struct dom_text *text,
			dom_string **result);
	dom_exception (*dom_text_replace_whole_text)(struct dom_text *text,
			dom_string *content, struct dom_text **result);
} dom_text_vtable;

static inline dom_exception dom_text_split_text(struct dom_text *text,
		uint32_t offset, struct dom_text **result)
{
	return ((dom_text_vtable *) ((dom_node *) text)->vtable)->
			dom_text_split_text(text, offset, result);
}
#define dom_text_split_text(t, o, r) dom_text_split_text((dom_text *) (t), \
		(uint32_t) (o), (dom_text **) (r))

static inline dom_exception dom_text_get_is_element_content_whitespace(
		struct dom_text *text, bool *result)
{
	return ((dom_text_vtable *) ((dom_node *) text)->vtable)->
			dom_text_get_is_element_content_whitespace(text, 
			result);
}
#define dom_text_get_is_element_content_whitespace(t, r) \
		dom_text_get_is_element_content_whitespace((dom_text *) (t), \
		(bool *) (r))

static inline dom_exception dom_text_get_whole_text(struct dom_text *text,
		dom_string **result)
{
	return ((dom_text_vtable *) ((dom_node *) text)->vtable)->
			dom_text_get_whole_text(text, result);
}
#define dom_text_get_whole_text(t, r) dom_text_get_whole_text((dom_text *) (t), (r))

static inline dom_exception dom_text_replace_whole_text(struct dom_text *text,
		dom_string *content, struct dom_text **result)
{
	return ((dom_text_vtable *) ((dom_node *) text)->vtable)->
			dom_text_replace_whole_text(text, content, result);
}
#define dom_text_replace_whole_text(t, c, r) dom_text_replace_whole_text( \
		(dom_text *) (t), (c), (dom_text **) (r))

#endif
