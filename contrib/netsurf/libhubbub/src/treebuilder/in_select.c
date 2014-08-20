/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell <takkaria@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"


/**
 * Handle token in "in head" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to handle
 * \return True to reprocess token, false otherwise
 */
hubbub_error handle_in_select(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;

	hubbub_ns ns;
	element_type otype;
	void *node;

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = append_text(treebuilder, &token->data.character);
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
		} else if (type == OPTION) {
			if (current_node(treebuilder) == OPTION) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			}

			err = insert_element(treebuilder, &token->data.tag, 
					true);
		} else if (type == OPTGROUP) {
			if (current_node(treebuilder) == OPTION) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			}

			if (current_node(treebuilder) == OPTGROUP) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			}

			err = insert_element(treebuilder, &token->data.tag, 
					true);
		} else if (type == SELECT || type == INPUT ||
				type == TEXTAREA) {

			if (element_in_scope(treebuilder, SELECT, true)) {
				element_stack_pop_until(treebuilder, 
						SELECT);
				reset_insertion_mode(treebuilder);
			} else {
				/* fragment case */
				/** \todo parse error */
			}

			if (type != SELECT)
				err = HUBBUB_REPROCESS;
		} else if (type == SCRIPT) {
			err = handle_in_head(treebuilder, token);
		} else {
			/** \todo parse error */
		}
	}
		break;
	case HUBBUB_TOKEN_END_TAG:
	{
		element_type type = element_type_from_name(treebuilder,
				&token->data.tag.name);

		if (type == OPTGROUP) {
			if (current_node(treebuilder) == OPTION &&
					prev_node(treebuilder) == OPTGROUP) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			}

			if (current_node(treebuilder) == OPTGROUP) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			} else {
				/** \todo parse error */
			}
		} else if (type == OPTION) {
			if (current_node(treebuilder) == OPTION) {
				element_stack_pop(treebuilder, &ns, &otype,
						&node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);
			} else {
				/** \todo parse error */
			}
		} else if (type == SELECT) {
			if (element_in_scope(treebuilder, SELECT, true)) {
				element_stack_pop_until(treebuilder, 
						SELECT);
				reset_insertion_mode(treebuilder);
			} else {
				/* fragment case */
				/** \todo parse error */
			}
		}
	}
		break;
	case HUBBUB_TOKEN_EOF:
		break;
	}

	return err;
}

