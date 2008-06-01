// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988 Free Software Foundation
    written by Doug Lea (dl@rocky.oswego.edu)
     based on code by Doug Schmidt

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
#include <stream.h>
#include "<T>.VOHSet.h"


/* codes for status fields */
#define EMPTYCELL   0
#define VALIDCELL   1
#define DELETEDCELL 2


<T>VOHSet::<T>VOHSet(int sz)
{
// The size of the hash table is always the smallest power of 2 >= the size
// indicated by the user.  This allows several optimizations, including
// the use of actual double hashing and elimination of the mod instruction.

  size = 1;
  while (size < sz) size <<= 1;
  tab = new <T>[size];
  status = new char[size];
  for (int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = cnt = 0;
}

<T>VOHSet::<T>VOHSet(<T>VOHSet& a)
{
  tab = new <T>[size = a.size];
  status = new char[size];
  for (int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = cnt = 0;
  for (Pix p = a.first(); p; a.next(p)) add(a(p));
}

Pix <T>VOHSet::seek(<T&> key)
{
// Uses ordered double hashing to perform a search of the table.
// This greatly speeds up the average-case time for an unsuccessful search.

  unsigned hashval = <T>HASH(key);

  // We can avoid the mod operation since size is a power of 2.
  unsigned h = hashval & (size - 1);

  // The increment must be odd, since all odd numbers are relatively
  // prime to a power of 2!!
  unsigned inc = ((((hashval / size) << 1) + 1) & (size - 1));
  
  // There is always at least 1 empty cell, so this loop is guaranteed to halt!
  while (status[h] != EMPTYCELL)
  {
    int cmp = <T>CMP (key, tab[h]);
    if (cmp == 0)
    {
      if (status[h] == VALIDCELL)
        return Pix(&tab[h]);
      else
        return 0;
    }
    else if (cmp < 0)
      return 0;
    else
      h = ((h + inc) & (size - 1));
  }
  return 0;
}

// This adds an item if it doesn't already exist.  By performing the initial
// comparison we assure that the table always contains at least 1 empty
// spot.  This speeds up later searching by a constant factor.
// The insertion algorithm uses ordered double hashing.  See Standish's
// 1980 ``Data Structure's Techniques'' book for details.

Pix <T>VOHSet::add(<T&> x)
{
  if (size <= cnt+1) 
    resize();
  
  unsigned hashval = <T>HASH(x);
  unsigned h = hashval & (size - 1);

  if (status[h] != VALIDCELL)   // save some work if possible
  {
    if (status[h] == EMPTYCELL)
      cnt++;
    count++;
    tab[h] = x;
    status[h] = VALIDCELL; 
    return Pix(&tab[h]);
  }

  <T> item = x;
  Pix mypix = 0;
  unsigned inc = ((((hashval / size) << 1) + 1) & (size - 1));

  for (;;)
  {
    if (status[h] != VALIDCELL)
    {
      if (status[h] == EMPTYCELL)
        cnt++;
      count++;
      tab[h] = item;
      status[h] = VALIDCELL; 
      return (mypix == 0)? Pix(&tab[h]) : mypix;
    }
    int cmp = <T>CMP(item, tab[h]);
    if (cmp == 0)
      return (mypix == 0)? Pix(&tab[h]) : mypix;
    else if (cmp < 0)
    {
      <T> temp = tab[h];
      tab[h] = item;
      item = temp;
      if (mypix == 0) mypix = Pix(&tab[h]);
      hashval = <T>HASH(item);
      h = hashval & (size - 1);
      inc = ((((hashval / size) << 1) + 1) & (size - 1));
    }
    else
      h = ((h + inc) & (size - 1));
  }
}


void <T>VOHSet::del(<T&> key)
{
// This performs a deletion by marking the item's status field.
// Note that we only decrease the count, *not* the cnt, since this
// would cause trouble for subsequent steps in the algorithm.  See
// Reingold and Hanson's ``Data Structure's'' book for a justification
// of this approach.

  unsigned hashval = <T>HASH(key);
  unsigned h   = hashval & (size - 1);
  unsigned inc = ((((hashval / size) << 1) + 1) & (size - 1));
  
  while (status[h] != EMPTYCELL)
  {
    int cmp = <T>CMP(key, tab[h]);
    if (cmp > 0)
      h = ((h + inc) & (size - 1));
    else if (status[h] == VALIDCELL && cmp == 0)
    {
      status[h] = DELETEDCELL;
      count--;
      return;
    }
    else 
      return;
  }
}

void <T>VOHSet::clear()
{
  for (int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = cnt = 0;
}

void <T>VOHSet::resize(int newsize)
{
  if (newsize <= count)
    newsize = count;
  int s = 1;
  while (s <= newsize) s <<= 1;
  newsize = s;

  <T>* oldtab = tab;
  char* oldstatus = status;
  int oldsize = size;
  tab = new <T>[size = newsize];
  status = new char[size];
  for (int i = 0; i < size; ++i) status[i] = EMPTYCELL;
  count = cnt = 0;
  
  for (int i = 0; i < oldsize; ++i) if (oldstatus[i] == VALIDCELL) add(oldtab[i]);
  delete [] oldtab;
  delete oldstatus;
}

Pix <T>VOHSet::first()
{
  for (int pos = 0; pos < size; ++pos)
    if (status[pos] == VALIDCELL) return Pix(&tab[pos]);
  return 0;
}

void <T>VOHSet::next(Pix& i)
{
  if (i == 0) return;
  int pos = ((unsigned)i - (unsigned)tab) / sizeof(<T>) + 1;
  for (; pos < size; ++pos)
    if (status[pos] == VALIDCELL)
    {
      i = Pix(&tab[pos]);
      return;
    }
  i = 0;
}


int <T>VOHSet:: operator == (<T>VOHSet& b)
{
  if (count != b.count)
    return 0;
  else
  {
    for (int i = 0; i < size; ++i)
      if (status[i] == VALIDCELL && b.seek(tab[i]) == 0)
          return 0;
    for (int i = 0; i < b.size; ++i)
      if (b.status[i] == VALIDCELL && seek(b.tab[i]) == 0)
          return 0;
    return 1;
  }
}

int <T>VOHSet:: operator != (<T>VOHSet& b)
{
  return !(*this == b);
}

int <T>VOHSet::operator <= (<T>VOHSet& b)
{
  if (count > b.count)
    return 0;
  else
  {
    for (int i = 0; i < size; ++i)
      if (status[i] == VALIDCELL && b.seek(tab[i]) == 0)
          return 0;
    return 1;
  }
}

void <T>VOHSet::operator |= (<T>VOHSet& b)
{
  if (&b == this || b.count == 0)
    return;
  for (int i = 0; i < b.size; ++i)
    if (b.status[i] == VALIDCELL) add(b.tab[i]);
}

void <T>VOHSet::operator &= (<T>VOHSet& b)
{
  if (&b == this || count == 0)
    return;
  for (int i = 0; i < size; ++i)
  {
    if (status[i] == VALIDCELL && b.seek(tab[i]) == 0)
    {
      status[i] = DELETEDCELL;
      --count;
    }
  }
}

void <T>VOHSet::operator -= (<T>VOHSet& b)
{
  for (int i = 0; i < size; ++i)
  {
    if (status[i] == VALIDCELL && b.seek(tab[i]) != 0)
    {
      status[i] = DELETEDCELL;
      --count;
    }
  }
}

int <T>VOHSet::OK()
{
  int v = tab != 0;
  v &= status != 0;
  int n = 0;
  for (int i = 0; i < size; ++i) 
  {
    if (status[i] == VALIDCELL) ++n;
    else if (status[i] != DELETEDCELL && status[i] != EMPTYCELL)
      v = 0;
  }
  v &= n == count;
  if (!v) error("invariant failure");
  return v;
}
