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
#include <builtin.h>
#include <Obstack.h>

Obstack::Obstack(int size, int alignment)
{
  alignmentmask = alignment - 1;
  chunksize = size;
  chunk = 0;
  nextfree = objectbase = 0;
  chunklimit = 0;
}

void Obstack::_free(void* obj)
{
  _obstack_chunk*  lp;
  _obstack_chunk*  plp;

  lp = chunk;
  while (lp != 0 && ((void*)lp > obj || (void*)(lp)->limit < obj))
  {
    plp = lp -> prev;
    delete [] (char*)lp;
    lp = plp;
  }
  if (lp)
  {
    objectbase = nextfree = (char *)(obj);
    chunklimit = lp->limit;
    chunk = lp;
  }
  else if (obj != 0)
    (*lib_error_handler)("Obstack", "deletion of nonexistent obj");
}

void Obstack::newchunk(int size)
{
  _obstack_chunk*	old_chunk = chunk;
  _obstack_chunk*	new_chunk;
  long	new_size;
  int obj_size = nextfree - objectbase;

  new_size = (obj_size + size) << 1;
  if (new_size < chunksize)
    new_size = chunksize;

  new_chunk = chunk = (_obstack_chunk*)(new char[new_size]);
  new_chunk->prev = old_chunk;
  new_chunk->limit = chunklimit = (char *) new_chunk + new_size;

  memcpy((void*)new_chunk->contents, (void*)objectbase, obj_size);
  objectbase = new_chunk->contents;
  nextfree = objectbase + obj_size;
}

void* Obstack::finish()
{
  void* value = (void*) objectbase;
  nextfree = (char*)((int)(nextfree + alignmentmask) & ~(alignmentmask));
  if (nextfree - (char*)chunk > chunklimit - (char*)chunk)
    nextfree = chunklimit;
  objectbase = nextfree;
  return value;
}

int Obstack::contains(void* obj) // true if obj somewhere in Obstack
{
  for (_obstack_chunk* ch = chunk; 
       ch != 0 && (obj < (void*)ch || obj >= (void*)(ch->limit)); 
       ch = ch->prev);

  return ch != 0;
}
         
int Obstack::OK()
{
  int v = chunksize > 0;        // valid size
  v &= alignmentmask != 0;      // and alignment
  v &= chunk != 0;
  v &= objectbase >= chunk->contents;
  v &= nextfree >= objectbase;
  v &= nextfree <= chunklimit;
  v &= chunklimit == chunk->limit;
  _obstack_chunk* p = chunk;
  // allow lots of chances to find bottom!
  long x = MAXLONG;
  while (p != 0 && x != 0) { --x; p = p->prev; }
  v &= x > 0;
  if (!v) 
    (*lib_error_handler)("Obstack", "invariant failure");
  return v;
}
