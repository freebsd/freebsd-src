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

class over_box : public box {
private:
  int reduce_size;
  box *num;
  box *den;
public:
  over_box(int small, box *, box *);
  ~over_box();
  void debug_print();
  int compute_metrics(int);
  void output();
  void check_tabs(int);
};

box *make_over_box(box *pp, box *qq)
{
  return new over_box(0, pp, qq);
}

box *make_small_over_box(box *pp, box *qq)
{
  return new over_box(1, pp, qq);
}

over_box::over_box(int is_small, box *pp, box *qq)
: num(pp), den(qq), reduce_size(is_small)
{
  spacing_type = INNER_TYPE;
}

over_box::~over_box()
{
  delete num;
  delete den;
}

int over_box::compute_metrics(int style)
{
  if (reduce_size) {
    style = script_style(style);
    printf(".nr " SIZE_FORMAT " \\n[.s]\n", uid);
    set_script_size();
    printf(".nr " SMALL_SIZE_FORMAT " \\n[.s]\n", uid);
  }
  int mark_uid;
  int res = num->compute_metrics(style);
  if (res)
    mark_uid = num->uid;
  int r = den->compute_metrics(cramped_style(style));
  if (r && res)
    error("multiple marks and lineups");
  else {
    mark_uid = den->uid;
    res = r;
  }
  if (reduce_size)
    printf(".ps \\n[" SIZE_FORMAT "]\n", uid);
  printf(".nr " WIDTH_FORMAT " (\\n[" WIDTH_FORMAT "]>?\\n[" WIDTH_FORMAT "]", 
	 uid, num->uid, den->uid);
  // allow for \(ru being wider than both the numerator and denominator
  if (!draw_flag)
    fputs(">?\\w" DELIMITER_CHAR "\\(ru" DELIMITER_CHAR, stdout);
  printf(")+%dM\n", null_delimiter_space*2 + over_hang*2);
  // 15b
  printf(".nr " SUP_RAISE_FORMAT " %dM\n",
	 uid, (reduce_size ? num2 : num1));
  printf(".nr " SUB_LOWER_FORMAT " %dM\n",
	 uid, (reduce_size ? denom2 : denom1));

  // 15d
  printf(".nr " SUP_RAISE_FORMAT " +(\\n[" DEPTH_FORMAT
	 "]-\\n[" SUP_RAISE_FORMAT "]+%dM+(%dM/2)+%dM)>?0\n",
	 uid, num->uid, uid, axis_height, default_rule_thickness,
	 default_rule_thickness*(reduce_size ? 1 : 3));
  printf(".nr " SUB_LOWER_FORMAT " +(\\n[" HEIGHT_FORMAT
	 "]-\\n[" SUB_LOWER_FORMAT "]-%dM+(%dM/2)+%dM)>?0\n",
	 uid, den->uid, uid, axis_height, default_rule_thickness,
	 default_rule_thickness*(reduce_size ? 1 : 3));


  printf(".nr " HEIGHT_FORMAT " \\n[" SUP_RAISE_FORMAT "]+\\n["
	 HEIGHT_FORMAT "]\n",
	 uid, uid, num->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" SUB_LOWER_FORMAT "]+\\n["
	 DEPTH_FORMAT "]\n",
	 uid, uid, den->uid);
  if (res)
    printf(".nr " MARK_REG " +(\\n[" WIDTH_FORMAT "]-\\n["
	   WIDTH_FORMAT "]/2)\n", uid, mark_uid);
  return res;
}

#define USE_Z

void over_box::output()
{
  if (reduce_size)
    printf("\\s[\\n[" SMALL_SIZE_FORMAT "]]", uid);
#ifdef USE_Z
  printf("\\Z" DELIMITER_CHAR);
#endif
  // move up to the numerator baseline
  printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid);
  // move across so that it's centered
  printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u'",
	 uid, num->uid);

  // print the numerator
  num->output();

#ifdef USE_Z
  printf(DELIMITER_CHAR);
#else
  // back again
  printf("\\h'-\\n[" WIDTH_FORMAT "]u'", num->uid);
  printf("\\h'-(\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, num->uid);
  // down again
  printf("\\v'\\n[" SUP_RAISE_FORMAT "]u'", uid);
#endif
#ifdef USE_Z
  printf("\\Z" DELIMITER_CHAR);
#endif
  // move down to the denominator baseline
  printf("\\v'\\n[" SUB_LOWER_FORMAT "]u'", uid);

  // move across so that it's centered
  printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u'",
	 uid, den->uid);

  // print the the denominator
  den->output();

#ifdef USE_Z
  printf(DELIMITER_CHAR);
#else
  // back again
  printf("\\h'-\\n[" WIDTH_FORMAT "]u'", den->uid);
  printf("\\h'-(\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u)'",
	 uid, den->uid);
  // up again
  printf("\\v'-\\n[" SUB_LOWER_FORMAT "]u'", uid);
#endif
  if (reduce_size)
    printf("\\s[\\n[" SIZE_FORMAT "]]", uid);
  // draw the line
  printf("\\h'%dM'", null_delimiter_space);
  printf("\\v'-%dM'", axis_height);
  fputs(draw_flag ? "\\D'l" : "\\l'", stdout);
  printf("\\n[" WIDTH_FORMAT "]u-%dM",
	 uid, 2*null_delimiter_space);
  fputs(draw_flag ? " 0'" : "\\&\\(ru'", stdout);
  printf("\\v'%dM'", axis_height);
  printf("\\h'%dM'", null_delimiter_space);
}

void over_box::debug_print()
{
  fprintf(stderr, "{ ");
  num->debug_print();
  if (reduce_size)
    fprintf(stderr, " } smallover { ");
  else
    fprintf(stderr, " } over { ");
  den->debug_print();
  fprintf(stderr, " }");
}

void over_box::check_tabs(int level)
{
  num->check_tabs(level + 1);
  den->check_tabs(level + 1);
}
