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
// piles and matrices

#include "eqn.h"
#include "pbox.h"

// SUP_RAISE_FORMAT gives the first baseline
// BASELINE_SEP_FORMAT gives the separation between baselines

int pile_box::compute_metrics(int style)
{
  int i;
  for (i = 0; i < col.len; i++)
    col.p[i]->compute_metrics(style);
  printf(".nr " WIDTH_FORMAT " 0", uid);
  for (i = 0; i < col.len; i++)
    printf(">?\\n[" WIDTH_FORMAT "]", col.p[i]->uid);
  printf("\n");
  printf(".nr " BASELINE_SEP_FORMAT " %dM",
	 uid, baseline_sep+col.space);
  for (i = 1; i < col.len; i++)
    printf(">?(\\n[" DEPTH_FORMAT "]+\\n[" HEIGHT_FORMAT "]+%dM)",
	   col.p[i-1]->uid, col.p[i]->uid, default_rule_thickness*5);
  // round it so that it's a multiple of the vertical resolution
  printf("/\\n(.V+(\\n(.V/2)*\\n(.V\n");

  printf(".nr " SUP_RAISE_FORMAT " \\n[" BASELINE_SEP_FORMAT "]*%d/2"
	 "+%dM\n",
	 uid, uid, col.len-1, axis_height - shift_down);
  printf(".nr " HEIGHT_FORMAT " \\n[" SUP_RAISE_FORMAT "]+\\n["
	 HEIGHT_FORMAT "]\n",
	 uid, uid, col.p[0]->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" BASELINE_SEP_FORMAT "]*%d+\\n["
	 DEPTH_FORMAT "]-\\n[" SUP_RAISE_FORMAT "]\n",
	 uid, uid, col.len-1, col.p[col.len-1]->uid, uid);
  return FOUND_NOTHING;
}

void pile_box::output()
{
  int i;
  printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid);
  for (i = 0; i < col.len; i++) {
    switch (col.align) {
    case LEFT_ALIGN:
      break;
    case CENTER_ALIGN:
      printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u'",
	     uid, col.p[i]->uid);
      break;
    case RIGHT_ALIGN:
      printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u'",
	     uid, col.p[i]->uid);
      break;
    default:
      assert(0);
    }
    col.p[i]->output();
    printf("\\h'-\\n[" WIDTH_FORMAT "]u'", col.p[i]->uid);
    switch (col.align) {
    case LEFT_ALIGN:
      break;
    case CENTER_ALIGN:
      printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u'",
	     col.p[i]->uid, uid);
      break;
    case RIGHT_ALIGN:
      printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u'",
	     col.p[i]->uid, uid);
      break;
    default:
      assert(0);
    }
    if (i != col.len - 1)
      printf("\\v'\\n[" BASELINE_SEP_FORMAT "]u'", uid);
  }
  printf("\\v'\\n[" SUP_RAISE_FORMAT "]u'", uid);
  printf("\\v'-(%du*\\n[" BASELINE_SEP_FORMAT "]u)'", col.len - 1, uid);
  printf("\\h'\\n[" WIDTH_FORMAT "]u'", uid);
}

pile_box::pile_box(box *pp) : col(pp)
{
}

void pile_box::check_tabs(int level)
{
  col.list_check_tabs(level);
}

void pile_box::debug_print()
{
  col.debug_print("pile");
}

int matrix_box::compute_metrics(int style)
{
  int i, j;
  int maxlen = 0;
  int space = 0;
  for (i = 0; i < len; i++) {
    for (j = 0; j < p[i]->len; j++)
      p[i]->p[j]->compute_metrics(style);
    if (p[i]->len > maxlen)
      maxlen = p[i]->len;
    if (p[i]->space > space)
      space = p[i]->space;
  }
  for (i = 0; i < len; i++) {
    printf(".nr " COLUMN_WIDTH_FORMAT " 0", uid, i);
    for (j = 0; j < p[i]->len; j++)
      printf(">?\\n[" WIDTH_FORMAT "]", p[i]->p[j]->uid);
    printf("\n");
  }
  printf(".nr " WIDTH_FORMAT " %dM",
	 uid, column_sep*(len-1)+2*matrix_side_sep);
  for (i = 0; i < len; i++)
    printf("+\\n[" COLUMN_WIDTH_FORMAT "]", uid, i);
  printf("\n");
  printf(".nr " BASELINE_SEP_FORMAT " %dM",
	 uid, baseline_sep+space);
  for (i = 0; i < len; i++)
    for (j = 1; j < p[i]->len; j++)
      printf(">?(\\n[" DEPTH_FORMAT "]+\\n[" HEIGHT_FORMAT "]+%dM)",
	   p[i]->p[j-1]->uid, p[i]->p[j]->uid, default_rule_thickness*5);
  // round it so that it's a multiple of the vertical resolution
  printf("/\\n(.V+(\\n(.V/2)*\\n(.V\n");
  printf(".nr " SUP_RAISE_FORMAT " \\n[" BASELINE_SEP_FORMAT "]*%d/2"
	 "+%dM\n",
	 uid, uid, maxlen-1, axis_height - shift_down);
  printf(".nr " HEIGHT_FORMAT " 0\\n[" SUP_RAISE_FORMAT "]+(0",
	 uid, uid);
  for (i = 0; i < len; i++)
    printf(">?\\n[" HEIGHT_FORMAT "]", p[i]->p[0]->uid);
  printf(")>?0\n");
  printf(".nr " DEPTH_FORMAT " \\n[" BASELINE_SEP_FORMAT "]*%d-\\n["
	 SUP_RAISE_FORMAT "]+(0",
	 uid, uid, maxlen-1, uid);
  for (i = 0; i < len; i++)
    if (p[i]->len == maxlen)
      printf(">?\\n[" DEPTH_FORMAT "]", p[i]->p[maxlen-1]->uid);
  printf(")>?0\n");
  return FOUND_NOTHING;
}

