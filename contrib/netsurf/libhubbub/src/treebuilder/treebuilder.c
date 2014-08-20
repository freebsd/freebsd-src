/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#include <assert.h>
#include <string.h>

#include <stdio.h>

#include "treebuilder/modes.h"
#include "treebuilder/internal.h"
#include "treebuilder/treebuilder.h"
#include "utils/utils.h"
#include "utils/string.h"


#define S(x)   x, SLEN(x)

static const struct {
	const char *name;
	size_t len;
	element_type type;
} name_type_map[] = {
	{ S("address"), ADDRESS },	{ S("area"), AREA },
	{ S("base"), BASE },		{ S("basefont"), BASEFONT },
	{ S("bgsound"), BGSOUND },	{ S("blockquote"), BLOCKQUOTE },
	{ S("body"), BODY },		{ S("br"), BR },
	{ S("center"), CENTER },	{ S("col"), COL },
	{ S("colgroup"), COLGROUP },	{ S("dd"), DD },
	{ S("dir"), DIR },		{ S("div"), DIV },
	{ S("dl"), DL },		{ S("dt"), DT },
	{ S("embed"), EMBED },		{ S("fieldset"), FIELDSET },
	{ S("form"), FORM },		{ S("frame"), FRAME },
	{ S("frameset"), FRAMESET },	{ S("h1"), H1 },
	{ S("h2"), H2 },		{ S("h3"), H3 },
	{ S("h4"), H4 },		{ S("h5"), H5 },
	{ S("h6"), H6 },		{ S("head"), HEAD },
	{ S("hr"), HR },		{ S("iframe"), IFRAME },
	{ S("image"), IMAGE },		{ S("img"), IMG },
	{ S("input"), INPUT },		{ S("isindex"), ISINDEX },
	{ S("li"), LI },		{ S("link"), LINK },
	{ S("listing"), LISTING },
	{ S("menu"), MENU },
	{ S("meta"), META },		{ S("noembed"), NOEMBED },
	{ S("noframes"), NOFRAMES },	{ S("noscript"), NOSCRIPT },
	{ S("ol"), OL },		{ S("optgroup"), OPTGROUP },
	{ S("option"), OPTION },	{ S("output"), OUTPUT },
	{ S("p"), P },			{ S("param"), PARAM },
	{ S("plaintext"), PLAINTEXT },	{ S("pre"), PRE },
	{ S("script"), SCRIPT },	{ S("select"), SELECT },
	{ S("spacer"), SPACER },	{ S("style"), STYLE },
 	{ S("tbody"), TBODY },		{ S("textarea"), TEXTAREA },
	{ S("tfoot"), TFOOT },		{ S("thead"), THEAD },
	{ S("title"), TITLE },		{ S("tr"), TR },
	{ S("ul"), UL },		{ S("wbr"), WBR },
	{ S("applet"), APPLET },	{ S("button"), BUTTON },
	{ S("caption"), CAPTION },	{ S("html"), HTML },
	{ S("marquee"), MARQUEE },	{ S("object"), OBJECT },
	{ S("table"), TABLE },		{ S("td"), TD },
	{ S("th"), TH },
	{ S("a"), A },			{ S("b"), B },
	{ S("big"), BIG },		{ S("em"), EM },
	{ S("font"), FONT },		{ S("i"), I },
	{ S("nobr"), NOBR },		{ S("s"), S },
	{ S("small"), SMALL },		{ S("strike"), STRIKE },
	{ S("strong"), STRONG },	{ S("tt"), TT },
	{ S("u"), U },			{ S("xmp"), XMP },

	{ S("math"), MATH },		{ S("mglyph"), MGLYPH },
	{ S("malignmark"), MALIGNMARK },
	{ S("mi"), MI },		{ S("mo"), MO },
	{ S("mn"), MN },		{ S("ms"), MS },
	{ S("mtext"), MTEXT },		{ S("annotation-xml"), ANNOTATION_XML },

	{ S("svg"), SVG },		{ S("desc"), DESC },
	{ S("foreignobject"), FOREIGNOBJECT },
};

static bool is_form_associated(element_type type);

/**
 * Create a hubbub treebuilder
 *
 * \param tokeniser    Underlying tokeniser instance
 * \param treebuilder  Pointer to location to receive treebuilder instance
 * \return HUBBUB_OK on success,
 *         HUBBUB_BADPARM on bad parameters
 *         HUBBUB_NOMEM on memory exhaustion
 */
