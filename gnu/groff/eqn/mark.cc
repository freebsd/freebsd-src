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

class mark_box : public pointer_box {
public:
  mark_box(box *);
  int compute_metrics(int);
  void output();
  void debug_print();
};

// we push down marks so that they don't interfere with spacing

box *make_mark_box(box *p)
{
  list_box *b = p->to_list_box();
  if (b != 0) {
    b->list.p[0] = make_mark_box(b->list.p[0]);
    return b;
  }
  else
    return new mark_box(p);
}

mark_box::mark_box(box *pp) : pointer_box(pp)
{
}

void mark_box::output()
{
  p->output();
}

int mark_box::compute_metrics(int style)
{
  int res = p->compute_metrics(style);
  if (res)
    error("multiple marks and lineups");
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " MARK_REG " 0\n");
  return FOUND_MARK;
}

void mark_box::debug_print()
{
  fprintf(stderr, "mark { ");
  p->debug_print();
  fprintf(stderr, " }");
}


class lineup_box : public pointer_box {
public:
  lineup_box(box *);
  void output();
  int compute_metrics(int style);
  void debug_print();
};

// we push down lineups so that they don't interfere with spacing

box *make_lineup_box(box *p)
{
  list_box *b = p->to_list_box();
  if (b != 0) {
    b->list.p[0] = make_lineup_box(b->list.p[0]);
    return b;
  }
  else
    return new lineup_box(p);
}

lineup_box::lineup_box(box *pp) : pointer_box(pp)
{
}

void lineup_box::output()
{
  p->output();
}

int lineup_box::compute_metrics(int style)
{
  int res = p->compute_metrics(style);
  if (res)
    error("multiple marks and lineups");
  printf(".nr " WIDTH_FORMAT " 0\\n[" WIDTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " HEIGHT_FORMAT " \\n[" HEIGHT_FORMAT "]\n", uid, p->uid);
  printf(".nr " DEPTH_FORMAT " \\n[" DEPTH_FORMAT "]\n", uid, p->uid);
  printf(".nr " MARK_REG " 0\n");
  return FOUND_LINEUP;
}

void lineup_box::debug_print()
{
  fprintf(stderr, "lineup { ");
  p->debug_print();
  fprintf(stderr, " }");
}
