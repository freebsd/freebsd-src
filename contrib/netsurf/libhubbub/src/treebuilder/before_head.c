/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Handle token in "before head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
hubbub_error handle_before_head(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = process_characters_expect_whitespace(treebuilder,
				token, false);
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
		} else if (type == HEAD) {
			handled = true;
		} else {
			err = HUBBUB_REPROCESS;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML || type == BODY ||
				type == HEAD || type == BR) {
			err = HUBBUB_REPROCESS;
		} else {
			/** \todo parse error */
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		err = HUBBUB_REPROCESS;
		break;
	}

	if (handled || err == HUBBUB_REPROCESS) {
		hubbub_error e;
		hubbub_tag tag;

		if (err == HUBBUB_REPROCESS) {
			/* Manufacture head tag */
			tag.ns = HUBBUB_NS_HTML;
			tag.name.ptr = (const uint8_t *) "head";
			tag.name.len = SLEN("head");

			tag.n_attributes = 0;
			tag.attributes = NULL;
		} else {
			tag = token->data.tag;
		}

		e = insert_element(treebuilder, &tag, true);
		if (e != HUBBUB_OK)
			return e;

		treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		treebuilder->context.head_element = 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node;

		treebuilder->context.mode = IN_HEAD;
	}

	return err;
}

