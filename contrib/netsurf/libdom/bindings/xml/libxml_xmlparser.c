/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <libxml/parser.h>
#include <libxml/SAX2.h>
#include <libxml/xmlerror.h>

#include <dom/dom.h>

#include <libwapcaplet/libwapcaplet.h>

#include "xmlerror.h"
#include "xmlparser.h"
#include "utils.h"

#include "core/document.h"

static void xml_parser_start_document(void *ctx);
static void xml_parser_end_document(void *ctx);
static void xml_parser_start_element_ns(void *ctx, const xmlChar *localname,
		const xmlChar *prefix, const xmlChar *URI,
		int nb_namespaces, const xmlChar **namespaces,
		int nb_attributes, int nb_defaulted,
		const xmlChar **attributes);
static void xml_parser_end_element_ns(void *ctx, const xmlChar *localname,
		const xmlChar *prefix, const xmlChar *URI);

static dom_exception xml_parser_link_nodes(dom_xml_parser *parser,
		struct dom_node *dom, xmlNodePtr xml);

static void xml_parser_add_node(dom_xml_parser *parser, struct dom_node *parent,
		xmlNodePtr child);
static void xml_parser_add_element_node(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_text_node(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_cdata_section(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_entity_reference(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_entity(dom_xml_parser *parser, 
        struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_comment(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);
static void xml_parser_add_document_type(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child);

static void xml_parser_internal_subset(void *ctx, const xmlChar *name,
		const xmlChar *ExternalID, const xmlChar *SystemID);
static int xml_parser_is_standalone(void *ctx);
static int xml_parser_has_internal_subset(void *ctx);
static int xml_parser_has_external_subset(void *ctx);
static xmlParserInputPtr xml_parser_resolve_entity(void *ctx,
		const xmlChar *publicId, const xmlChar *systemId);
static xmlEntityPtr xml_parser_get_entity(void *ctx, const xmlChar *name);
static void xml_parser_entity_decl(void *ctx, const xmlChar *name,
		int type, const xmlChar *publicId, const xmlChar *systemId,
		xmlChar *content);
static void xml_parser_notation_decl(void *ctx, const xmlChar *name,
		const xmlChar *publicId, const xmlChar *systemId);
static void xml_parser_attribute_decl(void *ctx, const xmlChar *elem,
		const xmlChar *fullname, int type, int def,
		const xmlChar *defaultValue, xmlEnumerationPtr tree);
static void xml_parser_element_decl(void *ctx, const xmlChar *name,
		int type, xmlElementContentPtr content);
static void xml_parser_unparsed_entity_decl(void *ctx, const xmlChar *name,
		const xmlChar *publicId, const xmlChar *systemId,
		const xmlChar *notationName);
static void xml_parser_set_document_locator(void *ctx, xmlSAXLocatorPtr loc);
static void xml_parser_reference(void *ctx, const xmlChar *name);
static void xml_parser_characters(void *ctx, const xmlChar *ch, int len);
static void xml_parser_comment(void *ctx, const xmlChar *value);
static xmlEntityPtr xml_parser_get_parameter_entity(void *ctx,
		const xmlChar *name);
static void xml_parser_cdata_block(void *ctx, const xmlChar *value, int len);
static void xml_parser_external_subset(void *ctx, const xmlChar *name,
		const xmlChar *ExternalID, const xmlChar *SystemID);

/**
 * libdom XML parser object
 */
struct dom_xml_parser {
	xmlParserCtxtPtr xml_ctx;	/**< libxml parser context */

	struct dom_document *doc;	/**< DOM Document we're building */

	dom_string *udkey;	/**< Key for DOM node user data */

	dom_msg msg;		/**< Informational message function */
	void *mctx;		/**< Pointer to client data */
};

/**
 * SAX callback dispatch table
 */
static xmlSAXHandler sax_handler = {
	.internalSubset         = xml_parser_internal_subset,
	.isStandalone           = xml_parser_is_standalone,
	.hasInternalSubset      = xml_parser_has_internal_subset,
	.hasExternalSubset      = xml_parser_has_external_subset,
	.resolveEntity          = xml_parser_resolve_entity,
	.getEntity              = xml_parser_get_entity,
	.entityDecl             = xml_parser_entity_decl,
	.notationDecl           = xml_parser_notation_decl,
	.attributeDecl          = xml_parser_attribute_decl,
	.elementDecl            = xml_parser_element_decl,
	.unparsedEntityDecl     = xml_parser_unparsed_entity_decl,
	.setDocumentLocator     = xml_parser_set_document_locator,
	.startDocument          = xml_parser_start_document,
	.endDocument            = xml_parser_end_document,
	.startElement           = NULL,
	.endElement             = NULL,
	.reference              = xml_parser_reference,
	.characters             = xml_parser_characters,
	.ignorableWhitespace    = xml_parser_characters,
	.processingInstruction  = NULL,
	.comment                = xml_parser_comment,
	.warning                = NULL,
	.error                  = NULL,
	.fatalError             = NULL,
	.getParameterEntity     = xml_parser_get_parameter_entity,
	.cdataBlock             = xml_parser_cdata_block,
	.externalSubset         = xml_parser_external_subset,
	.initialized            = XML_SAX2_MAGIC,
	._private               = NULL,
	.startElementNs         = xml_parser_start_element_ns,
	.endElementNs           = xml_parser_end_element_ns,
	.serror                 = NULL
};

static void *dom_xml_alloc(void *ptr, size_t len, void *pw)
{
	UNUSED(pw);

	if (ptr == NULL)
		return len > 0 ? malloc(len) : NULL;

	if (len == 0) {
		free(ptr);
		return NULL;
	}

	return realloc(ptr, len);
}

/**
 * Create an XML parser instance
 *
 * \param enc      Source charset, or NULL
 * \param int_enc  Desired charset of document buffer (UTF-8 or UTF-16)
 * \param msg      Informational message function
 * \param mctx     Pointer to client-specific private data
 * \return Pointer to instance, or NULL on memory exhaustion
 *
 * Neither ::enc nor ::int_enc are used here.
 * libxml only supports a UTF-8 document buffer and forcibly setting the
 * parser encoding is not yet implemented
 */
dom_xml_parser *dom_xml_parser_create(const char *enc, const char *int_enc,
		dom_msg msg, void *mctx, dom_document **document)
{
	dom_xml_parser *parser;
	dom_exception err;
	int ret;

	UNUSED(enc);
	UNUSED(int_enc);

	parser = dom_xml_alloc(NULL, sizeof(dom_xml_parser), NULL);
	if (parser == NULL) {
		msg(DOM_MSG_CRITICAL, mctx, "No memory for parser");
		return NULL;
	}

	parser->xml_ctx =
		xmlCreatePushParserCtxt(&sax_handler, parser, "", 0, NULL);
	if (parser->xml_ctx == NULL) {
		dom_xml_alloc(parser, 0, NULL);
		msg(DOM_MSG_CRITICAL, mctx, "Failed to create XML parser");
		return NULL;
	}

	/* Set options of parsing context */
	ret = xmlCtxtUseOptions(parser->xml_ctx, XML_PARSE_DTDATTR | 
			XML_PARSE_DTDLOAD);
	if (ret != 0) {
		xmlFreeParserCtxt(parser->xml_ctx);
		dom_xml_alloc(parser, 0, NULL);
		msg(DOM_MSG_CRITICAL, mctx, "Failed setting parser options");
		return NULL;
	}

	/* Create key for user data registration */
	err = dom_string_create((const uint8_t *) "__xmlnode", 
			SLEN("__xmlnode"), &parser->udkey);
	if (err != DOM_NO_ERR) {
		xmlFreeParserCtxt(parser->xml_ctx);
		dom_xml_alloc(parser, 0, NULL);
		msg(DOM_MSG_CRITICAL, mctx, "No memory for userdata key");
		return NULL;
	}

	err = dom_implementation_create_document(
			DOM_IMPLEMENTATION_XML,
			/* namespace */ NULL,
			/* qname */ NULL,
			/* doctype */ NULL,
			NULL,
			NULL,
			document);

	if (err != DOM_NO_ERR) {
		xmlFreeParserCtxt(parser->xml_ctx);
		dom_string_unref(parser->udkey);
		dom_xml_alloc(parser, 0, NULL);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"Failed creating document");
		return NULL;
	}

	parser->doc = (dom_document *) dom_node_ref(*document);

	parser->msg = msg;
	parser->mctx = mctx;

	return parser;
}

/**
 * Destroy an XML parser instance
 *
 * \param parser  The parser instance to destroy
 */
void dom_xml_parser_destroy(dom_xml_parser *parser)
{
	dom_string_unref(parser->udkey);
	dom_node_unref(parser->doc);

	xmlFreeDoc(parser->xml_ctx->myDoc);
	
	xmlFreeParserCtxt(parser->xml_ctx);

	dom_xml_alloc(parser, 0, NULL);
}

/**
 * Parse a chunk of data
 *
 * \param parser  The XML parser instance to use for parsing
 * \param data    Pointer to data chunk
 * \param len     Byte length of data chunk
 * \return DOM_XML_OK on success, DOM_XML_EXTERNAL_ERR | <libxml error> on failure
 */
dom_xml_error dom_xml_parser_parse_chunk(dom_xml_parser *parser,
		uint8_t *data, size_t len)
{
	xmlParserErrors err;

	err = xmlParseChunk(parser->xml_ctx, (char *) data, len, 0);
	if (err != XML_ERR_OK) {
		parser->msg(DOM_MSG_ERROR, parser->mctx, 
				"xmlParseChunk failed: %d", err);
		return DOM_XML_EXTERNAL_ERR | err;
	}

	return DOM_XML_OK;
}

/**
 * Notify parser that datastream is empty
 *
 * \param parser  The XML parser instance to notify
 * \return DOM_XML_OK on success, DOM_XML_EXTERNAL_ERR | <libxml error> on failure
 *
 * This will force any remaining data through the parser
 */
dom_xml_error dom_xml_parser_completed(dom_xml_parser *parser)
{
	xmlParserErrors err;

	err = xmlParseChunk(parser->xml_ctx, "", 0, 1);
	if (err != XML_ERR_OK) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"xmlParseChunk failed: %d", err);
		return DOM_XML_EXTERNAL_ERR | err;
	}

	return DOM_XML_OK;
}

/**
 * Handle a document start SAX event
 *
 * \param ctx  The callback context
 */
void xml_parser_start_document(void *ctx)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;
	dom_exception err;

	/* Invoke libxml2's default behaviour */
	xmlSAX2StartDocument(parser->xml_ctx);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) parser->doc,
			(xmlNodePtr) parser->xml_ctx->myDoc);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"Not able to link document nodes");
	}
}

