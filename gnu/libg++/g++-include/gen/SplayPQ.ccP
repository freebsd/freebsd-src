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
#include <stream.h>
#include "<T>.SplayPQ.h"


/* 

 struct to simulate the special `null' node in the Sleater & Tarjan JACM 1985
 splay tree algorithms

 All routines use a version of their `simple top-down' splay alg. (p 669)

*/

struct _dummySplayNode
{
  <T>SplayNode*    lt;
  <T>SplayNode*    rt;
  <T>SplayNode*    par;
} _dummy_null;


/*
 traversal primitives
*/


<T>SplayNode* <T>SplayPQ::leftmost()
{
  <T>SplayNode* t = root;
  if (t != 0) while (t->lt != 0) t = t->lt;
  return t;
}

<T>SplayNode* <T>SplayPQ::rightmost()
{
  <T>SplayNode* t = root;
  if (t != 0) while (t->rt != 0) t = t->rt;
  return t;
}

<T>SplayNode* <T>SplayPQ::succ(<T>SplayNode* t)
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

<T>SplayNode* <T>SplayPQ::pred(<T>SplayNode* t)
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


Pix <T>SplayPQ::seek(<T&> key)
{
  <T>SplayNode* t = root;
  if (t == 0)
    return 0;

  int comp = <T>CMP(key, t->item);
  if (comp == 0)
    return Pix(t);

  <T>SplayNode* dummy = (<T>SplayNode*)(&_dummy_null);
  <T>SplayNode* l = dummy;
  <T>SplayNode* r = dummy;
  dummy->rt = dummy->lt = dummy->par = 0;

  while (comp != 0)
  {
    if (comp > 0)
    {
      <T>SplayNode* tr = t->rt;
      if (tr == 0)
        break;
      else
      {
        comp = <T>CMP(key, tr->item);
        if (comp <= 0 || tr->rt == 0)
        {
          l->rt = t; t->par = l;
          l = t;
          t = tr;
          if (comp >= 0)
            break;
        }
        else
        {
          if ((t->rt = tr->lt) != 0) t->rt->par = t;
          tr->lt = t; t->par = tr;
          l->rt = tr; tr->par = l;
          l = tr;
          t = tr->rt;
          comp = <T>CMP(key, t->item);
        }
      }
    }
    else
    {
      <T>SplayNode* tl = t->lt;
      if (tl == 0)
        break;
      else
      {
        comp = <T>CMP(key, tl->item);
        if (comp >= 0 || tl->lt == 0)
        {
          r->lt = t; t->par = r;
          r = t;
          t = tl;
          if (comp <= 0)
            break;
        }
        else
        {
          if ((t->lt = tl->rt) != 0) t->lt->par = t;
          tl->rt = t; t->par = tl;
          r->lt = tl; tl->par = r;
          r = tl;
          t = tl->lt;
          comp = <T>CMP(key, t->item);
        }
      }
    }
  }
  if ((r->lt = t->rt) != 0) r->lt->par = r;
  if ((l->rt = t->lt) != 0) l->rt->par = l;
  if ((t->lt = dummy->rt) != 0) t->lt->par = t;
  if ((t->rt = dummy->lt) != 0) t->rt->par = t;
  t->par = 0;
  root = t;
  return (comp == 0) ? Pix(t) : 0;
}


Pix <T>SplayPQ::enq(<T&> item)
{
  ++count;
  <T>SplayNode* newnode = new <T>SplayNode(item);
  <T>SplayNode* t = root;
  if (t == 0)
  {
    root = newnode;
    return Pix(root);
  }

  int comp = <T>CMP(item, t->item);

  <T>SplayNode* dummy = (<T>SplayNode*)(&_dummy_null);
  <T>SplayNode* l = dummy;
  <T>SplayNode* r = dummy;
  dummy->rt = dummy->lt = dummy->par = 0;
  
  int done = 0;
  while (!done)
  {
    if (comp >= 0)
    {
      <T>SplayNode* tr = t->rt;
      if (tr == 0)
      {
        tr = newnode;
        comp = 0; done = 1;
      }
      else
        comp = <T>CMP(item, tr->item);
        
      if (comp <= 0)
      {
        l->rt = t; t->par = l;
        l = t;
        t = tr;
      }
      else 
      {
        <T>SplayNode* trr = tr->rt;
        if (trr == 0)
        {
          trr =  newnode;
          comp = 0; done = 1;
        }
        else
          comp = <T>CMP(item, trr->item);

        if ((t->rt = tr->lt) != 0) t->rt->par = t;
        tr->lt = t; t->par = tr;
        l->rt = tr; tr->par = l;
        l = tr;
        t = trr;
      }
    }
    else
    {
      <T>SplayNode* tl = t->lt;
      if (tl == 0)
      {
        tl = newnode;
        comp = 0; done = 1;
      }
      else
        comp = <T>CMP(item, tl->item);

      if (comp >= 0)
      {
        r->lt = t; t->par = r;
        r = t;
        t = tl;
      }
      else
      {
        <T>SplayNode* tll = tl->lt;
        if (tll == 0)
        {
          tll = newnode;
          comp = 0; done = 1;
        }
        else
          comp = <T>CMP(item, tll->item);

        if ((t->lt = tl->rt) != 0) t->lt->par = t;
        tl->rt = t; t->par = tl;
        r->lt = tl; tl->par = r;
        r = tl;
        t = tll;
      }
    }
  }
  if ((r->lt = t->rt) != 0) r->lt->par = r;
  if ((l->rt = t->lt) != 0) l->rt->par = l;
  if ((t->lt = dummy->rt) != 0) t->lt->par = t;
  if ((t->rt = dummy->lt) != 0) t->rt->par = t;
  t->par = 0;
  root = t;
  return Pix(root);
}


