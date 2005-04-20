// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2002 Free Software Foundation, Inc.
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

const char *current_roman_font;

char *gfont = 0;
char *grfont = 0;
char *gbfont = 0;
int gsize = 0;

int script_size_reduction = -1;	// negative means reduce by a percentage 

int positive_space = -1;
int negative_space = -1;

int minimum_size = 5;

int fat_offset = 4;
int body_height = 85;
int body_depth = 35;

int over_hang = 0;
int accent_width = 31;
int delimiter_factor = 900;
int delimiter_shortfall = 50;

int null_delimiter_space = 12;
int script_space = 5;
int thin_space = 17;
int medium_space = 22;
int thick_space = 28;

int num1 = 70;
int num2 = 40;
// we don't use num3, because we don't have \atop
int denom1 = 70;
int denom2 = 36;
int axis_height = 26;		// in 100ths of an em
int sup1 = 42;
int sup2 = 37;
int sup3 = 28;
int default_rule_thickness = 4;
int sub1 = 20;
int sub2 = 23;
int sup_drop = 38;
int sub_drop = 5;
int x_height = 45;
int big_op_spacing1 = 11;
int big_op_spacing2 = 17;
int big_op_spacing3 = 20;
int big_op_spacing4 = 60;
int big_op_spacing5 = 10;

// These are for piles and matrices.

int baseline_sep = 140;		// = num1 + denom1
int shift_down = 26;		// = axis_height
int column_sep = 100;		// = em space
int matrix_side_sep = 17;	// = thin space

int nroff = 0;			// should we grok ndefine or tdefine?

struct {
  const char *name;
  int *ptr;
} param_table[] = {
  { "fat_offset", &fat_offset },
  { "over_hang", &over_hang },
  { "accent_width", &accent_width },
  { "delimiter_factor", &delimiter_factor },
  { "delimiter_shortfall", &delimiter_shortfall },
  { "null_delimiter_space", &null_delimiter_space },
  { "script_space", &script_space },
  { "thin_space", &thin_space },
  { "medium_space", &medium_space },
  { "thick_space", &thick_space },
  { "num1", &num1 },
  { "num2", &num2 },
  { "denom1", &denom1 },
  { "denom2", &denom2 },
  { "axis_height", &axis_height },
  { "sup1", &sup1 },
  { "sup2", &sup2 },
  { "sup3", &sup3 },
  { "default_rule_thickness", &default_rule_thickness },
  { "sub1", &sub1 },
  { "sub2", &sub2 },
  { "sup_drop", &sup_drop },
  { "sub_drop", &sub_drop },
  { "x_height", &x_height },
  { "big_op_spacing1", &big_op_spacing1 },
  { "big_op_spacing2", &big_op_spacing2 },
  { "big_op_spacing3", &big_op_spacing3 },
  { "big_op_spacing4", &big_op_spacing4 },
  { "big_op_spacing5", &big_op_spacing5 },
  { "minimum_size", &minimum_size },
  { "baseline_sep", &baseline_sep },
  { "shift_down", &shift_down },
  { "column_sep", &column_sep },
  { "matrix_side_sep", &matrix_side_sep },
  { "draw_lines", &draw_flag },
  { "body_height", &body_height },
  { "body_depth", &body_depth },
  { "nroff", &nroff },
  { 0, 0 }
};

void set_param(const char *name, int value)
{
  for (int i = 0; param_table[i].name != 0; i++)
    if (strcmp(param_table[i].name, name) == 0) {
      *param_table[i].ptr = value;
      return;
    }
  error("unrecognised parameter `%1'", name);
}

int script_style(int style)
{
  return style > SCRIPT_STYLE ? style - 2 : style;
}

int cramped_style(int style)
{
  return (style & 1) ? style - 1 : style;
}

void set_space(int n)
{
  if (n < 0)
    negative_space = -n;
  else
    positive_space = n;
}

// Return 0 if the specified size is bad.
// The caller is responsible for giving the error message.

