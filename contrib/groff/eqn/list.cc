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

list_box *box::to_list_box()
{
  return 0;
}

list_box *list_box::to_list_box()
{
  return this;
}

void list_box::append(box *pp)
{
  list_box *q = pp->to_list_box();
  if (q == 0)
    list.append(pp);
  else {
    for (int i = 0; i < q->list.len; i++) {
      list.append(q->list.p[i]);
      q->list.p[i] = 0;
    }
    q->list.len = 0;
    delete q;
  }
}

list_box::list_box(box *pp) : list(pp), sty(-1)
{
  list_box *q = pp->to_list_box();
  if (q != 0) {
    // flatten it
    list.p[0] = q->list.p[0];
    for (int i = 1; i < q->list.len; i++) {
      list.append(q->list.p[i]);
      q->list.p[i] = 0;
    }
    q->list.len = 0;
    delete q;
  }
}

static int compute_spacing(int is_script, int left, int right)
{
  if (left == SUPPRESS_TYPE || right == SUPPRESS_TYPE)
    return 0;
  if (left == PUNCTUATION_TYPE)
    return is_script ? 0 : thin_space;
  if (left == OPENING_TYPE || right == CLOSING_TYPE)
    return 0;
  if (right == BINARY_TYPE || left == BINARY_TYPE)
    return is_script ? 0 : medium_space;
  if (right == RELATION_TYPE) {
    if (left == RELATION_TYPE)
      return 0;
    else
      return is_script ? 0 : thick_space;
  }
  if (left == RELATION_TYPE)
    return is_script ? 0 : thick_space;
  if (right == OPERATOR_TYPE)
    return thin_space;
  if (left == INNER_TYPE || right == INNER_TYPE)
    return is_script ? 0 : thin_space;
  if (left == OPERATOR_TYPE && right == ORDINARY_TYPE)
    return thin_space;
  return 0;
}

int list_box::compute_metrics(int style)
{
  sty = style;
  int i;
  for (i = 0; i < list.len; i++) {
    int t = list.p[i]->spacing_type; 
    // 5
    if (t == BINARY_TYPE) {
      int prevt;
      if (i == 0
	  || (prevt = list.p[i-1]->spacing_type) == BINARY_TYPE
	  || prevt == OPERATOR_TYPE
	  || prevt == RELATION_TYPE
	  || prevt == OPENING_TYPE
	  || prevt == PUNCTUATION_TYPE)
	list.p[i]->spacing_type = ORDINARY_TYPE;
    }
    // 7
    else if ((t == RELATION_TYPE || t == CLOSING_TYPE 
	      || t == PUNCTUATION_TYPE)
	     && i > 0 && list.p[i-1]->spacing_type == BINARY_TYPE)
      list.p[i-1]->spacing_type = ORDINARY_TYPE;
  }
  for (i = 0; i < list.len; i++) {
    unsigned flags = 0;
    if (i - 1 >= 0 && list.p[i - 1]->right_is_italic())
      flags |= HINT_PREV_IS_ITALIC;
    if (i + 1 < list.len && list.p[i + 1]->left_is_italic())
      flags |= HINT_NEXT_IS_ITALIC;
    if (flags)
      list.p[i]->hint(flags);
  }
  is_script = (style <= SCRIPT_STYLE);
  int total_spacing = 0;
  for (i = 1; i < list.len; i++)
    total_spacing += compute_spacing(is_script, list.p[i-1]->spacing_type,
				     list.p[i]->spacing_type);
  int res = 0;
  for (i = 0; i < list.len; i++)
    if (!list.p[i]->is_simple()) {
      int r = list.p[i]->compute_metrics(style);
      if (r) {
	if (res)
	  error("multiple marks and lineups");
	else {
	  compute_sublist_width(i);
	  printf(".nr " MARK_REG " +\\n[" TEMP_REG"]\n");
	  res = r;
	}
      }
    }
  printf(".nr " WIDTH_FORMAT " %dM", uid, total_spacing);
  for (i = 0; i < list.len; i++)
    if (!list.p[i]->is_simple())
      printf("+\\n[" WIDTH_FORMAT "]", list.p[i]->uid);
  printf("\n");
  printf(".nr " HEIGHT_FORMAT " 0", uid);
  for (i = 0; i < list.len; i++)
    if (!list.p[i]->is_simple())
      printf(">?\\n[" HEIGHT_FORMAT "]", list.p[i]->uid);
  printf("\n");
  printf(".nr " DEPTH_FORMAT " 0", uid);
  for (i = 0; i < list.len; i++)
    if (!list.p[i]->is_simple())
      printf(">?\\n[" DEPTH_FORMAT "]", list.p[i]->uid);
  printf("\n");
  int have_simple = 0;
  for (i = 0; i < list.len && !have_simple; i++)
    have_simple = list.p[i]->is_simple();
  if (have_simple) {
    printf(".nr " WIDTH_FORMAT " +\\w" DELIMITER_CHAR, uid);
    for (int i = 0; i < list.len; i++)
      if (list.p[i]->is_simple())
	list.p[i]->output();
    printf(DELIMITER_CHAR "\n");
    printf(".nr " HEIGHT_FORMAT " \\n[rst]>?\\n[" HEIGHT_FORMAT "]\n",
	   uid, uid);
    printf(".nr " DEPTH_FORMAT " 0-\\n[rsb]>?\\n[" DEPTH_FORMAT "]\n",
	   uid, uid);
  }
  return res;
}

void list_box::compute_sublist_width(int n)
{
  int total_spacing = 0;
  int i;
  for (i = 1; i < n + 1 && i < list.len; i++)
    total_spacing += compute_spacing(is_script, list.p[i-1]->spacing_type,
				     list.p[i]->spacing_type);
  printf(".nr " TEMP_REG " %dM", total_spacing);
  for (i = 0; i < n; i++)
    if (!list.p[i]->is_simple())
      printf("+\\n[" WIDTH_FORMAT "]", list.p[i]->uid);
  int have_simple = 0;
  for (i = 0; i < n && !have_simple; i++)
    have_simple = list.p[i]->is_simple();
  if (have_simple) {
    printf("+\\w" DELIMITER_CHAR);
    for (int i = 0; i < n; i++)
      if (list.p[i]->is_simple())
	list.p[i]->output();
    printf(DELIMITER_CHAR);
  }
  printf("\n");
}

void list_box::compute_subscript_kern()
{
  // We can only call compute_subscript_kern if we have called
  // compute_metrics first.
  if (list.p[list.len-1]->is_simple())
    list.p[list.len-1]->compute_metrics(sty);
  list.p[list.len-1]->compute_subscript_kern();
  printf(".nr " SUB_KERN_FORMAT " \\n[" SUB_KERN_FORMAT "]\n",
	 uid, list.p[list.len-1]->uid);
}

void list_box::output()
{
  for (int i = 0; i < list.len; i++) {
    if (i > 0) {
      int n = compute_spacing(is_script,
			      list.p[i-1]->spacing_type,
			      list.p[i]->spacing_type);
      if (n > 0)
	printf("\\h'%dM'", n);
    }
    list.p[i]->output();
  }
}

void list_box::handle_char_type(int st, int ft)
{
  for (int i = 0; i < list.len; i++)
    list.p[i]->handle_char_type(st, ft);
}

void list_box::debug_print()
{
  list.list_debug_print(" ");
}

void list_box::check_tabs(int level)
{
  list.list_check_tabs(level);
}