hubbub_error hubbub_treebuilder_create(hubbub_tokeniser *tokeniser,
		hubbub_treebuilder **treebuilder)
{
	hubbub_error error;
	hubbub_treebuilder *tb;
	hubbub_tokeniser_optparams tokparams;

	if (tokeniser == NULL || treebuilder == NULL)
		return HUBBUB_BADPARM;

	tb = malloc(sizeof(hubbub_treebuilder));
	if (tb == NULL)
		return HUBBUB_NOMEM;

	tb->tokeniser = tokeniser;

	tb->tree_handler = NULL;

	memset(&tb->context, 0, sizeof(hubbub_treebuilder_context));
	tb->context.mode = INITIAL;

	tb->context.element_stack = malloc(
			ELEMENT_STACK_CHUNK * sizeof(element_context));
	if (tb->context.element_stack == NULL) {
		free(tb);
		return HUBBUB_NOMEM;
	}
	tb->context.stack_alloc = ELEMENT_STACK_CHUNK;
	/* We rely on HTML not being equal to zero to determine
	 * if the first item in the stack is in use. Assert this here. */
	assert(HTML != 0);
	tb->context.element_stack[0].type = (element_type) 0;

	tb->context.strip_leading_lr = false;
	tb->context.frameset_ok = true;

	tb->error_handler = NULL;
	tb->error_pw = NULL;

	tokparams.token_handler.handler = hubbub_treebuilder_token_handler;
	tokparams.token_handler.pw = tb;

	error = hubbub_tokeniser_setopt(tokeniser,
			HUBBUB_TOKENISER_TOKEN_HANDLER, &tokparams);
	if (error != HUBBUB_OK) {
		free(tb->context.element_stack);
		free(tb);
		return error;
	}

	*treebuilder = tb;

	return HUBBUB_OK;
}

/**
 * Destroy a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to destroy
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error hubbub_treebuilder_destroy(hubbub_treebuilder *treebuilder)
{
	formatting_list_entry *entry, *next;
	hubbub_tokeniser_optparams tokparams;

	if (treebuilder == NULL)
		return HUBBUB_BADPARM;

	tokparams.token_handler.handler = NULL;
	tokparams.token_handler.pw = NULL;

	hubbub_tokeniser_setopt(treebuilder->tokeniser,
			HUBBUB_TOKENISER_TOKEN_HANDLER, &tokparams);

	/* Clean up context */
	if (treebuilder->tree_handler != NULL) {
		uint32_t n;

		if (treebuilder->context.head_element != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.head_element);
		}

		if (treebuilder->context.form_element != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.form_element);
		}

		if (treebuilder->context.document != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					treebuilder->context.document);
		}

		for (n = treebuilder->context.current_node;
				n > 0; n--) {
			treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[n].node);
		}
		if (treebuilder->context.element_stack[0].type == HTML) {
			treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[0].node);
		}
	}
	free(treebuilder->context.element_stack);
	treebuilder->context.element_stack = NULL;

	for (entry = treebuilder->context.formatting_list; entry != NULL;
			entry = next) {
		next = entry->next;

		if (treebuilder->tree_handler != NULL) {
			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					entry->details.node);
		}

		free(entry);
	}

	free(treebuilder);

	return HUBBUB_OK;
}

/**
 * Configure a hubbub treebuilder
 *
 * \param treebuilder  The treebuilder instance to configure
 * \param type         The option type to configure
 * \param params       Pointer to option-specific parameters
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error hubbub_treebuilder_setopt(hubbub_treebuilder *treebuilder,
		hubbub_treebuilder_opttype type,
		hubbub_treebuilder_optparams *params)
{
	if (treebuilder == NULL || params == NULL)
		return HUBBUB_BADPARM;

	switch (type) {
	case HUBBUB_TREEBUILDER_ERROR_HANDLER:
		treebuilder->error_handler = params->error_handler.handler;
		treebuilder->error_pw = params->error_handler.pw;
		break;
	case HUBBUB_TREEBUILDER_TREE_HANDLER:
		treebuilder->tree_handler = params->tree_handler;
		break;
	case HUBBUB_TREEBUILDER_DOCUMENT_NODE:
		treebuilder->context.document = params->document_node;
		break;
	case HUBBUB_TREEBUILDER_ENABLE_SCRIPTING:
		treebuilder->context.enable_scripting =
				params->enable_scripting;
		break;
	}

	return HUBBUB_OK;
}

/**
 * Handle tokeniser emitting a token
 *
 * \param token  The emitted token
 * \param pw     Pointer to treebuilder instance
 */
