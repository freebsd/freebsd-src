// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2003 Free Software Foundation, Inc.
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

#include "pic.h"
#include "common.h"

// output a dashed circle as a series of arcs

void common_output::dashed_circle(const position &cent, double rad,
				  const line_type &lt)
{
  assert(lt.type == line_type::dashed);
  line_type slt = lt;
  slt.type = line_type::solid;
  double dash_angle = lt.dash_width/rad;
  int ndashes;
  double gap_angle;
  if (dash_angle >= M_PI/4.0) {
    if (dash_angle < M_PI/2.0) {
      gap_angle = M_PI/2.0 - dash_angle;
      ndashes = 4;
    }
    else if (dash_angle < M_PI) {
      gap_angle = M_PI - dash_angle;
      ndashes = 2;
    }
    else {
      circle(cent, rad, slt, -1.0);
      return;
    }
  }
  else {
    ndashes = 4*int(ceil(M_PI/(4.0*dash_angle)));
    gap_angle = (M_PI*2.0)/ndashes - dash_angle;
  }
  for (int i = 0; i < ndashes; i++) {
    double start_angle = i*(dash_angle+gap_angle) - dash_angle/2.0;
    solid_arc(cent, rad, start_angle, start_angle + dash_angle, lt);
  }
}

// output a dotted circle as a series of dots

void common_output::dotted_circle(const position &cent, double rad,
				  const line_type &lt)
{
  assert(lt.type == line_type::dotted);
  double gap_angle = lt.dash_width/rad;
  int ndots;
  if (gap_angle >= M_PI/2.0) {
    // always have at least 2 dots
    gap_angle = M_PI;
    ndots = 2;
  }
  else {
    ndots = 4*int(M_PI/(2.0*gap_angle));
    gap_angle = (M_PI*2.0)/ndots;
  }
  double ang = 0.0;
  for (int i = 0; i < ndots; i++, ang += gap_angle)
    dot(cent + position(cos(ang), sin(ang))*rad, lt);
}

// recursive function for dash drawing, used by dashed_ellipse

void common_output::ellipse_arc(const position &cent,
				const position &z0, const position &z1,
				const distance &dim, const line_type &lt)
{
  assert(lt.type == line_type::solid);
  assert(dim.x != 0 && dim.y != 0);
  double eps = 0.0001;
  position zml = (z0 + z1) / 2;
  // apply affine transformation (from ellipse to circle) to compute angle
  // of new position, then invert transformation to get exact position
  double psi = atan2(zml.y / dim.y, zml.x / dim.x);
  position zm = position(dim.x * cos(psi), dim.y * sin(psi));
  // to approximate the ellipse arc with one or more circle arcs, we
  // first compute the radius of curvature in zm
  double a_2 = dim.x * dim.x;
  double a_4 = a_2 * a_2;
  double b_2 = dim.y * dim.y;
  double b_4 = b_2 * b_2;
  double e_2 = a_2 - b_2;
  double temp = a_4 * zm.y * zm.y + b_4 * zm.x * zm.x;
  double rho = sqrt(temp / a_4 / b_4 * temp / a_4 / b_4 * temp);
  // compute center of curvature circle
  position M = position(e_2 * zm.x / a_2 * zm.x / a_2 * zm.x,
			-e_2 * zm.y / b_2 * zm.y / b_2 * zm.y);
  // compute distance between circle and ellipse arc at start and end
  double phi0 = atan2(z0.y - M.y, z0.x - M.x);
  double phi1 = atan2(z1.y - M.y, z1.x - M.x);
  position M0 = position(rho * cos(phi0), rho * sin(phi0)) + M;
  position M1 = position(rho * cos(phi1), rho * sin(phi1)) + M;
  double dist0 = hypot(z0 - M0) / sqrt(z0 * z0);
  double dist1 = hypot(z1 - M1) / sqrt(z1 * z1);
  if (dist0 < eps && dist1 < eps)
    solid_arc(M + cent, rho, phi0, phi1, lt);
  else {
    ellipse_arc(cent, z0, zm, dim, lt);
    ellipse_arc(cent, zm, z1, dim, lt);
  }
}

// output a dashed ellipse as a series of arcs

