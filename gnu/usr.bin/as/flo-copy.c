/* flonum_copy.c - copy a flonum
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
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef lint
static char rcsid[] = "$FreeBSD$";
#endif

#include "as.h"

void
    flonum_copy(in, out)
FLONUM_TYPE *in;
FLONUM_TYPE *out;
{
	int in_length;	/* 0 origin */
	int out_length;	/* 0 origin */

	out->sign = in->sign;
	in_length = in->leader - in->low;

	if (in_length < 0) {
		out->leader = out->low - 1; /* 0.0 case */
	} else {
		out_length = out->high - out->low;
		/*
		 * Assume no GAPS in packing of littlenums.
		 * I.e. sizeof(array) == sizeof(element) * number_of_elements.
		 */
		if (in_length <= out_length) {
			{
				/*
				 * For defensive programming, zero any high-order littlenums we don't need.
				 * This is destroying evidence and wasting time, so why bother???
				 */
				if (in_length < out_length) {
memset((char *)(out->low + in_length + 1), '\0', out_length - in_length);
				}
			}
			memcpy((void *)(out->low), (void *)(in->low), (int)((in_length + 1) * sizeof(LITTLENUM_TYPE)));
			out->exponent = in->exponent;
			out->leader   = in->leader - in->low + out->low;
		} else {
			int	shorten;		/* 1-origin. Number of littlenums we drop. */

			shorten = in_length - out_length;
			/* Assume out_length >= 0 ! */
			memcpy((void *)( out->low), (void *)(in->low + shorten), (int)((out_length + 1) * sizeof(LITTLENUM_TYPE)));
			out->leader = out->high;
			out->exponent = in->exponent + shorten;
		}
	} /* if any significant bits */
} /* flonum_copy() */

/* end of flonum_copy.c */
