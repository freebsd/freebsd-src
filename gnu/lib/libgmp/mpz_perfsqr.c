/* mpz_perfect_square_p(arg) -- Return non-zero if ARG is a pefect square,
   zero otherwise.

Copyright (C) 1991 Free Software Foundation, Inc.

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
#include "longlong.h"

#if BITS_PER_MP_LIMB == 32
static unsigned int primes[] = {3, 5, 7, 11, 13, 17, 19, 23, 29};
static unsigned long int residue_map[] =
{0x3, 0x13, 0x17, 0x23b, 0x161b, 0x1a317, 0x30af3, 0x5335f, 0x13d122f3};

#define PP 0xC0CFD797L		/* 3 x 5 x 7 x 11 x 13 x ... x 29 */
#endif

/* sq_res_0x100[x mod 0x100] == 1 iff x mod 0x100 is a quadratic residue
   modulo 0x100.  */
static char sq_res_0x100[0x100] =
{
  1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
  0,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
};

int
#ifdef __STDC__
mpz_perfect_square_p (const MP_INT *a)
#else
mpz_perfect_square_p (a)
     const MP_INT *a;
#endif
{
  mp_limb n1, n0;
  mp_size i;
  mp_size asize = a->size;
  mp_srcptr aptr = a->d;
  mp_limb rem;
  mp_ptr root_ptr;

  /* No negative numbers are perfect squares.  */
  if (asize < 0)
    return 0;

  /* The first test excludes 55/64 (85.9%) of the perfect square candidates
     in O(1) time.  */
  if (sq_res_0x100[aptr[0] % 0x100] == 0)
    return 0;

#if BITS_PER_MP_LIMB == 32
  /* The second test excludes 30652543/30808063 (99.5%) of the remaining
     perfect square candidates in O(n) time.  */

  /* Firstly, compute REM = A mod PP.  */
  n1 = aptr[asize - 1];
  if (n1 >= PP)
    {
      n1 = 0;
      i = asize - 1;
    }
  else
    i = asize - 2;

  for (; i >= 0; i--)
    {
      mp_limb dummy;

      n0 = aptr[i];
      udiv_qrnnd (dummy, n1, n1, n0, PP);
    }
  rem = n1;

  /* We have A mod PP in REM.  Now decide if REM is a quadratic residue
     modulo the factors in PP.  */
  for (i = 0; i < (sizeof primes) / sizeof (int); i++)
    {
      unsigned int p;

      p = primes[i];
      rem %= p;
      if ((residue_map[i] & (1L << rem)) == 0)
	return 0;
    }
#endif

  /* For the third and last test, we finally compute the square root,
     to make sure we've really got a perfect square.  */
  root_ptr = (mp_ptr) alloca ((asize + 1) / 2 * BYTES_PER_MP_LIMB);

  /* Iff mpn_sqrt returns zero, the square is perfect.  */
  {
    int retval = !mpn_sqrt (root_ptr, NULL, aptr, asize);
    alloca (0);
    return retval;
  }
}