hubbub_error hubbub_treebuilder_token_handler(const hubbub_token *token,
		void *pw)
{
	hubbub_treebuilder *treebuilder = (hubbub_treebuilder *) pw;
	hubbub_error err = HUBBUB_REPROCESS;

	/* Do nothing if we have no document node or there's no tree handler */
	if (treebuilder->context.document == NULL ||
			treebuilder->tree_handler == NULL)
		return HUBBUB_OK;

	assert((signed) treebuilder->context.current_node >= 0);

/* A slightly nasty debugging hook, but very useful */
#ifdef NDEBUG
# define mode(x) \
		case x:
#else
# define mode(x) \
		case x: \
			printf( #x "\n");
#endif

	while (err == HUBBUB_REPROCESS) {
		switch (treebuilder->context.mode) {
		mode(INITIAL)
			err = handle_initial(treebuilder, token);
			break;
		mode(BEFORE_HTML)
			err = handle_before_html(treebuilder, token);
			break;
		mode(BEFORE_HEAD)
			err = handle_before_head(treebuilder, token);
			break;
		mode(IN_HEAD)
			err = handle_in_head(treebuilder, token);
			break;
		mode(IN_HEAD_NOSCRIPT)
			err = handle_in_head_noscript(treebuilder, token);
			break;
		mode(AFTER_HEAD)
			err = handle_after_head(treebuilder, token);
			break;
		mode(IN_BODY)
			err = handle_in_body(treebuilder, token);
			break;
		mode(IN_TABLE)
			err = handle_in_table(treebuilder, token);
			break;
		mode(IN_CAPTION)
			err = handle_in_caption(treebuilder, token);
			break;
		mode(IN_COLUMN_GROUP)
			err = handle_in_column_group(treebuilder, token);
			break;
		mode(IN_TABLE_BODY)
			err = handle_in_table_body(treebuilder, token);
			break;
		mode(IN_ROW)
			err = handle_in_row(treebuilder, token);
			break;
		mode(IN_CELL)
			err = handle_in_cell(treebuilder, token);
			break;
		mode(IN_SELECT)
			err = handle_in_select(treebuilder, token);
			break;
		mode(IN_SELECT_IN_TABLE)
			err = handle_in_select_in_table(treebuilder, token);
			break;
		mode(IN_FOREIGN_CONTENT)
			err = handle_in_foreign_content(treebuilder, token);
			break;
		mode(AFTER_BODY)
			err = handle_after_body(treebuilder, token);
			break;
		mode(IN_FRAMESET)
			err = handle_in_frameset(treebuilder, token);
			break;
		mode(AFTER_FRAMESET)
			err = handle_after_frameset(treebuilder, token);
			break;
		mode(AFTER_AFTER_BODY)
			err = handle_after_after_body(treebuilder, token);
			break;
		mode(AFTER_AFTER_FRAMESET)
			err = handle_after_after_frameset(treebuilder, token);
			break;
		mode(GENERIC_RCDATA)
			err = handle_generic_rcdata(treebuilder, token);
			break;
		}
	}

	return err;
}


/**
 * Process a character token in cases where we expect only whitespace
 *
 * \param treebuilder               The treebuilder instance
 * \param token                     The character token
 * \param insert_into_current_node  Whether to insert whitespace into
 *                                  current node
 * \return HUBBUB_REPROCESS if the token needs reprocessing
 *                          (token data updated to skip any leading whitespace),
 *         HUBBUB_OK if it contained only whitespace,
 *         appropriate error otherwise
 */
hubbub_error process_characters_expect_whitespace(
		hubbub_treebuilder *treebuilder,
		const hubbub_token *token, bool insert_into_current_node)
{
	const uint8_t *data = token->data.character.ptr;
	size_t len = token->data.character.len;
	size_t c;

	for (c = 0; c < len; c++) {
		if (data[c] != 0x09 && data[c] != 0x0A &&
				data[c] != 0x0C && data[c] != 0x20)
			break;
	}

	if (c > 0 && insert_into_current_node) {
		hubbub_error error;
		hubbub_string temp;

		temp.ptr = data;
		temp.len = c;

		error = append_text(treebuilder, &temp);
		if (error != HUBBUB_OK)
			return error;
	}

	/* Non-whitespace characters in token, so reprocess */
	if (c != len) {
		/* Update token data to strip leading whitespace */
		((hubbub_token *) token)->data.character.ptr += c;
		((hubbub_token *) token)->data.character.len -= c;

		return HUBBUB_REPROCESS;
	}

	return HUBBUB_OK;
}

/**
 * Process a comment token, appending it to the given parent
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The comment token
 * \param parent       The node to append to
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error process_comment_append(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, void *parent)
{
	hubbub_error error = HUBBUB_OK;
	element_type type = current_node(treebuilder);
	void *comment, *appended;

	error = treebuilder->tree_handler->create_comment(
			treebuilder->tree_handler->ctx,
			&token->data.comment, &comment);
	if (error != HUBBUB_OK)
		return error;

	if (treebuilder->context.in_table_foster &&
			(type == TABLE || type == TBODY || type == TFOOT ||
			type == THEAD || type == TR)) {
		error = aa_insert_into_foster_parent(treebuilder, comment,
				&appended);
	} else {
		error = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				parent, comment, &appended);
	}

	if (error == HUBBUB_OK) {
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, appended);
	}

	treebuilder->tree_handler->unref_node(
			treebuilder->tree_handler->ctx, comment);

	return error;
}

/**
 * Trigger parsing of generic (R)CDATA
 *
 * \param treebuilder  The treebuilder instance
 * \param token        The current token
 * \param rcdata       True for RCDATA, false for CDATA
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error parse_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, bool rcdata)
{
	hubbub_error error;
	element_type type;
	hubbub_tokeniser_optparams params;

	type = element_type_from_name(treebuilder, &token->data.tag.name);

	error = insert_element(treebuilder, &token->data.tag, true);
	if (error != HUBBUB_OK)
		return error;

	params.content_model.model = rcdata ? HUBBUB_CONTENT_MODEL_RCDATA
					    : HUBBUB_CONTENT_MODEL_CDATA;
	error = hubbub_tokeniser_setopt(treebuilder->tokeniser,
				HUBBUB_TOKENISER_CONTENT_MODEL, &params);
	/* There is no way that setopt can fail. Ensure this. */
	assert(error == HUBBUB_OK);

	treebuilder->context.collect.mode = treebuilder->context.mode;
	treebuilder->context.collect.type = type;

	treebuilder->context.mode = GENERIC_RCDATA;

	return HUBBUB_OK;
}

