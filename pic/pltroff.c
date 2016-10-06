/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/pic/
 *
 * Copyright (C) 2003, Lucent Technologies Inc. and others.
 * All Rights Reserved.
 *
 * Distributed under the terms of the Lucent Public License Version 1.02.
 */

/*	Sccsid @(#)pltroff.c	1.3 (gritter) 2/8/06	*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "pic.h"
extern int dbg;

#define	abs(n)	(n >= 0 ? n : -(n))
#define	max(x,y)	((x)>(y) ? (x) : (y))

char	*textshift = "\\v'.2m'";	/* move text this far down */

/* scaling stuff defined by s command as X0,Y0 to X1,Y1 */
/* output dimensions set by -l,-w options to 0,0 to hmax, vmax */
/* default output is 6x6 inches */


double	xscale;
double	yscale;

double	hpos	= 0;	/* current horizontal position in output coordinate system */
double	vpos	= 0;	/* current vertical position; 0 is top of page */

double	htrue	= 0;	/* where we really are */
double	vtrue	= 0;

double	X0, Y0;		/* left bottom of input */
double	X1, Y1;		/* right top of input */

double	hmax;		/* right end of output */
double	vmax;		/* top of output (down is positive) */

extern	double	deltx;
extern	double	delty;
extern	double	xmin, ymin, xmax, ymax;

double	xconv(double), yconv(double), xsc(double), ysc(double);
void	space(double, double, double, double);
void	hgoto(double), vgoto(double), hmot(double), vmot(double);
void	move(double, double), movehv(double, double);
void	cont(double, double);

void openpl(char *s)	/* initialize device; s is residue of .PS invocation line */
{
	double maxw, maxh, ratio = 1;
	double odeltx = deltx, odelty = delty;

	hpos = vpos = 0;
	maxw = getfval("maxpswid");
	maxh = getfval("maxpsht");
	if (deltx > maxw) {	/* shrink horizontal */
		ratio = maxw / deltx;
		deltx *= ratio;
		delty *= ratio;
	}
	if (delty > maxh) {	/* shrink vertical */
		ratio = maxh / delty;
		deltx *= ratio;
		delty *= ratio;
	}
	if (ratio != 1) {
		fprintf(stderr, "pic: %g X %g picture shrunk to", odeltx, odelty);
		fprintf(stderr, " %g X %g\n", deltx, delty);
	}
	space(xmin, ymin, xmax, ymax);
	printf(".\\\" %g %g %g %g\n", xmin, ymin, xmax, ymax);
	printf(".\\\" %.3fi %.3fi %.3fi %.3fi\n",
		xconv(xmin), yconv(ymin), xconv(xmax), yconv(ymax));
	printf(".nr 00 \\n(.u\n");
	printf(".nf\n");
	printf(".PS %.3fi %.3fi %s", yconv(ymin), xconv(xmax), s);
		/* assumes \n comes as part of s */
}

void space(double x0, double y0, double x1, double y1)	/* set limits of page */
{
	X0 = x0;
	Y0 = y0;
	X1 = x1;
	Y1 = y1;
	xscale = deltx == 0.0 ? 1.0 : deltx / (X1-X0);
	yscale = delty == 0.0 ? 1.0 : delty / (Y1-Y0);
}

double xconv(double x)	/* convert x from external to internal form */
{
	return (x-X0) * xscale;
}

double xsc(double x)	/* convert x from external to internal form, scaling only */
{

	return (x) * xscale;
}

double yconv(double y)	/* convert y from external to internal form */
{
	return (Y1-y) * yscale;
}

double ysc(double y)	/* convert y from external to internal form, scaling only */
{
	return (y) * yscale;
}

void closepl(char *PEline)	/* clean up after finished */
{
	printf(".if n .do\n");
	movehv(0.0, 0.0);	/* get back to where we started */
	if (strchr(PEline, 'F') == NULL) {
		printf(".sp 1+%.3fi\n", yconv(ymin));
	}
	printf("%s\n", PEline);
	printf(".if \\n(00 .fi\n");
}

void move(double x, double y)	/* go to position x, y in external coords */
{
	hgoto(xconv(x));
	vgoto(yconv(y));
}

void movehv(double h, double v)	/* go to internal position h, v */
{
	hgoto(h);
	vgoto(v);
}

void hmot(double n)	/* generate n units of horizontal motion */
{
	hpos += n;
}

void vmot(double n)	/* generate n units of vertical motion */
{
	vpos += n;
}

void hgoto(double n)
{
	hpos = n;
}

void vgoto(double n)
{
	vpos = n;
}

double fabs(double x)
{
	return x < 0 ? -x : x;
}

void hvflush(void)	/* get to proper point for output */
{
	if (fabs(hpos-htrue) >= 0.0005) {
		printf("\\h'%.3fi'", hpos - htrue);
		htrue = hpos;
	}
	if (fabs(vpos-vtrue) >= 0.0005) {
		printf("\\v'%.3fi'", vpos - vtrue);
		vtrue = vpos;
	}
}

void flyback(void)	/* return to upper left corner (entry point) */
{
	printf(".sp -1\n");
	htrue = vtrue = 0;
}

void printlf(int n, char *f)
{
	if (f)
		printf(".lf %d %s\n", n, f);
	else
		printf(".lf %d\n", n);
}

void troff(char *s)	/* output troff right here */
{
	printf("%s\n", s);
}

