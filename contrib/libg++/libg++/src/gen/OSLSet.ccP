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
#include "<T>.OSLSet.h"


Pix <T>OSLSet::seek(<T&> item)
{
  for (Pix i = p.first(); i != 0; p.next(i))
  {
    int cmp = <T>CMP(item, p(i));
    if (cmp == 0)
      return i;
    else if (cmp < 0)
      return 0;
  }
  return 0;
}

Pix <T>OSLSet::add(<T&> item)
{
  Pix i = p.first();
  if (i == 0) 
  {
    ++count;
    return p.prepend(item);
  }
  int cmp = <T>CMP(item, p(i));
  if (cmp == 0)
    return i;
  else if (cmp < 0)
  {
    ++count;
    return p.prepend(item);
  }
  else
  {
    Pix trail = i;
    p.next(i);
    for (;;)
    {
      if (i == 0)
      {
        ++count;
        return p.append(item);
      }
      cmp = <T>CMP(item, p(i));
      if (cmp == 0)
        return i;
      else if (cmp < 0)
      {
        ++count;
        return p.ins_after(trail, item);
      }
      else
      {
        trail = i;
        p.next(i);
      }
    }
  }
}

void <T>OSLSet::del(<T&> item)
{
  Pix i = p.first();
  if (i == 0)
    return;
  int cmp = <T>CMP(item, p(i));
  if (cmp < 0)
    return;
  else if (cmp == 0)
  {
    --count;
    p.del_front();
  }
  else
  {
    Pix trail = i;
    p.next(i);
    while (i != 0)
    {
      cmp = <T>CMP(item, p(i));
      if (cmp < 0)
        return;
      else if (cmp == 0)
      {
        --count;
        p.del_after(trail);
        return;
      }
      else
      {
        trail = i;
        p.next(i);
      }
    }
  }
}
        

int <T>OSLSet::operator <= (<T>OSLSet& b)
{
  if (count > b.count) return 0;
  Pix i = first();
  Pix j = b.first();
  for (;;)
  {
    if (i == 0)
      return 1;
    else if (j == 0)
      return 0;
    int cmp = <T>CMP(p(i), b.p(j));
    if (cmp == 0)
    {
      next(i); b.next(j);
    }
    else if (cmp < 0)
      return 0;
    else
      b.next(j);
  }
}

int <T>OSLSet::operator == (<T>OSLSet& b)
{
  if (count != b.count) return 0;
  if (count == 0) return 1;
  Pix i = p.first();
  Pix j = b.p.first();
  while (i != 0)
  {
    if (!<T>EQ(p(i),b.p(j))) return 0;
    next(i);
    b.next(j);
  }
  return 1;
}


void <T>OSLSet::operator |= (<T>OSLSet& b)
{
  if (&b == this || b.count == 0)
    return;
  else
  {
    Pix j = b.p.first();
    Pix i = p.first();
    Pix trail = 0;
    for (;;)
    {
      if (j == 0)
        return;
      else if (i == 0)
      {
        for (; j != 0; b.next(j))
        {
          ++count;
          p.append(b.p(j));
        }
        return;
      }
      int cmp = <T>CMP(p(i), b.p(j));
      if (cmp <= 0)
      {
        if (cmp == 0) b.next(j);
        trail = i;
        next(i);
      }
      else
      {
        ++count;
        if (trail == 0)
          trail = p.prepend(b.p(j));
        else
          trail = p.ins_after(trail, b.p(j));
        b.next(j);
      }
    }
  }
}


void <T>OSLSet::operator -= (<T>OSLSet& b)
{
  if (&b == this)
    clear();
  else if (count != 0 && b.count != 0)
  {
    Pix i = p.first();
    Pix j = b.p.first();
    Pix trail = 0;
    for (;;)
    {
      if (j == 0 || i == 0)
        return;
      int cmp = <T>CMP(p(i), b.p(j));
      if (cmp == 0)
      {
        --count;
        b.next(j);
        if (trail == 0)
        {
          p.del_front();
          i = p.first();
        }
        else
        {
          next(i);
          p.del_after(trail);
        }
      }
      else if (cmp < 0)
      {
        trail = i;
        next(i);
      }
      else
        b.next(j);
    }
  }
}

void <T>OSLSet::operator &= (<T>OSLSet& b)
{
  if (b.count == 0)
    clear();
  else if (&b != this && count != 0)
  {
    Pix i = p.first();
    Pix j = b.p.first();
    Pix trail = 0;
    for (;;)
    {
      if (i == 0)
        return;
      else if (j == 0)
      {
        if (trail == 0)
        {
          p.clear();
          count = 0;
        }
        else
        {
          while (i != 0)
          {
            --count;
            next(i);
            p.del_after(trail);
          }
        }
        return;
      }
      int cmp = <T>CMP(p(i), b.p(j));

      if (cmp == 0)
      {
        trail = i;
        next(i);
        b.next(j);
      }
      else if (cmp < 0)
      {
        --count;
        if (trail == 0)
        {
          p.del_front();
          i = p.first();
        }
        else
        {
          next(i);
          p.del_after(trail);
        }
      }
      else
        b.next(j);
    }
  }
}


int <T>OSLSet::OK()
{
  int v = p.OK();
  v &= count == p.length();
  Pix trail = p.first();
  if (trail == 0)
    v &= count == 0;
  else
  {
    Pix i = trail; next(i);
    while (i != 0)
    {
      v &= <T>CMP(p(trail), p(i)) < 0;
      trail = i;
      next(i);
    }
  }
  if (!v) error("invariant failure");
  return v;
}
