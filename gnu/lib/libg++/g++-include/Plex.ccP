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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef __GNUG__
#pragma implementation
#endif
#include <stream.h>
#include <builtin.h>
#include "<T>.Plex.h"

// IChunk support

void <T>IChunk::error(const char* msg) const
{
  (*lib_error_handler)("<T>IChunk", msg);
}

void <T>IChunk::index_error() const
{
  error("attempt to use invalid index");
}

void <T>IChunk::empty_error() const
{
  error("invalid use of empty chunk");
}

void <T>IChunk::full_error() const
{
  error("attempt to extend chunk beyond bounds");
}

<T>IChunk:: ~<T>IChunk() {}

<T>IChunk::<T>IChunk(<T>*     d,    
                     int      baseidx,
                     int      lowidx,
                     int      fenceidx,
                     int      topidx)
{
  if (d == 0 || baseidx > lowidx || lowidx > fenceidx || fenceidx > topidx)
    error("inconsistent specification");
  data = d;
  base = baseidx;
  low = lowidx;
  fence = fenceidx;
  top = topidx;
  nxt = prv = this;
}

void <T>IChunk:: re_index(int lo)
{
  int delta = lo - low;
  base += delta;
  low += delta;
  fence += delta;
  top += delta;
}


void <T>IChunk::clear(int lo)
{
  int s = top - base;
  low = base = fence = lo;
  top = base + s;
}

void <T>IChunk::cleardown(int hi)
{
  int s = top - base;
  low = top = fence = hi;
  base = top - s;
}

int <T>IChunk:: OK() const
{
  int v = data != 0;             // have some data
  v &= base <= low;              // ok, index-wise
  v &= low <= fence;
  v &= fence <= top;

  v &=  nxt->prv == this;      // and links are OK
  v &=  prv->nxt == this;
  if (!v) error("invariant failure");
  return(v);
}


// error handling


void <T>Plex::error(const char* msg) const
{
  (*lib_error_handler)("Plex", msg);
}

void <T>Plex::index_error() const
{
  error("attempt to access invalid index");
}

void <T>Plex::empty_error() const
{
  error("attempted operation on empty plex");
}

void <T>Plex::full_error() const
{
  error("attempt to increase size of plex past limit");
}

// generic plex ops

<T>Plex:: ~<T>Plex()
{
  invalidate();
}  


void <T>Plex::append (const <T>Plex& a)
{
  for (int i = a.low(); i < a.fence(); a.next(i)) add_high(a[i]);
}

void <T>Plex::prepend (const <T>Plex& a)
{
  for (int i = a.high(); i > a.ecnef(); a.prev(i)) add_low(a[i]);
}

void <T>Plex::reverse()
{
  <T> tmp;
  int l = low();
  int h = high();
  while (l < h)
  {
    tmp = (*this)[l];
    (*this)[l] = (*this)[h];
    (*this)[h] = tmp;
    next(l);
    prev(h);
  }
}


void <T>Plex::fill(const <T&> x)
{
  for (int i = lo; i < fnc; ++i) (*this)[i] = x;
}

void <T>Plex::fill(const <T&> x, int lo, int hi)
{
  for (int i = lo; i <= hi; ++i) (*this)[i] = x;
}


void <T>Plex::del_chunk(<T>IChunk* x)
{
  if (x != 0)
  {
    x->unlink();
    <T>* data = (<T>*)(x->invalidate());
    delete [] data;
    delete x;
  }
}


void <T>Plex::invalidate()
{
  <T>IChunk* t = hd;
  if (t != 0)
  {
    <T>IChunk* tail = tl();
    while (t != tail)
    {
      <T>IChunk* nxt = t->next();
      del_chunk(t);
      t = nxt;
    } 
    del_chunk(t);
    hd = 0;
  }
}

int <T>Plex::reset_low(int l)
{
  int old = lo;
  int diff = l - lo;
  if (diff != 0)
  {
    lo += diff;
    fnc += diff;
    <T>IChunk* t = hd;
    do
    {
      t->re_index(t->low_index() + diff);
      t = t->next();
    } while (t != hd);
  }
  return old;
}




