/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef css_stylesheet_h_
#define css_stylesheet_h_

#include <inttypes.h>
#include <stdio.h>

#include <libwapcaplet/libwapcaplet.h>

#include <libcss/errors.h>
#include <libcss/functypes.h>
#include <libcss/stylesheet.h>
#include <libcss/types.h>

#include "bytecode/bytecode.h"
#include "parse/parse.h"
#include "select/hash.h"

typedef struct css_rule css_rule;
typedef struct css_selector css_selector;

typedef struct css_style {
	css_code_t *bytecode;	      /**< Pointer to bytecode */
	uint32_t used;		      /**< number of code entries used */
	uint32_t allocated;	      /**< number of allocated code entries */
	struct css_stylesheet *sheet; /**< containing sheet */
} css_style;

typedef enum css_selector_type {
	CSS_SELECTOR_ELEMENT,
	CSS_SELECTOR_CLASS,
	CSS_SELECTOR_ID,
	CSS_SELECTOR_PSEUDO_CLASS,
	CSS_SELECTOR_PSEUDO_ELEMENT,
	CSS_SELECTOR_ATTRIBUTE,
	CSS_SELECTOR_ATTRIBUTE_EQUAL,
	CSS_SELECTOR_ATTRIBUTE_DASHMATCH,
	CSS_SELECTOR_ATTRIBUTE_INCLUDES,
	CSS_SELECTOR_ATTRIBUTE_PREFIX,
	CSS_SELECTOR_ATTRIBUTE_SUFFIX,
	CSS_SELECTOR_ATTRIBUTE_SUBSTRING
} css_selector_type;

typedef enum css_combinator {
	CSS_COMBINATOR_NONE,
	CSS_COMBINATOR_ANCESTOR,
	CSS_COMBINATOR_PARENT,
	CSS_COMBINATOR_SIBLING,
	CSS_COMBINATOR_GENERIC_SIBLING
} css_combinator;

typedef enum css_selector_detail_value_type {
	CSS_SELECTOR_DETAIL_VALUE_STRING,
	CSS_SELECTOR_DETAIL_VALUE_NTH
} css_selector_detail_value_type;

typedef union css_selector_detail_value {
	lwc_string *string;		/**< Interned string, or NULL */
	struct {
		int32_t a;
		int32_t b;
	} nth;				/**< Data for x = an + b */
} css_selector_detail_value;

typedef struct css_selector_detail {
	css_qname qname;			/**< Interned name */
	css_selector_detail_value value;	/**< Detail value */

	unsigned int type       : 4,		/**< Type of selector */
		     comb       : 3,		/**< Type of combinator */
		     next       : 1,		/**< Another selector detail 
						 * follows */
		     value_type : 1,		/**< Type of value field */
		     negate     : 1;		/**< Detail match is inverted */
} css_selector_detail;

struct css_selector {
	css_selector *combinator;		/**< Combining selector */

	css_rule *rule;				/**< Owning rule */

#define CSS_SPECIFICITY_A 0x01000000
#define CSS_SPECIFICITY_B 0x00010000
#define CSS_SPECIFICITY_C 0x00000100
#define CSS_SPECIFICITY_D 0x00000001
	uint32_t specificity;			/**< Specificity of selector */

	css_selector_detail data;		/**< Selector data */
};

typedef enum css_rule_type {
	CSS_RULE_UNKNOWN,
	CSS_RULE_SELECTOR,
	CSS_RULE_CHARSET,
	CSS_RULE_IMPORT,
	CSS_RULE_MEDIA,
	CSS_RULE_FONT_FACE,
	CSS_RULE_PAGE
} css_rule_type;

typedef enum css_rule_parent_type {
	CSS_RULE_PARENT_STYLESHEET,
	CSS_RULE_PARENT_RULE
} css_rule_parent_type;

struct css_rule {
	void *parent;				/**< containing rule or owning 
						 * stylesheet (defined by ptype)
						 */
	css_rule *next;				/**< next in list */
	css_rule *prev;				/**< previous in list */

	unsigned int type  :  4,		/**< css_rule_type */
		     index : 16,		/**< index in sheet */
		     items :  8,		/**< # items in rule */
		     ptype :  1;		/**< css_rule_parent_type */
} _ALIGNED;

typedef struct css_rule_selector {
	css_rule base;

	css_selector **selectors;
	css_style *style;
} css_rule_selector;

typedef struct css_rule_media {
	css_rule base;

	uint64_t media;

	css_rule *first_child;
	css_rule *last_child;
} css_rule_media;

typedef struct css_rule_font_face {
	css_rule base;

	css_font_face *font_face;
} css_rule_font_face;

typedef struct css_rule_page {
	css_rule base;

	css_selector *selector;
	css_style *style;
} css_rule_page;

typedef struct css_rule_import {
	css_rule base;

	lwc_string *url;
	uint64_t media;

	css_stylesheet *sheet;
} css_rule_import;

