/* bignum_copy.c - copy a bignum
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "as.h"

/*
 *			bignum_copy ()
 *
 * Copy a bignum from in to out.
 * If the output is shorter than the input, copy lower-order littlenums.
 * Return 0 or the number of significant littlenums dropped.
 * Assumes littlenum arrays are densely packed: no unused chars between
 * the littlenums. Uses memcpy() to move littlenums, and wants to
 * know length (in chars) of the input bignum.
 */

/* void */
int
    bignum_copy(in, in_length, out, out_length)
register LITTLENUM_TYPE *in;
register int in_length; /* in sizeof(littlenum)s */
register LITTLENUM_TYPE *out;
register int out_length; /* in sizeof(littlenum)s */
{
	int significant_littlenums_dropped;

	if (out_length < in_length) {
		LITTLENUM_TYPE *p; /* -> most significant (non-zero) input
				      littlenum. */

		memcpy((void *) out, (void *) in,
		      out_length << LITTLENUM_SHIFT);
		for (p = in + in_length - 1; p >= in; --p) {
			if (* p) break;
		}
		significant_littlenums_dropped = p - in - in_length + 1;

		if (significant_littlenums_dropped < 0) {
			significant_littlenums_dropped = 0;
		}
	} else {
		memcpy((char *) out, (char *) in,
		      in_length << LITTLENUM_SHIFT);

		if (out_length > in_length) {
			memset((char *) (out + out_length),
			      '\0', (out_length - in_length) << LITTLENUM_SHIFT);
		}

		significant_littlenums_dropped = 0;
	}

	return(significant_littlenums_dropped);
} /* bignum_copy() */

/* end of bignum-copy.c */
