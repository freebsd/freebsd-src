// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002
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

#include "pic.h"
#include "common.h"


const double RELATIVE_THICKNESS = -1.0;
const double BAD_THICKNESS = -2.0;

class simple_output : public common_output {
  virtual void simple_line(const position &, const position &) = 0;
  virtual void simple_spline(const position &, const position *, int n) = 0;
  virtual void simple_arc(const position &, const position &,
			  const position &) = 0;
  virtual void simple_circle(int, const position &, double rad) = 0;
  virtual void simple_ellipse(int, const position &, const distance &) = 0;
  virtual void simple_polygon(int, const position *, int) = 0;
  virtual void line_thickness(double) = 0;
  virtual void set_fill(double) = 0;
  virtual void set_color(char *, char *) = 0;
  virtual void reset_color() = 0;
  virtual char *get_last_filled() = 0;
  void dot(const position &, const line_type &) = 0;
public:
  void start_picture(double sc, const position &ll, const position &ur) = 0;
  void finish_picture() = 0;
  void text(const position &, text_piece *, int, double) = 0;
  void line(const position &, const position *, int n,
	    const line_type &);
  void polygon(const position *, int n,
	       const line_type &, double);
  void spline(const position &, const position *, int n,
	      const line_type &);
  void arc(const position &, const position &, const position &,
	   const line_type &);
  void circle(const position &, double rad, const line_type &, double);
  void ellipse(const position &, const distance &, const line_type &, double);
  int supports_filled_polygons();
};

int simple_output::supports_filled_polygons()
{
  return driver_extension_flag != 0;
}

void simple_output::arc(const position &start, const position &cent,
			const position &end, const line_type &lt)
{
  switch (lt.type) {
  case line_type::solid:
    line_thickness(lt.thickness);
    simple_arc(start, cent, end);
    break;
  case line_type::invisible:
    break;
  case line_type::dashed:
    dashed_arc(start, cent, end, lt);
    break;
  case line_type::dotted:
    dotted_arc(start, cent, end, lt);
    break;
  }
}

void simple_output::line(const position &start, const position *v, int n,
			 const line_type &lt)
{
  position pos = start;
  line_thickness(lt.thickness);
  for (int i = 0; i < n; i++) {
    switch (lt.type) {
    case line_type::solid:
      simple_line(pos, v[i]);
      break;
    case line_type::dotted:
      {
	distance vec(v[i] - pos);
	double dist = hypot(vec);
	int ndots = int(dist/lt.dash_width + .5);
	if (ndots == 0)
	  dot(pos, lt);
	else {
	  vec /= double(ndots);
	  for (int j = 0; j <= ndots; j++)
	    dot(pos + vec*j, lt);
	}
      }
      break;
    case line_type::dashed:
      {
	distance vec(v[i] - pos);
	double dist = hypot(vec);
	if (dist <= lt.dash_width*2.0)
	  simple_line(pos, v[i]);
	else {
	  int ndashes = int((dist - lt.dash_width)/(lt.dash_width*2.0) + .5);
	  distance dash_vec = vec*(lt.dash_width/dist);
	  double dash_gap = (dist - lt.dash_width)/ndashes;
	  distance dash_gap_vec = vec*(dash_gap/dist);
	  for (int j = 0; j <= ndashes; j++) {
	    position s(pos + dash_gap_vec*j);
	    simple_line(s, s + dash_vec);
	  }
	}
      }
      break;
    case line_type::invisible:
      break;
    default:
      assert(0);
    }
    pos = v[i];
  }
}

void simple_output::spline(const position &start, const position *v, int n,
			   const line_type &lt)
{
  line_thickness(lt.thickness);
  simple_spline(start, v, n);
}

void simple_output::polygon(const position *v, int n,
			    const line_type &lt, double fill)
{
  if (driver_extension_flag && ((fill >= 0.0) || (get_last_filled() != 0))) {
    if (get_last_filled() == 0)
      set_fill(fill);
    simple_polygon(1, v, n);
  }
  if (lt.type == line_type::solid && driver_extension_flag) {
    line_thickness(lt.thickness);
    simple_polygon(0, v, n);
  }
  else if (lt.type != line_type::invisible) {
    line_thickness(lt.thickness);
    line(v[n - 1], v, n, lt);
  }
}

