/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/html/html_input_element.h>

#include "html/html_document.h"
#include "html/html_input_element.h"

#include "core/node.h"
#include "core/attr.h"
#include "utils/utils.h"

static struct dom_element_protected_vtable _protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_HTML_INPUT_ELEMENT
	},
	DOM_HTML_INPUT_ELEMENT_PROTECT_VTABLE
};

/**
 * Create a dom_html_input_element object
 *
 * \param doc  The document object
 * \param ele  The returned element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_input_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_input_element **ele)
{
	struct dom_node_internal *node;

	*ele = malloc(sizeof(dom_html_input_element));
	if (*ele == NULL)
		return DOM_NO_MEM_ERR;

	/* Set up vtables */
	node = (struct dom_node_internal *) *ele;
	node->base.vtable = &_dom_html_element_vtable;
	node->vtable = &_protect_vtable;

	return _dom_html_input_element_initialise(doc, namespace, prefix, *ele);
}

/**
 * Initialise a dom_html_input_element object
 *
 * \param doc  The document object
 * \param ele  The dom_html_input_element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_input_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_input_element *ele)
{
	ele->form = NULL;
	ele->default_checked = false;
	ele->default_checked_set = false;
	ele->default_value = NULL;
	ele->default_value_set = false;

	return _dom_html_element_initialise(doc, &ele->base,
					    doc->memoised[hds_INPUT],
					    namespace, prefix);
}

/**
 * Finalise a dom_html_input_element object
 *
 * \param ele  The dom_html_input_element object
 */
void _dom_html_input_element_finalise(struct dom_html_input_element *ele)
{
	if (ele->default_value != NULL) {
		dom_string_unref(ele->default_value);
		ele->default_value = NULL;
	}

	_dom_html_element_finalise(&ele->base);
}

/**
 * Destroy a dom_html_input_element object
 *
 * \param ele  The dom_html_input_element object
 */
void _dom_html_input_element_destroy(struct dom_html_input_element *ele)
{
	_dom_html_input_element_finalise(ele);
	free(ele);
}

/*-----------------------------------------------------------------------*/
/* Public APIs */

/**
 * Get the disabled property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_get_disabled(dom_html_input_element *ele,
		bool *disabled)
{
	return dom_html_element_get_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Set the disabled property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_set_disabled(dom_html_input_element *ele,
		bool disabled)
{
	return dom_html_element_set_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Get the readOnly property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_get_read_only(dom_html_input_element *ele,
		bool *read_only)
{
	return dom_html_element_get_bool_property(&ele->base, "readonly",
			SLEN("readonly"), read_only);
}

/**
 * Set the readOnly property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_set_read_only(dom_html_input_element *ele,
		bool read_only)
{
	return dom_html_element_set_bool_property(&ele->base, "readonly",
			SLEN("readonly"), read_only);
}

/**
 * Get the checked property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_get_checked(dom_html_input_element *ele,
		bool *checked)
{
	return dom_html_element_get_bool_property(&ele->base, "checked",
			SLEN("checked"), checked);
}

/**
 * Set the checked property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_set_checked(dom_html_input_element *ele,
		bool checked)
{
	return dom_html_element_set_bool_property(&ele->base, "checked",
			SLEN("checked"), checked);
}

/**
 * Get the defaultValue property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_get_default_value(
	dom_html_input_element *ele, dom_string **default_value)
{
	*default_value = ele->default_value;

	if (*default_value != NULL)
		dom_string_ref(*default_value);

	return DOM_NO_ERR;
}

/**
 * Set the defaultValue property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_set_default_value(
	dom_html_input_element *ele, dom_string *default_value)
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
 * Get the defaultChecked property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_get_default_checked(
	dom_html_input_element *ele, bool *default_checked)
{
	*default_checked = ele->default_checked;

	return DOM_NO_ERR;
}

/**
 * Set the defaultChecked property
 *
 * \param ele       The dom_html_input_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_set_default_checked(
	dom_html_input_element *ele, bool default_checked)
{
	ele->default_checked = default_checked;
	ele->default_checked_set = true;

	return DOM_NO_ERR;
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual function used to parse attribute value, see src/core/element.c
 * for detail */
