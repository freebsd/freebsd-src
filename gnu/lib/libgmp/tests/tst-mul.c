/* Test mpz_add, mpz_cmp, mpz_cmp_ui, mpz_divmod, mpz_mul.

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

#include <stdio.h>
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"
#include "urandom.h"

void debug_mp ();
mp_size _mpn_mul_classic ();
void mpz_refmul ();

#ifndef SIZE
#define SIZE 8
#endif

main (argc, argv)
     int argc;
     char **argv;
{
  MP_INT multiplier, multiplicand;
  MP_INT product, ref_product;
  MP_INT quotient, remainder;
  mp_size multiplier_size, multiplicand_size;
  int i;
  int reps = 100000;

  if (argc == 2)
     reps = atoi (argv[1]);

  mpz_init (&multiplier);
  mpz_init (&multiplicand);
  mpz_init (&product);
  mpz_init (&ref_product);
  mpz_init (&quotient);
  mpz_init (&remainder);

  for (i = 0; i < reps; i++)
    {
      multiplier_size = urandom () % SIZE - SIZE/2;
      multiplicand_size = urandom () % SIZE - SIZE/2;

      mpz_random2 (&multiplier, multiplier_size);
      mpz_random2 (&multiplicand, multiplicand_size);

      mpz_mul (&product, &multiplier, &multiplicand);
      mpz_refmul (&ref_product, &multiplier, &multiplicand);
      if (mpz_cmp_ui (&multiplicand, 0) != 0)
	mpz_divmod (&quotient, &remainder, &product, &multiplicand);

      if (mpz_cmp (&product, &ref_product))
	dump_abort (&multiplier, &multiplicand);

      if (mpz_cmp_ui (&multiplicand, 0) != 0)
      if (mpz_cmp_ui (&remainder, 0) || mpz_cmp (&quotient, &multiplier))
	dump_abort (&multiplier, &multiplicand);
    }

  exit (0);
}

void
mpz_refmul (w, u, v)
     MP_INT *w;
     const MP_INT *u;
     const MP_INT *v;
{
  mp_size usize = u->size;
  mp_size vsize = v->size;
  mp_size wsize;
  mp_size sign_product;
  mp_ptr up, vp;
  mp_ptr wp;
  mp_ptr free_me = NULL;
  size_t free_me_size;

  sign_product = usize ^ vsize;
  usize = ABS (usize);
  vsize = ABS (vsize);

  if (usize < vsize)
    {
      /* Swap U and V.  */
      {const MP_INT *t = u; u = v; v = t;}
      {mp_size t = usize; usize = vsize; vsize = t;}
    }

  up = u->d;
  vp = v->d;
  wp = w->d;

  /* Ensure W has space enough to store the result.  */
  wsize = usize + vsize;
  if (w->alloc < wsize)
    {
      if (wp == up || wp == vp)
	{
	  free_me = wp;
	  free_me_size = w->alloc;
	}
      else
	(*_mp_free_func) (wp, w->alloc * BYTES_PER_MP_LIMB);

      w->alloc = wsize;
      wp = (mp_ptr) (*_mp_allocate_func) (wsize * BYTES_PER_MP_LIMB);
      w->d = wp;
    }
  else
    {
      /* Make U and V not overlap with W.  */
      if (wp == up)
	{
	  /* W and U are identical.  Allocate temporary space for U.  */
	  up = (mp_ptr) alloca (usize * BYTES_PER_MP_LIMB);
	  /* Is V identical too?  Keep it identical with U.  */
	  if (wp == vp)
	    vp = up;
	  /* Copy to the temporary space.  */
	  MPN_COPY (up, wp, usize);
	}
      else if (wp == vp)
	{
	  /* W and V are identical.  Allocate temporary space for V.  */
	  vp = (mp_ptr) alloca (vsize * BYTES_PER_MP_LIMB);
	  /* Copy to the temporary space.  */
	  MPN_COPY (vp, wp, vsize);
	}
    }

  wsize = _mpn_mul_classic (wp, up, usize, vp, vsize);
  w->size = sign_product < 0 ? -wsize : wsize;
  if (free_me != NULL)
    (*_mp_free_func) (free_me, free_me_size * BYTES_PER_MP_LIMB);

  alloca (0);
}

mp_size
_mpn_mul_classic (prodp, up, usize, vp, vsize)
     mp_ptr prodp;
     mp_srcptr up;
     mp_size usize;
     mp_srcptr vp;
     mp_size vsize;
{
  mp_size n;
  mp_size prod_size;
  mp_limb cy;
  mp_size i, j;
  mp_limb prod_low, prod_high;
  mp_limb cy_dig;
  mp_limb v_limb, c;

  if (vsize == 0)
    return 0;

  /* Offset UP and PRODP so that the inner loop can be faster.  */
  up += usize;
  prodp += usize;

  /* Multiply by the first limb in V separately, as the result can
     be stored (not added) to PROD.  We also avoid a loop for zeroing.  */
  v_limb = vp[0];
  cy_dig = 0;
  j = -usize;
  do
    {
      umul_ppmm (prod_high, prod_low, up[j], v_limb);
      add_ssaaaa (cy_dig, prodp[j], prod_high, prod_low, 0, cy_dig);
      j++;
    }
  while (j < 0);

  prodp[j] = cy_dig;
  prodp++;

  /* For each iteration in the outer loop, multiply one limb from
     U with one limb from V, and add it to PROD.  */
  for (i = 1; i < vsize; i++)
    {
      v_limb = vp[i];
      cy_dig = 0;
      j = -usize;

      /* Inner loops.  Simulate the carry flag by jumping between
	 these loops.  The first is used when there was no carry
	 in the previois iteration; the second when there was carry.  */

      do
	{
	  umul_ppmm (prod_high, prod_low, up[j], v_limb);
	  add_ssaaaa (cy_dig, prod_low, prod_high, prod_low, 0, cy_dig);
	  c = prodp[j];
	  prod_low += c;
	  prodp[j] = prod_low;
	  if (prod_low < c)
	    goto cy_loop;
	ncy_loop:
	  j++;
	}
      while (j < 0);

      prodp[j] = cy_dig;
      prodp++;
      continue;

      do
	{
	  umul_ppmm (prod_high, prod_low, up[j], v_limb);
	  add_ssaaaa (cy_dig, prod_low, prod_high, prod_low, 0, cy_dig);
	  c = prodp[j];
	  prod_low += c + 1;
	  prodp[j] = prod_low;
	  if (prod_low > c)
	    goto ncy_loop;
	cy_loop:
	  j++;
	}
      while (j < 0);

      cy_dig += 1;
      prodp[j] = cy_dig;
      prodp++;
    }

  return usize + vsize - (cy_dig == 0);
}

dump_abort (multiplier, multiplicand)
     MP_INT *multiplier, *multiplicand;
{
  fprintf (stderr, "ERROR\n");
  fprintf (stderr, "multiplier = "); debug_mp (multiplier, -16);
  fprintf (stderr, "multiplicand  = "); debug_mp (multiplicand, -16);
  abort();
}

void
debug_mp (x, base)
     MP_INT *x;
{
  mpz_out_str (stderr, base, x); fputc ('\n', stderr);
}
