// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1988, 1992 Free Software Foundation
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

#ifndef _G_NO_TEMPLATES
#ifdef __GNUG__
//#pragma implementation
#endif
#include <values.h>
#include <stream.h>
#include <builtin.h>
#include "SLList.h"

void BaseSLList::error(const char* msg)
{
  (*lib_error_handler)("SLList", msg);
}

int BaseSLList::length()
{
  int l = 0;
  BaseSLNode* t = last;
  if (t != 0) do { ++l; t = t->tl; } while (t != last);
  return l;
}

void BaseSLList::clear()
{
  if (last == 0)
    return;

  BaseSLNode* p = last->tl;
  last->tl = 0;
  last = 0;

  while (p != 0)
  {
    BaseSLNode* nxt = p->tl;
    delete_node(p);
    p = nxt;
  }
}


// Note:  This is an internal method.  It does *not* free old contents!

void BaseSLList::copy(const BaseSLList& a)
{
  if (a.last == 0)
    last = 0;
  else
  {
    BaseSLNode* p = a.last->tl;
    BaseSLNode* h = copy_node(p->item());
    last = h;
    for (;;)
    {
      if (p == a.last)
      {
        last->tl = h;
        return;
      }
      p = p->tl;
      BaseSLNode* n = copy_node(p->item());
      last->tl = n;
      last = n;
    }
  }
}

BaseSLList& BaseSLList::operator = (const BaseSLList& a)
{
  if (last != a.last)
  {
    clear();
    copy(a);
  }
  return *this;
}

Pix BaseSLList::prepend(void *datum)
{
  return prepend(copy_node(datum));
}


Pix BaseSLList::prepend(BaseSLNode* t)
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


Pix BaseSLList::append(void *datum)
{
  return append(copy_node(datum));
}

Pix BaseSLList::append(BaseSLNode* t)
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

void BaseSLList::join(BaseSLList& b)
{
  BaseSLNode* t = b.last;
  b.last = 0;
  if (last == 0)
    last = t;
  else if (t != 0)
  {
    BaseSLNode* f = last->tl;
    last->tl = t->tl;
    t->tl = f;
    last = t;
  }
}

Pix BaseSLList::ins_after(Pix p, void *datum)
{
  BaseSLNode* u = (BaseSLNode*)p;
  BaseSLNode* t = copy_node(datum);
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

void BaseSLList::del_after(Pix p)
{
  BaseSLNode* u = (BaseSLNode*)p;
  if (last == 0 || u == last) error("cannot del_after last");
  if (u == 0) u = last; // del_after 0 means delete first
  BaseSLNode* t = u->tl;
  if (u == t)
    last = 0;
  else
  {
    u->tl = t->tl;
    if (last == t)
      last = u;
  }
  delete_node(t);
}

int BaseSLList::owns(Pix p)
{
  BaseSLNode* t = last;
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

int BaseSLList::remove_front(void *dst, int signal_error)
{
  if (last)
  {
    BaseSLNode* t = last->tl;
    copy_item(dst, t->item());
    if (t == last)
      last = 0;
    else
      last->tl = t->tl;
    delete_node(t);
    return 1;
  }
  if (signal_error)
    error("remove_front of empty list");
  return 0;
}

void BaseSLList::del_front()
{
  if (last == 0) error("del_front of empty list");
  BaseSLNode* t = last->tl;
  if (t == last)
    last = 0;
  else
    last->tl = t->tl;
  delete_node(t);
}

int BaseSLList::OK()
{
  int v = 1;
  if (last != 0)
  {
    BaseSLNode* t = last;
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
#endif /*!_G_NO_TEMPLATES*/
