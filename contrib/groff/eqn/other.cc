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

#include "eqn.h"
#include "pbox.h"

class accent_box : public pointer_box {
private:
  box *ab;
public:
  accent_box(box *, box *);
  ~accent_box();
  int compute_metrics(int);
  void output();
  void debug_print();
  void check_tabs(int);
};

box *make_accent_box(box *p, box *q)
{
  return new accent_box(p, q);
}

accent_box::accent_box(box *pp, box *qq) : ab(qq), pointer_box(pp)
{
}

accent_box::~accent_box()
{
  delete ab;
}

#if 0
int accent_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  p->compute_skew();
  ab->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " SUP_RAISE_FORMAT " \\n[" HEIGHT_FORMAT "]-%dM>?0\n",
	 uid, p->uid, x_height);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]+\\n["
	 SUP_RAISE_FORMAT "]\n",
	 uid, ab->uid, uid);
  return r;
}

void accent_box::output()
{
  printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u+\\n["
	 SKEW_FORMAT "]u'",
	 p->uid, ab->uid, p->uid);
  printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid); 
  ab->output();
  printf("\\h'-\\n[" WIDTH_FORMAT "]u'", ab->uid);
  printf("\\v'\\n[" SUP_RAISE_FORMAT "]u'", uid);
  printf("\\h'-(\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u+\\n["
	 SKEW_FORMAT "]u)'",
	 p->uid, ab->uid, p->uid);
  p->output();
}
#endif

/* This version copes with the possibility of an accent's being wider
than its accentee.  LEFT_WIDTH_FORMAT gives the distance from the
left edge of the resulting box to the middle of the accentee's box.*/

int accent_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  p->compute_skew();
  ab->compute_metrics(style);
  printf(".nr " LEFT_WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]/2"
	 ">?(\\n[" WIDTH_FORMAT "]/2-\\n[" SKEW_FORMAT "])\n",
	 uid, p->uid, ab->uid, p->uid);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]/2"
	 ">?(\\n[" WIDTH_FORMAT "]/2+\\n[" SKEW_FORMAT "])"
	 "+\\n[" LEFT_WIDTH_FORMAT "]\n",
	 uid, p->uid, ab->uid, p->uid, uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " SUP_RAISE_FORMAT " \\n[" HEIGHT_FORMAT "]-%dM>?0\n",
	 uid, p->uid, x_height);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]+\\n["
	 SUP_RAISE_FORMAT "]\n",
	 uid, ab->uid, uid);
  if (r)
    printf(".nr " MARK_REG " +\\n[" LEFT_WIDTH_FORMAT "]"
	   "-(\\n[" WIDTH_FORMAT "]/2)'\n",
	   uid, p->uid);
  return r;
}

void accent_box::output()
{
  printf("\\Z" DELIMITER_CHAR);
  printf("\\h'\\n[" LEFT_WIDTH_FORMAT "]u+\\n[" SKEW_FORMAT "]u"
	 "-(\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, p->uid, ab->uid);
  printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid); 
  ab->output();
  printf(DELIMITER_CHAR);
  printf("\\Z" DELIMITER_CHAR);
  printf("\\h'\\n[" LEFT_WIDTH_FORMAT "]u-(\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, p->uid);
  p->output();
  printf(DELIMITER_CHAR);
  printf("\\h'\\n[" WIDTH_FORMAT "]u'", uid);
}

void accent_box::check_tabs(int level)
{
  ab->check_tabs(level + 1);
  p->check_tabs(level + 1);
}

void accent_box::debug_print()
{
  fprintf(stderr, "{ ");
  p->debug_print();
  fprintf(stderr, " } accent { ");
  ab->debug_print();
  fprintf(stderr, " }");
}

class overline_char_box : public simple_box {
public:
  overline_char_box();
  void output();
  void debug_print();
};

overline_char_box::overline_char_box()
{
}

void overline_char_box::output()
{
  printf("\\v'-%dM/2u-%dM'", 7*default_rule_thickness, x_height);
  printf((draw_flag ? "\\D'l%dM 0'" : "\\l'%dM\\&\\(ru'"),
	 accent_width);
  printf("\\v'%dM/2u+%dM'", 7*default_rule_thickness, x_height);
}

void overline_char_box::debug_print()
{
  fprintf(stderr, "<overline char>");
}

class overline_box : public pointer_box {
public:
  overline_box(box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

box *make_overline_box(box *p)
{
  if (p->is_char())
    return new accent_box(p, new overline_char_box);
  else
    return new overline_box(p);
}

overline_box::overline_box(box *pp) : pointer_box(pp)
{
}

int overline_box::compute_metrics(int style)
{
  int r = p->compute_metrics(cramped_style(style));
  // 9
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]+%dM\n",
	 uid, p->uid, default_rule_thickness*5);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  return r;
}

void overline_box::output()
{
  // 9
  printf("\\Z" DELIMITER_CHAR);
  printf("\\v'-\\n[" HEIGHT_FORMAT "]u-(%dM/2u)'",
	 p->uid, 7*default_rule_thickness);
  if (draw_flag)
    printf("\\D'l\\n[" WIDTH_FORMAT "]u 0'", p->uid);
  else
    printf("\\l'\\n[" WIDTH_FORMAT "]u\\&\\(ru'", p->uid);
  printf(DELIMITER_CHAR);
  p->output();
}

void overline_box::debug_print()
{
  fprintf(stderr, "{ ");
  p->debug_print();
  fprintf(stderr, " } bar");
}

class uaccent_box : public pointer_box {
  box *ab;
public:
  uaccent_box(box *, box *);
  ~uaccent_box();
  int compute_metrics(int);
  void output();
  void compute_subscript_kern();
  void check_tabs(int);
  void debug_print();
};

box *make_uaccent_box(box *p, box *q)
{
  return new uaccent_box(p, q);
}

uaccent_box::uaccent_box(box *pp, box *qq)
: pointer_box(pp), ab(qq)
{
}

uaccent_box::~uaccent_box()
{
  delete ab;
}

int uaccent_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  ab->compute_metrics(style);
  printf(".nr " LEFT_WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]/2"
	 ">?(\\n[" WIDTH_FORMAT "]/2)\n",
	 uid, p->uid, ab->uid);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]/2"
	 ">?(\\n[" WIDTH_FORMAT "]/2)"
	 "+\\n[" LEFT_WIDTH_FORMAT "]\n",
	 uid, p->uid, ab->uid, uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]"
	 "+\\n[" DEPTH_FORMAT "]\n",
	 uid, p->uid, ab->uid);
  if (r)
    printf(".nr " MARK_REG " +\\n[" LEFT_WIDTH_FORMAT "]"
	   "-(\\n[" WIDTH_FORMAT "]/2)'\n",
	   uid, p->uid);
  return r;
}