int set_gsize(const char *s)
{
  const char *p = (*s == '+' || *s == '-') ? s + 1 : s;
  char *end;
  long n = strtol(p, &end, 10);
  if (n <= 0 || *end != '\0' || n > INT_MAX)
    return 0;
  if (p > s) {
    if (!gsize)
      gsize = 10;
    if (*s == '+') {
      if (gsize > INT_MAX - n)
	return 0;
      gsize += int(n);
    }
    else {
      if (gsize - n <= 0)
	return 0;
      gsize -= int(n);
    }
  }
  else
    gsize = int(n);
  return 1;
}

void set_script_reduction(int n)
{
  script_size_reduction = n;
}

const char *get_gfont()
{
  return gfont ? gfont : "I";
}

const char *get_grfont()
{
  return grfont ? grfont : "R";
}

const char *get_gbfont()
{
  return gbfont ? gbfont : "B";
}

void set_gfont(const char *s)
{
  a_delete gfont;
  gfont = strsave(s);
}

void set_grfont(const char *s)
{
  a_delete grfont;
  grfont = strsave(s);
}

void set_gbfont(const char *s)
{
  a_delete gbfont;
  gbfont = strsave(s);
}

// this must be precisely 2 characters in length
#define COMPATIBLE_REG "0C"

void start_string()
{
  printf(".nr " COMPATIBLE_REG " \\n(.C\n");
  printf(".cp 0\n");
  printf(".ds " LINE_STRING "\n");
}

void output_string()
{
  printf("\\*(" LINE_STRING "\n");
}

void restore_compatibility()
{
  printf(".cp \\n(" COMPATIBLE_REG "\n");
}

void do_text(const char *s)
{
  printf(".eo\n");
  printf(".as " LINE_STRING " \"%s\n", s);
  printf(".ec\n");
}

void set_minimum_size(int n)
{
  minimum_size = n;
}

void set_script_size()
{
  if (minimum_size < 0)
    minimum_size = 0;
  if (script_size_reduction >= 0)
    printf(".ps \\n[.s]-%d>?%d\n", script_size_reduction, minimum_size);
  else
    printf(".ps (u;\\n[.ps]*7+5/10>?%d)\n", minimum_size);
}

int box::next_uid = 0;

box::box() : spacing_type(ORDINARY_TYPE), uid(next_uid++)
{
}

box::~box()
{
}