/**
 * Determine if an element is in (table) scope
 *
 * \param treebuilder  Treebuilder to look in
 * \param type         Element type to find
 * \param in_table     Whether we're looking in table scope
 * \return Element stack index, or 0 if not in scope
 */
uint32_t element_in_scope(hubbub_treebuilder *treebuilder,
		element_type type, bool in_table)
{
	uint32_t node;

	if (treebuilder->context.element_stack == NULL)
		return 0;

	assert((signed) treebuilder->context.current_node >= 0);

	for (node = treebuilder->context.current_node; node > 0; node--) {
		hubbub_ns node_ns =
				treebuilder->context.element_stack[node].ns;
		element_type node_type =
				treebuilder->context.element_stack[node].type;

		if (node_type == type)
			return node;

		if (node_type == TABLE)
			break;

		/* The list of element types given in the spec here are the
		 * scoping elements excluding TABLE and HTML. TABLE is handled
		 * in the previous conditional and HTML should only occur
		 * as the first node in the stack, which is never processed
		 * in this loop. */
		if (!in_table && (is_scoping_element(node_type) ||
				(node_type == FOREIGNOBJECT &&
						node_ns == HUBBUB_NS_SVG))) {
			break;
		}
	}

	return 0;
}

/**
 * Reconstruct the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error reconstruct_active_formatting_list(hubbub_treebuilder *treebuilder)
{
	hubbub_error error = HUBBUB_OK;
	formatting_list_entry *entry, *initial_entry;
	uint32_t sp = treebuilder->context.current_node;

	if (treebuilder->context.formatting_list == NULL)
		return HUBBUB_OK;

	entry = treebuilder->context.formatting_list_end;

	/* Assumption: HTML and TABLE elements are not inserted into the list */
	if (is_scoping_element(entry->details.type) || entry->stack_index != 0)
		return HUBBUB_OK;

	while (entry->prev != NULL) {
		entry = entry->prev;

		if (is_scoping_element(entry->details.type) ||
				entry->stack_index != 0) {
			entry = entry->next;
			break;
		}
	}

	/* Save initial entry for later */
	initial_entry = entry;

	/* Process formatting list entries, cloning nodes and
	 * inserting them into the DOM and element stack */
	while (entry != NULL) {
		void *clone, *appended;
		bool foster;
		element_type type = current_node(treebuilder);

		error = treebuilder->tree_handler->clone_node(
				treebuilder->tree_handler->ctx,
				entry->details.node,
				false,
				&clone);
		if (error != HUBBUB_OK)
			goto cleanup;

		foster = treebuilder->context.in_table_foster &&
				(type == TABLE || type == TBODY ||
					type == TFOOT || type == THEAD ||
					type == TR);

		if (foster) {
			error = aa_insert_into_foster_parent(treebuilder,
					clone, &appended);
		} else {
			error = treebuilder->tree_handler->append_child(
					treebuilder->tree_handler->ctx,
					treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
					clone,
					&appended);
		}

		/* No longer interested in clone */
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				clone);

		if (error != HUBBUB_OK)
			goto cleanup;

		error = element_stack_push(treebuilder, entry->details.ns,
				entry->details.type, appended);
		if (error != HUBBUB_OK) {
			remove_node_from_dom(treebuilder, appended);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					appended);

			goto cleanup;
		}

		entry = entry->next;
	}

	/* Now, replace the formatting list entries */
	for (entry = initial_entry; entry != NULL; entry = entry->next) {
		void *node;
		hubbub_ns prev_ns;
		element_type prev_type;
		void *prev_node;
		uint32_t prev_stack_index;

		node = treebuilder->context.element_stack[++sp].node;

		treebuilder->tree_handler->ref_node(
				treebuilder->tree_handler->ctx, node);

		error = formatting_list_replace(treebuilder, entry,
				entry->details.ns, entry->details.type,
				node, sp,
				&prev_ns, &prev_type, &prev_node,
				&prev_stack_index);
		/* Cannot fail. Ensure this. */
		assert(error == HUBBUB_OK);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				prev_node);
	}

	return HUBBUB_OK;

cleanup:
	/* An error occurred while cloning nodes and inserting them.
	 * We must restore the state on entry here. */
	while (treebuilder->context.current_node > sp) {
		hubbub_ns ns;
		element_type type;
		void *node;

		element_stack_pop(treebuilder, &ns, &type, &node);

		remove_node_from_dom(treebuilder, node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);
	}

	return error;
}