void uaccent_box::output()
{
  printf("\\Z" DELIMITER_CHAR);
  printf("\\h'\\n[" LEFT_WIDTH_FORMAT "]u-(\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, ab->uid);
  printf("\\v'\\n[" DEPTH_FORMAT "]u'", p->uid); 
  ab->output();
  printf(DELIMITER_CHAR);
  printf("\\Z" DELIMITER_CHAR);
  printf("\\h'\\n[" LEFT_WIDTH_FORMAT "]u-(\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, p->uid);
  p->output();
  printf(DELIMITER_CHAR);
  printf("\\h'\\n[" WIDTH_FORMAT "]u'", uid);
}

void uaccent_box::check_tabs(int level)
{
  ab->check_tabs(level + 1);
  p->check_tabs(level + 1);
}

void uaccent_box::compute_subscript_kern()
{
  box::compute_subscript_kern(); // want 0 subscript kern
}

void uaccent_box::debug_print()
{
  fprintf(stderr, "{ ");
  p->debug_print();
  fprintf(stderr, " } uaccent { ");
  ab->debug_print();
  fprintf(stderr, " }");
}

class underline_char_box : public simple_box {
public:
  underline_char_box();
  void output();
  void debug_print();
};

underline_char_box::underline_char_box()
{
}

void underline_char_box::output()
{
  printf("\\v'%dM/2u'", 7*default_rule_thickness);
  printf((draw_flag ? "\\D'l%dM 0'" : "\\l'%dM\\&\\(ru'"),
	 accent_width);
  printf("\\v'-%dM/2u'", 7*default_rule_thickness);
}

void underline_char_box::debug_print()
{
  fprintf(stderr, "<underline char>");
}


class underline_box : public pointer_box {
public:
  underline_box(box *);
  int compute_metrics(int);
  void output();
  void compute_subscript_kern();
  void debug_print();
};

box *make_underline_box(box *p)
{
  if (p->is_char())
    return new uaccent_box(p, new underline_char_box);
  else
    return new underline_box(p);
}

underline_box::underline_box(box *pp) : pointer_box(pp)
{
}

int underline_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  // 10
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]+%dM\n",
	 uid, p->uid, default_rule_thickness*5);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  return r;
}

void underline_box::output()
{
  // 10
  printf("\\Z" DELIMITER_CHAR);
  printf("\\v'\\n[" DEPTH_FORMAT "]u+(%dM/2u)'",
	 p->uid, 7*default_rule_thickness);
  if (draw_flag)
    printf("\\D'l\\n[" WIDTH_FORMAT "]u 0'", p->uid);
  else
    printf("\\l'\\n[" WIDTH_FORMAT "]u\\&\\(ru'", p->uid);
  printf(DELIMITER_CHAR);
  p->output();
}

// we want an underline box to have 0 subscript kern

void underline_box::compute_subscript_kern()
{
  box::compute_subscript_kern();
}

