/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 1989 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


/*	from OpenSolaris "draw.c	1.5	05/06/08 SMI"	*/

/*
 * Portions Copyright (c) 2005 Gunnar Ritter, Freiburg i. Br., Germany
 *
 * Sccsid @(#)draw.c	1.3 (gritter) 8/8/05
 */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<math.h>
#define	PI	3.141592654
#define	hmot(n)		hpos += n
#define	hgoto(n)	hpos = n
#define	vmot(n)		vgoto(vpos + n)

extern	int	hpos;
extern	int	vpos;
extern	int	size;
extern	short	*pstab;
extern	int	DX;	/* step size in x */
extern	int	DY;	/* step size in y */
extern	int	drawdot;	/* character to use when drawing */
extern	int	drawsize;	/* shrink point size by this facter */

int	maxdots	= 32000;	/* maximum number of dots in an object */

#define	sgn(n)	((n > 0) ? 1 : ((n < 0) ? -1 : 0))
#define	abs(n)	((n) >= 0 ? (n) : -(n))
#define	max(x,y)	((x) > (y) ? (x) : (y))
#define	min(x,y)	((x) < (y) ? (x) : (y))
#define	arcmove(x,y)	{ hgoto(x); vmot(-vpos-(y)); }

extern void setsize(int);
extern void vgoto(int);
extern int t_size(int);
extern void put1(int);

void drawline(int, int, char *);
void drawwig(char *);
char *getstr(char *, char *);
void drawcirc(int);
int dist(int, int, int, int);
void drawarc(int, int, int, int);
void drawellip(int, int);
void conicarc(int, int, int, int, int, int, int, int);
void putdot(int, int);

void
drawline(int dx, int dy, char *s) /* draw line from here to dx, dy using s */
{
	int xd, yd;
	float val, slope;
	int i, numdots;
	int dirmot, perp;
	int motincr, perpincr;
	int ohpos, ovpos, osize;
	float incrway;

	int itemp; /*temp. storage for value returned byint function sgn*/
	osize = size;
	setsize(t_size(pstab[osize-1] / drawsize));
	ohpos = hpos;
	ovpos = vpos;
	xd = dx / DX;
	yd = dy / DX;
	if (xd == 0) {
		numdots = abs (yd);
		numdots = min(numdots, maxdots);
		motincr = DX * sgn (yd);
		for (i = 0; i < numdots; i++) {
			vmot(motincr);
			put1(drawdot);
		}
		vgoto(ovpos + dy);
		setsize(osize);
		return;
	}
	if (yd == 0) {
		numdots = abs (xd);
		motincr = DX * sgn (xd);
		for (i = 0; i < numdots; i++) {
			hmot(motincr);
			put1(drawdot);
		}
		hgoto(ohpos + dx);
		setsize(osize);
		return;
	}
	if (abs (xd) > abs (yd)) {
		val = slope = (float) xd/yd;
		numdots = abs (xd);
		numdots = min(numdots, maxdots);
		dirmot = 'h';
		perp = 'v';
		motincr = DX * sgn (xd);
		perpincr = DX * sgn (yd);
	}
	else {
		val = slope = (float) yd/xd;
		numdots = abs (yd);
		numdots = min(numdots, maxdots);
		dirmot = 'v';
		perp = 'h';
		motincr = DX * sgn (yd);
		perpincr = DX * sgn (xd);
	}
	incrway = itemp = sgn ((int) slope);
	for (i = 0; i < numdots; i++) {
		val -= incrway;
		if (dirmot == 'h')
			hmot(motincr);
		else
			vmot(motincr);
		if (val * slope < 0) {
			if (perp == 'h')
				hmot(perpincr);
			else
				vmot(perpincr);
			val += slope;
		}
		put1(drawdot);
	}
	hgoto(ohpos + dx);
	vgoto(ovpos + dy);
	setsize(osize);
}

