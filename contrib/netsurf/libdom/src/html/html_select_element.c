/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/html/html_option_element.h>
#include <dom/html/html_options_collection.h>

#include "html/html_document.h"
#include "html/html_select_element.h"

#include "core/node.h"
#include "utils/utils.h"

static struct dom_element_protected_vtable _protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_HTML_SELECT_ELEMENT
	},
	DOM_HTML_SELECT_ELEMENT_PROTECT_VTABLE
};

static bool is_option(struct dom_node_internal *node, void *ctx);

/**
 * Create a dom_html_select_element object
 *
 * \param doc  The document object
 * \param ele  The returned element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_select_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_select_element **ele)
{
	struct dom_node_internal *node;

	*ele = malloc(sizeof(dom_html_select_element));
	if (*ele == NULL)
		return DOM_NO_MEM_ERR;
	
	/* Set up vtables */
	node = (struct dom_node_internal *) *ele;
	node->base.vtable = &_dom_html_element_vtable;
	node->vtable = &_protect_vtable;

	return _dom_html_select_element_initialise(doc, namespace, prefix, *ele);
}

/**
 * Initialise a dom_html_select_element object
 *
 * \param doc  The document object
 * \param ele  The dom_html_select_element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_select_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_select_element *ele)
{
	ele->form = NULL;

	return _dom_html_element_initialise(doc, &ele->base,
					    doc->memoised[hds_SELECT],
					    namespace, prefix);
}

/**
 * Finalise a dom_html_select_element object
 *
 * \param ele  The dom_html_select_element object
 */
void _dom_html_select_element_finalise(struct dom_html_select_element *ele)
{
	_dom_html_element_finalise(&ele->base);
}

/**
 * Destroy a dom_html_select_element object
 *
 * \param ele  The dom_html_select_element object
 */
