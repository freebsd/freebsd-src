// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 1993, 2003
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
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

class common_output : public output {
private:
  void dash_line(const position &start, const position &end,
		 const line_type &lt, double dash_width, double gap_width,
		 double *offsetp);
  void dash_arc(const position &cent, double rad,
		double start_angle, double end_angle, const line_type &lt,
		double dash_width, double gap_width, double *offsetp);
  void dot_line(const position &start, const position &end,
		const line_type &lt, double gap_width, double *offsetp);
  void dot_arc(const position &cent, double rad,
	       double start_angle, double end_angle, const line_type &lt,
	       double gap_width, double *offsetp);
protected:
  virtual void dot(const position &, const line_type &) = 0;
  void ellipse_arc(const position &, const position &,
		   const position &, const distance &,
		   const line_type &);
  void dashed_circle(const position &, double rad, const line_type &);
  void dotted_circle(const position &, double rad, const line_type &);
  void dashed_ellipse(const position &, const distance &, const line_type &);
  void dotted_ellipse(const position &, const distance &, const line_type &);
  void dashed_arc(const position &, const position &, const position &,
		  const line_type &);
  void dotted_arc(const position &, const position &, const position &,
		  const line_type &);
  virtual void solid_arc(const position &cent, double rad, double start_angle,
			 double end_angle, const line_type &lt);
  void dashed_rounded_box(const position &, const distance &, double,
			  const line_type &);
  void dotted_rounded_box(const position &, const distance &, double,
			  const line_type &);
  void solid_rounded_box(const position &, const distance &, double,
			 const line_type &);
  void filled_rounded_box(const position &, const distance &, double, double);
public:
  void start_picture(double sc, const position &ll, const position &ur) = 0;
  void finish_picture() = 0;
  void circle(const position &, double rad, const line_type &, double) = 0;
  void text(const position &, text_piece *, int, double) = 0;
  void line(const position &, const position *, int n, const line_type &) = 0;
  void polygon(const position *, int n, const line_type &, double) = 0;
  void spline(const position &, const position *, int n,
	      const line_type &) = 0;
  void arc(const position &, const position &, const position &,
	   const line_type &) = 0;
  void ellipse(const position &, const distance &,
	       const line_type &, double) = 0;
  void rounded_box(const position &, const distance &, double,
		   const line_type &, double);
  void set_color(char *, char *) = 0;
  void reset_color() = 0;
  char *get_last_filled() = 0;
  char *get_outline_color() = 0;
};

int compute_arc_center(const position &start, const position &cent,
		       const position &end, position *result);

