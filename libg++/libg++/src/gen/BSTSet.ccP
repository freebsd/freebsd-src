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
#include <stream.h>
#include "<T>.BSTSet.h"


/*
 traversal primitives
*/


<T>BSTNode* <T>BSTSet::leftmost()
{
  <T>BSTNode* t = root;
  if (t != 0) while (t->lt != 0) t = t->lt;
  return t;
}

<T>BSTNode* <T>BSTSet::rightmost()
{
  <T>BSTNode* t = root;
  if (t != 0) while (t->rt != 0) t = t->rt;
  return t;
}

<T>BSTNode* <T>BSTSet::succ(<T>BSTNode* t)
{
  if (t == 0)
    return 0;
  if (t->rt != 0)
  {
    t = t->rt;
    while (t->lt != 0) t = t->lt;
    return t;
  }
  else
  {
    for (;;)
    {
      if (t->par == 0 || t == t->par->lt)
        return t->par;
      else
        t = t->par;
    }
  }
}

<T>BSTNode* <T>BSTSet::pred(<T>BSTNode* t)
{
  if (t == 0)
    return 0;
  else if (t->lt != 0)
  {
    t = t->lt;
    while (t->rt != 0) t = t->rt;
    return t;
  }
  else
  {
    for (;;)
    {
      if (t->par == 0 || t == t->par->rt)
        return t->par;
      else
        t = t->par;
    }
  }
}


Pix <T>BSTSet::seek(<T&> key)
{
  <T>BSTNode* t = root;
  for (;;)
  {
    if (t == 0)
      return 0;
    int comp = <T>CMP(key, t->item);
    if (comp == 0)
      return Pix(t);
    else if (comp < 0)
      t = t->lt;
    else
      t = t->rt;
  }
}


Pix <T>BSTSet::add(<T&> item)
{
  if (root == 0)
  {
    ++count;
    root = new <T>BSTNode(item);
    return Pix(root);
  }

  <T>BSTNode* t = root;
  <T>BSTNode* p = root;
  for (;;)
  {
    int comp = <T>CMP(t->item, item);
    if (comp == 0)
      return Pix(t);
    else if (comp > 0)
      t = t->lt;
    else
      t = t->rt;
    if (t == 0)
    {
      ++count;
      t = new <T>BSTNode(item);
      if (comp > 0)
        p->lt = t;
      else
        p->rt = t;
      t->par = p;
      return Pix(t);
    }
    p = t;
  }
}


void <T>BSTSet::del(<T&> key)
{
  <T>BSTNode* t = root;
  <T>BSTNode* p = root;
  int comp;
  for (;;)
  {
    if (t == 0)
      return;
    comp = <T>CMP(key, t->item);
    if (comp == 0)
    {
      --count;
      <T>BSTNode* repl;
      if (t->lt == 0)
        repl = t->rt;
      else if (t->rt == 0)
        repl = t->lt;
      else
      {
        <T>BSTNode* prepl = t;
        repl = t->lt;
        while (repl->rt != 0)
        {
          prepl = repl;
          repl = repl->rt;
        }
        if (prepl != t)
        {
          prepl->rt = repl->lt;
          if (prepl->rt != 0) prepl->rt->par = prepl;
          repl->lt = t->lt;
          if (repl->lt != 0) repl->lt->par = repl;
        }
        repl->rt = t->rt;
        if (repl->rt != 0) repl->rt->par = repl;
      }
      if (t == root)
      {
        root = repl;
        if (repl != 0) repl->par = 0;
      }
      else
      {
        if (t == p->lt)
          p->lt = repl;
        else
          p->rt = repl;
        if (repl != 0) repl->par = p;
      }
      delete t;
      return;
    }
    p = t;
    if (comp < 0)
      t = t->lt;
    else
      t = t->rt;
  }
}


void <T>BSTSet::_kill(<T>BSTNode* t)
{
  if (t != 0)
  {
    _kill(t->lt);
    _kill(t->rt);
    delete t;
  }
}

