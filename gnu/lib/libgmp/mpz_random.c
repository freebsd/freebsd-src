/* mpz_random -- Generate a random MP_INT of specified size.

Copyright (C) 1991, 1993 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

The GNU MP Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with the GNU MP Library; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "gmp.h"
#include "gmp-impl.h"

#if defined (hpux) || defined (__alpha__)
/* HPUX lacks random().  DEC Alpha's random() returns a double.  */
static inline long
urandom ()
{
  return mrand48 ();
}
#else
long random ();

static inline long
urandom ()
{
  /* random() returns 31 bits, we want 32.  */
  return random() ^ (random() << 1);
}
#endif

void
#ifdef __STDC__
mpz_random (MP_INT *x, mp_size size)
#else
mpz_random (x, size)
     MP_INT *x;
     mp_size size;
#endif
{
  mp_size i;
  mp_limb ran;

  if (x->alloc < size)
    _mpz_realloc (x, size);

  for (i = 0; i < size; i++)
    {
      ran = urandom ();
      x->d[i] = ran;
    }

  for (i = size - 1; i >= 0; i--)
    if (x->d[i] != 0)
      break;

  x->size = i + 1;
}
