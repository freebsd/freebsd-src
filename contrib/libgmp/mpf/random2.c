/* mpf_random2 -- Generate a positive random mpf_t of specified size, with
   long runs of consecutive ones and zeros in the binary representation.
   Intended for testing of other MP routines.

Copyright (C) 1995, 1996 Free Software Foundation, Inc.

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

#if defined (__hpux) || defined (alpha__)  || defined (__svr4__) || defined (__SVR4)
/* HPUX lacks random().  DEC OSF/1 1.2 random() returns a double.  */
long mrand48 ();
static inline long
random ()
{
  return mrand48 ();
}
#else
long random ();
#endif

void
#if __STDC__
mpf_random2 (mpf_ptr x, mp_size_t size, mp_exp_t exp)
#else
mpf_random2 (x, size, exp)
     mpf_ptr x;
     mp_size_t size;
     mp_exp_t exp;
#endif
{
  mp_size_t asize;
  mp_size_t prec = x->_mp_prec;

  asize = ABS (size);
  if (asize != 0)
    {
      if (asize > prec + 1)
	asize = prec + 1;

      mpn_random2 (x->_mp_d, asize);
    }

  if (exp != 0)
    exp = random () % (2 * exp) - exp;
  x->_mp_exp = asize == 0 ? 0 : exp;
  x->_mp_size = size < 0 ? -asize : asize;
}