void common_output::dashed_ellipse(const position &cent, const distance &dim,
				   const line_type &lt)
{
  assert(lt.type == line_type::dashed);
  double dim_x = dim.x / 2;
  double dim_y = dim.y / 2;
  line_type slt = lt;
  slt.type = line_type::solid;
  double dw = lt.dash_width;
  // we use an approximation to compute the ellipse length (found in:
  // Bronstein, Semendjajew, Taschenbuch der Mathematik)
  double lambda = (dim.x - dim.y) / (dim.x + dim.y);
  double le = M_PI / 2 * (dim.x + dim.y)
	      * ((64 - 3 * lambda * lambda * lambda * lambda )
		 / (64 - 16 * lambda * lambda));
  // for symmetry we make nmax a multiple of 8
  int nmax = 8 * int(le / dw / 8 + 0.5);
  if (nmax < 8) {
    nmax = 8;
    dw = le / 8;
  }
  int ndash = nmax / 2;
  double gapwidth = (le - dw * ndash) / ndash;
  double l = 0;
  position z = position(dim_x, 0);
  position zdot = z;
  int j = 0;
  int jmax = int(10 / lt.dash_width);
  for (int i = 0; i <= nmax; i++) {
    position zold = z;
    position zpre = zdot;
    double ld = (int(i / 2) + 0.5) * dw + int((i + 1) / 2) * gapwidth;
    double lold = 0;
    double dl = 1;
    // find next position for fixed arc length
    while (l < ld) {
      j++;
      lold = l;
      zold = z;
      double phi = j * 2 * M_PI / jmax;
      z = position(dim_x * cos(phi), dim_y * sin(phi));
      dl = hypot(z - zold);
      l += dl;
    }
    // interpolate linearly between the last two points,
    // using the length difference as the scaling factor
    double delta = (ld - lold) / dl;
    zdot = zold + (z - zold) * delta;
    // compute angle of new position on the affine circle
    // and use it to get the exact value on the ellipse
    double psi = atan2(zdot.y / dim_y, zdot.x / dim_x);
    zdot = position(dim_x * cos(psi), dim_y * sin(psi));
    if ((i % 2 == 0) && (i > 1))
      ellipse_arc(cent, zpre, zdot, dim / 2, slt);
  }
}

// output a dotted ellipse as a series of dots

void common_output::dotted_ellipse(const position &cent, const distance &dim,
				   const line_type &lt)
{
  assert(lt.type == line_type::dotted);
  double dim_x = dim.x / 2;
  double dim_y = dim.y / 2;
  line_type slt = lt;
  slt.type = line_type::solid;
  // we use an approximation to compute the ellipse length (found in:
  // Bronstein, Semendjajew, Taschenbuch der Mathematik)
  double lambda = (dim.x - dim.y) / (dim.x + dim.y);
  double le = M_PI / 2 * (dim.x + dim.y)
	      * ((64 - 3 * lambda * lambda * lambda * lambda )
		 / (64 - 16 * lambda * lambda));
  // for symmetry we make nmax a multiple of 4
  int ndots = 4 * int(le / lt.dash_width / 4 + 0.5);
  if (ndots < 4)
    ndots = 4;
  double l = 0;
  position z = position(dim_x, 0);
  int j = 0;
  int jmax = int(10 / lt.dash_width);
  for (int i = 1; i <= ndots; i++) {
    position zold = z;
    double lold = l;
    double ld = i * le / ndots;
    double dl = 1;
    // find next position for fixed arc length
    while (l < ld) {
      j++;
      lold = l;
      zold = z;
      double phi = j * 2 * M_PI / jmax;
      z = position(dim_x * cos(phi), dim_y * sin(phi));
      dl = hypot(z - zold);
      l += dl;
    }
    // interpolate linearly between the last two points,
    // using the length difference as the scaling factor
    double delta = (ld - lold) / dl;
    position zdot = zold + (z - zold) * delta;
    // compute angle of new position on the affine circle
    // and use it to get the exact value on the ellipse
    double psi = atan2(zdot.y / dim_y, zdot.x / dim_x);
    zdot = position(dim_x * cos(psi), dim_y * sin(psi));
    dot(cent + zdot, slt);
  }
}

// return non-zero iff we can compute a center