void simple_output::circle(const position &cent, double rad,
			   const line_type &lt, double fill)
{
  if (driver_extension_flag && ((fill >= 0.0) || (get_last_filled() != 0))) {
    if (get_last_filled() == 0)
      set_fill(fill);
    simple_circle(1, cent, rad);
  }
  line_thickness(lt.thickness);
  switch (lt.type) {
  case line_type::invisible:
    break;
  case line_type::dashed:
    dashed_circle(cent, rad, lt);
    break;
  case line_type::dotted:
    dotted_circle(cent, rad, lt);
    break;
  case line_type::solid:
    simple_circle(0, cent, rad);
    break;
  default:
    assert(0);
  }
}

void simple_output::ellipse(const position &cent, const distance &dim,
			    const line_type &lt, double fill)
{
  if (driver_extension_flag && ((fill >= 0.0) || (get_last_filled() != 0))) {
    if (get_last_filled() == 0)
      set_fill(fill);
    simple_ellipse(1, cent, dim);
  }
  if (lt.type != line_type::invisible)
    line_thickness(lt.thickness);
  switch (lt.type) {
  case line_type::invisible:
    break;
  case line_type::dotted:
  case line_type::dashed:
  case line_type::solid:
    simple_ellipse(0, cent, dim);
    break;
  default:
    assert(0);
  }
}

#define FILL_MAX 1000

class troff_output : public simple_output {
  const char *last_filename;
  position upper_left;
  double height;
  double scale;
  double last_line_thickness;
  double last_fill;
  char *last_filled;		// color
  char *last_outlined;		// color
public:
  troff_output();
  ~troff_output();
  void start_picture(double, const position &ll, const position &ur);
  void finish_picture();
  void text(const position &, text_piece *, int, double);
  void dot(const position &, const line_type &);
  void command(const char *, const char *, int);
  void set_location(const char *, int);
  void simple_line(const position &, const position &);
  void simple_spline(const position &, const position *, int n);
  void simple_arc(const position &, const position &, const position &);
  void simple_circle(int, const position &, double rad);
  void simple_ellipse(int, const position &, const distance &);
  void simple_polygon(int, const position *, int);
  void line_thickness(double p);
  void set_fill(double);
  void set_color(char *, char *);
  void reset_color();
  char *get_last_filled();
  char *get_outline_color();
  position transform(const position &);
};

output *make_troff_output()
{
  return new troff_output;
}

troff_output::troff_output()
: last_filename(0), last_line_thickness(BAD_THICKNESS),
  last_fill(-1.0), last_filled(0), last_outlined(0)
{
}

troff_output::~troff_output()
{
}

inline position troff_output::transform(const position &pos)
{
  return position((pos.x - upper_left.x)/scale,
		  (upper_left.y - pos.y)/scale);
}

#define FILL_REG "00"

// If this register > 0, then pic will generate \X'ps: ...' commands
// if the aligned attribute is used.
#define GROPS_REG "0p"

// If this register is defined, geqn won't produce `\x's.
#define EQN_NO_EXTRA_SPACE_REG "0x"

void troff_output::start_picture(double sc,
				 const position &ll, const position &ur)
{
  upper_left.x = ll.x;
  upper_left.y = ur.y;
  scale = compute_scale(sc, ll, ur);
  height = (ur.y - ll.y)/scale;
  double width = (ur.x - ll.x)/scale;
  printf(".PS %.3fi %.3fi", height, width);
  if (args)
    printf(" %s\n", args);
  else
    putchar('\n');
  printf(".\\\" %g %g %g %g\n", ll.x, ll.y, ur.x, ur.y);
  printf(".\\\" %.3fi %.3fi %.3fi %.3fi\n", 0.0, height, width, 0.0);
  printf(".nr " FILL_REG " \\n(.u\n.nf\n");
  printf(".nr " EQN_NO_EXTRA_SPACE_REG " 1\n");
  // This guarantees that if the picture is used in a diversion it will
  // have the right width.
  printf("\\h'%.3fi'\n.sp -1\n", width);
}

