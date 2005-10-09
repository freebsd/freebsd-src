// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2003 Free Software Foundation, Inc.
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

#include "eqn.h"
#include "pbox.h"
#include "ptable.h"

class char_box : public simple_box {
  unsigned char c;
  char next_is_italic;
  char prev_is_italic;
public:
  char_box(unsigned char);
  void debug_print();
  void output();
  int is_char();
  int left_is_italic();
  int right_is_italic();
  void hint(unsigned);
  void handle_char_type(int, int);
};

class special_char_box : public simple_box {
  char *s;
public:
  special_char_box(const char *);
  ~special_char_box();
  void output();
  void debug_print();
  int is_char();
  void handle_char_type(int, int);
};

const char *spacing_type_table[] = {
  "ordinary",
  "operator",
  "binary",
  "relation",
  "opening",
  "closing",
  "punctuation",
  "inner",
  "suppress",
  0,
};

const int DIGIT_TYPE = 0;
const int LETTER_TYPE = 1;

const char *font_type_table[] = {
  "digit",
  "letter",
  0,
};

struct char_info {
  int spacing_type;
  int font_type;
  char_info();
};

char_info::char_info()
: spacing_type(ORDINARY_TYPE), font_type(DIGIT_TYPE)
{
}

static char_info char_table[256];

declare_ptable(char_info)
implement_ptable(char_info)

PTABLE(char_info) special_char_table;

static int get_special_char_spacing_type(const char *ch)
{
  char_info *p = special_char_table.lookup(ch);
  return p ? p->spacing_type : ORDINARY_TYPE;
}

static int get_special_char_font_type(const char *ch)
{
  char_info *p = special_char_table.lookup(ch);
  return p ? p->font_type : DIGIT_TYPE;
}

static void set_special_char_type(const char *ch, int st, int ft)
{
  char_info *p = special_char_table.lookup(ch);
  if (!p) {
    p = new char_info[1];
    special_char_table.define(ch, p);
  }
  if (st >= 0)
    p->spacing_type = st;
  if (ft >= 0)
    p->font_type = ft;
}

void init_char_table()
{
  set_special_char_type("pl", 2, -1); // binary
  set_special_char_type("mi", 2, -1);
  set_special_char_type("eq", 3, -1); // relation
  set_special_char_type("<=", 3, -1);
  set_special_char_type(">=", 3, -1);
  char_table['}'].spacing_type = 5; // closing
  char_table[')'].spacing_type = 5;
  char_table[']'].spacing_type = 5;
  char_table['{'].spacing_type = 4; // opening
  char_table['('].spacing_type = 4;
  char_table['['].spacing_type = 4;
  char_table[','].spacing_type = 6; // punctuation
  char_table[';'].spacing_type = 6;
  char_table[':'].spacing_type = 6;
  char_table['.'].spacing_type = 6;
  char_table['>'].spacing_type = 3;
  char_table['<'].spacing_type = 3;
  char_table['*'].spacing_type = 2; // binary
  for (int i = 0; i < 256; i++)
    if (csalpha(i))
      char_table[i].font_type = LETTER_TYPE;
}

static int lookup_spacing_type(const char *type)
{
  for (int i = 0; spacing_type_table[i] != 0; i++)
    if (strcmp(spacing_type_table[i], type) == 0)
      return i;
  return -1;
}

static int lookup_font_type(const char *type)
{
  for (int i = 0; font_type_table[i] != 0; i++)
    if (strcmp(font_type_table[i], type) == 0)
      return i;
  return -1;
}

void box::set_spacing_type(char *type)
{
  int t = lookup_spacing_type(type);
  if (t < 0)
    error("unrecognised type `%1'", type);
  else
    spacing_type = t;
  a_delete type;
}

char_box::char_box(unsigned char cc)
: c(cc), next_is_italic(0), prev_is_italic(0)
{
  spacing_type = char_table[c].spacing_type;
}

