// -*- C++ -*-

// <groff_src_dir>/src/include/printer.h

/* Copyright (C) 1989, 1990, 1991, 1992, 2001, 2002, 2003
   Free Software Foundation, Inc.

   Written by James Clark (jjc@jclark.com)

   Last update: 12 Apr 2002

   This file is part of groff.

   groff is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   groff is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with groff; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.
*/

/* Description

   The class `printer' performs the postprocessing.  Each
   postprocessor only needs to implement a derived class of `printer' and
   a suitable function `make_printer' for the device-dependent tasks.
   Then the methods of class `printer' are called automatically by
   `do_file()' in `input.cpp'.
*/

#include "color.h"

struct environment {
  int fontno;
  int size;
  int hpos;
  int vpos;
  int height;
  int slant;
  color *col;
  color *fill;
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
  virtual void set_numbered_char(int n, const environment *env,
				 int *widthp = 0);
  int set_char_and_width(const char *nm, const environment *env,
			 int *widthp, font **f);
  font *get_font_from_index(int fontno);
  virtual void draw(int code, int *p, int np, const environment *env);
  // perform change of line color (text, outline) in the print-out 
  virtual void change_color(const environment * const env);
  // perform change of fill color in the print-out 
  virtual void change_fill_color(const environment * const env);
  virtual void begin_page(int) = 0;
  virtual void end_page(int page_length) = 0;
  virtual font *make_font(const char *nm);
  virtual void end_of_line();
  virtual void special(char *arg, const environment *env,
		       char type = 'p');
protected:
  font_pointer_list *font_list;
  font **font_table;
  int nfonts;

  // information about named characters
  int is_char_named;
  int is_named_set;
  char named_command;
  const char *named_char_s;
  int named_char_n;

private:
  font *find_font(const char *);
  virtual void set_char(int index, font *f, const environment *env,
			int w, const char *name) = 0;
};

printer *make_printer();
