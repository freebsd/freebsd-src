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

#undef DEBUG_IN_BODY

/**
 * Bookmark for formatting list. Used in adoption agency
 */
typedef struct bookmark {
	formatting_list_entry *prev;	/**< Previous entry */
	formatting_list_entry *next;	/**< Next entry */
} bookmark;

static hubbub_error process_character(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_start_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_end_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

static hubbub_error process_html_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_body_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_frameset_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_container_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token);
static hubbub_error process_hN_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_form_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type);
static hubbub_error process_plaintext_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_a_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_presentational_in_body(
		hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type);
static hubbub_error process_nobr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_button_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_applet_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, const hubbub_token *token, 
		element_type type);
static hubbub_error process_hr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_image_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_isindex_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_textarea_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_select_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_opt_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
static hubbub_error process_phrasing_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

static hubbub_error process_0body_in_body(hubbub_treebuilder *treebuilder);
static hubbub_error process_0container_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static hubbub_error process_0form_in_body(hubbub_treebuilder *treebuilder);
static hubbub_error process_0p_in_body(hubbub_treebuilder *treebuilder);
static hubbub_error process_0dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static hubbub_error process_0h_in_body(hubbub_treebuilder *treebuilder,
		element_type type);
static hubbub_error process_0presentational_in_body(
		hubbub_treebuilder *treebuilder,
		element_type type);
static hubbub_error process_0applet_button_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, element_type type);
static hubbub_error process_0br_in_body(hubbub_treebuilder *treebuilder);
static hubbub_error process_0generic_in_body(hubbub_treebuilder *treebuilder, 
		element_type type);

static hubbub_error aa_find_and_validate_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type,
		formatting_list_entry **element);
static formatting_list_entry *aa_find_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type);
static hubbub_error aa_find_furthest_block(hubbub_treebuilder *treebuilder,
		formatting_list_entry *formatting_element, 
		uint32_t *furthest_block);
static hubbub_error aa_reparent_node(hubbub_treebuilder *treebuilder, 
		void *node, void *new_parent, void **reparented);
static hubbub_error aa_find_bookmark_location_reparenting_misnested(
		hubbub_treebuilder *treebuilder, 
		uint32_t formatting_element, uint32_t *furthest_block,
		bookmark *bookmark, uint32_t *last_node);
static hubbub_error aa_remove_element_stack_item(
		hubbub_treebuilder *treebuilder, 
		uint32_t index, uint32_t limit);
static hubbub_error aa_clone_and_replace_entries(
		hubbub_treebuilder *treebuilder,
		formatting_list_entry *element);


/**
 * Handle tokens in "in body" insertion mode
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token, false otherwise
 */
hubbub_error handle_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	uint32_t i;

#if !defined(NDEBUG) && defined(DEBUG_IN_BODY)
	fprintf(stdout, "Processing token %d\n", token->type);
	element_stack_dump(treebuilder, stdout);
	formatting_list_dump(treebuilder, stdout);
#endif

	if (treebuilder->context.strip_leading_lr &&
			token->type != HUBBUB_TOKEN_CHARACTER) {
		/* Reset the LR stripping flag */
		treebuilder->context.strip_leading_lr = false;
	}

	switch (token->type) {
	case HUBBUB_TOKEN_CHARACTER:
		err = process_character(treebuilder, token);
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
		err = process_start_tag(treebuilder, token);
		break;
	case HUBBUB_TOKEN_END_TAG:
		err = process_end_tag(treebuilder, token);
		break;
	case HUBBUB_TOKEN_EOF:
		for (i = treebuilder->context.current_node; 
				i > 0; i--) {
			element_type type = 
				treebuilder->context.element_stack[i].type;

			if (!(type == DD || type == DT || type == LI ||
					type == P || type == TBODY || 
					type == TD || type == TFOOT ||
					type == TH || type == THEAD ||
					type == TR || type == BODY)) {
				/** \todo parse error */
				break;
			}
		}
		break;
	}

#if !defined(NDEBUG) && defined(DEBUG_IN_BODY)
	fprintf(stdout, "Processed\n");
	element_stack_dump(treebuilder, stdout);
	formatting_list_dump(treebuilder, stdout);
#endif

	return err;
}

/**
 * Process a character token
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_character(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	hubbub_string dummy = token->data.character;
	bool lr_flag = treebuilder->context.strip_leading_lr;
	const uint8_t *p;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	if (treebuilder->context.strip_leading_lr) {
		const uint8_t *str = dummy.ptr;

		if (*str == '\n') {
			dummy.ptr++;
			dummy.len--;
		}

		treebuilder->context.strip_leading_lr = false;
	}

	if (dummy.len) {
		err = append_text(treebuilder, &dummy);
		if (err != HUBBUB_OK) {
			/* Restore LR stripping flag */
			treebuilder->context.strip_leading_lr = lr_flag;

			return err;
		}
	}

	if (treebuilder->context.frameset_ok) {
		for (p = dummy.ptr; p < dummy.ptr + dummy.len; p++) {
			if (*p != 0x0009 && *p != 0x000a &&
					*p != 0x000c && *p != 0x0020) {
				treebuilder->context.frameset_ok = false;
				break;
			}
		}
	}

	return HUBBUB_OK;
}

/**
 * Process a start tag
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return HUBBUB_OK on success,
 *         HUBBUB_REPROCESS to reprocess the token,
 *         appropriate error otherwise.
 */
