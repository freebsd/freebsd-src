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

#ifdef __GNUG__
#pragma implementation
#endif

#include <stream.h>
#include <time.h>
#include "<T>.<C>.SkipMap.h"

/*
 * Bags implemented via William Pugh SkipList algorithms.
 * CACM, June 1990, p 668-676.
 *
 */

MLCG* <T><C>SkipMap::gen = 0;
int <T><C>SkipMapinit::count = 0;

static int countbits(long bits)
{
  int n = 0;
  while(bits>>=1) n++;
  return n;
}

<T><C>SkipMap::<T><C>SkipMap(<C&> dflt, long size)
: <T><C>Map(dflt),
  level(0),
  header(new <T><C>SkipMapNode (countbits(size)+1)),
  max_levels (countbits(size)+1),
  random_bits(gen->asLong()),
  randoms_left(BITS_IN_RANDOM / 2)
{
  <T><C>SkipMapNodePtr *buffer_start = header->forward;
  <T><C>SkipMapNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  while (trav > buffer_start)
    *--trav = (<T><C>SkipMapNodePtr) header;
}

<T><C>SkipMap::<T><C>SkipMap(<T><C>SkipMap& b)
: <T><C>Map(b.def),
  level (0),
  header (new <T><C>SkipMapNode (b.max_levels)),
  max_levels (b.max_levels),
  random_bits (gen->asLong()),
  randoms_left (BITS_IN_RANDOM / 2)
{
  <T><C>SkipMapNodePtr *buffer_start = header->forward;
  <T><C>SkipMapNodePtr *trav = &header->forward[max_levels];
  
  count = 0;
  
   while (trav > buffer_start)
     *--trav = (<T><C>SkipMapNodePtr)header;
  
  for (<T><C>SkipMapNodePtr t = b.leftmost(); t; t = b.succ(t))
    (*this)[t->item] = t->cont;
}

<C>& <T><C>SkipMap::operator [] (<T&> item)
{
  <T><C>SkipMapNodePtr *update = new <T><C>SkipMapNodePtr[max_levels+1];
  <T><C>SkipMapNodePtr curr =
    (<T><C>SkipMapNodePtr) this->header;
  int   l = level;
  <T><C>SkipMapNodePtr temp;
  
  do
  {
    while ((temp = curr->forward[l])!=header &&
	   <T>CMP(temp->item, item) < 0)
      curr = temp;
    update[l] = curr;
  } 
  while (--l >= 0);
  
  if (temp != header && <T>CMP(temp->item, item) == 0)
  {
      delete update;
      return temp->cont;
  }

  if ((l = random_level ()) > level)
  {
    l = ++level;
    update[l] = (<T><C>SkipMapNodePtr)header;
  };

  temp = new <T><C>RealSkipMapNode (item, def, l);
  <T><C>SkipMapNodePtr *temp_forward = temp->forward;
  
  do
  {
    <T><C>SkipMapNodePtr *curr_forward = update[l]->forward;
    
    temp_forward[l] = curr_forward[l];
    curr_forward[l] = temp;
  } 
  while (--l >= 0);

  count++;
  delete update;
  return temp->cont;
}

void <T><C>SkipMap::del(<T&> key)
{
  
  int   l = level;
  int   curr_level = level;
  <T><C>SkipMapNodePtr *update = new <T><C>SkipMapNodePtr[max_levels+1];
  <T><C>SkipMapNodePtr curr = (<T><C>SkipMapNodePtr)header;
  <T><C>SkipMapNodePtr temp;
  
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
    <T><C>SkipMapNodePtr *temp_forward = temp->forward;
    
    for (l = 0;
	 l <= curr_level && (curr = update[l])->forward[l] == temp;
	 l++)
      curr->forward[l] = temp_forward[l];
    
    delete temp;
    
    <T><C>SkipMapNodePtr *forward = header->forward;
    
    while (forward[curr_level]==header && curr_level > 0)
      curr_level--;
    
    level = curr_level;
    count--;
    delete update;
    return;
  }
}

<T><C>SkipMapNodePtr <T><C>SkipMap::rightmost()
{
  <T><C>SkipMapNodePtr temp;
  <T><C>SkipMapNode*   curr = header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=header)
      curr = temp;
  while (--l >= 0);
  
  return temp==header ? 0 : temp;
}

<T><C>SkipMapNodePtr <T><C>SkipMap::pred(<T><C>SkipMapNodePtr t)
{
  <T><C>SkipMapNodePtr temp, curr = (<T><C>SkipMapNodePtr) header;
  int l = level;
  
  do
    while ((temp = curr->forward[l])!=t)
      curr = temp;
  while (--l >= 0);
  
  return curr == header ? 0 : curr;
}

void <T><C>SkipMap::_kill()
{
  <T><C>SkipMapNode *p = this->header->forward[0];
  
  while (p != header)
  {
    <T><C>SkipMapNodePtr q = p->forward[0];
    delete p;
    p = q;
  }
}

void <T><C>SkipMap::clear()
{
  <T><C>SkipMapNodePtr *buffer_start = header->forward;
  <T><C>SkipMapNodePtr *trav = &header->forward[level+1];
  _kill();
  count = 0;
    
  while (trav > buffer_start)
    *--trav = (<T><C>SkipMapNodePtr)header;
}

Pix <T><C>SkipMap::seek(<T&> key)
{
  <T><C>SkipMapNodePtr temp;
  <T><C>SkipMapNode *curr  = header;
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
int <T><C>SkipMap::random_level (void)
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

int <T><C>SkipMap::OK()
{
  int v = 1;
  if (header == 0) 
    v = 0;
  else
  {
    int n = 0;
    <T><C>SkipMapNodePtr trail = leftmost();
    <T><C>SkipMapNodePtr t = 0;
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

<T><C>SkipMapinit::<T><C>SkipMapinit()
{
    if (!count)
	<T><C>SkipMap::gen = new MLCG(time(0));
    count++;
}

<T><C>SkipMapinit::~<T><C>SkipMapinit()
{
    count--;
    if (!count)
	delete <T><C>SkipMap::gen;
}

	    