void matrix_box::output()
{
  printf("\\h'%dM'", matrix_side_sep);
  for (int i = 0; i < len; i++) {
    int j;
    printf("\\v'-\\n[" SUP_RAISE_FORMAT "]u'", uid);
    for (j = 0; j < p[i]->len; j++) {
      switch (p[i]->align) {
      case LEFT_ALIGN:
	break;
      case CENTER_ALIGN:
	printf("\\h'\\n[" COLUMN_WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u/2u'",
	       uid, i, p[i]->p[j]->uid);
	break;
      case RIGHT_ALIGN:
	printf("\\h'\\n[" COLUMN_WIDTH_FORMAT "]u-\\n[" WIDTH_FORMAT "]u'",
	       uid, i, p[i]->p[j]->uid);
	break;
      default:
	assert(0);
      }
      p[i]->p[j]->output();
      printf("\\h'-\\n[" WIDTH_FORMAT "]u'", p[i]->p[j]->uid);
      switch (p[i]->align) {
      case LEFT_ALIGN:
	break;
      case CENTER_ALIGN:
	printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" COLUMN_WIDTH_FORMAT "]u/2u'",
	       p[i]->p[j]->uid, uid, i);
	break;
      case RIGHT_ALIGN:
	printf("\\h'\\n[" WIDTH_FORMAT "]u-\\n[" COLUMN_WIDTH_FORMAT "]u'",
	       p[i]->p[j]->uid, uid, i);
	break;
      default:
	assert(0);
      }
      if (j != p[i]->len - 1)
	printf("\\v'\\n[" BASELINE_SEP_FORMAT "]u'", uid);
    }
    printf("\\v'\\n[" SUP_RAISE_FORMAT "]u'", uid);
    printf("\\v'-(%du*\\n[" BASELINE_SEP_FORMAT "]u)'", p[i]->len - 1, uid);
    printf("\\h'\\n[" COLUMN_WIDTH_FORMAT "]u'", uid, i);
    if (i != len - 1)
      printf("\\h'%dM'", column_sep);
  }
  printf("\\h'%dM'", matrix_side_sep);
}

matrix_box::matrix_box(column *pp)
{
  p = new column*[10];
  for (int i = 0; i < 10; i++)
    p[i] = 0;
  maxlen = 10;
  len = 1;
  p[0] = pp;
}

matrix_box::~matrix_box()
{
  for (int i = 0; i < len; i++)
    delete p[i];
  a_delete p;
}

void matrix_box::append(column *pp)
{
  if (len + 1 > maxlen) {
    column **oldp = p;
    maxlen *= 2;
    p = new column*[maxlen];
    memcpy(p, oldp, sizeof(column*)*len);
    a_delete oldp;
  }
  p[len++] = pp;
}

void matrix_box::check_tabs(int level)
{
  for (int i = 0; i < len; i++)
    p[i]->list_check_tabs(level);
}

void matrix_box::debug_print()
{
  fprintf(stderr, "matrix { ");
  p[0]->debug_print("col");
  for (int i = 1; i < len; i++) {
    fprintf(stderr, " ");
    p[i]->debug_print("col");
  }
  fprintf(stderr, " }");
}

column::column(box *pp) : box_list(pp), align(CENTER_ALIGN), space(0)
{
}

void column::set_alignment(alignment a)
{
  align = a;
}

void column::set_space(int n)
{
  space = n;
}

void column::debug_print(const char *s)
{
  char c = '\0';		// shut up -Wall
  switch (align) {
  case LEFT_ALIGN:
    c = 'l';
    break;
  case RIGHT_ALIGN:
    c = 'r';
    break;
  case CENTER_ALIGN:
    c = 'c';
    break;
  default:
    assert(0);
  }
  fprintf(stderr, "%c%s %d { ", c, s, space);
  list_debug_print(" above ");
  fprintf(stderr, " }");
}

