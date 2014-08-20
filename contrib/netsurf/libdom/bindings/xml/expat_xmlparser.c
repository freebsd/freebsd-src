/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2012 Daniel Silverstone <dsilvers@netsurf-browser.org>
 */

#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <stdlib.h>
#include <stdio.h>

#include <dom/dom.h>

#include "xmlparser.h"
#include "utils.h"

#include <expat.h>

/**
 * expat XML parser object
 */
struct dom_xml_parser {
	dom_msg msg;			/**< Informational message function */
	void *mctx;			/**< Pointer to client data */
	XML_Parser parser;		/**< expat parser context */
	struct dom_document *doc;	/**< DOM Document we're building */
	struct dom_node *current;	/**< DOM node we're currently building */
	bool is_cdata;			/**< If the character data is cdata or text */
};

/* Binding functions */

static void
expat_xmlparser_start_element_handler(void *_parser,
				      const XML_Char *name,
				      const XML_Char **atts)
{
	dom_xml_parser *parser = _parser;
	dom_exception err;
	dom_element *elem, *ins_elem;
	dom_string *tag_name;
	dom_string *namespace = NULL;
	const XML_Char *ns_sep = strchr(name, '\n');

	if (ns_sep != NULL) {
		err = dom_string_create_interned((const uint8_t *)name,
						 ns_sep - name,
						 &namespace);
		if (err != DOM_NO_ERR) {
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				    "No memory for namespace name");
			return;
		}
		name = ns_sep + 1;
	}

	err = dom_string_create_interned((const uint8_t *)name,
					 strlen(name),
					 &tag_name);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "No memory for tag name");
		if (namespace != NULL)
			dom_string_unref(namespace);
		return;
	}

	if (namespace == NULL)
		err = dom_document_create_element(parser->doc,
						  tag_name, &elem);
	else
		err = dom_document_create_element_ns(parser->doc, namespace,
						     tag_name, &elem);
	if (err != DOM_NO_ERR) {
		if (namespace != NULL)
			dom_string_unref(namespace);
		dom_string_unref(tag_name);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "Failed to create element '%s'", name);
		return;
	}

	dom_string_unref(tag_name);
	if (namespace != NULL)
		dom_string_unref(namespace);

	/* Add attributes to the element */
	while (*atts) {
		dom_string *key, *value;
		ns_sep = strchr(*atts, '\n');
		if (ns_sep != NULL) {
			err = dom_string_create_interned((const uint8_t *)(*atts),
							 ns_sep - (*atts),
							 &namespace);
			if (err != DOM_NO_ERR) {
				parser->msg(DOM_MSG_CRITICAL, parser->mctx,
					    "No memory for attr namespace");
				dom_node_unref(elem);
				return;
			}
		} else
			namespace = NULL;
		if (ns_sep == NULL)
			err = dom_string_create_interned((const uint8_t *)(*atts),
							 strlen(*atts), &key);
		else
			err = dom_string_create_interned((const uint8_t *)(ns_sep + 1),
							 strlen(ns_sep + 1),
							 &key);
		if (err != DOM_NO_ERR) {
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				    "No memory for attribute name");
			if (namespace != NULL)
				dom_string_unref(namespace);
			dom_node_unref(elem);
			return;
		}
		atts++;
		err = dom_string_create((const uint8_t *)(*atts),
					strlen(*atts), &value);
		if (err != DOM_NO_ERR) {
			dom_node_unref(elem);
			if (namespace != NULL)
				dom_string_unref(namespace);
			dom_string_unref(key);
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				    "No memory for attribute value");
			return;
		}
		atts++;

		if (namespace == NULL)
			err = dom_element_set_attribute(elem, key, value);
		else
			err = dom_element_set_attribute_ns(elem, namespace,
							   key, value);
	        if (namespace != NULL)
			dom_string_unref(namespace);
		dom_string_unref(key);
		dom_string_unref(value);
		if (err != DOM_NO_ERR) {
			dom_node_unref(elem);
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				    "No memory for setting attribute");
			return;
		}
	}

	err = dom_node_append_child(parser->current, (struct dom_node *) elem,
				    (struct dom_node **) (void *) &ins_elem);
	if (err != DOM_NO_ERR) {
		dom_node_unref(elem);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "No memory for appending child node");
		return;
	}

	dom_node_unref(ins_elem);

	dom_node_unref(parser->current);
	parser->current = (struct dom_node *)elem; /* Steal initial ref */
}

static void
expat_xmlparser_end_element_handler(void *_parser,
				    const XML_Char *name)
{
	dom_xml_parser *parser = _parser;
	dom_exception err;
	dom_node *parent;

	UNUSED(name);

	err = dom_node_get_parent_node(parser->current, &parent);

	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "Unable to find a parent while closing element.");
		return;
	}

	dom_node_unref(parser->current);
	parser->current = parent;  /* Takes the ref given by get_parent_node */
}