/**
 * Handle a document end SAX event
 *
 * \param ctx  The callback context
 */
void xml_parser_end_document(void *ctx)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;
	xmlNodePtr node;
	xmlNodePtr n;
	dom_exception err;

	/* Invoke libxml2's default behaviour */
	xmlSAX2EndDocument(parser->xml_ctx);

	/* If there is no document, we can't do anything */
	if (parser->doc == NULL) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"No document in end_document");
		return;
	}

	/* We need to mirror any child nodes at the end of the list of
	 * children which occur after the last Element node in the list */

	/* Get XML node */
	err = dom_node_get_user_data((struct dom_node *) parser->doc,
			parser->udkey, (void **) (void *) &node);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"Failed finding XML node");
		return;
	}

	/* Find last Element node, if any */
	for (n = node->last; n != NULL; n = n->prev) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	if (n == NULL) {
		/* No Element node found; entire list needs mirroring */
		n = node->children;
	} else {
		/* Found last Element; skip over it */
		n = n->next;
	}

	/* Now, mirror nodes in the DOM */
	for (; n != NULL; n = n->next) {
		xml_parser_add_node(parser,
				(struct dom_node *) node->_private, n);
	}
}

/**
 * Handle an element open SAX event
 *
 * \param ctx            The callback context
 * \param localname      The local name of the element
 * \param prefix         The element namespace prefix
 * \param URI            The element namespace URI
 * \param nb_namespaces  The number of namespace definitions
 * \param namespaces     Array of nb_namespaces prefix/URI pairs
 * \param nb_attributes  The total number of attributes
 * \param nb_defaulted   The number of defaulted attributes
 * \param attributes     Array of nb_attributes attribute values
 *
 * The number of non-defaulted attributes is ::nb_attributes - ::nb_defaulted
 * The defaulted attributes are at the end of the array ::attributes.
 */
