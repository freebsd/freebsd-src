/* popcount.c

Copyright (C) 1994, 1996 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU Library General Public License as published by
the Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
License for more details.

You should have received a copy of the GNU Library General Public License
along with the GNU MP Library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
MA 02111-1307, USA. */

#include "gmp.h"
#include "gmp-impl.h"

#if defined __GNUC__
#if defined __sparc_v9__ && BITS_PER_MP_LIMB == 64
#define popc_limb(a) \
  ({									\
    DItype __res;							\
    asm ("popc %1,%0" : "=r" (__res) : "rI" (a));			\
    __res;								\
  })
#endif
#endif

#ifndef popc_limb

/* Cool population count of a mp_limb_t.
   You have to figure out how this works, I won't tell you!  */

static inline unsigned int
popc_limb (x)
     mp_limb_t x;
{
#if BITS_PER_MP_LIMB == 64
  /* We have to go into some trouble to define these constants.
     (For mp_limb_t being `long long'.)  */
  mp_limb_t cnst;
  cnst = 0x55555555L | ((mp_limb_t) 0x55555555L << BITS_PER_MP_LIMB/2);
  x = ((x & ~cnst) >> 1) + (x & cnst);
  cnst = 0x33333333L | ((mp_limb_t) 0x33333333L << BITS_PER_MP_LIMB/2);
  x = ((x & ~cnst) >> 2) + (x & cnst);
  cnst = 0x0f0f0f0fL | ((mp_limb_t) 0x0f0f0f0fL << BITS_PER_MP_LIMB/2);
  x = ((x >> 4) + x) & cnst;
  x = ((x >> 8) + x);
  x = ((x >> 16) + x);
  x = ((x >> 32) + x) & 0xff;
#endif
#if BITS_PER_MP_LIMB == 32
  x = ((x >> 1) & 0x55555555L) + (x & 0x55555555L);
  x = ((x >> 2) & 0x33333333L) + (x & 0x33333333L);
  x = ((x >> 4) + x) & 0x0f0f0f0fL;
  x = ((x >> 8) + x);
  x = ((x >> 16) + x) & 0xff;
#endif
  return x;
}
#endif

unsigned long int
#if __STDC__
mpn_popcount (register mp_srcptr p, register mp_size_t size)
#else
mpn_popcount (p, size)
     register mp_srcptr p;
     register mp_size_t size;
#endif
{
  unsigned long int popcnt;
  mp_size_t i;

  popcnt = 0;
  for (i = 0; i < size; i++)
    popcnt += popc_limb (p[i]);

  return popcnt;
}