hubbub_error process_start_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	element_type type = element_type_from_name(treebuilder,
			&token->data.tag.name);

	if (type == HTML) {
		err = process_html_in_body(treebuilder, token);
	} else if (type == BASE || type == COMMAND || type == LINK ||
			type == META || type == NOFRAMES || type == SCRIPT ||
			type == STYLE || type == TITLE) {
		/* Process as "in head" */
		err = handle_in_head(treebuilder, token);
	} else if (type == BODY) {
		err = process_body_in_body(treebuilder, token);
	} else if (type == FRAMESET) {
		err = process_frameset_in_body(treebuilder, token);
	} else if (type == ADDRESS || type == ARTICLE || type == ASIDE ||
			type == BLOCKQUOTE || type == CENTER ||
			type == DATAGRID || type == DETAILS ||
			type == DIALOG || type == DIR ||
			type == DIV || type == DL || type == FIELDSET ||
			type == FIGURE || type == FOOTER ||
			type == HEADER || type == MENU || type == NAV ||
			type == OL || type == P || type == SECTION ||
			type == UL) {
		err = process_container_in_body(treebuilder, token);
	} else if (type == H1 || type == H2 || type == H3 ||
			type == H4 || type == H5 || type == H6) {
		err = process_hN_in_body(treebuilder, token);
	} else if (type == PRE || type == LISTING) {
		err = process_container_in_body(treebuilder, token);

		if (err == HUBBUB_OK) {
			treebuilder->context.strip_leading_lr = true;
			treebuilder->context.frameset_ok = false;
		}
	} else if (type == FORM) {
		err = process_form_in_body(treebuilder, token);
	} else if (type == DD || type == DT || type == LI) {
		err = process_dd_dt_li_in_body(treebuilder, token, type);
	} else if (type == PLAINTEXT) {
		err = process_plaintext_in_body(treebuilder, token);
	} else if (type == A) {
		err = process_a_in_body(treebuilder, token);
	} else if (type == B || type == BIG || type == CODE || type == EM || 
			type == FONT || type == I || type == S || 
			type == SMALL || type == STRIKE || 
			type == STRONG || type == TT || type == U) {
		err = process_presentational_in_body(treebuilder, 
				token, type);
	} else if (type == NOBR) {
		err = process_nobr_in_body(treebuilder, token);
	} else if (type == BUTTON) {
		err = process_button_in_body(treebuilder, token);
	} else if (type == APPLET || type == MARQUEE || 
			type == OBJECT) {
		err = process_applet_marquee_object_in_body(treebuilder,
				token, type);
	} else if (type == XMP) {
		err = reconstruct_active_formatting_list(treebuilder);
		if (err != HUBBUB_OK)
			return err;

		treebuilder->context.frameset_ok = false;

		err = parse_generic_rcdata(treebuilder, token, false);
	} else if (type == TABLE) {
		err = process_container_in_body(treebuilder, token);
		if (err == HUBBUB_OK) {
			treebuilder->context.frameset_ok = false;

			treebuilder->context.element_stack[
				current_table(treebuilder)].tainted = false;
			treebuilder->context.mode = IN_TABLE;
		}
	} else if (type == AREA || type == BASEFONT || 
			type == BGSOUND || type == BR || 
			type == EMBED || type == IMG || type == INPUT ||
			type == PARAM || type == SPACER || type == WBR) {
		err = reconstruct_active_formatting_list(treebuilder);
		if (err != HUBBUB_OK)
			return err;

		err = insert_element(treebuilder, &token->data.tag, false);
		if (err == HUBBUB_OK)
			treebuilder->context.frameset_ok = false;
	} else if (type == HR) {
		err = process_hr_in_body(treebuilder, token);
	} else if (type == IMAGE) {
		err = process_image_in_body(treebuilder, token);
	} else if (type == ISINDEX) {
		err = process_isindex_in_body(treebuilder, token);
	} else if (type == TEXTAREA) {
		err = process_textarea_in_body(treebuilder, token);
	} else if (type == IFRAME || type == NOEMBED || 
			type == NOFRAMES || 
			(treebuilder->context.enable_scripting && 
			type == NOSCRIPT)) {
		if (type == IFRAME)
			treebuilder->context.frameset_ok = false;
		err = parse_generic_rcdata(treebuilder, token, false);
	} else if (type == SELECT) {
		err = process_select_in_body(treebuilder, token);
		if (err != HUBBUB_OK)
			return err;

		if (treebuilder->context.mode == IN_BODY) {
			treebuilder->context.mode = IN_SELECT;
		} else if (treebuilder->context.mode == IN_TABLE ||
				treebuilder->context.mode == IN_CAPTION ||
				treebuilder->context.mode == IN_COLUMN_GROUP ||
				treebuilder->context.mode == IN_TABLE_BODY ||
				treebuilder->context.mode == IN_ROW ||
				treebuilder->context.mode == IN_CELL) {
			treebuilder->context.mode = IN_SELECT_IN_TABLE;
		}
	} else if (type == OPTGROUP || type == OPTION) {
		err = process_opt_in_body(treebuilder, token);
	} else if (type == RP || type == RT) {
		/** \todo ruby */
	} else if (type == MATH || type == SVG) {
		hubbub_tag tag = token->data.tag;

		err = reconstruct_active_formatting_list(treebuilder);
		if (err != HUBBUB_OK)
			return err;

		adjust_foreign_attributes(treebuilder, &tag);

		if (type == SVG) {
			adjust_svg_attributes(treebuilder, &tag);
			tag.ns = HUBBUB_NS_SVG;
		} else {
			adjust_mathml_attributes(treebuilder, &tag);
			tag.ns = HUBBUB_NS_MATHML;
		}

		if (token->data.tag.self_closing) {
			err = insert_element(treebuilder, &tag, false);
			/** \todo ack sc flag */
		} else {
			err = insert_element(treebuilder, &tag, true);
			if (err == HUBBUB_OK) {
				treebuilder->context.second_mode =
						treebuilder->context.mode;
				treebuilder->context.mode = IN_FOREIGN_CONTENT;
			}
		}
	} else if (type == CAPTION || type == COL || type == COLGROUP ||
			type == FRAME || type == HEAD || type == TBODY ||
			type == TD || type == TFOOT || type == TH ||
			type == THEAD || type == TR) {
		/** \todo parse error */
	} else {
		err = process_phrasing_in_body(treebuilder, token);
	}

	return err;
}

/**
 * Process an end tag
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \return True to reprocess the token
 */
hubbub_error process_end_tag(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;
	element_type type = element_type_from_name(treebuilder,
			&token->data.tag.name);

	if (type == BODY) {
		err = process_0body_in_body(treebuilder);
		/* Never reprocess */
		if (err == HUBBUB_REPROCESS)
			err = HUBBUB_OK;
	} else if (type == HTML) {
		/* Act as if </body> has been seen then, if
		 * that wasn't ignored, reprocess this token */
		err = process_0body_in_body(treebuilder);
	} else if (type == ADDRESS || type == ARTICLE || type == ASIDE ||
			type == BLOCKQUOTE || type == CENTER || type == DIR || 
			type == DATAGRID || type == DIV || type == DL || 
			type == FIELDSET || type == FOOTER || type == HEADER ||
			type == LISTING || type == MENU || type == NAV ||
			type == OL || type == PRE || type == SECTION ||
			type == UL) {
		err = process_0container_in_body(treebuilder, type);
	} else if (type == FORM) {
		err = process_0form_in_body(treebuilder);
	} else if (type == P) {
		err = process_0p_in_body(treebuilder);
	} else if (type == DD || type == DT || type == LI) {
		err = process_0dd_dt_li_in_body(treebuilder, type);
	} else if (type == H1 || type == H2 || type == H3 || 
			type == H4 || type == H5 || type == H6) {
		err = process_0h_in_body(treebuilder, type);
	} else if (type == A || type == B || type == BIG || type == CODE ||
			type == EM || type == FONT || type == I ||
			type == NOBR || type == S || type == SMALL ||
			type == STRIKE || type == STRONG ||
			type == TT || type == U) {
		err = process_0presentational_in_body(treebuilder, type);
	} else if (type == APPLET || type == BUTTON ||
			type == MARQUEE || type == OBJECT) {
		err = process_0applet_button_marquee_object_in_body(
				treebuilder, type);
	} else if (type == BR) {
		err = process_0br_in_body(treebuilder);
	} else if (type == AREA || type == BASEFONT || 
			type == BGSOUND || type == EMBED || 
			type == HR || type == IFRAME ||
			type == IMAGE || type == IMG ||
			type == INPUT || type == ISINDEX ||
			type == NOEMBED || type == NOFRAMES ||
			type == PARAM || type == SELECT ||
			type == SPACER || type == TABLE ||
			type == TEXTAREA || type == WBR ||
			(treebuilder->context.enable_scripting && 
					type == NOSCRIPT)) {
		/** \todo parse error */
	} else {
		err = process_0generic_in_body(treebuilder, type);
	}

	return err;
}

