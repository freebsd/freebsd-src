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
#include "<T>.XPPQ.h"

int <T>XPPQ::OK()
{
  int v = p.OK();
  v &= p.low() == 1;
  v &= count == p.length();
  if (!v) error("invariant failure");
  return v;
}

Pix <T>XPPQ::seek(<T&> item)
{
  for (int i = p.low(); i < p.fence(); p.next(i))
    if (<T>EQ(p[i],item)) return p.index_to_Pix(i);
  return 0;
}

// standard 2-ary heap ops
// pointers are used a lot to avoid thrashing across chunks with plexes

Pix <T>XPPQ::enq(<T&> item)
{
  p.add_high(item);
  <T>* pk = &(p.high_element());
  int par = ++count >> 1;
  while (par != 0)
  {
    <T>* ppar = &(p[par]);
    if (!(<T>LE(*ppar, item)))
    {
      *pk = *ppar;
      pk = ppar;
      par >>= 1;
    }
    else
      break;
  }
  *pk = item;
  return Pix(pk);
}

void <T>XPPQ::del_front()
{
  if (count == 0) error("empty PQ");
  --count;
  <T>* pk = &(p.low_element());
  <T>* ph = &(p.high_element());
  int child = 2;
  while (child <= count)
  {
    <T>* pchild = &(p[child]);
    if (child < count)
    {
      <T>* prchild = &(p[child+1]);
      if (!(<T>LE(*pchild, *prchild)))
      {
        pchild = prchild;
        ++child;
      }
    }
    if (!(<T>LE(*ph, *pchild)))
    {
      *pk = *pchild;
      pk = pchild;
      child <<= 1;
    }
    else
      break;
  }
  *pk = *ph;
  p.del_high();
}


void <T>XPPQ::del(Pix i)
{
  if (i == 0) error("null Pix");
  --count;
  int k = p.Pix_to_index(i);
  <T>* pk = &(p[k]);
  <T>* ph = &(p.high_element());
  int child = k << 1;
  while (child <= count)
  {
    <T>* pchild = &(p[child]);
    if (child < count)
    {
      <T>* prchild = &(p[child+1]);
      if (!(<T>LE(*pchild, *prchild)))
      {
        pchild = prchild;
        ++child;
      }
    }
    if (!(<T>LE(*ph, *pchild)))
    {
      *pk = *pchild;
      pk = pchild;
      child <<= 1;
    }
    else
      break;
  }
  int par = child >> 2;
  while (par != 0)
  {
    <T>* ppar = &(p[par]);
    if (!(<T>LE(*ppar, *ph)))
    {
      *pk = *ppar;
      pk = ppar;
      par >>= 1;
    }
    else
      break;
  }
  *pk = *ph;
  p.del_high();
}


