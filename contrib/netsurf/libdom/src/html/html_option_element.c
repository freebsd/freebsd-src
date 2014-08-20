/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <stdlib.h>

#include <dom/dom.h>
#include <dom/html/html_option_element.h>
#include <dom/html/html_select_element.h>

#include "html/html_document.h"
#include "html/html_option_element.h"

#include "core/node.h"
#include "core/attr.h"
#include "utils/utils.h"

static struct dom_element_protected_vtable _protect_vtable = {
	{
		DOM_NODE_PROTECT_VTABLE_HTML_OPTION_ELEMENT
	},
	DOM_HTML_OPTION_ELEMENT_PROTECT_VTABLE
};

/**
 * Create a dom_html_option_element object
 *
 * \param doc  The document object
 * \param ele  The returned element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_option_element_create(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_option_element **ele)
{
	struct dom_node_internal *node;

	*ele = malloc(sizeof(dom_html_option_element));
	if (*ele == NULL)
		return DOM_NO_MEM_ERR;

	/* Set up vtables */
	node = (struct dom_node_internal *) *ele;
	node->base.vtable = &_dom_html_element_vtable;
	node->vtable = &_protect_vtable;

	return _dom_html_option_element_initialise(doc, namespace, prefix, *ele);
}

/**
 * Initialise a dom_html_option_element object
 *
 * \param doc  The document object
 * \param ele  The dom_html_option_element object
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception _dom_html_option_element_initialise(struct dom_html_document *doc,
		dom_string *namespace, dom_string *prefix,
		struct dom_html_option_element *ele)
{
	ele->default_selected = false;
	ele->default_selected_set = false;

	return _dom_html_element_initialise(doc, &ele->base,
					    doc->memoised[hds_OPTION],
					    namespace, prefix);
}

/**
 * Finalise a dom_html_option_element object
 *
 * \param ele  The dom_html_option_element object
 */
void _dom_html_option_element_finalise(struct dom_html_option_element *ele)
{
	_dom_html_element_finalise(&ele->base);
}

/**
 * Destroy a dom_html_option_element object
 *
 * \param ele  The dom_html_option_element object
 */
void _dom_html_option_element_destroy(struct dom_html_option_element *ele)
{
	_dom_html_option_element_finalise(ele);
	free(ele);
}

/*-----------------------------------------------------------------------*/
/* Public APIs */

dom_exception dom_html_option_element_get_form(
	dom_html_option_element *option, dom_html_form_element **form)
{
	dom_html_document *doc;
	dom_node_internal *select = ((dom_node_internal *) option)->parent;

	doc = (dom_html_document *) ((dom_node_internal *) option)->owner;

	/* Search ancestor chain for SELECT element */
	while (select != NULL) {
		if (select->type == DOM_ELEMENT_NODE &&
				dom_string_caseless_isequal(select->name,
						doc->memoised[hds_SELECT]))
			break;

		select = select->parent;
	}

	if (select != NULL) {
		return dom_html_select_element_get_form((dom_html_select_element *) select,
				form);
	}

	*form = NULL;

	return DOM_NO_ERR;
}

/**
 * Get the defaultSelected property
 *
 * \param option            The dom_html_option_element object
 * \param default_selected  Pointer to location to receive value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_get_default_selected(
	dom_html_option_element *option, bool *default_selected)
{
	*default_selected = option->default_selected;

	return DOM_NO_ERR;
}

/**
 * Set the defaultSelected property
 *
 * \param option            The dom_html_option_element object
 * \param default_selected  New value for property
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_set_default_selected(
	dom_html_option_element *option, bool default_selected)
{
	option->default_selected = default_selected;
	option->default_selected_set = true;

	return DOM_NO_ERR;
}

/**
 * Helper for dom_html_option_element_get_text
 */
static dom_exception dom_html_option_element_get_text_node(
	dom_node_internal *n, dom_string **text)
{
	dom_string *node_name = NULL;
	dom_string *node_ns = NULL;
	dom_document *owner = NULL;
	dom_string *str = NULL;
	dom_string *ret = NULL;
	dom_exception exc;

	*text = NULL;

	assert(n->owner != NULL);
	owner = n->owner;

	for (n = n->first_child; n != NULL; n = n->next) {
		/* Skip irrelevent node types */
		if (n->type == DOM_COMMENT_NODE ||
		    n->type == DOM_PROCESSING_INSTRUCTION_NODE)
			continue;

		if (n->type == DOM_ELEMENT_NODE) {
			/* Skip script elements with html or svg namespace */
			exc = dom_node_get_local_name(n, &node_name);
			if (exc != DOM_NO_ERR)
				return exc;
			if (dom_string_caseless_isequal(node_name,
					owner->script_string)) {
				exc = dom_node_get_namespace(n, &node_ns);
				if (exc != DOM_NO_ERR) {
					dom_string_unref(node_name);
					return exc;
				}
				if (dom_string_caseless_isequal(node_ns,
						dom_namespaces[
							DOM_NAMESPACE_HTML]) ||
				    dom_string_caseless_isequal(node_ns,
						dom_namespaces[
							DOM_NAMESPACE_SVG])) {
					dom_string_unref(node_name);
					dom_string_unref(node_ns);
					continue;
				}
				dom_string_unref(node_ns);
			}
			dom_string_unref(node_name);

			/* Get text inside child node 'n' */
			dom_html_option_element_get_text_node(n,
					(str == NULL) ? &str : &ret);
		} else {
			/* Handle other nodes with their get_text_content
			 * specialisation */
			dom_node_get_text_content(n,
					(str == NULL) ? &str : &ret);
		}

		/* If we already have text, concatenate it */
		if (ret != NULL) {
			dom_string *new_str;
			dom_string_concat(str, ret, &new_str);
			dom_string_unref(str);
			dom_string_unref(ret);
			str = new_str;
		}
	}

	/* Strip and collapse whitespace */
	if (str != NULL) {
		dom_string_whitespace_op(str,
				DOM_WHITESPACE_STRIP_COLLAPSE, text);
		dom_string_unref(str);
	}

	return DOM_NO_ERR;
}