/**
 * Process a html start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_html_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	/** \todo parse error */

	return treebuilder->tree_handler->add_attributes(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[0].node,
			token->data.tag.attributes, 
			token->data.tag.n_attributes);
}

/**
 * Process a body start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_body_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	/** \todo parse error */

	if (treebuilder->context.current_node < 1 || 
			treebuilder->context.element_stack[1].type != BODY)
		return HUBBUB_OK;

	return treebuilder->tree_handler->add_attributes(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[1].node,
			token->data.tag.attributes,
			token->data.tag.n_attributes);
}

/**
 * Process a frameset start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_frameset_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err = HUBBUB_OK;

	/** \todo parse error */

	if (treebuilder->context.current_node < 1 ||
			treebuilder->context.element_stack[1].type != BODY)
		return HUBBUB_OK;

	if (treebuilder->context.frameset_ok == false)
		return HUBBUB_OK;

	err = remove_node_from_dom(treebuilder, 
			treebuilder->context.element_stack[1].node);
	if (err != HUBBUB_OK)
		return err;

	err = element_stack_pop_until(treebuilder, BODY);
	assert(err == HUBBUB_OK);

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err == HUBBUB_OK)
		treebuilder->context.mode = IN_FRAMESET;

	return err;
}

/**
 * Process a generic container start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_container_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err;

	if (element_in_scope(treebuilder, P, false)) {
		err = process_0p_in_body(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	return insert_element(treebuilder, &token->data.tag, true);
}

/**
 * Process a hN start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_hN_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err;
	element_type type;

	if (element_in_scope(treebuilder, P, false)) {
		err = process_0p_in_body(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	type = treebuilder->context.element_stack[
			treebuilder->context.current_node].type;

	if (type == H1 || type == H2 || type == H3 || type == H4 ||
			type == H5 || type == H6) {
		hubbub_ns ns;
		element_type otype;
		void *node;

		/** \todo parse error */

		err = element_stack_pop(treebuilder, &ns, &otype, &node);
		assert(err == HUBBUB_OK);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);
	}

	return insert_element(treebuilder, &token->data.tag, true);
}

/**
 * Process a form start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_form_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;

	if (treebuilder->context.form_element != NULL) {
		/** \todo parse error */
	} else {
		if (element_in_scope(treebuilder, P, false)) {
			err = process_0p_in_body(treebuilder);
			if (err != HUBBUB_OK)
				return err;
		}

		err = insert_element(treebuilder, &token->data.tag, true);
		if (err != HUBBUB_OK)
			return err;

		/* Claim a reference on the node and 
		 * use it as the current form element */
		treebuilder->tree_handler->ref_node(
			treebuilder->tree_handler->ctx,
			treebuilder->context.element_stack[
			treebuilder->context.current_node].node);

		treebuilder->context.form_element =
			treebuilder->context.element_stack[
			treebuilder->context.current_node].node;
	}

	return HUBBUB_OK;
}

/**
 * Process a dd, dt or li start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
hubbub_error process_dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	hubbub_error err;
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node;

	treebuilder->context.frameset_ok = false;

	if (element_in_scope(treebuilder, P, false)) {
		err = process_0p_in_body(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	/* Find last LI/(DD,DT) on stack, if any */
	for (node = treebuilder->context.current_node; node > 0; node--) {
		element_type ntype = stack[node].type;

		if (type == LI && ntype == LI)
			break;

		if (((type == DD || type == DT) && 
				(ntype == DD || ntype == DT)))
			break;

		if (!is_formatting_element(ntype) &&
				!is_phrasing_element(ntype) &&
				ntype != ADDRESS && 
				ntype != DIV)
			break;
	}

	/* If we found one, then pop all nodes up to and including it */
	if (stack[node].type == LI || stack[node].type == DD ||
			stack[node].type == DT) {
		/* Check that we're only popping one node 
		 * and emit a parse error if not */
		if (treebuilder->context.current_node > node) {
			/** \todo parse error */
		}

		do {
			hubbub_ns ns;
			element_type otype;
			void *node;

			err = element_stack_pop(treebuilder, &ns, 
					&otype, &node);
			assert(err == HUBBUB_OK);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);
		} while (treebuilder->context.current_node >= node);
	}

	return insert_element(treebuilder, &token->data.tag, true);
}

/**
 * Process a plaintext start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_plaintext_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;
	hubbub_tokeniser_optparams params;

	if (element_in_scope(treebuilder, P, false)) {
		err = process_0p_in_body(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	params.content_model.model = HUBBUB_CONTENT_MODEL_PLAINTEXT;

	err = hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_CONTENT_MODEL,
			&params);
	assert(err == HUBBUB_OK);

	return HUBBUB_OK;
}

/**
 * Process an "a" start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_a_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;
	formatting_list_entry *entry = 
			aa_find_formatting_element(treebuilder, A);

	if (entry != NULL) {
		uint32_t index = entry->stack_index;
		void *node = entry->details.node;
		formatting_list_entry *entry2;

		/** \todo parse error */

		/* Act as if </a> were seen */
		err = process_0presentational_in_body(treebuilder, A);
		if (err != HUBBUB_OK)
			return err;

		entry2 = aa_find_formatting_element(treebuilder, A);

		/* Remove from formatting list, if it's still there */
		if (entry2 == entry && entry2->details.node == node) {
			hubbub_ns ons;
			element_type otype;
			void *onode;
			uint32_t oindex;

			err = formatting_list_remove(treebuilder, entry,
					&ons, &otype, &onode, &oindex);
			assert(err == HUBBUB_OK);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, onode);
				
		}

		/* Remove from the stack of open elements, if still there */
		if (index <= treebuilder->context.current_node &&
				treebuilder->context.element_stack[index].node 
				== node) {
			hubbub_ns ns;
			element_type otype;
			void *onode;

			err = element_stack_remove(treebuilder, index, &ns, 
					&otype,	&onode);
			assert(err == HUBBUB_OK);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, onode);
		}
	}

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	err = formatting_list_append(treebuilder, token->data.tag.ns, A, 
		treebuilder->context.element_stack[
			treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
	if (err != HUBBUB_OK) {
		hubbub_ns ns;
		element_type type;
		void *node;

		remove_node_from_dom(treebuilder, 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		element_stack_pop(treebuilder, &ns, &type, &node);

		/* Unref twice (once for stack, once for formatting list) */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return err;
	}

	return HUBBUB_OK;
}

/**
 * Process a b, big, em, font, i, s, small, 
 * strike, strong, tt, or u start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
hubbub_error process_presentational_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	hubbub_error err;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	err = formatting_list_append(treebuilder, token->data.tag.ns, type, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
	if (err != HUBBUB_OK) {
		hubbub_ns ns;
		element_type type;
		void *node;

		remove_node_from_dom(treebuilder, 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		element_stack_pop(treebuilder, &ns, &type, &node);

		/* Unref twice (once for stack, once for formatting list) */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return err;
	}

	return HUBBUB_OK;
}