int compute_arc_center(const position &start, const position &cent,
		       const position &end, position *result)
{
  // This finds the point along the vector from start to cent that
  // is equidistant between start and end.
  distance c = cent - start;
  distance e = end - start;
  double n = c*e;
  if (n == 0.0)
    return 0;
  *result = start + c*((e*e)/(2.0*n));
  return 1;
}

// output a dashed arc as a series of arcs

void common_output::dashed_arc(const position &start, const position &cent,
			       const position &end, const line_type &lt)
{
  assert(lt.type == line_type::dashed);
  position c;
  if (!compute_arc_center(start, cent, end, &c)) {
    line(start, &end, 1, lt);
    return;
  }
  distance start_offset = start - c;
  distance end_offset = end - c;
  double start_angle = atan2(start_offset.y, start_offset.x);
  double end_angle = atan2(end_offset.y, end_offset.x);
  double rad = hypot(c - start);
  double dash_angle = lt.dash_width/rad;
  double total_angle = end_angle - start_angle;
  while (total_angle < 0)
    total_angle += M_PI + M_PI;
  if (total_angle <= dash_angle*2.0) {
    solid_arc(cent, rad, start_angle, end_angle, lt);
    return;
  }
  int ndashes = int((total_angle - dash_angle)/(dash_angle*2.0) + .5);
  double dash_and_gap_angle = (total_angle - dash_angle)/ndashes;
  for (int i = 0; i <= ndashes; i++)
    solid_arc(cent, rad, start_angle + i*dash_and_gap_angle,
	      start_angle + i*dash_and_gap_angle + dash_angle, lt);
}

// output a dotted arc as a series of dots

void common_output::dotted_arc(const position &start, const position &cent,
			       const position &end, const line_type &lt)
{
  assert(lt.type == line_type::dotted);
  position c;
  if (!compute_arc_center(start, cent, end, &c)) {
    line(start, &end, 1, lt);
    return;
  }
  distance start_offset = start - c;
  distance end_offset = end - c;
  double start_angle = atan2(start_offset.y, start_offset.x);
  double total_angle = atan2(end_offset.y, end_offset.x) - start_angle;
  while (total_angle < 0)
    total_angle += M_PI + M_PI;
  double rad = hypot(c - start);
  int ndots = int(total_angle/(lt.dash_width/rad) + .5);
  if (ndots == 0)
    dot(start, lt);
  else {
    for (int i = 0; i <= ndots; i++) {
      double a = start_angle + (total_angle*i)/ndots;
      dot(cent + position(cos(a), sin(a))*rad, lt);
    }
  }
}

void common_output::solid_arc(const position &cent, double rad,
			      double start_angle, double end_angle,
			      const line_type &lt)
{
  line_type slt = lt;
  slt.type = line_type::solid;
  arc(cent + position(cos(start_angle), sin(start_angle))*rad,
      cent,
      cent + position(cos(end_angle), sin(end_angle))*rad,
      slt);
}


void common_output::rounded_box(const position &cent, const distance &dim,
				double rad, const line_type &lt, double fill)
{
  if (fill >= 0.0)
    filled_rounded_box(cent, dim, rad, fill);
  switch (lt.type) {
  case line_type::invisible:
    break;
  case line_type::dashed:
    dashed_rounded_box(cent, dim, rad, lt);
    break;
  case line_type::dotted:
    dotted_rounded_box(cent, dim, rad, lt);
    break;
  case line_type::solid:
    solid_rounded_box(cent, dim, rad, lt);
    break;
  default:
    assert(0);
  }
}


