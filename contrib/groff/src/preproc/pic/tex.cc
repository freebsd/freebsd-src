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

#include "pic.h"

#ifdef TEX_SUPPORT

#include "common.h"

class tex_output : public common_output {
public:
  tex_output();
  ~tex_output();
  void start_picture(double, const position &ll, const position &ur);
  void finish_picture();
  void text(const position &, text_piece *, int, double);
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
  void command(const char *, const char *, int);
  int supports_filled_polygons();
private:
  position upper_left;
  double height;
  double width;
  double scale;
  double pen_size;

  void point(const position &);
  void dot(const position &, const line_type &);
  void solid_arc(const position &cent, double rad, double start_angle,
		 double end_angle, const line_type &lt);
  position transform(const position &);
protected:
  virtual void set_pen_size(double ps);
};

// convert inches to milliinches

inline int milliinches(double x)
{
  return int(x*1000.0 + .5);
}

inline position tex_output::transform(const position &pos)
{
  return position((pos.x - upper_left.x)/scale,
		  (upper_left.y - pos.y)/scale);
}

output *make_tex_output()
{
  return new tex_output;
}

tex_output::tex_output()
{
}

tex_output::~tex_output()
{
}

const int DEFAULT_PEN_SIZE = 8;

void tex_output::set_pen_size(double ps)
{
  if (ps < 0.0)
    ps = -1.0;
  if (ps != pen_size) {
    pen_size = ps;
    printf("    \\special{pn %d}%%\n", 
	   ps < 0.0 ? DEFAULT_PEN_SIZE : int(ps*(1000.0/72.0) + .5));
  }
}

void tex_output::start_picture(double sc, const position &ll,
			       const position &ur)
{
  upper_left.x = ll.x;
  upper_left.y = ur.y;
  scale = compute_scale(sc, ll, ur);
  height = (ur.y - ll.y)/scale;
  width = (ur.x - ll.x)/scale;
  /* the point of \vskip 0pt is to ensure that the vtop gets
    a height of 0 rather than the height of the hbox; this
    might be non-zero if text from text attributes lies outside pic's
    idea of the bounding box of the picture. */
  fputs("\\expandafter\\ifx\\csname graph\\endcsname\\relax \\csname newbox\\endcsname\\graph\\fi\n"
	"\\expandafter\\ifx\\csname graphtemp\\endcsname\\relax \\csname newdimen\\endcsname\\graphtemp\\fi\n"
	"\\setbox\\graph=\\vtop{\\vskip 0pt\\hbox{%\n",
	stdout);
  pen_size = -2.0;
}

void tex_output::finish_picture()
{
  printf("    \\hbox{\\vrule depth%.3fin width0pt height 0pt}%%\n"
	 "    \\kern %.3fin\n"
	 "  }%%\n"
	 "}%%\n",
	 height, width);
}

void tex_output::text(const position &center, text_piece *v, int n, double)
{
  position c = transform(center);
  for (int i = 0; i < n; i++)
    if (v[i].text != 0 && *v[i].text != '\0') {
      int j = 2*i - n + 1;
      if (v[i].adj.v == ABOVE_ADJUST)
	j--;
      else if (v[i].adj.v == BELOW_ADJUST)
	j++;
      if (j == 0) {
	printf("    \\graphtemp=.5ex\\advance\\graphtemp by %.3fin\n", c.y);
      }
      else {
	printf("    \\graphtemp=\\baselineskip"
	       "\\multiply\\graphtemp by %d"
	       "\\divide\\graphtemp by 2\n"
	       "    \\advance\\graphtemp by .5ex"
	       "\\advance\\graphtemp by %.3fin\n",
	       j, c.y);
      }
      printf("    \\rlap{\\kern %.3fin\\lower\\graphtemp", c.x);
      fputs("\\hbox to 0pt{", stdout);
      if (v[i].adj.h != LEFT_ADJUST)
	fputs("\\hss ", stdout);
      fputs(v[i].text, stdout);
      if (v[i].adj.h != RIGHT_ADJUST)
	fputs("\\hss", stdout);
      fputs("}}%\n", stdout);
    }
}

void tex_output::point(const position &pos)
{
  position p = transform(pos);
  printf("    \\special{pa %d %d}%%\n", milliinches(p.x), milliinches(p.y));
}

void tex_output::line(const position &start, const position *v, int n,
		      const line_type &lt)
{
  set_pen_size(lt.thickness);
  point(start);
  for (int i = 0; i < n; i++)
    point(v[i]);
  fputs("    \\special{", stdout);
  switch(lt.type) {
  case line_type::invisible:
    fputs("ip", stdout);
    break;
  case line_type::solid:
    fputs("fp", stdout);
    break;
  case line_type::dotted:
    printf("dt %.3f", lt.dash_width/scale);
    break;
  case line_type::dashed:
    printf("da %.3f", lt.dash_width/scale);
    break;
  }
  fputs("}%\n", stdout);
}

void tex_output::polygon(const position *v, int n,
			 const line_type &lt, double fill)
{
  if (fill >= 0.0) {
    if (fill > 1.0)
      fill = 1.0;
    printf("    \\special{sh %.3f}%%\n", fill);
  }
  line(v[n-1], v, n, lt);
}

