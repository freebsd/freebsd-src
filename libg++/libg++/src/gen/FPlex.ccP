// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)
    based on code by Marc Shapiro (shapiro@sor.inria.fr)

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
#include "<T>.FPlex.h"


<T>FPlex:: <T>FPlex()
{
  lo = fnc = 0;
  csize = DEFAULT_INITIAL_CAPACITY;
  <T>* data = new <T>[csize];
  hd = new <T>IChunk(data,  lo, lo, fnc, csize);
}

<T>FPlex:: <T>FPlex(int maxsize)
{
  if (maxsize == 0) error("invalid constructor specification");
  lo = fnc = 0;
  if (maxsize > 0)
  {
    csize = maxsize;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  lo, lo, fnc, csize);
  }
  else
  {
    csize = -maxsize;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  maxsize, lo, fnc, fnc);
  }
}


<T>FPlex:: <T>FPlex(int l, int maxsize)
{
  if (maxsize == 0) error("invalid constructor specification");
  lo = fnc = l;
  if (maxsize > 0)
  {
    csize = maxsize;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  lo, lo, fnc, csize+lo);
  }
  else
  {
    csize = -maxsize;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  maxsize+lo, lo, fnc, fnc);
  }
}

<T>FPlex:: <T>FPlex(int l, int hi, const <T&> initval, int maxsize)
{
  lo = l;
  fnc = hi + 1;
  if (maxsize >= 0)
  {
    csize = maxsize;
    if (csize < fnc - lo)
      csize = fnc - lo;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  lo, lo, fnc, csize);
  }
  else
  {
    csize = -maxsize;
    if (csize < fnc - lo)
      csize = fnc - lo;
    <T>* data = new <T>[csize];
    hd = new <T>IChunk(data,  -csize, lo, fnc, fnc);
  }
  fill(initval);
}

<T>FPlex::<T>FPlex(const <T>FPlex& a)
{
  lo = a.lo;
  fnc = a.fnc;
  csize = fnc - lo;
  if (csize < a.csize) csize = a.csize;
  <T>* data = new <T> [csize];
  hd = new <T>IChunk(data,  lo, lo, fnc, lo+csize);
  for (int i = a.low(); i < a.fence(); a.next(i)) (*this)[i] = a[i];
}

void <T>FPlex::operator= (const <T>FPlex& a)
{
  if (&a != this)
  {
    del_chunk(hd);
    lo = a.lo;
    fnc = a.fnc;
    csize = fnc - lo;
    if (csize < a.csize) csize = a.csize;
    <T>* data = new <T> [csize];
    hd = new <T>IChunk(data,  lo, lo, fnc, lo+csize);
    for (int i = a.low(); i < a.fence(); a.next(i)) (*this)[i] = a[i];
  }
}


void <T>FPlex::reverse()
{
  <T> tmp;
  int l = lo;
  int h = fnc - 1;
  while (l < h)
  {
    tmp = (*this)[l];
    (*this)[l] = (*this)[h];
    (*this)[h] = tmp;
    next(l);
    prev(h);
  }
}

void <T>FPlex::fill(const <T&> x)
{
  for (int i = lo; i < fnc; ++i) (*this)[i] = x;
}

void <T>FPlex::fill(const <T&> x, int lo, int hi)
{
  for (int i = lo; i <= hi; ++i) (*this)[i] = x;
}

void <T>FPlex::clear()
{
  if (fnc != lo)
  {
    hd->clear(lo);
    fnc = lo;
  }
}

int <T>FPlex::OK () const
{
  int v = hd != 0;                    // hd exists
  v &= hd-><T>IChunk::OK();           // and is OK
  v &= fnc - lo <= hd->size();        // and has enough space
  v &= lo <= fnc;                     // plex indices consistent
  v &= lo == hd->low_index();         // and match those 
  v &= fnc == hd->fence_index();      //   of chunk
  v &= one_chunk();                   // and only one chunk
  if (!v) error("invariant failure");
  return v;
}

