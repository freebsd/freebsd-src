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
#include "<T>.<C>.CHMap.h"

// The nodes are linked together serially via a version
// of a trick used in some vtables: odd pointers are
// actually links to the next table entry. 
// Not terrible, but not wonderful either

static inline int goodCHptr(<T><C>CHNode* t)
{
  return ((((unsigned)t) & 1) == 0);
}

static inline <T><C>CHNode* index_to_CHptr(int i)
{
  return (<T><C>CHNode*)((i << 1) + 1);
}

static inline int CHptr_to_index(<T><C>CHNode* t)
{
  return ( ((unsigned) t) >> 1);
}

<T><C>CHMap::<T><C>CHMap(<C&> dflt, unsigned int sz)
     :<T><C>Map(dflt)
{
  tab = (<T><C>CHNode**)(new <T><C>CHNodePtr[size = sz]);
  for (unsigned int i = 0; i < size; ++i) tab[i] = index_to_CHptr(i+1);
  count = 0;
}

<T><C>CHMap::<T><C>CHMap(<T><C>CHMap& a) :<T><C>Map(a.def)
{
  tab = (<T><C>CHNode**)(new <T><C>CHNodePtr[size = a.size]);
  for (unsigned int i = 0; i < size; ++i) tab[i] = index_to_CHptr(i+1);
  count = 0;
  for (Pix p = a.first(); p; a.next(p)) (*this)[a.key(p)] = a.contents(p);
}


Pix <T><C>CHMap::seek(<T&> key)
{
  unsigned int h = <T>HASH(key) % size;

  for (<T><C>CHNode* t = tab[h]; goodCHptr(t); t = t->tl)
    if (<T>EQ(key, t->hd))
      return Pix(t);

  return 0;
}


<C>& <T><C>CHMap::operator [](<T&> item)
{
  unsigned int h = <T>HASH(item) % size;

  for (<T><C>CHNode* t = tab[h]; goodCHptr(t); t = t->tl)
    if (<T>EQ(item, t->hd))
      return t->cont;

  t = new <T><C>CHNode(item, def, tab[h]);
  tab[h] = t;
  ++count;
  return t->cont;
}


void <T><C>CHMap::del(<T&> key)
{
  unsigned int h = <T>HASH(key) % size;

  <T><C>CHNode* t = tab[h]; 
  <T><C>CHNode* trail = t;
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


void <T><C>CHMap::clear()
{
  for (unsigned int i = 0; i < size; ++i)
  {
    <T><C>CHNode* p = tab[i];
    tab[i] = index_to_CHptr(i+1);
    while (goodCHptr(p))
    {
      <T><C>CHNode* nxt = p->tl;
      delete(p);
      p = nxt;
    }
  }
  count = 0;
}

Pix <T><C>CHMap::first()
{
  for (unsigned int i = 0; i < size; ++i) if (goodCHptr(tab[i])) return Pix(tab[i]);
  return 0;
}

void <T><C>CHMap::next(Pix& p)
{
  <T><C>CHNode* t = ((<T><C>CHNode*)p)->tl;
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


int <T><C>CHMap::OK()
{
  int v = tab != 0;
  int n = 0;
  for (unsigned int i = 0; i < size; ++i)
  {
    for (<T><C>CHNode* p = tab[i]; goodCHptr(p); p = p->tl) ++n;
    v &= CHptr_to_index(p) == i + 1;
  }
  v &= count == n;
  if (!v) error("invariant failure");
  return v;
}