/**
 * Process a nobr start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_nobr_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	if (element_in_scope(treebuilder, NOBR, false)) {
		/** \todo parse error */

		/* Act as if </nobr> were seen */
		err = process_0presentational_in_body(treebuilder, NOBR);
		if (err != HUBBUB_OK)
			return err;

		/* Yes, again */
		err = reconstruct_active_formatting_list(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	err = formatting_list_append(treebuilder, token->data.tag.ns, NOBR, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
	if (err != HUBBUB_OK) {
		hubbub_ns ns;
		element_type type;
		void *node;

		remove_node_from_dom(treebuilder, 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		element_stack_pop(treebuilder, &ns, &type, &node);

		/* Unref twice (once for stack, once for formatting list) */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return err;
	}

	return HUBBUB_OK;
}

/**
 * Process a button start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_button_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;

	if (element_in_scope(treebuilder, BUTTON, false)) {
		/** \todo parse error */

		/* Act as if </button> has been seen */
		err = process_0applet_button_marquee_object_in_body(
				treebuilder, BUTTON);
		assert(err == HUBBUB_OK);
	}

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	err = formatting_list_append(treebuilder, token->data.tag.ns, BUTTON, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node,
		treebuilder->context.current_node);
	if (err != HUBBUB_OK) {
		hubbub_ns ns;
		element_type type;
		void *node;

		remove_node_from_dom(treebuilder, 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		element_stack_pop(treebuilder, &ns, &type, &node);

		/* Unref twice (once for stack, once for formatting list) */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return err;
	}

	treebuilder->context.frameset_ok = false;

	return HUBBUB_OK;
}

/**
 * Process an applet, marquee or object start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 * \param type         The element type
 */
hubbub_error process_applet_marquee_object_in_body(
		hubbub_treebuilder *treebuilder,
		const hubbub_token *token, element_type type)
{
	hubbub_error err;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err != HUBBUB_OK)
		return err;

	treebuilder->tree_handler->ref_node(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node);

	err = formatting_list_append(treebuilder, token->data.tag.ns, type, 
		treebuilder->context.element_stack[
		treebuilder->context.current_node].node, 
		treebuilder->context.current_node);
	if (err != HUBBUB_OK) {
		hubbub_ns ns;
		element_type type;
		void *node;

		remove_node_from_dom(treebuilder, 
				treebuilder->context.element_stack[
				treebuilder->context.current_node].node);

		element_stack_pop(treebuilder, &ns, &type, &node);

		/* Unref twice (once for stack, once for formatting list) */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return err;
	}

	treebuilder->context.frameset_ok = false;

	return HUBBUB_OK;
}

/**
 * Process an hr start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_hr_in_body(hubbub_treebuilder *treebuilder, 
		const hubbub_token *token)
{
	hubbub_error err;

	if (element_in_scope(treebuilder, P, false)) {
		err = process_0p_in_body(treebuilder);
		if (err != HUBBUB_OK)
			return err;
	}

	err = insert_element(treebuilder, &token->data.tag, false);
	if (err == HUBBUB_OK)
		treebuilder->context.frameset_ok = false;

	return err;
}

/**
 * Process an image start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_image_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;
	hubbub_tag tag;

	tag.ns = HUBBUB_NS_HTML;
	tag.name.ptr = (const uint8_t *) "img";
	tag.name.len = SLEN("img");

	tag.n_attributes = token->data.tag.n_attributes;
	tag.attributes = token->data.tag.attributes;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	return insert_element(treebuilder, &tag, false);
}

/**
 * Process an isindex start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_isindex_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;
	hubbub_token dummy;
	hubbub_attribute *action = NULL;
	hubbub_attribute *prompt = NULL;
	hubbub_attribute *attrs = NULL;
	size_t n_attrs = 0;

	/** \todo parse error */

	if (treebuilder->context.form_element != NULL)
		return HUBBUB_OK;

	/* First up, clone the token's attributes */
	if (token->data.tag.n_attributes > 0) {
		uint32_t i;
		attrs = malloc((token->data.tag.n_attributes + 1) *
						sizeof(hubbub_attribute));
		if (attrs == NULL)
			return HUBBUB_NOMEM;

		for (i = 0; i < token->data.tag.n_attributes; i++) {
			hubbub_attribute *attr = &token->data.tag.attributes[i];
			const uint8_t *name = attr->name.ptr;

			if (strncmp((const char *) name, "action",
					attr->name.len) == 0) {
				action = attr;
			} else if (strncmp((const char *) name, "prompt",
					attr->name.len) == 0) {
				prompt = attr;
			} else if (strncmp((const char *) name, "name",
					attr->name.len) == 0) {
			} else {
				attrs[n_attrs++] = *attr;
			}
		}

		attrs[n_attrs].ns = HUBBUB_NS_HTML;
		attrs[n_attrs].name.ptr = (const uint8_t *) "name";
		attrs[n_attrs].name.len = SLEN("name");
		attrs[n_attrs].value.ptr = (const uint8_t *) "isindex";
		attrs[n_attrs].value.len = SLEN("isindex");
		n_attrs++;
	}

	/* isindex algorithm */

	/* Set up dummy as a start tag token */
	dummy.type = HUBBUB_TOKEN_START_TAG;
	dummy.data.tag.ns = HUBBUB_NS_HTML;

	/* Act as if <form> were seen */
	dummy.data.tag.name.ptr = (const uint8_t *) "form";
	dummy.data.tag.name.len = SLEN("form");

	dummy.data.tag.n_attributes = action != NULL ? 1 : 0;
	dummy.data.tag.attributes = action;

	err = process_form_in_body(treebuilder, &dummy);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* Act as if <hr> were seen */
	dummy.data.tag.name.ptr = (const uint8_t *) "hr";
	dummy.data.tag.name.len = SLEN("hr");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	err = process_hr_in_body(treebuilder, &dummy);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* Act as if <p> were seen */
	dummy.data.tag.name.ptr = (const uint8_t *) "p";
	dummy.data.tag.name.len = SLEN("p");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	err = process_container_in_body(treebuilder, &dummy);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* Act as if <label> were seen */
	dummy.data.tag.name.ptr = (const uint8_t *) "label";
	dummy.data.tag.name.len = SLEN("label");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	err = process_phrasing_in_body(treebuilder, &dummy);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* Act as if a stream of characters were seen */
	dummy.type = HUBBUB_TOKEN_CHARACTER;
	if (prompt != NULL) {
		dummy.data.character = prompt->value;
	} else {
		/** \todo Localisation */
#define PROMPT "This is a searchable index. Insert your search keywords here: "
		dummy.data.character.ptr = (const uint8_t *) PROMPT;
		dummy.data.character.len = SLEN(PROMPT);
#undef PROMPT
	}
	
	err = process_character(treebuilder, &dummy);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* Act as if <input> was seen */
	dummy.type = HUBBUB_TOKEN_START_TAG;
	dummy.data.tag.ns = HUBBUB_NS_HTML;
	dummy.data.tag.name.ptr = (const uint8_t *) "input";
	dummy.data.tag.name.len = SLEN("input");

	dummy.data.tag.n_attributes = n_attrs;
	dummy.data.tag.attributes = attrs;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	err = insert_element(treebuilder, &dummy.data.tag, false);
	if (err != HUBBUB_OK) {
		free(attrs);
		return err;
	}

	/* No longer need attrs */
	free(attrs);

	treebuilder->context.frameset_ok = false;

	/* Act as if </label> was seen */
	err = process_0generic_in_body(treebuilder, LABEL);
	assert(err == HUBBUB_OK);

	/* Act as if </p> was seen */
	err = process_0p_in_body(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	/* Act as if <hr> was seen */
	dummy.data.tag.name.ptr = (const uint8_t *) "hr";
	dummy.data.tag.name.len = SLEN("hr");
	dummy.data.tag.n_attributes = 0;
	dummy.data.tag.attributes = NULL;

	err = process_hr_in_body(treebuilder, &dummy);
	if (err != HUBBUB_OK)
		return err;

	/* Act as if </form> was seen */
	return process_0container_in_body(treebuilder, FORM);
}