void troff_output::finish_picture()
{
  line_thickness(BAD_THICKNESS);
  last_fill = -1.0;		// force it to be reset for each picture
  reset_color();
  if (!flyback_flag)
    printf(".sp %.3fi+1\n", height);
  printf(".if \\n(" FILL_REG " .fi\n");
  printf(".br\n");
  printf(".nr " EQN_NO_EXTRA_SPACE_REG " 0\n");
  // this is a little gross
  set_location(current_filename, current_lineno);
  fputs(flyback_flag ? ".PF\n" : ".PE\n", stdout);
}

void troff_output::command(const char *s,
			   const char *filename, int lineno)
{
  if (filename != 0)
    set_location(filename, lineno);
  fputs(s, stdout);
  putchar('\n');
}

void troff_output::simple_circle(int filled, const position &cent, double rad)
{
  position c = transform(cent);
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'"
	 "\\D'%c%.3fi'"
	 "\n.sp -1\n",
	 c.x - rad/scale,
	 c.y,
	 (filled ? 'C' : 'c'),
	 rad*2.0/scale);
}

void troff_output::simple_ellipse(int filled, const position &cent,
				  const distance &dim)
{
  position c = transform(cent);
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'"
	 "\\D'%c%.3fi %.3fi'"
	 "\n.sp -1\n",
	 c.x - dim.x/(2.0*scale),
	 c.y,
	 (filled ? 'E' : 'e'),
	 dim.x/scale, dim.y/scale);
}

void troff_output::simple_arc(const position &start, const distance &cent,
			      const distance &end)
{
  position s = transform(start);
  position c = transform(cent);
  distance cv = c - s;
  distance ev = transform(end) - c;
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'"
	 "\\D'a%.3fi %.3fi %.3fi %.3fi'"
	 "\n.sp -1\n",
	 s.x, s.y, cv.x, cv.y, ev.x, ev.y);
}

void troff_output::simple_line(const position &start, const position &end)
{
  position s = transform(start);
  distance ev = transform(end) - s;
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'"
	 "\\D'l%.3fi %.3fi'"
	 "\n.sp -1\n",
	 s.x, s.y, ev.x, ev.y);
}

void troff_output::simple_spline(const position &start,
				 const position *v, int n)
{
  position pos = transform(start);
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'",
	 pos.x, pos.y);
  fputs("\\D'~", stdout);
  for (int i = 0; i < n; i++) {
    position temp = transform(v[i]);
    distance d = temp - pos;
    pos = temp;
    if (i != 0)
      putchar(' ');
    printf("%.3fi %.3fi", d.x, d.y);
  }
  printf("'\n.sp -1\n");
}

// a solid polygon

void troff_output::simple_polygon(int filled, const position *v, int n)
{
  position pos = transform(v[0]);
  printf("\\h'%.3fi'"
	 "\\v'%.3fi'",
	 pos.x, pos.y);
  printf("\\D'%c", (filled ? 'P' : 'p'));
  for (int i = 1; i < n; i++) {
    position temp = transform(v[i]);
    distance d = temp - pos;
    pos = temp;
    if (i != 1)
      putchar(' ');
    printf("%.3fi %.3fi", d.x, d.y);
  }
  printf("'\n.sp -1\n");
}

const double TEXT_AXIS = 0.22;	// in ems

static const char *choose_delimiter(const char *text)
{
  if (strchr(text, '\'') == 0)
    return "'";
  else
    return "\\(ts";
}

