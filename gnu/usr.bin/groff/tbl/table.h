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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#include "cset.h"
#include "cmap.h"
#include "stringclass.h"
#include "errarg.h"
#include "error.h"
#include "lib.h"

struct inc_number {
  short inc;
  short val;
};

struct entry_modifier {
  inc_number point_size;
  inc_number vertical_spacing;
  string font;
  enum { CENTER, TOP, BOTTOM } vertical_alignment;
  char zero_width;
  char stagger;

  entry_modifier();
  ~entry_modifier();
};

enum format_type {
  FORMAT_LEFT, 
  FORMAT_CENTER, 
  FORMAT_RIGHT, 
  FORMAT_NUMERIC,
  FORMAT_ALPHABETIC,
  FORMAT_SPAN, 
  FORMAT_VSPAN,
  FORMAT_HLINE,
  FORMAT_DOUBLE_HLINE
};

struct entry_format : entry_modifier {
  format_type type;

  entry_format(format_type);
  entry_format();
  void debug_print() const;
};

struct table_entry;
struct horizontal_span;
struct stuff;
struct vertical_rule;

class table {
  unsigned flags;
  int nrows;
  int ncolumns;
  int linesize;
  char delim[2];
  char decimal_point_char;
  vertical_rule *vrule_list;
  stuff *stuff_list;
  horizontal_span *span_list;
  table_entry *entry_list;
  table_entry ***entry;
  char **vline;
  char *row_is_all_lines;
  string *minimum_width;
  int *column_separation;
  char *equal;
  int left_separation;
  int right_separation;
  int allocated_rows;
  void build_span_list();
  void do_hspan(int r, int c);
  void do_vspan(int r, int c);
  void allocate(int r);
  void compute_widths();
  void divide_span(int, int);
  void sum_columns(int, int);
  void compute_separation_factor();
  void compute_column_positions();
  void do_row(int);
  void init_output();
  void add_stuff(stuff *);
  void do_top();
  void do_bottom();
  void do_vertical_rules();
  void build_vrule_list();
  void add_vertical_rule(int, int, int, int);
  void define_bottom_macro();
  int vline_spanned(int r, int c);
  int row_begins_section(int);
  int row_ends_section(int);
  void make_columns_equal();
  void compute_vrule_top_adjust(int, int, string &);
  void compute_vrule_bot_adjust(int, int, string &);
  void determine_row_type();
public:
  /* used by flags */
  enum {
    CENTER = 01,
    EXPAND = 02,
    BOX = 04,
    ALLBOX = 010,
    DOUBLEBOX = 020,
    NOKEEP = 040
    };
  table(int nc, unsigned flags, int linesize, char decimal_point_char);
  ~table();

  void add_text_line(int r, const string &, const char *, int);
  void add_single_hline(int r);
  void add_double_hline(int r);
  void add_entry(int r, int c, const string &, const entry_format *,
		 const char *, int lineno);
  void add_vlines(int r, const char *);
  void check();
  void print();
  void set_minimum_width(int c, const string &w);
  void set_column_separation(int c, int n);
  void set_equal_column(int c);
  void set_delim(char c1, char c2);
  void print_single_hline(int r);
  void print_double_hline(int r);
  int get_nrows();
};

void set_troff_location(const char *, int);
