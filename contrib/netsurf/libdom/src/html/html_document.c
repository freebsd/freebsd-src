/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <stdlib.h>

#include "html/html_document.h"
#include "html/html_element.h"
#include "html/html_collection.h"
#include "html/html_html_element.h"
#include "html/html_head_element.h"
#include "html/html_body_element.h"
#include "html/html_link_element.h"
#include "html/html_title_element.h"
#include "html/html_meta_element.h"
#include "html/html_form_element.h"
#include "html/html_button_element.h"
#include "html/html_input_element.h"
#include "html/html_text_area_element.h"
#include "html/html_opt_group_element.h"
#include "html/html_option_element.h"
#include "html/html_select_element.h"
#include "html/html_hr_element.h"

#include "core/attr.h"
#include "core/string.h"
#include "utils/namespace.h"
#include "utils/utils.h"

static struct dom_html_document_vtable html_document_vtable = {
	{
		{
			{
				DOM_NODE_EVENT_TARGET_VTABLE
			},
			DOM_NODE_VTABLE,
		},
		DOM_DOCUMENT_VTABLE_HTML
	},
	DOM_HTML_DOCUMENT_VTABLE
};

static struct dom_node_protect_vtable html_document_protect_vtable = {
	DOM_HTML_DOCUMENT_PROTECT_VTABLE
};

/* Create a HTMLDocument */
dom_exception _dom_html_document_create(
		dom_events_default_action_fetcher daf,
		void *daf_ctx,
		dom_html_document **doc)
{
	dom_exception error;
	dom_html_document *result;

	result = malloc(sizeof(dom_html_document));
	if (result == NULL)
		return DOM_NO_MEM_ERR;

	result->base.base.base.vtable = &html_document_vtable;
	result->base.base.vtable = &html_document_protect_vtable;
	
	error = _dom_html_document_initialise(result, daf, daf_ctx);
	if (error != DOM_NO_ERR) {
		free(result);
		return error;
	}

	*doc = result;
	return DOM_NO_ERR;
}