void box::top_level()
{
  // debug_print();
  // putc('\n', stderr);
  box *b = this;
  printf(".nr " SAVED_FONT_REG " \\n[.f]\n");
  printf(".ft\n");
  printf(".nr " SAVED_PREV_FONT_REG " \\n[.f]\n");
  printf(".ft %s\n", get_gfont());
  printf(".nr " SAVED_SIZE_REG " \\n[.ps]\n");
  if (gsize > 0) {
    char buf[INT_DIGITS + 1];
    sprintf(buf, "%d", gsize);
    b = new size_box(strsave(buf), b);
  }
  current_roman_font = get_grfont();
  // This catches tabs used within \Z (which aren't allowed).
  b->check_tabs(0);
  int r = b->compute_metrics(DISPLAY_STYLE);
  printf(".ft \\n[" SAVED_PREV_FONT_REG "]\n");
  printf(".ft \\n[" SAVED_FONT_REG "]\n");
  printf(".nr " MARK_OR_LINEUP_FLAG_REG " %d\n", r);
  if (r == FOUND_MARK) {
    printf(".nr " SAVED_MARK_REG " \\n[" MARK_REG "]\n");
    printf(".nr " MARK_WIDTH_REG " 0\\n[" WIDTH_FORMAT "]\n", b->uid);
  }
  else if (r == FOUND_LINEUP)
    printf(".if r" SAVED_MARK_REG " .as1 " LINE_STRING " \\h'\\n["
	   SAVED_MARK_REG "]u-\\n[" MARK_REG "]u'\n");
  else
    assert(r == FOUND_NOTHING);
  // The problem here is that the argument to \f is read in copy mode,
  // so we cannot use \E there; so we hide it in a string instead.
  // Another problem is that if we use \R directly, then the space will
  // prevent it working in a macro argument.
  printf(".ds " SAVE_FONT_STRING " "
	 "\\R'" SAVED_INLINE_FONT_REG " \\\\n[.f]'"
	 "\\fP"
	 "\\R'" SAVED_INLINE_PREV_FONT_REG " \\\\n[.f]'"
	 "\\R'" SAVED_INLINE_SIZE_REG " \\\\n[.ps]'"
	 "\\s0"
	 "\\R'" SAVED_INLINE_PREV_SIZE_REG " \\\\n[.ps]'"
	 "\n"
	 ".ds " RESTORE_FONT_STRING " "
	 "\\f[\\\\n[" SAVED_INLINE_PREV_FONT_REG "]]"
	 "\\f[\\\\n[" SAVED_INLINE_FONT_REG "]]"
	 "\\s'\\\\n[" SAVED_INLINE_PREV_SIZE_REG "]u'"
	 "\\s'\\\\n[" SAVED_INLINE_SIZE_REG "]u'"
	 "\n");
  printf(".as1 " LINE_STRING " \\&\\E*[" SAVE_FONT_STRING "]");
  printf("\\f[%s]", get_gfont());
  printf("\\s'\\En[" SAVED_SIZE_REG "]u'");
  current_roman_font = get_grfont();
  b->output();
  printf("\\E*[" RESTORE_FONT_STRING "]\n");
  if (r == FOUND_LINEUP)
    printf(".if r" SAVED_MARK_REG " .as1 " LINE_STRING " \\h'\\n["
	   MARK_WIDTH_REG "]u-\\n[" SAVED_MARK_REG "]u-(\\n["
	   WIDTH_FORMAT "]u-\\n[" MARK_REG "]u)'\n",
	   b->uid);
  b->extra_space();
  if (!inline_flag)
    printf(".ne \\n[" HEIGHT_FORMAT "]u-%dM>?0+(\\n["
	   DEPTH_FORMAT "]u-%dM>?0)\n",
	   b->uid, body_height, b->uid, body_depth);
  delete b;
  next_uid = 0;
}

// gpic defines this register so as to make geqn not produce `\x's
#define EQN_NO_EXTRA_SPACE_REG "0x"

void box::extra_space()
{
  printf(".if !r" EQN_NO_EXTRA_SPACE_REG " "
	 ".nr " EQN_NO_EXTRA_SPACE_REG " 0\n");
  if (positive_space >= 0 || negative_space >= 0) {
    if (positive_space > 0)
      printf(".if !\\n[" EQN_NO_EXTRA_SPACE_REG "] "
	     ".as1 " LINE_STRING " \\x'-%dM'\n", positive_space);
    if (negative_space > 0)
      printf(".if !\\n[" EQN_NO_EXTRA_SPACE_REG "] "
	     ".as1 " LINE_STRING " \\x'%dM'\n", negative_space);
    positive_space = negative_space = -1;
  }
  else {
    printf(".if !\\n[" EQN_NO_EXTRA_SPACE_REG "] "
	   ".if \\n[" HEIGHT_FORMAT "]>%dM .as1 " LINE_STRING
	   " \\x'-(\\n[" HEIGHT_FORMAT
	   "]u-%dM)'\n",
	   uid, body_height, uid, body_height);
    printf(".if !\\n[" EQN_NO_EXTRA_SPACE_REG "] "
	   ".if \\n[" DEPTH_FORMAT "]>%dM .as1 " LINE_STRING
	   " \\x'\\n[" DEPTH_FORMAT
	   "]u-%dM'\n",
	   uid, body_depth, uid, body_depth);
  }
}

int box::compute_metrics(int)
{
  printf(".nr " WIDTH_FORMAT " 0\n", uid);
  printf(".nr " HEIGHT_FORMAT " 0\n", uid);
  printf(".nr " DEPTH_FORMAT " 0\n", uid);
  return FOUND_NOTHING;
}

void box::compute_subscript_kern()
{
  printf(".nr " SUB_KERN_FORMAT " 0\n", uid);
}

void box::compute_skew()
{
  printf(".nr " SKEW_FORMAT " 0\n", uid);
}

void box::output()
{
}

void box::check_tabs(int)
{
}

int box::is_char()
{
  return 0;
}

