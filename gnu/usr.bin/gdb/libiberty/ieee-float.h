/* IEEE floating point support declarations, for GDB, the GNU Debugger.
   Copyright (C) 1991 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if !defined (IEEE_FLOAT_H)
#define IEEE_FLOAT_H 1

#include "ansidecl.h"

/* Parameters for extended float format:  */

struct ext_format {
  unsigned totalsize;		/* Total size of extended number */
  unsigned signbyte;		/* Byte number of sign bit */
  unsigned char signmask;	/* Mask for sign bit */
  unsigned expbyte_h;		/* High byte of exponent */
  unsigned expbyte_l;		/* Low  byte of exponent */
  unsigned manbyte_h;		/* High byte of mantissa */
  unsigned manbyte_l;		/* Low  byte of mantissa */
};

#define	TOTALSIZE	ext_format->totalsize
#define	SIGNBYTE	ext_format->signbyte
#define	SIGNMASK	ext_format->signmask
#define EXPBYTE_H	ext_format->expbyte_h
#define EXPBYTE_L	ext_format->expbyte_l
#define	MANBYTE_H	ext_format->manbyte_h
#define	MANBYTE_L	ext_format->manbyte_l

/* Actual ext_format structs for various machines are in the *-tdep.c file
   for each machine.  */

#define	EXT_EXP_NAN	0x7FFF	/* Exponent value that indicates NaN */
#define	EXT_EXP_BIAS	0x3FFF	/* Amount added to "true" exponent for ext */
#define	DBL_EXP_BIAS	 0x3FF	/* Ditto, for doubles */

/* Convert an IEEE extended float to a double.
   FROM is the address of the extended float.
   Store the double in *TO.  */

extern void
ieee_extended_to_double PARAMS ((const struct ext_format *, char *, double *));

/* The converse: convert the double *FROM to an extended float
   and store where TO points.  */

extern void
double_to_ieee_extended PARAMS ((const struct ext_format *, double *, char *));

#endif	/* defined (IEEE_FLOAT_H) */
