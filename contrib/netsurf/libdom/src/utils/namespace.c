/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *			http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <string.h>

#include <dom/dom.h>

#include "utils/namespace.h"
#include "utils/validate.h"
#include "utils/utils.h"


/** XML prefix */
static dom_string *xml;
/** XMLNS prefix */
static dom_string *xmlns;

/* The namespace strings */
static const char *namespaces[DOM_NAMESPACE_COUNT] = {
	NULL,
	"http://www.w3.org/1999/xhtml",
	"http://www.w3.org/1998/Math/MathML",
	"http://www.w3.org/2000/svg",
	"http://www.w3.org/1999/xlink",
	"http://www.w3.org/XML/1998/namespace",
	"http://www.w3.org/2000/xmlns/"
};

dom_string *dom_namespaces[DOM_NAMESPACE_COUNT] = {
	NULL,
};

/**
 * Initialise the namespace component
 *
 * \return DOM_NO_ERR on success.
 */
static dom_exception _dom_namespace_initialise(void)
{
	int i;
	dom_exception err;

	err = dom_string_create((const uint8_t *) "xml", SLEN("xml"), &xml);
	if (err != DOM_NO_ERR) {
		return err;
	}

	err = dom_string_create((const uint8_t *) "xmlns", SLEN("xmlns"), 
			&xmlns);
	if (err != DOM_NO_ERR) {
		dom_string_unref(xml);
		xml = NULL;

		return err;
	}

	for (i = 1; i < DOM_NAMESPACE_COUNT; i++) {
		err = dom_string_create(
				(const uint8_t *) namespaces[i],
				strlen(namespaces[i]), &dom_namespaces[i]);
		if (err != DOM_NO_ERR) {
			dom_string_unref(xmlns);
			xmlns = NULL;

			dom_string_unref(xml);
			xml = NULL;

			return err;
		}
	}

	return DOM_NO_ERR;
}

#ifdef FINALISE_NAMESPACE
/**
 * Finalise the namespace component
 *
 * \return DOM_NO_ERR on success.
 */
dom_exception _dom_namespace_finalise(void)
{
	int i;

	if (xmlns != NULL) {
		dom_string_unref(xmlns);
		xmlns = NULL;
	}

	if (xml != NULL) {
		dom_string_unref(xml);
		xml = NULL;
	}

	for (i = 1; i < DOM_NAMESPACE_COUNT; i++) {
		if (dom_namespaces[i] != NULL) {
			dom_string_unref(dom_namespaces[i]);
			dom_namespaces[i] = NULL;
		}
	}

	return DOM_NO_ERR;
}
#endif

/**
 * Ensure a QName is valid
 *
 * \param qname      The qname to validate
 * \param namespace  The namespace URI associated with the QName, or NULL
 * \return DOM_NO_ERR                if valid,
 *         DOM_INVALID_CHARACTER_ERR if ::qname contains an invalid character,
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
 *                                   "xmlns".
 */
