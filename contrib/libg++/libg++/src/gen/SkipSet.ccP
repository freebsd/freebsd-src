// This may look like C code, but it is really -*- C++ -*-
/* 
Copyright (C) 1991 Free Software Foundation

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

/*
 * Sets implemented via William Pugh SkipList algorithms.
 * CACM, June 1990, p 668-676.
 *
 */

#include <stream.h>
#include <time.h>

#include "<T>.SkipSet.h"

MLCG* <T>SkipSet::gen = 0;
int <T>SkipSetinit::count = 0;

static int countbits(long bits)
{
   int n = 0;
   while(bits>>=1L) n++;
   return n;
}

<T>SkipSet::<T>SkipSet(long size)
: level(0),
  header(new <T>SkipSetNode (countbits(size)+1)),
  max_levels (countbits(size)+1),
  random_bits(gen->asLong()),
  randoms_left(BITS_IN_RANDOM / 2)
{
  <T>SkipSetNodePtr *buffer_start = header->forward;
  <T>SkipSetNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  while (trav > buffer_start)
    *--trav = (<T>SkipSetNodePtr) header;
}

<T>SkipSet::<T>SkipSet(<T>SkipSet& b)
: level (0),
  header (new <T>SkipSetNode (b.max_levels)),
  max_levels (b.max_levels),
  random_bits (gen->asLong()),
  randoms_left (BITS_IN_RANDOM / 2)
{
  <T>SkipSetNodePtr *buffer_start = header->forward;
  <T>SkipSetNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  
   while (trav > buffer_start)
     *--trav = (<T>SkipSetNodePtr)header;
  
  for (<T>SkipSetNodePtr t = b.leftmost(); t; t = b.succ(t))
      add(t->item);
}

/* relationals */

