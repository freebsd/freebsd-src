/* hex_value.c - char=>radix-value -
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/*
 * Export: Hex_value[]. Converts digits to their radix-values.
 *	As distributed assumes 8 bits per char (256 entries) and ASCII.
 */

#define __ (42)			/* blatently illegal digit value */
				/* exceeds any normal radix */
#if !defined(__STDC__) && !defined(const)
#define const /* empty */
#endif
const char
hex_value [256] = {		/* for fast ASCII -> binary */
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
   0,  1,  2,  3,  4,  5,  6,  7,  8,  9, __, __, __, __, __, __,
  __, 10, 11, 12, 13, 14, 15, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, 10, 11, 12, 13, 14, 15, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,
  __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __
  };

#ifdef VMS
dummy2()
{
}
#endif
/* end:hex_value.c */
