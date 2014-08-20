/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 */

#ifndef dom_string_h_
#define dom_string_h_

#include <inttypes.h>
#include <stddef.h>
#include <libwapcaplet/libwapcaplet.h>

#include <dom/functypes.h>
#include <dom/core/exceptions.h>

typedef struct dom_string dom_string;
struct dom_string {
	uint32_t refcnt;
} _ALIGNED;


/* Claim a reference on a DOM string */
static inline dom_string *dom_string_ref(dom_string *str)
{
	if (str != NULL)
		str->refcnt++;
	return str;
}

/* Destroy a DOM string */
void dom_string_destroy(dom_string *str);

/* Release a reference on a DOM string */
static inline void dom_string_unref(dom_string *str) 
{
	if ((str != NULL) && (--(str->refcnt) == 0)) {
		dom_string_destroy(str);
	}
}

/* Create a DOM string from a string of characters */
dom_exception dom_string_create(const uint8_t *ptr, size_t len, 
		dom_string **str);
dom_exception dom_string_create_interned(const uint8_t *ptr, size_t len,
		dom_string **str);

/* Obtain an interned representation of a dom string */
dom_exception dom_string_intern(dom_string *str, 
		struct lwc_string_s **lwcstr);

/* Case sensitively compare two DOM strings */
bool dom_string_isequal(const dom_string *s1, const dom_string *s2);
/* Case insensitively compare two DOM strings */
bool dom_string_caseless_isequal(const dom_string *s1, const dom_string *s2);

/* Case sensitively compare DOM string and lwc_string */
bool dom_string_lwc_isequal(const dom_string *s1, lwc_string *s2);
/* Case insensitively compare DOM string and lwc_string */
bool dom_string_caseless_lwc_isequal(const dom_string *s1, lwc_string *s2);

/* Get the index of the first occurrence of a character in a dom string */
uint32_t dom_string_index(dom_string *str, uint32_t chr);
/* Get the index of the last occurrence of a character in a dom string */
uint32_t dom_string_rindex(dom_string *str, uint32_t chr);

/* Get the length, in characters, of a dom string */
uint32_t dom_string_length(dom_string *str);

/**
 * Get the raw character data of the dom_string.
 * @note: This function is just provided for the convenience of accessing the 
 * raw C string character, no change on the result string is allowed.
 */
const char *dom_string_data(const dom_string *str);

/* Get the byte length of this dom_string */
size_t dom_string_byte_length(const dom_string *str);

/* Get the UCS-4 character at position index, the index should be in 
 * [0, length), and length can be get by calling dom_string_length
 */
dom_exception dom_string_at(dom_string *str, uint32_t index, 
		uint32_t *ch);

/* Concatenate two dom strings */
dom_exception dom_string_concat(dom_string *s1, dom_string *s2,
		dom_string **result);

/* Extract a substring from a dom string */
dom_exception dom_string_substr(dom_string *str, 
		uint32_t i1, uint32_t i2, dom_string **result);

/* Insert data into a dom string at the given location */
dom_exception dom_string_insert(dom_string *target,
		dom_string *source, uint32_t offset,
		dom_string **result);

/* Replace a section of a dom string */
dom_exception dom_string_replace(dom_string *target,
		dom_string *source, uint32_t i1, uint32_t i2,
		dom_string **result);

/* Generate an uppercase version of the given string */
dom_exception dom_string_toupper(dom_string *source, bool ascii_only,
				 dom_string **upper);

/* Generate an lowercase version of the given string */
dom_exception dom_string_tolower(dom_string *source, bool ascii_only,
				 dom_string **lower);

/* Calculate a hash value from a dom string */
uint32_t dom_string_hash(dom_string *str);

#endif
