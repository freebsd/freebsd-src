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
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

As a special exception, if you link this library with files
compiled with a GNU compiler to produce an executable, this does not cause
the resulting executable to be covered by the GNU General Public License.
This exception does not however invalidate any other reasons why
the executable file might be covered by the GNU General Public License. */

/*  Written by Per Bothner (bothner@cygnus.com).
    Based on ideas in the X11 MFB server. */

#include "bitprims.h"
#define ONES ((_BS_word)(~0))

/* Copy LENGTH bits from (starting at SRCBIT) into pdst starting at DSTBIT.
   This will work even if psrc & pdst overlap. */

void
_BS_blt (op, pdst, dstbit, psrc, srcbit, length)
     enum _BS_alu op;
     register _BS_word* pdst;
     int dstbit;
     register const _BS_word* psrc;
     int srcbit;
     _BS_size_t length;
{
  _BS_word ca1, cx1, ca2, cx2;
  switch (op)
    {
    case _BS_alu_clear:
      _BS_clear (pdst, dstbit, length);
      return;
    case _BS_alu_and:
      _BS_and (pdst, dstbit, psrc, srcbit, length);
      return;
    case _BS_alu_andReverse:
      ca1 = ONES; cx1 = 0; ca2 = ONES; cx2 = 0;
      break;
    case _BS_alu_copy:
      _BS_copy (pdst, dstbit, psrc, srcbit, length);
      return;
    case _BS_alu_andInverted:
      ca1 = ONES; cx1 = ONES; ca2 = 0; cx2 = 0;
      break;
    case _BS_alu_noop:
      return;
    case _BS_alu_xor:
      _BS_xor (pdst, dstbit, psrc, srcbit, length);
      return;
    case _BS_alu_or:
      ca1 = ONES; cx1 = ONES; ca2 = ONES; cx2 = 0;
      break;
    case _BS_alu_nor:
      ca1 = ONES; cx1 = ONES; ca2 = ONES; cx2 = ONES;
      break;
    case _BS_alu_equiv:
      ca1 = 0; cx1 = ONES; ca2 = ONES; cx2 = ONES;
      break;
    case _BS_alu_invert:
      _BS_invert (pdst, dstbit, length);
      return;
    case _BS_alu_orReverse:
      ca1 = ONES; cx1 = ONES; ca2 = 0; cx2 = ONES;
      break;
    case _BS_alu_copyInverted:
      ca1 = 0; cx1 = 0; ca2 = ONES; cx2 = ONES;
      break;
    case _BS_alu_orInverted:
      ca1 = ONES; cx1 = 0; ca2 = ONES; cx2 = ONES;
      break;
    case _BS_alu_nand:
      ca1 = ONES; cx1 = 0; ca2 = 0; cx2 = ONES;
      break;
    case _BS_alu_set:
      _BS_set (pdst, dstbit, length);
      return;
    }
  {
#define COMBINE(dst, src)  ( ((dst) & ( ((src) & ca1) ^ cx1)) ^ ( ((src) & ca2) ^ cx2))
#include "bitdo2.h"
  }
}
