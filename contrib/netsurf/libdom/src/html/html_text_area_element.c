/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/html/html_text_area_element.h>

#include "html/html_document.h"
#include "html/html_text_area_element.h"

#include "core/node.h"
#include "core/attr.h"
#include "utils/utils.h"

static struct dom_element_protected_vtable _protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_HTML_TEXT_AREA_ELEMENT
	},
	DOM_HTML_TEXT_AREA_ELEMENT_PROTECT_VTABLE
};

/**
 * Create a dom_html_text_area_element object
 *
 * \param doc  The document object
 * \param ele  The returned element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_text_area_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_text_area_element **ele)
{
	struct dom_node_internal *node;

	*ele = malloc(sizeof(dom_html_text_area_element));
	if (*ele == NULL)
		return DOM_NO_MEM_ERR;

	/* Set up vtables */
	node = (struct dom_node_internal *) *ele;
	node->base.vtable = &_dom_html_element_vtable;
	node->vtable = &_protect_vtable;

	return _dom_html_text_area_element_initialise(doc, namespace, prefix, *ele);
}

/**
 * Initialise a dom_html_text_area_element object
 *
 * \param doc  The document object
 * \param ele  The dom_html_text_area_element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_text_area_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_text_area_element *ele)
{
	ele->form = NULL;
	ele->default_value = NULL;
	ele->default_value_set = false;
	ele->value = NULL;
	ele->value_set = false;

	return _dom_html_element_initialise(doc, &ele->base,
					    doc->memoised[hds_TEXTAREA],
					    namespace, prefix);
}

/**
 * Finalise a dom_html_text_area_element object
 *
 * \param ele  The dom_html_text_area_element object
 */
void _dom_html_text_area_element_finalise(struct dom_html_text_area_element *ele)
{
	if (ele->default_value != NULL) {
		dom_string_unref(ele->default_value);
		ele->default_value = NULL;
		ele->default_value_set = false;
	}

	if (ele->value != NULL) {
		dom_string_unref(ele->value);
		ele->value = NULL;
		ele->value_set = false;
	}

	_dom_html_element_finalise(&ele->base);
}

/**
 * Destroy a dom_html_text_area_element object
 *
 * \param ele  The dom_html_text_area_element object
 */
void _dom_html_text_area_element_destroy(struct dom_html_text_area_element *ele)
{
	_dom_html_text_area_element_finalise(ele);
	free(ele);
}

/*-----------------------------------------------------------------------*/
/* Public APIs */

/**
 * Get the disabled property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_get_disabled(dom_html_text_area_element *ele,
		bool *disabled)
{
	return dom_html_element_get_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Set the disabled property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_set_disabled(dom_html_text_area_element *ele,
		bool disabled)
{
	return dom_html_element_set_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Get the readOnly property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_get_read_only(dom_html_text_area_element *ele,
		bool *read_only)
{
	return dom_html_element_get_bool_property(&ele->base, "readonly",
			SLEN("readonly"), read_only);
}

/**
 * Set the readOnly property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_set_read_only(dom_html_text_area_element *ele,
		bool read_only)
{
	return dom_html_element_set_bool_property(&ele->base, "readonly",
			SLEN("readonly"), read_only);
}

/**
 * Get the defaultValue property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_get_default_value(
	dom_html_text_area_element *ele, dom_string **default_value)
{
	dom_exception err;

	if (ele->default_value_set == false) {
		err = dom_node_get_text_content((dom_node *)ele,
						&ele->default_value);
		if (err == DOM_NO_ERR) {
			ele->default_value_set = true;
		}
	}

	*default_value = ele->default_value;

	if (*default_value != NULL)
		dom_string_ref(*default_value);

	return DOM_NO_ERR;
}

/**
 * Set the defaultValue property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_set_default_value(
	dom_html_text_area_element *ele, dom_string *default_value)
{
	if (ele->default_value != NULL)
		dom_string_unref(ele->default_value);

	ele->default_value = default_value;
	ele->default_value_set = true;

	if (ele->default_value != NULL)
		dom_string_ref(ele->default_value);

	return DOM_NO_ERR;
}

/**
 * Get the value property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_get_value(
	dom_html_text_area_element *ele, dom_string **value)
{
	dom_exception err;

	if (ele->value_set == false) {
		err = dom_node_get_text_content((dom_node *)ele,
						&ele->value);
		if (err == DOM_NO_ERR) {
			ele->default_value_set = true;
			if (ele->default_value_set == false) {
				ele->default_value_set = true;
				ele->default_value = ele->value;
				dom_string_ref(ele->default_value);
			}
		}
	}

	*value = ele->value;

	if (*value != NULL)
		dom_string_ref(*value);

	return DOM_NO_ERR;
}

/**
 * Set the value property
 *
 * \param ele       The dom_html_text_area_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_set_value(
	dom_html_text_area_element *ele, dom_string *value)
{
	dom_exception err;

	if (ele->default_value_set == false) {
		err = dom_node_get_text_content((dom_node *)ele,
						&ele->default_value);
		if (err == DOM_NO_ERR) {
			ele->default_value_set = true;
		}
	}

	if (ele->value != NULL)
		dom_string_unref(ele->value);

	ele->value = value;
	ele->value_set = true;

	if (ele->value != NULL)
		dom_string_ref(ele->value);

	return DOM_NO_ERR;
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual function used to parse attribute value, see src/core/element.c
 * for detail */
dom_exception _dom_html_text_area_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	UNUSED(ele);
	UNUSED(name);

	dom_string_ref(value);
	*parsed = value;

	return DOM_NO_ERR;
}

