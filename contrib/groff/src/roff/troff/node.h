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


struct hyphen_list {
  unsigned char hyphen;
  unsigned char breakable;
  unsigned char hyphenation_code;
  hyphen_list *next;
  hyphen_list(unsigned char code, hyphen_list *p = 0);
};

void hyphenate(hyphen_list *, unsigned);

enum hyphenation_type { HYPHEN_MIDDLE, HYPHEN_BOUNDARY, HYPHEN_INHIBIT };

class ascii_output_file;

struct breakpoint;
struct vertical_size;
struct charinfo;

class macro;

class troff_output_file;
class tfont;
class environment;

class glyph_node;
class diverted_space_node;
class token_node;

struct node {
  node *next;
  node *last;
  node();
  node(node *n);
  node *add_char(charinfo *c, environment *, hunits *widthp, int *spacep);

  virtual ~node();
  virtual node *copy() = 0;
  virtual int set_unformat_flag();
  virtual int force_tprint() = 0;
  virtual hunits width();
  virtual hunits subscript_correction();
  virtual hunits italic_correction();
  virtual hunits left_italic_correction();
  virtual hunits skew();
  virtual int nspaces();
  virtual int merge_space(hunits, hunits, hunits);
  virtual vunits vertical_width();
  virtual node *last_char_node();
  virtual void vertical_extent(vunits *min, vunits *max);
  virtual int character_type();
  virtual void set_vertical_size(vertical_size *);
  virtual int ends_sentence();
  virtual node *merge_self(node *);
  virtual node *add_discretionary_hyphen();
  virtual node *add_self(node *, hyphen_list **);
  virtual hyphen_list *get_hyphen_list(hyphen_list *s = 0);
  virtual void ascii_print(ascii_output_file *);
  virtual void asciify(macro *);
  virtual int discardable();
  virtual void spread_space(int *, hunits *);
  virtual void freeze_space();
  virtual void is_escape_colon();
  virtual breakpoint *get_breakpoints(hunits width, int nspaces,
				      breakpoint *rest = 0,
				      int is_inner = 0);
  virtual int nbreaks();
  virtual void split(int, node **, node **);
  virtual hyphenation_type get_hyphenation_type();
  virtual int reread(int *);
  virtual token_node *get_token_node();
  virtual int overlaps_vertically();
  virtual int overlaps_horizontally();
  virtual units size();
  virtual int interpret(macro *);

  virtual node *merge_glyph_node(glyph_node *);
  virtual tfont *get_tfont();
  virtual void tprint(troff_output_file *);
  virtual void zero_width_tprint(troff_output_file *);

  node *add_italic_correction(hunits *);

  virtual int same(node *) = 0;
  virtual const char *type() = 0;
};

inline node::node() : next(0), last(0)
{
}

inline node::node(node *n) : next(n), last(0)
{
}

inline node::~node()
{
}

// 0 means it doesn't, 1 means it does, 2 means it's transparent

int node_list_ends_sentence(node *);

struct breakpoint {
  breakpoint *next;
  hunits width;
  int nspaces;
  node *nd;
  int index;
  char hyphenated;
};

class line_start_node : public node {
public:
  line_start_node() {}
  node *copy() { return new line_start_node; }
  int same(node *);
  int force_tprint();
  const char *type();
  void asciify(macro *);
};

class space_node : public node {
private:
#if 0
  enum { BLOCK = 1024 };
  static space_node *free_list;
  void operator delete(void *);
#endif
protected:
  hunits n;
  char set;
  char was_escape_colon;
  space_node(hunits, int, int, node * = 0);
public:
  space_node(hunits d, node *p = 0);
#if 0
  ~space_node();
  void *operator new(size_t);
#endif
  node *copy();
  int nspaces();
  hunits width();
  int discardable();
  int merge_space(hunits, hunits, hunits);
  void freeze_space();
  void is_escape_colon();
  void spread_space(int *, hunits *);
  void tprint(troff_output_file *);
  breakpoint *get_breakpoints(hunits width, int nspaces, breakpoint *rest = 0,
			      int is_inner = 0);
  int nbreaks();
  void split(int, node **, node **);
  void ascii_print(ascii_output_file *);
  int same(node *);
  void asciify(macro *);
  const char *type();
  int force_tprint();
  hyphenation_type get_hyphenation_type();
};

struct width_list {
  hunits width;
  hunits sentence_width;
  width_list *next;
  width_list(hunits, hunits);
  width_list(width_list *);
};

class word_space_node : public space_node {
protected:
  width_list *orig_width;
  unsigned char unformat;
  word_space_node(hunits, int, width_list *, int, node * = 0);
public:
  word_space_node(hunits, width_list *, node * = 0);
  ~word_space_node();
  node *copy();
  int reread(int *);
  int set_unformat_flag();
  void tprint(troff_output_file *);
  int same(node *);
  void asciify(macro *);
  const char *type();
  int merge_space(hunits, hunits, hunits);
  int force_tprint();
};

