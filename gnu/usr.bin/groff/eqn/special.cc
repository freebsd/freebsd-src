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

#include "eqn.h"
#include "pbox.h"

#define STRING_FORMAT PREFIX "str%d"

#define SPECIAL_STRING "0s"
#define SPECIAL_WIDTH_REG "0w"
#define SPECIAL_HEIGHT_REG "0h"
#define SPECIAL_DEPTH_REG "0d"
#define SPECIAL_SUB_KERN_REG "0skern"
#define SPECIAL_SKEW_REG "0skew"

/*
For example:

.de Cl
.ds 0s \Z'\\*[0s]'\v'\\n(0du'\D'l \\n(0wu -\\n(0hu-\\n(0du'\v'\\n(0hu'
..
.EQ
define cancel 'special Cl'
.EN
*/


class special_box : public pointer_box {
  char *macro_name;
public:
  special_box(char *, box *);
  ~special_box();
  int compute_metrics(int);
  void compute_subscript_kern();
  void compute_skew();
  void output();
  void debug_print();
};

box *make_special_box(char *s, box *p)
{
  return new special_box(s, p);
}

special_box::special_box(char *s, box *pp) :macro_name(s), pointer_box(pp)
{
}

special_box::~special_box()
{
  a_delete macro_name;
}

int special_box::compute_metrics(int style)
{
  int r = p->compute_metrics(style);
  p->compute_subscript_kern();
  p->compute_skew();
  printf(".ds " SPECIAL_STRING " \"");
  p->output();
  printf("\n");
  printf(".nr " SPECIAL_WIDTH_REG " 0\\n[" WIDTH_FORMAT "]\n", p->uid);
  printf(".nr " SPECIAL_HEIGHT_REG " \\n[" HEIGHT_FORMAT "]\n", p->uid);
  printf(".nr " SPECIAL_DEPTH_REG " \\n[" DEPTH_FORMAT "]\n", p->uid);
  printf(".nr " SPECIAL_SUB_KERN_REG " \\n[" SUB_KERN_FORMAT "]\n", p->uid);
  printf(".nr " SPECIAL_SKEW_REG " 0\\n[" SKEW_FORMAT "]\n", p->uid);
  printf(".%s\n", macro_name);
  printf(".rn " SPECIAL_STRING " " STRING_FORMAT "\n", uid);
  printf(".nr " WIDTH_FORMAT " 0\\n[" SPECIAL_WIDTH_REG "]\n", uid);
  printf(".nr " HEIGHT_FORMAT " 0>?\\n[" SPECIAL_HEIGHT_REG "]\n", uid);
  printf(".nr " DEPTH_FORMAT " 0>?\\n[" SPECIAL_DEPTH_REG "]\n", uid);
  printf(".nr " SUB_KERN_FORMAT " 0>?\\n[" SPECIAL_SUB_KERN_REG "]\n", uid);
  printf(".nr " SKEW_FORMAT " 0\\n[" SPECIAL_SKEW_REG "]\n", uid);
  // User will have to change MARK_REG if appropriate.
  return r;
}

void special_box::compute_subscript_kern()
{
  // Already computed in compute_metrics(), so do nothing.
}

void special_box::compute_skew()
{
  // Already computed in compute_metrics(), so do nothing.
}

void special_box::output()
{
  printf("\\*[" STRING_FORMAT "]", uid);
}

void special_box::debug_print()
{
  fprintf(stderr, "special %s { ", macro_name);
  p->debug_print();
  fprintf(stderr, " }");
}