/**
 * Process a textarea start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_textarea_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	treebuilder->context.strip_leading_lr = true;
	treebuilder->context.frameset_ok = false;
	return parse_generic_rcdata(treebuilder, token, true);
}

/**
 * Process a select start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_select_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	err = insert_element(treebuilder, &token->data.tag, true);
	if (err == HUBBUB_OK)
		treebuilder->context.frameset_ok = false;

	return err;
}

/**
 * Process an option or optgroup start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_opt_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err; 

	if (element_in_scope(treebuilder, OPTION, false)) {
		err = process_0generic_in_body(treebuilder, OPTION);
		/* Cannot fail */
		assert(err == HUBBUB_OK);
	}
	
	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	return insert_element(treebuilder, &token->data.tag, true);
}

/**
 * Process a phrasing start tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The token to process
 */
hubbub_error process_phrasing_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token)
{
	hubbub_error err;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	return insert_element(treebuilder, &token->data.tag, true);
}

/**
 * Process a body end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \return True if processed, false otherwise
 */
hubbub_error process_0body_in_body(hubbub_treebuilder *treebuilder)
{
	hubbub_error err = HUBBUB_OK;

	if (!element_in_scope(treebuilder, BODY, false)) {
		/** \todo parse error */
	} else {
		element_context *stack = treebuilder->context.element_stack;
		uint32_t node;

		for (node = treebuilder->context.current_node; 
				node > 0; node--) {
			element_type ntype = stack[node].type;

			if (ntype != DD && ntype != DT && ntype != LI && 
					ntype != OPTGROUP && ntype != OPTION &&
					ntype != P && ntype != RP && 
					ntype != RT && ntype != TBODY &&
					ntype != TD && ntype != TFOOT &&
					ntype != TH && ntype != THEAD &&
					ntype != TR && ntype != BODY) {
				/** \todo parse error */
			}
		}

		if (treebuilder->context.mode == IN_BODY)
			treebuilder->context.mode = AFTER_BODY;

		err = HUBBUB_REPROCESS;
	}

	return err;
}

/**
 * Process a container end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0container_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			element_stack_pop(treebuilder, &ns, &otype, &node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}
	}

	return HUBBUB_OK;
}

/**
 * Process a form end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 */
hubbub_error process_0form_in_body(hubbub_treebuilder *treebuilder)
{
	void *node = treebuilder->context.form_element;
	uint32_t idx = 0;

	if (treebuilder->context.form_element != NULL)
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.form_element);
	treebuilder->context.form_element = NULL;

	idx = element_in_scope(treebuilder, FORM, false);

	if (idx == 0 || node == NULL || 
			treebuilder->context.element_stack[idx].node != node) {
		/** \todo parse error */
	} else {
		hubbub_ns ns;
		element_type otype;
		void *onode;

		close_implied_end_tags(treebuilder, UNKNOWN);

		if (treebuilder->context.element_stack[
				treebuilder->context.current_node].node != 
				node) {
			/** \todo parse error */
		}

		element_stack_remove(treebuilder, idx, 
				&ns, &otype, &onode);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				onode);
	}

	return HUBBUB_OK;
}


/**
 * Process a p end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 */
hubbub_error process_0p_in_body(hubbub_treebuilder *treebuilder)
{
	hubbub_error err = HUBBUB_OK;
	uint32_t popped = 0;

	if (treebuilder->context.element_stack[
			treebuilder->context.current_node].type != P) {
		/** \todo parse error */
	}

	while (element_in_scope(treebuilder, P, false)) {
		hubbub_ns ns;
		element_type type;
		void *node;

		err = element_stack_pop(treebuilder, &ns, &type, &node);
		assert(err == HUBBUB_OK);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		popped++;
	}

	if (popped == 0) {
		hubbub_token dummy;

		dummy.type = HUBBUB_TOKEN_START_TAG;
		dummy.data.tag.ns = HUBBUB_NS_HTML;
		dummy.data.tag.name.ptr = (const uint8_t *) "p";
		dummy.data.tag.name.len = SLEN("p");
		dummy.data.tag.n_attributes = 0;
		dummy.data.tag.attributes = NULL;

		err = process_container_in_body(treebuilder, &dummy);
		if (err != HUBBUB_OK)
			return err;

		/* Reprocess the end tag. This is safe as we've just 
		 * inserted a <p> into the current scope */
		err = process_0p_in_body(treebuilder);
		/* Cannot fail */
		assert(err == HUBBUB_OK);
	}

	return err;
}

