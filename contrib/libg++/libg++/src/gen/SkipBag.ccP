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
 * Bags implemented via William Pugh SkipList algorithms.
 * CACM, June 1990, p 668-676.
 *
 */

#include <stream.h>
#include <time.h>
#include "<T>.SkipBag.h"

MLCG* <T>SkipBag::gen = 0;
int <T>SkipBaginit::count = 0;

static int countbits(long bits)
{
   int n = 0;
   while(bits>>=1L) n++;
   return n;
}

<T>SkipBag::<T>SkipBag(long size)
: level(0),
  header(new <T>SkipBagNode (countbits(size)+1)),
  max_levels (countbits(size)+1),
  random_bits(gen->asLong()),
  randoms_left(BITS_IN_RANDOM / 2)
{
  <T>SkipBagNodePtr *buffer_start = header->forward;
  <T>SkipBagNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  while (trav > buffer_start)
    *--trav = (<T>SkipBagNodePtr) header;
}

<T>SkipBag::<T>SkipBag(<T>SkipBag& b)
: level (0),
  header (new <T>SkipBagNode (b.max_levels)),
  max_levels (b.max_levels),
  random_bits (gen->asLong()),
  randoms_left (BITS_IN_RANDOM / 2)
{
  <T>SkipBagNodePtr *buffer_start = header->forward;
  <T>SkipBagNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  
   while (trav > buffer_start)
     *--trav = (<T>SkipBagNodePtr)header;
  
  for (<T>SkipBagNodePtr t = b.leftmost(); t; t = b.succ(t))
      add(t->item);
}

Pix <T>SkipBag::add (<T&> item)
{
  <T>SkipBagNodePtr *update = new <T>SkipBagNodePtr[max_levels+1];
  <T>SkipBagNodePtr curr = (<T>SkipBagNodePtr) this->header;
  int   l = level;
  <T>SkipBagNodePtr temp;
  
  do
  {
    while ((temp = curr->forward[l])!=header &&
	   <T>CMP(temp->item, item) < 0)
      curr = temp;
    update[l] = curr;
  } 
  while (--l >= 0);
  
  if ((l = random_level ()) > level)
  {
    l = ++level;
    update[l] = (<T>SkipBagNodePtr)header;
  };

  temp = new <T>RealSkipBagNode (item, l);
  <T>SkipBagNodePtr *temp_forward = temp->forward;
  
  do
  {
    <T>SkipBagNodePtr *curr_forward = update[l]->forward;
    
    temp_forward[l] = curr_forward[l];
    curr_forward[l] = temp;
  } 
  while (--l >= 0);

  count++;
  delete update;
  return Pix(temp);
}

void <T>SkipBag::del(<T&> key)
{
  
  int   l = level;
  int   curr_level = level;
  <T>SkipBagNodePtr *update = new <T>SkipBagNodePtr[max_levels+1];
  <T>SkipBagNodePtr curr = (<T>SkipBagNodePtr)header;
  <T>SkipBagNodePtr temp;
  
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
    <T>SkipBagNodePtr *temp_forward = temp->forward;
    
    for (l = 0;
	 l <= curr_level && (curr = update[l])->forward[l] == temp;
	 l++)
      curr->forward[l] = temp_forward[l];
    
    delete temp;
    
    <T>SkipBagNodePtr *forward = header->forward;
    
    while (forward[curr_level]==header && curr_level > 0)
      curr_level--;

    level = curr_level;
    count--;
    delete update;
    return;
  }
}

<T>SkipBagNodePtr <T>SkipBag::rightmost()
{
  <T>SkipBagNodePtr temp;
  <T>SkipBagNode*   curr = header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=header)
      curr = temp;
  while (--l >= 0);
  
  return temp==header ? 0 : temp;
}

<T>SkipBagNodePtr <T>SkipBag::pred(<T>SkipBagNodePtr t)
{
  <T>SkipBagNodePtr temp, curr = (<T>SkipBagNodePtr) header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=t)
      curr = temp;
  while (--l >= 0);
  
  return curr == header ? 0 : curr;
}

void <T>SkipBag::_kill()
{
  <T>SkipBagNode *p = this->header->forward[0];
  
  while (p != header)
  {
    <T>SkipBagNodePtr q = p->forward[0];
    delete p;
    p = q;
  }
}

void <T>SkipBag::clear()
{
  <T>SkipBagNodePtr *buffer_start = header->forward;
  <T>SkipBagNodePtr *trav = &header->forward[level+1];
  _kill();
  count = 0;
    
  while (trav > buffer_start)
    *--trav = (<T>SkipBagNodePtr)header;
}

Pix <T>SkipBag::seek(<T&> key, Pix i)
{
  <T>SkipBagNodePtr temp;
  <T>SkipBagNode *curr  = header;
  int   l = level;
  if (i)
      curr = (<T>SkipBagNode *)i;
  
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


int <T>SkipBag::nof(<T&> item)
{
    int n = 0;
    <T>SkipBagNodePtr t = (<T>SkipBagNodePtr)(seek(item));
    if (t != 0)
    {
	do
	{
	    ++n;
	    t = succ(t);
	} while (t != 0 && <T>EQ(item, t->item));
    }
    return n;
}

void <T>SkipBag::remove(<T&> key)
{
    Pix t = seek(key);
    while (t != 0)
    {
	del(key);
	t = seek(key);
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
int <T>SkipBag::random_level (void)
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

int <T>SkipBag::OK()
{
  int v = 1;
  if (header == 0) 
    v = 0;
  else
  {
    int n = 0;
    <T>SkipBagNodePtr trail = leftmost();
    <T>SkipBagNodePtr t = 0;
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

<T>SkipBaginit::<T>SkipBaginit()
{
    if (!count)
	<T>SkipBag::gen = new MLCG(time(0));
    count++;
}

<T>SkipBaginit::~<T>SkipBaginit()
{
    count--;
    if (!count)
	delete <T>SkipBag::gen;
}
