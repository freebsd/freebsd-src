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
#include "<T>.RPlex.h"

typedef <T>IChunk* _<T>IChunk_ptr;

<T>RPlex:: <T>RPlex()
{
  lo = fnc = 0;
  csize = DEFAULT_INITIAL_CAPACITY;
  <T>* data = new <T>[csize];
  set_cache(new <T>IChunk(data,  lo, lo, fnc, lo+csize));
  hd = ch;
  maxch = MIN_NCHUNKS;
  lch = maxch / 2;
  fch = lch + 1;
  base = ch->base_index() - lch * csize;
  chunks = new _<T>IChunk_ptr[maxch];
  chunks[lch] = ch;
}

<T>RPlex:: <T>RPlex(int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  lo = fnc = 0;
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
  maxch = MIN_NCHUNKS;
  lch = maxch / 2;
  fch = lch + 1;
  base = ch->base_index() - lch * csize;
  chunks = new _<T>IChunk_ptr[maxch];
  chunks[lch] = ch;
}


<T>RPlex:: <T>RPlex(int l, int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  lo = fnc = l;
  if (chunksize > 0)
  {
    csize = chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  lo, lo, fnc, lo+csize));
    hd = ch;
  }
  else
  {
    csize = -chunksize;
    <T>* data = new <T>[csize];
    set_cache(new <T>IChunk(data,  chunksize+lo, lo, fnc, fnc));
    hd = ch;
  }
  maxch = MIN_NCHUNKS;
  lch = maxch / 2;
  fch = lch + 1;
  base = ch->base_index() - lch * csize;
  chunks = new _<T>IChunk_ptr[maxch];
  chunks[lch] = ch;
}

void <T>RPlex::make_initial_chunks(int up)
{
  int count = 0;
  int need = fnc - lo;
  hd = 0;
  if (up)
  {
    int l = lo;
    do
    {
      ++count;
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
      ++count;
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
  set_cache((<T>IChunk*)hd);
  
  maxch = MIN_NCHUNKS;
  if (maxch < count * 2)
    maxch = count * 2;
  chunks = new _<T>IChunk_ptr[maxch];
  lch = maxch / 3;
  fch = lch + count;
  base = ch->base_index() - csize * lch;
  int k = lch;
  do
  {
    chunks[k++] = ch;
    set_cache(ch->next());
  } while (ch != hd);
}

<T>RPlex:: <T>RPlex(int l, int hi, const <T&> initval, int chunksize)
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

<T>RPlex::<T>RPlex(const <T>RPlex& a)
{
  lo = a.lo;
  fnc = a.fnc;
  csize = a.csize;
  make_initial_chunks();
  for (int i = a.low(); i < a.fence(); a.next(i)) (*this)[i] = a[i];
}

void <T>RPlex::operator= (const <T>RPlex& a)
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


void <T>RPlex::cache(const <T>* p) const
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

int <T>RPlex::owns(Pix px) const
{
  <T>* p = (<T>*)px;
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = (t->next());
    if (t == old) return 0;
  }
  set_cache(t);
  return 1;
}


<T>* <T>RPlex::dosucc(const <T>* p) const
{
  if (p == 0) return 0;
  const <T>IChunk* old = ch;
  const <T>IChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = (t->next());
    if (t == old) return 0;
  }
  int i = t->index_of(p) + 1;
  if (i >= fnc) return 0;
  if (i >= t->fence_index()) t = (t->next());
  set_cache(t);
  return t->pointer_to(i);
}

<T>* <T>RPlex::dopred(const <T>* p) const
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

int <T>RPlex::add_high(const <T&> elem)
{
  <T>IChunk* t = tl();
  if (!t->can_grow_high())
  {
    if (t-><T>IChunk::empty() && one_chunk())
    {
      t->clear(fnc);
      base = t->base_index() - lch * csize;
    }
    else
    {
      <T>* data = new <T> [csize];
      t = (new <T>IChunk(data,  fnc, fnc, fnc,fnc+csize));
      t->link_to_prev(tl());
      if (fch == maxch)
      {
        maxch *= 2;
        <T>IChunk** newch = new _<T>IChunk_ptr [maxch];
        memcpy(newch, chunks, fch * sizeof(_<T>IChunk_ptr));
        delete chunks;
        chunks = newch;
      }
      chunks[fch++] = t;
    }
  }
  *((t-><T>IChunk::grow_high())) = elem;
  set_cache(t);
  return fnc++;
}