/**
 * Process a dd, dt, or li end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0dd_dt_li_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, type);

		do {
			hubbub_ns ns;
			void *node;

			element_stack_pop(treebuilder, 
					&ns, &otype, &node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}
	}

	return HUBBUB_OK;
}

/**
 * Process a h1, h2, h3, h4, h5, or h6 end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0h_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	UNUSED(type);

	/** \todo optimise this */
	if (element_in_scope(treebuilder, H1, false) ||
			element_in_scope(treebuilder, H2, false) ||
			element_in_scope(treebuilder, H3, false) ||
			element_in_scope(treebuilder, H4, false) ||
			element_in_scope(treebuilder, H5, false) ||
			element_in_scope(treebuilder, H6, false)) {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			element_stack_pop(treebuilder, &ns, &otype, &node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != H1 && otype != H2 &&
				otype != H3 && otype != H4 &&
				otype != H5 && otype != H6);

		if (popped > 1) {
			/** \todo parse error */
		}
	} else {
		/** \todo parse error */
	}

	return HUBBUB_OK;
}

/**
 * Process a presentational end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0presentational_in_body(hubbub_treebuilder *treebuilder,
		element_type type)
{
	hubbub_error err;

	/* Welcome to the adoption agency */

	while (true) {
		element_context *stack = treebuilder->context.element_stack;

		formatting_list_entry *entry;
		uint32_t formatting_element;
		uint32_t common_ancestor;
		uint32_t furthest_block;
		bookmark bookmark;
		uint32_t last_node;
		void *reparented;
		void *fe_clone = NULL;
		void *clone_appended = NULL;
		hubbub_ns ons;
		element_type otype;
		void *onode;
		uint32_t oindex;

		/* 1 */
		err = aa_find_and_validate_formatting_element(treebuilder,
				type, &entry);
		assert(err == HUBBUB_OK || err == HUBBUB_REPROCESS);
		if (err == HUBBUB_OK)
			return err;

		assert(entry->details.type == type);

		/* Take a copy of the stack index for use
		 * during stack manipulation */
		formatting_element = entry->stack_index;

		/* 2 & 3 */
		err = aa_find_furthest_block(treebuilder,
				entry, &furthest_block);
		assert(err == HUBBUB_OK || err == HUBBUB_REPROCESS);
		if (err == HUBBUB_OK)
			return err;

		/* 4 */
		common_ancestor = formatting_element - 1;

		/* 5 */
		bookmark.prev = entry->prev;
		bookmark.next = entry->next;

		/* 6 */
		err = aa_find_bookmark_location_reparenting_misnested(
				treebuilder, formatting_element, 
				&furthest_block, &bookmark, &last_node);
		if (err != HUBBUB_OK)
			return err;

		/* 7 */
		if (stack[common_ancestor].type == TABLE ||
				stack[common_ancestor].type == TBODY ||
				stack[common_ancestor].type == TFOOT ||
				stack[common_ancestor].type == THEAD ||
				stack[common_ancestor].type == TR) {
			err = aa_insert_into_foster_parent(treebuilder,
					stack[last_node].node, &reparented);
		} else {
			err = aa_reparent_node(treebuilder, 
					stack[last_node].node,
					stack[common_ancestor].node,
					&reparented);
		}
		if (err != HUBBUB_OK)
			return err;

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				stack[last_node].node);

		/* If the reparented node is not the same as the one we were
		 * previously using, then have it take the place of the other
		 * one in the formatting list and stack. */
		if (reparented != stack[last_node].node) {
			struct formatting_list_entry *node_entry;
			for (node_entry = treebuilder->context.formatting_list_end;
					node_entry != NULL; 
					node_entry = node_entry->prev) {
				if (node_entry->stack_index == last_node) {
					treebuilder->tree_handler->ref_node(
						treebuilder->tree_handler->ctx,
						reparented);
					node_entry->details.node = reparented;
					treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						stack[last_node].node);
					break;
				}
			}
			/* Already have enough references, so don't need to 
			 * explicitly reference it here. */
			stack[last_node].node = reparented;
		}

		/* 8 */
		err = treebuilder->tree_handler->clone_node(
				treebuilder->tree_handler->ctx,
				entry->details.node, false, &fe_clone);
		if (err != HUBBUB_OK)
			return err;

		/* 9 */
		err = treebuilder->tree_handler->reparent_children(
				treebuilder->tree_handler->ctx,
				stack[furthest_block].node, fe_clone);
		if (err != HUBBUB_OK) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					fe_clone);
			return err;
		}

		/* 10 */
		err = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				stack[furthest_block].node, fe_clone,
				&clone_appended);
		if (err != HUBBUB_OK) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					fe_clone);
			return err;
		}

		if (clone_appended != fe_clone) {
			/* No longer interested in fe_clone */
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx, 
					fe_clone);
			/* Need an extra reference, as we'll insert into the 
			 * formatting list and element stack */
			treebuilder->tree_handler->ref_node(
					treebuilder->tree_handler->ctx,
					clone_appended);
		}

		/* 11 and 12 are reversed here so that we know the correct
		 * stack index to use when inserting into the formatting list */

		/* 12 */
		err = aa_remove_element_stack_item(treebuilder, 
				formatting_element, furthest_block);
		assert(err == HUBBUB_OK);

		/* Fix up furthest block index */
		furthest_block--;

		/* Now, in the gap after furthest block,
		 * we insert an entry for clone */
		stack[furthest_block + 1].type = entry->details.type;
		stack[furthest_block + 1].node = clone_appended;

		/* 11 */
		err = formatting_list_remove(treebuilder, entry,
				&ons, &otype, &onode, &oindex);
		assert(err == HUBBUB_OK);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,	onode);

		err = formatting_list_insert(treebuilder,
				bookmark.prev, bookmark.next,
				ons, otype, clone_appended, furthest_block + 1);
		if (err != HUBBUB_OK) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					clone_appended);
			return err;
		}

		/* 13 */
	}
}

/**
 * Adoption agency: find and validate the formatting element
 *
 * \param treebuilder  The treebuilder instance
 * \param type         Element type to search for
 * \param element      Pointer to location to receive list entry
 * \return HUBBUB_REPROCESS to continue processing,
 *         HUBBUB_OK to stop.
 */
