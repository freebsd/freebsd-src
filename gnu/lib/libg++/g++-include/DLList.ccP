// This may look like C code, but it is really -*- C++ -*-
// WARNING: This file is obsolete.  Use ../DLList.cc, if you can.
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
#include "<T>.DLList.h"

// error handling



void <T>DLList::error(const char* msg)
{
  (*lib_error_handler)("DLList", msg);
}

int <T>DLList::length()
{
  int l = 0;
  <T>DLListNode* t = h;
  if (t != 0) do { ++l; t = t->fd; } while (t != h);
  return l;
}

<T>DLList::<T>DLList(const <T>DLList& a)
{
  if (a.h == 0)
    h = 0;
  else
  {
    <T>DLListNode* p = a.h;
    <T>DLListNode* t = new <T>DLListNode(p->hd);
    h = t;
    p = p->fd;
    while (p != a.h)
    {
      <T>DLListNode* n = new <T>DLListNode(p->hd);
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

<T>DLList& <T>DLList::operator = (const <T>DLList& a)
{
  if (h != a.h)
  {
    clear();
    if (a.h != 0)
    {
      <T>DLListNode* p = a.h;
      <T>DLListNode* t = new <T>DLListNode(p->hd);
      h = t;
      p = p->fd;
      while (p != a.h)
      {
        <T>DLListNode* n = new <T>DLListNode(p->hd);
        t->fd = n;
        n->bk = t;
        t = n;
        p = p->fd;
      }
      t->fd = h;
      h->bk = t;
    }
  }
  return *this;
}

void <T>DLList::clear()
{
  if (h == 0)
    return;

  <T>DLListNode* p = h->fd;
  h->fd = 0;
  h = 0;

  while (p != 0)
  {
    <T>DLListNode* nxt = p->fd;
    delete(p);
    p = nxt;
  }
}


Pix <T>DLList::prepend(<T&> item)
{
  <T>DLListNode* t = new <T>DLListNode(item);
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

Pix <T>DLList::append(<T&> item)
{
  <T>DLListNode* t = new <T>DLListNode(item);
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

Pix <T>DLList::ins_after(Pix p, <T&> item)
{
  if (p == 0) return prepend(item);
  <T>DLListNode* u = (<T>DLListNode*) p;
  <T>DLListNode* t = new <T>DLListNode(item, u, u->fd);
  u->fd->bk = t;
  u->fd = t;
  return Pix(t);
}

Pix <T>DLList::ins_before(Pix p, <T&> item)
{
  if (p == 0) error("null Pix");
  <T>DLListNode* u = (<T>DLListNode*) p;
  <T>DLListNode* t = new <T>DLListNode(item, u->bk, u);
  u->bk->fd = t;
  u->bk = t;
  if (u == h) h = t;
  return Pix(t);
}

void <T>DLList::join(<T>DLList& b)
{
  <T>DLListNode* t = b.h;
  b.h = 0;
  if (h == 0)
    h = t;
  else if (t != 0)
  {
    <T>DLListNode* l = t->bk;
    h->bk->fd = t;
    t->bk = h->bk;
    h->bk = l;
    l->fd = h;
  }
}

int <T>DLList::owns(Pix p)
{
  <T>DLListNode* t = h;
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

void <T>DLList::del(Pix& p, int dir)
{
  if (p == 0) error("null Pix");
  <T>DLListNode* t = (<T>DLListNode*) p;
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
  delete t;
}

void <T>DLList::del_after(Pix& p)
{
  if (p == 0)
  {
    del_front();
    return;
  }

  <T>DLListNode* b = (<T>DLListNode*) p;
  <T>DLListNode* t = b->fd;

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
  delete t;
}

<T> <T>DLList::remove_front()
{
  if (h == 0)
    error("remove_front of empty list");
  <T>DLListNode* t = h;
  <T> res = t->hd;
  if (h->fd == h)
    h = 0;
  else
  {
    h->fd->bk = h->bk;
    h->bk->fd = h->fd;
    h = h->fd;
  }
  delete t;
  return res;
}


void <T>DLList::del_front()
{
  if (h == 0)
    error("del_front of empty list");
  <T>DLListNode* t = h;
  if (h->fd == h)
    h = 0;
  else
  {
    h->fd->bk = h->bk;
    h->bk->fd = h->fd;
    h = h->fd;
  }
  delete t;
}

<T> <T>DLList::remove_rear()
{
  if (h == 0)
    error("remove_rear of empty list");
  <T>DLListNode* t = h->bk;
  <T> res = t->hd;
  if (h->fd == h)
    h = 0;
  else
  {
    t->fd->bk = t->bk;
    t->bk->fd = t->fd;
  }
  delete t;
  return res;
}


void <T>DLList::del_rear()
{
  if (h == 0)
    error("del_rear of empty list");
  <T>DLListNode* t = h->bk;
  if (h->fd == h)
    h = 0;
  else
  {
    t->fd->bk = t->bk;
    t->bk->fd = t->fd;
  }
  delete t;
}


int <T>DLList::OK()
{
  int v = 1;
  if (h != 0)
  {
    <T>DLListNode* t = h;
    long count = MAXLONG;      // Lots of chances to find h!
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
