// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1989 Free Software Foundation
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
#include <std.h>
#include <AllocRing.h>
#include <new.h>

AllocRing::AllocRing(int max)
  :n(max), current(0), nodes(new AllocQNode[max])
{
  for (int i = 0; i < n; ++i)
  {
    nodes[i].ptr = 0;
    nodes[i].sz = 0;
  }
}

int AllocRing::find(void* p)
{
  if (p == 0) return -1;

  for (int i = 0; i < n; ++i)
    if (nodes[i].ptr == p)
      return i;

  return -1;
}


void AllocRing::clear()
{
  for (int i = 0; i < n; ++i)
  {
    if (nodes[i].ptr != 0)
    {
      delete(nodes[i].ptr);
      nodes[i].ptr = 0;
    }
    nodes[i].sz = 0;
  }
  current = 0;
}


void AllocRing::free(void* p)
{
  int idx = find(p);
  if (idx >= 0)
  {
    delete nodes[idx].ptr;
    nodes[idx].ptr = 0;
  }
}

AllocRing::~AllocRing()
{
  clear();
}

int AllocRing::contains(void* p)
{
  return find(p) >= 0;
}

static inline unsigned int good_size(unsigned int s)
{
  unsigned int req = s + 4;
  unsigned int good = 8;
  while (good < req) good <<= 1;
  return good - 4;
}

void* AllocRing::alloc(int s)
{
  unsigned int size = good_size(s);

  void* p;
  if (nodes[current].ptr != 0 && 
      nodes[current].sz >= int(size) && 
      nodes[current].sz < int(4 * size))
    p = nodes[current].ptr;
  else
  {
    if (nodes[current].ptr != 0) delete nodes[current].ptr;
    p = new char[size];
    nodes[current].ptr = p;
    nodes[current].sz = size;
  }
  ++current;
  if (current >= n) current = 0;
  return p;
}