void xml_parser_start_element_ns(void *ctx, const xmlChar *localname,
		const xmlChar *prefix, const xmlChar *URI,
		int nb_namespaces, const xmlChar **namespaces,
		int nb_attributes, int nb_defaulted,
		const xmlChar **attributes)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;
	xmlNodePtr parent = parser->xml_ctx->node;

	/* Invoke libxml2's default behaviour */
	xmlSAX2StartElementNs(parser->xml_ctx, localname, prefix, URI,
			nb_namespaces, namespaces, nb_attributes,
			nb_defaulted, attributes);

	/* If there is no document, we can't do anything */
	if (parser->doc == NULL) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"No document in start_element_ns");
		return;
	}

	if (parent == NULL) {
		/* No parent; use document */
		parent = (xmlNodePtr) parser->xml_ctx->myDoc;
	}

	if (parent->type == XML_DOCUMENT_NODE ||
			parent->type == XML_ELEMENT_NODE) {
		/* Mirror in the DOM all children of the parent node
		 * between the previous Element child (or the start,
		 * whichever is encountered first) and the Element
		 * just created */
		xmlNodePtr n;

		/* Find previous element node, if any */
		for (n = parser->xml_ctx->node->prev; n != NULL;
				n = n->prev) {
			if (n->type == XML_ELEMENT_NODE)
				break;
		}

		if (n == NULL) {
			/* No previous Element; use parent's children */
			n = parent->children;
		} else {
			/* Previous Element; skip over it */
			n = n->next;
		}

		/* Now, mirror nodes in the DOM */
		for (; n != parser->xml_ctx->node; n = n->next) {
			xml_parser_add_node(parser,
					(struct dom_node *) parent->_private,
					n);
		}
	}

	/* Mirror the created node and its attributes in the DOM */
	xml_parser_add_node(parser, (struct dom_node *) parent->_private,
			parser->xml_ctx->node);

}

