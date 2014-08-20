/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef hubbub_treebuilder_internal_h_
#define hubbub_treebuilder_internal_h_

#include "treebuilder/treebuilder.h"

typedef enum
{
/* Special */
	ADDRESS, AREA, ARTICLE, ASIDE, BASE, BASEFONT, BGSOUND, BLOCKQUOTE,
	BODY, BR, CENTER, COL, COLGROUP, COMMAND, DATAGRID, DD, DETAILS,
	DIALOG, DIR, DIV, DL, DT, EMBED, FIELDSET, FIGURE, FOOTER, FORM, FRAME,
	FRAMESET, H1, H2, H3, H4, H5, H6, HEAD, HEADER, HR, IFRAME, IMAGE, IMG,
	INPUT, ISINDEX, LI, LINK, LISTING, MENU, META, NAV, NOEMBED, NOFRAMES, 
	NOSCRIPT, OL, OPTGROUP, OPTION, P, PARAM, PLAINTEXT, PRE, SCRIPT, 
	SECTION, SELECT, SPACER, STYLE, TBODY, TEXTAREA, TFOOT, THEAD, TITLE, 
	TR, UL, WBR,
/* Scoping */
	APPLET, BUTTON, CAPTION, HTML, MARQUEE, OBJECT, TABLE, TD, TH,
/* Formatting */
	A, B, BIG, CODE, EM, FONT, I, NOBR, S, SMALL, STRIKE, STRONG, TT, U,
/* Phrasing */
	/**< \todo Enumerate phrasing elements */
	LABEL, OUTPUT, RP, RT, RUBY, SPAN, SUB, SUP, VAR, XMP,
/* MathML */
	MATH, MGLYPH, MALIGNMARK, MI, MO, MN, MS, MTEXT, ANNOTATION_XML,
/* SVG */
	SVG, FOREIGNOBJECT, /* foreignobject is scoping, but only in SVG ns */
	DESC,
	UNKNOWN
} element_type;

/**
 * Item on the element stack
 */
typedef struct element_context
{
	hubbub_ns ns;			/**< Element namespace */
	element_type type;		/**< Element type */
	uint8_t *name;			/**< Element name (interned) */

	bool tainted;			/**< Only for tables.  "Once the
					 * current table has been tainted,
					 * whitespace characters are inserted
					 * into the foster parent element
					 * instead of the current node." */

	void *node;			/**< Node pointer */
} element_context;

/**
 * Entry in a formatting list
 */
typedef struct formatting_list_entry
{
	element_context details;	/**< Entry details */

	uint32_t stack_index;		/**< Index into element stack */

	struct formatting_list_entry *prev;	/**< Previous in list */
	struct formatting_list_entry *next;	/**< Next in list */
} formatting_list_entry;

/**
 * Context for a tree builder
 */
typedef struct hubbub_treebuilder_context
{
	insertion_mode mode;		/**< The current insertion mode */
	insertion_mode second_mode;	/**< The secondary insertion mode */

#define ELEMENT_STACK_CHUNK 128
	element_context *element_stack;	/**< Stack of open elements */
	uint32_t stack_alloc;		/**< Number of stack slots allocated */
	uint32_t current_node;		/**< Index of current node in stack */

	formatting_list_entry *formatting_list;	/**< List of active formatting 
						 * elements */
	formatting_list_entry *formatting_list_end;	/**< End of active 
							 * formatting list */

	void *head_element;		/**< Pointer to HEAD element */

	void *form_element;		/**< Pointer to most recently 
					 * opened FORM element */

	void *document;			/**< Pointer to the document node */

	bool enable_scripting;		/**< Whether scripting is enabled */

	struct {
		insertion_mode mode;	/**< Insertion mode to return to */
		element_type type;	/**< Type of node */
	} collect;			/**< Context for character collecting */

	bool strip_leading_lr;		/**< Whether to strip a LR from the
					 * start of the next character sequence
					 * received */

	bool in_table_foster;		/**< Whether nodes that would be
					* inserted into the current node should
					* be foster parented */

	bool frameset_ok;		/**< Whether to process a frameset */
} hubbub_treebuilder_context;

/**
 * Treebuilder object
 */
struct hubbub_treebuilder
{
	hubbub_tokeniser *tokeniser;	/**< Underlying tokeniser */