void char_box::hint(unsigned flags)
{
  if (flags & HINT_PREV_IS_ITALIC)
    prev_is_italic = 1;
  if (flags & HINT_NEXT_IS_ITALIC)
    next_is_italic = 1;
}

void char_box::output()
{
  int font_type = char_table[c].font_type;
  if (font_type != LETTER_TYPE)
    printf("\\f[%s]", current_roman_font);
  if (!prev_is_italic)
    fputs("\\,", stdout);
  if (c == '\\')
    fputs("\\e", stdout);
  else
    putchar(c);
  if (!next_is_italic)
    fputs("\\/", stdout);
  else
    fputs("\\&", stdout);		// suppress ligaturing and kerning
  if (font_type != LETTER_TYPE)
    fputs("\\fP", stdout);
}

int char_box::left_is_italic()
{
  int font_type = char_table[c].font_type;
  return font_type == LETTER_TYPE;
}

int char_box::right_is_italic()
{
  int font_type = char_table[c].font_type;
  return font_type == LETTER_TYPE;
}

int char_box::is_char()
{
  return 1;
}

void char_box::debug_print()
{
  if (c == '\\') {
    putc('\\', stderr);
    putc('\\', stderr);
  }
  else
    putc(c, stderr);
}

special_char_box::special_char_box(const char *t)
{
  s = strsave(t);
  spacing_type = get_special_char_spacing_type(s);
}

special_char_box::~special_char_box()
{
  a_delete s;
}

void special_char_box::output()
{
  int font_type = get_special_char_font_type(s);
  if (font_type != LETTER_TYPE)
    printf("\\f[%s]", current_roman_font);
  printf("\\,\\[%s]\\/", s);
  if (font_type != LETTER_TYPE)
    printf("\\fP");
}

int special_char_box::is_char()
{
  return 1;
}

void special_char_box::debug_print()
{
  fprintf(stderr, "\\[%s]", s);
}


void char_box::handle_char_type(int st, int ft)
{
  if (st >= 0)
    char_table[c].spacing_type = st;
  if (ft >= 0)
    char_table[c].font_type = ft;
}

void special_char_box::handle_char_type(int st, int ft)
{
  set_special_char_type(s, st, ft);
}

void set_char_type(const char *type, char *ch)
{
  assert(ch != 0);
  int st = lookup_spacing_type(type);
  int ft = lookup_font_type(type);
  if (st < 0 && ft < 0) {
    error("bad character type `%1'", type);
    a_delete ch;
    return;
  }
  box *b = split_text(ch);
  b->handle_char_type(st, ft);
  delete b;
}

/* We give primes special treatment so that in ``x' sub 2'', the ``2''
will be tucked under the prime */

class prime_box : public pointer_box {
  box *pb;
public:
  prime_box(box *);
  ~prime_box();
  int compute_metrics(int style);
  void output();
  void compute_subscript_kern();
  void debug_print();
  void handle_char_type(int, int);
};

box *make_prime_box(box *pp)
{
  return new prime_box(pp);
}

prime_box::prime_box(box *pp) : pointer_box(pp)
{
  pb = new special_char_box("fm");
}

prime_box::~prime_box()
{
  delete pb;
}

int prime_box::compute_metrics(int style)
{
  int res = p->compute_metrics(style);
  pb->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]"
	 "+\\n[" WIDTH_FORMAT "]\n",
	 uid, p->uid, pb->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]"
	 ">?\\n[" HEIGHT_FORMAT "]\n",
	 uid, p->uid, pb->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]"
	 ">?\\n[" DEPTH_FORMAT "]\n",
	 uid, p->uid, pb->uid);
  return res;
}

void prime_box::compute_subscript_kern()
{
  p->compute_subscript_kern();
  printf(".nr " SUB_KERN_FORMAT " 0\\n[" WIDTH_FORMAT "]"
	 "+\\n[" SUB_KERN_FORMAT "]>?0\n",
	 uid, pb->uid, p->uid);
}

