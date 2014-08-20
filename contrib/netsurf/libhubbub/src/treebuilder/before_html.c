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
 * Handle token in "before html" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
hubbub_error handle_before_html(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	bool handled = false;

	switch (token->type) {
	case HUBBUB_TOKEN_DOCTYPE:
		/** \todo parse error */
		break;
	case HUBBUB_TOKEN_COMMENT:
		err = process_comment_append(treebuilder, token,
				treebuilder->context.document);
		break;
	case HUBBUB_TOKEN_CHARACTER:
		err = process_characters_expect_whitespace(treebuilder,
				token, false);
		break;
	case HUBBUB_TOKEN_START_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == HTML) {
			handled = true;
		} else {
			err = HUBBUB_REPROCESS;
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	case HUBBUB_TOKEN_EOF:
		err = HUBBUB_REPROCESS;
		break;
	}


	if (handled || err == HUBBUB_REPROCESS) {
		hubbub_error e;
		void *html, *appended;

		/* We can't use insert_element() here, as it assumes
		 * that we're inserting into current_node. There is
		 * no current_node to insert into at this point so
		 * we get to do it manually. */

		if (err == HUBBUB_REPROCESS) {
			/* Need to manufacture html element */
			hubbub_tag tag;

			tag.ns = HUBBUB_NS_HTML;
			tag.name.ptr = (const uint8_t *) "html";
			tag.name.len = SLEN("html");

			tag.n_attributes = 0;
			tag.attributes = NULL;

			e = treebuilder->tree_handler->create_element(
					treebuilder->tree_handler->ctx,
					&tag, &html);
		} else {
			e = treebuilder->tree_handler->create_element(
					treebuilder->tree_handler->ctx,
					&token->data.tag, &html);
		}

		if (e != HUBBUB_OK)
			return e;

		e = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.document,
				html, &appended);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				html);

		if (e != HUBBUB_OK)
			return e;

		/* We can't use element_stack_push() here, as it 
		 * assumes that current_node is pointing at the index 
		 * before the one to insert at. For the first entry in 
		 * the stack, this does not hold so we must insert
		 * manually. */
		treebuilder->context.element_stack[0].type = HTML;
		treebuilder->context.element_stack[0].node = appended;
		treebuilder->context.current_node = 0;

		/** \todo cache selection algorithm */

		treebuilder->context.mode = BEFORE_HEAD;
	}

	return err;
}

