/*
 * This file is part of libdom.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2007 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2009 Bo Yang <struggleyb.nku@gmail.com>
 */

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <parserutils/charset/utf8.h>

#include "core/string.h"
#include "core/document.h"
#include "utils/utils.h"

/**
 * Type of a DOM string
 */
enum dom_string_type {
	DOM_STRING_CDATA = 0,
	DOM_STRING_INTERNED = 1
};

/**
 * A DOM string
 *
 * Strings are reference counted so destruction is performed correctly.
 */
typedef struct dom_string_internal {
	dom_string base;

	union {
		struct {
			uint8_t *ptr;	/**< Pointer to string data */
			size_t len;	/**< Byte length of string */
		} cdata;
		lwc_string *intern;	/**< Interned string */
	} data;

	enum dom_string_type type;	/**< String type */
} dom_string_internal;

/**
 * Empty string, for comparisons against NULL
 */
static const dom_string_internal empty_string = {
	{ 0 },
	{ { (uint8_t *) "", 0 } },
	DOM_STRING_CDATA
};

void dom_string_destroy(dom_string *str)
{
	dom_string_internal *istr = (dom_string_internal *)str;
	if (str != NULL) {
		assert(istr->base.refcnt == 0);
		switch (istr->type) {
		case DOM_STRING_INTERNED:
			if (istr->data.intern != NULL) {
				lwc_string_unref(istr->data.intern);
			}
			break;
		case DOM_STRING_CDATA:
			free(istr->data.cdata.ptr);
			break;
		}

		free(str);
	}
}

/**
 * Create a DOM string from a string of characters
 *
 * \param ptr    Pointer to string of characters
 * \param len    Length, in bytes, of string of characters
 * \param str    Pointer to location to receive result
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * The returned string will already be referenced, so there is no need
 * to explicitly reference it.
 *
 * The string of characters passed in will be copied for use by the 
 * returned DOM string.
 */
dom_exception dom_string_create(const uint8_t *ptr, size_t len, 
		dom_string **str)
{
	dom_string_internal *ret;

	if (ptr == NULL || len == 0) {
		ptr = (const uint8_t *) "";
		len = 0;
	}

	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return DOM_NO_MEM_ERR;

	ret->data.cdata.ptr = malloc(len + 1);
	if (ret->data.cdata.ptr == NULL) {
		free(ret);
		return DOM_NO_MEM_ERR;
	}

	memcpy(ret->data.cdata.ptr, ptr, len);
	ret->data.cdata.ptr[len] = '\0';

	ret->data.cdata.len = len;

	ret->base.refcnt = 1;

	ret->type = DOM_STRING_CDATA;

	*str = (dom_string *)ret;

	return DOM_NO_ERR;
}

/**
 * Create an interned DOM string from a string of characters
 *
 * \param ptr    Pointer to string of characters
 * \param len    Length, in bytes, of string of characters
 * \param str    Pointer to location to receive result
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * The returned string will already be referenced, so there is no need
 * to explicitly reference it.
 *
 * The string of characters passed in will be copied for use by the 
 * returned DOM string.
 */
dom_exception dom_string_create_interned(const uint8_t *ptr, size_t len, 
		dom_string **str)
{
	dom_string_internal *ret;

	if (ptr == NULL || len == 0) {
		ptr = (const uint8_t *) "";
		len = 0;
	}

	ret = malloc(sizeof(*ret));
	if (ret == NULL)
		return DOM_NO_MEM_ERR;

	if (lwc_intern_string((const char *) ptr, len, 
			&ret->data.intern) != lwc_error_ok) {
		free(ret);
		return DOM_NO_MEM_ERR;
	}

	ret->base.refcnt = 1;

	ret->type = DOM_STRING_INTERNED;

	*str = (dom_string *)ret;

	return DOM_NO_ERR;
}

