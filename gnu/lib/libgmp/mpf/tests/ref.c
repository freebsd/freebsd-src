/* Reference floating point routines.

Copyright (C) 1996 Free Software Foundation, Inc.

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

#if __STDC__
void ref_mpf_add (mpf_t, const mpf_t, const mpf_t);
void ref_mpf_sub (mpf_t, const mpf_t, const mpf_t);
#else
void ref_mpf_add ();
void ref_mpf_sub ();
#endif

void
#if __STDC__
ref_mpf_add (mpf_t w, const mpf_t u, const mpf_t v)
#else
ref_mpf_add (w, u, v)
     mpf_t w;
     const mpf_t u;
     const mpf_t v;
#endif
{
  mp_size_t hi, lo, size;
  mp_ptr ut, vt, wt;
  int neg;
  mp_exp_t exp;
  mp_limb_t cy;
  TMP_DECL (mark);

  TMP_MARK (mark);

  if (SIZ (u) == 0)
    {
      size = ABSIZ (v);
      wt = (mp_ptr) TMP_ALLOC (size * BYTES_PER_MP_LIMB);
      MPN_COPY (wt, PTR (v), size);
      exp = EXP (v);
      neg = SIZ (v) < 0;
      goto done;
    }
  if (SIZ (v) == 0)
    {
      size = ABSIZ (u);
      wt = (mp_ptr) TMP_ALLOC (size * BYTES_PER_MP_LIMB);
      MPN_COPY (wt, PTR (u), size);
      exp = EXP (u);
      neg = SIZ (u) < 0;
      goto done;
    }
  if ((SIZ (u) ^ SIZ (v)) < 0)
    {
      mpf_t tmp;
      SIZ (tmp) = -SIZ (v);
      EXP (tmp) = EXP (v);
      PTR (tmp) = PTR (v);
      ref_mpf_sub (w, u, tmp);
      return;
    }
  neg = SIZ (u) < 0;

  /* Compute the significance of the hi and lo end of the result.  */
  hi = MAX (EXP (u), EXP (v));
  lo = MIN (EXP (u) - ABSIZ (u), EXP (v) - ABSIZ (v));
  size = hi - lo;
  ut = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  vt = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  wt = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  MPN_ZERO (ut, size);
  MPN_ZERO (vt, size);
  {int off;
  off = size + (EXP (u) - hi) - ABSIZ (u);
  MPN_COPY (ut + off, PTR (u), ABSIZ (u));
  off = size + (EXP (v) - hi) - ABSIZ (v);
  MPN_COPY (vt + off, PTR (v), ABSIZ (v));
  }

  cy = mpn_add_n (wt, ut, vt, size);
  wt[size] = cy;
  size += cy;
  exp = hi + cy;

done:
  if (size > PREC (w))
    {
      wt += size - PREC (w);
      size = PREC (w);
    }
  MPN_COPY (PTR (w), wt, size);
  SIZ (w) = neg == 0 ? size : -size;
  EXP (w) = exp;
  TMP_FREE (mark);
}

void
#if __STDC__
ref_mpf_sub (mpf_t w, const mpf_t u, const mpf_t v)
#else
ref_mpf_sub (w, u, v)
     mpf_t w;
     const mpf_t u;
     const mpf_t v;
#endif
{
  mp_size_t hi, lo, size;
  mp_ptr ut, vt, wt;
  int neg;
  mp_exp_t exp;
  TMP_DECL (mark);

  TMP_MARK (mark);

  if (SIZ (u) == 0)
    {
      size = ABSIZ (v);
      wt = (mp_ptr) TMP_ALLOC (size * BYTES_PER_MP_LIMB);
      MPN_COPY (wt, PTR (v), size);
      exp = EXP (v);
      neg = SIZ (v) > 0;
      goto done;
    }
  if (SIZ (v) == 0)
    {
      size = ABSIZ (u);
      wt = (mp_ptr) TMP_ALLOC (size * BYTES_PER_MP_LIMB);
      MPN_COPY (wt, PTR (u), size);
      exp = EXP (u);
      neg = SIZ (u) < 0;
      goto done;
    }
  if ((SIZ (u) ^ SIZ (v)) < 0)
    {
      mpf_t tmp;
      SIZ (tmp) = -SIZ (v);
      EXP (tmp) = EXP (v);
      PTR (tmp) = PTR (v);
      ref_mpf_add (w, u, tmp);
      if (SIZ (u) < 0)
	mpf_neg (w, w);
      return;
    }
  neg = SIZ (u) < 0;

  /* Compute the significance of the hi and lo end of the result.  */
  hi = MAX (EXP (u), EXP (v));
  lo = MIN (EXP (u) - ABSIZ (u), EXP (v) - ABSIZ (v));
  size = hi - lo;
  ut = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  vt = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  wt = (mp_ptr) TMP_ALLOC ((size + 1) * BYTES_PER_MP_LIMB);
  MPN_ZERO (ut, size);
  MPN_ZERO (vt, size);
  {int off;
  off = size + (EXP (u) - hi) - ABSIZ (u);
  MPN_COPY (ut + off, PTR (u), ABSIZ (u));
  off = size + (EXP (v) - hi) - ABSIZ (v);
  MPN_COPY (vt + off, PTR (v), ABSIZ (v));
  }

  if (mpn_cmp (ut, vt, size) >= 0)
    mpn_sub_n (wt, ut, vt, size);
  else
    {
      mpn_sub_n (wt, vt, ut, size);
      neg ^= 1;
    }
  exp = hi;
  while (size != 0 && wt[size - 1] == 0)
    {
      size--;
      exp--;
    }

done:
  if (size > PREC (w))
    {
      wt += size - PREC (w);
      size = PREC (w);
    }
  MPN_COPY (PTR (w), wt, size);
  SIZ (w) = neg == 0 ? size : -size;
  EXP (w) = exp;
  TMP_FREE (mark);
}