void label(char *s, int t, int nh)	/* text s of type t nh half-lines up */
{
	int q;
	char *p;

	if (!s)
		return;
	hvflush();
	dprintf("label: %s %o %d\n", s, t, nh);
	printf("%s", textshift);	/* shift down and left */
	if (t & ABOVE)
		nh++;
	else if (t & BELOW)
		nh--;
	if (nh)
		printf("\\v'%du*\\n(.vu/2u'", -nh);
	/* just in case the text contains a quote: */
	q = 0;
	for (p = s; *p; p++)
		if (*p == '\'') {
			q = 1;
			break;
		}
	t &= ~(ABOVE|BELOW);
	if (t & LJUST) {
		printf("%s", s);
	} else if (t & RJUST) {
		if (q)
			printf("\\h\\(ts-\\w\\(ts%s\\(tsu\\(ts%s", s, s);
		else
			printf("\\h'-\\w'%s'u'%s", s, s);
	} else {	/* CENTER */
		if (q)
			printf("\\h\\(ts-\\w\\(ts%s\\(tsu/2u\\(ts%s", s, s);
		else
			printf("\\h'-\\w'%s'u/2u'%s", s, s);
	}
	printf("\n");
	flyback();
}

void line(double x0, double y0, double x1, double y1)	/* draw line from x0,y0 to x1,y1 */
{
	move(x0, y0);
	cont(x1, y1);
}

void arrow(double x0, double y0, double x1, double y1, double w, double h,
	 double ang, int nhead) 	/* draw arrow (without shaft) */
{
	double alpha, rot, drot, hyp;
	double dx, dy;
	int i;

	rot = atan2(w / 2, h);
	hyp = sqrt(w/2 * w/2 + h * h);
	alpha = atan2(y1-y0, x1-x0) + ang;
	if (nhead < 2)
		nhead = 2;
	dprintf("rot=%g, hyp=%g, alpha=%g\n", rot, hyp, alpha);
	printf(".if t \\{\\\n");
	for (i = nhead-1; i >= 0; i--) {
		drot = 2 * rot / (double) (nhead-1) * (double) i;
		dx = hyp * cos(alpha + PI - rot + drot);
		dy = hyp * sin(alpha + PI - rot + drot);
		dprintf("dx,dy = %g,%g\n", dx, dy);
		line(x1+dx, y1+dy, x1, y1);
	}
	printf(".\\}\n");
}

double lastgray = 0;

void fillstart(double v)	/* this works only for postscript, obviously. */
{				/* uses drechsler's dpost conventions... */
	hvflush();
	printf("\\X'BeginObject %g setgray'\n", v);
	lastgray = v;
	flyback();
}

void fillend(int vis, int fill)
{
	hvflush();
	printf("\\X'EndObject gsave eofill grestore %g setgray %s'\n",
		!vis ? lastgray : 0.0,
		vis ? "stroke" : "");
	/* for dashed: [50] 0 setdash just before stroke. */
	lastgray = 0;
	flyback();
}

void box(double x0, double y0, double x1, double y1)
{
	move(x0, y0);
	cont(x0, y1);
	cont(x1, y1);
	cont(x1, y0);
	cont(x0, y0);
}

void cont(double x, double y)	/* continue line from here to x,y */
{
	double h1, v1;
	double dh, dv;
	int rh = 0;

	h1 = xconv(x);
	v1 = yconv(y);
	dh = h1 - hpos;
	dv = v1 - vpos;
	if (dh > 0 && !dv) {
		rh = 1;
		move(x, y);
		dh = -dh;
	}
	hvflush();
	printf("\\D'l%.3fi %.3fi'\n", dh, dv);
	if (rh) move(x, y);
	flyback();	/* expensive */
	hpos = h1;
	vpos = v1;
}

void circle(double x, double y, double r)
{
	move(x-r, y);
	hvflush();
	printf("\\D'c%.3fi'\n", xsc(2 * r));
	flyback();
}

void spline(double x, double y, double n, ofloat *p, int dashed, double ddval)
{
	int i;
	double dx, dy;
	double xerr, yerr;

	move(x, y);
	hvflush();
	xerr = yerr = 0.0;
	printf("\\D'~");
	for (i = 0; i < 2 * n; i += 2) {
		dx = xsc(xerr += p[i]);
		xerr -= dx/xscale;
		dy = ysc(yerr += p[i+1]);
		yerr -= dy/yscale;
		printf(" %.3fi %.3fi", dx, -dy);	/* WATCH SIGN */
	}
	printf("'\n");
	flyback();
}

void ellipse(double x, double y, double r1, double r2)
{
	double ir1, ir2;

	move(x-r1, y);
	hvflush();
	ir1 = xsc(r1);
	ir2 = ysc(r2);
	printf("\\D'e%.3fi %.3fi'\n", 2 * ir1, 2 * abs(ir2));
	flyback();
}

void arc(double x, double y, double x0, double y0, double x1, double y1)	/* draw arc with center x,y */
{

	move(x0, y0);
	hvflush();
	printf("\\D'a%.3fi %.3fi %.3fi %.3fi'\n",
		xsc(x-x0), -ysc(y-y0), xsc(x1-x), -ysc(y1-y));	/* WATCH SIGNS */
	flyback();
}

void dot(void) {
	hvflush();
	/* what character to draw here depends on what's available. */
	/* on the 202, l. is good but small. */
	/* in general, use a smaller, shifted period and hope */

	printf("\\&\\f1\\h'-.1m'\\v'.03m'\\s-3.\\s+3\\fP\n");
	flyback();
}
