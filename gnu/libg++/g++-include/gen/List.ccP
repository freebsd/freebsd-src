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
#include <builtin.h>
#include "<T>.List.h"

<T>ListNode Nil<T>ListNode;

class init_Nil<T>ListNode
{
public:
  init_Nil<T>ListNode() 
  {
    Nil<T>ListNode.tl = &Nil<T>ListNode;
    Nil<T>ListNode.ref = -1;
  }
};

static init_Nil<T>ListNode Nil<T>ListNode_initializer;

<T>List& <T>List::operator = (<T>List& a)
{
  reference(a.P);
  dereference(P);
  P = a.P;
  return *this;
}

<T> <T>List::pop()
{
  <T> res = P->hd;
  <T>ListNode* tail = P->tl;
  reference(tail);
  dereference(P);
  P = tail;
  return res;
}

void <T>List::set_tail(<T>List& a)
{
  reference(a.P);
  dereference(P->tl);
  P->tl = a.P;
}

<T>List <T>List::nth(int n)
{
  for (<T>ListNode* p = P; n-- > 0; p = p->tl);
  reference(p);
  return <T>List(p);
}

<T>List <T>List::last()
{
  <T>ListNode* p = P;
  if (p != &Nil<T>ListNode) while (p->tl != &Nil<T>ListNode) p = p->tl;
  reference(p);
  return <T>List(p);
}

void <T>List::append(<T>List& l)
{
  <T>ListNode* p = P;
  <T>ListNode* a = l.P;
  reference(a);
  if (p != &Nil<T>ListNode)
  {
    while (p->tl != &Nil<T>ListNode) p = p->tl;
    p->tl = a;
  }
  else
    P = a;
}

int <T>List::length()
{
  int l = 0;
  for (<T>ListNode* p = P; p != &Nil<T>ListNode; p = p->tl) ++l;
  return l;
}

<T>&  <T>List::operator [] (int n)
{
  for (<T>ListNode* p = P; n-- > 0; p = p->tl);
  return (p->hd);
}

int operator == (<T>List& x, <T>List& y)
{
  <T>ListNode* a = x.P;
  <T>ListNode* b = y.P;
  
  for (;;)
  {
    if (a == &Nil<T>ListNode)
      return b == &Nil<T>ListNode;
    else if (b == &Nil<T>ListNode)
      return 0;
    else if (<T>EQ(a->hd, b->hd))
    {
      a = a->tl;
      b = b->tl;
    }
    else
      return 0;
  }
}


void <T>List::apply(<T>Procedure f)
{
  for(<T>ListNode* p = P; p != &Nil<T>ListNode; p = p->tl)
    (*f)((p->hd));
}

void <T>List::subst(<T&> old, <T&> repl)
{
  for(<T>ListNode* p = P; p != &Nil<T>ListNode; p = p->tl)
    if (<T>EQ(p->hd, old))
      p->hd = repl;
}

<T> <T>List::reduce(<T>Combiner f, <T&> base)
{
  <T> r = base;
  for(<T>ListNode* p = P; p != &Nil<T>ListNode; p = p->tl)
    r = (*f)(r, (p->hd));
  return r;
}

int <T>List::position(<T&> targ)
{
  int l = 0;
  <T>ListNode* p = P;
  for (;;)
  {
    if (p == &Nil<T>ListNode)
      return -1;
    else if (<T>EQ(p->hd, targ))
      return l;
    else
    {
      ++l;
      p = p->tl;
    }
  }
}

int <T>List::contains(<T&> targ)
{
  <T>ListNode* p = P;
  for (;;)
  {
    if (p == &Nil<T>ListNode)
      return 0;
    else if (<T>EQ(p->hd, targ))
      return 1;
    else
      p = p->tl;
  }
}

<T>List <T>List::find(<T&> targ)
{
  <T>ListNode* p = P;
  while (p != &Nil<T>ListNode && !<T>EQ(p->hd, targ))
    p=p->tl;
  reference(p);
  return <T>List(p);
}

Pix <T>List::seek(<T&> targ)
{
  <T>ListNode* p = P; 
  for (;;)
  {
    if (p == &Nil<T>ListNode)
      return 0;
    else if (<T>EQ(p->hd, targ))
      return Pix(p);
    else
      p = p->tl;
  }
}