int <T>RPlex::del_high ()
{
  if (empty()) empty_error();
  <T>IChunk* t = tl();
  if (t-><T>IChunk::empty()) // kill straggler first
  {
    <T>IChunk* pred = t->prev();
    del_chunk(t);
    t = (pred);
    --fch;
  }
  t-><T>IChunk::shrink_high();
  if (t-><T>IChunk::empty() && !one_chunk())
  {
    <T>IChunk* pred = t->prev();
    del_chunk(t);
    t = (pred);
    --fch;
  }
  set_cache(t);
  return --fnc - 1;
}

int <T>RPlex::add_low (const <T&> elem)
{
  <T>IChunk* t = hd;
  if (!t->can_grow_low())
  {
    if (t-><T>IChunk::empty() && one_chunk())
    {
      t->cleardown(lo);
      base = t->base_index() - lch * csize;
    }
    else
    {
      <T>* data = new <T> [csize];
      hd = new <T>IChunk(data,  lo-csize, lo, lo, lo);
      hd->link_to_next(t);
      t = ( hd);
      if (lch == 0)
      {
        lch = maxch;
        fch += maxch;
        maxch *= 2;
        <T>IChunk** newch = new _<T>IChunk_ptr [maxch];
        memcpy(&(newch[lch]), chunks, lch * sizeof(_<T>IChunk_ptr));
        delete chunks;
        chunks = newch;
        base = t->base_index() - (lch - 1) * csize;
      }
      chunks[--lch] = t;
    }
  }
  *((t-><T>IChunk::grow_low())) = elem;
  set_cache(t);
  return --lo;
}


int <T>RPlex::del_low ()
{
  if (empty()) empty_error();
  <T>IChunk* t = hd;
  if (t-><T>IChunk::empty())
  {
    hd = t->next();
    del_chunk(t);
    t = hd;
    ++lch;
  }
  t-><T>IChunk::shrink_low();
  if (t-><T>IChunk::empty() && !one_chunk())
  {
    hd = t->next();
    del_chunk(t);
    t = hd;
    ++lch;
  }
  set_cache(t);
  return ++lo;
}

void <T>RPlex::reverse()
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

void <T>RPlex::fill(const <T&> x)
{
  for (int i = lo; i < fnc; ++i) (*this)[i] = x;
}

void <T>RPlex::fill(const <T&> x, int lo, int hi)
{
  for (int i = lo; i <= hi; ++i) (*this)[i] = x;
}


void <T>RPlex::clear()
{
  for (int i = lch + 1; i < fch; ++i)
    del_chunk(chunks[i]);
  fch = lch + 1;
  set_cache(chunks[lch]);
  ch-><T>IChunk::clear(lo);
  fnc = lo;
}

int <T>RPlex::reset_low(int l)
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
  base = hd->base_index() - lch * csize;
  return old;
}


int <T>RPlex::OK () const
{
  int v = hd != 0 && ch != 0;         // at least one chunk

  v &= fnc == tl()->fence_index();  // last chunk fnc == plex fnc
  v &= lo == hd-><T>IChunk::low_index();    // first lo == plex lo

  v &= base == hd->base_index() - lch * csize; // base is correct;
  v &= lch < fch;
  v &= fch <= maxch;                  // within allocation;

// loop for others:

  int k = lch;                        // to cross-check nch

  int found_ch = 0;                   // to make sure ch is in list;
  const <T>IChunk* t = (hd);
  for (;;)
  {
    v &= chunks[k++] == t;             // each chunk is at proper index
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
  v &= fch == k;
  if (!v) error("invariant failure");
  return v;
}
