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

#ifndef _G_NO_TEMPLATES
#ifdef __GNUG__
//#pragma implementation
#endif
#include <limits.h>
#include <stream.h>
#include <builtin.h>
#include "DLList.h"

void BaseDLList::error(const char* msg) const
{
  (*lib_error_handler)("DLList", msg);
}

int BaseDLList::length() const
{
  int l = 0;
  BaseDLNode* t = h;
  if (t != 0) do { ++l; t = t->fd; } while (t != h);
  return l;
}

// Note:  This is an internal method.  It does *not* free old contents!

void BaseDLList::copy(const BaseDLList& a)
{
  if (a.h == 0)
    h = 0;
  else
  {
    BaseDLNode* p = a.h;
    BaseDLNode* t = copy_node(p->item());
    h = t;
    p = p->fd;
    while (p != a.h)
    {
      BaseDLNode* n = copy_node(p->item());
      t->fd = n;
      n->bk = t;
      t = n;
      p = p->fd;
    }
    t->fd = h;
    h->bk = t;
    return;
  }
}

void BaseDLList::clear()
{
  if (h == 0)
    return;

  BaseDLNode* p = h->fd;
  h->fd = 0;
  h = 0;

  while (p != 0)
  {
    BaseDLNode* nxt = p->fd;
    delete_node(p);
    p = nxt;
  }
}

BaseDLList& BaseDLList::operator = (const BaseDLList& a)
{
  if (h != a.h)
  {
    clear();
    copy(a);
  }
  return *this;
}


Pix BaseDLList::prepend(const void *datum)
{
  BaseDLNode* t = copy_node(datum);
  if (h == 0)
    t->fd = t->bk = h = t;
  else
  {
    t->fd = h;
    t->bk = h->bk;
    h->bk->fd = t;
    h->bk = t;
    h = t;
  }
  return Pix(t);
}

Pix BaseDLList::append(const void *datum)
{
  BaseDLNode* t = copy_node(datum);
  if (h == 0)
    t->fd = t->bk = h = t;
  else
  {
    t->bk = h->bk;
    t->bk->fd = t;
    t->fd = h;
    h->bk = t;
  }
  return Pix(t);
}

Pix BaseDLList::ins_after(Pix p, const void *datum)
{
  if (p == 0) return prepend(datum);
  BaseDLNode* u = (BaseDLNode*) p;
  BaseDLNode* t = copy_node(datum);
  t->bk = u;
  t->fd = u->fd;
  u->fd->bk = t;
  u->fd = t;
  return Pix(t);
}

Pix BaseDLList::ins_before(Pix p, const void *datum)
{
  if (p == 0) error("null Pix");
  BaseDLNode* u = (BaseDLNode*) p;
  BaseDLNode* t = copy_node(datum);
  t->bk = u->bk;
  t->fd = u;
  u->bk->fd = t;
  u->bk = t;
  if (u == h) h = t;
  return Pix(t);
}

void BaseDLList::join(BaseDLList& b)
{
  BaseDLNode* t = b.h;
  b.h = 0;
  if (h == 0)
    h = t;
  else if (t != 0)
  {
    BaseDLNode* l = t->bk;
    h->bk->fd = t;
    t->bk = h->bk;
    h->bk = l;
    l->fd = h;
  }
}

int BaseDLList::owns(Pix p) const
{
  BaseDLNode* t = h;
  if (t != 0 && p != 0)
  {
    do
    {
      if (Pix(t) == p) return 1;
      t = t->fd;
    } while (t != h);
  }
  return 0;
}

void BaseDLList::del(Pix& p, int dir)
{
  if (p == 0) error("null Pix");
  BaseDLNode* t = (BaseDLNode*) p;
  if (t->fd == t)
  {
    h = 0;
    p = 0;
  }
  else
  {
    if (dir < 0)
    {
      if (t == h)
        p = 0;
      else
        p = Pix(t->bk);
    }
    else
    {
      if (t == h->bk)
        p = 0;
      else
        p = Pix(t->fd);
    }
    t->bk->fd = t->fd;
    t->fd->bk = t->bk;
    if (t == h) h = t->fd;
  }
  delete_node(t);
}

void BaseDLList::del_after(Pix& p)
{
  if (p == 0)
  {
    del_front();
    return;
  }

  BaseDLNode* b = (BaseDLNode*) p;
  BaseDLNode* t = b->fd;

  if (b == t)
  {
    h = 0;
    p = 0;
  }
  else
  {
    t->bk->fd = t->fd;
    t->fd->bk = t->bk;
    if (t == h) h = t->fd;
  }
  delete_node(t);
}

void BaseDLList::remove_front(void *dst)
{
  if (h == 0)
    error("remove_front of empty list");
  else {
      BaseDLNode* t = h;
      copy_item(dst, t->item());
      if (h->fd == h)
	  h = 0;
      else
	  {
	      h->fd->bk = h->bk;
	      h->bk->fd = h->fd;
	      h = h->fd;
	  }
      delete_node(t);
  }
}

void BaseDLList::del_front()
{
  if (h == 0)
    error("del_front of empty list");
  BaseDLNode* t = h;
  if (h->fd == h)
    h = 0;
  else
  {
    h->fd->bk = h->bk;
    h->bk->fd = h->fd;
    h = h->fd;
  }
  delete_node(t);
}

void BaseDLList::remove_rear(void *dst)
{
  if (h == 0)
    error("remove_rear of empty list");
  else
    {
      BaseDLNode* t = h->bk;
      copy_item(dst, t->item());
      if (h->fd == h)
	h = 0;
      else
	{
	  t->fd->bk = t->bk;
	  t->bk->fd = t->fd;
        }
      delete_node(t);
    }
}

void BaseDLList::del_rear()
{
  if (h == 0)
    error("del_rear of empty list");
  BaseDLNode* t = h->bk;
  if (h->fd == h)
    h = 0;
  else
  {
    t->fd->bk = t->bk;
    t->bk->fd = t->fd;
  }
  delete_node(t);
}


int BaseDLList::OK() const
{
  int v = 1;
  if (h != 0)
  {
    BaseDLNode* t = h;
    long count = LONG_MAX;      // Lots of chances to find h!
    do
    {
      count--;
      v &= t->bk->fd == t;
      v &= t->fd->bk == t;
      t = t->fd;
    } while (v && count > 0 && t != h);
    v &= count > 0;
  }
  if (!v) error("invariant failure");
  return v;
}
#endif