static void
expat_xmlparser_start_cdata_handler(void *_parser)
{
	dom_xml_parser *parser = _parser;

	parser->is_cdata = true;
}

static void
expat_xmlparser_end_cdata_handler(void *_parser)
{
	dom_xml_parser *parser = _parser;

	parser->is_cdata = false;
}

static void
expat_xmlparser_cdata_handler(void *_parser,
			      const XML_Char *s,
			      int len)
{
	dom_xml_parser *parser = _parser;
	dom_string *data;
	dom_exception err;
	struct dom_node *cdata, *ins_cdata, *lastchild = NULL;
	dom_node_type ntype = 0;

	err = dom_string_create((const uint8_t *)s, len, &data);
	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "No memory for cdata section contents");
		return;
	}

	err = dom_node_get_last_child(parser->current, &lastchild);

	if (err == DOM_NO_ERR && lastchild != NULL) {
		err = dom_node_get_node_type(lastchild, &ntype);
	}

	if (err != DOM_NO_ERR) {
		dom_string_unref(data);
		if (lastchild != NULL)
			dom_node_unref(lastchild);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "No memory for cdata section");
		return;
	}

	if (ntype == DOM_TEXT_NODE && parser->is_cdata == false) {
		/* We can append this text instead */
		err = dom_characterdata_append_data(
			(dom_characterdata *)lastchild, data);
		dom_string_unref(data);
		if (lastchild != NULL)
			dom_node_unref(lastchild);
		if (err != DOM_NO_ERR) {
			parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				    "No memory for cdata section");
		}
		return;
	}

	if (lastchild != NULL)
		dom_node_unref(lastchild);

	/* We can't append directly, so make a new node */
	err = parser->is_cdata ?
		dom_document_create_cdata_section(parser->doc, data,
				(dom_cdata_section **) (void *) &cdata) :
		dom_document_create_text_node(parser->doc, data,
					      (dom_text **) (void *) &cdata);
	if (err != DOM_NO_ERR) {
		dom_string_unref(data);
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "No memory for cdata section");
		return;
	}

	/* No longer need data */
	dom_string_unref(data);

	/* Append cdata section to parent */
	err = dom_node_append_child(parser->current, cdata, &ins_cdata);
	if (err != DOM_NO_ERR) {
		dom_node_unref((struct dom_node *) cdata);
		parser->msg(DOM_MSG_ERROR, parser->mctx,
				"Failed attaching cdata section");
		return;
	}

	/* We're not interested in the inserted cdata section */
	if (ins_cdata != NULL)
		dom_node_unref(ins_cdata);

	/* No longer interested in cdata section */
	dom_node_unref(cdata);
}

static int
expat_xmlparser_external_entity_ref_handler(XML_Parser parser,
					    const XML_Char *context,
					    const XML_Char *base,
					    const XML_Char *system_id,
					    const XML_Char *public_id)
{
	FILE *fh;
	XML_Parser subparser;
	unsigned char data[1024];
	size_t len;
	enum XML_Status status;

	UNUSED(base);
	UNUSED(public_id);

	if (system_id == NULL)
		return XML_STATUS_OK;

	fh = fopen(system_id, "r");

	if (fh == NULL)
		return XML_STATUS_OK;

	subparser = XML_ExternalEntityParserCreate(parser,
						   context,
						   NULL);

	if (subparser == NULL) {
		fclose(fh);
		return XML_STATUS_OK;
	}

	/* Parse the file bit by bit */
	while ((len = fread(data, 1, 1024, fh)) > 0) {
		status = XML_Parse(subparser, (const char *)data, len, 0);
		if (status != XML_STATUS_OK) {
			XML_ParserFree(subparser);
			fclose(fh);
			return XML_STATUS_OK;
		}
	}
	XML_Parse(subparser, "", 0, 1);
	XML_ParserFree(subparser);
	fclose(fh);
	return XML_STATUS_OK;
}

static void
expat_xmlparser_comment_handler(void *_parser,
				const XML_Char *_comment)
{
	dom_xml_parser *parser = _parser;
	struct dom_comment *comment, *ins_comment = NULL;
	dom_string *data;
	dom_exception err;

	/* Create DOM string data for comment */
	err = dom_string_create((const uint8_t *)_comment,
			strlen((const char *) _comment), &data);
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
	err = dom_node_append_child(parser->current, (struct dom_node *) comment,
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

	/* No longer interested in comment */
	dom_node_unref((struct dom_node *) comment);

}

static void
expat_xmlparser_start_doctype_decl_handler(void *_parser,
					   const XML_Char *doctype_name,
					   const XML_Char *system_id,
					   const XML_Char *public_id,
					   int has_internal_subset)
{
	dom_xml_parser *parser = _parser;
	struct dom_document_type *doctype, *ins_doctype = NULL;
	dom_exception err;

	UNUSED(has_internal_subset);

	err = dom_implementation_create_document_type(
		doctype_name, system_id ? system_id : "",
		public_id ? public_id : "",
		&doctype);

	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
				"Failed to create document type");
		return;
	}

	/* Add doctype to document */
	err = dom_node_append_child(parser->doc, (struct dom_node *) doctype,
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

	/* No longer interested in doctype */
	dom_node_unref((struct dom_node *) doctype);
}