void _dom_html_select_element_destroy(struct dom_html_select_element *ele)
{
	_dom_html_select_element_finalise(ele);
	free(ele);
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual function used to parse attribute value, see src/core/element.c
 * for detail */
dom_exception _dom_html_select_element_parse_attribute(dom_element *ele,
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
void _dom_virtual_html_select_element_destroy(dom_node_internal *node)
{
	_dom_html_select_element_destroy((struct dom_html_select_element *) node);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_select_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_html_element_copy(old, copy);
}

/*-----------------------------------------------------------------------*/
/* Public APIs */

static dom_exception _dom_html_select_element_make_collection(
	dom_html_select_element *ele,
	dom_html_options_collection **col)
{
	dom_html_document *doc = (dom_html_document *) dom_node_get_owner(ele);

	assert(doc != NULL);

	return _dom_html_options_collection_create(doc,
						   (dom_node_internal *) ele,
						   is_option, ele, col);
}

/**
 * Get the type of selection control
 *
 * \param ele   The Select element
 * \param type  Pointer to location to receive type
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_get_type(
		dom_html_select_element *ele, dom_string **type)
{
	dom_html_document *doc = (dom_html_document *) dom_node_get_owner(ele);
	dom_exception err;
	bool multiple;

	err = dom_html_select_element_get_multiple(ele, &multiple);
	if (err != DOM_NO_ERR)
		return err;

	if (multiple)
		*type = dom_string_ref(doc->memoised[hds_select_multiple]);
	else
		*type = dom_string_ref(doc->memoised[hds_select_one]);

	return DOM_NO_ERR;
}

/**
 * Get the ordinal index of the selected option
 *
 * \param ele    The element object
 * \param index  The returned index
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_get_selected_index(
		dom_html_select_element *ele, int32_t *index)
{
	dom_exception err;
	uint32_t idx, len;
	dom_node *option;
	bool selected;
	dom_html_options_collection *col;
	
	err = _dom_html_select_element_make_collection(ele, &col);

	err = dom_html_options_collection_get_length(col, &len);
	if (err != DOM_NO_ERR) {
		dom_html_options_collection_unref(col);
		return err;
	}

	for (idx = 0; idx < len; idx++) {
		err = dom_html_options_collection_item(col,
				idx, &option);
		if (err != DOM_NO_ERR) {
			dom_html_options_collection_unref(col);
			return err;
		}
		
		err = dom_html_option_element_get_selected(
				(dom_html_option_element *) option, &selected);

		dom_node_unref(option);

		if (err != DOM_NO_ERR) {
			dom_html_options_collection_unref(col);
			return err;
		}
		
		if (selected) {
			*index = idx;
			dom_html_options_collection_unref(col);
			return DOM_NO_ERR;
		}
	}

	*index = -1;

	dom_html_options_collection_unref(col);
	return DOM_NO_ERR;
}

/**
 * Set the ordinal index of the selected option
 *
 * \param ele    The element object
 * \param index  The new index
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_set_selected_index(
		dom_html_select_element *ele, int32_t index)
{
	UNUSED(ele);
	UNUSED(index);

	/** \todo Implement */
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the value of this form control
 *
 * \param ele    The select element
 * \param value  Pointer to location to receive value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_get_value(
		dom_html_select_element *ele, dom_string **value)
{
	dom_exception err;
	uint32_t idx, len;
	dom_node *option;
	bool selected;
	dom_html_options_collection *col;
	
	err = _dom_html_select_element_make_collection(ele, &col);
	if (err != DOM_NO_ERR)
		return err;

	err = dom_html_select_element_get_length(ele, &len);
	if (err != DOM_NO_ERR) {
		dom_html_options_collection_unref(col);
		return err;
	}

	for (idx = 0; idx < len; idx++) {
		err = dom_html_options_collection_item(col,
				idx, &option);
		if (err != DOM_NO_ERR) {
			dom_html_options_collection_unref(col);
			return err;
		}

		err = dom_html_option_element_get_selected(
				(dom_html_option_element *) option, &selected);
		if (err != DOM_NO_ERR) {
			dom_html_options_collection_unref(col);
			dom_node_unref(option);
			return err;
		}

		if (selected) {
			err = dom_html_option_element_get_value(
					(dom_html_option_element *) option,
					value);

			dom_html_options_collection_unref(col);
			dom_node_unref(option);

			return err;
		}
	}

	*value = NULL;
	dom_html_options_collection_unref(col);
			
	return DOM_NO_ERR;
}

/**
 * Set the value of this form control
 *
 * \param ele    The select element
 * \param value  New value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_set_value(
		dom_html_select_element *ele, dom_string *value)
{
	UNUSED(ele);
	UNUSED(value);

	/** \todo Implement */
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the number of options in this select element
 *
 * \param ele  The element object
 * \param len  The returned len
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_get_length(
		dom_html_select_element *ele, uint32_t *len)
{
	dom_exception err;
	dom_html_options_collection *col;
	
	err = _dom_html_select_element_make_collection(ele, &col);
	if (err != DOM_NO_ERR)
		return err;

	err = dom_html_options_collection_get_length(col, len);
	
	dom_html_options_collection_unref(col);
	
	return err;
}

/**
 * Set the number of options in this select element
 *
 * \param ele  The element object
 * \param len  The new len
 * \return DOM_NOT_SUPPORTED_ERR.
 *
 * todo: how to deal with set the len of the children option objects?
 */
dom_exception dom_html_select_element_set_length(
		dom_html_select_element *ele, uint32_t len)
{
	UNUSED(ele);
	UNUSED(len);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the form associated with a select
 *
 * \param select  The dom_html_select_element object
 * \param form    Pointer to location to receive form
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_get_form(
	dom_html_select_element *select, dom_html_form_element **form)
{
	*form = select->form;

	if (*form != NULL)
		dom_node_ref(*form);

	return DOM_NO_ERR;
}

/**
 * The collection of OPTION elements of this element
 *
 * \param ele  The element object
 * \param col  THe returned collection object
 * \return DOM_NO_ERR on success.
 */
dom_exception dom__html_select_element_get_options(
		dom_html_select_element *ele,
		struct dom_html_options_collection **col)
{
	return _dom_html_select_element_make_collection(ele, col);
}

/**
 * Whether this element is disabled
 *
 * \param ele       The element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_get_disabled(
		dom_html_select_element *ele, bool *disabled)
{
	return dom_html_element_get_bool_property(&ele->base,
			"disabled", SLEN("disabled"), disabled);
}

/**
 * Set the disabled status of this element
 *
 * \param ele       The element object
 * \param disabled  The disabled status
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_set_disabled(
		dom_html_select_element *ele, bool disabled)
{
	return dom_html_element_set_bool_property(&ele->base,
			"disabled", SLEN("disabled"), disabled);
}

/**
 * Whether this element can be multiple selected
 *
 * \param ele       The element object
 * \param multiple  The returned status
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_get_multiple(
		dom_html_select_element *ele, bool *multiple)
{
	return dom_html_element_get_bool_property(&ele->base,
			"multiple", SLEN("multiple"), multiple);
}

/**
 * Set whether this element can be multiple selected
 *
 * \param ele       The element object
 * \param multiple  The status
 * \return DOM_NO_ERR on success.
 */
dom_exception dom_html_select_element_set_multiple(
		dom_html_select_element *ele, bool multiple)
{
	return dom_html_element_set_bool_property(&ele->base,
			"multiple", SLEN("multiple"), multiple);
}

/**
 * Get the name property
 *
 * \param ele   The select element
 * \param name  Pointer to location to receive name
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */ 
dom_exception dom_html_select_element_get_name(
		dom_html_select_element *ele, dom_string **name)
{
	dom_html_document *doc = (dom_html_document *) dom_node_get_owner(ele);

	return dom_element_get_attribute(ele,
			doc->memoised[hds_name], name);
}

/**
 * Set the name property
 *
 * \param ele   The select element
 * \param name  New name
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */ 
dom_exception dom_html_select_element_set_name(
		dom_html_select_element *ele, dom_string *name)
{
	dom_html_document *doc = (dom_html_document *) dom_node_get_owner(ele);

	return dom_element_set_attribute(ele,
			doc->memoised[hds_name], name);

}

/**
 * Get the size property
 *
 * \param ele   The select element
 * \param size  Pointer to location to receive size
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_get_size(
		dom_html_select_element *ele, int32_t *size)
{
	return dom_html_element_get_int32_t_property(&ele->base, "size",
			SLEN("size"), size);
}

/**
 * Set the size property
 *
 * \param ele   The select element
 * \param size  New size
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_set_size(
		dom_html_select_element *ele, int32_t size)
{
	return dom_html_element_set_int32_t_property(&ele->base, "size",
			SLEN("size"), size);
}

/**
 * Get the tabindex property
 *
 * \param ele        The select element
 * \param tab_index  Pointer to location to receive tab index
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_get_tab_index(
		dom_html_select_element *ele, int32_t *tab_index)
{
	return dom_html_element_get_int32_t_property(&ele->base, "tabindex",
			SLEN("tabindex"), tab_index);
}

/**
 * Set the tabindex property
 *
 * \param ele        The select element
 * \param tab_index  New tab index
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_select_element_set_tab_index(
		dom_html_select_element *ele, int32_t tab_index)
{
	return dom_html_element_set_int32_t_property(&ele->base, "tabindex",
			SLEN("tabindex"), tab_index);
}


/* Functions */
dom_exception dom__html_select_element_add(dom_html_select_element *select,
		struct dom_html_element *ele, struct dom_html_element *before)
{
	UNUSED(select);
	UNUSED(ele);
	UNUSED(before);

	/** \todo Implement */
	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception dom_html_select_element_remove(dom_html_select_element *ele,
		int32_t index)
{
	dom_exception err;
	uint32_t len;

	err = dom_html_select_element_get_length(ele, &len);
	if (err != DOM_NO_ERR)
		return err;

	/* Ensure index is in range */
	if (index < 0 || (uint32_t)index >= len)
		return DOM_NO_ERR;

	/** \todo What does remove mean? Remove option from tree and destroy it? */
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Blur this control
 *
 * \param ele  Element to blur
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_select_element_blur(struct dom_html_select_element *ele)
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
 * \param ele  Element to focus
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_select_element_focus(struct dom_html_select_element *ele)
{
	struct dom_html_document *doc =
		(dom_html_document *) dom_node_get_owner(ele);
	bool success = false;
	assert(doc != NULL);

	/** \todo Is this event (a) default (b) bubbling and (c) cancelable? */
	return _dom_dispatch_generic_event((dom_document *) doc,
					   (dom_event_target *) ele,
					   doc->memoised[hds_focus], true,
					   true, &success);
}


/*-----------------------------------------------------------------------*/
/* Helper functions */

/* Test whether certain node is an option node */
bool is_option(struct dom_node_internal *node, void *ctx)
{
	dom_html_select_element *ele = ctx;
	dom_html_document *doc = (dom_html_document *) dom_node_get_owner(ele);
	
	if (dom_string_isequal(node->name, doc->memoised[hds_OPTION]))
		return true;

	return false;
}

dom_exception _dom_html_select_element_set_form(
	dom_html_select_element *select, dom_html_form_element *form)
{
	select->form = form;

	return DOM_NO_ERR;
}

