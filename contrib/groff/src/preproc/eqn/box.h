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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

struct list_box;

class box {
private:
  static int next_uid;
public:
  int spacing_type;
  const int uid;
  box();
  virtual void debug_print() = 0;
  virtual ~box();
  void top_level();
  virtual int compute_metrics(int);
  virtual void compute_subscript_kern();
  virtual void compute_skew();
  virtual void output();
  void extra_space();
  virtual list_box *to_list_box();
  virtual int is_simple();
  virtual int is_char();
  virtual int left_is_italic();
  virtual int right_is_italic();
  virtual void handle_char_type(int, int);
  enum { FOUND_NOTHING = 0, FOUND_MARK = 1, FOUND_LINEUP = 2 };
  void set_spacing_type(char *type);
  virtual void hint(unsigned);
  virtual void check_tabs(int);
};

class box_list {
private:
  int maxlen;
public:
  box **p;
  int len;

  box_list(box *);
  ~box_list();
  void append(box *);
  void list_check_tabs(int);
  void list_debug_print(const char *sep);
  friend class list_box;
};

class list_box : public box {
  int is_script;
  box_list list;
  int sty;
public:
  list_box(box *);
  void debug_print();
  int compute_metrics(int);
  void compute_subscript_kern();
  void output();
  void check_tabs(int);
  void append(box *);
  list_box *to_list_box();
  void handle_char_type(int, int);
  void compute_sublist_width(int n);
  friend box *make_script_box(box *, box *, box *);
  friend box *make_mark_box(box *);
  friend box *make_lineup_box(box *);
};

enum alignment { LEFT_ALIGN, RIGHT_ALIGN, CENTER_ALIGN };

class column : public box_list {
  alignment align;
  int space;
public:
  column(box *);
  void set_alignment(alignment);
  void set_space(int);
  void debug_print(const char *);

  friend class matrix_box;
  friend class pile_box;
};

class pile_box : public box {
  column col;
public:
  pile_box(box *);
  int compute_metrics(int);
  void output();
  void debug_print();
  void check_tabs(int);
  void set_alignment(alignment a) { col.set_alignment(a); }
  void set_space(int n) { col.set_space(n); }
  void append(box *p) { col.append(p); }
};

class matrix_box : public box {
private:
  int len;
  int maxlen;
  column **p;
public:
  matrix_box(column *);
  ~matrix_box();
  void append(column *);
  int compute_metrics(int);
  void output();
  void check_tabs(int);
  void debug_print();
};

class pointer_box : public box {
protected:
  box *p;
public:
  pointer_box(box *);
  ~pointer_box();
  int compute_metrics(int);
  void compute_subscript_kern();
  void compute_skew();
  void debug_print() = 0;
  void check_tabs(int);
};

class vcenter_box : public pointer_box {
public:
  vcenter_box(box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

class simple_box : public box {
public:
  int compute_metrics(int);
  void compute_subscript_kern();
  void compute_skew();
  void output() = 0;
  void debug_print() = 0;
  int is_simple();
};

class quoted_text_box : public simple_box {
  char *text;
public:
  quoted_text_box(char *);
  ~quoted_text_box();
  void debug_print();
  void output();
};

class half_space_box : public simple_box {
public:
  half_space_box();
  void output();
  void debug_print();
};

class space_box : public simple_box {
public:
  space_box();
  void output();
  void debug_print();
};

class tab_box : public box {
  int disabled;
public:
  tab_box();
  void output();
  void debug_print();
  void check_tabs(int);
};

class size_box : public pointer_box {
private:
  char *size;
public:
  size_box(char *, box *);
  ~size_box();
  int compute_metrics(int);
  void output();
  void debug_print();
};

class font_box : public pointer_box {
private:
  char *f;
public:
  font_box(char *, box *);
  ~font_box();
  int compute_metrics(int);
  void output();
  void debug_print();
};

class fat_box : public pointer_box {
public:
  fat_box(box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

class vmotion_box : public pointer_box {
private:
  int n;			// up is >= 0
public:
  vmotion_box(int, box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

class hmotion_box : public pointer_box {
  int n;
public:
  hmotion_box(int, box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

box *split_text(char *);
box *make_script_box(box *, box *, box *);
box *make_mark_box(box *);
box *make_lineup_box(box *);
box *make_delim_box(char *, box *, char *);
box *make_sqrt_box(box *);
box *make_prime_box(box *);
box *make_over_box(box *, box *);
box *make_small_over_box(box *, box *);
box *make_limit_box(box *, box *, box *);
box *make_accent_box(box *, box *);
box *make_uaccent_box(box *, box *);
box *make_overline_box(box *);
box *make_underline_box(box *);
box *make_special_box(char *, box *);

void set_space(int);
int set_gsize(const char *);
void set_gfont(const char *);
void set_grfont(const char *);
void set_gbfont(const char *);
const char *get_gfont();
const char *get_grfont();
const char *get_gbfont();
void start_string();
void output_string();
void do_text(const char *);
void restore_compatibility();
void set_script_reduction(int n);
void set_minimum_size(int n);
void set_param(const char *name, int value);

void set_char_type(const char *type, char *ch);

void init_char_table();
void init_extensible();
void define_extensible(const char *name, const char *ext, const char *top = 0,
		       const char *mid = 0, const char *bot = 0);