<T>BSTNode* <T>BSTSet::_copy(<T>BSTNode* t)
{
  if (t == 0)
    return 0;
  else
  {
    <T>BSTNode* u = new <T>BSTNode(t->item, _copy(t->lt), _copy(t->rt));
    if (u->lt != 0) u->lt->par = u;
    if (u->rt != 0) u->rt->par = u;
    return u;
  }
}


int <T>BSTSet::operator == (<T>BSTSet& y)
{
  if (count != y.count)
    return 0;
  else
  {
    <T>BSTNode* t = leftmost();
    <T>BSTNode* u = y.leftmost();
    for (;;)
    {
      if (t == 0)
        return 1;
      else if (!<T>EQ(t->item, u->item))
        return 0;
      else
      {
        t = succ(t);
        u = y.succ(u);
      }
    }
  }
}

int <T>BSTSet::operator <= (<T>BSTSet& y)
{
  if (count > y.count)
    return 0;
  else
  {
    <T>BSTNode* t = leftmost();
    <T>BSTNode* u = y.leftmost();
    for (;;)
    {
      if (t == 0)
        return 1;
      else if (u == 0)
        return 0;
      int cmp = <T>CMP(t->item, u->item);
      if (cmp == 0)
      {
        t = succ(t);
        u = y.succ(u);
      }
      else if (cmp < 0)
        return 0;
      else
        u = y.succ(u);
    }
  }
}


// linear-time, zero space overhead binary tree rebalancing from
// Stout & Warren, ``Tree rebalancing in linear space and time''
// CACM, Sept, 1986, p902.


void <T>BSTSet::balance()
{
  if (count <= 2) return; // don't bother -- 
                          // also we assume non-null root, below

  // make re-attaching the root easy via trickery

  struct _fake_node { _fake_node *lt, *rt, *par; } fake_root;

  fake_root.rt = (_fake_node*)root; 
  fake_root.par = 0;
  <T>BSTNode* pseudo_root = (<T>BSTNode*)&fake_root;

  // phase 1: tree-to-vine

  <T>BSTNode* vine_tail = pseudo_root;
  <T>BSTNode* remainder = root;

  while (remainder != 0)
  {
    if (remainder->lt == 0)
    {
      vine_tail = remainder;
      remainder = remainder->rt;
    }
    else
    {
      <T>BSTNode* tmp = remainder->lt;
      remainder->lt = tmp->rt;
      if (remainder->lt != 0) remainder->lt->par = remainder;
      tmp->rt = remainder;
      remainder->par = tmp;
      vine_tail->rt = remainder = tmp;
    }
  }

  // phase 2: vine-to-tree
  
  // Uses the slightly simpler version adapted from
  // Day ``Balancing a binary tree'' Computer Journal, Nov. 1976,
  // since it's not generally important whether the `stray' leaves are
  // on the left or on the right.

  unsigned int spines = count - 1;
  while (spines > 1)
  {
    int compressions = spines >> 1; // compress every other node
    spines -= compressions + 1;     // halve for next time

    <T>BSTNode* scanner = pseudo_root;
    while (compressions-- > 0)
    {
      <T>BSTNode* child = scanner->rt;
      <T>BSTNode* grandchild = child->rt;
      scanner->rt = grandchild;
      grandchild->par = scanner;
      child->rt = grandchild->lt;
      if (child->rt != 0) child->rt->par = child;
      grandchild->lt = child;
      child->par = grandchild;
      scanner = grandchild;
    }
  }

  root = pseudo_root->rt;
  root->par = 0;
}


int <T>BSTSet::OK()
{
  int v = 1;
  if (root == 0) 
    v = count == 0;
  else
  {
    int n = 1;
    <T>BSTNode* trail = leftmost();
    <T>BSTNode* t = succ(trail);
    while (t != 0)
    {
      ++n;
      v &= <T>CMP(trail->item, t->item) < 0;
      trail = t;
      t = succ(t);
    }
    v &= n == count;
  }
  if (!v) error("invariant failure");
  return v;
}
