// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2002, 2003
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */


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
 *
 *                            [I'm sure there is a better way to do this, but
 *                             I don't know how.  Please can someone let me
 *                             know or "improve" this function.]
 */

void check_output_arc_limits(int x1, int y1,
			     int xv1, int yv1,
			     int xv2, int yv2,
			     double c0, double c1,
			     int *minx, int *maxx,
			     int *miny, int *maxy)
{
  int radius = (int)sqrt(c0*c0 + c1*c1);
  int x2 = x1 + xv1 + xv2;			// end of arc is (x2, y2)
  int y2 = y1 + yv1 + yv2;

  // firstly lets use the `circle' limitation
  *minx = x1 + xv1 - radius;
  *maxx = x1 + xv1 + radius;
  *miny = y1 + yv1 - radius;
  *maxy = y1 + yv1 + radius;

  /*  now to see which min/max can be reduced and increased for the limits of
   *  the arc
   *
   *       Q2   |   Q1
   *       -----+-----
   *       Q3   |   Q4
   *
   *
   *  NB. (x1+xv1, y1+yv1) is at the origin
   *
   *  below we ask a nested question
   *  (i)  from which quadrant does the first vector start?
   *  (ii) into which quadrant does the second vector go?
   *  from the 16 possible answers we determine the limits of the arc
   */
  if (xv1 > 0 && yv1 > 0) {
    // first vector in Q3
    if (xv2 >= 0 && yv2 >= 0 ) {
      // second in Q1
      *maxx = x2;
      *miny = y1;
    }
    else if (xv2 < 0 && yv2 >= 0) {
      // second in Q2
      *maxx = x2;
      *miny = y1;
    }
    else if (xv2 >= 0 && yv2 < 0) {
      // second in Q4
      *miny = MIN(y1, y2);
    }
    else if (xv2 < 0 && yv2 < 0) {
      // second in Q3
      if (x1 >= x2) {
	*minx = x2;
	*maxx = x1;
	*miny = MIN(y1, y2);
	*maxy = MAX(y1, y2);
      }
      else {
	// xv2, yv2 could all be zero?
      }
    }
  }
  else if (xv1 > 0 && yv1 < 0) {
    // first vector in Q2
    if (xv2 >= 0 && yv2 >= 0) {
      // second in Q1
      *maxx = MAX(x1, x2);
      *minx = MIN(x1, x2);
      *miny = y1;
    }
    else if (xv2 < 0 && yv2 >= 0) {
      // second in Q2
      if (x1 < x2) {
	*maxx = x2;
	*minx = x1;
	*miny = MIN(y1, y2);
	*maxy = MAX(y1, y2);
      }
      else {
	// otherwise almost full circle anyway
      }
    }
    else if (xv2 >= 0 && yv2 < 0) {
      // second in Q4
      *miny = y2;
      *minx = x1;
    }
    else if (xv2 < 0 && yv2 < 0) {
      // second in Q3
      *minx = MIN(x1, x2);
    }
  }
  else if (xv1 <= 0 && yv1 <= 0) {
    // first vector in Q1
    if (xv2 >= 0 && yv2 >= 0) {
      // second in Q1
      if (x1 < x2) {
	*minx = x1;
	*maxx = x2;
	*miny = MIN(y1, y2);
	*maxy = MAX(y1, y2);
      }
      else {
	// nearly full circle
      }
    }
    else if (xv2 < 0 && yv2 >= 0) {
      // second in Q2
      *maxy = MAX(y1, y2);
    }
    else if (xv2 >= 0 && yv2 < 0) {
      // second in Q4
      *miny = MIN(y1, y2);
      *maxy = MAX(y1, y2);
      *minx = MIN(x1, x2);
    }
    else if (xv2 < 0 && yv2 < 0) {
      // second in Q3
      *minx = x2;
      *maxy = y1;
    }
  }
  else if (xv1 <= 0 && yv1 > 0) {
    // first vector in Q4
    if (xv2 >= 0 && yv2 >= 0) {
      // second in Q1
      *maxx = MAX(x1, x2);
    }
    else if (xv2 < 0 && yv2 >= 0) {
      // second in Q2
      *maxy = MAX(y1, y2);
      *maxx = MAX(x1, x2);
    }
    else if (xv2 >= 0 && yv2 < 0) {
      // second in Q4
      if (x1 >= x2) {
	*miny = MIN(y1, y2);
	*maxy = MAX(y1, y2);
	*minx = MIN(x1, x2);
	*maxx = MAX(x2, x2);
      }
      else {
	// nearly full circle
      }
    }
    else if (xv2 < 0 && yv2 < 0) {
      // second in Q3
      *maxy = MAX(y1, y2);
      *minx = MIN(x1, x2);
      *maxx = MAX(x1, x2);
    }
  }

  // this should *never* happen but if it does it means a case above is wrong
  // this code is only present for safety sake
  if (*maxx < *minx) {
    fprintf(stderr, "assert failed *minx > *maxx\n");
    fflush(stderr);
    *maxx = *minx;
  }
  if (*maxy < *miny) {
    fprintf(stderr, "assert failed *miny > *maxy\n");
    fflush(stderr);
    *maxy = *miny;
  }
}