void tex_output::spline(const position &start, const position *v, int n,
			const line_type &lt)
{
  if (lt.type == line_type::invisible)
    return;
  set_pen_size(lt.thickness);
  point(start);
  for (int i = 0; i < n; i++)
    point(v[i]);
  fputs("    \\special{sp", stdout);
  switch(lt.type) {
  case line_type::solid:
    break;
  case line_type::dotted:
    printf(" %.3f", -lt.dash_width/scale);
    break;
  case line_type::dashed:
    printf(" %.3f", lt.dash_width/scale);
    break;
  case line_type::invisible:
    assert(0);
  }
  fputs("}%\n", stdout);
}

void tex_output::solid_arc(const position &cent, double rad,
			   double start_angle, double end_angle,
			   const line_type &lt)
{
  set_pen_size(lt.thickness);
  position c = transform(cent);
  printf("    \\special{ar %d %d %d %d %f %f}%%\n",
	 milliinches(c.x),
	 milliinches(c.y),
	 milliinches(rad/scale),
	 milliinches(rad/scale),
	 -end_angle,
	 (-end_angle > -start_angle) ? (double)M_PI * 2 - start_angle
	 			     : -start_angle);
}
  
void tex_output::arc(const position &start, const position &cent,
		     const position &end, const line_type &lt)
{
  switch (lt.type) {
  case line_type::invisible:
    break;
  case line_type::dashed:
    dashed_arc(start, cent, end, lt);
    break;
  case line_type::dotted:
    dotted_arc(start, cent, end, lt);
    break;
  case line_type::solid:
    {
      position c;
      if (!compute_arc_center(start, cent, end, &c)) {
	line(start, &end, 1, lt);
	break;
      }
      solid_arc(c,
		hypot(cent - start),
		atan2(start.y - c.y, start.x - c.x),
		atan2(end.y - c.y, end.x - c.x),
		lt);
      break;
    }
  }
}

void tex_output::circle(const position &cent, double rad,
			const line_type &lt, double fill)
{
  if (fill >= 0.0 && lt.type != line_type::solid) {
    if (fill > 1.0)
      fill = 1.0;
    line_type ilt;
    ilt.type = line_type::invisible;
    ellipse(cent, position(rad*2.0, rad*2.0), ilt, fill);
  }
  switch (lt.type) {
  case line_type::dashed:
    dashed_circle(cent, rad, lt);
    break;
  case line_type::invisible:
    break;
  case line_type::solid:
    ellipse(cent, position(rad*2.0,rad*2.0), lt, fill);
    break;
  case line_type::dotted:
    dotted_circle(cent, rad, lt);
    break;
  default:
    assert(0);
  }
}

void tex_output::ellipse(const position &cent, const distance &dim,
			 const line_type &lt, double fill)
{
  if (lt.type == line_type::invisible) {
    if (fill < 0.0)
      return;
  }
  else
    set_pen_size(lt.thickness);
  if (fill >= 0.0) {
    if (fill > 1.0)
      fill = 1.0;
    printf("    \\special{sh %.3f}%%\n", fill);
  }
  position c = transform(cent);
  printf("    \\special{%s %d %d %d %d 0 6.28319}%%\n",
	 (lt.type == line_type::invisible ? "ia" : "ar"),
	 milliinches(c.x),
	 milliinches(c.y),
	 milliinches(dim.x/(2.0*scale)),
	 milliinches(dim.y/(2.0*scale)));
}

void tex_output::command(const char *s, const char *, int)
{
  fputs(s, stdout);
  putchar('%');			// avoid unwanted spaces
  putchar('\n');
}

int tex_output::supports_filled_polygons()
{
  return 1;
}

void tex_output::dot(const position &pos, const line_type &lt)
{
  if (zero_length_line_flag) {
    line_type slt = lt;
    slt.type = line_type::solid;
    line(pos, &pos, 1, slt);
  }
  else {
    int dot_rad = int(lt.thickness*(1000.0/(72.0*2)) + .5);
    if (dot_rad == 0)
      dot_rad = 1;
    position p = transform(pos);
    printf("    \\special{sh 1}%%\n"
	   "    \\special{ia %d %d %d %d 0 6.28319}%%\n",
	   milliinches(p.x), milliinches(p.y), dot_rad, dot_rad);
  }
}

class tpic_output : public tex_output {
public:
  tpic_output();
  void command(const char *, const char *, int);
private:
  void set_pen_size(double ps);
  int default_pen_size;
  int prev_default_pen_size;
};

tpic_output::tpic_output()
: default_pen_size(DEFAULT_PEN_SIZE), prev_default_pen_size(DEFAULT_PEN_SIZE)
{
}

void tpic_output::command(const char *s, const char *filename, int lineno)
{
  assert(s[0] == '.');
  if (s[1] == 'p' && s[2] == 's' && (s[3] == '\0' || !csalpha(s[3]))) {
    const char *p = s + 3;
    while (csspace(*p))
      p++;
    if (*p == '\0') {
      int temp = default_pen_size;
      default_pen_size = prev_default_pen_size;
      prev_default_pen_size = temp;
    }
    else {
      char *ptr;
      int temp = (int)strtol(p, &ptr, 10);
      if (temp == 0 && ptr == p)
	error_with_file_and_line(filename, lineno,
				 "argument to `.ps' not an integer");
      else if (temp < 0)
	error_with_file_and_line(filename, lineno,
				 "negative pen size");
      else {
	prev_default_pen_size = default_pen_size;
	default_pen_size = temp;
      }
    }
  }
  else
    printf("\\%s%%\n", s + 1);
}

void tpic_output::set_pen_size(double ps)
{
  if (ps < 0.0)
    printf("    \\special{pn %d}%%\n", default_pen_size);
  else
    tex_output::set_pen_size(ps);
}

output *make_tpic_output()
{
  return new tpic_output;
}
 
#endif