/**
 * Handle an element close SAX event
 *
 * \param ctx        The callback context
 * \param localname  The local name of the element
 * \param prefix     The element namespace prefix
 * \param URI        The element namespace URI
 */
void xml_parser_end_element_ns(void *ctx, const xmlChar *localname,
		const xmlChar *prefix, const xmlChar *URI)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;
	xmlNodePtr node = parser->xml_ctx->node;
	xmlNodePtr n;

	/* Invoke libxml2's default behaviour */
	xmlSAX2EndElementNs(parser->xml_ctx, localname, prefix, URI);

	/* If there is no document, we can't do anything */
	if (parser->doc == NULL) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"No document in end_element_ns");
		return;
	}

	/* If node wasn't linked, we can't do anything */
	if (node->_private == NULL) {
		parser->msg(DOM_MSG_WARNING, parser->mctx,
				"Node '%s' not linked", node->name);
		return;
	}

	/* We need to mirror any child nodes at the end of the list of
	 * children which occur after the last Element node in the list */

	/* Find last Element node, if any */
	for (n = node->last; n != NULL; n = n->prev) {
		if (n->type == XML_ELEMENT_NODE)
			break;
	}

	if (n == NULL) {
		/* No Element node found; entire list needs mirroring */
		n = node->children;
	} else {
		/* Found last Element; skip over it */
		n = n->next;
	}

	/* Now, mirror nodes in the DOM */
	for (; n != NULL; n = n->next) {
		xml_parser_add_node(parser,
				(struct dom_node *) node->_private, n);
	}
}

/**
 * Link a DOM and XML node together
 *
 * \param parser  The parser context
 * \param dom     The DOM node
 * \param xml     The XML node
 * \return DOM_NO_ERR on success, appropriate error otherwise
 */
dom_exception xml_parser_link_nodes(dom_xml_parser *parser, 
		struct dom_node *dom, xmlNodePtr xml)
{
	void *prev_data;
	dom_exception err;

	/* Register XML node as user data for DOM node */
	err = dom_node_set_user_data(dom, parser->udkey, xml, NULL,
			&prev_data);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed setting user data: %d", err);
		return err;
	}

	/* Register DOM node with the XML node */
	xml->_private = dom;

	return DOM_NO_ERR;
}