int <T>List::owns(Pix i)
{
  <T>ListNode* p = P; 
  for (;;)
  {
    if (p == &Nil<T>ListNode)
      return 0;
    else if (Pix(p) == i)
      return 1;
    else
      p = p->tl;
  }
}

<T>List <T>List::find(<T>List& target)
{
  <T>ListNode* targ = target.P;
  if (targ == &Nil<T>ListNode)
    return <T>List(targ);

  <T>ListNode* p = P;
  while (p != &Nil<T>ListNode)
  {
    if (<T>EQ(p->hd, targ->hd))
    {
      <T>ListNode* a = p->tl;
      <T>ListNode* t = targ->tl;
      for(;;)
      {
        if (t == &Nil<T>ListNode)
        {
          reference(p);
          return <T>List(p);
        }
        else if (a == &Nil<T>ListNode || !<T>EQ(a->hd, t->hd))
          break;
        else
        {
          a = a->tl;
          t = t->tl;
        }
      }
    }
    p = p->tl;
  }
  return <T>List(&Nil<T>ListNode);
}

int <T>List::contains(<T>List& target)
{
  <T>ListNode* targ = target.P;
  if (targ == &Nil<T>ListNode)
    return 0;

  <T>ListNode* p = P;
  while (p != &Nil<T>ListNode)
  {
    if (<T>EQ(p->hd, targ->hd))
    {
      <T>ListNode* a = p->tl;
      <T>ListNode* t = targ->tl;
      for(;;)
      {
        if (t == &Nil<T>ListNode)
          return 1;
        else if (a == &Nil<T>ListNode || !<T>EQ(a->hd, t->hd))
          break;
        else
        {
          a = a->tl;
          t = t->tl;
        }
      }
    }
    p = p->tl;
  }
  return 0;
}

void <T>List::del(<T&> targ)
{
  <T>ListNode* h = P;

  for (;;)
  {
    if (h == &Nil<T>ListNode)
    {
      P = h;
      return;
    }
    else if (<T>EQ(h->hd, targ))
    {
      <T>ListNode* nxt = h->tl;
      reference(nxt);
      dereference(h);
      h = nxt;
    }
    else
      break;
  }

  <T>ListNode* trail = h;
  <T>ListNode* p = h->tl;
  while (p != &Nil<T>ListNode)
  {
    if (<T>EQ(p->hd, targ))
    {
      <T>ListNode* nxt = p->tl;
      reference(nxt);
      dereference(p);
      trail->tl = nxt;
      p = nxt;
    }
    else
    {
      trail = p;
      p = p->tl;
    }
  }
  P = h;
}

void <T>List::del(<T>Predicate f)
{
  <T>ListNode* h = P;
  for (;;)
  {
    if (h == &Nil<T>ListNode)
    {
      P = h;
      return;
    }
    else if ((*f)(h->hd))
    {
      <T>ListNode* nxt = h->tl;
      reference(nxt);
      dereference(h);
      h = nxt;
    }
    else
      break;
  }

  <T>ListNode* trail = h;
  <T>ListNode* p = h->tl;
  while (p != &Nil<T>ListNode)
  {
    if ((*f)(p->hd))
    {
      <T>ListNode* nxt = p->tl;
      reference(nxt);
      dereference(p);
      trail->tl = nxt;
      p = nxt;
    }
    else
    {
      trail = p;
      p = p->tl;
    }
  }
  P = h;
}

void <T>List::select(<T>Predicate f)
{
  <T>ListNode* h = P;
  for (;;)
  {
    if (h == &Nil<T>ListNode)
    {
      P = h;
      return;
    }
    else if (!(*f)(h->hd))
    {
      <T>ListNode* nxt = h->tl;
      reference(nxt);
      dereference(h);
      h = nxt;
    }
    else
      break;
  }
  <T>ListNode* trail = h;
  <T>ListNode* p = h->tl;
  while (p != &Nil<T>ListNode)
  {
    if (!(*f)(p->hd))
    {
      <T>ListNode* nxt = p->tl;
      reference(nxt);
      dereference(p);
      trail->tl = nxt;
      p = nxt;
    }
    else
    {
      trail = p;
      p = p->tl;
    }
  }
  P = h;
}

void <T>List::reverse()
{
  <T>ListNode* l = &Nil<T>ListNode;
  <T>ListNode* p = P; 
  while (p != &Nil<T>ListNode)
  {
    <T>ListNode* nxt = p->tl;
    p->tl = l;
    l = p;
    p = nxt;
  }
  P = l;
}