void
drawwig(char *s)	/* draw wiggly line */
{
	int x[50], y[50], xp, yp, pxp, pyp;
	float t1, t2, t3, w;
	int i, j, numdots, N;
	int osize;
	char temp[50], *p;

	osize = size;
	setsize(t_size(pstab[osize-1] / drawsize));
	p = s;
	for (N = 2; (p=getstr(p,temp)) != NULL && N < sizeof(x)/sizeof(x[0]); N++) {
		x[N] = atoi(temp);
		p = getstr(p, temp);
		y[N] = atoi(temp);
	}
	x[0] = x[1] = hpos;
	y[0] = y[1] = vpos;
	for (i = 1; i < N; i++) {
		x[i+1] += x[i];
		y[i+1] += y[i];
	}
	x[N] = x[N-1];
	y[N] = y[N-1];
	pxp = pyp = -9999;
	for (i = 0; i < N-1; i++) {	/* interval */
		numdots = (dist(x[i],y[i], x[i+1],y[i+1]) + dist(x[i+1],y[i+1], x[i+2],y[i+2])) / 2;
		numdots /= DX;
		numdots = min(numdots, maxdots);
		for (j = 0; j < numdots; j++) {	/* points within */
			w = (float) j / numdots;
			t1 = 0.5 * w * w;
			w = w - 0.5;
			t2 = 0.75 - w * w;
			w = w - 0.5;
			t3 = 0.5 * w * w;
			xp = t1 * x[i+2] + t2 * x[i+1] + t3 * x[i] + 0.5;
			yp = t1 * y[i+2] + t2 * y[i+1] + t3 * y[i] + 0.5;
			if (xp != pxp || yp != pyp) {
				hgoto(xp);
				vgoto(yp);
				put1(drawdot);
				pxp = xp;
				pyp = yp;
			}
		}
	}
	setsize(osize);
}

char *getstr(char *p, char *temp)	/* copy next non-blank string from p to temp, update p */
{
	while (*p == ' ' || *p == '\t' || *p == '\n')
		p++;
	if (*p == '\0') {
		temp[0] = 0;
		return(NULL);
	}
	while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
		*temp++ = *p++;
	*temp = '\0';
	return(p);
}

void
drawcirc(int d)
{
	int xc, yc;

	xc = hpos;
	yc = vpos;
	conicarc(hpos + d/2, -vpos, hpos, -vpos, hpos, -vpos, d/2, d/2);
	hgoto(xc + d);	/* circle goes to right side */
	vgoto(yc);
}

int
dist(int x1, int y1, int x2, int y2) /* integer distance from x1,y1 to x2,y2 */
{
	float dx, dy;

	dx = x2 - x1;
	dy = y2 - y1;
	return sqrt(dx*dx + dy*dy) + 0.5;
}

void
drawarc(int dx1, int dy1, int dx2, int dy2)
{
	int x0, y0, x2, y2, r;

	x0 = hpos + dx1;	/* center */
	y0 = vpos + dy1;
	x2 = x0 + dx2;	/* "to" */
	y2 = y0 + dy2;
	r = sqrt((float) dx1 * dx1 + (float) dy1 * dy1) + 0.5;
	conicarc(x0, -y0, hpos, -vpos, x2, -y2, r, r);
}

void
drawellip(int a, int b)
{
	int xc, yc;

	xc = hpos;
	yc = vpos;
	conicarc(hpos + a/2, -vpos, hpos, -vpos, hpos, -vpos, a/2, b/2);
	hgoto(xc + a);
	vgoto(yc);
}

#define sqr(x) (long int)(x)*(x)

