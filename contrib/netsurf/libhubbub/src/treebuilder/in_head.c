/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include <parserutils/charset/mibenum.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"

#include "charset/detect.h"

#include "utils/utils.h"
#include "utils/string.h"


/**
 * Process a meta tag as if "in head".
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
static hubbub_error process_meta_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	static uint16_t utf16, utf16be, utf16le;
	uint16_t charset_enc = 0;
	uint16_t content_type_enc = 0;
	size_t i;
	hubbub_error err = HUBBUB_OK;

	err = insert_element(treebuilder, &token->data.tag, false);
	if (err != HUBBUB_OK)
		return err;

	/** \todo ack sc flag */

	if (treebuilder->tree_handler->encoding_change == NULL)
		return err;

	/* Grab UTF-16 MIBenums */
	if (utf16 == 0) {
		utf16 = parserutils_charset_mibenum_from_name(
				"utf-16", SLEN("utf-16"));
		utf16be = parserutils_charset_mibenum_from_name(
				"utf-16be", SLEN("utf-16be"));
		utf16le = parserutils_charset_mibenum_from_name(
				"utf-16le", SLEN("utf-16le"));
		assert(utf16 != 0 && utf16be != 0 && utf16le != 0);
	}

	for (i = 0; i < token->data.tag.n_attributes; i++) {
		hubbub_attribute *attr = &token->data.tag.attributes[i];

		if (hubbub_string_match(attr->name.ptr, attr->name.len,
				(const uint8_t *) "charset",
				SLEN("charset")) == true) {
			/* Extract charset */
			charset_enc = parserutils_charset_mibenum_from_name(
					(const char *) attr->value.ptr,
					attr->value.len);
		} else if (hubbub_string_match(attr->name.ptr, attr->name.len,
				(const uint8_t *) "content",
				SLEN("content")) == true) {
			/* Extract charset from Content-Type */
			content_type_enc = hubbub_charset_parse_content(
					attr->value.ptr, attr->value.len);
		}
	}

	/* Fall back, if necessary */
	if (charset_enc == 0 && content_type_enc != 0)
		charset_enc = content_type_enc;

	if (charset_enc != 0) {
		const char *name;

		hubbub_charset_fix_charset(&charset_enc);

		/* Change UTF-16 to UTF-8 */
		if (charset_enc == utf16le || charset_enc == utf16be ||
				charset_enc == utf16) {
			charset_enc = parserutils_charset_mibenum_from_name(
					"UTF-8", SLEN("UTF-8"));
		}

		name = parserutils_charset_mibenum_to_name(charset_enc);

		err = treebuilder->tree_handler->encoding_change(
				treebuilder->tree_handler->ctx,	name);
	}

	return err;
}

/**
 * Handle token in "in head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
hubbub_error handle_in_head(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = process_characters_expect_whitespace(treebuilder,
				token, true);
		break;
	case HUBBUB_TOKEN_COMMENT:
		err = process_comment_append(treebuilder, token,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);
		break;
	case HUBBUB_TOKEN_DOCTYPE:
		/** \todo parse error */
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			/* Process as if "in body" */
			err = handle_in_body(treebuilder, token);
		} else if (type == BASE || type == COMMAND || type == LINK) {
			err = insert_element(treebuilder, &token->data.tag, 
					false);

			/** \todo ack sc flag */
		} else if (type == META) {
			err = process_meta_in_head(treebuilder, token);
		} else if (type == TITLE) {
			err = parse_generic_rcdata(treebuilder, token, true);
		} else if (type == NOFRAMES || type == STYLE) {
			err = parse_generic_rcdata(treebuilder, token, false);
		} else if (type == NOSCRIPT) {
			if (treebuilder->context.enable_scripting) {
				err = parse_generic_rcdata(treebuilder, token, 
						false);
			} else {
				err = insert_element(treebuilder, 
						&token->data.tag, true);
				if (err != HUBBUB_OK)
					return err;

				treebuilder->context.mode = IN_HEAD_NOSCRIPT;
			}
		} else if (type == SCRIPT) {
			/** \todo need to ensure that the client callback
			 * sets the parser-inserted/already-executed script 
			 * flags. */
			err = parse_generic_rcdata(treebuilder, token, false);
		} else if (type == HEAD) {
			/** \todo parse error */
		} else {
			err = HUBBUB_REPROCESS;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HEAD) {
			handled = true;
		} else if (type == HTML || type == BODY || type == BR) {
			err = HUBBUB_REPROCESS;
		} /** \todo parse error */
	}
		break;
	case HUBBUB_TOKEN_EOF:
		err = HUBBUB_REPROCESS;
		break;
	}

	if (handled || err == HUBBUB_REPROCESS) {
		hubbub_ns ns;
		element_type otype;
		void *node;

		element_stack_pop(treebuilder, &ns, &otype, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		treebuilder->context.mode = AFTER_HEAD;
	}

	return err;
}
