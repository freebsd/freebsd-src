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
#include "<T>.CHSet.h"

// A CHSet is implemented as an array (tab) of buckets, each of which
// contains a pointer to a list of <T>CHNodes.  Each node contains a
// pointer to the next node in the list, and a pointer to the <T>.
// The end of the list is marked by a next node pointer which is odd
// when considered as an integer (least significant bit = 1).  The
// assumption is that CHNodes will all begin on even addresses.  If
// the odd pointer is right-shifted by one bit, it becomes the index
// within the tab array of the next bucket (that is, bucket i has
// next bucket pointer 2*(i+1)+1).

// The bucket pointers are initialized by the constructor and
// used to support the next(Pix&) method.

// This implementation is not portable to machines with different
// pointer and integer sizes, or on which CHNodes might be aligned on
// odd byte boundaries, but allows the same pointer to be used for
// chaining within a bucket and to the next bucket.


static inline int goodCHptr(<T>CHNode* t)
{
  return ((((unsigned)t) & 1) == 0);
}

static inline <T>CHNode* index_to_CHptr(int i)
{
  return (<T>CHNode*)((i << 1) + 1);
}

static inline int CHptr_to_index(<T>CHNode* t)
{
  return ( ((unsigned) t) >> 1);
}

<T>CHSet::<T>CHSet(unsigned int sz)
{
  tab = (<T>CHNode**)(new <T>CHNodePtr[size = sz]);
  for (unsigned int i = 0; i < size; ++i) tab[i] = index_to_CHptr(i+1);
  count = 0;
}

<T>CHSet::<T>CHSet(<T>CHSet& a)
{
  tab = (<T>CHNode**)(new <T>CHNodePtr[size = a.size]);
  for (unsigned int i = 0; i < size; ++i) tab[i] = index_to_CHptr(i+1);
  count = 0;
  for (Pix p = a.first(); p; a.next(p)) add(a(p));
}


Pix <T>CHSet::seek(<T&> key)
{
  unsigned int h = <T>HASH(key) % size;

  for (<T>CHNode* t = tab[h]; goodCHptr(t); t = t->tl)
    if (<T>EQ(key, t->hd))
      return Pix(t);

  return 0;
}


Pix <T>CHSet::add(<T&> item)
{
  unsigned int h = <T>HASH(item) % size;
  <T>CHNode* t;

  for (t = tab[h]; goodCHptr(t); t = t->tl)
    if (<T>EQ(item, t->hd))
      return Pix(t);

  ++count;
  t = new <T>CHNode(item, tab[h]);
  tab[h] = t;
  return Pix(t);
}


void <T>CHSet::del(<T&> key)
{
  unsigned int h = <T>HASH(key) % size;

  <T>CHNode* t = tab[h]; 
  <T>CHNode* trail = t;
  while (goodCHptr(t))
  {
    if (<T>EQ(key, t->hd))
    {
      if (trail == t)
        tab[h] = t->tl;
      else
        trail->tl = t->tl;
      delete t;
      --count;
      return;
    }
    trail = t;
    t = t->tl;
  }
}


void <T>CHSet::clear()
{
  for (unsigned int i = 0; i < size; ++i)
  {
    <T>CHNode* p = tab[i];
    tab[i] = index_to_CHptr(i+1);
    while (goodCHptr(p))
    {
      <T>CHNode* nxt = p->tl;
      delete(p);
      p = nxt;
    }
  }
  count = 0;
}

Pix <T>CHSet::first()
{
  for (unsigned int i = 0; i < size; ++i) if (goodCHptr(tab[i])) return Pix(tab[i]);
  return 0;
}

void <T>CHSet::next(Pix& p)
{
  if (p == 0) return;
  <T>CHNode* t = ((<T>CHNode*)p)->tl;
  if (goodCHptr(t))
    p = Pix(t);
  else
  {
    for (unsigned int i = CHptr_to_index(t); i < size; ++i) 
    {
      if (goodCHptr(tab[i]))
      {
        p =  Pix(tab[i]);
        return;
      }
    }
    p = 0;
  }
}

int <T>CHSet::operator == (<T>CHSet& b)
{
  if (count != b.count)
    return 0;
  else
  {
    <T>CHNode* p;
    for (unsigned int i = 0; i < size; ++i)
      for (p = tab[i]; goodCHptr(p); p = p->tl)
        if (b.seek(p->hd) == 0)
          return 0;
    for (unsigned int i = 0; i < b.size; ++i)
      for (p = b.tab[i]; goodCHptr(p); p = p->tl)
        if (seek(p->hd) == 0)
          return 0;
    return 1;
  }
}

int <T>CHSet::operator <= (<T>CHSet& b)
{
  if (count > b.count)
    return 0;
  else
  {
    for (unsigned int i = 0; i < size; ++i)
      for (<T>CHNode* p = tab[i]; goodCHptr(p); p = p->tl)
        if (b.seek(p->hd) == 0)
          return 0;
    return 1;
  }
}

void <T>CHSet::operator |= (<T>CHSet& b)
{
  if (&b == this || b.count == 0)
    return;
  for (unsigned int i = 0; i < b.size; ++i)
    for (<T>CHNode* p = b.tab[i]; goodCHptr(p); p = p->tl)
      add(p->hd);
}

void <T>CHSet::operator &= (<T>CHSet& b)
{
  for (unsigned int i = 0; i < size; ++i)
  {
    <T>CHNode* t = tab[i]; 
    <T>CHNode* trail = t;
    while (goodCHptr(t))
    {
      <T>CHNode* nxt = t->tl;
      if (b.seek(t->hd) == 0)
      {
        if (trail == tab[i])
          trail = tab[i] = nxt;
        else
          trail->tl = nxt;
        delete t;
        --count;
      }
      else
        trail = t;
      t = nxt;
    }
  }
}

void <T>CHSet::operator -= (<T>CHSet& b)
{
  for (unsigned int i = 0; i < size; ++i)
  {
    <T>CHNode* t = tab[i]; 
    <T>CHNode* trail = t;
    while (goodCHptr(t))
    {
      <T>CHNode* nxt = t->tl;
      if (b.seek(t->hd) != 0)
      {
        if (trail == tab[i])
          trail = tab[i] = nxt;
        else
          trail->tl = nxt;
        delete t;
        --count;
      }
      else
        trail = t;
      t = nxt;
    }
  }
}

int <T>CHSet::OK()
{
  int v = tab != 0;
  int n = 0;
  for (unsigned int i = 0; i < size; ++i)
  {
    <T>CHNode* p;
    for (p = tab[i]; goodCHptr(p); p = p->tl) ++n;
    v &= CHptr_to_index(p) == i + 1;
  }
  v &= count == n;
  if (!v) error("invariant failure");
  return v;
}