static void
expat_xmlparser_unknown_data_handler(void *_parser,
				     const XML_Char *s,
				     int len)
{
	UNUSED(_parser);
	UNUSED(s);
	UNUSED(len);
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
 * int_enc is ignored due to it being made of bees.
 */
dom_xml_parser *
dom_xml_parser_create(const char *enc, const char *int_enc,
		      dom_msg msg, void *mctx, dom_document **document)
{
	dom_xml_parser *parser;
	dom_exception err;

	UNUSED(int_enc);

	parser = calloc(sizeof(*parser), 1);
	if (parser == NULL) {
		msg(DOM_MSG_CRITICAL, mctx, "No memory for parser");
		return NULL;
	}

	parser->msg = msg;
	parser->mctx = mctx;

	parser->parser = XML_ParserCreateNS(enc, '\n');

	if (parser->parser == NULL) {
		free(parser);
		msg(DOM_MSG_CRITICAL, mctx, "No memory for parser");
		return NULL;
	}

	parser->doc = NULL;

	err = dom_implementation_create_document(
		DOM_IMPLEMENTATION_XML,
		/* namespace */ NULL,
		/* qname */ NULL,
		/* doctype */ NULL,
		NULL,
		NULL,
		document);

	if (err != DOM_NO_ERR) {
		parser->msg(DOM_MSG_CRITICAL, parser->mctx,
			    "Failed creating document");
		XML_ParserFree(parser->parser);
		free(parser);
		return NULL;
	}

	parser->doc = (dom_document *) dom_node_ref(*document);

	XML_SetUserData(parser->parser, parser);

	XML_SetElementHandler(parser->parser,
			      expat_xmlparser_start_element_handler,
			      expat_xmlparser_end_element_handler);

	XML_SetCdataSectionHandler(parser->parser,
				   expat_xmlparser_start_cdata_handler,
				   expat_xmlparser_end_cdata_handler);

	XML_SetCharacterDataHandler(parser->parser,
				    expat_xmlparser_cdata_handler);

	XML_SetParamEntityParsing(parser->parser,
				  XML_PARAM_ENTITY_PARSING_ALWAYS);

	XML_SetExternalEntityRefHandler(parser->parser,
					expat_xmlparser_external_entity_ref_handler);

	XML_SetCommentHandler(parser->parser,
			      expat_xmlparser_comment_handler);

	XML_SetStartDoctypeDeclHandler(parser->parser,
				       expat_xmlparser_start_doctype_decl_handler);

	XML_SetDefaultHandlerExpand(parser->parser,
			      expat_xmlparser_unknown_data_handler);

	parser->current = dom_node_ref(parser->doc);

	parser->is_cdata = false;

	return parser;
}

/**
 * Destroy an XML parser instance
 *
 * \param parser  The parser instance to destroy
 */
void
dom_xml_parser_destroy(dom_xml_parser *parser)
{
	XML_ParserFree(parser->parser);
	if (parser->current != NULL)
		dom_node_unref(parser->current);
	dom_node_unref(parser->doc);
	free(parser);
}

/**
 * Parse a chunk of data
 *
 * \param parser  The XML parser instance to use for parsing
 * \param data    Pointer to data chunk
 * \param len     Byte length of data chunk
 * \return DOM_XML_OK on success, DOM_XML_EXTERNAL_ERR | <expat error> on failure
 */
dom_xml_error
dom_xml_parser_parse_chunk(dom_xml_parser *parser, uint8_t *data, size_t len)
{
	enum XML_Status status;

	status = XML_Parse(parser->parser, (const char *)data, len, 0);
	if (status != XML_STATUS_OK) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
			    "XML_Parse failed: %d", status);
		return DOM_XML_EXTERNAL_ERR | status;
	}

	return DOM_XML_OK;
}

/**
 * Notify parser that datastream is empty
 *
 * \param parser  The XML parser instance to notify
 * \return DOM_XML_OK on success, DOM_XML_EXTERNAL_ERR | <expat error> on failure
 *
 * This will force any remaining data through the parser
 */
dom_xml_error
dom_xml_parser_completed(dom_xml_parser *parser)
{
	enum XML_Status status;

	status = XML_Parse(parser->parser, "", 0, 1);
	if (status != XML_STATUS_OK) {
		parser->msg(DOM_MSG_ERROR, parser->mctx,
			    "XML_Parse failed: %d", status);
		return DOM_XML_EXTERNAL_ERR | status;
	}

	return DOM_XML_OK;

}
