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
#include <limits.h>
#include <builtin.h>
#include <Obstack.h>
#include <new.h>

/* We use subtraction of (char *)0 instead of casting to int
   because on word-addressable machines a simple cast to int
   may ignore the byte-within-word field of the pointer.  */

#ifndef __PTR_TO_INT
#define __PTR_TO_INT(P) ((P) - (char *)0)
#endif

#ifndef __INT_TO_PTR
#define __INT_TO_PTR(P) ((P) + (char *)0)
#endif

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

  new_chunk = chunk = new (operator new (new_size)) _obstack_chunk;
  new_chunk->prev = old_chunk;
  new_chunk->limit = chunklimit = (char *) new_chunk + new_size;

  memcpy((void*)new_chunk->contents, (void*)objectbase, obj_size);
  objectbase = new_chunk->contents;
  nextfree = objectbase + obj_size;
}

void* Obstack::finish()
{
  void* value = (void*) objectbase;
  nextfree = __INT_TO_PTR (__PTR_TO_INT (nextfree + alignmentmask)
			   & ~alignmentmask);
  if (nextfree - (char*)chunk > chunklimit - (char*)chunk)
    nextfree = chunklimit;
  objectbase = nextfree;
  return value;
}

int Obstack::contains(void* obj) // true if obj somewhere in Obstack
{
  _obstack_chunk* ch;
  for (ch = chunk; 
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
  long x = LONG_MAX;
  while (p != 0 && x != 0) { --x; p = p->prev; }
  v &= x > 0;
  if (!v) 
    (*lib_error_handler)("Obstack", "invariant failure");
  return v;
}
