/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *			http://www.opensource.org/licenses/mit-license.php
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 *
 * This file contains the API used to validate whether certain character in
 * name/value is legal according the XML 1.0 standard. See 
 *
 * http://www.w3.org/TR/2004/REC-xml-20040204/
 * http://www.w3.org/TR/REC-xml/
 * 
 * for detail.
 */

#ifndef dom_utils_character_valid_h_
#define dom_utils_character_valid_h_

#include <stdbool.h>
#include <stdlib.h>

struct xml_char_range {
	unsigned int start;
	unsigned int end;
};

struct xml_char_group {
	size_t len;
	const struct xml_char_range *range;
};

/* The groups */
extern const struct xml_char_group base_char_group;
extern const struct xml_char_group char_group;
extern const struct xml_char_group combining_char_group;
extern const struct xml_char_group digit_char_group;
extern const struct xml_char_group extender_group;
extern const struct xml_char_group ideographic_group;

bool _dom_is_character_in_group(unsigned int ch,
		const struct xml_char_group *group);

#define is_base_char(ch) _dom_is_character_in_group((ch), &base_char_group)
#define is_char(ch) _dom_is_character_in_group((ch), &char_group)
#define is_combining_char(ch) _dom_is_character_in_group((ch), \
		&combining_char_group)
#define is_digit(ch) _dom_is_character_in_group((ch), &digit_char_group)
#define is_extender(ch) _dom_is_character_in_group((ch), &extender_group)
#define is_ideographic(ch) _dom_is_character_in_group((ch), &ideographic_group)

#define is_letter(ch)  (is_base_char(ch) || is_ideographic(ch))

#endif