hubbub_error aa_find_and_validate_formatting_element(
		hubbub_treebuilder *treebuilder,
		element_type type, formatting_list_entry **element)
{
	formatting_list_entry *entry;

	entry = aa_find_formatting_element(treebuilder, type);

	if (entry == NULL || (entry->stack_index != 0 &&
			element_in_scope(treebuilder, entry->details.type,
					false) != entry->stack_index)) {
		/** \todo parse error */
		return HUBBUB_OK;
	}

	if (entry->stack_index == 0) {
		/* Not in element stack => remove from formatting list */
		hubbub_ns ns;
		element_type type;
		void *node;
		uint32_t index;

		/** \todo parse error */

		formatting_list_remove(treebuilder, entry,
				&ns, &type, &node, &index);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return HUBBUB_OK;
	}

	if (entry->stack_index != treebuilder->context.current_node) {
		/** \todo parse error */
	}

	*element = entry;

	return HUBBUB_REPROCESS;
}

/**
 * Adoption agency: find formatting element
 *
 * \param treebuilder  The treebuilder instance
 * \param type         Type of element to search for
 * \return Pointer to formatting element, or NULL if none found
 */
formatting_list_entry *aa_find_formatting_element(
		hubbub_treebuilder *treebuilder, element_type type)
{
	formatting_list_entry *entry;

	for (entry = treebuilder->context.formatting_list_end;
			entry != NULL; entry = entry->prev) {

		/* Assumption: HTML and TABLE elements are not in the list */
		if (is_scoping_element(entry->details.type) ||
				entry->details.type == type)
			break;
	}

	/* Check if we stopped on a marker, rather than a formatting element */
	if (entry != NULL && is_scoping_element(entry->details.type))
		entry = NULL;

	return entry;
}

/**
 * Adoption agency: find furthest block
 *
 * \param treebuilder         The treebuilder instance
 * \param formatting_element  The formatting element
 * \param furthest_block      Pointer to location to receive furthest block
 * \return HUBBUB_REPROCESS to continue processing (::furthest_block filled in),
 *         HUBBUB_OK to stop.
 */
hubbub_error aa_find_furthest_block(hubbub_treebuilder *treebuilder,
		formatting_list_entry *formatting_element,
		uint32_t *furthest_block)
{
	uint32_t fe_index = formatting_element->stack_index;
	uint32_t fb;

	for (fb = fe_index + 1; fb <= treebuilder->context.current_node; fb++) {
		element_type type = treebuilder->context.element_stack[fb].type;

		if (!(is_phrasing_element(type) || is_formatting_element(type)))
			break;
	}

	if (fb > treebuilder->context.current_node) {
		hubbub_ns ns;
		element_type type;
		void *node;
		uint32_t index;

		/* Pop all elements off the stack up to,
		 * and including, the formatting element */
		do {
			element_stack_pop(treebuilder, &ns, &type, &node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);
		} while (treebuilder->context.current_node >= fe_index);

		/* Remove the formatting element from the list */
		formatting_list_remove(treebuilder, formatting_element,
				&ns, &type, &node, &index);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		return HUBBUB_OK;
	}

	*furthest_block = fb;

	return HUBBUB_REPROCESS;
}

/**
 * Adoption agency: reparent a node
 *
 * \param treebuilder  The treebuilder instance
 * \param node         The node to reparent
 * \param new_parent   The new parent
 * \param reparented   Pointer to location to receive reparented node
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error aa_reparent_node(hubbub_treebuilder *treebuilder, void *node,
		void *new_parent, void **reparented)
{
	hubbub_error err;

	err = remove_node_from_dom(treebuilder, node);
	if (err != HUBBUB_OK)
		return err;

	return treebuilder->tree_handler->append_child(
			treebuilder->tree_handler->ctx,
			new_parent, node, reparented);
}

/**
 * Adoption agency: this is step 6
 *
 * \param treebuilder         The treebuilder instance
 * \param formatting_element  The stack index of the formatting element
 * \param furthest_block      Pointer to index of furthest block in element
 *                            stack (updated on exit)
 * \param bookmark            Pointer to bookmark (pre-initialised)
 * \param last_node           Pointer to location to receive index of last node
 */
hubbub_error aa_find_bookmark_location_reparenting_misnested(
		hubbub_treebuilder *treebuilder,
		uint32_t formatting_element, uint32_t *furthest_block,
		bookmark *bookmark, uint32_t *last_node)
{
	hubbub_error err;
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node, last, fb;
	formatting_list_entry *node_entry;

	node = last = fb = *furthest_block;

	while (true) {
		void *reparented;

		/* i */
		node--;

		/* ii */
		for (node_entry = treebuilder->context.formatting_list_end;
				node_entry != NULL;
				node_entry = node_entry->prev) {
			if (node_entry->stack_index == node)
				break;
		}

		/* Node is not in list of active formatting elements */
		if (node_entry == NULL) {
			err = aa_remove_element_stack_item(treebuilder,
				node, treebuilder->context.current_node);
			assert(err == HUBBUB_OK);

			/* Update furthest block index and the last node index,
			 * as these are always below node in the stack */
			fb--;
			last--;

			/* Fixup the current_node index */
			treebuilder->context.current_node--;

			/* Back to i */
			continue;
		}

		/* iii */
		if (node == formatting_element)
			break;

		/* iv */
		if (last == fb) {
			bookmark->prev = node_entry;
			bookmark->next = node_entry->next;
		}

		/* v */
		err = aa_clone_and_replace_entries(treebuilder, node_entry);
		if (err != HUBBUB_OK)
			return err;

		/* vi */
		err = aa_reparent_node(treebuilder, stack[last].node, 
				stack[node].node, &reparented);
		if (err != HUBBUB_OK)
			return err;

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				stack[last].node);

		/* If the reparented node is not the same as the one we were
		 * previously using, then have it take the place of the other
		 * one in the formatting list and stack. */
		if (reparented != stack[last].node) {
			for (node_entry = 
				treebuilder->context.formatting_list_end;
					node_entry != NULL; 
					node_entry = node_entry->prev) {
				if (node_entry->stack_index == last) {
					treebuilder->tree_handler->ref_node(
						treebuilder->tree_handler->ctx,
						reparented);
					node_entry->details.node = reparented;
					treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						stack[last].node);
					break;
				}
			}
			/* Already have enough references, so don't need to 
			 * explicitly reference it here. */
			stack[last].node = reparented;
		}

		/* vii */
		last = node;

		/* viii */
	}

	*furthest_block = fb;
	*last_node = last;

	return HUBBUB_OK;
}