/**
 * Make the dom_string be interned
 *
 * \param str     The dom_string to be interned
 * \param lwcstr  The result lwc_string	
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_string_intern(dom_string *str, 
		struct lwc_string_s **lwcstr)
{
	dom_string_internal *istr = (dom_string_internal *) str;
	/* If this string is already interned, do nothing */
	if (istr->type != DOM_STRING_INTERNED) {
		lwc_string *ret;
		lwc_error lerr;

		lerr = lwc_intern_string((const char *) istr->data.cdata.ptr, 
				istr->data.cdata.len, &ret);
		if (lerr != lwc_error_ok) {
			return _dom_exception_from_lwc_error(lerr);
		}

		free(istr->data.cdata.ptr);

		istr->data.intern = ret;

		istr->type = DOM_STRING_INTERNED;
	}

	*lwcstr = lwc_string_ref(istr->data.intern);

	return DOM_NO_ERR;
}

/**
 * Case sensitively compare two DOM strings
 *
 * \param s1  The first string to compare
 * \param s2  The second string to compare
 * \return true if strings match, false otherwise
 */
bool dom_string_isequal(const dom_string *s1, const dom_string *s2)
{
	size_t len;
	const dom_string_internal *is1 = (dom_string_internal *) s1;
	const dom_string_internal *is2 = (dom_string_internal *) s2;

	if (s1 == NULL)
		is1 = &empty_string;

	if (s2 == NULL)
		is2 = &empty_string;

	if (is1->type == DOM_STRING_INTERNED && 
			is2->type == DOM_STRING_INTERNED) {
		bool match;

		(void) lwc_string_isequal(is1->data.intern, is2->data.intern,
			&match);

		return match;
	}

	len = dom_string_byte_length((dom_string *) is1);

	if (len != dom_string_byte_length((dom_string *)is2))
		return false;

	return 0 == memcmp(dom_string_data((dom_string *) is1), dom_string_data((dom_string *)is2), len);
}

/**
 * Trivial locale-agnostic lower case convertor
 */
static inline uint8_t dolower(const uint8_t c)
{
	if ('A' <= c && c <= 'Z')
		return c + 'a' - 'A';
	return c;
}

/**
 * Case insensitively compare two DOM strings
 *
 * \param s1  The first string to compare
 * \param s2  The second string to compare
 * \return true if strings match, false otherwise
 */
bool dom_string_caseless_isequal(const dom_string *s1, const dom_string *s2)
{
	const uint8_t *d1 = NULL;
	const uint8_t *d2 = NULL;
	size_t len;
	const dom_string_internal *is1 = (dom_string_internal *) s1;
	const dom_string_internal *is2 = (dom_string_internal *) s2;

	if (s1 == NULL)
		is1 = &empty_string;

	if (s2 == NULL)
		is2 = &empty_string;

	if (is1->type == DOM_STRING_INTERNED && 
			is2->type == DOM_STRING_INTERNED) {
		bool match;

		if (lwc_string_caseless_isequal(is1->data.intern,
				is2->data.intern, &match) != lwc_error_ok)
			return false;

		return match;
	}

	len = dom_string_byte_length((dom_string *) is1);

	if (len != dom_string_byte_length((dom_string *)is2))
		return false;

	d1 = (const uint8_t *) dom_string_data((dom_string *) is1);
	d2 = (const uint8_t *) dom_string_data((dom_string *)is2);

	while (len > 0) {
		if (dolower(*d1) != dolower(*d2))
			return false;

		d1++;
		d2++;
		len--;
	}

	return true;
}


/**
 * Case sensitively compare DOM string with lwc_string
 *
 * \param s1  The first string to compare
 * \param s2  The second string to compare
 * \return true if strings match, false otherwise
 *
 * Returns false if either are NULL.
 */
bool dom_string_lwc_isequal(const dom_string *s1, lwc_string *s2)
{
	size_t len;
	dom_string_internal *is1 = (dom_string_internal *) s1;

	if (s1 == NULL || s2 == NULL)
		return false;

	if (is1->type == DOM_STRING_INTERNED) {
		bool match;

		(void) lwc_string_isequal(is1->data.intern, s2, &match);

		return match;
	}

	/* Handle non-interned case */
	len = dom_string_byte_length(s1);

	if (len != lwc_string_length(s2))
		return false;

	return 0 == memcmp(dom_string_data(s1), lwc_string_data(s2), len);
}


/**
 * Case insensitively compare DOM string with lwc_string
 *
 * \param s1  The first string to compare
 * \param s2  The second string to compare
 * \return true if strings match, false otherwise
 *
 * Returns false if either are NULL.
 */
