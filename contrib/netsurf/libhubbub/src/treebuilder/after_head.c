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
 * Handle tokens in "after head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
hubbub_error handle_after_head(hubbub_treebuilder *treebuilder,
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
		} else if (type == BODY) {
			handled = true;
		} else if (type == FRAMESET) {
			err = insert_element(treebuilder, &token->data.tag, 
				true);
			if (err == HUBBUB_OK)
				treebuilder->context.mode = IN_FRAMESET;
		} else if (type == BASE || type == LINK || type == META ||
				type == NOFRAMES || type == SCRIPT ||
				type == STYLE || type == TITLE) {
			hubbub_ns ns;
			element_type otype;
			void *node;
			uint32_t index;

			/** \todo parse error */

			err = element_stack_push(treebuilder,
					HUBBUB_NS_HTML,
					HEAD,
					treebuilder->context.head_element);
			if (err != HUBBUB_OK)
				return err;

			index = treebuilder->context.current_node;

			/* Process as "in head" */
			err = handle_in_head(treebuilder, token);

			element_stack_remove(treebuilder, index,
					&ns, &otype, &node);

			/* No need to unref node as we never increased
			 * its reference count when pushing it on the stack */
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

		if (type == HTML || type == BODY || type == BR) {
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
			/* Manufacture body */
			tag.ns = HUBBUB_NS_HTML;
			tag.name.ptr = (const uint8_t *) "body";
			tag.name.len = SLEN("body");

			tag.n_attributes = 0;
			tag.attributes = NULL;
		} else {
			tag = token->data.tag;
		}

		e = insert_element(treebuilder, &tag, true);
		if (e != HUBBUB_OK)
			return e;

		treebuilder->context.mode = IN_BODY;
	}

	return err;
}

