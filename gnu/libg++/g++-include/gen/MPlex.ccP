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
#include "<T>.MPlex.h"

// <T>MChunk support


<T>MChunk::<T>MChunk(<T>*    d,      
               int      baseidx,
               int      lowidx, 
               int      fenceidx,
               int      topidx)
     : <T>IChunk(d, baseidx, lowidx, fenceidx, topidx)
{
  unused = fence - low;
  unsigned msize = (top - base)/_MAP_BITS + 1;
  map = (unsigned long *) (new long[msize]);
  memset((void*)map, 0, msize * sizeof(long));
}

void <T>MChunk:: shrink_high ()
{
  if (fence <= low) empty_error();
  --fence;
  if (!valid(fence)) 
    --unused;
  else
    free(fence);
  reset_high();
}

void <T>MChunk:: shrink_low ()
{
  if (fence <= low) empty_error();
  if (!valid(low)) 
    --unused;
  else
    free(low);
  ++low;
  reset_low();
}

void <T>MChunk::clear(int lo)
{
  int s = top - base;
  low = base = fence = lo;
  top = base + s;
  unused = 0;
  memset((void*)map, 0, ((top - base)/_MAP_BITS + 1) * sizeof(long));
}

void <T>MChunk::cleardown(int hi)
{
  int s = top - base;
  low = top = fence = hi;
  base = top - s;
  unused = 0;
  memset((void*)map, 0, ((top - base)/_MAP_BITS + 1) * sizeof(long));
}

int <T>MChunk::del(int idx)
{
  if (idx < low || idx >= fence) index_error();
  int v = valid(idx);
  if (v)
  {
    free(idx);
    ++unused;
  }
  return v;
}


int <T>MChunk::undel(int idx)
{
  if (idx < low || idx >= fence) index_error();
  int v = valid(idx);
  if (!v)
  {
    mark(idx);
    --unused;
  }
  return v;
}

int <T>MChunk::unused_index() const
{
  if (unused_indices() == 0) index_error();
  int slot;
  if (low == base)              // can traverse 32 slots at a time
  {
    int blk = 0;
    while (map[blk] == ~0L) ++blk;
    slot = blk * _MAP_BITS + base;
  }
  else
    slot = low;

  while(valid(slot)) ++slot;
  return slot;
}

int <T>MChunk::first_index() const
{
  if (empty()) return fence;
  int slot;
  if (low == base)
  {
    int blk = 0;
    while (map[blk] == 0) ++blk;
    slot = blk * _MAP_BITS + base;
  }
  else
    slot = low;

  while(!valid(slot)) ++slot;
  return slot;
}

int <T>MChunk::last_index() const
{
  if (empty()) return low - 1;
  int slot;
  if (top == fence)
  {
    int blk = (top - base) / _MAP_BITS;
    while (map[blk] == 0) --blk;
    slot = blk * _MAP_BITS + base + _MAP_BITS - 1;
  }
  else
    slot = fence - 1;

  while(!valid(slot)) --slot;
  return slot;
}


int <T>MChunk:: OK() const
{
  int v = data != 0;             // have some data
  v &= map != 0;                 // and a map
  v &= base <= low;              // ok, index-wise
  v &= low <= fence;
  v &= fence <= top;

  v &=  ((<T>MChunk*)(nxt->prev())) == this;      // and links are OK
  v &=  ((<T>MChunk*)(prv->next())) == this;

  int bitcount = 0;              // and unused count correct
  for (int i = low; i < fence; ++i) if (!valid(i)) ++bitcount;
  v &= unused == bitcount;
  
  if (!v) error("invariant failure");
  return(v);
}

<T>* <T>MChunk::succ(<T>* p) const
{
  int i = ((int) p - (int) data) / sizeof(<T>) + base + 1;
  if (p == 0 || i < low) return 0;
  while (i < fence && !valid(i)) ++i;
  if (i >= fence) return 0;
  return pointer_to(i);
}