bool dom_string_caseless_lwc_isequal(const dom_string *s1, lwc_string *s2)
{
	size_t len;
	const uint8_t *d1 = NULL;
	const uint8_t *d2 = NULL;
	dom_string_internal *is1 = (dom_string_internal *) s1;

	if (s1 == NULL || s2 == NULL)
		return false;

	if (is1->type == DOM_STRING_INTERNED) {
		bool match;

		if (lwc_string_caseless_isequal(is1->data.intern, s2, &match) != lwc_error_ok)
			return false;

		return match;
	}

	len = dom_string_byte_length(s1);

	if (len != lwc_string_length(s2))
		return false;

	d1 = (const uint8_t *) dom_string_data(s1);
	d2 = (const uint8_t *) lwc_string_data(s2);

	while (len > 0) {
		if (dolower(*d1) != dolower(*d2))
			return false;

		d1++;
		d2++;
		len--;
	}

	return true;
}


/**
 * Get the index of the first occurrence of a character in a dom string 
 * 
 * \param str  The string to search in
 * \param chr  UCS4 value to look for
 * \return Character index of found character, or -1 if none found 
 */
uint32_t dom_string_index(dom_string *str, uint32_t chr)
{
	const uint8_t *s;
	size_t clen, slen;
	uint32_t c, index;
	parserutils_error err;

	s = (const uint8_t *) dom_string_data(str);
	slen = dom_string_byte_length(str);

	index = 0;

	while (slen > 0) {
		err = parserutils_charset_utf8_to_ucs4(s, slen, &c, &clen);
		if (err != PARSERUTILS_OK) {
			return (uint32_t) -1;
		}

		if (c == chr) {
			return index;
		}

		s += clen;
		slen -= clen;
		index++;
	}

	return (uint32_t) -1;
}

/**
 * Get the index of the last occurrence of a character in a dom string 
 * 
 * \param str  The string to search in
 * \param chr  UCS4 value to look for
 * \return Character index of found character, or -1 if none found
 */
uint32_t dom_string_rindex(dom_string *str, uint32_t chr)
{
	const uint8_t *s;
	size_t clen = 0, slen;
	uint32_t c, coff, index;
	parserutils_error err;

	s = (const uint8_t *) dom_string_data(str);
	slen = dom_string_byte_length(str);

	index = dom_string_length(str);

	while (slen > 0) {
		err = parserutils_charset_utf8_prev(s, slen, 
				(uint32_t *) &coff);
		if (err == PARSERUTILS_OK) {
			err = parserutils_charset_utf8_to_ucs4(s + coff, 
					slen - clen, &c, &clen);
		}

		if (err != PARSERUTILS_OK) {
			return (uint32_t) -1;
		}

		if (c == chr) {
			return index;
		}

		slen -= clen;
		index--;
	}

	return (uint32_t) -1;
}

/**
 * Get the length, in characters, of a dom string
 *
 * \param str  The string to measure the length of
 * \return The length of the string, in characters
 */
uint32_t dom_string_length(dom_string *str)
{
	const uint8_t *s;
	size_t slen, clen;
	parserutils_error err;

	s = (const uint8_t *) dom_string_data(str);
	slen = dom_string_byte_length(str);

	err = parserutils_charset_utf8_length(s, slen, &clen);
	if (err != PARSERUTILS_OK) {
		return 0;
	}

	return clen;
}

/**
 * Get the UCS4 character at position index
 *
 * \param index  The position of the charater
 * \param ch     The UCS4 character
 * \return DOM_NO_ERR on success, appropriate dom_exception on failure.
 */
dom_exception dom_string_at(dom_string *str, uint32_t index, 
		uint32_t *ch)
{
	const uint8_t *s;
	size_t clen, slen;
	uint32_t c, i;
	parserutils_error err;

	s = (const uint8_t *) dom_string_data(str);
	slen = dom_string_byte_length(str);

	i = 0;

	while (slen > 0) {
		err = parserutils_charset_utf8_char_byte_length(s, &clen);
		if (err != PARSERUTILS_OK) {
			return (uint32_t) -1;
		}

		i++;
		if (i == index + 1)
			break;

		s += clen;
		slen -= clen;
	}

	if (i == index + 1) {
		err = parserutils_charset_utf8_to_ucs4(s, slen, &c, &clen);
		if (err != PARSERUTILS_OK) {
			return (uint32_t) -1;
		}

		*ch = c;
		return DOM_NO_ERR;
	} else {
		return DOM_DOMSTRING_SIZE_ERR;
	}
}

