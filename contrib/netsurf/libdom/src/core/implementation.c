/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <string.h>

#include <dom/core/implementation.h>

#include "core/document.h"
#include "core/document_type.h"

#include "html/html_document.h"

#include "utils/namespace.h"
#include "utils/utils.h"
#include "utils/validate.h"

/**
 * Test whether a DOM implementation implements a specific feature
 * and version
 *
 * \param feature  The feature to test for
 * \param version  The version number of the feature to test for
 * \param result   Pointer to location to receive result
 * \return DOM_NO_ERR.
 */
dom_exception dom_implementation_has_feature(
		const char *feature, const char *version,
		bool *result)
{
	UNUSED(feature);
	UNUSED(version);
	UNUSED(result);

	return DOM_NOT_SUPPORTED_ERR;
}

/**
 * Create a document type node
 *
 * \param qname      The qualified name of the document type
 * \param public_id  The external subset public identifier
 * \param system_id  The external subset system identifier
 * \param doctype    Pointer to location to receive result
 * \return DOM_NO_ERR on success,
 *         DOM_INVALID_CHARACTER_ERR if ::qname is invalid,
 *         DOM_NAMESPACE_ERR         if ::qname is malformed,
 *         DOM_NOT_SUPPORTED_ERR     if ::impl does not support the feature
 *                                   "XML" and the language exposed through
 *                                   Document does not support XML
 *                                   namespaces.
 *
 * The doctype will be referenced, so the client need not do this
 * explicitly. The client must unref the doctype once it has
 * finished with it.
 */
dom_exception dom_implementation_create_document_type(
		const char *qname, const char *public_id, 
		const char *system_id,
		struct dom_document_type **doctype)
{
	struct dom_document_type *d;
	dom_string *qname_s = NULL, *prefix = NULL, *lname = NULL;
	dom_string *public_id_s = NULL, *system_id_s = NULL;
	dom_exception err;

	if (qname == NULL) {
		return DOM_INVALID_CHARACTER_ERR;
	}

	err = dom_string_create((const uint8_t *) qname,
				strlen(qname), &qname_s);
	if (err != DOM_NO_ERR)
		return err;

	err = _dom_namespace_split_qname(qname_s, &prefix, &lname);
	if (err != DOM_NO_ERR) {
		dom_string_unref(qname_s);
		return err;
	}

	if (public_id != NULL) {
		err = dom_string_create((const uint8_t *) public_id,
				strlen(public_id), &public_id_s);
		if (err != DOM_NO_ERR) {
			dom_string_unref(lname);
			dom_string_unref(prefix);
			dom_string_unref(qname_s);
			return err;
		}
	}

	if (system_id != NULL) {
		err = dom_string_create((const uint8_t *) system_id,
				strlen(system_id), &system_id_s);
		if (err != DOM_NO_ERR) {
			dom_string_unref(public_id_s);
			dom_string_unref(lname);
			dom_string_unref(prefix);
			dom_string_unref(qname_s);
			return err;
		}
	}

	/* Create the doctype */
	err = _dom_document_type_create(qname_s, public_id_s, system_id_s, &d);

	if (err == DOM_NO_ERR)
		*doctype = d;

	dom_string_unref(system_id_s);
	dom_string_unref(public_id_s);
	dom_string_unref(prefix);
	dom_string_unref(lname);
	dom_string_unref(qname_s);

	return err;
}

/**
 * Create a document node
 *
 * \param impl_type  The type of document object to create
 * \param namespace  The namespace URI of the document element
 * \param qname      The qualified name of the document element
 * \param doctype    The type of document to create
 * \param doc        Pointer to location to receive result
 * \return DOM_NO_ERR on success,
 *         DOM_INVALID_CHARACTER_ERR if ::qname is invalid,
 *         DOM_NAMESPACE_ERR         if ::qname is malformed, or if ::qname
 *                                   has a prefix and ::namespace is NULL,
 *                                   or if ::qname is NULL and ::namespace
 *                                   is non-NULL, or if ::qname has a prefix
 *                                   "xml" and ::namespace is not
 *                                   "http://www.w3.org/XML/1998/namespace",
 *                                   or if ::impl does not support the "XML"
 *                                   feature and ::namespace is non-NULL,
 *         DOM_WRONG_DOCUMENT_ERR    if ::doctype is already being used by a
 *                                   document, or if it was not created by
 *                                   ::impl,
 *         DOM_NOT_SUPPORTED_ERR     if ::impl does not support the feature
 *                                   "XML" and the language exposed through
 *                                   Document does not support XML
 *                                   namespaces.
 *
 * The document will be referenced, so the client need not do this
 * explicitly. The client must unref the document once it has
 * finished with it.
 */