<T>* <T>MChunk::pred(<T>* p) const
{
  int i = ((int) p - (int) data) / sizeof(<T>) + base - 1;
  if (p == 0 || i >= fence) return 0;
  while (i >= low && !valid(i)) --i;
  if (i < low) return 0;
  return pointer_to(i);
}

<T>* <T>MChunk::first_pointer() const
{
  if (empty()) return 0;
  int slot;
  if (low == base)
  {
    int blk = 0;
    while (map[blk] == 0) ++blk;
    slot = blk * _MAP_BITS + base;
  }
  else
    slot = low;

  while(!valid(slot)) ++slot;
  return pointer_to(slot);
}

<T>* <T>MChunk::last_pointer() const
{
  if (empty()) return 0;
  int slot;
  if (top == fence)
  {
    int blk = (top - base) / _MAP_BITS;
    while (map[blk] == 0) --blk;
    slot = blk * _MAP_BITS + base + _MAP_BITS - 1;
  }
  else
    slot = fence - 1;

  while(!valid(slot)) --slot;
  return pointer_to(slot);
}

<T>MPlex:: <T>MPlex()
{
  unused = 0;
  lo = fnc = 0;
  csize = DEFAULT_INITIAL_CAPACITY;
  <T>* data = new <T>[csize];
  hd = ch = new <T>MChunk(data,  lo, lo, fnc, lo+csize);
}

<T>MPlex:: <T>MPlex(int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  unused = 0;
  lo = fnc = 0;
  if (chunksize > 0)
  {
    csize = chunksize;
    <T>* data = new <T>[csize];
    hd = ch = new <T>MChunk(data,  lo, lo, fnc, csize);
  }
  else
  {
    csize = -chunksize;
    <T>* data = new <T>[csize];
    hd = ch = new <T>MChunk(data,  chunksize, lo, fnc, fnc);
  }
}


<T>MPlex:: <T>MPlex(int l, int chunksize)
{
  if (chunksize == 0) error("invalid constructor specification");
  unused = 0;
  lo = fnc = l;
  if (chunksize > 0)
  {
    csize = chunksize;
    <T>* data = new <T>[csize];
    hd = ch = new <T>MChunk(data,  lo, lo, fnc, csize+lo);
  }
  else
  {
    csize = -chunksize;
    <T>* data = new <T>[csize];
    hd = ch = new <T>MChunk(data,  chunksize+lo, lo, fnc, fnc);
  }
}


void <T>MPlex::make_initial_chunks(int up)
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
      <T>MChunk* h = new <T>MChunk(data,  l, l, l+sz, l+csize);
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
      <T>MChunk* h = new <T>MChunk(data,  hi-csize, hi-sz, hi, hi);
      if (hd != 0)
        h->link_to_next(hd);
      hd = h;
      hi -= sz;
      need -= sz;
    } while (need > 0);
  }
  ch = (<T>MChunk*) hd;
}

<T>MPlex:: <T>MPlex(int l, int hi, const <T&> initval, int chunksize)
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
  unused = fnc - lo;
  for (int i=lo; i<fnc; ++i)
    undel_index(i);
  fill(initval);
}

<T>MPlex::<T>MPlex(const <T>MPlex& a)
{
  lo = a.lo;
  fnc = a.fnc;
  csize = a.csize;
  unused = fnc - lo;
  hd = 0;
  const <T>IChunk* p = a.hd;
  do
  {
    <T>* data = new <T> [p->size()];
    <T>MChunk* h = new <T>MChunk(data,  p->base_index(),
                          p->low_index(), p->fence_index(), p->top_index());
    if (hd != 0)
      h->link_to_next(hd);
    else
      hd = h;
    p = p->next();
  } while (p != a.hd);
  ch = (<T>MChunk*) hd;
  for (int i = a.low(); i < a.fence(); a.next(i)) 
  {
    undel_index(i);
    (*this)[i] = a[i];
  }
}

