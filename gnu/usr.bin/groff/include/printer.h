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

struct environment {
  int fontno;
  int size;
  int hpos;
  int vpos;
  int height;
  int slant;
};

struct font;

struct font_pointer_list {
  font *p;
  font_pointer_list *next;

  font_pointer_list(font *, font_pointer_list *);
};

class printer {
public:
  printer();
  virtual ~printer();
  void load_font(int i, const char *name);
  void set_ascii_char(unsigned char c, const environment *env,
		      int *widthp = 0);
  void set_special_char(const char *nm, const environment *env,
			int *widthp = 0);
  void set_numbered_char(int n, const environment *env, int *widthp = 0);
  virtual void draw(int code, int *p, int np, const environment *env);
  virtual void begin_page(int) = 0;
  virtual void end_page(int page_length) = 0;
  virtual font *make_font(const char *nm);
  virtual void end_of_line();
  virtual void special(char *arg, const environment *env);
  static int adjust_arc_center(const int *, double *);
protected:
  font_pointer_list *font_list;
private:
  font **font_table;
  int nfonts;
  font *find_font(const char *);
  virtual void set_char(int index, font *f, const environment *env,
			int w) = 0;
};

printer *make_printer();