dom_exception dom_implementation_create_document(
		uint32_t impl_type,
		const char *namespace, const char *qname,
		struct dom_document_type *doctype,
		dom_events_default_action_fetcher daf,
		void *daf_ctx,
		struct dom_document **doc)
{
	struct dom_document *d;
	dom_string *namespace_s = NULL, *qname_s = NULL;
	dom_exception err;

	if (namespace != NULL) {
		err = dom_string_create((const uint8_t *) namespace,
				strlen(namespace), &namespace_s);
		if (err != DOM_NO_ERR)
			return err;
	}

	if (qname != NULL) {
		err = dom_string_create((const uint8_t *) qname, 
				strlen(qname), &qname_s);
		if (err != DOM_NO_ERR) {
			dom_string_unref(namespace_s);
			return err;
		}
	}

	if (qname_s != NULL && _dom_validate_name(qname_s) == false) {
		dom_string_unref(qname_s);
		dom_string_unref(namespace_s);
		return DOM_INVALID_CHARACTER_ERR;
	}
  
	err = _dom_namespace_validate_qname(qname_s, namespace_s);
	if (err != DOM_NO_ERR) {
		dom_string_unref(qname_s);
		dom_string_unref(namespace_s);
		return DOM_NAMESPACE_ERR;
	}

	if (doctype != NULL && dom_node_get_parent(doctype) != NULL) {
		dom_string_unref(qname_s);
		dom_string_unref(namespace_s);
		return DOM_WRONG_DOCUMENT_ERR;
	}

	/* Create document object that reflects the required APIs */
 	if (impl_type == DOM_IMPLEMENTATION_HTML) {
		dom_html_document *html_doc;

		err = _dom_html_document_create(daf, daf_ctx, &html_doc);

		d = (dom_document *) html_doc;
	} else {
		err = _dom_document_create(daf, daf_ctx, &d);
	}

	if (err != DOM_NO_ERR) {
		dom_string_unref(qname_s);
		dom_string_unref(namespace_s);
		return err;
	}

	/* Set its doctype, if necessary */
	if (doctype != NULL) {
		struct dom_node *ins_doctype = NULL;

		err = dom_node_append_child((struct dom_node *) d, 
				(struct dom_node *) doctype, &ins_doctype);
		if (err != DOM_NO_ERR) {
			dom_node_unref((struct dom_node *) d);
			dom_string_unref(qname_s);
			dom_string_unref(namespace_s);
			return err;
		}

		/* Not interested in inserted doctype */
		if (ins_doctype != NULL)
			dom_node_unref(ins_doctype);
	}

	/* Create root element and attach it to document */
	if (qname_s != NULL) {
		struct dom_element *e;
		struct dom_node *inserted;

		err = dom_document_create_element_ns(d, namespace_s, qname_s, &e);
		if (err != DOM_NO_ERR) {
			dom_node_unref((struct dom_node *) d);
			dom_string_unref(qname_s);
			dom_string_unref(namespace_s);
			return err;
		}

		err = dom_node_append_child((struct dom_node *) d,
				(struct dom_node *) e, &inserted);
		if (err != DOM_NO_ERR) {
			dom_node_unref((struct dom_node *) e);
			dom_node_unref((struct dom_node *) d);
			dom_string_unref(qname_s);
			dom_string_unref(namespace_s);
			return err;
		}

		/* No longer interested in inserted node */
		dom_node_unref(inserted);

		/* Done with element */
		dom_node_unref((struct dom_node *) e);
	}

	/* Clean up strings we created */
	dom_string_unref(qname_s);
	dom_string_unref(namespace_s);

	*doc = d;

	return DOM_NO_ERR;
}

/**
 * Retrieve a specialized object which implements the specified
 * feature and version
 *
 * \param feature  The requested feature
 * \param version  The version number of the feature
 * \param object   Pointer to location to receive object
 * \return DOM_NO_ERR.
 *
 * Any memory allocated by this call should be allocated using
 * the provided memory (de)allocation function.
 */
dom_exception dom_implementation_get_feature(
		const char *feature, const char *version,
		void **object)
{
	UNUSED(feature);
	UNUSED(version);
	UNUSED(object);

	return DOM_NOT_SUPPORTED_ERR;
}