/** 
 * Concatenate two dom strings 
 * 
 * \param s1      The first string
 * \param s2      The second string
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * The returned string will be referenced. The client
 * should dereference it once it has finished with it.
 */
dom_exception dom_string_concat(dom_string *s1, dom_string *s2,
		dom_string **result)
{
	dom_string_internal *concat;
	const uint8_t *s1ptr, *s2ptr;
	size_t s1len, s2len;

	assert(s1 != NULL);
	assert(s2 != NULL);

	s1ptr = (const uint8_t *) dom_string_data(s1);
	s2ptr = (const uint8_t *) dom_string_data(s2);
	s1len = dom_string_byte_length(s1);
	s2len = dom_string_byte_length(s2);

	concat = malloc(sizeof(*concat));
	if (concat == NULL) {
		return DOM_NO_MEM_ERR;
	}

	concat->data.cdata.ptr = malloc(s1len + s2len + 1);
	if (concat->data.cdata.ptr == NULL) {
		free(concat);

		return DOM_NO_MEM_ERR;
	}

	memcpy(concat->data.cdata.ptr, s1ptr, s1len);

	memcpy(concat->data.cdata.ptr + s1len, s2ptr, s2len);

	concat->data.cdata.ptr[s1len + s2len] = '\0';

	concat->data.cdata.len = s1len + s2len;

	concat->base.refcnt = 1;

	concat->type = DOM_STRING_CDATA;

	*result = (dom_string *)concat;

	return DOM_NO_ERR;
}

/**
 * Extract a substring from a dom string 
 *
 * \param str     The string to extract from
 * \param i1      The character index of the start of the substring
 * \param i2      The character index of the end of the substring
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion
 *
 * The returned string will have its reference count increased. The client
 * should dereference it once it has finished with it.
 */
dom_exception dom_string_substr(dom_string *str, 
		uint32_t i1, uint32_t i2, dom_string **result)
{
	const uint8_t *s;
	size_t slen;
	uint32_t b1, b2;
	parserutils_error err;

	/* target string is NULL equivalent to empty. */
	if (str == NULL)
		str = (dom_string *)&empty_string;

	s = (const uint8_t *) dom_string_data(str);
	slen = dom_string_byte_length(str);

	/* Initialise the byte index of the start to 0 */
	b1 = 0;
	/* Make the end a character offset from the start */
	i2 -= i1;

	/* Calculate the byte index of the start */
	while (i1 > 0) {
		err = parserutils_charset_utf8_next(s, slen, b1, &b1);
		if (err != PARSERUTILS_OK) {
			return DOM_NO_MEM_ERR;
		}

		i1--;
	}

	/* Initialise the byte index of the end to that of the start */
	b2 = b1;

	/* Calculate the byte index of the end */
	while (i2 > 0) {
		err = parserutils_charset_utf8_next(s, slen, b2, &b2);
		if (err != PARSERUTILS_OK) {
			return DOM_NO_MEM_ERR;
		}

		i2--;
	}

	/* Create a string from the specified byte range */
	return dom_string_create(s + b1, b2 - b1, result);
}

/**
 * Insert data into a dom string at the given location
 *
 * \param target  Pointer to string to insert into
 * \param source  Pointer to string to insert
 * \param offset  Character offset of location to insert at
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR          on success, 
 *         DOM_NO_MEM_ERR      on memory exhaustion,
 *         DOM_INDEX_SIZE_ERR  if ::offset > len(::target).
 *
 * The returned string will have its reference count increased. The client
 * should dereference it once it has finished with it. 
 */
