// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2001 Free Software Foundation, Inc.
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

#include "driver.h"

printer *pr = 0;

font_pointer_list::font_pointer_list(font *f, font_pointer_list *fp)
: p(f), next(fp)
{
}

printer::printer()
: font_list(0), font_table(0), nfonts(0)
{
}

printer::~printer()
{
  a_delete font_table;
  while (font_list) {
    font_pointer_list *tem = font_list;
    font_list = font_list->next;
    delete tem->p;
    delete tem;
  }
  if (ferror(stdout) || fflush(stdout) < 0)
    fatal("output error");
}

void printer::load_font(int n, const char *nm)
{
  assert(n >= 0);
  if (n >= nfonts) {
    if (nfonts == 0) {
      nfonts = 10;
      if (nfonts <= n)
	nfonts = n + 1;
      font_table = new font *[nfonts];
      for (int i = 0; i < nfonts; i++)
	font_table[i] = 0;
    }
    else {
      font **old_font_table = font_table;
      int old_nfonts = nfonts;
      nfonts *= 2;
      if (n >= nfonts)
	nfonts = n + 1;
      font_table = new font *[nfonts];
      int i;
      for (i = 0; i < old_nfonts; i++)
	font_table[i] = old_font_table[i];
      for (i = old_nfonts; i < nfonts; i++)
	font_table[i] = 0;
      a_delete old_font_table;
    }
  }
  font *f = find_font(nm);
  font_table[n] = f;
}

font *printer::find_font(const char *nm)
{
  for (font_pointer_list *p = font_list; p; p = p->next)
    if (strcmp(p->p->get_name(), nm) == 0)
      return p->p;
  font *f = make_font(nm);
  if (!f)
    fatal("sorry, I can't continue");
  font_list = new font_pointer_list(f, font_list);
  return f;
}

font *printer::make_font(const char *nm)
{
  return font::load_font(nm);
}

void printer::end_of_line()
{
}

void printer::special(char *, const environment *, char)
{
}

void printer::draw(int, int *, int, const environment *)
{
}

void printer::set_ascii_char(unsigned char c, const environment *env, 
			     int *widthp)
{
  char  buf[2];
  int   w;
  font *f;

  buf[0] = c;
  buf[1] = '\0';

  int i = set_char_and_width(buf, env, &w, &f);
  set_char(i, f, env, w, 0);
  if (widthp) {
    *widthp = w;
  }
}

void printer::set_special_char(const char *nm, const environment *env,
			       int *widthp)
{
  font *f;
  int   w;
  int i = set_char_and_width(nm, env, &w, &f);
  if (i != -1) {
    set_char(i, f, env, w, nm);
    if (widthp) {
      *widthp = w;
    }
  }
}

int printer::set_char_and_width(const char *nm, const environment *env,
				int *widthp, font **f)
{
  int i = font::name_to_index(nm);
  int fn = env->fontno;
  if (fn < 0 || fn >= nfonts) {
    error("bad font position `%1'", fn);
    return(-1);
  }
  *f = font_table[fn];
  if (*f == 0) {
    error("no font mounted at `%1'", fn);
    return(-1);
  }
  if (!(*f)->contains(i)) {
    if (nm[0] != '\0' && nm[1] == '\0')
      error("font `%1' does not contain ascii character `%2'",
	    (*f)->get_name(),
	    nm[0]);
    else
      error("font `%1' does not contain special character `%2'",
	    (*f)->get_name(),
	    nm);
    return(-1);
  }
  int w = (*f)->get_width(i, env->size);
  if (widthp)
    *widthp = w;
  return( i );
}

void printer::set_numbered_char(int num, const environment *env, int *widthp)
{
  int i = font::number_to_index(num);
  int fn = env->fontno;
  if (fn < 0 || fn >= nfonts) {
    error("bad font position `%1'", fn);
    return;
  }
  font *f = font_table[fn];
  if (f == 0) {
    error("no font mounted at `%1'", fn);
    return;
  }
  if (!f->contains(i)) {
    error("font `%1' does not contain numbered character %2",
	  f->get_name(),
	  num);
    return;
  }
  int w = f->get_width(i, env->size);
  if (widthp)
    *widthp = w;
  set_char(i, f, env, w, 0);
}

font *printer::get_font_from_index(int fontno)
{
  if ((fontno >= 0) && (fontno < nfonts))
    return(font_table[fontno]);
  else
    return(0);
}

// This utility function adjusts the specified center of the
// arc so that it is equidistant between the specified start
// and end points. (p[0], p[1]) is a vector from the current
// point to the center; (p[2], p[3]) is a vector from the 
// center to the end point.  If the center can be adjusted,
// a vector from the current point to the adjusted center is
// stored in c[0], c[1] and 1 is returned.  Otherwise 0 is
// returned.

#if 1
int printer::adjust_arc_center(const int *p, double *c)
{
  // We move the center along a line parallel to the line between
  // the specified start point and end point so that the center
  // is equidistant between the start and end point.
  // It can be proved (using Lagrange multipliers) that this will
  // give the point nearest to the specified center that is equidistant
  // between the start and end point.

  double x = p[0] + p[2];	// (x, y) is the end point
  double y = p[1] + p[3];
  double n = x*x + y*y;
  if (n != 0) {
    c[0]= double(p[0]);
    c[1] = double(p[1]);
    double k = .5 - (c[0]*x + c[1]*y)/n;
    c[0] += k*x;
    c[1] += k*y;
    return 1;
  }
  else
    return 0;
}
#else
int printer::adjust_arc_center(const int *p, double *c)
{
  int x = p[0] + p[2];	// (x, y) is the end point
  int y = p[1] + p[3];
  // Start at the current point; go in the direction of the specified
  // center point until we reach a point that is equidistant between
  // the specified starting point and the specified end point. Place
  // the center of the arc there.
  double n = p[0]*double(x) + p[1]*double(y);
  if (n > 0) {
    double k = (double(x)*x + double(y)*y)/(2.0*n);
    // (cx, cy) is our chosen center
    c[0] = k*p[0];
    c[1] = k*p[1];
    return 1;
  }
  else {
    // We would never reach such a point.  So instead start at the
    // specified end point of the arc.  Go towards the specified
    // center point until we reach a point that is equidistant between
    // the specified start point and specified end point. Place
    // the center of the arc there.
    n = p[2]*double(x) + p[3]*double(y);
    if (n > 0) {
      double k = 1 - (double(x)*x + double(y)*y)/(2.0*n);
      // (c[0], c[1]) is our chosen center
      c[0] = p[0] + k*p[2];
      c[1] = p[1] + k*p[3];
      return 1;
    }
    else
      return 0;
  }
}  
#endif