/**
 * Add a node to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_node(dom_xml_parser *parser, struct dom_node *parent,
		xmlNodePtr child)
{
	static const char *node_types[] = {
		"THIS_IS_NOT_A_NODE",
		"XML_ELEMENT_NODE",
		"XML_ATTRIBUTE_NODE",
		"XML_TEXT_NODE",
		"XML_CDATA_SECTION_NODE",
		"XML_ENTITY_REF_NODE",
		"XML_ENTITY_NODE",
		"XML_PI_NODE",
		"XML_COMMENT_NODE",
		"XML_DOCUMENT_NODE",
		"XML_DOCUMENT_TYPE_NODE",
		"XML_DOCUMENT_FRAG_NODE",
		"XML_NOTATION_NODE",
		"XML_HTML_DOCUMENT_NODE",
		"XML_DTD_NODE",
		"XML_ELEMENT_DECL",
		"XML_ATTRIBUTE_DECL",
		"XML_ENTITY_DECL",
		"XML_NAMESPACE_DECL",
		"XML_XINCLUDE_START",
		"XML_XINCLUDE_END",
		"XML_DOCB_DOCUMENT_NODE"
	};

	switch (child->type) {
	case XML_ELEMENT_NODE:
		xml_parser_add_element_node(parser, parent, child);
		break;
	case XML_TEXT_NODE:
		xml_parser_add_text_node(parser, parent, child);
		break;
	case XML_CDATA_SECTION_NODE:
		xml_parser_add_cdata_section(parser, parent, child);
		break;
	case XML_ENTITY_REF_NODE:
		xml_parser_add_entity_reference(parser, parent, child);
		break;
	case XML_COMMENT_NODE:
		xml_parser_add_comment(parser, parent, child);
		break;
	case XML_DTD_NODE:
		xml_parser_add_document_type(parser, parent, child);
		break;
    case XML_ENTITY_DECL:
        xml_parser_add_entity(parser, parent, child);
        break;
	default:
		parser->msg(DOM_MSG_NOTICE, parser->mctx,
				"Unsupported node type: %s",
				node_types[child->type]);
	}
}

/**
 * Add an element node to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_element_node(dom_xml_parser *parser, 
		struct dom_node *parent, xmlNodePtr child)
{
	struct dom_element *el, *ins_el = NULL;
	xmlAttrPtr a;
	dom_exception err;

	/* Create the element node */
	if (child->ns == NULL) {
		/* No namespace */
		dom_string *tag_name;

		/* Create tag name DOM string */
		err = dom_string_create(child->name, 
				strlen((const char *) child->name), &tag_name);
		if (err != DOM_NO_ERR) {
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"No memory for tag name");
			return;
		}

		/* Create element node */
		err = dom_document_create_element(parser->doc,
				tag_name, &el);
		if (err != DOM_NO_ERR) {
			dom_string_unref(tag_name);
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"Failed creating element '%s'",
					child->name);
			return;
		}

		/* No longer need tag name */
		dom_string_unref(tag_name);
	} else {
		/* Namespace */
		dom_string *namespace;
		dom_string *qname;
		size_t qnamelen = (child->ns->prefix != NULL ?
			strlen((const char *) child->ns->prefix) : 0) +
			(child->ns->prefix != NULL ? 1 : 0) /* ':' */ +
			strlen((const char *) child->name);
		uint8_t qnamebuf[qnamelen + 1 /* '\0' */];

		/* Create namespace DOM string */
		err = dom_string_create(
				child->ns->href,
				strlen((const char *) child->ns->href),
				&namespace);
		if (err != DOM_NO_ERR) {
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"No memory for namespace");
			return;
		}

		/* QName is "prefix:localname",
		 * or "localname" if there is no prefix */
		sprintf((char *) qnamebuf, "%s%s%s",
			child->ns->prefix != NULL ?
				(const char *) child->ns->prefix : "",
			child->ns->prefix != NULL ? ":" : "",
			(const char *) child->name);

		/* Create qname DOM string */
		err = dom_string_create(
				qnamebuf,
				qnamelen,
				&qname);
		if (err != DOM_NO_ERR) {
			dom_string_unref(namespace);
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"No memory for qname");
			return;
		}

		/* Create element node */
		err = dom_document_create_element_ns(parser->doc,
				namespace, qname, &el);
		if (err != DOM_NO_ERR) {
			dom_string_unref(namespace);
			dom_string_unref(qname);
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"Failed creating element '%s'",
					qnamebuf);
			return;
		}

		/* No longer need namespace / qname */
		dom_string_unref(namespace);
		dom_string_unref(qname);
	}

	/* Add attributes to created element */
	for (a = child->properties; a != NULL; a = a->next) {
		struct dom_attr *attr, *prev_attr;
		xmlNodePtr c;

		/* Create attribute node */
		if (a->ns == NULL) {
			/* Attribute has no namespace */
			dom_string *name;

			/* Create attribute name DOM string */
			err = dom_string_create(
					a->name,
					strlen((const char *) a->name),
					&name);
			if (err != DOM_NO_ERR) {
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
						"No memory for attribute name");
				goto cleanup;
			}

			/* Create attribute */
			err = dom_document_create_attribute(parser->doc,
					name, &attr);
			if (err != DOM_NO_ERR) {
				dom_string_unref(name);
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
						"Failed creating attribute \
						'%s'", a->name);
				goto cleanup;
			}

			/* No longer need attribute name */
			dom_string_unref(name);
		} else {
			/* Attribute has namespace */
			dom_string *namespace;
			dom_string *qname;
			size_t qnamelen = (a->ns->prefix != NULL ?
				strlen((const char *) a->ns->prefix) : 0) +
				(a->ns->prefix != NULL ? 1 : 0) /* ':' */ +
				strlen((const char *) a->name);
			uint8_t qnamebuf[qnamelen + 1 /* '\0' */];

			/* Create namespace DOM string */
			err = dom_string_create(
					a->ns->href,
					strlen((const char *) a->ns->href),
					&namespace);
			if (err != DOM_NO_ERR) {
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
						"No memory for namespace");
				return;
			}

			/* QName is "prefix:localname",
			 * or "localname" if there is no prefix */
			sprintf((char *) qnamebuf, "%s%s%s",
				a->ns->prefix != NULL ?
					(const char *) a->ns->prefix : "",
				a->ns->prefix != NULL ? ":" : "",
				(const char *) a->name);

			/* Create qname DOM string */
			err = dom_string_create(
					qnamebuf,
					qnamelen,
					&qname);
			if (err != DOM_NO_ERR) {
				dom_string_unref(namespace);
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
						"No memory for qname");
				return;
			}

			/* Create attribute */
			err = dom_document_create_attribute_ns(parser->doc,
					namespace, qname, &attr);
			if (err != DOM_NO_ERR) {
				dom_string_unref(namespace);
				dom_string_unref(qname);
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
						"Failed creating attribute \
						'%s'", qnamebuf);
				return;
			}

			/* No longer need namespace / qname */
			dom_string_unref(namespace);
			dom_string_unref(qname);
		}

		/* Clone subtree (attribute value) */
		for (c = a->children; c != NULL; c = c->next) {
			xml_parser_add_node(parser,
					(struct dom_node *) attr, c);
		}

		/* Link nodes together */
		err = xml_parser_link_nodes(parser,
				(struct dom_node *) attr, (xmlNodePtr) a);
		if (err != DOM_NO_ERR) {
			dom_node_unref((struct dom_node *) attr);
			goto cleanup;
		}

		if (a->ns == NULL) {
			/* And add attribute to the element */
			err = dom_element_set_attribute_node(el, attr,
					&prev_attr);
			if (err != DOM_NO_ERR) {
				dom_node_unref((struct dom_node *) attr);
				parser->msg(DOM_MSG_ERROR, parser->mctx,
						"Failed attaching attribute \
						'%s'", a->name);
				goto cleanup;
			}
		} else {
			err = dom_element_set_attribute_node_ns(el, attr,
					&prev_attr);
			if (err != DOM_NO_ERR) {
				dom_node_unref((struct dom_node *) attr);
				parser->msg(DOM_MSG_ERROR, parser->mctx,
						"Failed attaching attribute \
						'%s'", a->name);
				goto cleanup;
			}
		}

		/* We're not interested in the previous attribute (if any) */
		if (prev_attr != NULL && prev_attr != attr)
			dom_node_unref((struct dom_node *) prev_attr);

		/* We're no longer interested in the attribute node */
		dom_node_unref((struct dom_node *) attr);
	}

	/* Append element to parent */
	err = dom_node_append_child(parent, (struct dom_node *) el,
			(struct dom_node **) (void *) &ins_el);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed attaching element '%s'",
				child->name);
		goto cleanup;
	}

	/* We're not interested in the inserted element */
	if (ins_el != NULL)
		dom_node_unref((struct dom_node *) ins_el);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) el,
			child);
	if (err != DOM_NO_ERR) {
		goto cleanup;
	}

	/* No longer interested in element node */
	dom_node_unref((struct dom_node *) el);

	return;