void troff_output::text(const position &center, text_piece *v, int n,
			double ang)
{
  line_thickness(BAD_THICKNESS); // the text might use lines (eg in equations)
  int rotate_flag = 0;
  if (driver_extension_flag && ang != 0.0) {
    rotate_flag = 1;
    position c = transform(center);
    printf(".if \\n(" GROPS_REG " \\{\\\n"
	   "\\h'%.3fi'"
	   "\\v'%.3fi'"
	   "\\X'ps: exec gsave currentpoint 2 copy translate %.4f rotate neg exch neg exch translate'"
	   "\n.sp -1\n"
	   ".\\}\n",
	   c.x, c.y, -ang*180.0/M_PI);
  }
  for (int i = 0; i < n; i++)
    if (v[i].text != 0 && *v[i].text != '\0') {
      position c = transform(center);
      if (v[i].filename != 0)
	set_location(v[i].filename, v[i].lineno);
      printf("\\h'%.3fi", c.x);
      const char *delim = choose_delimiter(v[i].text);
      if (v[i].adj.h == RIGHT_ADJUST)
	printf("-\\w%s%s%su", delim, v[i].text, delim);
      else if (v[i].adj.h != LEFT_ADJUST)
	printf("-(\\w%s%s%su/2u)", delim, v[i].text, delim);
      putchar('\'');
      printf("\\v'%.3fi-(%dv/2u)+%dv+%.2fm",
	     c.y,
	     n - 1,
	     i,
	     TEXT_AXIS);
      if (v[i].adj.v == ABOVE_ADJUST)
	printf("-.5v");
      else if (v[i].adj.v == BELOW_ADJUST)
	printf("+.5v");
      putchar('\'');
      fputs(v[i].text, stdout);
      fputs("\n.sp -1\n", stdout);
    }
  if (rotate_flag)
    printf(".if '\\*(.T'ps' \\{\\\n"
	   "\\X'ps: exec grestore'\n.sp -1\n"
	   ".\\}\n");
}

void troff_output::line_thickness(double p)
{
  if (p < 0.0)
    p = RELATIVE_THICKNESS;
  if (driver_extension_flag && p != last_line_thickness) {
    printf("\\D't %.3fp'\\h'%.3fp'\n.sp -1\n", p, -p);
    last_line_thickness = p;
  }
}

void troff_output::set_fill(double f)
{
  if (driver_extension_flag && f != last_fill) {
    printf("\\D'f %du'\\h'%du'\n.sp -1\n", int(f*FILL_MAX), -int(f*FILL_MAX));
    last_fill = f;
  }
  if (last_filled) {
    free(last_filled);
    last_filled = 0;
    printf("\\M[]\n.sp -1\n");
  }
}

void troff_output::set_color(char *color_fill, char *color_outlined)
{
  if (driver_extension_flag) {
    if (last_filled || last_outlined) {
      reset_color();
    }
    if (color_fill) {
      printf("\\M[%s]\n.sp -1\n", color_fill);
      last_filled = strdup(color_fill);
    }
    if (color_outlined) {
      printf("\\m[%s]\n.sp -1\n", color_outlined);
      last_outlined = strdup(color_outlined);
    }
  }
}

void troff_output::reset_color()
{
  if (driver_extension_flag) {
    if (last_filled) {
      printf("\\M[]\n.sp -1\n");
      free(last_filled);
      last_filled = 0;
    }
    if (last_outlined) {
      printf("\\m[]\n.sp -1\n");
      free(last_outlined);
      last_outlined = 0;
    }
  }
}

char *troff_output::get_last_filled()
{
  return last_filled;
}

char *troff_output::get_outline_color()
{
  return last_outlined;
}

const double DOT_AXIS = .044;

void troff_output::dot(const position &cent, const line_type &lt)
{
  if (driver_extension_flag) {
    line_thickness(lt.thickness);
    simple_line(cent, cent);
  }
  else {
    position c = transform(cent);
    printf("\\h'%.3fi-(\\w'.'u/2u)'"
	   "\\v'%.3fi+%.2fm'"
	   ".\n.sp -1\n",
	   c.x,
	   c.y, 
	   DOT_AXIS);
  }
}

void troff_output::set_location(const char *s, int n)
{
  if (last_filename != 0 && strcmp(s, last_filename) == 0)
    printf(".lf %d\n", n);
  else {
    printf(".lf %d %s\n", n, s);
    last_filename = s;
  }
}