void common_output::dashed_rounded_box(const position &cent,
				       const distance &dim, double rad,
				       const line_type &lt)
{
  line_type slt = lt;
  slt.type = line_type::solid;

  double hor_length = dim.x + (M_PI/2.0 - 2.0)*rad;
  int n_hor_dashes = int(hor_length/(lt.dash_width*2.0) + .5);
  double hor_gap_width = (n_hor_dashes != 0
			  ? hor_length/n_hor_dashes - lt.dash_width
			  : 0.0);

  double vert_length = dim.y + (M_PI/2.0 - 2.0)*rad;
  int n_vert_dashes = int(vert_length/(lt.dash_width*2.0) + .5);
  double vert_gap_width = (n_vert_dashes != 0
			   ? vert_length/n_vert_dashes - lt.dash_width
			   : 0.0);
  // Note that each corner arc has to be split into two for dashing,
  // because one part is dashed using vert_gap_width, and the other
  // using hor_gap_width.
  double offset = lt.dash_width/2.0;
  dash_arc(cent + position(dim.x/2.0 - rad, -dim.y/2.0 + rad), rad,
	   -M_PI/4.0, 0, slt, lt.dash_width, vert_gap_width, &offset);
  dash_line(cent + position(dim.x/2.0, -dim.y/2.0 + rad),
	    cent + position(dim.x/2.0, dim.y/2.0 - rad),
	    slt, lt.dash_width, vert_gap_width, &offset);
  dash_arc(cent + position(dim.x/2.0 - rad, dim.y/2.0 - rad), rad,
	   0, M_PI/4.0, slt, lt.dash_width, vert_gap_width, &offset);

  offset = lt.dash_width/2.0;
  dash_arc(cent + position(dim.x/2.0 - rad, dim.y/2.0 - rad), rad,
	   M_PI/4.0, M_PI/2, slt, lt.dash_width, hor_gap_width, &offset);
  dash_line(cent + position(dim.x/2.0 - rad, dim.y/2.0),
	    cent + position(-dim.x/2.0 + rad, dim.y/2.0),
	    slt, lt.dash_width, hor_gap_width, &offset);
  dash_arc(cent + position(-dim.x/2.0 + rad, dim.y/2.0 - rad), rad,
	   M_PI/2, 3*M_PI/4.0, slt, lt.dash_width, hor_gap_width, &offset);

  offset = lt.dash_width/2.0;
  dash_arc(cent + position(-dim.x/2.0 + rad, dim.y/2.0 - rad), rad,
	   3.0*M_PI/4.0, M_PI, slt, lt.dash_width, vert_gap_width, &offset);
  dash_line(cent + position(-dim.x/2.0, dim.y/2.0 - rad),
	    cent + position(-dim.x/2.0, -dim.y/2.0 + rad),
	    slt, lt.dash_width, vert_gap_width, &offset);
  dash_arc(cent + position(-dim.x/2.0 + rad, -dim.y/2.0 + rad), rad,
	   M_PI, 5.0*M_PI/4.0, slt, lt.dash_width, vert_gap_width, &offset);

  offset = lt.dash_width/2.0;
  dash_arc(cent + position(-dim.x/2.0 + rad, -dim.y/2.0 + rad), rad,
	   5*M_PI/4.0, 3*M_PI/2.0, slt, lt.dash_width, hor_gap_width, &offset);
  dash_line(cent + position(-dim.x/2.0 + rad, -dim.y/2.0),
	    cent + position(dim.x/2.0 - rad, -dim.y/2.0),
	    slt, lt.dash_width, hor_gap_width, &offset);
  dash_arc(cent + position(dim.x/2.0 - rad, -dim.y/2.0 + rad), rad,
	   3*M_PI/2, 7*M_PI/4, slt, lt.dash_width, hor_gap_width, &offset);
}

// Used by dashed_rounded_box.

void common_output::dash_arc(const position &cent, double rad,
			     double start_angle, double end_angle,
			     const line_type &lt,
			     double dash_width, double gap_width,
			     double *offsetp)
{
  double length = (end_angle - start_angle)*rad;
  double pos = 0.0;
  for (;;) {
    if (*offsetp >= dash_width) {
      double rem = dash_width + gap_width - *offsetp;
      if (pos + rem > length) {
	*offsetp += length - pos;
	break;
      }
      else {
	pos += rem;
	*offsetp = 0.0;
      }
    }
    else {
      double rem = dash_width  - *offsetp;
      if (pos + rem > length) {
	solid_arc(cent, rad, start_angle + pos/rad, end_angle, lt);
	*offsetp += length - pos;
	break;
      }
      else {
	solid_arc(cent, rad, start_angle + pos/rad,
		  start_angle + (pos + rem)/rad, lt);
	pos += rem;
	*offsetp = dash_width;
      }
    }
  }
}

// Used by dashed_rounded_box.