/**
 * Remove a node from the DOM
 *
 * \param treebuilder  Treebuilder instance
 * \param node         Node to remove
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error remove_node_from_dom(hubbub_treebuilder *treebuilder, void *node)
{
	hubbub_error err;
	void *parent = NULL;
	void *removed;

	err = treebuilder->tree_handler->get_parent(
			treebuilder->tree_handler->ctx,
			node, false, &parent);
	if (err != HUBBUB_OK)
		return err;

	if (parent != NULL) {
		err = treebuilder->tree_handler->remove_child(
				treebuilder->tree_handler->ctx,
				parent, node, &removed);
		if (err != HUBBUB_OK)
			return err;

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				parent);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				removed);
	}

	return HUBBUB_OK;
}

/**
 * Clear the list of active formatting elements up to the last marker
 *
 * \param treebuilder  The treebuilder instance containing the list
 */
void clear_active_formatting_list_to_marker(hubbub_treebuilder *treebuilder)
{
	formatting_list_entry *entry;
	bool done = false;

	while ((entry = treebuilder->context.formatting_list_end) != NULL) {
		hubbub_ns ns;
		element_type type;
		void *node;
		uint32_t stack_index;

		if (is_scoping_element(entry->details.type))
			done = true;

		formatting_list_remove(treebuilder, entry,
				&ns, &type, &node, &stack_index);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		if (done)
			break;
	}
}

/**
 * Create element and insert it into the DOM,
 * potentially pushing it on the stack
 *
 * \param treebuilder  The treebuilder instance
 * \param tag          The element to insert
 * \param push         Whether to push the element onto the stack
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error insert_element(hubbub_treebuilder *treebuilder,
		const hubbub_tag *tag, bool push)
{
	element_type type = current_node(treebuilder);
	hubbub_error error;
	void *node, *appended;

	error = treebuilder->tree_handler->create_element(
			treebuilder->tree_handler->ctx, tag, &node);
	if (error != HUBBUB_OK)
		return error;

	if (treebuilder->context.in_table_foster &&
			(type == TABLE || type == TBODY || type == TFOOT ||
			type == THEAD || type == TR)) {
		error = aa_insert_into_foster_parent(treebuilder, node,
				&appended);
	} else {
		error = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
				node, &appended);
	}

	/* No longer interested in node */
	treebuilder->tree_handler->unref_node(
			treebuilder->tree_handler->ctx, node);

	if (error != HUBBUB_OK)
		return error;

	type = element_type_from_name(treebuilder, &tag->name);
	if (treebuilder->context.form_element != NULL &&
			is_form_associated(type)) {
		/* Consideration of @form is left to the client */
		error = treebuilder->tree_handler->form_associate(
				treebuilder->tree_handler->ctx,
				treebuilder->context.form_element,
				appended);
		if (error != HUBBUB_OK) {
			remove_node_from_dom(treebuilder, appended);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					appended);

			return error;
		}
	}

	if (push) {
		error = element_stack_push(treebuilder,
				tag->ns, type, appended);
		if (error != HUBBUB_OK) {
			remove_node_from_dom(treebuilder, appended);

			treebuilder->tree_handler->unref_node(
					treebuilder->tree_handler->ctx,
					appended);
			return error;
		}
	} else {
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, appended);
	}

	return HUBBUB_OK;
}

/**
 * Close implied end tags
 *
 * \param treebuilder  The treebuilder instance
 * \param except       Tag type to exclude from processing [DD,DT,LI,OPTION,
 *                     OPTGROUP,P,RP,RT], UNKNOWN to exclude nothing
 */
void close_implied_end_tags(hubbub_treebuilder *treebuilder,
		element_type except)
{
	element_type type;

	type = treebuilder->context.element_stack[
			treebuilder->context.current_node].type;

	while (type == DD || type == DT || type == LI || type == OPTION ||
			type == OPTGROUP || type == P || type == RP ||
			type == RT) {
		hubbub_ns ns;
		element_type otype;
		void *node;

		if (except != UNKNOWN && type == except)
			break;

		element_stack_pop(treebuilder, &ns, &otype, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx,
				node);

		type = treebuilder->context.element_stack[
				treebuilder->context.current_node].type;
	}
}

/**
 * Reset the insertion mode
 *
 * \param treebuilder  The treebuilder to reset
 */
void reset_insertion_mode(hubbub_treebuilder *treebuilder)
{
	uint32_t node;
	element_context *stack = treebuilder->context.element_stack;

	/** \todo fragment parsing algorithm */

	for (node = treebuilder->context.current_node; node > 0; node--) {
		if (stack[node].ns != HUBBUB_NS_HTML) {
			treebuilder->context.mode = IN_FOREIGN_CONTENT;
			treebuilder->context.second_mode = IN_BODY;
			break;
		}

		switch (stack[node].type) {
		case SELECT:
			/* fragment case */
			break;
		case TD:
		case TH:
			treebuilder->context.mode = IN_CELL;
			return;
		case TR:
			treebuilder->context.mode = IN_ROW;
			return;
		case TBODY:
		case TFOOT:
		case THEAD:
			treebuilder->context.mode = IN_TABLE_BODY;
			return;
		case CAPTION:
			treebuilder->context.mode = IN_CAPTION;
			return;
		case COLGROUP:
			/* fragment case */
			break;
		case TABLE:
			treebuilder->context.mode = IN_TABLE;
			return;
		case HEAD:
			/* fragment case */
			break;
		case BODY:
			treebuilder->context.mode = IN_BODY;
			return;
		case FRAMESET:
			/* fragment case */
			break;
		case HTML:
			/* fragment case */
			break;
		default:
			break;
		}
	}
}