int box::left_is_italic()
{
  return 0;
}

int box::right_is_italic()
{
  return 0;
}

void box::hint(unsigned)
{
}
  
void box::handle_char_type(int, int)
{
}


box_list::box_list(box *pp)
{
  p = new box*[10];
  for (int i = 0; i < 10; i++)
    p[i] = 0;
  maxlen = 10;
  len = 1;
  p[0] = pp;
}

void box_list::append(box *pp)
{
  if (len + 1 > maxlen) {
    box **oldp = p;
    maxlen *= 2;
    p = new box*[maxlen];
    memcpy(p, oldp, sizeof(box*)*len);
    a_delete oldp;
  }
  p[len++] = pp;
}

box_list::~box_list()
{
  for (int i = 0; i < len; i++)
    delete p[i];
  a_delete p;
}

void box_list::list_check_tabs(int level)
{
  for (int i = 0; i < len; i++)
    p[i]->check_tabs(level);
}


pointer_box::pointer_box(box *pp) : p(pp)
{
  spacing_type = p->spacing_type;
}

pointer_box::~pointer_box()
{
  delete p;
}

int pointer_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  return r;
}

void pointer_box::compute_subscript_kern()
{
  p->compute_subscript_kern();
  printf(".nr " SUB_KERN_FORMAT " \\n[" SUB_KERN_FORMAT "]\n", uid, p->uid);
}

void pointer_box::compute_skew()
{
  p->compute_skew();
  printf(".nr " SKEW_FORMAT " 0\\n[" SKEW_FORMAT "]\n",
	 uid, p->uid);
}

void pointer_box::check_tabs(int level)
{
  p->check_tabs(level);
}

int simple_box::compute_metrics(int)
{
  printf(".nr " WIDTH_FORMAT " 0\\w" DELIMITER_CHAR, uid);
  output();
  printf(DELIMITER_CHAR "\n");
  printf(".nr " HEIGHT_FORMAT " 0>?\\n[rst]\n", uid);
  printf(".nr " DEPTH_FORMAT " 0-\\n[rsb]>?0\n", uid);
  printf(".nr " SUB_KERN_FORMAT " 0-\\n[ssc]>?0\n", uid);
  printf(".nr " SKEW_FORMAT " 0\\n[skw]\n", uid);
  return FOUND_NOTHING;
}

void simple_box::compute_subscript_kern()
{
  // do nothing, we already computed it in do_metrics
}

void simple_box::compute_skew()
{
  // do nothing, we already computed it in do_metrics
}

int box::is_simple()
{
  return 0;
}

int simple_box::is_simple()
{
  return 1;
}

quoted_text_box::quoted_text_box(char *s) : text(s)
{
}

quoted_text_box::~quoted_text_box()
{
  a_delete text;
}

void quoted_text_box::output()
{
  if (text)
    fputs(text, stdout);
}

tab_box::tab_box() : disabled(0)
{
}

// We treat a tab_box as having width 0 for width computations.

void tab_box::output()
{
  if (!disabled)
    printf("\\t");
}

void tab_box::check_tabs(int level)
{
  if (level > 0) {
    error("tabs allowed only at outermost level");
    disabled = 1;
  }
}

space_box::space_box()
{
  spacing_type = SUPPRESS_TYPE;
}

void space_box::output()
{
  printf("\\h'%dM'", thick_space);
}

half_space_box::half_space_box()
{
  spacing_type = SUPPRESS_TYPE;
}

void half_space_box::output()
{
  printf("\\h'%dM'", thin_space);
}

void box_list::list_debug_print(const char *sep)
{
  p[0]->debug_print();
  for (int i = 1; i < len; i++) {
    fprintf(stderr, "%s", sep);
    p[i]->debug_print();
  }
}

void quoted_text_box::debug_print()
{
  fprintf(stderr, "\"%s\"", (text ? text : ""));
}

void half_space_box::debug_print()
{
  fprintf(stderr, "^");
}

void space_box::debug_print()
{
  fprintf(stderr, "~");
}

void tab_box::debug_print()
{
  fprintf(stderr, "<tab>");
}