dom_exception dom_string_insert(dom_string *target,
		dom_string *source, uint32_t offset,
		dom_string **result)
{
	dom_string_internal *res;
	const uint8_t *t, *s;
	uint32_t tlen, slen, clen;
	uint32_t ins = 0;
	parserutils_error err;

	/* target string is NULL equivalent to empty. */
	if (target == NULL)
		target = (dom_string *)&empty_string;

	t = (const uint8_t *) dom_string_data(target);
	tlen = dom_string_byte_length(target);
	s = (const uint8_t *) dom_string_data(source);
	slen = dom_string_byte_length(source);

	clen = dom_string_length(target);

	if (offset > clen)
		return DOM_INDEX_SIZE_ERR;

	/* Calculate the byte index of the insertion point */
	if (offset == clen) {
		/* Optimisation for append */
		ins = tlen;
	} else {
		while (offset > 0) {
			err = parserutils_charset_utf8_next(t, tlen, 
					ins, &ins);

			if (err != PARSERUTILS_OK) {
				return DOM_NO_MEM_ERR;
			}

			offset--;
		}
	}

	/* Allocate result string */
	res = malloc(sizeof(*res));
	if (res == NULL) {
		return DOM_NO_MEM_ERR;
	}

	/* Allocate data buffer for result contents */
	res->data.cdata.ptr = malloc(tlen + slen + 1);
	if (res->data.cdata.ptr == NULL) {
		free(res);
		return DOM_NO_MEM_ERR;
	}

	/* Copy initial portion of target, if any, into result */
	if (ins > 0) {
		memcpy(res->data.cdata.ptr, t, ins);
	}

	/* Copy inserted data into result */
	memcpy(res->data.cdata.ptr + ins, s, slen);

	/* Copy remainder of target, if any, into result */
	if (tlen - ins > 0) {
		memcpy(res->data.cdata.ptr + ins + slen, t + ins, tlen - ins);
	}

	res->data.cdata.ptr[tlen + slen] = '\0';

	res->data.cdata.len = tlen + slen;

	res->base.refcnt = 1;

	res->type = DOM_STRING_CDATA;

	*result = (dom_string *)res;

	return DOM_NO_ERR;
}

/** 
 * Replace a section of a dom string
 *
 * \param target  Pointer to string of which to replace a section
 * \param source  Pointer to replacement string
 * \param i1      Character index of start of region to replace
 * \param i2      Character index of end of region to replace
 * \param result  Pointer to location to receive result
 * \return DOM_NO_ERR on success, DOM_NO_MEM_ERR on memory exhaustion.
 *
 * The returned string will have its reference count increased. The client
 * should dereference it once it has finished with it. 
 */
dom_exception dom_string_replace(dom_string *target,
		dom_string *source, uint32_t i1, uint32_t i2,
		dom_string **result)
{
	dom_string_internal *res;
	const uint8_t *t, *s;
	uint32_t tlen, slen;
	uint32_t b1, b2;
	parserutils_error err;

	/* target string is NULL equivalent to empty. */
	if (target == NULL)
		target = (dom_string *)&empty_string;

	t = (const uint8_t *) dom_string_data(target);
	tlen = dom_string_byte_length(target);
	s = (const uint8_t *) dom_string_data(source);
	slen = dom_string_byte_length(source);

	/* Initialise the byte index of the start to 0 */
	b1 = 0;
	/* Make the end a character offset from the start */
	i2 -= i1;

	/* Calculate the byte index of the start */
	while (i1 > 0) {
		err = parserutils_charset_utf8_next(t, tlen, b1, &b1);

		if (err != PARSERUTILS_OK) {
			return DOM_NO_MEM_ERR;
		}

		i1--;
	}

	/* Initialise the byte index of the end to that of the start */
	b2 = b1;

	/* Calculate the byte index of the end */
	while (i2 > 0) {
		err = parserutils_charset_utf8_next(t, tlen, b2, &b2);

		if (err != PARSERUTILS_OK) {
			return DOM_NO_MEM_ERR;
		}

		i2--;
	}

	/* Allocate result string */
	res = malloc(sizeof(*res));
	if (res == NULL) {
		return DOM_NO_MEM_ERR;
	}

	/* Allocate data buffer for result contents */
	res->data.cdata.ptr = malloc(tlen + slen - (b2 - b1) + 1);
	if (res->data.cdata.ptr == NULL) {
		free(res);
		return DOM_NO_MEM_ERR;
	}

	/* Copy initial portion of target, if any, into result */
	if (b1 > 0) {
		memcpy(res->data.cdata.ptr, t, b1);
	}

	/* Copy replacement data into result */
	if (slen > 0) {
		memcpy(res->data.cdata.ptr + b1, s, slen);
	}

	/* Copy remainder of target, if any, into result */
	if (tlen - b2 > 0) {
		memcpy(res->data.cdata.ptr + b1 + slen, t + b2, tlen - b2);
	}

	res->data.cdata.ptr[tlen + slen - (b2 - b1)] = '\0';

	res->data.cdata.len = tlen + slen - (b2 - b1);

	res->base.refcnt = 1;

	res->type = DOM_STRING_CDATA;

	*result = (dom_string *)res;

	return DOM_NO_ERR;
}