void prime_box::output()
{
  p->output();
  pb->output();
}

void prime_box::handle_char_type(int st, int ft)
{
  p->handle_char_type(st, ft);
  pb->handle_char_type(st, ft);
}

void prime_box::debug_print()
{
  p->debug_print();
  putc('\'', stderr);
}

box *split_text(char *text)
{
  list_box *lb = 0;
  box *fb = 0;
  char *s = text;
  while (*s != '\0') {
    char c = *s++;
    box *b = 0;
    switch (c) {
    case '+':
      b = new special_char_box("pl");
      break;
    case '-':
      b = new special_char_box("mi");
      break;
    case '=':
      b = new special_char_box("eq");
      break;
    case '\'':
      b = new special_char_box("fm");
      break;
    case '<':
      if (*s == '=') {
	b = new special_char_box("<=");
	s++;
	break;
      }
      goto normal_char;
    case '>':
      if (*s == '=') {
	b = new special_char_box(">=");
	s++;
	break;
      }
      goto normal_char;
    case '\\':
      if (*s == '\0') {
	lex_error("bad escape");
	break;
      }
      c = *s++;
      switch (c) {
      case '(':
	{
	  char buf[3];
	  if (*s != '\0') {
	    buf[0] = *s++;
	    if (*s != '\0') {
	      buf[1] = *s++;
	      buf[2] = '\0';
	      b = new special_char_box(buf);
	    }
	    else {
	      lex_error("bad escape");
	    }
	  }
	  else {
	    lex_error("bad escape");
	  }
	}
	break;
      case '[':
	{
	  char *ch = s;
	  while (*s != ']' && *s != '\0')
	    s++;
	  if (*s == '\0')
	    lex_error("bad escape");
	  else {
	    *s++ = '\0';
	    b = new special_char_box(ch);
	  }
	}
	break;
      case 'f':
      case 'g':
      case 'k':
      case 'n':
      case '*':
	{
	  char *escape_start = s - 2;
	  switch (*s) {
	  case '(':
	    if (*++s != '\0')
	      ++s;
	    break;
	  case '[':
	    for (++s; *s != '\0' && *s != ']'; s++)
	      ;
	    break;
	  }
	  if (*s == '\0')
	    lex_error("bad escape");
	  else {
	    ++s;
	    char *buf = new char[s - escape_start + 1];
	    memcpy(buf, escape_start, s - escape_start);
	    buf[s - escape_start] = '\0';
	    b = new quoted_text_box(buf);
	  }
	}
	break;
      case '-':
      case '_':
	{
	  char buf[2];
	  buf[0] = c;
	  buf[1] = '\0';
	  b = new special_char_box(buf);
	}
	break;
      case '`':
	b = new special_char_box("ga");
	break;
      case '\'':
	b = new special_char_box("aa");
	break;
      case 'e':
      case '\\':
	b = new char_box('\\');
	break;
      case '^':
      case '|':
      case '0':
	{
	  char buf[3];
	  buf[0] = '\\';
	  buf[1] = c;
	  buf[2] = '\0';
	  b = new quoted_text_box(strsave(buf));
	  break;
	}
      default:
	lex_error("unquoted escape");
	b = new quoted_text_box(strsave(s - 2));
	s = strchr(s, '\0');
	break;
      }
      break;
    default:
    normal_char:
      b = new char_box(c);
      break;
    }
    while (*s == '\'') {
      if (b == 0)
	b = new quoted_text_box(0);
      b = new prime_box(b);
      s++;
    }
    if (b != 0) {
      if (lb != 0)
	lb->append(b);
      else if (fb != 0) {
	lb = new list_box(fb);
	lb->append(b);
      }
      else
	fb = b;
    }
  }
  a_delete text;
  if (lb != 0)
    return lb;
  else if (fb != 0)
    return fb;
  else
    return new quoted_text_box(0);
}