void underline_box::debug_print()
{
  fprintf(stderr, "{ ");
  p->debug_print();
  fprintf(stderr, " } under");
}

size_box::size_box(char *s, box *pp) : size(s), pointer_box(pp)
{
}

int size_box::compute_metrics(int style)
{
  printf(".nr " SIZE_FORMAT " \\n[.s]\n", uid);
  printf(".ps %s\n", size);
  printf(".nr " SMALL_SIZE_FORMAT " \\n[.s]\n", uid);
  int r = p->compute_metrics(style);
  printf(".ps \\n[" SIZE_FORMAT "]\n", uid);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  return r;
}

void size_box::output()
{
  printf("\\s[\\n[" SMALL_SIZE_FORMAT "]]", uid);
  p->output();
  printf("\\s[\\n[" SIZE_FORMAT "]]", uid);
}

size_box::~size_box()
{
  a_delete size;
}

void size_box::debug_print()
{
  fprintf(stderr, "size %s { ", size);
  p->debug_print();
  fprintf(stderr, " }");
}


font_box::font_box(char *s, box *pp) : pointer_box(pp), f(s)
{
}

font_box::~font_box()
{
  a_delete f;
}

int font_box::compute_metrics(int style)
{
  const char *old_roman_font = current_roman_font;
  current_roman_font = f;
  printf(".nr " FONT_FORMAT " \\n[.f]\n", uid);
  printf(".ft %s\n", f);
  int r = p->compute_metrics(style);
  current_roman_font = old_roman_font;
  printf(".ft \\n[" FONT_FORMAT "]\n", uid);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  return r;
}

void font_box::output()
{
  printf("\\f[%s]", f);
  const char *old_roman_font = current_roman_font;
  current_roman_font = f;
  p->output();
  current_roman_font = old_roman_font;
  printf("\\f[\\n[" FONT_FORMAT "]]", uid);
}

void font_box::debug_print()
{
  fprintf(stderr, "font %s { ", f);
  p->debug_print();
  fprintf(stderr, " }");
}

fat_box::fat_box(box *pp) : pointer_box(pp)
{
}

int fat_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]+%dM\n",
	 uid, p->uid, fat_offset);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  return r;
}

void fat_box::output()
{
  p->output();
  printf("\\h'-\\n[" WIDTH_FORMAT "]u'", p->uid);
  printf("\\h'%dM'", fat_offset);
  p->output();
}


void fat_box::debug_print()
{
  fprintf(stderr, "fat { ");
  p->debug_print();
  fprintf(stderr, " }");
}


vmotion_box::vmotion_box(int i, box *pp) : n(i), pointer_box(pp)
{
}

int vmotion_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  if (n > 0) {
    printf(".nr " HEIGHT_FORMAT " %dM+\\n[" HEIGHT_FORMAT "]\n",
	   uid, n, p->uid);
    printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  }
  else {
    printf(".nr " DEPTH_FORMAT " %dM+\\n[" DEPTH_FORMAT "]>?0\n",
	   uid, -n, p->uid);
    printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n",
	   uid, p->uid);
  }
  return r;
}

void vmotion_box::output()
{
  printf("\\v'%dM'", -n);
  p->output();
  printf("\\v'%dM'", n);
}

void vmotion_box::debug_print()
{
  if (n >= 0)
    fprintf(stderr, "up %d { ", n);
  else
    fprintf(stderr, "down %d { ", -n);
  p->debug_print();
  fprintf(stderr, " }");
}

hmotion_box::hmotion_box(int i, box *pp) : n(i), pointer_box(pp)
{
}

int hmotion_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]+%dM\n",
	 uid, p->uid, n);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  if (r)
    printf(".nr " MARK_REG " +%dM\n", n);
  return r;
}

void hmotion_box::output()
{
  printf("\\h'%dM'", n);
  p->output();
}

void hmotion_box::debug_print()
{
  if (n >= 0)
    fprintf(stderr, "fwd %d { ", n);
  else
    fprintf(stderr, "back %d { ", -n);
  p->debug_print();
  fprintf(stderr, " }");
}

vcenter_box::vcenter_box(box *pp) : pointer_box(pp)
{
}

int vcenter_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " SUP_RAISE_FORMAT " \\n[" DEPTH_FORMAT "]-\\n["
	 HEIGHT_FORMAT "]/2+%dM\n",
	 uid, p->uid, p->uid, axis_height);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]+\\n["
	 SUP_RAISE_FORMAT "]>?0\n", uid, p->uid, uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]-\\n["
	 SUP_RAISE_FORMAT "]>?0\n", uid, p->uid, uid);

  return r;
}

void vcenter_box::output()
{
  printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid);
  p->output();
  printf("\\v'\\n[" SUP_RAISE_FORMAT "]u'", uid);
}

void vcenter_box::debug_print()
{
  fprintf(stderr, "vcenter { ");
  p->debug_print();
  fprintf(stderr, " }");
}