cleanup:
	/* No longer want node (any attributes attached to it
	 * will be cleaned up with it) */
	dom_node_unref((struct dom_node *) el);

	return;
}

/**
 * Add a text node to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_text_node(dom_xml_parser *parser, struct dom_node *parent,
		xmlNodePtr child)
{
	struct dom_text *text, *ins_text = NULL;
	dom_string *data;
	dom_exception err;

	/* Create DOM string data for text node */
	err = dom_string_create(child->content,
			strlen((const char *) child->content), &data);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for text node contents ");
		return;
	}

	/* Create text node */
	err = dom_document_create_text_node(parser->doc, data, &text);
	if (err != DOM_NO_ERR) {
		dom_string_unref(data);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for text node");
		return;
	}

	/* No longer need data */
	dom_string_unref(data);

	/* Append text node to parent */
	err = dom_node_append_child(parent, (struct dom_node *) text,
			(struct dom_node **) (void *) &ins_text);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) text);
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed attaching text node");
		return;
	}

	/* We're not interested in the inserted text node */
	if (ins_text != NULL)
		dom_node_unref((struct dom_node *) ins_text);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) text,
			child);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) text);
		return;
	}

	/* No longer interested in text node */
	dom_node_unref((struct dom_node *) text);
}

/**
 * Add a cdata section to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_cdata_section(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child)
{
	struct dom_cdata_section *cdata, *ins_cdata = NULL;
	dom_string *data;
	dom_exception err;

	/* Create DOM string data for cdata section */
	err = dom_string_create(child->content,
			strlen((const char *) child->content), &data);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for cdata section contents");
		return;
	}

	/* Create cdata section */
	err = dom_document_create_cdata_section(parser->doc, data, &cdata);
	if (err != DOM_NO_ERR) {
		dom_string_unref(data);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for cdata section");
		return;
	}

	/* No longer need data */
	dom_string_unref(data);

	/* Append cdata section to parent */
	err = dom_node_append_child(parent, (struct dom_node *) cdata,
			(struct dom_node **) (void *) &ins_cdata);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) cdata);
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed attaching cdata section");
		return;
	}

	/* We're not interested in the inserted cdata section */
	if (ins_cdata != NULL)
		dom_node_unref((struct dom_node *) ins_cdata);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) cdata,
			child);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) cdata);
		return;
	}

	/* No longer interested in cdata section */
	dom_node_unref((struct dom_node *) cdata);
}