/**
 * Get the text contained in the option
 *
 * \param option  The dom_html_option_element object
 * \param text    Pointer to location to receive text
 * \return DOM_NO_ERR on success, appropriate error otherwise
 */
dom_exception dom_html_option_element_get_text(
	dom_html_option_element *option, dom_string **text)
{
	return dom_html_option_element_get_text_node(
			(dom_node_internal *) option, text);
}

/**
 * Obtain the index of this option in its parent
 * 
 * \param option  The dom_html_option_element object
 * \param index   Pointer to receive zero-based index
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_get_index(
	dom_html_option_element *option, unsigned long *index)
{
	UNUSED(option);
	UNUSED(index);

	/** \todo Implement */
	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Get the disabled property
 *
 * \param ele       The dom_html_option_element object
 * \param disabled  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_option_element_get_disabled(dom_html_option_element *ele,
		bool *disabled)
{
	return dom_html_element_get_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Set the disabled property
 *
 * \param ele       The dom_html_option_element object
 * \param disabled  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_option_element_set_disabled(dom_html_option_element *ele,
		bool disabled)
{
	return dom_html_element_set_bool_property(&ele->base, "disabled",
			SLEN("disabled"), disabled);
}

/**
 * Get the label property
 *
 * \param option  The dom_html_option_element object
 * \param label   Pointer to location to receive label
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_get_label(
	dom_html_option_element *option, dom_string **label)
{
	dom_html_document *doc;

	doc = (dom_html_document *) ((dom_node_internal *) option)->owner;

	return dom_element_get_attribute(option,
			doc->memoised[hds_label], label);
}

/**
 * Set the label property
 *
 * \param option  The dom_html_option_element object
 * \param label   Label value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_set_label(
	dom_html_option_element *option, dom_string *label)
{
	dom_html_document *doc;

	doc = (dom_html_document *) ((dom_node_internal *) option)->owner;

	return dom_element_set_attribute(option,
			doc->memoised[hds_label], label);
}

/**
 * Get the selected property
 *
 * \param ele       The dom_html_option_element object
 * \param selected  The returned status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_option_element_get_selected(dom_html_option_element *ele,
		bool *selected)
{
	return dom_html_element_get_bool_property(&ele->base, "selected",
			SLEN("selected"), selected);
}

/**
 * Set the selected property
 *
 * \param ele       The dom_html_option_element object
 * \param selected  The status
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_html_option_element_set_selected(dom_html_option_element *ele,
		bool selected)
{
	return dom_html_element_set_bool_property(&ele->base, "selected",
			SLEN("selected"), selected);
}

/**
 * Get the value property
 *
 * \param option  The dom_html_option_element object
 * \param value   Pointer to location to receive property value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_get_value(
	dom_html_option_element *option, dom_string **value)
{
	dom_html_document *doc;
	bool has_value = false;
	dom_exception err;

	doc = (dom_html_document *) ((dom_node_internal *) option)->owner;

	err = dom_element_has_attribute(option, 
			doc->memoised[hds_value], &has_value);
	if (err != DOM_NO_ERR)
		return err;

	if (has_value) {
		return dom_element_get_attribute(option,
				doc->memoised[hds_value], value);
	}

	return dom_html_option_element_get_text(option, value);
}

/**
 * Set the value property
 *
 * \param option  The dom_html_option_element object
 * \param value   Property value
 * \return DOM_NO_ERR on success, appropriate error otherwise.
 */
dom_exception dom_html_option_element_set_value(
	dom_html_option_element *option, dom_string *value)
{
	dom_html_document *doc;

	doc = (dom_html_document *) ((dom_node_internal *) option)->owner;

	return dom_element_set_attribute(option,
			doc->memoised[hds_value], value);
}

/*------------------------------------------------------------------------*/
/* The protected virtual functions */

/* The virtual function used to parse attribute value, see src/core/element.c
 * for detail */
dom_exception _dom_html_option_element_parse_attribute(dom_element *ele,
		dom_string *name, dom_string *value,
		dom_string **parsed)
{
	dom_html_option_element *option = (dom_html_option_element *)ele;
	dom_html_document *html = (dom_html_document *)(ele->base.owner);

	/** \todo Find some way to do the equiv for default_selected to be
	 * false instead of true.  Some end-tag hook in the binding perhaps?
	 */
	if (dom_string_caseless_isequal(name, html->memoised[hds_selected])) {
		if (option->default_selected_set == false) {
			option->default_selected = true;
			option->default_selected_set = true;
		}
	}

	dom_string_ref(value);
	*parsed = value;

	return DOM_NO_ERR;
}

/* The virtual destroy function, see src/core/node.c for detail */
void _dom_virtual_html_option_element_destroy(dom_node_internal *node)
{
	_dom_html_option_element_destroy((struct dom_html_option_element *) node);
}

/* The virtual copy function, see src/core/node.c for detail */
dom_exception _dom_html_option_element_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	return _dom_html_element_copy(old, copy);
}

