// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)

This file is part of the GNU C++ Library.  This library is free
software; you can redistribute it and/or modify it under the terms of
the GNU Library General Public License as published by the Free
Software Foundation; either version 2 of the License, or (at your
option) any later version.  This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the GNU Library General Public License for more details.
You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free Software
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include "<T>.XPBag.h"

int <T>XPBag::OK()
{
  int v = p.OK();
  v &= count == p.length();
  if (!v) error("invariant failure");
  return v;
}

Pix <T>XPBag::seek(<T&> item, Pix i)
{
  if (i == 0) i = p.first(); else next(i);
  for (; i != 0; p.next(i)) if (<T>EQ(p(i), item)) return i;
  return 0;
}

int <T>XPBag::nof(<T&> item)
{
  int n = 0;
  for (int i = p.low(); i < p.fence(); p.next(i)) if (<T>EQ(p[i], item)) ++n;
  return n;
}

void <T>XPBag::del(<T&> item)
{
  for (int i = p.low(); i < p.fence(); p.next(i))
  {
    if (<T>EQ(p[i], item))
    {
      --count;
      p[i] = p.low_element();
      p.del_low();
      return;
    }
  }
}

void <T>XPBag::remove(<T&> item)
{
  for (int i = p.low(); i < p.fence(); p.next(i))
  {
    if (<T>EQ(p[i], item))
    {
      --count;
      p[i] = p.low_element();
      p.del_low();
    }
  }
}