void <T>MPlex::operator= (const <T>MPlex& a)
{
  if (&a != this)
  {
    invalidate();
    lo = a.lo;
    fnc = a.fnc;
    csize = a.csize;
    unused = fnc - lo;
    hd = 0;
    const <T>IChunk* p = a.hd;
    do
    {
      <T>* data = new <T> [p->size()];
      <T>MChunk* h = new <T>MChunk(data,  p->base_index(),
                                   p->low_index(), p->fence_index(), 
                                   p->top_index());
      if (hd != 0)
        h->link_to_next(hd);
      else
        hd = h;
      p = p->next();
    } while (p != a.hd);
    ch = (<T>MChunk*) hd;
    for (int i = a.low(); i < a.fence(); a.next(i)) 
    {
      undel_index(i);
      (*this)[i] = a[i];
    }
  }
}

int <T>MPlex::valid(int idx) const
{
  const <T>MChunk* tail = (<T>MChunk*)tl();
  const <T>MChunk* t = ch;
  while (idx >= t->fence_index())
  {
    if (t == tail)  return 0; 
    t = ((<T>MChunk*)(t->next()));
  }
  while (idx < t->low_index())
  {
    if (t == (<T>MChunk*)(hd)) return 0; 
    t = ((<T>MChunk*)(t->prev()));
  }
  set_cache(t);
  return t-><T>MChunk::valid_index(idx);
}

void <T>MPlex::cache(int idx) const
{
  const <T>MChunk* tail = (<T>MChunk*)tl();
  const <T>MChunk* t = ch;
  while (idx >= t->fence_index())
  {
    if (t == tail) index_error();
    t = ((<T>MChunk*)(t->next()));
  }
  while (idx < t->low_index())
  {
    if (t == (<T>MChunk*)hd) index_error();
    t = ((<T>MChunk*)(t->prev()));
  }
  if (!t-><T>MChunk::valid_index(idx)) index_error();
  set_cache(t);
}

void <T>MPlex::cache(const <T>* p) const
{
  const <T>MChunk* old = ch;
  const <T>MChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = ((<T>MChunk*)(t->next()));
    if (t == old) index_error();
  }
  if (!t-><T>MChunk::valid_pointer(p)) index_error();
  set_cache(t);
}

int <T>MPlex::owns(Pix px) const
{
  <T>* p = (<T>*)px;
  const <T>MChunk* old = ch;
  const <T>MChunk* t = ch;
  while (!t->actual_pointer(p))
  {
    t = ((<T>MChunk*)(t->next()));
    if (t == old)  return 0; 
  }
  set_cache(t);
  return t-><T>MChunk::valid_pointer(p);
}

int <T>MPlex::add_high(const <T&> elem)
{
  <T>MChunk* t = ((<T>MChunk*) tl());

  if (!t->can_grow_high())
  {
    <T>* data = new <T> [csize];
    t = (new <T>MChunk(data, fnc,fnc,fnc,fnc+csize));
    t->link_to_prev(tl());
  }

  *((t-><T>MChunk::grow_high())) = elem;
  set_cache(t);
  return fnc++;
}

int <T>MPlex::add_low (const <T&> elem)
{
  <T>MChunk* t = ((<T>MChunk*) hd);
  if (!t->can_grow_low())
  {
    <T>* data = new <T> [csize];
    hd = new <T>MChunk(data,  lo-csize, lo, lo, lo);
    hd->link_to_next(t);
    t = ((<T>MChunk*) hd);
  }

  *((t-><T>MChunk::grow_low())) = elem;
  set_cache(t);
  return --lo;
}

void <T>MPlex::adjust_bounds()
{
  <T>MChunk* t = ((<T>MChunk*) tl());

  // clean up tail

  t->reset_high();
  while (t-><T>MChunk::empty() && !one_chunk())
  {
    <T>MChunk* pred = (<T>MChunk*)(t->prev());
    del_chunk(t);
    pred->reset_high();
    t = (pred);
  }
  if (one_chunk())
    t->reset_high();

  int oldfnc = fnc;
  fnc = t->fence_index();
  unused -= oldfnc - fnc;

  // and head..
  t = ((<T>MChunk*) hd);
  t->reset_low();
  while (t-><T>MChunk::empty() && !one_chunk())
  {
    hd = (<T>MChunk*)(t->next());
    del_chunk(t);
    t = ((<T>MChunk*) hd);
    t->reset_low();
  }

  int oldlo = lo;
  lo = t->low_index();
  unused -= lo - oldlo;


  set_cache(t);
}

