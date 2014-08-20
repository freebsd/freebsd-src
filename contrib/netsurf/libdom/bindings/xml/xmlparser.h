/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef xml_xmlparser_h_
#define xml_xmlparser_h_

#include <stddef.h>
#include <inttypes.h>

#include <dom/dom.h>

#include "xmlerror.h"

typedef struct dom_xml_parser dom_xml_parser;

/* Create an XML parser instance */
dom_xml_parser *dom_xml_parser_create(const char *enc, const char *int_enc,
		dom_msg msg, void *mctx, dom_document **document);

/* Destroy an XML parser instance */
void dom_xml_parser_destroy(dom_xml_parser *parser);

/* Parse a chunk of data */
dom_xml_error dom_xml_parser_parse_chunk(dom_xml_parser *parser,
		uint8_t *data, size_t len);

/* Notify parser that datastream is empty */
dom_xml_error dom_xml_parser_completed(dom_xml_parser *parser);

#endif