/**
 * Calculate a hash value from a dom string 
 *
 * \param str  The string to calculate a hash of
 * \return The hash value associated with the string
 */
uint32_t dom_string_hash(dom_string *str)
{
	const uint8_t *s = (const uint8_t *) dom_string_data(str);
	size_t slen = dom_string_byte_length(str);
	uint32_t hash = 0x811c9dc5;

	while (slen > 0) {
		hash *= 0x01000193;
		hash ^= *s;

		s++;
		slen--;
	}

	return hash;
}

/**
 * Convert a lwc_error to a dom_exception
 * 
 * \param err  The input lwc_error
 * \return the dom_exception
 */
dom_exception _dom_exception_from_lwc_error(lwc_error err)
{
	switch (err) {
	case lwc_error_ok:
		return DOM_NO_ERR;
	case lwc_error_oom:
		return DOM_NO_MEM_ERR;
	case lwc_error_range:
		return DOM_INDEX_SIZE_ERR;
	}

	return DOM_NO_ERR;
}

/**
 * Get the raw character data of the dom_string.
 *
 * \param str	The dom_string object
 * \return      The C string pointer
 *
 * @note: This function is just provided for the convenience of accessing the 
 * raw C string character, no change on the result string is allowed.
 */
const char *dom_string_data(const dom_string *str)
{
	dom_string_internal *istr = (dom_string_internal *) str;
	if (istr->type == DOM_STRING_CDATA) {
		return (const char *) istr->data.cdata.ptr;
	} else {
		return lwc_string_data(istr->data.intern);
	}
}

/** Get the byte length of this dom_string 
 *
 * \param str	The dom_string object
 */
size_t dom_string_byte_length(const dom_string *str)
{
	dom_string_internal *istr = (dom_string_internal *) str;
	if (istr->type == DOM_STRING_CDATA) {
		return istr->data.cdata.len;
	} else {
		return lwc_string_length(istr->data.intern);
	}
}

/** Convert the given string to uppercase
 *
 * \param source 
 * \param ascii_only  Whether to only convert [a-z] to [A-Z]
 * \param upper       Result pointer for uppercase string.  Caller owns ref
 *
 * \return DOM_NO_ERR on success.
 *
 * \note Right now, will return DOM_NOT_SUPPORTED_ERR if ascii_only is false.
 */
dom_exception
dom_string_toupper(dom_string *source, bool ascii_only, dom_string **upper)
{
	const uint8_t *orig_s = (const uint8_t *) dom_string_data(source);
	const size_t nbytes = dom_string_byte_length(source);
	uint8_t *copy_s;
	size_t index = 0, clen;
	parserutils_error err;
	dom_exception exc;
	
	if (ascii_only == false)
		return DOM_NOT_SUPPORTED_ERR;
	
	copy_s = malloc(nbytes);
	if (copy_s == NULL)
		return DOM_NO_MEM_ERR;
	memcpy(copy_s, orig_s, nbytes);
	
	while (index < nbytes) {
		err = parserutils_charset_utf8_char_byte_length(orig_s + index,
								&clen);
		if (err != PARSERUTILS_OK) {
			free(copy_s);
			/** \todo Find a better exception */
			return DOM_NO_MEM_ERR;
		}
		
		if (clen == 1) {
			if (orig_s[index] >= 'a' &&
			    orig_s[index] <= 'z')
				copy_s[index] -= 'a' - 'A';
		}
		
		index += clen;
	}
	
	if (((dom_string_internal*)source)->type == DOM_STRING_CDATA) {
		exc = dom_string_create(copy_s, nbytes, upper);
	} else {
		exc = dom_string_create_interned(copy_s, nbytes, upper);
	}
	
	free(copy_s);
	
	return exc;
}

