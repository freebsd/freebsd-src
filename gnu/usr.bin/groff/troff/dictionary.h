// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
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
Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. */



// there is no distinction between name with no value and name with NULL value
// null names are not permitted (they will be ignored).

struct association {
  symbol s;
  void *v;
  association() :  v(0) {}
};

class dictionary;

class dictionary_iterator {
  dictionary *dict;
  int i;
public:
  dictionary_iterator(dictionary &);
  int get(symbol *, void **);
};

class dictionary {
  int size;
  int used;
  double threshold;
  double factor;
  association *table;
  void rehash(int);
public:
  dictionary(int);
  void *lookup(symbol s, void *v=0); // returns value associated with key
  void *lookup(const char *);
  // if second parameter not NULL, value will be replaced
  void *remove(symbol);
  friend class dictionary_iterator;
};

class object {
  int rcount;
 public:
  object();
  virtual ~object();
  void add_reference();
  void remove_reference();
};

class object_dictionary;

class object_dictionary_iterator {
  dictionary_iterator di;
public:
  object_dictionary_iterator(object_dictionary &);
  int get(symbol *, object **);
};

class object_dictionary {
  dictionary d;
public:
  object_dictionary(int);
  object *lookup(symbol nm);
  void define(symbol nm, object *obj);
  void rename(symbol oldnm, symbol newnm);
  void remove(symbol nm);
  int alias(symbol newnm, symbol oldnm);
  friend class object_dictionary_iterator;
};


inline int object_dictionary_iterator::get(symbol *sp, object **op)
{
  return di.get(sp, (void **)op);
}
