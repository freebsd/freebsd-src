// This may look like C code, but it is really -*- C++ -*-
// WARNING: This file is obsolete.  Use ../SLList.cc, if you can.
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
#include <values.h>
#include <stream.h>
#include <builtin.h>
#include "<T>.SLList.h"

void <T>SLList::error(const char* msg)
{
  (*lib_error_handler)("SLList", msg);
}

int <T>SLList::length()
{
  int l = 0;
  <T>SLListNode* t = last;
  if (t != 0) do { ++l; t = t->tl; } while (t != last);
  return l;
}

<T>SLList::<T>SLList(const <T>SLList& a)
{
  if (a.last == 0)
    last = 0;
  else
  {
    <T>SLListNode* p = a.last->tl;
    <T>SLListNode* h = new <T>SLListNode(p->hd);
    last = h;
    for (;;)
    {
      if (p == a.last)
      {
        last->tl = h;
        return;
      }
      p = p->tl;
      <T>SLListNode* n = new <T>SLListNode(p->hd);
      last->tl = n;
      last = n;
    }
  }
}

<T>SLList& <T>SLList::operator = (const <T>SLList& a)
{
  if (last != a.last)
  {
    clear();
    if (a.last != 0)
    {
      <T>SLListNode* p = a.last->tl;
      <T>SLListNode* h = new <T>SLListNode(p->hd);
      last = h;
      for (;;)
      {
        if (p == a.last)
        {
          last->tl = h;
          break;
        }
        p = p->tl;
        <T>SLListNode* n = new <T>SLListNode(p->hd);
        last->tl = n;
        last = n;
      }
    }
  }
  return *this;
}

void <T>SLList::clear()
{
  if (last == 0)
    return;

  <T>SLListNode* p = last->tl;
  last->tl = 0;
  last = 0;

  while (p != 0)
  {
    <T>SLListNode* nxt = p->tl;
    delete(p);
    p = nxt;
  }
}


Pix <T>SLList::prepend(<T&> item)
{
  <T>SLListNode* t = new <T>SLListNode(item);
  if (last == 0)
    t->tl = last = t;
  else
  {
    t->tl = last->tl;
    last->tl = t;
  }
  return Pix(t);
}


Pix <T>SLList::prepend(<T>SLListNode* t)
{
  if (t == 0) return 0;
  if (last == 0)
    t->tl = last = t;
  else
  {
    t->tl = last->tl;
    last->tl = t;
  }
  return Pix(t);
}


Pix <T>SLList::append(<T&> item)
{
  <T>SLListNode* t = new <T>SLListNode(item);
  if (last == 0)
    t->tl = last = t;
  else
  {
    t->tl = last->tl;
    last->tl = t;
    last = t;
  }
  return Pix(t);
}

Pix <T>SLList::append(<T>SLListNode* t)
{
  if (t == 0) return 0;
  if (last == 0)
    t->tl = last = t;
  else
  {
    t->tl = last->tl;
    last->tl = t;
    last = t;
  }
  return Pix(t);
}

void <T>SLList::join(<T>SLList& b)
{
  <T>SLListNode* t = b.last;
  b.last = 0;
  if (last == 0)
    last = t;
  else if (t != 0)
  {
    <T>SLListNode* f = last->tl;
    last->tl = t->tl;
    t->tl = f;
    last = t;
  }
}

Pix <T>SLList::ins_after(Pix p, <T&> item)
{
  <T>SLListNode* u = (<T>SLListNode*)p;
  <T>SLListNode* t = new <T>SLListNode(item);
  if (last == 0)
    t->tl = last = t;
  else if (u == 0) // ins_after 0 means prepend
  {
    t->tl = last->tl;
    last->tl = t;
  }
  else
  {
    t->tl = u->tl;
    u->tl = t;
    if (u == last) 
      last = t;
  }
  return Pix(t);
}


void <T>SLList::del_after(Pix p)
{
  <T>SLListNode* u = (<T>SLListNode*)p;
  if (last == 0 || u == last) error("cannot del_after last");
  if (u == 0) u = last; // del_after 0 means delete first
  <T>SLListNode* t = u->tl;
  if (u == t)
    last = 0;
  else
  {
    u->tl = t->tl;
    if (last == t)
      last = u;
  }
  delete t;
}

int <T>SLList::owns(Pix p)
{
  <T>SLListNode* t = last;
  if (t != 0 && p != 0)
  {
    do
    {
      if (Pix(t) == p) return 1;
      t = t->tl;
    } while (t != last);
  }
  return 0;
}

<T> <T>SLList::remove_front()
{
  if (last == 0) error("remove_front of empty list");
  <T>SLListNode* t = last->tl;
  <T> res = t->hd;
  if (t == last)
    last = 0;
  else
    last->tl = t->tl;
  delete t;
  return res;
}

int <T>SLList::remove_front(<T>& x)
{
  if (last == 0)
    return 0;
  else
  {
    <T>SLListNode* t = last->tl;
    x = t->hd;
    if (t == last)
      last = 0;
    else
      last->tl = t->tl;
    delete t;
    return 1;
  }
}


void <T>SLList::del_front()
{
  if (last == 0) error("del_front of empty list");
  <T>SLListNode* t = last->tl;
  if (t == last)
    last = 0;
  else
    last->tl = t->tl;
  delete t;
}

int <T>SLList::OK()
{
  int v = 1;
  if (last != 0)
  {
    <T>SLListNode* t = last;
    long count = MAXLONG;      // Lots of chances to find last!
    do
    {
      count--;
      t = t->tl;
    } while (count > 0 && t != last);
    v &= count > 0;
  }
  if (!v) error("invariant failure");
  return v;
}