void
conicarc(int x, int y, int x0, int y0, int x1, int y1, int a, int b)
{
	/* based on Bresenham, CACM, Feb 77, pp 102-3 */
	/* by Chris Van Wyk */
	/* capitalized vars are an internal reference frame */
	long dotcount = 0;
	int osize;
	int	xs, ys, xt, yt, Xs, Ys, qs, Xt, Yt, qt,
		M1x, M1y, M2x, M2y, M3x, M3y,
		Q, move, Xc, Yc;
	int ox1, oy1;
	long	delta;
	float	xc, yc;
	float	radius, slope;
	float	xstep, ystep;

	osize = size;
	setsize(t_size(pstab[osize-1] / drawsize));
	ox1 = x1;
	oy1 = y1;
	if (a != b)	/* an arc of an ellipse; internally, will still think of circle */
		if (a > b) {
			xstep = (float)a / b;
			ystep = 1;
			radius = b;
		} else {
			xstep = 1;
			ystep = (float)b / a;
			radius = a;
		} 
	else {	/* a circular arc; radius is computed from center and first point */	
		xstep = ystep = 1;
		radius = sqrt((float)(sqr(x0 - x) + sqr(y0 - y)));
	}


	xc = x0;
	yc = y0;
	/* now, use start and end point locations to figure out
	the angle at which start and end happen; use these
	angles with known radius to figure out where start
	and end should be
	*/
	slope = atan2((double)(y0 - y), (double)(x0 - x) );
	if (slope == 0.0 && x0 < x)
		slope = 3.14159265;
	x0 = x + radius * cos(slope) + 0.5;
	y0 = y + radius * sin(slope) + 0.5;
	slope = atan2((double)(y1 - y), (double)(x1 - x));
	if (slope == 0.0 && x1 < x)
		slope = 3.14159265;
	x1 = x + radius * cos(slope) + 0.5;
	y1 = y + radius * sin(slope) + 0.5;
	/* step 2: translate to zero-centered circle */
	xs = x0 - x;
	ys = y0 - y;
	xt = x1 - x;
	yt = y1 - y;
	/* step 3: normalize to first quadrant */
	if (xs < 0)
		if (ys < 0) {
			Xs = abs(ys);
			Ys = abs(xs);
			qs = 3;
			M1x = 0;
			M1y = -1;
			M2x = 1;
			M2y = -1;
			M3x = 1;
			M3y = 0;
		} else {
			Xs = abs(xs);
			Ys = abs(ys);
			qs = 2;
			M1x = -1;
			M1y = 0;
			M2x = -1;
			M2y = -1;
			M3x = 0;
			M3y = -1;
		} 
	else if (ys < 0) {
		Xs = abs(xs);
		Ys = abs(ys);
		qs = 0;
		M1x = 1;
		M1y = 0;
		M2x = 1;
		M2y = 1;
		M3x = 0;
		M3y = 1;
	} else {
		Xs = abs(ys);
		Ys = abs(xs);
		qs = 1;
		M1x = 0;
		M1y = 1;
		M2x = -1;
		M2y = 1;
		M3x = -1;
		M3y = 0;
	}


	Xc = Xs;
	Yc = Ys;
	if (xt < 0)
		if (yt < 0) {
			Xt = abs(yt);
			Yt = abs(xt);
			qt = 3;
		} else {
			Xt = abs(xt);
			Yt = abs(yt);
			qt = 2;
		} 
	else if (yt < 0) {
		Xt = abs(xt);
		Yt = abs(yt);
		qt = 0;
	} else {
		Xt = abs(yt);
		Yt = abs(xt);
		qt = 1;
	}


	/* step 4: calculate number of quadrant crossings */
	if (((4 + qt - qs)
	     % 4 == 0)
	     && (Xt <= Xs)
	     && (Yt >= Ys)
	    )
		Q = 3;
	else
		Q = (4 + qt - qs) % 4 - 1;
	/* step 5: calculate initial decision difference */
	delta = sqr(Xs + 1)
	 + sqr(Ys - 1)
	-sqr(xs)
	-sqr(ys);
	/* here begins the work of drawing
   we hope it ends here too */
	while ((Q >= 0)
	     || ((Q > -2)
	     && ((Xt > Xc)
	     && (Yt < Yc)
	    )
	    )
	    ) {
		if (dotcount++ % DX == 0)
			putdot((int)xc, (int)yc);
		if (Yc < 0.5) {
			/* reinitialize */
			Xs = Xc = 0;
			Ys = Yc = sqrt((float)(sqr(xs) + sqr(ys)));
			delta = sqr(Xs + 1) + sqr(Ys - 1) - sqr(xs) - sqr(ys);
			Q--;
			M1x = M3x;
			M1y = M3y;
			 {
				int	T;
				T = M2y;
				M2y = M2x;
				M2x = -T;
				T = M3y;
				M3y = M3x;
				M3x = -T;
			}
		} else {
			if (delta <= 0)
				if (2 * delta + 2 * Yc - 1 <= 0)
					move = 1;
				else
					move = 2;
			else if (2 * delta - 2 * Xc - 1 <= 0)
				move = 2;
			else
				move = 3;
			switch (move) {
			case 1:
				Xc++;
				delta += 2 * Xc + 1;
				xc += M1x * xstep;
				yc += M1y * ystep;
				break;
			case 2:
				Xc++;
				Yc--;
				delta += 2 * Xc - 2 * Yc + 2;
				xc += M2x * xstep;
				yc += M2y * ystep;
				break;
			case 3:
				Yc--;
				delta -= 2 * Yc + 1;
				xc += M3x * xstep;
				yc += M3y * ystep;
				break;
			}
		}
	}


	setsize(osize);
	drawline((int)xc-ox1,(int)yc-oy1,".");
}

void
putdot(int x, int y)
{
	arcmove(x, y);
	put1(drawdot);
}
