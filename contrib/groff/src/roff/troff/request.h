// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

typedef void (*REQUEST_FUNCP)();

class macro;

class request_or_macro : public object {
public:
  request_or_macro();
  virtual void invoke(symbol s) = 0;
  virtual macro *to_macro();
};

class request : public request_or_macro {
  REQUEST_FUNCP p;
public:
  void invoke(symbol);
  request(REQUEST_FUNCP);
};

void delete_request_or_macro(request_or_macro *);

extern object_dictionary request_dictionary;

struct macro_header;
struct node;

class macro : public request_or_macro {
  macro_header *p;
  const char *filename;		// where was it defined?
  int lineno;
  int length;
public:
  macro();
  ~macro();
  macro(const macro &);
  macro &operator=(const macro &);
  void append(unsigned char);
  void append(node *);
  void append_unsigned(unsigned int i);
  void append_int(int i);
  void append_str(const char *);
  void invoke(symbol);
  macro *to_macro();
  void print_size();
  int empty();
  friend class string_iterator;
  friend void chop_macro();
  friend void substring_macro();
  friend int operator==(const macro &, const macro &);
};

extern void init_input_requests();
extern void init_markup_requests();
extern void init_div_requests();
extern void init_node_requests();
extern void init_reg_requests();
extern void init_env_requests();
extern void init_hyphen_requests();
extern void init_request(const char *s, REQUEST_FUNCP f);

class charinfo;
class environment;

node *charinfo_to_node_list(charinfo *, const environment *);
