/*
 * This file is part of Hubbub.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2008 Andrew Sidwell
 */

#ifndef hubbub_string_h_
#define hubbub_string_h_

/** Match two strings case-sensitively */
bool hubbub_string_match(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len);

/** Match two strings case-insensitively */
bool hubbub_string_match_ci(const uint8_t *a, size_t a_len,
		const uint8_t *b, size_t b_len);

#endif