void common_output::dash_line(const position &start, const position &end,
			      const line_type &lt,
			      double dash_width, double gap_width,
			      double *offsetp)
{
  distance dist = end - start;
  double length = hypot(dist);
  if (length == 0.0)
    return;
  double pos = 0.0;
  for (;;) {
    if (*offsetp >= dash_width) {
      double rem = dash_width + gap_width - *offsetp;
      if (pos + rem > length) {
	*offsetp += length - pos;
	break;
      }
      else {
	pos += rem;
	*offsetp = 0.0;
      }
    }
    else {
      double rem = dash_width  - *offsetp;
      if (pos + rem > length) {
	line(start + dist*(pos/length), &end, 1, lt);
	*offsetp += length - pos;
	break;
      }
      else {
	position p(start + dist*((pos + rem)/length));
	line(start + dist*(pos/length), &p, 1, lt);
	pos += rem;
	*offsetp = dash_width;
      }
    }
  }
}

void common_output::dotted_rounded_box(const position &cent,
				       const distance &dim, double rad,
				       const line_type &lt)
{
  line_type slt = lt;
  slt.type = line_type::solid;

  double hor_length = dim.x + (M_PI/2.0 - 2.0)*rad;
  int n_hor_dots = int(hor_length/lt.dash_width + .5);
  double hor_gap_width = (n_hor_dots != 0
			  ? hor_length/n_hor_dots
			  : lt.dash_width);

  double vert_length = dim.y + (M_PI/2.0 - 2.0)*rad;
  int n_vert_dots = int(vert_length/lt.dash_width + .5);
  double vert_gap_width = (n_vert_dots != 0
			   ? vert_length/n_vert_dots
			   : lt.dash_width);
  double epsilon = lt.dash_width/(rad*100.0);

  double offset = 0.0;
  dot_arc(cent + position(dim.x/2.0 - rad, -dim.y/2.0 + rad), rad,
	   -M_PI/4.0, 0, slt, vert_gap_width, &offset);
  dot_line(cent + position(dim.x/2.0, -dim.y/2.0 + rad),
	    cent + position(dim.x/2.0, dim.y/2.0 - rad),
	    slt, vert_gap_width, &offset);
  dot_arc(cent + position(dim.x/2.0 - rad, dim.y/2.0 - rad), rad,
	   0, M_PI/4.0 - epsilon, slt, vert_gap_width, &offset);

  offset = 0.0;
  dot_arc(cent + position(dim.x/2.0 - rad, dim.y/2.0 - rad), rad,
	   M_PI/4.0, M_PI/2, slt, hor_gap_width, &offset);
  dot_line(cent + position(dim.x/2.0 - rad, dim.y/2.0),
	    cent + position(-dim.x/2.0 + rad, dim.y/2.0),
	    slt, hor_gap_width, &offset);
  dot_arc(cent + position(-dim.x/2.0 + rad, dim.y/2.0 - rad), rad,
	   M_PI/2, 3*M_PI/4.0 - epsilon, slt, hor_gap_width, &offset);

  offset = 0.0;
  dot_arc(cent + position(-dim.x/2.0 + rad, dim.y/2.0 - rad), rad,
	   3.0*M_PI/4.0, M_PI, slt, vert_gap_width, &offset);
  dot_line(cent + position(-dim.x/2.0, dim.y/2.0 - rad),
	    cent + position(-dim.x/2.0, -dim.y/2.0 + rad),
	    slt, vert_gap_width, &offset);
  dot_arc(cent + position(-dim.x/2.0 + rad, -dim.y/2.0 + rad), rad,
	   M_PI, 5.0*M_PI/4.0 - epsilon, slt, vert_gap_width, &offset);

  offset = 0.0;
  dot_arc(cent + position(-dim.x/2.0 + rad, -dim.y/2.0 + rad), rad,
	   5*M_PI/4.0, 3*M_PI/2.0, slt, hor_gap_width, &offset);
  dot_line(cent + position(-dim.x/2.0 + rad, -dim.y/2.0),
	    cent + position(dim.x/2.0 - rad, -dim.y/2.0),
	    slt, hor_gap_width, &offset);
  dot_arc(cent + position(dim.x/2.0 - rad, -dim.y/2.0 + rad), rad,
	   3*M_PI/2, 7*M_PI/4 - epsilon, slt, hor_gap_width, &offset);
}

// Used by dotted_rounded_box.

