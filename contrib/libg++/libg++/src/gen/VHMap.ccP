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
#include "<T>.<C>.VHMap.h"

/* codes for status fields */

#define EMPTYCELL   0
#define VALIDCELL   1
#define DELETEDCELL 2


<T><C>VHMap::<T><C>VHMap(<C&> dflt, unsigned int sz)
     :<T><C>Map(dflt)
{
  tab = new <T>[size = sz];
  cont = new <C>[size];
  status = new char[size];
  for (unsigned int i = 0; i < size; ++i) status[i] = EMPTYCELL;
}

<T><C>VHMap::<T><C>VHMap(<T><C>VHMap& a) : <T><C>Map(a.def)
{
  tab = new <T>[size = a.size];
  cont = new <C>[size];
  status = new char[size];
  for (unsigned int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = 0;
  for (Pix p = a.first(); p; a.next(p)) (*this)[a.key(p)] = a.contents(p);
}


/* 
 * hashing method: double hash based on high bits of hash fct,
 * followed by linear probe. Can't do too much better if table
 * sizes not constrained to be prime.
*/


static inline unsigned int doublehashinc(unsigned int h, unsigned int s)
{
  unsigned int dh =  ((h / s) % s);
  return (dh > 1)? dh : 1;
}

Pix <T><C>VHMap::seek(<T&> key)
{
  unsigned int hashval = <T>HASH(key);
  unsigned int h = hashval % size;
  for (unsigned int i = 0; i <= size; ++i)
  {
    if (status[h] == EMPTYCELL)
      return 0;
    else if (status[h] == VALIDCELL && <T>EQ(key, tab[h]))
      return Pix(&tab[h]);
    if (i == 0)
      h = (h + doublehashinc(hashval, size)) % size;
    else if (++h >= size)
      h -= size;
  }
  return 0;
}


<C>& <T><C>VHMap::operator [](<T&> item)
{
  if (HASHTABLE_TOO_CROWDED(count, size))
    resize();

  unsigned int bestspot = size;
  unsigned int hashval = <T>HASH(item);
  unsigned int h = hashval % size;
  for (unsigned int i = 0; i <= size; ++i)
  {
    if (status[h] == EMPTYCELL)
    {
      ++count;
      if (bestspot >= size) bestspot = h;
      tab[bestspot] = item;
      status[bestspot] = VALIDCELL;
      cont[bestspot] = def;
      return cont[bestspot];
    }
    else if (status[h] == DELETEDCELL)
    {
      if (bestspot >= size) bestspot = h;
    }
    else if (<T>EQ(tab[h],item))
      return cont[h];

    if (i == 0)
      h = (h + doublehashinc(hashval, size)) % size;
    else if (++h >= size)
      h -= size;
  }

  ++count;
  status[bestspot] = VALIDCELL;
  tab[bestspot] = item;
  cont[bestspot] = def;
  return cont[bestspot];
}


void <T><C>VHMap::del(<T&> key)
{
  unsigned int hashval = <T>HASH(key);
  unsigned int h = hashval % size;
  for (unsigned int i = 0; i <= size; ++i)
  {
    if (status[h] == EMPTYCELL)
      return;
    else if (status[h] == VALIDCELL && <T>EQ(key, tab[h]))
    {
      status[h] = DELETEDCELL;
      --count;
      return;
    }
    if (i == 0)
      h = (h + doublehashinc(hashval, size)) % size;
    else if (++h >= size)
      h -= size;
  }
}


void <T><C>VHMap::clear()
{
  for (unsigned int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = 0;
}

void <T><C>VHMap::resize(unsigned int newsize)
{
  if (newsize <= count)
  {
    newsize = DEFAULT_INITIAL_CAPACITY;
    while (HASHTABLE_TOO_CROWDED(count, newsize)) newsize <<= 1;
  }
  <T>* oldtab = tab;
  <C>* oldcont = cont;
  char* oldstatus = status;
  unsigned int oldsize = size;
  tab = new <T>[size = newsize];
  cont = new <C>[size];
  status = new char[size];
  for (unsigned int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = 0;
  for (unsigned int i = 0; i < oldsize; ++i) 
    if (oldstatus[i] == VALIDCELL) 
      (*this)[oldtab[i]] = oldcont[i];
  delete [] oldtab;
  delete [] oldcont;
  delete [] oldstatus;
}

Pix <T><C>VHMap::first()
{
  for (unsigned int pos = 0; pos < size; ++pos)
    if (status[pos] == VALIDCELL) return Pix(&tab[pos]);
  return 0;
}

void <T><C>VHMap::next(Pix& i)
{
  if (i == 0) return;
  unsigned int pos = ((unsigned)i - (unsigned)tab) / sizeof(<T>) + 1;
  for (; pos < size; ++pos)
    if (status[pos] == VALIDCELL)
    {
      i = Pix(&tab[pos]);
      return;
    }
  i = 0;
}
  

int <T><C>VHMap::OK()
{
  int v = tab != 0;
  v &= status != 0;
  int n = 0;
  for (unsigned int i = 0; i < size; ++i) 
  {
    if (status[i] == VALIDCELL) ++n;
    else if (status[i] != DELETEDCELL && status[i] != EMPTYCELL)
      v = 0;
  }
  v &= n == count;
  if (!v) error("invariant failure");
  return v;
}
