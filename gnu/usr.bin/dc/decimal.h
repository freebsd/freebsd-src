/* 
 * Header file for decimal.c (arbitrary precision decimal arithmetic)
 *
 * Copyright (C) 1984 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can either send email to this
 * program's author (see below) or write to: The Free Software Foundation,
 * Inc.; 675 Mass Ave. Cambridge, MA 02139, USA.
 */

/* Autoconf stuff */
#ifndef HAVE_BCOPY
#undef bcopy
#define bcopy(s2, s1, n) memcpy (s1, s2, n)
#endif

#ifndef HAVE_BZERO
#undef bzero
#define bzero(b, l) memset (b, 0, l)
#endif

/* Define the radix to use by default, and for representing the
   numbers internally.  This does not need to be decimal; that is just
   the default for it.  */

/* Currently, this is required to be even for this program to work. */

#ifndef RADIX
#define RADIX 10
#endif

/* The user must define the external function `decimal_error'
   which is called with two arguments to report errors in this package.
   The two arguments may be passed to `printf' to print a message. */

/* Structure that represents a decimal number */

struct decimal
{
  unsigned int sign: 1;		/* One for negative number */
				/* The sign should always be zero for the number 0 */
  int after: 15;		/* number of fraction digits */
  unsigned short before;	/* number of non-fraction digits */
  unsigned short refcnt;	/* number of pointers to this number */
				/* (used by calling program) */
  char contents[1];		/* the digits themselves, least significant first. */
				/* digits are just numbers 0 .. RADIX-1 */
};

/* There may never be leading nonfraction zeros or trailing fraction
   zeros in a number.  They must be removed by all the arithmetic
   functions.  Therefore, the number zero always has no digits stored. */

typedef struct decimal *decimal;

/* Decimal numbers are always passed around as pointers.
   All the external entries in this file allocate new numbers
   using `malloc' to store values in.
   They never modify their arguments or any existing numbers. */

/* Return the total number of digits stored in the number `b' */
#define LENGTH(b) ((b)->before + (b)->after)

/* Some constant decimal numbers */


#define DECIMAL_ZERO &decimal_zero


#define DECIMAL_ONE &decimal_one

#define DECIMAL_HALF &decimal_half

decimal decimal_add (), decimal_sub (), decimal_mul (), decimal_div ();
decimal decimal_mul_dc (), decimal_mul_rounded (), decimal_rem ();
decimal decimal_round_digits ();
decimal make_decimal (), decimal_copy (), decimal_parse ();
decimal decimal_sqrt (), decimal_expt ();

void decimal_print ();

/* End of decimal.h */