/**
 * Script processing and execution
 *
 * \param treebuilder  The treebuilder instance
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error complete_script(hubbub_treebuilder *treebuilder)
{
	hubbub_error error = HUBBUB_OK;
	error = treebuilder->tree_handler->complete_script(
		treebuilder->tree_handler->ctx,
		treebuilder->context.element_stack[
			treebuilder->context.current_node].node);
	return error;
}

/**
 * Append text to the current node, inserting into the last child of the
 * current node, iff it's a Text node.
 *
 * \param treebuilder  The treebuilder instance
 * \param string       The string to append
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error append_text(hubbub_treebuilder *treebuilder,
		const hubbub_string *string)
{
	element_type type = current_node(treebuilder);
	hubbub_error error = HUBBUB_OK;
	void *text, *appended;

	error = treebuilder->tree_handler->create_text(
			treebuilder->tree_handler->ctx, string, &text);
	if (error != HUBBUB_OK)
		return error;

	if (treebuilder->context.in_table_foster &&
			(type == TABLE || type == TBODY || type == TFOOT ||
			type == THEAD || type == TR)) {
		error = aa_insert_into_foster_parent(treebuilder, text,
				&appended);
	} else {
		error = treebuilder->tree_handler->append_child(
				treebuilder->tree_handler->ctx,
				treebuilder->context.element_stack[
					treebuilder->context.current_node].node,
						text, &appended);
	}

	if (error == HUBBUB_OK) {
		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, appended);
	}

	treebuilder->tree_handler->unref_node(
			treebuilder->tree_handler->ctx, text);

	return error;
}

/**
 * Convert an element name into an element type
 *
 * \param treebuilder  The treebuilder instance
 * \param tag_name     The tag name to consider
 * \return The corresponding element type
 */
element_type element_type_from_name(hubbub_treebuilder *treebuilder,
		const hubbub_string *tag_name)
{
	const uint8_t *name = tag_name->ptr;
	size_t len = tag_name->len;
	uint32_t i;

	UNUSED(treebuilder);

	/** \todo optimise this */

	for (i = 0; i < N_ELEMENTS(name_type_map); i++) {
		if (name_type_map[i].len != len)
			continue;

		if (strncasecmp(name_type_map[i].name,
				(const char *) name, len) == 0)
			return name_type_map[i].type;
	}

	return UNKNOWN;
}

/**
 * Determine if a node is a special element
 *
 * \param type  Node type to consider
 * \return True iff node is a special element
 */
bool is_special_element(element_type type)
{
	return (type <= WBR);
}

/**
 * Determine if a node is a scoping element
 *
 * \param type  Node type to consider
 * \return True iff node is a scoping element
 */
bool is_scoping_element(element_type type)
{
	return (type >= APPLET && type <= TH);
}

/**
 * Determine if a node is a formatting element
 *
 * \param type  Node type to consider
 * \return True iff node is a formatting element
 */
bool is_formatting_element(element_type type)
{
	return (type >= A && type <= U);
}

/**
 * Determine if a node is a phrasing element
 *
 * \param type  Node type to consider
 * \return True iff node is a phrasing element
 */
bool is_phrasing_element(element_type type)
{
	return (type > U);
}

/**
 * Determine if a node is form associated
 *
 * \param type  Node type to consider
 * \return True iff node is form associated
 */
bool is_form_associated(element_type type)
{
	return type == FIELDSET || type == LABEL || type == INPUT ||
			type == BUTTON || type == SELECT || type == TEXTAREA ||
			type == OUTPUT;
}