class unbreakable_space_node : public word_space_node {
  unbreakable_space_node(hunits, int, node * = 0);
public:
  unbreakable_space_node(hunits, node * = 0);
  node *copy();
  int reread(int *);
  int same(node *);
  void asciify(macro *);
  const char *type();
  int force_tprint();
  breakpoint *get_breakpoints(hunits width, int nspaces, breakpoint *rest = 0,
			      int is_inner = 0);
  int nbreaks();
  void split(int, node **, node **);
  int merge_space(hunits, hunits, hunits);
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *ss = 0);
  hyphenation_type get_hyphenation_type();
};

class diverted_space_node : public node {
public:
  vunits n;
  diverted_space_node(vunits d, node *p = 0);
  node *copy();
  int reread(int *);
  int same(node *);
  const char *type();
  int force_tprint();
};

class diverted_copy_file_node : public node {
  symbol filename;
public:
  vunits n;
  diverted_copy_file_node(symbol s, node *p = 0);
  node *copy();
  int reread(int *);
  int same(node *);
  const char *type();
  int force_tprint();
};

class extra_size_node : public node {
  vunits n;
public:
  extra_size_node(vunits i) : n(i) {}
  void set_vertical_size(vertical_size *);
  node *copy();
  int same(node *);
  const char *type();
  int force_tprint();
};

class vertical_size_node : public node {
  vunits n;
public:
  vertical_size_node(vunits i) : n(i) {}
  void set_vertical_size(vertical_size *);
  void asciify(macro *);
  node *copy();
  int set_unformat_flag();
  int same(node *);
  const char *type();
  int force_tprint();
};

class hmotion_node : public node {
protected:
  hunits n;
  unsigned char was_tab;
  unsigned char unformat;
public:
  hmotion_node(hunits i, node *next = 0)
  : node(next), n(i), was_tab(0), unformat(0) {}
  hmotion_node(hunits i, int flag1, int flag2, node *next = 0)
  : node(next), n(i), was_tab(flag1), unformat(flag2) {}
  node *copy();
  int reread(int *);
  int set_unformat_flag();
  void asciify(macro *);
  void tprint(troff_output_file *);
  hunits width();
  void ascii_print(ascii_output_file *);
  int same(node *);
  const char *type();
  int force_tprint();
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *ss = 0);
  hyphenation_type get_hyphenation_type();
};

class space_char_hmotion_node : public hmotion_node {
public:
  space_char_hmotion_node(hunits i, node *next = 0);
  node *copy();
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  int same(node *);
  const char *type();
  int force_tprint();
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *ss = 0);
  hyphenation_type get_hyphenation_type();
};

class vmotion_node : public node {
  vunits n;
public:
  vmotion_node(vunits i) : n(i) {}
  void tprint(troff_output_file *);
  node *copy();
  vunits vertical_width();
  int same(node *);
  const char *type();
  int force_tprint();
};

class hline_node : public node {
  hunits x;
  node *n;
public:
  hline_node(hunits i, node *c, node *next = 0) : node(next), x(i), n(c) {}
  ~hline_node();
  node *copy();
  hunits width();
  void tprint(troff_output_file *);
  int same(node *);
  const char *type();
  int force_tprint();
};

class vline_node : public node {
  vunits x;
  node *n;
public:
  vline_node(vunits i, node *c, node *next= 0) : node(next), x(i), n(c) {}
  ~vline_node();
  node *copy();
  void tprint(troff_output_file *);
  hunits width();
  vunits vertical_width();
  void vertical_extent(vunits *, vunits *);
  int same(node *);
  const char *type();
  int force_tprint();
};


class dummy_node : public node {
public:
  dummy_node(node *nd = 0) : node(nd) {}
  node *copy();
  int same(node *);
  const char *type();
  int force_tprint();
  hyphenation_type get_hyphenation_type();
};

class transparent_dummy_node : public node {
public:
  transparent_dummy_node(node *nd = 0) : node(nd) {}
  node *copy();
  int same(node *);
  const char *type();
  int force_tprint();
  int ends_sentence();
  hyphenation_type get_hyphenation_type();
};

class zero_width_node : public node {
  node *n;
public:
  zero_width_node(node *gn);
  ~zero_width_node();
  node *copy();
  void tprint(troff_output_file *);
  int same(node *);
  const char *type();
  int force_tprint();
  void append(node *);
  int character_type();
  void vertical_extent(vunits *min, vunits *max);
};

