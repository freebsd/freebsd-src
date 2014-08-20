/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#include "rufl_internal.h"


/**
 * Test if a character set contains a character.
 *
 * \param  charset  character set
 * \param  c        character code
 * \return  true if present, false if absent
 */

bool rufl_character_set_test(struct rufl_character_set *charset,
		unsigned int c)
{
	unsigned int block = c >> 8;
	unsigned int byte = (c >> 3) & 31;
	unsigned int bit = c & 7;

	if (256 <= block)
		return false;

	if (charset->index[block] == BLOCK_EMPTY)
		return false;
	else if (charset->index[block] == BLOCK_FULL)
		return true;
	else {
		unsigned char z = charset->block[charset->index[block]][byte];
		return z & (1 << bit);
	}
}
