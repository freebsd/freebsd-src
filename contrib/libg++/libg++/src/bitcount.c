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

/* bit_count[I] is number of '1' bits in I. */
static const unsigned char
four_bit_count[16] = {
    0, 1, 1, 2,
    1, 2, 2, 3,
    1, 2, 2, 3,
    2, 3, 3, 4};

#if !defined(inline) && !defined(__GNUC__) && !defined(__cplusplus)
#define inline
#endif

static inline int
_BS_count_word (word)
     register _BS_word word;
{
  register int count = 0;
  while (word > 0)
    {
      count += four_bit_count[word & 15];
      word >>= 4;
    }
  return count;
}

int
_BS_count (ptr, offset, length)
     register const _BS_word *ptr;
     int offset;
     _BS_size_t length;
{
  register int count = 0;
#undef DOIT
#define DOIT(WORD, MASK) count += _BS_count_word ((WORD) & (MASK));
#include "bitdo1.h"
  return count;
}
