/* mpf_neg -- Negate a float.

Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.

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

void
#if __STDC__
mpf_neg (mpf_ptr r, mpf_srcptr u)
#else
mpf_neg (r, u)
     mpf_ptr r;
     mpf_srcptr u;
#endif
{
  mp_size_t size;

  size = -u->_mp_size;
  if (r != u)
    {
      mp_size_t prec;
      mp_size_t asize;
      mp_ptr rp, up;

      prec = r->_mp_prec + 1;	/* lie not to lose precision in assignment */
      asize = ABS (size);
      rp = r->_mp_d;
      up = u->_mp_d;

      if (asize > prec)
	{
	  up += asize - prec;
	  asize = prec;
	}

      MPN_COPY (rp, up, asize);
      r->_mp_exp = u->_mp_exp;
      size = size >= 0 ? asize : -asize;
    }
  r->_mp_size = size;
}