/* Initialise a HTMLDocument */
dom_exception _dom_html_document_initialise(dom_html_document *doc,
		dom_events_default_action_fetcher daf,
		void *daf_ctx)
{
	dom_exception error;
	int sidx;

	error = _dom_document_initialise(&doc->base, daf, daf_ctx);
	if (error != DOM_NO_ERR)
		return error;

	doc->title = NULL;
	doc->referrer = NULL;
	doc->domain = NULL;
	doc->url = NULL;
	doc->cookie = NULL;
	
	doc->memoised = calloc(sizeof(dom_string *), hds_COUNT);
	if (doc->memoised == NULL) {
		error = DOM_NO_MEM_ERR;
		goto out;
	}
	
#define HTML_DOCUMENT_STRINGS_ACTION(attr,str)                             \
	error = dom_string_create_interned((const uint8_t *) #str,	\
					   SLEN(#str), &doc->memoised[hds_##attr]); \
	if (error != DOM_NO_ERR) {					\
		goto out;						\
	}

#include "html_document_strings.h"
#undef HTML_DOCUMENT_STRINGS_ACTION

out:
	if (doc->memoised != NULL && error != DOM_NO_ERR) {
		for(sidx = 0; sidx < hds_COUNT; ++sidx) {
			if (doc->memoised[sidx] != NULL) {
				dom_string_unref(doc->memoised[sidx]);
			}
		}
		free(doc->memoised);
		doc->memoised = NULL;
	}
	return error;
}

/* Finalise a HTMLDocument */
bool _dom_html_document_finalise(dom_html_document *doc)
{
	int sidx;
	
	if (doc->cookie != NULL)
		dom_string_unref(doc->cookie);
	if (doc->url != NULL)
		dom_string_unref(doc->url);
	if (doc->domain != NULL)
		dom_string_unref(doc->domain);
	if (doc->referrer != NULL)
		dom_string_unref(doc->referrer);
	if (doc->title != NULL)
		dom_string_unref(doc->title);
	
	if (doc->memoised != NULL) {
		for(sidx = 0; sidx < hds_COUNT; ++sidx) {
			if (doc->memoised[sidx] != NULL) {
				dom_string_unref(doc->memoised[sidx]);
			}
		}
		free(doc->memoised);
		doc->memoised = NULL;
	}
	
	return _dom_document_finalise(&doc->base);
}

/* Destroy a HTMLDocument */
void _dom_html_document_destroy(dom_node_internal *node)
{
	dom_html_document *doc = (dom_html_document *) node;

	if (_dom_html_document_finalise(doc) == true)
		free(doc);
}

dom_exception _dom_html_document_copy(dom_node_internal *old,
		dom_node_internal **copy)
{
	UNUSED(old);
	UNUSED(copy);

	return DOM_NOT_SUPPORTED_ERR;
}

/* Overloaded methods inherited from super class */

/** Internal method to support both kinds of create method */
static dom_exception
_dom_html_document_create_element_internal(dom_html_document *html,
					   dom_string *in_tag_name,
					   dom_string *namespace,
					   dom_string *prefix,
					   dom_html_element **result)
{
	dom_exception exc;
	dom_string *tag_name;

	exc = dom_string_toupper(in_tag_name, true, &tag_name);
	if (exc != DOM_NO_ERR)
		return exc;

	if (dom_string_caseless_isequal(tag_name, html->memoised[hds_HTML])) {
		exc = _dom_html_html_element_create(html, namespace, prefix,
				(dom_html_html_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_HEAD])) {
		exc = _dom_html_head_element_create(html, namespace, prefix,
				(dom_html_head_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_TITLE])) {
		exc = _dom_html_title_element_create(html, namespace, prefix,
				(dom_html_title_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_BODY])) {
		exc = _dom_html_body_element_create(html, namespace, prefix,
				(dom_html_body_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_FORM])) {
		exc = _dom_html_form_element_create(html, namespace, prefix,
				(dom_html_form_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_LINK])) {
		exc = _dom_html_link_element_create(html, namespace, prefix,
				(dom_html_link_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_BUTTON])) {
		exc = _dom_html_button_element_create(html, namespace, prefix,
				(dom_html_button_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_INPUT])) {
		exc = _dom_html_input_element_create(html, namespace, prefix,
				(dom_html_input_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_TEXTAREA])) {
		exc = _dom_html_text_area_element_create(html, namespace, prefix,
				(dom_html_text_area_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_OPTGROUP])) {
		exc = _dom_html_opt_group_element_create(html, namespace, prefix,
				(dom_html_opt_group_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_OPTION])) {
		exc = _dom_html_option_element_create(html, namespace, prefix,
				(dom_html_option_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_SELECT])) {
		exc = _dom_html_select_element_create(html, namespace, prefix,
				(dom_html_select_element **) result);
	} else if (dom_string_caseless_isequal(tag_name, html->memoised[hds_HR])) {
		exc = _dom_html_hr_element_create(html, namespace, prefix,
				(dom_html_hr_element **) result);
	} else {
		exc =  _dom_html_element_create(html, tag_name, namespace,
						prefix, result);
	}

	dom_string_unref(tag_name);

	return exc;
}

dom_exception _dom_html_document_create_element(dom_document *doc,
		dom_string *tag_name, dom_element **result)
{
	dom_html_document *html = (dom_html_document *) doc;

	return _dom_html_document_create_element_internal(html,
			tag_name, NULL, NULL,
			(dom_html_element **)result);
}

dom_exception _dom_html_document_create_element_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_element **result)
{
	dom_html_document *html = (dom_html_document *) doc;
	dom_string *prefix, *localname;
	dom_exception err;

	/* Divide QName into prefix/localname pair */
	err = _dom_namespace_split_qname(qname, &prefix, &localname);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Attempt to create element */
	err = _dom_html_document_create_element_internal(html, localname,
			namespace, prefix, (dom_html_element **)result);

	/* Tidy up */
	dom_string_unref(localname);

	if (prefix != NULL) {
		dom_string_unref(prefix);
	}

	return err;
}

/**
 * Create an attribute
 *
 * \param doc     The document owning the attribute
 * \param name    The name of the attribute
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR                on success,
 *
 * The constructed attribute will always be classified as 'specified'.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_html_document_create_attribute(dom_document *doc,
		dom_string *name, dom_attr **result)
{
	return _dom_attr_create(doc, name, NULL, NULL, true, result);
}

/**
 * Create an attribute from the qualified name and namespace URI
 *
 * \param doc        The document owning the attribute
 * \param namespace  The namespace URI to use
 * \param qname      The qualified name of the attribute
 * \param result     Pointer to location to receive result
 * \return DOM_NO_ERR                on success,
 *         DOM_NAMESPACE_ERR         if ::qname is malformed, or it has a
 *                                   prefix and ::namespace is NULL, or
 *                                   ::qname has a prefix "xml" and
 *                                   ::namespace is not
 *                                   "http://www.w3.org/XML/1998/namespace",
 *                                   or ::qname has a prefix "xmlns" and
 *                                   ::namespace is not
 *                                   "http://www.w3.org/2000/xmlns", or
 *                                   ::namespace is
 *                                   "http://www.w3.org/2000/xmlns" and
 *                                   ::qname is not (or is not prefixed by)
 *                                   "xmlns",
 *         DOM_NOT_SUPPORTED_ERR     if ::doc does not support the "XML"
 *                                   feature.
 *
 * The returned node will have its reference count increased. It is
 * the responsibility of the caller to unref the node once it has
 * finished with it.
 */
dom_exception _dom_html_document_create_attribute_ns(dom_document *doc,
		dom_string *namespace, dom_string *qname,
		dom_attr **result)
{
	dom_string *prefix, *localname;
	dom_exception err;

	/* Divide QName into prefix/localname pair */
	err = _dom_namespace_split_qname(qname, &prefix, &localname);
	if (err != DOM_NO_ERR) {
		return err;
	}

	/* Attempt to create attribute */
	err = _dom_attr_create(doc, localname, namespace, prefix, true, result);

	/* Tidy up */
	if (localname != NULL) {
		dom_string_unref(localname);
	}

	if (prefix != NULL) {
		dom_string_unref(prefix);
	}

	return err;
}

/**
 * Retrieve a list of all elements with a given tag name
 *
 * \param doc      The document to search in
 * \param tagname  The tag name to search for ("*" for all)
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 *
 * The returned list will have its reference count increased. It is
 * the responsibility of the caller to unref the list once it has
 * finished with it.
 */
dom_exception _dom_html_document_get_elements_by_tag_name(dom_document *doc,
		dom_string *tagname, dom_nodelist **result)
{
	return _dom_document_get_nodelist(doc, DOM_NODELIST_BY_NAME_CASELESS,
			(dom_node_internal *) doc,  tagname, NULL, NULL, 
			result);
}

/**
 * Retrieve a list of all elements with a given local name and namespace URI
 *
 * \param doc        The document to search in
 * \param namespace  The namespace URI
 * \param localname  The local name
 * \param result     Pointer to location to receive result
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 *
 * The returned list will have its reference count increased. It is
 * the responsibility of the caller to unref the list once it has
 * finished with it.
 */
dom_exception _dom_html_document_get_elements_by_tag_name_ns(
		dom_document *doc, dom_string *namespace,
		dom_string *localname, dom_nodelist **result)
{
	return _dom_document_get_nodelist(doc, DOM_NODELIST_BY_NAMESPACE_CASELESS,
			(dom_node_internal *) doc, NULL, namespace, localname, 
			result);
}

/*-----------------------------------------------------------------------*/
/* The DOM spec public API */

/**
 * Get the title of this HTMLDocument 
 * \param doc    The document object
 * \param title  The reutrned title string
 * \return DOM_NO_ERR on success, appropriated dom_exception on failure.
 *
 * @note: this method find a title for the document as following:
 * 1. If there is a title in the document object set by 
 *    dom_html_document_set_title, then use it;
 * 2. If there is no such one, find the <title> element and use its text
 *    as the returned title.
 */
dom_exception _dom_html_document_get_title(dom_html_document *doc,
		dom_string **title)
{
	dom_exception exc = DOM_NO_ERR;
	*title = NULL;
	
	if (doc->title != NULL) {
		*title = dom_string_ref(doc->title);
	} else {
		dom_element *node;
		dom_nodelist *nodes;
		uint32_t len;
		
		exc = dom_document_get_elements_by_tag_name(doc,
							    doc->memoised[hds_TITLE],
							    &nodes);
		if (exc != DOM_NO_ERR) {
			return exc;
		}
		
		exc = dom_nodelist_get_length(nodes, &len);
		if (exc != DOM_NO_ERR) {
			dom_nodelist_unref(nodes);
			return exc;
		}
		
		if (len == 0) {
			dom_nodelist_unref(nodes);
			return DOM_NO_ERR;
		}
		
		exc = dom_nodelist_item(nodes, 0, (void *) &node);
		dom_nodelist_unref(nodes);
		if (exc != DOM_NO_ERR) {
			return exc;
		}
		
		exc = dom_node_get_text_content(node, title);
		dom_node_unref(node);
	}

	return exc;
}

dom_exception _dom_html_document_set_title(dom_html_document *doc,
		dom_string *title)
{
	if (doc->title != NULL)
		dom_string_unref(doc->title);

	doc->title = dom_string_ref(title);

	return DOM_NO_ERR;
}

dom_exception _dom_html_document_get_referrer(dom_html_document *doc,
		dom_string **referrer)
{
	*referrer = dom_string_ref(doc->referrer);

	return DOM_NO_ERR;
}

dom_exception _dom_html_document_get_domain(dom_html_document *doc,
		dom_string **domain)
{
	*domain = dom_string_ref(doc->domain);

	return DOM_NO_ERR;
}

dom_exception _dom_html_document_get_url(dom_html_document *doc,
		dom_string **url)
{
	*url = dom_string_ref(doc->url);

	return DOM_NO_ERR;
}

dom_exception _dom_html_document_get_body(dom_html_document *doc,
		struct dom_html_element **body)
{
	UNUSED(doc);
	UNUSED(body);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_set_body(dom_html_document *doc,
		struct dom_html_element *body)
{
	UNUSED(doc);
	UNUSED(body);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_get_images(dom_html_document *doc,
		struct dom_html_collection **col)
{
	UNUSED(doc);
	UNUSED(col);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_get_applets(dom_html_document *doc,
		struct dom_html_collection **col)
{
	UNUSED(doc);
	UNUSED(col);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_get_links(dom_html_document *doc,
		struct dom_html_collection **col)
{
	UNUSED(doc);
	UNUSED(col);

	return DOM_NOT_SUPPORTED_ERR;
}

static bool __dom_html_document_node_is_form(dom_node_internal *node,
					     void *ctx)
{
	dom_html_document *doc = (dom_html_document *)node->owner;
	
	UNUSED(ctx);
	
	return dom_string_caseless_isequal(node->name,
					   doc->memoised[hds_FORM]);
}

dom_exception _dom_html_document_get_forms(dom_html_document *doc,
		struct dom_html_collection **col)
{
	dom_html_collection *result;
	dom_element *root;
	dom_exception err;

	err = dom_document_get_document_element(doc, &root);
	if (err != DOM_NO_ERR)
		return err;

	err = _dom_html_collection_create(doc, (dom_node_internal *) root, 
			__dom_html_document_node_is_form, NULL, &result);
	if (err != DOM_NO_ERR) {
		dom_node_unref(root);
		return err;
	}

	dom_node_unref(root);

	*col = result;

	return DOM_NO_ERR;
}

dom_exception _dom_html_document_get_anchors(dom_html_document *doc,
		struct dom_html_collection **col)
{
	UNUSED(doc);
	UNUSED(col);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_get_cookie(dom_html_document *doc,
		dom_string **cookie)
{
	UNUSED(doc);
	UNUSED(cookie);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_set_cookie(dom_html_document *doc,
		dom_string *cookie)
{
	UNUSED(doc);
	UNUSED(cookie);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_open(dom_html_document *doc)
{
	UNUSED(doc);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_close(dom_html_document *doc)
{
	UNUSED(doc);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_write(dom_html_document *doc,
		dom_string *text)
{
	UNUSED(doc);
	UNUSED(text);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_writeln(dom_html_document *doc,
		dom_string *text)
{
	UNUSED(doc);
	UNUSED(text);

	return DOM_NOT_SUPPORTED_ERR;
}

dom_exception _dom_html_document_get_elements_by_name(dom_html_document *doc,
		dom_string *name, struct dom_nodelist **list)
{
	UNUSED(doc);
	UNUSED(name);
	UNUSED(list);

	return DOM_NOT_SUPPORTED_ERR;
}