/**
 * Add an entity reference to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_entity_reference(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child)
{
	struct dom_entity_reference *entity, *ins_entity = NULL;
	dom_string *name;
	xmlNodePtr c;
	dom_exception err;

	/* Create name of entity reference */
	err = dom_string_create(child->name,
			strlen((const char *) child->name), &name);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for entity reference name");
		return;
	}

	/* Create text node */
	err = dom_document_create_entity_reference(parser->doc, name,
			&entity);
	if (err != DOM_NO_ERR) {
		dom_string_unref(name);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for entity reference");
		return;
	}

	/* No longer need name */
	dom_string_unref(name);

	/* Mirror subtree (reference value) */
	for (c = child->children; c != NULL; c = c->next) {
		xml_parser_add_node(parser, (struct dom_node *) entity, c);
	}

	/* Append entity reference to parent */
	err = dom_node_append_child(parent, (struct dom_node *) entity,
			(struct dom_node **) (void *) &ins_entity);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) entity);
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed attaching entity reference");
		return;
	}

	/* We're not interested in the inserted entity reference */
	if (ins_entity != NULL)
		dom_node_unref((struct dom_node *) ins_entity);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) entity,
			child);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) entity);
		return;
	}

	/* No longer interested in entity reference */
	dom_node_unref((struct dom_node *) entity);
}

static void xml_parser_add_entity(dom_xml_parser *parser, 
        struct dom_node *parent, xmlNodePtr child)
{
    UNUSED(parser);
    UNUSED(parent);
    UNUSED(child);
}

/**
 * Add a comment to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_comment(dom_xml_parser *parser, struct dom_node *parent,
		xmlNodePtr child)
{
	struct dom_comment *comment, *ins_comment = NULL;
	dom_string *data;
	dom_exception err;

	/* Create DOM string data for comment */
	err = dom_string_create(child->content,
			strlen((const char *) child->content), &data);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"No memory for comment data");
		return;
	}

	/* Create comment */
	err = dom_document_create_comment(parser->doc, data, &comment);
	if (err != DOM_NO_ERR) {
		dom_string_unref(data);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"No memory for comment node");
		return;
	}

	/* No longer need data */
	dom_string_unref(data);

	/* Append comment to parent */
	err = dom_node_append_child(parent, (struct dom_node *) comment,
			(struct dom_node **) (void *) &ins_comment);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) comment);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"Failed attaching comment node");
		return;
	}

	/* We're not interested in the inserted comment */
	if (ins_comment != NULL)
		dom_node_unref((struct dom_node *) ins_comment);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) comment,
			child);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) comment);
		return;
	}

	/* No longer interested in comment */
	dom_node_unref((struct dom_node *) comment);
}

/**
 * Add a document type to the DOM
 *
 * \param parser  The parser context
 * \param parent  The parent DOM node
 * \param child   The xmlNode to mirror in the DOM as a child of parent
 */
void xml_parser_add_document_type(dom_xml_parser *parser,
		struct dom_node *parent, xmlNodePtr child)
{
	xmlDtdPtr dtd = (xmlDtdPtr) child;
	struct dom_document_type *doctype, *ins_doctype = NULL;
	const char *qname, *public_id, *system_id;
	dom_exception err;