/* The virtual destroy function, see src/core/node.c for detail */
void _dom_virtual_html_text_area_element_destroy(dom_node_internal *node)
{
	_dom_html_text_area_element_destroy((struct dom_html_text_area_element *) node);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_text_area_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_html_element_copy(old, copy);
}

/*-----------------------------------------------------------------------*/
/* API functions */

#define SIMPLE_GET(attr)						\
	dom_exception dom_html_text_area_element_get_##attr(		\
		dom_html_text_area_element *element,			\
		dom_string **attr)					\
	{								\
		dom_exception ret;					\
		dom_string *_memo_##attr;				\
									\
		_memo_##attr =						\
			((struct dom_html_document *)			\
			 ((struct dom_node_internal *)element)->owner)->\
			memoised[hds_##attr];				\
									\
		ret = dom_element_get_attribute(element, _memo_##attr, attr); \
									\
		return ret;						\
	}
#define SIMPLE_SET(attr)						\
dom_exception dom_html_text_area_element_set_##attr(			\
		dom_html_text_area_element *element,			\
		dom_string *attr)					\
	{								\
		dom_exception ret;					\
		dom_string *_memo_##attr;				\
									\
		_memo_##attr =						\
			((struct dom_html_document *)			\
			 ((struct dom_node_internal *)element)->owner)->\
			memoised[hds_##attr];				\
									\
		ret = dom_element_set_attribute(element, _memo_##attr, attr); \
									\
		return ret;						\
	}

#define SIMPLE_GET_SET(attr) SIMPLE_GET(attr) SIMPLE_SET(attr)

SIMPLE_GET_SET(access_key);
SIMPLE_GET_SET(name);

dom_exception dom_html_text_area_element_get_type(
	dom_html_text_area_element *text_area, dom_string **type)
{
	dom_html_document *html = (dom_html_document *)
		((dom_node_internal *)text_area)->owner;

	*type = html->memoised[hds_textarea];
	dom_string_ref(*type);

	return DOM_NO_ERR;
}

dom_exception dom_html_text_area_element_get_tab_index(
	dom_html_text_area_element *text_area, int32_t *tab_index)
{
	return dom_html_element_get_int32_t_property(&text_area->base, "tabindex",
			SLEN("tabindex"), tab_index);
}

dom_exception dom_html_text_area_element_set_tab_index(
	dom_html_text_area_element *text_area, uint32_t tab_index)
{
	return dom_html_element_set_int32_t_property(&text_area->base, "tabindex",
			SLEN("tabindex"), tab_index);
}

dom_exception dom_html_text_area_element_get_cols(
	dom_html_text_area_element *text_area, int32_t *cols)
{
	return dom_html_element_get_int32_t_property(&text_area->base, "cols",
			SLEN("cols"), cols);
}

dom_exception dom_html_text_area_element_set_cols(
	dom_html_text_area_element *text_area, uint32_t cols)
{
	return dom_html_element_set_int32_t_property(&text_area->base, "cols",
			SLEN("cols"), cols);
}

dom_exception dom_html_text_area_element_get_rows(
	dom_html_text_area_element *text_area, int32_t *rows)
{
	return dom_html_element_get_int32_t_property(&text_area->base, "rows",
			SLEN("rows"), rows);
}

dom_exception dom_html_text_area_element_set_rows(
	dom_html_text_area_element *text_area, uint32_t rows)
{
	return dom_html_element_set_int32_t_property(&text_area->base, "rows",
			SLEN("rows"), rows);
}

dom_exception dom_html_text_area_element_get_form(
	dom_html_text_area_element *text_area, dom_html_form_element **form)
{
	*form = text_area->form;

	if (*form != NULL)
		dom_node_ref(*form);

	return DOM_NO_ERR;
}

dom_exception _dom_html_text_area_element_set_form(
	dom_html_text_area_element *text_area, dom_html_form_element *form)
{
	text_area->form = form;

	return DOM_NO_ERR;
}

/**
 * Blur this control
 *
 * \param ele  The form object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_blur(dom_html_text_area_element *ele)
{
	struct dom_html_document *doc =
		(dom_html_document *) dom_node_get_owner(ele);
	bool success = false;
	assert(doc != NULL);

	/** \todo Is this event (a) default (b) bubbling and (c) cancelable? */
	return _dom_dispatch_generic_event((dom_document *) doc,
					   (dom_event_target *) ele,
					   doc->memoised[hds_blur], true,
					   true, &success);
}

/**
 * Focus this control
 *
 * \param ele  The form object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_focus(dom_html_text_area_element *ele)
{
	struct dom_html_document *doc =
		(dom_html_document *) dom_node_get_owner(ele);
	bool success = false;
	assert(doc != NULL);

	/** \todo Is this event (a) default (b) bubbling and (c) cancelable? */
	return _dom_dispatch_generic_event((dom_document *)doc,
					   (dom_event_target *) ele,
					   doc->memoised[hds_focus], true,
					   true, &success);
}

/**
 * Select this control
 *
 * \param ele  The form object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_text_area_element_select(dom_html_text_area_element *ele)
{
	struct dom_html_document *doc =
		(dom_html_document *) dom_node_get_owner(ele);
	bool success = false;
	assert(doc != NULL);

	/** \todo Is this event (a) default (b) bubbling and (c) cancelable? */
	return _dom_dispatch_generic_event((dom_document *)doc,
					   (dom_event_target *) ele,
					   doc->memoised[hds_select], true,
					   true, &success);
}
