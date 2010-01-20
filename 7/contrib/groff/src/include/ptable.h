// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2003, 2004
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include <assert.h>
#include <string.h>

#ifdef TRADITIONAL_CPP
#define name2(a,b) a/**/b
#else /* not TRADITIONAL_CPP */
#define name2(a,b) name2x(a,b)
#define name2x(a,b) a ## b
#endif /* not TRADITIONAL_CPP */

#define PTABLE(T) name2(T,_ptable)
#define PASSOC(T) name2(T,_passoc)
#define PTABLE_ITERATOR(T) name2(T,_ptable_iterator)

extern unsigned next_ptable_size(unsigned);
extern unsigned long hash_string(const char *);

#define declare_ptable(T)						      \
									      \
struct PASSOC(T) {							      \
  char *key;							      	      \
  T *val;								      \
  PASSOC(T)();								      \
};									      \
									      \
class PTABLE(T);							      \
									      \
class PTABLE_ITERATOR(T) {						      \
  PTABLE(T) *p;								      \
  unsigned i;								      \
public:									      \
  PTABLE_ITERATOR(T)(PTABLE(T) *);					      \
  int next(const char **, T **);					      \
};									      \
									      \
class PTABLE(T) {							      \
  PASSOC(T) *v;								      \
  unsigned size;							      \
  unsigned used;							      \
  enum { FULL_NUM = 2, FULL_DEN = 3, INITIAL_SIZE = 17 };		      \
public:									      \
  PTABLE(T)();								      \
  ~PTABLE(T)();								      \
  void define(const char *, T *);					      \
  T *lookup(const char *);						      \
  friend class PTABLE_ITERATOR(T);					      \
};


// Keys (which are strings) are allocated and freed by PTABLE.
// Values must be allocated by the caller (always using new[], not new)
// and are freed by PTABLE.

#define implement_ptable(T)						      \
									      \
PASSOC(T)::PASSOC(T)()							      \
: key(0), val(0)							      \
{									      \
}									      \
									      \
PTABLE(T)::PTABLE(T)()							      \
{									      \
  v = new PASSOC(T)[size = INITIAL_SIZE];				      \
  used = 0;								      \
}									      \
									      \
PTABLE(T)::~PTABLE(T)()							      \
{									      \
  for (unsigned i = 0; i < size; i++) {					      \
    a_delete v[i].key;							      \
    a_delete v[i].val;							      \
  }									      \
  a_delete v;								      \
}									      \
									      \
void PTABLE(T)::define(const char *key, T *val)				      \
{									      \
  assert(key != 0);							      \
  unsigned long h = hash_string(key);					      \
  unsigned n;								      \
  for (n = unsigned(h % size);					      	      \
       v[n].key != 0;							      \
       n = (n == 0 ? size - 1 : n - 1))					      \
    if (strcmp(v[n].key, key) == 0) {					      \
      a_delete v[n].val;						      \
      v[n].val = val;							      \
      return;								      \
    }									      \
  if (val == 0)								      \
    return;								      \
  if (used*FULL_DEN >= size*FULL_NUM) {					      \
    PASSOC(T) *oldv = v;						      \
    unsigned old_size = size;						      \
    size = next_ptable_size(size);					      \
    v = new PASSOC(T)[size];						      \
    for (unsigned i = 0; i < old_size; i++)				      \
      if (oldv[i].key != 0) {						      \
	if (oldv[i].val == 0)						      \
	  a_delete oldv[i].key;						      \
	else {								      \
	  unsigned j;							      \
	  for (j = unsigned(hash_string(oldv[i].key) % size);	      	      \
	       v[j].key != 0;						      \
	       j = (j == 0 ? size - 1 : j - 1))				      \
		 ;							      \
	  v[j].key = oldv[i].key;					      \
	  v[j].val = oldv[i].val;					      \
	}								      \
      }									      \
    for (n = unsigned(h % size);					      \
	 v[n].key != 0;							      \
	 n = (n == 0 ? size - 1 : n - 1))				      \
      ;									      \
    a_delete oldv;							      \
  }									      \
  char *temp = new char[strlen(key)+1];					      \
  strcpy(temp, key);							      \
  v[n].key = temp;							      \
  v[n].val = val;							      \
  used++;								      \
}									      \
									      \
T *PTABLE(T)::lookup(const char *key)					      \
{									      \
  assert(key != 0);							      \
  for (unsigned n = unsigned(hash_string(key) % size);			      \
       v[n].key != 0;							      \
       n = (n == 0 ? size - 1 : n - 1))					      \
    if (strcmp(v[n].key, key) == 0)					      \
      return v[n].val;							      \
  return 0;								      \
}									      \
									      \
PTABLE_ITERATOR(T)::PTABLE_ITERATOR(T)(PTABLE(T) *t)			      \
: p(t), i(0)								      \
{									      \
}									      \
									      \
int PTABLE_ITERATOR(T)::next(const char **keyp, T **valp)		      \
{									      \
  unsigned size = p->size;						      \
  PASSOC(T) *v = p->v;							      \
  for (; i < size; i++)							      \
    if (v[i].key != 0) {						      \
      *keyp = v[i].key;							      \
      *valp = v[i].val;							      \
      i++;								      \
      return 1;								      \
    }									      \
  return 0;								      \
}