<T>List copy(<T>List& x)
{
  <T>ListNode* a = x.P;
  if (a == &Nil<T>ListNode)
    return <T>List(a);
  else
  {
    <T>ListNode* h = new<T>ListNode(a->hd);
    <T>ListNode* trail = h;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new<T>ListNode(a->hd);
      trail->tl = n;
      trail = n;
    }
    trail->tl = &Nil<T>ListNode;
    return <T>List(h);
  }
}


<T>List subst(<T&> old, <T&> repl, <T>List& x)
{
  <T>ListNode* a = x.P;
  if (a == &Nil<T>ListNode)
    return <T>List(a);
  else
  {
    <T>ListNode* h = new <T>ListNode;
    h->ref = 1;
    if (<T>EQ(a->hd, old))
      h->hd = repl;
    else
      h->hd = a->hd;
    <T>ListNode* trail = h;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new <T>ListNode;
      n->ref = 1;
      if (<T>EQ(a->hd, old))
        n->hd = repl;
      else
        n->hd = a->hd;
      trail->tl = n;
      trail = n;
    }
    trail->tl = &Nil<T>ListNode;
    return <T>List(h);
  }
}

<T>List combine(<T>Combiner f, <T>List& x, <T>List& y)
{
  <T>ListNode* a = x.P;
  <T>ListNode* b = y.P;
  if (a == &Nil<T>ListNode || b == &Nil<T>ListNode)
    return <T>List(&Nil<T>ListNode);
  else
  {
    <T>ListNode* h = new<T>ListNode((*f)(a->hd, b->hd));
    <T>ListNode* trail = h;
    a = a->tl;
    b = b->tl;
    while (a != &Nil<T>ListNode && b != &Nil<T>ListNode)
    {
      <T>ListNode* n = new<T>ListNode((*f)(a->hd, b->hd));
      trail->tl = n;
      trail = n;
      a = a->tl;
      b = b->tl;
    }
    trail->tl = &Nil<T>ListNode;
    return <T>List(h);
  }
}

<T>List reverse(<T>List& x)
{
  <T>ListNode* a = x.P;
  if (a == &Nil<T>ListNode)
    return <T>List(a);
  else
  {
    <T>ListNode* l = new<T>ListNode(a->hd);
    l->tl = &Nil<T>ListNode;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new<T>ListNode(a->hd);
      n->tl = l;
      l = n;
    }
    return <T>List(l);
  }
}

<T>List append(<T>List& x, <T>List& y)
{
  <T>ListNode* a = x.P;
  <T>ListNode* b = y.P;
  reference(b);
  if (a != &Nil<T>ListNode)
  {
    <T>ListNode* h = new<T>ListNode(a->hd);
    <T>ListNode* trail = h;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new<T>ListNode(a->hd);
      trail->tl = n;
      trail = n;
    }
    trail->tl = b;
    return <T>List(h);
  }
  else
    return <T>List(b);
}

void <T>List::prepend(<T>List& y)
{
  <T>ListNode* b = y.P;
  if (b != &Nil<T>ListNode)
  {
    <T>ListNode* h = new<T>ListNode(b->hd);
    <T>ListNode* trail = h;
    for(b = b->tl; b != &Nil<T>ListNode; b = b->tl)
    {
      <T>ListNode* n = new<T>ListNode(b->hd);
      trail->tl = n;
      trail = n;
    }
    trail->tl = P;
    P = h;
  }
}

<T>List concat(<T>List& x, <T>List& y)
{
  <T>ListNode* a = x.P;
  <T>ListNode* b = y.P;
  if (a != &Nil<T>ListNode)
  {
    <T>ListNode* h = new<T>ListNode(a->hd);
    <T>ListNode* trail = h;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new<T>ListNode(a->hd);
      trail->tl = n;
      trail = n;
    };
    for(;b != &Nil<T>ListNode; b = b->tl)
    {
      <T>ListNode* n = new<T>ListNode(b->hd);
      trail->tl = n;
      trail = n;
    }
    trail->tl = &Nil<T>ListNode;
    return <T>List(h);
  }
  else if (b != &Nil<T>ListNode)
  {
    <T>ListNode* h = new<T>ListNode(b->hd);
    <T>ListNode* trail = h;
    for(b = b->tl; b != &Nil<T>ListNode; b = b->tl)
    {
      <T>ListNode* n = new<T>ListNode(b->hd);
      trail->tl = n;
      trail = n;
    }
    trail->tl = &Nil<T>ListNode;
    return <T>List(h);
  }
  else
    return <T>List(&Nil<T>ListNode);
}