/**
 * Adoption agency: remove an entry from the stack at the given index
 *
 * \param treebuilder  The treebuilder instance
 * \param index        The index of the item to remove
 * \param limit        The index of the last item to move
 *
 * Preconditions: index < limit, limit <= current_node
 * Postcondition: stack[limit] is empty
 */
hubbub_error aa_remove_element_stack_item(hubbub_treebuilder *treebuilder,
		uint32_t index, uint32_t limit)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t n;

	assert(index < limit);
	assert(limit <= treebuilder->context.current_node);

	/* First, scan over subsequent entries in the stack,
	 * searching for them in the list of active formatting
	 * entries. If found, update the corresponding
	 * formatting list entry's stack index to match the
	 * new stack location */
	for (n = index + 1; n <= limit; n++) {
		if (is_formatting_element(stack[n].type) ||
				(is_scoping_element(stack[n].type) &&
				stack[n].type != HTML &&
				stack[n].type != TABLE)) {
			formatting_list_entry *e;

			for (e = treebuilder->context.formatting_list_end;
					e != NULL; e = e->prev) {
				if (e->stack_index == n)
					e->stack_index--;
			}
		}
	}

	/* Reduce node's reference count */
	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
					stack[index].node);

	/* Now, shuffle the stack up one, removing node in the process */
	memmove(&stack[index], &stack[index + 1],
			(limit - index) * sizeof(element_context));

	return HUBBUB_OK;
}

/**
 * Adoption agency: shallow clone a node and replace its formatting list
 * and element stack entries
 *
 * \param treebuilder  The treebuilder instance
 * \param element      The item in the formatting list containing the node
 */
hubbub_error aa_clone_and_replace_entries(hubbub_treebuilder *treebuilder,
		formatting_list_entry *element)
{
	hubbub_error err;
	hubbub_ns ons;
	element_type otype;
	uint32_t oindex;
	void *clone, *onode;

	/* Shallow clone of node */
	err = treebuilder->tree_handler->clone_node(
			treebuilder->tree_handler->ctx,
			element->details.node, false, &clone);
	if (err != HUBBUB_OK)
		return err;

	/* Replace formatting list entry for node with clone */
	err = formatting_list_replace(treebuilder, element,
			element->details.ns, element->details.type, 
			clone, element->stack_index,
			&ons, &otype, &onode, &oindex);
	assert(err == HUBBUB_OK);

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			onode);

	treebuilder->tree_handler->ref_node(treebuilder->tree_handler->ctx,
			clone);

	/* Replace node's stack entry with clone */
	treebuilder->context.element_stack[element->stack_index].node = clone;

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			onode);

	return HUBBUB_OK;
}

/**
 * Adoption agency: locate foster parent and insert node into it
 *
 * \param treebuilder  The treebuilder instance
 * \param node         The node to insert
 * \param inserted     Pointer to location to receive inserted node
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error aa_insert_into_foster_parent(hubbub_treebuilder *treebuilder, 
		void *node, void **inserted)
{
	hubbub_error err;
	element_context *stack = treebuilder->context.element_stack;
	void *foster_parent = NULL;
	bool insert = false;

	uint32_t cur_table = current_table(treebuilder);

	stack[cur_table].tainted = true;

	if (cur_table == 0) {
		treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx,
				stack[0].node);

		foster_parent = stack[0].node;
	} else {
		void *t_parent = NULL;

		treebuilder->tree_handler->get_parent(
			treebuilder->tree_handler->ctx,
			stack[cur_table].node,
			true, &t_parent);

		if (t_parent != NULL) {
			foster_parent = t_parent;
			insert = true;
		} else {
			treebuilder->tree_handler->ref_node(
					treebuilder->tree_handler->ctx,
					stack[cur_table - 1].node);
			foster_parent = stack[cur_table - 1].node;
		}
	}

	err = remove_node_from_dom(treebuilder, node);
	if (err != HUBBUB_OK) {
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				foster_parent);
		return err;
	}

	if (insert) {
		err = treebuilder->tree_handler->insert_before(
				treebuilder->tree_handler->ctx,
				foster_parent, node,
				stack[cur_table].node,
				inserted);
	} else {
		err = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				foster_parent, node,
				inserted);
	}
	if (err != HUBBUB_OK) {
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				foster_parent);
		return err;
	}

	treebuilder->tree_handler->unref_node(treebuilder->tree_handler->ctx,
			foster_parent);

	return HUBBUB_OK;
}


/**
 * Process an applet, button, marquee, or object end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0applet_button_marquee_object_in_body(
		hubbub_treebuilder *treebuilder, element_type type)
{
	if (!element_in_scope(treebuilder, type, false)) {
		/** \todo parse error */
	} else {
		uint32_t popped = 0;
		element_type otype;

		close_implied_end_tags(treebuilder, UNKNOWN);

		do {
			hubbub_ns ns;
			void *node;

			element_stack_pop(treebuilder, &ns, &otype, &node);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					node);

			popped++;
		} while (otype != type);

		if (popped > 1) {
			/** \todo parse error */
		}

		clear_active_formatting_list_to_marker(treebuilder);
	}

	return HUBBUB_OK;
}

/**
 * Process a br end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 */
hubbub_error process_0br_in_body(hubbub_treebuilder *treebuilder)
{
	hubbub_error err;
	hubbub_tag tag;

	/** \todo parse error */

	/* Act as if <br> has been seen. */

	tag.ns = HUBBUB_NS_HTML;
	tag.name.ptr = (const uint8_t *) "br";
	tag.name.len = SLEN("br");

	tag.n_attributes = 0;
	tag.attributes = NULL;

	err = reconstruct_active_formatting_list(treebuilder);
	if (err != HUBBUB_OK)
		return err;

	return insert_element(treebuilder, &tag, false);
}

/**
 * Process a generic end tag as if in "in body"
 *
 * \param treebuilder  The treebuilder instance
 * \param type         The element type
 */
hubbub_error process_0generic_in_body(hubbub_treebuilder *treebuilder, 
		element_type type)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t node = treebuilder->context.current_node;

	do {
		if (stack[node].type == type) {
			uint32_t popped = 0;
			element_type otype;

			close_implied_end_tags(treebuilder, UNKNOWN);

			while (treebuilder->context.current_node >= node) {
				hubbub_ns ns;
				void *node;

				element_stack_pop(treebuilder,
						&ns, &otype, &node);

				treebuilder->tree_handler->unref_node(
						treebuilder->tree_handler->ctx,
						node);

				popped++;

				if (otype == type)
					break;
			}

			if (popped > 1) {
				/** \todo parse error */
			}

			break;
		} else if (!is_formatting_element(stack[node].type) && 
				!is_phrasing_element(stack[node].type)) {
			/** \todo parse error */
			break;
		}
	} while (--node > 0);

	return HUBBUB_OK;
}

