/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell
 */

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include "utils/string.h"


/**
 * Check that one string is exactly equal to another
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
bool hubbub_string_match(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len)
{
	if (a_len != b_len)
		return false;

	return memcmp((const char *) a, (const char *) b, b_len) == 0;
}

/**
 * Check that one string is case-insensitively equal to another
 *
 * \param a	String to compare
 * \param a_len	Length of first string
 * \param b	String to compare
 * \param b_len	Length of second string
 */
bool hubbub_string_match_ci(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len)
{
	if (a_len != b_len)
		return false;

	while (b_len-- > 0) {
		uint8_t aa = *(a++);
		uint8_t bb = *(b++);

		aa = ('a' <= aa && aa <= 'z') ? (aa - 0x20) : aa; 
		bb = ('a' <= bb && bb <= 'z') ? (bb - 0x20) : bb;

		if (aa != bb)
			return false;
	}

	return true;
}