class left_italic_corrected_node : public node {
  node *n;
  hunits x;
public:
  left_italic_corrected_node(node * = 0);
  ~left_italic_corrected_node();
  void tprint(troff_output_file *);
  void ascii_print(ascii_output_file *);
  void asciify(macro *);
  node *copy();
  int same(node *);
  const char *type();
  int force_tprint();
  hunits width();
  node *last_char_node();
  void vertical_extent(vunits *, vunits *);
  int ends_sentence();
  int overlaps_horizontally();
  int overlaps_vertically();
  hyphenation_type get_hyphenation_type();
  tfont *get_tfont();
  int character_type();
  hunits skew();
  hunits italic_correction();
  hunits subscript_correction();
  hyphen_list *get_hyphen_list(hyphen_list *ss = 0);
  node *add_self(node *, hyphen_list **);
  node *merge_glyph_node(glyph_node *);
};

class overstrike_node : public node {
  node *list;
  hunits max_width;
public:
  overstrike_node();
  ~overstrike_node();
  node *copy();
  void tprint(troff_output_file *);
  void overstrike(node *);	// add another node to be overstruck
  hunits width();
  int same(node *);
  const char *type();
  int force_tprint();
  node *add_self(node *, hyphen_list **);
  hyphen_list *get_hyphen_list(hyphen_list *ss = 0);
  hyphenation_type get_hyphenation_type();
};

class bracket_node : public node {
  node *list;
  hunits max_width;
public:
  bracket_node();
  ~bracket_node();
  node *copy();
  void tprint(troff_output_file *);
  void bracket(node *);	// add another node to be overstruck
  hunits width();
  int same(node *);
  const char *type();
  int force_tprint();
};

class special_node : public node {
  macro mac;
  tfont *tf;
  int no_init_string;
  void tprint_start(troff_output_file *);
  void tprint_char(troff_output_file *, unsigned char);
  void tprint_end(troff_output_file *);
public:
  special_node(const macro &, int = 0);
  special_node(const macro &, tfont *, int = 0);
  node *copy();
  void tprint(troff_output_file *);
  int same(node *);
  const char *type();
  int force_tprint();
  int ends_sentence();
  tfont *get_tfont();
};

class suppress_node : public node {
  int is_on;
  int emit_limits;	// must we issue the extent of the area written out?
  symbol filename;
  char position;
public:
  suppress_node(int, int);
  suppress_node(symbol f, char p);
  suppress_node(int, int, symbol f, char p);
  node *copy();
  void tprint(troff_output_file *);
  hunits width();
  int same(node *);
  const char *type();
  int force_tprint();
private:
  void put(troff_output_file *out, const char *s);
};

struct hvpair {
  hunits h;
  vunits v;
  hvpair();
};

class draw_node : public node {
  int npoints;
  font_size sz;
  char code;
  hvpair *point;
public:
  draw_node(char, hvpair *, int, font_size);
  ~draw_node();
  hunits width();
  vunits vertical_width();
  node *copy();
  void tprint(troff_output_file *);
  int same(node *);
  const char *type();
  int force_tprint();
};

class charinfo;
node *make_node(charinfo *ci, environment *);
int character_exists(charinfo *, environment *);

int same_node_list(node *n1, node *n2);
node *reverse_node_list(node *n);
void delete_node_list(node *);
node *copy_node_list(node *);

int get_bold_fontno(int f);

inline hyphen_list::hyphen_list(unsigned char code, hyphen_list *p)
: hyphen(0), breakable(0), hyphenation_code(code), next(p)
{
}

extern void read_desc();
extern int mount_font(int n, symbol, symbol = NULL_SYMBOL);
extern void mount_style(int n, symbol);
extern int is_good_fontno(int n);
extern int symbol_fontno(symbol);
extern int next_available_font_position();
extern void init_size_table(int *);
extern int get_underline_fontno();

class output_file {
  char make_g_plus_plus_shut_up;
public:
  output_file();
  virtual ~output_file();
  virtual void trailer(vunits page_length);
  virtual void flush() = 0;
  virtual void transparent_char(unsigned char) = 0;
  virtual void print_line(hunits x, vunits y, node *n,
			  vunits before, vunits after, hunits width) = 0;
  virtual void begin_page(int pageno, vunits page_length) = 0;
  virtual void copy_file(hunits x, vunits y, const char *filename) = 0;
  virtual int is_printing() = 0;
  virtual void put_filename(const char *filename);
  virtual void on();
  virtual void off();
#ifdef COLUMN
  virtual void vjustify(vunits, symbol);
#endif /* COLUMN */
};

#ifndef POPEN_MISSING
extern char *pipe_command;
#endif

extern output_file *the_output;
extern void init_output();
int in_output_page_list(int n);

class font_family {
  int *map;
  int map_size;
public:
  const symbol nm;
  font_family(symbol);
  ~font_family();
  int make_definite(int);
  static void invalidate_fontno(int);
};

font_family *lookup_family(symbol);