<T>List select(<T>Predicate f, <T>List& x)
{
  <T>ListNode* a = x.P;
  <T>ListNode* h = &Nil<T>ListNode;
  while (a != &Nil<T>ListNode)
  {
    if ((*f)(a->hd))
    {
      h = new<T>ListNode(a->hd);
      <T>ListNode* trail = h;
      for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
      {
        if ((*f)(a->hd))
        {
          <T>ListNode* n = new<T>ListNode(a->hd);
          trail->tl = n;
          trail = n;
        }
      }
      trail->tl = &Nil<T>ListNode;
      break;
    }
    else
      a = a->tl;
  }
  return <T>List(h);
}

<T>List remove(<T>Predicate f, <T>List& x)
{
  <T>ListNode* a = x.P;
  <T>ListNode* h = &Nil<T>ListNode;
  while (a != &Nil<T>ListNode)
  {
    if (!(*f)(a->hd))
    {
      h = new<T>ListNode(a->hd);
      <T>ListNode* trail = h;
      for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
      {
        if (!(*f)(a->hd))
        {
          <T>ListNode* n = new<T>ListNode(a->hd);
          trail->tl = n;
          trail = n;
        }
      }
      trail->tl = &Nil<T>ListNode;
      break;
    }
    else
      a = a->tl;
  }
  return <T>List(h);
}

<T>List remove(<T&> targ, <T>List& x)
{
  <T>ListNode* a = x.P;
  <T>ListNode* h = &Nil<T>ListNode;
  while (a != &Nil<T>ListNode)
  {
    if (!(<T>EQ(a->hd, targ)))
    {
      h = new<T>ListNode(a->hd);
      <T>ListNode* trail = h;
      for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
      {
        if (!<T>EQ(a->hd, targ))
        {
          <T>ListNode* n = new<T>ListNode(a->hd);
          trail->tl = n;
          trail = n;
        }
      }
      trail->tl = &Nil<T>ListNode;
      break;
    }
    else
      a = a->tl;
  }
  return <T>List(h);
}

<T>List map(<T>Mapper f, <T>List& x)
{
  <T>ListNode* a = x.P;
  <T>ListNode* h = &Nil<T>ListNode;
  if (a != &Nil<T>ListNode)
  {
    h = new<T>ListNode((*f)(a->hd));
    <T>ListNode* trail = h;
    for(a = a->tl; a != &Nil<T>ListNode; a = a->tl)
    {
      <T>ListNode* n = new<T>ListNode((*f)(a->hd));
      trail->tl = n;
      trail = n;
    }
    trail->tl = &Nil<T>ListNode;
  }
  return <T>List(h);
}


<T>List merge(<T>List& x, <T>List& y, <T>Comparator f)
{
  <T>ListNode* a = x.P;
  <T>ListNode* b = y.P;

  if (a == &Nil<T>ListNode)
  {
    if (b == &Nil<T>ListNode)
      return <T>List(&Nil<T>ListNode);
    else
      return copy(y);
  }
  else if (b == &Nil<T>ListNode)
    return copy(x);

  <T>ListNode* h = new <T>ListNode;
  h->ref = 1;
  if ((*f)(a->hd, b->hd) <= 0)
  {
    h->hd = a->hd;
    a = a->tl;
  }
  else
  {
    h->hd = b->hd;
    b = b->tl;
  }

  <T>ListNode* r = h;

  for(;;)
  {
    if (a == &Nil<T>ListNode)
    {
      while (b != &Nil<T>ListNode)
      {
        <T>ListNode* n = new <T>ListNode;
        n->ref = 1;
        n->hd = b->hd;
        r->tl = n;
        r = n;
        b = b->tl;
      }
      r->tl = &Nil<T>ListNode;
      return <T>List(h);
    }
    else if (b == &Nil<T>ListNode)
    {
      while (a != &Nil<T>ListNode)
      {
        <T>ListNode* n = new <T>ListNode;
        n->ref = 1;
        n->hd = a->hd;
        r->tl = n;
        r = n;
        a = a->tl;
      }
      r->tl = &Nil<T>ListNode;
      return <T>List(h);
    }
    else if ((*f)(a->hd, b->hd) <= 0)
    {
      <T>ListNode* n = new <T>ListNode;
      n->ref = 1;
      n->hd = a->hd;
      r->tl = n;
      r = n;
      a = a->tl;
    }
    else
    {
      <T>ListNode* n = new <T>ListNode;
      n->ref = 1;
      n->hd = b->hd;
      r->tl = n;
      r = n;
      b = b->tl;
    }
  }
}

