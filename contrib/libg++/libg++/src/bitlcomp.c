/* Copyright (C) 1994 Free Software Foundation

This file is part of the GNU BitString Library.  This library is free
software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option)
any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */

/*  Written by Per Bothner (bothner@cygnus.com) */

#include "bitprims.h"
#include <stdlib.h>

/* Return -1, 0, 1 depending on whether (ptr0, len0) is
   lexicographically less than, equal, or greater than (ptr1, len1).
   Both bitstrings must be left-aligned. */

int
_BS_lcompare_0 (ptr0, len0, ptr1, len1)
     register const _BS_word *ptr0;
     _BS_size_t len0;
     register const _BS_word *ptr1;
     _BS_size_t len1;
{
  _BS_size_t nwords0 = len0 / _BS_BITS_PER_WORD;
  _BS_size_t nwords1 = len1 / _BS_BITS_PER_WORD;
  register _BS_word word0, word1;
  _BS_size_t nwords = nwords0 > nwords1 ? nwords1 : nwords0;
  for (; nwords != 0; nwords--)
    {
      word0 = *ptr0++;
      word1 = *ptr1++;
      if (word0 != word1)
	{
#if _BS_BIGENDIAN
	  return (word0 < word1) ? -1 : 1;
#else
	  {
	    _BS_word diff=(word0^word1); 	/* one's where different */
	    _BS_word mask=diff&~(diff-1);	/* first bit different */
	    return (word0&mask)?1:-1;
	  }
#endif
	}
    }
  len0 -= nwords0 * _BS_BITS_PER_WORD;
  len1 -= nwords1 * _BS_BITS_PER_WORD;
  if (len0 == 0 || len1 == 0)
    return (len1 == 0) - (len0 == 0);
  len0 &= _BS_BITS_PER_WORD - 1;
  len1 &= _BS_BITS_PER_WORD - 1;
  word0 = *ptr0++ & ~((_BS_word)(~0) _BS_RIGHT len0);
  word1 = *ptr1++ & ~((_BS_word)(~0) _BS_RIGHT len1);
  if (word0 == word1)
    return len0 == len1 ? 0 : len0 < len1 ? -1 : 1;
#if _BS_BIGENDIAN
  return (word0 < word1) ? -1 : 1;
#else
  {
    _BS_word diff=(word0^word1); 	/* one's where different */
    _BS_word mask=diff&~(diff-1);	/* first bit different */
    return (word0&mask)?1:-1;
  }
#endif
}