/**
 * Push an element onto the stack of open elements
 *
 * \param treebuilder  The treebuilder instance containing the stack
 * \param ns           The namespace of element being pushed
 * \param type         The type of element being pushed
 * \param node         The node to push
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error element_stack_push(hubbub_treebuilder *treebuilder,
		hubbub_ns ns, element_type type, void *node)
{
	uint32_t slot = treebuilder->context.current_node + 1;

	if (slot >= treebuilder->context.stack_alloc) {
		element_context *temp = realloc(
				treebuilder->context.element_stack,
				(treebuilder->context.stack_alloc +
					ELEMENT_STACK_CHUNK) *
					sizeof(element_context));

		if (temp == NULL)
			return HUBBUB_NOMEM;

		treebuilder->context.element_stack = temp;
		treebuilder->context.stack_alloc += ELEMENT_STACK_CHUNK;
	}

	treebuilder->context.element_stack[slot].ns = ns;
	treebuilder->context.element_stack[slot].type = type;
	treebuilder->context.element_stack[slot].node = node;

	treebuilder->context.current_node = slot;

	return HUBBUB_OK;
}

/**
 * Pop an element off the stack of open elements
 *
 * \param treebuilder  The treebuilder instance containing the stack
 * \param ns           Pointer to location to receive element namespace
 * \param type         Pointer to location to receive element type
 * \param node         Pointer to location to receive node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error element_stack_pop(hubbub_treebuilder *treebuilder,
		hubbub_ns *ns, element_type *type, void **node)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t slot = treebuilder->context.current_node;
	formatting_list_entry *entry;

	/* We're popping a table, find previous */
	if (stack[slot].type == TABLE) {
		uint32_t t;
		for (t = slot - 1; t > 0; t--) {
			if (stack[t].type == TABLE)
				break;
		}
	}

	if (is_formatting_element(stack[slot].type) ||
			(is_scoping_element(stack[slot].type) &&
			stack[slot].type != HTML &&
			stack[slot].type != TABLE)) {
		/* Find occurrences of the node we're about to pop in the list
		 * of active formatting elements. We need to invalidate their
		 * stack index information. */
		for (entry = treebuilder->context.formatting_list_end;
				entry != NULL; entry = entry->prev) {
			/** \todo Can we optimise this?
			 * (i.e. by not traversing the entire list) */
			if (entry->stack_index == slot)
				entry->stack_index = 0;
		}
	}

	*ns = stack[slot].ns;
	*type = stack[slot].type;
	*node = stack[slot].node;

	/** \todo reduce allocated stack size once there's enough free */

	treebuilder->context.current_node = slot - 1;
	assert((signed) treebuilder->context.current_node >= 0);

	return HUBBUB_OK;
}

/**
 * Pop elements until an element of type "element" has been popped.
 *
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error element_stack_pop_until(hubbub_treebuilder *treebuilder,
		element_type type)
{
	element_type otype = UNKNOWN;
	void *node;
	hubbub_ns ns;

	while (otype != type) {
		element_stack_pop(treebuilder, &ns, &otype, &node);

		treebuilder->tree_handler->unref_node(
				treebuilder->tree_handler->ctx, node);

		assert((signed) treebuilder->context.current_node >= 0);
	}

	return HUBBUB_OK;
}

/**
 * Remove a node from the stack of open elements
 *
 * \param treebuilder  The treebuilder instance
 * \param index        The index of the node to remove
 * \param ns           Pointer to location to receive namespace
 * \param type         Pointer to location to receive type
 * \param removed      Pointer to location to receive removed node
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error element_stack_remove(hubbub_treebuilder *treebuilder,
		uint32_t index, hubbub_ns *ns, element_type *type,
		void **removed)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t n;

	assert(index <= treebuilder->context.current_node);

	/* Scan over subsequent entries in the stack,
	 * searching for them in the list of active formatting
	 * entries. If found, update the corresponding
	 * formatting list entry's stack index to match the
	 * new stack location */
	for (n = index + 1; n <= treebuilder->context.current_node; n++) {
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

	*ns = stack[index].ns;
	*type = stack[index].type;
	*removed = stack[index].node;

	/* Now, shuffle the stack up one, removing node in the process */
	if (index < treebuilder->context.current_node) {
		memmove(&stack[index], &stack[index + 1],
				(treebuilder->context.current_node - index) *
				sizeof(element_context));
	}

	treebuilder->context.current_node--;

	return HUBBUB_OK;
}

/**
 * Find the stack index of the current table.
 */
uint32_t current_table(hubbub_treebuilder *treebuilder)
{
	element_context *stack = treebuilder->context.element_stack;
	size_t t;

	for (t = treebuilder->context.current_node; t != 0; t--) {
		if (stack[t].type == TABLE)
			return t;
	}

	/* fragment case */
	return 0;
}

/**
 * Peek at the top element of the element stack.
 *
 * \param treebuilder  Treebuilder instance
 * \return Element type on the top of the stack
 */
element_type current_node(hubbub_treebuilder *treebuilder)
{
	return treebuilder->context.element_stack
			[treebuilder->context.current_node].type;
}

/**
 * Peek at the element below the top of the element stack.
 *
 * \param treebuilder  Treebuilder instance
 * \return Element type of the element one below the top of the stack
 */
element_type prev_node(hubbub_treebuilder *treebuilder)
{
	if (treebuilder->context.current_node == 0)
		return UNKNOWN;

	return treebuilder->context.element_stack
			[treebuilder->context.current_node - 1].type;
}