int <T>SkipSet::operator == (<T>SkipSet& y)
{
  if (count != y.count)
    return 0;
  else
  {
    <T>SkipSetNodePtr t = leftmost();
    <T>SkipSetNodePtr u = y.leftmost();
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

int <T>SkipSet::operator <= (<T>SkipSet& y)
{
  if (count > y.count)
    return 0;
  else
  {
    <T>SkipSetNodePtr t = leftmost();
    <T>SkipSetNodePtr u = y.leftmost();
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


void <T>SkipSet::operator |=(<T>SkipSet& y)
{
  if (&y == this) return;
  <T>SkipSetNodePtr u = y.leftmost();
  while (u != 0)
  {
    add(u->item);
    u = y.succ(u);
  }
}

void <T>SkipSet::operator &= (<T>SkipSet& y)
{
  if (y.count == 0)
    clear();
  else if (&y != this && count != 0)
  {
    <T>SkipSetNodePtr t = leftmost();
    while (t != 0)
    {
      <T>SkipSetNodePtr s = succ(t);
      if (y.seek(t->item) == 0) del(t->item);
      t = s;
    }
  }
}


void <T>SkipSet::operator -=(<T>SkipSet& y)
{
  if (&y == this)
    clear();
  else if (y.count != 0)
  {
    <T>SkipSetNodePtr t = leftmost();
    while (t != 0)
    {
      <T>SkipSetNodePtr s = succ(t);
      if (y.seek(t->item) != 0) del(t->item);
      t = s;
    }
  }
}

Pix <T>SkipSet::add (<T&> i)
{
  <T>SkipSetNodePtr *update = new <T>SkipSetNodePtr[max_levels+1];
  <T>SkipSetNodePtr curr = (<T>SkipSetNodePtr) this->header;
  int   l = level;
  <T>SkipSetNodePtr temp;
  
  do
  {
    while ((temp = curr->forward[l])!=header &&
	   <T>CMP(temp->item, i) < 0)
      curr = temp;
    update[l] = curr;
  } 
  while (--l >= 0);
  
  if (temp != header && <T>CMP(temp->item, i) == 0)
    return Pix(temp);

  if ((l = random_level ()) > level)
  {
    l = ++level;
    update[l] = (<T>SkipSetNodePtr)header;
  };

  temp = new <T>RealSkipSetNode (i, l);
  <T>SkipSetNodePtr *temp_forward = temp->forward;
  
  do
  {
    <T>SkipSetNodePtr *curr_forward = update[l]->forward;
    
    temp_forward[l] = curr_forward[l];
    curr_forward[l] = temp;
  } 
  while (--l >= 0);

  count++;
  delete update;
  return Pix(temp);
}

void <T>SkipSet::del(<T&> key)
{
  
  int   l = level;
  int   curr_level = level;
  <T>SkipSetNodePtr *update = new <T>SkipSetNodePtr[max_levels+1];
  <T>SkipSetNodePtr curr = (<T>SkipSetNodePtr)header;
  <T>SkipSetNodePtr temp;
  
  do
  {
    while ((temp = curr->forward[l])!=header
	   && <T>CMP(temp->item,key) < 0)
      curr = temp;
    update[l] = curr;
  } 
  while (--l >= 0);
  
  if (<T>CMP(temp->item,key)==0)
  {
    <T>SkipSetNodePtr *temp_forward = temp->forward;
    
    for (l = 0;
	 l <= curr_level && (curr = update[l])->forward[l] == temp;
	 l++)
      curr->forward[l] = temp_forward[l];
    
    delete temp;
    
    <T>SkipSetNodePtr *forward = header->forward;
    
    while (forward[curr_level]==header && curr_level > 0)
      curr_level--;

    level = curr_level;
    count--;
    delete update;
    return;
  }
}

<T>SkipSetNodePtr <T>SkipSet::rightmost()
{
  <T>SkipSetNodePtr temp;
  <T>SkipSetNode*   curr = header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=header)
      curr = temp;
  while (--l >= 0);
  
  return temp==header ? 0 : temp;
}

<T>SkipSetNodePtr <T>SkipSet::pred(<T>SkipSetNodePtr t)
{
  <T>SkipSetNodePtr temp, curr = (<T>SkipSetNodePtr) header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=t)
      curr = temp;
  while (--l >= 0);
  
  return curr == header ? 0 : curr;
}

void <T>SkipSet::_kill()
{
  <T>SkipSetNode *p = this->header->forward[0];
  
  while (p != header)
  {
    <T>SkipSetNodePtr q = p->forward[0];
    delete p;
    p = q;
  }
}

void <T>SkipSet::clear()
{
  <T>SkipSetNodePtr *buffer_start = header->forward;
  <T>SkipSetNodePtr *trav = &header->forward[level+1];
  _kill();
  count = 0;
    
  while (trav > buffer_start)
    *--trav = (<T>SkipSetNodePtr)header;
}

Pix <T>SkipSet::seek(<T&> key)
{
  <T>SkipSetNodePtr temp;
  <T>SkipSetNode *curr  = header;
  int   l = level;
  
  do
  {
    while ((temp = curr->forward[l])!=header &&
	   <T>CMP(temp->item, key) < 0)
      curr = temp;
  }
  while (--l >= 0);
  
  if (<T>CMP(temp->item, key) != 0)
    return 0;
  else
  {
    return Pix(temp);
  }
}


/*
 * random function for probabilistic balancing
 *
 * Hardwired for p = .25.  Not too flexible,
 * but fast.  Changing this would require a constructor
 * that would accept a different value for p, etc.
 * Perhaps someone else would like to implement this?
 *
 */
int <T>SkipSet::random_level (void)
{
  int rlevel = 0;
  int b;
  
  do
  {
    b = random_bits & 3L;
    if (!b)
      rlevel++;
    random_bits >>= 2;
    if (--randoms_left == 0)
    {
      random_bits = gen->asLong();
      randoms_left = BITS_IN_RANDOM / 2;
    };
  } 
  while (!b);
  
  return rlevel > max_levels ? max_levels : rlevel;
}

int <T>SkipSet::OK()
{
  int v = 1;
  if (header == 0) 
    v = 0;
  else
  {
    int n = 0;
    <T>SkipSetNodePtr trail = leftmost();
    <T>SkipSetNodePtr t = 0;
    if (trail) t = succ(trail);
    if (t) n++;
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

<T>SkipSetinit::<T>SkipSetinit()
{
    if (!count)
	<T>SkipSet::gen = new MLCG(time(0));
    count++;
}

<T>SkipSetinit::~<T>SkipSetinit()
{
    count--;
    if (!count)
	delete <T>SkipSet::gen;
}
