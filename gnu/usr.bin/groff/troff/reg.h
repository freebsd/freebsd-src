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


class reg : public object {
public:
  virtual const char *get_string() = 0;
  virtual int get_value(units *);
  virtual void increment();
  virtual void decrement();
  virtual void set_increment(units);
  virtual void alter_format(char f, int w = 0);
  virtual const char *get_format();
  virtual void set_value(units);
};

class constant_int_reg : public reg {
  int *p;
public:
  constant_int_reg(int *);
  const char *get_string();
};

class general_reg : public reg {
  char format;
  int width;
  int inc;
public:
  general_reg();
  const char *get_string();
  void increment();
  void decrement();
  void alter_format(char f, int w = 0);
  void set_increment(units);
  const char *get_format();
  void add_value(units);

  void set_value(units) = 0;
  int get_value(units *) = 0;
};

class variable_reg : public general_reg {
  units *ptr;
public:
  variable_reg(int *);
  void set_value(units);
  int get_value(units *);
};

extern object_dictionary number_reg_dictionary;
extern void set_number_reg(symbol nm, units n);

reg *lookup_number_reg(symbol);
#if 0
void inline_define_reg();
#endif