	hubbub_treebuilder_context context;	/**< Our context */

	hubbub_tree_handler *tree_handler;	/**< Callback table */

	hubbub_error_handler error_handler;	/**< Error handler */
	void *error_pw;				/**< Error handler data */
};

hubbub_error hubbub_treebuilder_token_handler(
		const hubbub_token *token, void *pw);

hubbub_error process_characters_expect_whitespace(
		hubbub_treebuilder *treebuilder, const hubbub_token *token,
		bool insert_into_current_node);
hubbub_error process_comment_append(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, void *parent);
hubbub_error parse_generic_rcdata(hubbub_treebuilder *treebuilder,
		const hubbub_token *token, bool rcdata);

uint32_t element_in_scope(hubbub_treebuilder *treebuilder,
		element_type type, bool in_table);
hubbub_error reconstruct_active_formatting_list(
		hubbub_treebuilder *treebuilder);
void clear_active_formatting_list_to_marker(
		hubbub_treebuilder *treebuilder);
hubbub_error remove_node_from_dom(hubbub_treebuilder *treebuilder, 
		void *node);
hubbub_error insert_element(hubbub_treebuilder *treebuilder, 
		const hubbub_tag *tag_name, bool push);
void close_implied_end_tags(hubbub_treebuilder *treebuilder, 
		element_type except);
void reset_insertion_mode(hubbub_treebuilder *treebuilder);
hubbub_error append_text(hubbub_treebuilder *treebuilder,
		const hubbub_string *string);
hubbub_error complete_script(hubbub_treebuilder *treebuilder);

element_type element_type_from_name(hubbub_treebuilder *treebuilder,
		const hubbub_string *tag_name);

bool is_special_element(element_type type);
bool is_scoping_element(element_type type);
bool is_formatting_element(element_type type);
bool is_phrasing_element(element_type type);

hubbub_error element_stack_push(hubbub_treebuilder *treebuilder,
		hubbub_ns ns, element_type type, void *node);
hubbub_error element_stack_pop(hubbub_treebuilder *treebuilder,
		hubbub_ns *ns, element_type *type, void **node);
hubbub_error element_stack_pop_until(hubbub_treebuilder *treebuilder,
		element_type type);
hubbub_error element_stack_remove(hubbub_treebuilder *treebuilder, 
		uint32_t index, hubbub_ns *ns, element_type *type, 
		void **removed);
uint32_t current_table(hubbub_treebuilder *treebuilder);
element_type current_node(hubbub_treebuilder *treebuilder);
element_type prev_node(hubbub_treebuilder *treebuilder);

hubbub_error formatting_list_append(hubbub_treebuilder *treebuilder,
		hubbub_ns ns, element_type type, void *node, 
		uint32_t stack_index);
hubbub_error formatting_list_insert(hubbub_treebuilder *treebuilder,
		formatting_list_entry *prev, formatting_list_entry *next,
		hubbub_ns ns, element_type type, void *node, 
		uint32_t stack_index);
hubbub_error formatting_list_remove(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		hubbub_ns *ns, element_type *type, void **node, 
		uint32_t *stack_index);
hubbub_error formatting_list_replace(hubbub_treebuilder *treebuilder,
		formatting_list_entry *entry,
		hubbub_ns ns, element_type type, void *node, 
		uint32_t stack_index,
		hubbub_ns *ons, element_type *otype, void **onode, 
		uint32_t *ostack_index);

/* in_foreign_content.c */
void adjust_mathml_attributes(hubbub_treebuilder *treebuilder, hubbub_tag *tag);
void adjust_svg_attributes(hubbub_treebuilder *treebuilder,
		hubbub_tag *tag);
void adjust_svg_tagname(hubbub_treebuilder *treebuilder,
		hubbub_tag *tag);
void adjust_foreign_attributes(hubbub_treebuilder *treebuilder,
		hubbub_tag *tag);

/* in_body.c */
hubbub_error aa_insert_into_foster_parent(hubbub_treebuilder *treebuilder, 
		void *node, void **inserted);

#ifndef NDEBUG
#include <stdio.h>

void element_stack_dump(hubbub_treebuilder *treebuilder, FILE *fp);
void formatting_list_dump(hubbub_treebuilder *treebuilder, FILE *fp);

const char *element_type_to_name(element_type type);

#endif

#endif