void <T>SplayPQ::del(Pix pix)
{
  <T>SplayNode* t = (<T>SplayNode*)pix;
  if (t == 0) return;

  <T>SplayNode* p = t->par;

  --count;
  if (t->rt == 0)
  {
    if (t == root)
    {
      if ((root = t->lt) != 0) root->par = 0;
    }
    else if (t == p->lt)
    {
      if ((p->lt = t->lt) != 0) p->lt->par = p;
    }
    else
      if ((p->rt = t->lt) != 0) p->rt->par = p;
  }
  else
  {
    <T>SplayNode* r = t->rt;
    <T>SplayNode* l = r->lt;
    for(;;)
    {
      if (l == 0)
      {
        if (t == root)
        {
          root = r;
          r->par = 0;
        }
        else if (t == p->lt) 
        {
          p->lt = r;
          r->par = p;
        }
        else
        {
          p->rt = r;
          r->par = p;
        }
        if ((r->lt = t->lt) != 0) r->lt->par = r;
        break;
      }
      else
      {
        if ((r->lt = l->rt) != 0) r->lt->par = r;
        l->rt = r; r->par = l;
        r = l;
        l = l->lt;
      }
    }
  }
  delete t;
}

<T>& <T>SplayPQ::front()
{
  if (root == 0)
    error ("min: empty tree\n");
//  else 
  {
    <T>SplayNode* t = root;
    <T>SplayNode* l = root->lt;
    for(;;)
    {
      if (l == 0)
      {
        root = t;
        root->par = 0;
        return root->item;
      }
      else
      {
        if ((t->lt = l->rt) != 0) t->lt->par = t;
        l->rt = t; t->par = l;
        t = l;
        l = l->lt;
      }
    }
  }
}

void <T>SplayPQ::del_front()
{
  if (root != 0)
  {
    --count;
    <T>SplayNode* t = root;
    <T>SplayNode* l = root->lt;
    if (l == 0)
    {
      if ((root = t->rt) != 0) root->par = 0;
      delete t;
    }
    else
    {
      for(;;)
      {
        <T>SplayNode* ll = l->lt;
        if (ll == 0)
        {
          if ((t->lt = l->rt) != 0) t->lt->par = t;
          delete l;
          break;
        }
        else
        {
          <T>SplayNode* lll = ll->lt;
          if (lll == 0)
          {
            if ((l->lt = ll->rt) != 0) l->lt->par = l;
            delete ll;
            break;
          }
          else
          {
            t->lt = ll; ll->par = t;
            if ((l->lt = ll->rt) != 0) l->lt->par = l;
            ll->rt = l; l->par = ll;
            t = ll;
            l = lll;
          }
        }
      }
    }
  }
}

<T> <T>SplayPQ::deq()
{
  if (root == 0)
    error("deq: empty tree");
//  else
  {
    --count;
    <T>SplayNode* t = root;
    <T>SplayNode* l = root->lt;
    if (l == 0)
    {
      if ((root = t->rt) != 0) root->par = 0;
      <T> res = t->item;
      delete t;
      return res;
    }
    else
    {
      for(;;)
      {
        <T>SplayNode* ll = l->lt;
        if (ll == 0)
        {
          if ((t->lt = l->rt) != 0) t->lt->par = t;
          <T> res = l->item;
          delete l;
          return res;
        }
        else
        {
          <T>SplayNode* lll = ll->lt;
          if (lll == 0)
          {
            if ((l->lt = ll->rt) != 0) l->lt->par = l;
            <T> res = ll->item;
            delete ll;
            return res;
          }
          else
          {
            t->lt = ll; ll->par = t;
            if ((l->lt = ll->rt) != 0) l->lt->par = l;
            ll->rt = l; l->par = ll;
            t = ll;
            l = lll;
          }
        }
      }
    }
  }
}


void <T>SplayPQ::_kill(<T>SplayNode* t)
{
  if (t != 0)
  {
    _kill(t->lt);
    _kill(t->rt);
    delete t;
  }
}


<T>SplayNode* <T>SplayPQ::_copy(<T>SplayNode* t)
{
  if (t != 0)
  {
    <T>SplayNode* l = _copy(t->lt);
    <T>SplayNode* r = _copy(t->rt);
    <T>SplayNode* x = new <T>SplayNode(t->item, l, r);
    if (l != 0) l->par = x;
    if (r != 0) r->par = x;
    return x;
  }
  else 
    return 0;
}

int <T>SplayPQ::OK()
{
  int v = 1;
  if (root == 0) 
    v = count == 0;
  else
  {
    int n = 1;
    <T>SplayNode* trail = leftmost();
    <T>SplayNode* t = succ(trail);
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