/** Convert the given string to lowercase
 *
 * \param source 
 * \param ascii_only  Whether to only convert [a-z] to [A-Z]
 * \param lower       Result pointer for lowercase string.  Caller owns ref
 *
 * \return DOM_NO_ERR on success.
 *
 * \note Right now, will return DOM_NOT_SUPPORTED_ERR if ascii_only is false.
 */
dom_exception
dom_string_tolower(dom_string *source, bool ascii_only, dom_string **lower)
{
	const uint8_t *orig_s = (const uint8_t *) dom_string_data(source);
	const size_t nbytes = dom_string_byte_length(source);
	uint8_t *copy_s;
	size_t index = 0, clen;
	parserutils_error err;
	dom_exception exc;
	
	if (ascii_only == false)
		return DOM_NOT_SUPPORTED_ERR;
	
	copy_s = malloc(nbytes);
	if (copy_s == NULL)
		return DOM_NO_MEM_ERR;
	memcpy(copy_s, orig_s, nbytes);
	
	while (index < nbytes) {
		err = parserutils_charset_utf8_char_byte_length(orig_s + index,
								&clen);
		if (err != PARSERUTILS_OK) {
			free(copy_s);
			/** \todo Find a better exception */
			return DOM_NO_MEM_ERR;
		}
		
		if (clen == 1) {
			if (orig_s[index] >= 'A' &&
			    orig_s[index] <= 'Z')
				copy_s[index] += 'a' - 'A';
		}
		
		index += clen;
	}
	
	if (((dom_string_internal*)source)->type == DOM_STRING_CDATA) {
		exc = dom_string_create(copy_s, nbytes, lower);
	} else {
		exc = dom_string_create_interned(copy_s, nbytes, lower);
	}
	
	free(copy_s);
	
	return exc;
}

/* exported function documented in string.h */
dom_exception dom_string_whitespace_op(dom_string *s,
		enum dom_whitespace_op op, dom_string **ret)
{
	const uint8_t *src_text = (const uint8_t *) dom_string_data(s);
	size_t len = dom_string_byte_length(s);
	const uint8_t *src_pos;
	const uint8_t *src_end;
	dom_exception exc;
	uint8_t *temp_pos;
	uint8_t *temp;

	if (len == 0) {
		*ret = dom_string_ref(s);
	}

	temp = malloc(len);
	if (temp == NULL) {
		return DOM_NO_MEM_ERR;
	}

	src_pos = src_text;
	src_end = src_text + len;
	temp_pos = temp;

	if (op & DOM_WHITESPACE_STRIP_LEADING) {
		while (src_pos < src_end) {
			if (*src_pos == ' '  || *src_pos == '\t' ||
			    *src_pos == '\n' || *src_pos == '\r' ||
			    *src_pos == '\f')
				src_pos++;
			else
				break;
		}
	}

	while (src_pos < src_end) {
		if ((op & DOM_WHITESPACE_COLLAPSE) &&
				(*src_pos == ' ' || *src_pos == '\t' ||
				*src_pos == '\n' || *src_pos == '\r' ||
				*src_pos == '\f')) {
			/* Got a whitespace character */
			do {
				/* Skip all adjacent whitespace */
				src_pos++;
			} while (src_pos < src_end &&
					(*src_pos == ' ' || *src_pos == '\t' ||
					*src_pos == '\n' || *src_pos == '\r' ||
					*src_pos == '\f'));
			/* Gets replaced with single space in output */
			*temp_pos++ = ' ';
		} else {
			/* Otherwise, copy to output */
			*temp_pos++ = *src_pos++;
		}
	}

	if (op & DOM_WHITESPACE_STRIP_TRAILING) {
		while (temp_pos > temp) {
			temp_pos--;
			if (*temp_pos != ' '  && *temp_pos != '\t' &&
			    *temp_pos != '\n' && *temp_pos != '\r' &&
			    *temp_pos != '\f') {
				temp_pos++;
				break;
			}
		}
	}

	/* New length */
	len = temp_pos - temp;

	/* Make new string */
	if (((dom_string_internal *) s)->type == DOM_STRING_CDATA) {
		exc = dom_string_create(temp, len, ret);
	} else {
		exc = dom_string_create_interned(temp, len, ret);
	}

	free(temp);

	return exc;
}

