/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_treebuilder_modes_h_
#define hubbub_treebuilder_modes_h_

#include "treebuilder/treebuilder.h"

/** The various treebuilder insertion modes */
typedef enum
{
	INITIAL,
	BEFORE_HTML,
	BEFORE_HEAD,
	IN_HEAD,
	IN_HEAD_NOSCRIPT,
	AFTER_HEAD,
	IN_BODY,
	IN_TABLE,
	IN_CAPTION,
	IN_COLUMN_GROUP,
	IN_TABLE_BODY,
	IN_ROW,
	IN_CELL,
	IN_SELECT,
	IN_SELECT_IN_TABLE,
	IN_FOREIGN_CONTENT,
	AFTER_BODY,
	IN_FRAMESET,
	AFTER_FRAMESET,
	AFTER_AFTER_BODY,
	AFTER_AFTER_FRAMESET,

	GENERIC_RCDATA
} insertion_mode;



hubbub_error handle_initial(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_before_html(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_before_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_head_noscript(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_after_head(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_table(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_caption(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_column_group(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_table_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_row(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_cell(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_select(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_select_in_table(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_script_collect_characters(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_foreign_content(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_after_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_in_frameset(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_after_frameset(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_after_after_body(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);
hubbub_error handle_after_after_frameset(hubbub_treebuilder *treebuilder,
		const hubbub_token *token);

#endif