void <T>List::sort(<T>Comparator f)
{
  // strategy: place runs in queue, merge runs until done
  // This is often very fast

  if (P == &Nil<T>ListNode || P->tl == &Nil<T>ListNode)
    return;

  int qlen = 250;   // guess a good queue size, realloc if necessary

  <T>ListNode** queue = (<T>ListNode**)malloc(qlen * sizeof(<T>ListNode*));

  <T>ListNode* h = P;
  <T>ListNode* a = h;
  <T>ListNode* b = a->tl;
  int qin = 0;

  while (b != &Nil<T>ListNode)
  {
    if ((*f)(a->hd, b->hd) > 0)
    {
      if (h == a)               // minor optimization: ensure runlen >= 2
      {
        h = b;
        a->tl = b->tl;
        b->tl = a;
        b = a->tl;
      }
      else
      {
        if (qin >= qlen)
        {
          qlen *= 2;
          queue = (<T>ListNode**)realloc(queue, qlen * sizeof(<T>ListNode*));
        }
        queue[qin++] = h;
        a->tl = &Nil<T>ListNode;
        h = a = b;
        b = b->tl;
      }
    }
    else
    {
      a = b;
      b = b->tl;
    }
  }

  int count = qin;
  queue[qin] = h;
  if (++qin >= qlen) qin = 0;
  int qout = 0;

  while (count-- > 0)
  {
    a = queue[qout];
    if (++qout >= qlen) qout = 0;
    b = queue[qout];
    if (++qout >= qlen) qout = 0;

    if ((*f)(a->hd, b->hd) <= 0)
    {
      h = a;
      a = a->tl;
    }
    else
    {
      h = b;
      b = b->tl;
    }
    queue[qin] = h;
    if (++qin >= qlen) qin = 0;

    for (;;)
    {
      if (a == &Nil<T>ListNode)
      {
        h->tl = b;
        break;
      }
      else if (b == &Nil<T>ListNode)
      {
        h->tl = a;
        break;
      }
      else if ((*f)(a->hd, b->hd) <= 0)
      {
        h->tl = a;
        h = a;
        a = a->tl;
      }
      else
      {
        h->tl = b;
        h = b;
        b = b->tl;
      }
    }
  }
  P = queue[qout];
  free(queue);
}
    
int <T>List::list_length()
{
  <T>ListNode* fast = P;
  if (fast == &Nil<T>ListNode)
    return 0;

  <T>ListNode* slow = fast->tl;
  if (slow == &Nil<T>ListNode)
    return 1;

  fast = slow->tl;
  int n = 2;

  for (;;)
  {
    if (fast == &Nil<T>ListNode)
      return n;
    else if (fast->tl == &Nil<T>ListNode)
      return n+1;
    else if (fast == slow)
      return -1;
    else
    {
      n += 2;
      fast = fast->tl->tl;
      slow = slow->tl;
    }
  }
}

void <T>List::error(const char* msg)
{
  (*lib_error_handler)("List", msg);
}

int <T>List::OK()
{
  int v = P != 0;               // have a node
  // check that all nodes OK, even if circular:

  <T>ListNode* fast = P;
  if (fast != &Nil<T>ListNode)
  {
    v &= fast->ref != 0;
    <T>ListNode* slow = fast->tl;
    v &= slow->ref != 0;
    if (v && slow != &Nil<T>ListNode)
    {
      fast = slow->tl;
      v &= fast->ref != 0;
      while (v)
      {
        if (fast == &Nil<T>ListNode)
          break;
        else if (fast->tl == &Nil<T>ListNode)
          break;
        else if (fast == slow)
          break;
        else
        {
          v &= fast->ref != 0 && slow->ref != 0;
          fast = fast->tl->tl;
          slow = slow->tl;
        }
      }
    }
  }
  if (!v) error ("invariant failure");
  return v;
}