dom_exception _dom_namespace_validate_qname(dom_string *qname,
		dom_string *namespace)
{
	uint32_t colon, len;

	if (xml == NULL) {
		dom_exception err = _dom_namespace_initialise();
		if (err != DOM_NO_ERR)
			return err;
	}

	if (qname == NULL) {
		if (namespace != NULL)
			return DOM_NAMESPACE_ERR;
		if (namespace == NULL)
			return DOM_NO_ERR;
	}

	if (_dom_validate_name(qname) == false)
		return DOM_NAMESPACE_ERR;

	len = dom_string_length(qname);

	/* Find colon */
	colon = dom_string_index(qname, ':');

	if (colon == (uint32_t) -1) {
		/* No prefix */
		/* If namespace URI is for xmlns, ensure qname == "xmlns" */
		if (namespace != NULL && 
				dom_string_isequal(namespace, 
				dom_namespaces[DOM_NAMESPACE_XMLNS]) &&
				dom_string_isequal(qname, xmlns) == false) {
			return DOM_NAMESPACE_ERR;
		}

		/* If qname == "xmlns", ensure namespace URI is for xmlns */
		if (namespace != NULL && 
				dom_string_isequal(qname, xmlns) &&
				dom_string_isequal(namespace, 
				dom_namespaces[DOM_NAMESPACE_XMLNS]) == false) {
			return DOM_NAMESPACE_ERR;
		}
	} else if (colon == 0) {
		/* Some name like ":name" */
		if (namespace != NULL)
			return DOM_NAMESPACE_ERR;
	} else {
		/* Prefix */
		dom_string *prefix;
		dom_string *lname;
		dom_exception err;

		/* Ensure there is a namespace URI */
		if (namespace == NULL) {
			return DOM_NAMESPACE_ERR;
		}

		err = dom_string_substr(qname, 0, colon, &prefix);
		if (err != DOM_NO_ERR) {
			return err;
		}

		err = dom_string_substr(qname, colon + 1, len, &lname);
		if (err != DOM_NO_ERR) {
			dom_string_unref(prefix);
			return err;
		}

		if ((_dom_validate_ncname(prefix) == false) ||
		    (_dom_validate_ncname(lname) == false)) {
			dom_string_unref(prefix);
			dom_string_unref(lname);
			return DOM_NAMESPACE_ERR;
		}
		dom_string_unref(lname);

		/* Test for invalid XML namespace */
		if (dom_string_isequal(prefix, xml) &&
				dom_string_isequal(namespace,
				dom_namespaces[DOM_NAMESPACE_XML]) == false) {
			dom_string_unref(prefix);
			return DOM_NAMESPACE_ERR;
		}

		/* Test for invalid xmlns namespace */
		if (dom_string_isequal(prefix, xmlns) &&
				dom_string_isequal(namespace,
				dom_namespaces[DOM_NAMESPACE_XMLNS]) == false) {
			dom_string_unref(prefix);
			return DOM_NAMESPACE_ERR;
		}

		/* Test for presence of xmlns namespace with non xmlns prefix */
		if (dom_string_isequal(namespace, 
				dom_namespaces[DOM_NAMESPACE_XMLNS]) &&
				dom_string_isequal(prefix, xmlns) == false) {
			dom_string_unref(prefix);
			return DOM_NAMESPACE_ERR;
		}

		dom_string_unref(prefix);
	}

	return DOM_NO_ERR;
}

/**
 * Split a QName into a namespace prefix and localname string
 *
 * \param qname      The qname to split
 * \param prefix     Pointer to location to receive prefix
 * \param localname  Pointer to location to receive localname
 * \return DOM_NO_ERR on success.
 *
 * If there is no prefix present in ::qname, then ::prefix will be NULL.
 *
 * ::prefix and ::localname will be referenced. The caller should unreference
 * them once finished.
 */
dom_exception _dom_namespace_split_qname(dom_string *qname,
		dom_string **prefix, dom_string **localname)
{
	uint32_t colon;
	dom_exception err;

	if (xml == NULL) {
		err = _dom_namespace_initialise();
		if (err != DOM_NO_ERR)
			return err;
	}

	/* Find colon, if any */
	colon = dom_string_index(qname, ':');

	if (colon == (uint32_t) -1) {
		/* None found => no prefix */
		*prefix = NULL;
		*localname = dom_string_ref(qname);
	} else {
		/* Found one => prefix */
		err = dom_string_substr(qname, 0, colon, prefix);
		if (err != DOM_NO_ERR) {
			return err;
		}

		err = dom_string_substr(qname, colon + 1,
				dom_string_length(qname), localname);
		if (err != DOM_NO_ERR) {
			dom_string_unref(*prefix);
			*prefix = NULL;
			return err;
		}
	}

	return DOM_NO_ERR;
}

/**
 * Get the XML prefix dom_string 
 *
 * \return the xml prefix dom_string.
 * 
 * Note: The client of this function may or may not call the dom_string_ref
 * on the returned dom_string, because this string will only be destroyed when
 * the dom_finalise is called. But if the client call dom_string_ref, it must
 * call dom_string_unref to maintain a correct ref count of the dom_string.
 */
dom_string *_dom_namespace_get_xml_prefix(void)
{
	if (xml == NULL) {
		if (_dom_namespace_initialise() != DOM_NO_ERR)
			return NULL;
	}

	return xml;
}

/**
 * Get the XMLNS prefix dom_string.
 *
 * \return the xmlns prefix dom_string
 * 
 * Note: The client of this function may or may not call the dom_string_ref
 * on the returned dom_string, because this string will only be destroyed when
 * the dom_finalise is called. But if the client call dom_string_ref, it must
 * call dom_string_unref to maintain a correct ref count of the dom_string.
 */
dom_string *_dom_namespace_get_xmlns_prefix(void)
{
	if (xml == NULL) {
		if (_dom_namespace_initialise() != DOM_NO_ERR)
			return NULL;
	}

	return xmlns;
}