void common_output::dot_arc(const position &cent, double rad,
			    double start_angle, double end_angle,
			    const line_type &lt, double gap_width,
			    double *offsetp)
{
  double length = (end_angle - start_angle)*rad;
  double pos = 0.0;
  for (;;) {
    if (*offsetp == 0.0) {
      double ang = start_angle + pos/rad;
      dot(cent + position(cos(ang), sin(ang))*rad, lt);
    }
    double rem = gap_width - *offsetp;
    if (pos + rem > length) {
      *offsetp += length - pos;
      break;
    }
    else {
      pos += rem;
      *offsetp = 0.0;
    }
  }
}

// Used by dotted_rounded_box.

void common_output::dot_line(const position &start, const position &end,
			     const line_type &lt, double gap_width,
			     double *offsetp)
{
  distance dist = end - start;
  double length = hypot(dist);
  if (length == 0.0)
    return;
  double pos = 0.0;
  for (;;) {
    if (*offsetp == 0.0)
      dot(start + dist*(pos/length), lt);
    double rem = gap_width - *offsetp;
    if (pos + rem > length) {
      *offsetp += length - pos;
      break;
    }
    else {
      pos += rem;
      *offsetp = 0.0;
    }
  }
}

void common_output::solid_rounded_box(const position &cent,
				      const distance &dim, double rad,
				      const line_type &lt)
{
  position tem = cent - dim/2.0;
  arc(tem + position(0.0, rad),
      tem + position(rad, rad),
      tem + position(rad, 0.0),
      lt);
  tem = cent + position(-dim.x/2.0, dim.y/2.0);
  arc(tem + position(rad, 0.0),
      tem + position(rad, -rad),
      tem + position(0.0, -rad),
      lt);
  tem = cent + dim/2.0;
  arc(tem + position(0.0, -rad),
      tem + position(-rad, -rad),
      tem + position(-rad, 0.0),
      lt);
  tem = cent + position(dim.x/2.0, -dim.y/2.0);
  arc(tem + position(-rad, 0.0),
      tem + position(-rad, rad),
      tem + position(0.0, rad),
      lt);
  position end;
  end = cent + position(-dim.x/2.0, dim.y/2.0 - rad);
  line(cent - dim/2.0 + position(0.0, rad), &end, 1, lt);
  end = cent + position(dim.x/2.0 - rad, dim.y/2.0);
  line(cent + position(-dim.x/2.0 + rad, dim.y/2.0), &end, 1, lt);
  end = cent + position(dim.x/2.0, -dim.y/2.0 + rad);
  line(cent + position(dim.x/2.0, dim.y/2.0 - rad), &end, 1, lt);
  end = cent + position(-dim.x/2.0 + rad, -dim.y/2.0);
  line(cent + position(dim.x/2.0 - rad, -dim.y/2.0), &end, 1, lt);
}

void common_output::filled_rounded_box(const position &cent,
				       const distance &dim, double rad,
				       double fill)
{
  line_type ilt;
  ilt.type = line_type::invisible;
  circle(cent + position(dim.x/2.0 - rad, dim.y/2.0 - rad), rad, ilt, fill);
  circle(cent + position(-dim.x/2.0 + rad, dim.y/2.0 - rad), rad, ilt, fill);
  circle(cent + position(-dim.x/2.0 + rad, -dim.y/2.0 + rad), rad, ilt, fill);
  circle(cent + position(dim.x/2.0 - rad, -dim.y/2.0 + rad), rad, ilt, fill);
  position vec[4];
  vec[0] = cent + position(dim.x/2.0, dim.y/2.0 - rad);
  vec[1] = cent + position(-dim.x/2.0, dim.y/2.0 - rad);
  vec[2] = cent + position(-dim.x/2.0, -dim.y/2.0 + rad);
  vec[3] = cent + position(dim.x/2.0, -dim.y/2.0 + rad);
  polygon(vec, 4, ilt, fill);
  vec[0] = cent + position(dim.x/2.0 - rad, dim.y/2.0);
  vec[1] = cent + position(-dim.x/2.0 + rad, dim.y/2.0);
  vec[2] = cent + position(-dim.x/2.0 + rad, -dim.y/2.0);
  vec[3] = cent + position(dim.x/2.0 - rad, -dim.y/2.0);
  polygon(vec, 4, ilt, fill);
}
