
/* This file contains code for X-CHESS.
   Copyright (C) 1986 Free Software Foundation, Inc.

This file is part of X-CHESS.

X-CHESS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the X-CHESS General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
X-CHESS, but only under the conditions described in the
X-CHESS General Public License.   A copy of this license is
supposed to have been given to you along with X-CHESS so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  */


/* RCS Info: $Revision: 1.2 $ on $Date: 86/11/23 17:17:04 $
 *           $Source: /users/faustus/xchess/RCS/XCircle.c,v $
 * Copyright (c) 1986 Wayne A. Christopher, U. C. Berkeley CAD Group
 *	Permission is granted to do anything with this code except sell it
 *	or remove this message.
 *
 */

#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/X10.h>
#include <math.h>

#define PI 3.1415926535897932384

#define MAXVERTS 1000

void
XCircle(win, x, y, rad, start, end, width, height, pixel, func, planes)
	Window win;
	int x, y, rad;
	double start, end;
	int pixel;
	int width, height;
	int func, planes;
{
	Vertex verts[MAXVERTS];
	double xp, yp, ang;
	int lx, ly, xpt, ypt, i;
	double gradincr = 2 / (double) rad;
	int bk = 0;

	while (end >= PI * 2)
		end -= PI * 2;
	while (start >= PI * 2)
		start -= PI * 2;
	while (end < 0)
		end += PI * 2;
	while (start < 0)
		start += PI * 2;
	if (end == start) {
		if (end < gradincr)
			end = end + PI * 2 - gradincr / 2;
		else
			end -= gradincr / 2;
	}
	for (ang = start, i = 0; i < MAXVERTS; ) {

		xp = x + rad * cos(ang);
		yp = y + rad * sin(ang);

		xpt = xp;
		ypt = yp;

		if (!i || (lx != xpt) || (ly != ypt)) {
			verts[i].x = xpt;
			verts[i].y = ypt;
			verts[i].flags = 0;
			i++;
		}
		lx = xpt;
		ly = ypt;
		if (bk)
			break;
		if (((ang < end) && (ang + gradincr > end)) || ((end < start)
				&& (ang + gradincr > 2 * PI)
				&& (ang + gradincr - 2 * PI > end))) {
			ang = end;
			bk = 1;
		} else if (ang == end) {
			break;
		} else {
			ang += gradincr;
		}
		if (ang >= PI * 2)
			ang -= PI * 2;
	}

	/* Now draw the thing.. */
	XDraw(win, verts, i, width, height, pixel, func, planes);

	return;
}

#ifdef notdef	VertexCurved is screwed up

void
XCircle(win, x, y, rad, start, end, width, height, pixel, func, planes)
	Window win;
	int x, y, rad;
	double start, end;
	int pixel;
	int width, height;
	int func, planes;
{
	Vertex verts[7];
	int i, j, sv, ev;
	int dp = 0;

	for (i = j = 0 ; i < 4; i++) {
		verts[j].x = x + rad * cos((double) (PI * i / 2));
		verts[j].y = y + rad * sin((double) (PI * i / 2));
		verts[j].flags = VertexCurved;
		if ((start >= PI * i / 2) && (start < PI * (i + 1) / 2) &&
				(start != end)) {
			j++;
			verts[j].x = x + rad * cos(start);
			verts[j].y = y + rad * sin(start);
			verts[j].flags = VertexCurved;
			sv = j;
		} else if ((end >= PI * i / 2) && (end < PI * (i + 1) / 2)
				&& (start != end)) {
			j++;
			verts[j].x = x + rad * cos(end);
			verts[j].y = y + rad * sin(end);
			verts[j].flags = VertexCurved;
			ev = j;
		}
		j++;
	}
	verts[0].flags |= VertexStartClosed;
	verts[j].x = verts[0].x;
	verts[j].y = verts[0].y;
	verts[j].flags = (verts[0].flags & ~VertexStartClosed) | 
			VertexEndClosed;
	for (i = 0; i < 15; i++) {
		if (dp)
			verts[i % 7].flags |= VertexDontDraw;
		if (i % 7 == ev)
			dp = 1;
		else if (i % 7 == sv)
			dp = 0;
	}
	XDraw(win, verts, j + 1, width, height, pixel, func, planes);

	return;
}

#endif notdef