dom_exception _dom_html_input_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	dom_html_input_element *input = (dom_html_input_element *)ele;
	dom_html_document *html = (dom_html_document *)(ele->base.owner);

	/** \todo Find some way to do the equiv for default_checked to be
	 * false instead of true.  Some end-tag hook in the binding perhaps?
	 */
	if (dom_string_caseless_isequal(name, html->memoised[hds_checked])) {
		if (input->default_checked_set == false) {
			input->default_checked = true;
			input->default_checked_set = true;
		}
	}

	if (dom_string_caseless_isequal(name, html->memoised[hds_value])) {
		if (input->default_value_set == false) {
			input->default_value = value;
			dom_string_ref(value);
			input->default_value_set = true;
		}
	}

	dom_string_ref(value);
	*parsed = value;

	return DOM_NO_ERR;
}

/* The virtual destroy function, see src/core/node.c for detail */
void _dom_virtual_html_input_element_destroy(dom_node_internal *node)
{
	_dom_html_input_element_destroy((struct dom_html_input_element *) node);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_input_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_html_element_copy(old, copy);
}

/*-----------------------------------------------------------------------*/
/* API functions */

#define SIMPLE_GET(attr)						\
	dom_exception dom_html_input_element_get_##attr(		\
		dom_html_input_element *element,			\
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
dom_exception dom_html_input_element_set_##attr(			\
		dom_html_input_element *element,			\
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

SIMPLE_GET_SET(accept);
SIMPLE_GET_SET(access_key);
SIMPLE_GET_SET(align);
SIMPLE_GET_SET(alt);
SIMPLE_GET_SET(name);
SIMPLE_GET_SET(size);
SIMPLE_GET_SET(src);
SIMPLE_GET(type);
SIMPLE_GET_SET(use_map);
SIMPLE_GET_SET(value);

dom_exception dom_html_input_element_get_tab_index(
	dom_html_input_element *input, int32_t *tab_index)
{
	return dom_html_element_get_int32_t_property(&input->base, "tabindex",
			SLEN("tabindex"), tab_index);
}

dom_exception dom_html_input_element_set_tab_index(
	dom_html_input_element *input, uint32_t tab_index)
{
	return dom_html_element_set_int32_t_property(&input->base, "tabindex",
			SLEN("tabindex"), tab_index);
}

dom_exception dom_html_input_element_get_max_length(
	dom_html_input_element *input, int32_t *max_length)
{
	return dom_html_element_get_int32_t_property(&input->base, "maxlength",
			SLEN("maxlength"), max_length);
}

dom_exception dom_html_input_element_set_max_length(
	dom_html_input_element *input, uint32_t max_length)
{
	return dom_html_element_set_int32_t_property(&input->base, "maxlength",
			SLEN("maxlength"), max_length);
}

dom_exception dom_html_input_element_get_form(
	dom_html_input_element *input, dom_html_form_element **form)
{
	*form = input->form;

	if (*form != NULL)
		dom_node_ref(*form);

	return DOM_NO_ERR;
}

dom_exception _dom_html_input_element_set_form(
	dom_html_input_element *input, dom_html_form_element *form)
{
	input->form = form;

	return DOM_NO_ERR;
}

/**
 * Blur this control
 *
 * \param ele  The form object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_blur(dom_html_input_element *ele)
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
dom_exception dom_html_input_element_focus(dom_html_input_element *ele)
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
dom_exception dom_html_input_element_select(dom_html_input_element *ele)
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

/**
 * Click this control
 *
 * \param ele  The form object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_input_element_click(dom_html_input_element *ele)
{
	struct dom_html_document *doc =
		(dom_html_document *) dom_node_get_owner(ele);
	bool success = false;
	assert(doc != NULL);

	/** \todo Is this is meant to check/uncheck boxes, radios etc */
	/** \todo Is this event (a) default (b) bubbling and (c) cancelable? */
	return _dom_dispatch_generic_event((dom_document *)doc,
					   (dom_event_target *) ele,
					   doc->memoised[hds_click], true,
					   true, &success);
}