int <T>MPlex::del_high ()
{
  if (empty()) empty_error();
  <T>MChunk* t = ((<T>MChunk*) tl());
  while (t-><T>MChunk::empty() && !one_chunk()) // possible stragglers
  {
    <T>MChunk* pred = (<T>MChunk*)(t->prev());
    del_chunk(t);
    pred->reset_high();
    t = (pred);
  }
  t-><T>MChunk::shrink_high();
  while (t-><T>MChunk::empty() && !one_chunk())
  {
    <T>MChunk* pred = (<T>MChunk*)(t->prev());
    del_chunk(t);
    pred->reset_high();
    t = (pred);
  }
  int oldfnc = fnc;
  fnc = t->fence_index();
  unused -= oldfnc - fnc - 1;
  set_cache(t);
  return fnc - 1;
}

int <T>MPlex::del_low ()
{
  if (empty()) empty_error();
  <T>MChunk* t = ((<T>MChunk*) hd);
  while (t-><T>MChunk::empty() && !one_chunk())
  {
    hd = (<T>MChunk*)(t->next());
    del_chunk(t);
    t = ((<T>MChunk*) hd);
    t->reset_low();
  }
  t-><T>MChunk::shrink_low();
  while (t-><T>MChunk::empty() && !one_chunk())
  {
    hd = (<T>MChunk*)(t->next());
    del_chunk(t);
    t = ((<T>MChunk*) hd);
    t->reset_low();
  }
  int oldlo = lo;
  lo = t->low_index();
  unused -= lo - oldlo - 1;
  set_cache(t);
  return lo;
}

int <T>MPlex::add(const <T&> elem)
{
  if (unused == 0) 
    return add_high(elem);

  for(<T>MChunk* t = ch; 
      t->unused_indices() == 0; 
      t = (<T>MChunk*)(t->prev()))
    ;

  int i =  t->unused_index();
  set_cache(t);
  undel_index(i);
  (*this)[i] = elem;
  return i;
}

int <T>MPlex::unused_index() const
{
  if (unused == 0) index_error();

  for(<T>MChunk* t = ch; 
      t->unused_indices() == 0; 
      t = (<T>MChunk*)(t->prev()))
    ;

  set_cache(t);
  return t->unused_index();
}

Pix <T>MPlex::unused_Pix() const
{
  if (unused == 0) return 0;

  for(<T>MChunk* t = ch; 
      t->unused_indices() == 0; 
      t = (<T>MChunk*)(t->prev()))
    ;

  set_cache(t);
  return t->pointer_to(t->unused_index()); 
}

int <T>MPlex::del_index(int idx)
{
  if (idx < lo || idx >= fnc) index_error();
  if (<T>MPlex::valid(idx))
  {
    ++unused;
    ch-><T>MChunk::del(idx);
    return 1;
  }
  else
    return 0;
}

int <T>MPlex::dopred(int idx) const
{

  if (idx >= fnc) idx = fnc;
  if (idx <= lo) return lo - 1;

  const <T>MChunk* t = ch;
  
  while (idx > t->fence_index())
  {
    t = ((<T>MChunk*)(t->next()));
  }
  while (idx <= t->low_index())
  {
    t = ((<T>MChunk*)(t->prev()));
  }
  int i = t-><T>MChunk::pred(idx);
  while (i < t->low_index() && i >= lo)
  {
    t = ((<T>MChunk*)(t->prev()));
    i = t-><T>MChunk::last_index();
  }
  set_cache(t);
  return i;
}