	/* Create qname for doctype */
	qname = (const char *) dtd->name;

	/* Create public ID for doctype */
	public_id = dtd->ExternalID != NULL ? 
			(const char *) dtd->ExternalID : "";

	/* Create system ID for doctype */
	system_id = dtd->SystemID != NULL ? 
			(const char *) dtd->SystemID : "";

	/* Create doctype */
	err = dom_implementation_create_document_type(
			qname, public_id, system_id, 
			&doctype);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"Failed to create document type");
		return;
	}

	/* Add doctype to document */
	err = dom_node_append_child(parent, (struct dom_node *) doctype,
			(struct dom_node **) (void *) &ins_doctype);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) doctype);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					"Failed attaching doctype");
		return;
	}

	/* Not interested in inserted node */
	if (ins_doctype != NULL)
		dom_node_unref((struct dom_node *) ins_doctype);

	/* Link nodes together */
	err = xml_parser_link_nodes(parser, (struct dom_node *) doctype,
			child);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) doctype);
		return;
	}

	/* No longer interested in doctype */
	dom_node_unref((struct dom_node *) doctype);
}

/* ------------------------------------------------------------------------*/

void xml_parser_internal_subset(void *ctx, const xmlChar *name,
		const xmlChar *ExternalID, const xmlChar *SystemID)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2InternalSubset(parser->xml_ctx, name, ExternalID, SystemID);
}

int xml_parser_is_standalone(void *ctx)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2IsStandalone(parser->xml_ctx);
}

int xml_parser_has_internal_subset(void *ctx)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2HasInternalSubset(parser->xml_ctx);
}

int xml_parser_has_external_subset(void *ctx)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2HasExternalSubset(parser->xml_ctx);
}

xmlParserInputPtr xml_parser_resolve_entity(void *ctx,
		const xmlChar *publicId, const xmlChar *systemId)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2ResolveEntity(parser->xml_ctx, publicId, systemId);
}

xmlEntityPtr xml_parser_get_entity(void *ctx, const xmlChar *name)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2GetEntity(parser->xml_ctx, name);
}

void xml_parser_entity_decl(void *ctx, const xmlChar *name,
		int type, const xmlChar *publicId, const xmlChar *systemId,
		xmlChar *content)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2EntityDecl(parser->xml_ctx, name, type, publicId, systemId,
			content);
}

void xml_parser_notation_decl(void *ctx, const xmlChar *name,
		const xmlChar *publicId, const xmlChar *systemId)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2NotationDecl(parser->xml_ctx, name, publicId, systemId);
}

void xml_parser_attribute_decl(void *ctx, const xmlChar *elem,
		const xmlChar *fullname, int type, int def,
		const xmlChar *defaultValue, xmlEnumerationPtr tree)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2AttributeDecl(parser->xml_ctx, elem, fullname, type, def,
			defaultValue, tree);
}

void xml_parser_element_decl(void *ctx, const xmlChar *name,
		int type, xmlElementContentPtr content)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2ElementDecl(parser->xml_ctx, name, type, content);
}

void xml_parser_unparsed_entity_decl(void *ctx, const xmlChar *name,
		const xmlChar *publicId, const xmlChar *systemId,
		const xmlChar *notationName)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2UnparsedEntityDecl(parser->xml_ctx, name, publicId,
			systemId, notationName);
}

void xml_parser_set_document_locator(void *ctx, xmlSAXLocatorPtr loc)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2SetDocumentLocator(parser->xml_ctx, loc);
}

void xml_parser_reference(void *ctx, const xmlChar *name)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2Reference(parser->xml_ctx, name);
}

void xml_parser_characters(void *ctx, const xmlChar *ch, int len)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2Characters(parser->xml_ctx, ch, len);
}

void xml_parser_comment(void *ctx, const xmlChar *value)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2Comment(parser->xml_ctx, value);
}

xmlEntityPtr xml_parser_get_parameter_entity(void *ctx, const xmlChar *name)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	return xmlSAX2GetParameterEntity(parser->xml_ctx, name);
}

void xml_parser_cdata_block(void *ctx, const xmlChar *value, int len)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2CDataBlock(parser->xml_ctx, value, len);
}

void xml_parser_external_subset(void *ctx, const xmlChar *name,
		const xmlChar *ExternalID, const xmlChar *SystemID)
{
	dom_xml_parser *parser = (dom_xml_parser *) ctx;

	xmlSAX2ExternalSubset(parser->xml_ctx, name, ExternalID, SystemID);
}