/**
 * Append an element to the end of the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \param ns           Namespace of node being inserted
 * \param type         Type of node being inserted
 * \param node         Node being inserted
 * \param stack_index  Index into stack of open elements
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error formatting_list_append(hubbub_treebuilder *treebuilder,
		hubbub_ns ns, element_type type, void *node,
		uint32_t stack_index)
{
	formatting_list_entry *entry;

	entry = malloc(sizeof(formatting_list_entry));
	if (entry == NULL)
		return HUBBUB_NOMEM;

	entry->details.ns = ns;
	entry->details.type = type;
	entry->details.node = node;
	entry->stack_index = stack_index;

	entry->prev = treebuilder->context.formatting_list_end;
	entry->next = NULL;

	if (entry->prev != NULL)
		entry->prev->next = entry;
	else
		treebuilder->context.formatting_list = entry;

	treebuilder->context.formatting_list_end = entry;

	return HUBBUB_OK;
}

/**
 * Insert an element into the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \param prev         Previous entry
 * \param next         Next entry
 * \param ns           Namespace of node being inserted
 * \param type         Type of node being inserted
 * \param node         Node being inserted
 * \param stack_index  Index into stack of open elements
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error formatting_list_insert(hubbub_treebuilder *treebuilder,
		formatting_list_entry *prev, formatting_list_entry *next,
		hubbub_ns ns, element_type type, void *node,
		uint32_t stack_index)
{
	formatting_list_entry *entry;

	if (prev != NULL) {
		assert(prev->next == next);
	}

	if (next != NULL) {
		assert(next->prev == prev);
	}

	entry = malloc(sizeof(formatting_list_entry));
	if (entry == NULL)
		return HUBBUB_NOMEM;

	entry->details.ns = ns;
	entry->details.type = type;
	entry->details.node = node;
	entry->stack_index = stack_index;

	entry->prev = prev;
	entry->next = next;

	if (entry->prev != NULL)
		entry->prev->next = entry;
	else
		treebuilder->context.formatting_list = entry;

	if (entry->next != NULL)
		entry->next->prev = entry;
	else
		treebuilder->context.formatting_list_end = entry;

	return HUBBUB_OK;
}


/**
 * Remove an element from the list of active formatting elements
 *
 * \param treebuilder  Treebuilder instance containing list
 * \param entry        The item to remove
 * \param ns           Pointer to location to receive namespace of node
 * \param type         Pointer to location to receive type of node
 * \param node         Pointer to location to receive node
 * \param stack_index  Pointer to location to receive stack index
 * \return HUBBUB_OK on success, appropriate error otherwise.
 */
hubbub_error formatting_list_remove(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		hubbub_ns *ns, element_type *type, void **node,
		uint32_t *stack_index)
{
	*ns = entry->details.ns;
	*type = entry->details.type;
	*node = entry->details.node;
	*stack_index = entry->stack_index;

	if (entry->prev == NULL)
		treebuilder->context.formatting_list = entry->next;
	else
		entry->prev->next = entry->next;

	if (entry->next == NULL)
		treebuilder->context.formatting_list_end = entry->prev;
	else
		entry->next->prev = entry->prev;

	free(entry);

	return HUBBUB_OK;
}

/**
 * Remove an element from the list of active formatting elements
 *
 * \param treebuilder   Treebuilder instance containing list
 * \param entry         The item to replace
 * \param ns            Replacement node namespace
 * \param type          Replacement node type
 * \param node          Replacement node
 * \param stack_index   Replacement stack index
 * \param ons           Pointer to location to receive old namespace
 * \param otype         Pointer to location to receive old type
 * \param onode         Pointer to location to receive old node
 * \param ostack_index  Pointer to location to receive old stack index
 * \return HUBBUB_OK on success, appropriate error otherwise
 */
hubbub_error formatting_list_replace(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		hubbub_ns ns, element_type type, void *node,
		uint32_t stack_index,
		hubbub_ns *ons, element_type *otype, void **onode,
		uint32_t *ostack_index)
{
	UNUSED(treebuilder);

	*ons = entry->details.ns;
	*otype = entry->details.type;
	*onode = entry->details.node;
	*ostack_index = entry->stack_index;

	entry->details.ns = ns;
	entry->details.type = type;
	entry->details.node = node;
	entry->stack_index = stack_index;

	return HUBBUB_OK;
}



#ifndef NDEBUG

/**
 * Dump an element stack to the given file pointer
 *
 * \param treebuilder  The treebuilder instance
 * \param fp           The file to dump to
 */
void element_stack_dump(hubbub_treebuilder *treebuilder, FILE *fp)
{
	element_context *stack = treebuilder->context.element_stack;
	uint32_t i;

	for (i = 0; i <= treebuilder->context.current_node; i++) {
		fprintf(fp, "%u: %s %p\n",
				i,
				element_type_to_name(stack[i].type),
				stack[i].node);
	}
}

/**
 * Dump a formatting list to the given file pointer
 *
 * \param treebuilder  The treebuilder instance
 * \param fp           The file to dump to
 */
void formatting_list_dump(hubbub_treebuilder *treebuilder, FILE *fp)
{
	formatting_list_entry *entry;

	for (entry = treebuilder->context.formatting_list; entry != NULL;
			entry = entry->next) {
		fprintf(fp, "%s %p %u\n",
				element_type_to_name(entry->details.type),
				entry->details.node, entry->stack_index);
	}
}

/**
 * Convert an element type to a name
 *
 * \param type  The element type
 * \return Pointer to name
 */
const char *element_type_to_name(element_type type)
{
	size_t i;

	for (i = 0;
			i < sizeof(name_type_map) / sizeof(name_type_map[0]);
			i++) {
		if (name_type_map[i].type == type)
			return name_type_map[i].name;
	}

	return "UNKNOWN";
}
#endif