int <T>MPlex::dosucc(int idx) const
{
  if (idx < lo) idx = lo;
  if (idx >= fnc - 1) return fnc;

  const <T>MChunk* t = ch;
  while (idx >= t->fence_index())
  {
    t = ((<T>MChunk*)(t->next()));
  }
  while (idx < t->low_index())
  {
    t = ((<T>MChunk*)(t->prev()));
  }
  int i = t-><T>MChunk::succ(idx);
  while (i >= t->fence_index() && i < fnc)
  {
    t = (<T>MChunk*)(t->next());
    i = t-><T>MChunk::first_index();
  }
  set_cache(t);
  return i;
}

void <T>MPlex::prev(Pix& i) const
{
  if (i == 0) return;

  <T>* p = (<T>*) i;
  const <T>MChunk* old = ch;
  const <T>MChunk* t = ch;

  while (!t->actual_pointer(p))
  {
    t = ((<T>MChunk*)(t->prev()));
    if (t == old) 
    { 
      i = 0; 
      return; 
    }
  }
  <T>* q = t-><T>MChunk::pred(p);
  while (q == 0 && t != (<T>MChunk*)hd)
  {
    t = ((<T>MChunk*)(t->prev()));
    q = t-><T>MChunk::last_pointer();
  }

  i = Pix(q); 
  set_cache(t);
  return; 
}

void <T>MPlex::next(Pix& i) const
{
  if (i == 0) return;

  <T>* p = (<T>*) i;
  const <T>MChunk* tail = (<T>MChunk*)(tl());
  const <T>MChunk* old = ch;
  const <T>MChunk* t = ch;

  while (!t->actual_pointer(p))
  {
    t = ((<T>MChunk*)(t->next()));
    if (t == old) 
    { 
      i = 0; 
      return; 
    }
  }
  <T>* q = t-><T>MChunk::succ(p);
  while  (q == 0 && t != tail)
  {
    t = ((<T>MChunk*)(t->next()));
    q = t-><T>MChunk::first_pointer();
  }

  i = Pix(q); 
  set_cache(t);
  return; 
}

    
void <T>MPlex::undel_index(int idx)
{
  if (idx < lo || idx >= fnc) index_error();

  <T>MChunk* t = ch;
  while (idx >= t->fence_index())
  {
    t = ((<T>MChunk*)(t->next()));
  }
  while (idx < t->low_index())
  {
    t = ((<T>MChunk*)(t->prev()));
  }
  int was_present = t-><T>MChunk::undel(idx);
  if (!was_present) 
  {
    --unused;
  }
  set_cache(t);
  return;
}

void <T>MPlex::clear()
{
  if (fnc != lo)
  {
    <T>MChunk* t = ((<T>MChunk*)tl());
    while (t != hd)
    {
      <T>MChunk* prv = (<T>MChunk*)(t->prev());
      del_chunk(t);
      t = prv;
    }
    t-><T>MChunk::clear(lo);
    set_cache(t);
    fnc = lo;
    unused = 0;
  }
}

int <T>MPlex::OK () const
{
  int v = hd != 0;                    // at least one chunk

  int found_ch = 0;                   // to make sure ch is in list;

  int count = 0;                      // to count unused slots

  const <T>MChunk* t = (<T>MChunk*)(hd);

  int gap = t->low_index() - lo;
  v &= gap == 0;                      // hd lo not less than lo.
  count += gap;

  for (;;)
  {
    if (t == ch) ++found_ch;
    v &= t-><T>MChunk::OK();             // each chunk is OK
    count += t->unused_indices();
    if (t == (<T>MChunk*)(tl()))
      break;
    else                              // and has indices less than succ
    {
      gap = t->next()->base_index() - t->top_index();
      v &= gap == 0;
      count += gap;

      if (t != (<T>MChunk*)hd)                  // internal chunks can't grow
        v &= !t->can_grow_low() && !t->can_grow_high();

      t = (const <T>MChunk*)(t->next());
    }
  }
  gap = fnc - t->fence_index();
  v &= gap == 0;
  count += gap;

  v &= count == unused;              // chunk counts agree with plex

  v &= found_ch == 1;
  if (!v) error("invariant failure");
  return v;
}

