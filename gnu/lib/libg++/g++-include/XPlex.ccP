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
#include "<T>.XPlex.h"


<T>XPlex:: <T>XPlex()
{
  lo = fnc = 0;
  csize = DEFAULT_INITIAL_CAPACITY;
  <T>* data = new <T>[csize];
  set_cache(new <T>IChunk(data,  lo, lo, fnc, lo+csize));
  hd = ch;
}

<T>XPlex:: <T>XPlex(int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  lo = fnc = 0;
  if (chunksize > 0)
  {
    csize = chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  lo, lo, fnc, csize));
    hd = ch;
  }
  else
  {
    csize = -chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  chunksize, lo, fnc, fnc));
    hd = ch;
  }
}


<T>XPlex:: <T>XPlex(int l, int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  lo = fnc = l;
  if (chunksize > 0)
  {
    csize = chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  lo, lo, fnc, csize+lo));
    hd = ch;
  }
  else
  {
    csize = -chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  chunksize+lo, lo, fnc, fnc));
    hd = ch;
  }
}

void <T>XPlex::make_initial_chunks(int up)
{
  int need = fnc - lo;
  hd = 0;
  if (up)
  {
    int l = lo;
    do
    {
      int sz;
      if (need >= csize)
        sz = csize;
      else
        sz = need;
      <T>* data = new <T> [csize];
      <T>IChunk* h = new <T>IChunk(data,  l, l, l+sz, l+csize);
      if (hd != 0)
        h->link_to_next(hd);
      else
        hd = h;
      l += sz;
      need -= sz;
    } while (need > 0);
  }
  else
  {
    int hi = fnc;
    do
    {
      int sz;
      if (need >= csize)
        sz = csize;
      else
        sz = need;
      <T>* data = new <T> [csize];
      <T>IChunk* h = new <T>IChunk(data,  hi-csize, hi-sz, hi, hi);
      if (hd != 0)
        h->link_to_next(hd);
      hd = h;
      hi -= sz;
      need -= sz;
    } while (need > 0);
  }
  set_cache(hd);
}

<T>XPlex:: <T>XPlex(int l, int hi, const <T&> initval, int chunksize)
{
  lo = l;
  fnc = hi + 1;
  if (chunksize == 0)
  {
    csize = fnc - l;
    make_initial_chunks(1);
  }
  else if (chunksize < 0)
  {
    csize = -chunksize;
    make_initial_chunks(0);
  }
  else
  {
    csize = chunksize;
    make_initial_chunks(1);
  }
  fill(initval);
}

<T>XPlex::<T>XPlex(const <T>XPlex& a)
{
  lo = a.lo;
  fnc = a.fnc;
  csize = a.csize;
  make_initial_chunks();
  for (int i = a.low(); i < a.fence(); a.next(i)) (*this)[i] = a[i];
}

void <T>XPlex::operator= (const <T>XPlex& a)
{
  if (&a != this)
  {
    invalidate();
    lo = a.lo;
    fnc = a.fnc;
    csize = a.csize;
    make_initial_chunks();
    for (int i = a.low(); i < a.fence(); a.next(i)) (*this)[i] = a[i];
  }
}


void <T>XPlex::cache(int idx) const
{
  const <T>IChunk* tail = tl();
  const <T>IChunk* t = ch;
  while (idx >= t->fence_index())
  {
    if (t == tail) index_error();
    t = (t->next());
  }
  while (idx < t->low_index())
  {
    if (t == hd) index_error();
    t = (t->prev());
  }
  set_cache(t);
}


void <T>XPlex::cache(const <T>* p) const
{
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = (t->next());
    if (t == old) index_error();
  }
  set_cache(t);
}

int <T>XPlex::owns(Pix px) const
{
  <T>* p = (<T>*)px;
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = (t->next());
    if (t == old) { set_cache(t); return 0; }
  }
  set_cache(t);
  return 1;
}


<T>* <T>XPlex::dosucc(const <T>* p) const
{
  if (p == 0) return 0;
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
 
  while (!t->actual_pointer(p))
  {
    t = (t->next());
    if (t == old)  return 0;
  }
  int i = t->index_of(p) + 1;
  if (i >= fnc) return 0;
  if (i >= t->fence_index()) t = (t->next());
  set_cache(t);
  return (t->pointer_to(i));
}

