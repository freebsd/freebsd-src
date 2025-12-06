/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#include "rand48.h"

unsigned short *
seed48(unsigned short xseed[3])
{
	static unsigned short sseed[3];

	STORERAND48(_rand48_seed, sseed);
	LOADRAND48(_rand48_seed, xseed);
	_rand48_mult = RAND48_MULT;
	_rand48_add = RAND48_ADD;
	return (sseed);
}
