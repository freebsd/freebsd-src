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
#include <stream.h>
#include "<T>.VStack.h"

// error handling


<T>VStack::<T>VStack(<T>VStack& b)
:size(b.size), ptr(b.ptr), s(new <T> [b.size])
{
  for (int i = 0; i < ptr; ++i) s[i] = b.s[i];
}

void <T>VStack::operator = (<T>VStack& b)
{
  if (&b == this) return;
  if (size < b.ptr)
  {
    delete [] s;
    s = new <T> [b.size];
    size = b.size;
  }
  ptr = b.ptr;
  for (int i = 0; i < ptr; ++i) s[i] = b.s[i];
}


void <T>VStack::resize(int newsz)
{
  if (newsz < ptr) error("resize: new size too small");
  <T>* news = new <T> [newsz];
  for (int i = 0; i < ptr; ++i) news[i] = s[i];
  delete [] s;
  s = news;
  size = newsz;
}

int <T>VStack::OK()
{
  int v = s != 0;               // have space
  v &= size >= 0;               // a legal size
  v &= ptr <= size;             // ptr within bounds
  v &= ptr >= 0;
  if (!v) error("invariant failure");
  return v;
}
