/*
 * This file is part of RUfl
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license
 * Copyright 2005 James Bursa <james@semichrome.net>
 */

#include "oslib/font.h"
#include "rufl_internal.h"


/**
 * Clear the internal font handle cache.
 *
 * Call this function on mode changes or output redirection changes.
 */

void rufl_invalidate_cache(void)
{
	unsigned int i;

	for (i = 0; i != rufl_CACHE_SIZE; i++) {
		if (rufl_cache[i].font != rufl_CACHE_NONE) {
			xfont_lose_font(rufl_cache[i].f);
			rufl_cache[i].font = rufl_CACHE_NONE;
		}
        }
}
