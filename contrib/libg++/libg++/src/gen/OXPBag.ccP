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
#include "<T>.OXPBag.h"


Pix <T>OXPBag::seek(<T&> item, Pix i)
{
  if (i == 0)
  {
    int l = p.low();
    int h = p.high();
    while (l <= h)
    {
      int mid = (l + h) / 2;
      int cmp = <T>CMP(item, p[mid]);
      if (cmp == 0)
      {
        while (mid > p.low() && <T>EQ(item, p[mid - 1])) --mid;
        return p.index_to_Pix(mid);
      }
      else if (cmp < 0)
        h = mid - 1;
      else
        l = mid + 1;
    }
    return 0;
  }
  int cmp = <T>CMP(item, p(i));
  if (cmp == 0)
  {
    next(i);
    return (<T>EQ(item, p(i)))? i : 0;
  }
  else if (cmp < 0)
  {
    int ind = p.Pix_to_index(i);
    int l = ind;
    int h = p.high();
    while (l <= h)
    {
      int mid = (l + h) / 2;
      cmp = <T>CMP(item, p[mid]);
      if (cmp == 0)
      {
        while (mid > ind && <T>EQ(item, p[mid - 1])) --mid;
        return p.index_to_Pix(mid);
      }
      else if (cmp < 0)
        h = mid - 1;
      else
        l = mid + 1;
    }
    return 0;
  }
  else
    return 0;
}

int <T>OXPBag::nof(<T&> item)
{
  int l = p.low();
  int h = p.high();
  while (l <= h)
  {
    int mid = (l + h) / 2;
    int cmp = <T>CMP(item, p[mid]);
    if (cmp == 0)
    {
      l = h = mid;
      while (l > p.low() && <T>EQ(item, p[l - 1])) --l;
      while (h < p.high() && <T>EQ(item, p[h + 1])) ++h;
      return h - l + 1;
    }
    else if (cmp < 0)
      h = mid - 1;
    else
      l = mid + 1;
  }
  return 0;
}

Pix <T>OXPBag::add(<T&> item)
{
  if (count == 0) 
  {
    ++count;
    return p.index_to_Pix(p.add_high(item));
  }
  int l = p.low();
  int h = p.high();
  while (l <= h)
  {
    int mid = (l + h) / 2;
    int cmp = <T>CMP(item, p[mid]);
    if (cmp == 0)
    {
      l = mid;
      break;
    }
    else if (cmp < 0)
      h = mid - 1;
    else
      l = mid + 1;
  }
  // add on whichever side is shortest
  ++count;
  if (l == p.fence())
    return p.index_to_Pix(p.add_high(item));
  else if (l == p.low())
    return p.index_to_Pix(p.add_low(item));
  else 
  {
    if (p.high() - l < l - p.low())
    {
      h = p.add_high(p.high_element());
      for (int i = h - 1; i > l; --i) p[i] = p[i-1];
    }
    else
    {
      --l;
      h = p.add_low(p.low_element());
      for (int i = h + 1; i < l; ++i) p[i] = p[i+1];
    }
    p[l] = item;
    return p.index_to_Pix(l);
  }
}

void <T>OXPBag::del(<T&> item)
{
  int l = p.low();
  int h = p.high();
  while (l <= h)
  {
    int mid = (l + h) / 2;
    int cmp = <T>CMP(item, p[mid]);
    if (cmp == 0)
    {
      --count;
      if (p.high() - mid < mid - p.low())
      {
        for (int i = mid; i < p.high(); ++i) p[i] = p[i+1];
        p.del_high();
      }
      else
      {
        for (int i = mid; i > p.low(); --i) p[i] = p[i-1];
        p.del_low();
      }
      return;
    }
    else if (cmp < 0)
      h = mid - 1;
    else
      l = mid + 1;
  }
}

void <T>OXPBag::remove(<T&> item)
{
  int l = p.low();
  int h = p.high();
  while (l <= h)
  {
    int mid = (l + h) / 2;
    int cmp = <T>CMP(item, p[mid]);
    if (cmp == 0)
    {
      l = h = mid;
      while (l > p.low() && <T>EQ(item, p[l - 1])) --l;
      while (h < p.high() && <T>EQ(item, p[h + 1])) ++h;
      int n = h - l + 1;
      count -= n;
      if (p.high() - h < l - p.low())
      {
        h = p.high() - n;
        for (int i = l; i <= h; ++i) p[i] = p[i+n];
        while (n-- > 0) p.del_high();
      }
      else
      {
        l = p.low() + n;
        for (int i = h; i >= l; --i) p[i] = p[i-n];
        while (n-- > 0) p.del_low();
      }
      return;
    }
    else if (cmp < 0)
      h = mid - 1;
    else
      l = mid + 1;
  }
}

int <T>OXPBag::OK()
{
  int v = p.OK();
  v &= count == p.length();
  for (int i = p.low(); i < p.high(); ++i) v &= <T>CMP(p[i], p[i+1]) <= 0;
  if (!v) error("invariant failure");
  return v;
}
