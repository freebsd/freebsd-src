// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.
     Written by Gaius Mulley <gaius@glam.ac.uk>
     using adjust_arc_center() from printer.cpp, written by James Clark.

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


#include <stdio.h>
#include <math.h>

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))


// This utility function adjusts the specified center of the
// arc so that it is equidistant between the specified start
// and end points.  (p[0], p[1]) is a vector from the current
// point to the center; (p[2], p[3]) is a vector from the 
// center to the end point.  If the center can be adjusted,
// a vector from the current point to the adjusted center is
// stored in c[0], c[1] and 1 is returned.  Otherwise 0 is
// returned.

#if 1
int adjust_arc_center(const int *p, double *c)
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
  // the specified starting point and the specified end point.  Place
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
    // the specified start point and specified end point.  Place
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


/*
 *  check_output_arc_limits - works out the smallest box that will encompass
 *                            an arc defined by an origin (x, y) and two
 *                            vectors (p0, p1) and (p2, p3).
 *                            (x1, y1) -> start of arc
 *                            (x1, y1) + (xv1, yv1) -> center of circle
 *                            (x1, y1) + (xv1, yv1) + (xv2, yv2) -> end of arc
 *
 *                            Works out in which quadrant the arc starts and
 *                            stops, and from this it determines the x, y
 *                            max/min limits.  The arc is drawn clockwise.
 */

void check_output_arc_limits(int x_1, int y_1,
			     int xv_1, int yv_1,
			     int xv_2, int yv_2,
			     double c_0, double c_1,
			     int *minx, int *maxx,
			     int *miny, int *maxy)
{
  int radius = (int)sqrt(c_0 * c_0 + c_1 * c_1);
  // clockwise direction
  int xcenter = x_1 + xv_1;
  int ycenter = y_1 + yv_1;
  int xend = xcenter + xv_2;
  int yend = ycenter + yv_2;
  // for convenience, transform to counterclockwise direction,
  // centered at the origin
  int xs = xend - xcenter;
  int ys = yend - ycenter;
  int xe = x_1 - xcenter;
  int ye = y_1 - ycenter;
  *minx = *maxx = xs;
  *miny = *maxy = ys;
  if (xe > *maxx)
    *maxx = xe;
  else if (xe < *minx)
    *minx = xe;
  if (ye > *maxy)
    *maxy = ye;
  else if (ye < *miny)
    *miny = ye;
  int qs, qe;			// quadrants 0..3
  if (xs >= 0)
    qs = (ys >= 0) ? 0 : 3;
  else
    qs = (ys >= 0) ? 1 : 2;
  if (xe >= 0)
    qe = (ye >= 0) ? 0 : 3;
  else
    qe = (ye >= 0) ? 1 : 2;
  // make qs always smaller than qe
  if ((qs > qe)
      || ((qs == qe) && (double(xs) * ye < double(xe) * ys)))
    qe += 4;
  for (int i = qs; i < qe; i++)
    switch (i % 4) {
    case 0:
      *maxy = radius;
      break;
    case 1:
      *minx = -radius;
      break;
    case 2:
      *miny = -radius;
      break;
    case 3:
      *maxx = radius;
      break;
    }
  *minx += xcenter;
  *maxx += xcenter;
  *miny += ycenter;
  *maxy += ycenter;
}