typedef struct css_rule_charset {
	css_rule base;

	lwc_string *encoding;	/** \todo use MIB enum? */
} css_rule_charset;

struct css_stylesheet {
	css_selector_hash *selectors;		/**< Hashtable of selectors */

	uint32_t rule_count;			/**< Number of rules in sheet */
	css_rule *rule_list;			/**< List of rules in sheet */
	css_rule *last_rule;			/**< Last rule in list */

	bool disabled;				/**< Whether this sheet is 
						 * disabled */

	char *url;				/**< URL of this sheet */
	char *title;				/**< Title of this sheet */

	css_language_level level;		/**< Language level of sheet */
	css_parser *parser;			/**< Core parser for sheet */
	void *parser_frontend;			/**< Frontend parser */
	lwc_string **propstrings;		/**< Property strings, for parser */

	bool quirks_allowed;			/**< Quirks permitted */
	bool quirks_used;			/**< Quirks actually used */

	bool inline_style;			/**< Is an inline style */

	size_t size;				/**< Size, in bytes */

	css_import_notification_fn import;	/**< Import notification function */
	void *import_pw;			/**< Private word */

	css_url_resolution_fn resolve;		/**< URL resolution function */
	void *resolve_pw;			/**< Private word */

	css_color_resolution_fn color;		/**< Colour resolution function */
	void *color_pw;				/**< Private word */

	/** System font resolution function */
	css_font_resolution_fn font;		
	void *font_pw;				/**< Private word */
  
	css_style *cached_style;		/**< Cache for style parsing */
  
	lwc_string **string_vector;             /**< Bytecode string vector */
	uint32_t string_vector_l;               /**< The string vector allocated
						 * length in entries */
	uint32_t string_vector_c;               /**< The number of string 
						 * vector entries used */ 
};

css_error css__stylesheet_style_create(css_stylesheet *sheet, 
		css_style **style);
css_error css__stylesheet_style_append(css_style *style, css_code_t code);
css_error css__stylesheet_style_vappend(css_style *style, uint32_t style_count,
		...);
css_error css__stylesheet_style_destroy(css_style *style);
css_error css__stylesheet_merge_style(css_style *target, css_style *style);

/** Helper function to avoid distinct buildOPV call */ 
static inline css_error css__stylesheet_style_appendOPV(css_style *style, 
		opcode_t opcode, uint8_t flags, uint16_t value)
{
	return css__stylesheet_style_append(style, 
			buildOPV(opcode, flags, value));
}

/** Helper function to set inherit flag */ 
static inline css_error css_stylesheet_style_inherit(css_style *style, 
		opcode_t opcode)
{
	return css__stylesheet_style_append(style, 
			buildOPV(opcode, FLAG_INHERIT, 0));
}

css_error css__stylesheet_selector_create(css_stylesheet *sheet,
		css_qname *qname, css_selector **selector);
css_error css__stylesheet_selector_destroy(css_stylesheet *sheet,
		css_selector *selector);

css_error css__stylesheet_selector_detail_init(css_stylesheet *sheet,
		css_selector_type type, css_qname *qname,
		css_selector_detail_value value, 
		css_selector_detail_value_type value_type,
		bool negate, css_selector_detail *detail);

css_error css__stylesheet_selector_append_specific(css_stylesheet *sheet,
		css_selector **parent, const css_selector_detail *specific);

css_error css__stylesheet_selector_combine(css_stylesheet *sheet,
		css_combinator type, css_selector *a, css_selector *b);

css_error css__stylesheet_rule_create(css_stylesheet *sheet, css_rule_type type,
		css_rule **rule);
css_error css__stylesheet_rule_destroy(css_stylesheet *sheet, css_rule *rule);

css_error css__stylesheet_rule_add_selector(css_stylesheet *sheet, 
		css_rule *rule, css_selector *selector);

css_error css__stylesheet_rule_append_style(css_stylesheet *sheet,
		css_rule *rule, css_style *style);

css_error css__stylesheet_rule_set_charset(css_stylesheet *sheet,
		css_rule *rule, lwc_string *charset);

css_error css__stylesheet_rule_set_nascent_import(css_stylesheet *sheet,
		css_rule *rule, lwc_string *url, uint64_t media);

css_error css__stylesheet_rule_set_media(css_stylesheet *sheet,
		css_rule *rule, uint64_t media);

css_error css__stylesheet_rule_set_page_selector(css_stylesheet *sheet,
		css_rule *rule, css_selector *sel);

css_error css__stylesheet_add_rule(css_stylesheet *sheet, css_rule *rule,
		css_rule *parent);
css_error css__stylesheet_remove_rule(css_stylesheet *sheet, css_rule *rule);

css_error css__stylesheet_string_get(css_stylesheet *sheet, 
		uint32_t string_number, lwc_string **string);

css_error css__stylesheet_string_add(css_stylesheet *sheet, 
		lwc_string *string, uint32_t *string_number);

#endif

