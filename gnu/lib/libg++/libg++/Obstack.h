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


#ifndef _Obstack_h
#ifdef __GNUG__
#pragma interface
#endif
#define _Obstack_h 1

#include <std.h>

class Obstack
{
  struct _obstack_chunk
  {
    char*           limit;
    _obstack_chunk* prev;
    char            contents[4];
  };

protected:
  long	          chunksize;
  _obstack_chunk* chunk;
  char*	          objectbase;
  char*	          nextfree;
  char*	          chunklimit;
  int             alignmentmask;

  void  _free(void* obj);
  void  newchunk(int size);

public:
        Obstack(int size = 4080, int alignment = 4); // 4080=4096-mallocslop

        ~Obstack();

  void* base();
  void* next_free();
  int   alignment_mask();
  int   chunk_size();
  int   size();
  int   room();
  int   contains(void* p);      // does Obstack hold pointer p?

  void  grow(const void* data, int size);
  void  grow(const void* data, int size, char terminator);
  void  grow(const char* s);
  void  grow(char c);
  void  grow_fast(char c);
  void  blank(int size);
  void  blank_fast(int size);

  void* finish();
  void* finish(char terminator);

  void* copy(const void* data, int size);
  void* copy(const void* data, int size, char terminator);
  void* copy(const char* s);
  void* copy(char c);
  void* alloc(int size);

  void  free(void* obj);
  void  shrink(int size = 1); // suggested by ken@cs.rochester.edu

  int   OK();                   // rep invariant
};


inline Obstack::~Obstack()
{
  _free(0); 
}

inline void* Obstack::base()
{
  return objectbase; 
}

inline void* Obstack::next_free()
{
  return nextfree; 
}

inline int Obstack::alignment_mask()
{
  return alignmentmask; 
}

inline int Obstack::chunk_size()
{
  return chunksize; 
}

inline int Obstack::size()
{
  return nextfree - objectbase; 
}

inline int Obstack::room()
{
  return chunklimit - nextfree; 
}

inline void Obstack:: grow(const void* data, int size)
{
  if (nextfree+size > chunklimit) 
    newchunk(size);
  memcpy(nextfree, data, size);
  nextfree += size; 
}

inline void Obstack:: grow(const void* data, int size, char terminator)
{
  if (nextfree+size+1 > chunklimit) 
    newchunk(size+1);
  memcpy(nextfree, data, size);
  nextfree += size; 
  *(nextfree)++ = terminator; 
}

inline void Obstack:: grow(const char* s)
{
  grow((const void*)s, strlen(s), 0); 
}

inline void Obstack:: grow(char c)
{
  if (nextfree+1 > chunklimit) 
    newchunk(1); 
  *(nextfree)++ = c; 
}

inline void Obstack:: blank(int size)
{
  if (nextfree+size > chunklimit) 
    newchunk(size);
  nextfree += size; 
}

inline void* Obstack::finish(char terminator)
{
  grow(terminator); 
  return finish(); 
}

inline void* Obstack::copy(const void* data, int size)
{
  grow (data, size);
  return finish(); 
}

inline void* Obstack::copy(const void* data, int size, char terminator)
{
  grow(data, size, terminator); 
  return finish(); 
}

inline void* Obstack::copy(const char* s)
{
  grow((const void*)s, strlen(s), 0); 
  return finish(); 
}

inline void* Obstack::copy(char c)
{
  grow(c);
  return finish(); 
}

inline void* Obstack::alloc(int size)
{
  blank(size);
  return finish(); 
}

inline void Obstack:: free(void* obj)     
{
  if (obj >= (void*)chunk && obj<(void*)chunklimit)
    nextfree = objectbase = (char *) obj;
  else 
    _free(obj); 
}

inline void Obstack:: grow_fast(char c)
{
  *(nextfree)++ = c; 
}

inline void Obstack:: blank_fast(int size)
{
  nextfree += size; 
}

inline void Obstack:: shrink(int size) // from ken@cs.rochester.edu
{
  if (nextfree >= objectbase + size)
    nextfree -= size;
}

#endif