<T>* <T>XPlex::dopred(const <T>* p) const
{
  if (p == 0) return 0;
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = (t->prev());
    if (t == old) return 0;
  }
  int i = t->index_of(p) - 1;
  if (i < lo) return 0;
  if (i < t->low_index()) t = (t->prev());
  set_cache(t);
  return (t->pointer_to(i));
}


int <T>XPlex::add_high(const <T&> elem)
{
  <T>IChunk* t = tl();
  if (!t->can_grow_high())
  {
    if (t-><T>IChunk::empty() && one_chunk())
      t->clear(fnc);
    else
    {
      <T>* data = new <T> [csize];
      t = (new <T>IChunk(data,  fnc, fnc, fnc,fnc+csize));
      t->link_to_prev(tl());
    }
  }
  *((t-><T>IChunk::grow_high())) = elem;
  set_cache(t);
  return fnc++;
}

int <T>XPlex::del_high ()
{
  if (empty()) empty_error();
  <T>IChunk* t = tl();
  t-><T>IChunk::shrink_high();
  if (t-><T>IChunk::empty() && !one_chunk())
  {
    <T>IChunk* pred = t->prev();
    del_chunk(t);
    t = pred;
  }
  set_cache(t);
  return --fnc - 1;
}

int <T>XPlex::add_low (const <T&> elem)
{
  <T>IChunk* t = hd;
  if (!t->can_grow_low())
  {
    if (t-><T>IChunk::empty() && one_chunk())
      t->cleardown(lo);
    else
    {
      <T>* data = new <T> [csize];
      hd = new <T>IChunk(data,  lo-csize, lo, lo, lo);
      hd->link_to_next(t);
      t = hd;
    }
  }
  *((t-><T>IChunk::grow_low())) = elem;
  set_cache(t);
  return --lo;
}


int <T>XPlex::del_low ()
{
  if (empty()) empty_error();
  <T>IChunk* t = hd;
  t-><T>IChunk::shrink_low();
  if (t-><T>IChunk::empty() && !one_chunk())
  {
    hd = t->next();
    del_chunk(t);
    t = hd;
  }
  set_cache(t);
  return ++lo;
}

void <T>XPlex::reverse()
{
  <T> tmp;
  int l = lo;
  int h = fnc - 1;
  <T>IChunk* loch = hd;
  <T>IChunk* hich = tl();
  while (l < h)
  {
    <T>* lptr = loch->pointer_to(l);
    <T>* hptr = hich->pointer_to(h);
    tmp = *lptr;
    *lptr = *hptr;
    *hptr = tmp;
    if (++l >= loch->fence_index()) loch = loch->next();
    if (--h < hich->low_index()) hich = hich->prev();
  }
}

void <T>XPlex::fill(const <T&> x)
{
  for (int i = lo; i < fnc; ++i) (*this)[i] = x;
}

void <T>XPlex::fill(const <T&> x, int l, int hi)
{
  for (int i = l; i <= hi; ++i) (*this)[i] = x;
}


void <T>XPlex::clear()
{
  if (fnc != lo)
  {
    <T>IChunk* t = tl();
    while (t != hd)
    {
      <T>IChunk* prv = t->prev();
      del_chunk(t);
      t = prv;
    }
    t-><T>IChunk::clear(lo);
    set_cache(t);
    fnc = lo;
  }
}


int <T>XPlex::OK () const
{
  int v = hd != 0 && ch != 0;     // at least one chunk

  v &= fnc == tl()->fence_index();// last chunk fence == plex fence
  v &= lo == ((hd))-><T>IChunk::low_index();    // first lo == plex lo

// loop for others:
  int found_ch = 0;                   // to make sure ch is in list;
  const <T>IChunk* t = (hd);
  for (;;)
  {
    if (t == ch) ++found_ch;
    v &= t-><T>IChunk::OK();              // each chunk is OK
    if (t == tl())
      break;
    else                              // and has indices contiguous to succ
    {
      v &= t->top_index() == t->next()->base_index();
      if (t != hd)                  // internal chunks full
      {
        v &= !t->empty();
        v &= !t->can_grow_low();
        v &= !t->can_grow_high();
      }
      t = t->next();
    }
  }
  v &= found_ch == 1;
  if (!v) error("invariant failure");
  return v;
}
