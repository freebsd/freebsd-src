/* urandom.h -- define urandom returning a full unsigned long random value.

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

#if defined (__hpux) || defined (__svr4__) || defined (__SVR4)
/* HPUX lacks random().  */
static inline unsigned long
urandom ()
{
  return mrand48 ();
}
#define __URANDOM
#endif

#if defined (__alpha) && !defined (__URANDOM)
/* DEC OSF/1 1.2 random() returns a double.  */
long mrand48 ();
static inline unsigned long
urandom ()
{
  return mrand48 () | (mrand48 () << 32);
}
#define __URANDOM
#endif

#if BITS_PER_MP_LIMB == 32 && !defined (__URANDOM)
long random ();
static inline unsigned long
urandom ()
{
  /* random() returns 31 bits, we want 32.  */
  return random () ^ (random () << 1);
}
#define __URANDOM
#endif

#if BITS_PER_MP_LIMB == 64 && !defined (__URANDOM)
long random ();
static inline unsigned long
urandom ()
{
  /* random() returns 31 bits, we want 64.  */
  return random () ^ (random () << 31) ^ (random () << 62);
}
#define __URANDOM
#endif

