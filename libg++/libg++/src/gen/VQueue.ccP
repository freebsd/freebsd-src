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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <stream.h>
#include "<T>.VQueue.h"

<T>VQueue::<T>VQueue(<T>VQueue& b)
:size(b.size), cnt(b.cnt), inp(b.inp), outp(b.outp), s(new <T> [b.size])
{
  int j = outp;
  for (int i = 0; i < cnt; ++i)
  {
    s[j] = b.s[j];
    if (++j == size) j = 0;
  }
}

void <T>VQueue::operator = (<T>VQueue& b)
{
  if (&b == this) return;
  if (size != b.size)
  {
    delete [] s;
    s = new <T> [b.size];
    size = b.size;
  }
  inp = b.inp; outp = b.outp; cnt = b.cnt;
  int j = outp;
  for (int i = 0; i < cnt; ++i)
  {
    s[j] = b.s[j];
    if (++j == size) j = 0;
  }
}


void <T>VQueue::resize(int newsz)
{
  if (newsz < cnt)
    error("resize: new size too small");
  <T>* news = new <T> [newsz];
  int j = outp;
  for (int i = 0; i < cnt; ++i)
  {
    news[i] = s[j];
    if (++j == size) j = 0;
  }
  inp = cnt;
  outp = 0;
  delete [] s;
  s = news;
  size = newsz;
}

int <T>VQueue::OK()
{
  int v = s != 0;               // have space
  v &= size >= 0;               // a legal size
  v &= inp >= 0 && inp <= size; // pointers with bounds  
  v &= outp >= 0 && outp <= size;
  int c = (size + inp - outp) % size;
  v &= cnt == size || cnt == c; // correct count
  if (!v) error("invariant failure");
  return v;
}
