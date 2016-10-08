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

/*	Sccsid @(#)print.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <math.h>
#include "pic.h"
#include "y.tab.h"

void dotext(obj *);
void dotline(double, double, double, double, int, double);
void dotbox(double, double, double, double, int, double);
void ellipse(double, double, double, double);
void circle(double, double, double);
void arc(double, double, double, double, double, double);
void arrow(double, double, double, double, double, double, double, int);
void line(double, double, double, double);
void box(double, double, double, double);
void spline(double x, double y, double n, ofloat *p, int dashed, double ddval);
void move(double, double);
void troff(char *);
void dot(void);
void fillstart(double), fillend(int vis, int noedge);

void print(void)
{
	obj *p;
	int i, j, k, m;
	int fill, vis, invis;
	double x0, y0, x1 = 0, y1 = 0, ox, oy, dx, dy, ndx, ndy;

	for (i = 0; i < nobj; i++) {
		p = objlist[i];
		ox = p->o_x;
		oy = p->o_y;
		if (p->o_count >= 1)
			x1 = p->o_val[0];
		if (p->o_count >= 2)
			y1 = p->o_val[1];
		m = p->o_mode;
		fill = p->o_attr & FILLBIT;
		invis = p->o_attr & INVIS;
		vis = !invis;
		switch (p->o_type) {
		case TROFF:
			troff(text[p->o_nt1].t_val);
			break;
		case BOX:
		case BLOCK:
			x0 = ox - x1 / 2;
			y0 = oy - y1 / 2;
			x1 = ox + x1 / 2;
			y1 = oy + y1 / 2;
			if (fill) {
				move(x0, y0);
				fillstart(p->o_fillval);
			}
			if (p->o_type == BLOCK)
				;	/* nothing at all */
			else if (invis && !fill)
				;	/* nothing at all */
			else if (p->o_attr & (DOTBIT|DASHBIT))
				dotbox(x0, y0, x1, y1, p->o_attr, p->o_ddval);
			else
				box(x0, y0, x1, y1);
			if (fill)
				fillend(vis, fill);
			move(ox, oy);
			dotext(p);	/* if there are any text strings */
			if (ishor(m))
				move(isright(m) ? x1 : x0, oy);	/* right side */
			else
				move(ox, isdown(m) ? y0 : y1);	/* bottom */
			break;
		case BLOCKEND:
			break;
		case CIRCLE:
			if (fill)
				fillstart(p->o_fillval);
			if (vis || fill)
				circle(ox, oy, x1);
			if (fill)
				fillend(vis, fill);
			move(ox, oy);
			dotext(p);
			/* clang may have found a bug here. Parentheses added
			 * to "?:" operator. (CK) */
			if (ishor(m))
				move(ox + (isright(m) ? x1 : -x1), oy);
			else
				move(ox, oy + (isup(m) ? x1 : -x1));
			break;
		case ELLIPSE:
			if (fill)
				fillstart(p->o_fillval);
			if (vis || fill)
				ellipse(ox, oy, x1, y1);
			if (fill)
				fillend(vis, fill);
			move(ox, oy);
			dotext(p);
			/* Parentheses added (CK) */
			if (ishor(m))
				move(ox + (isright(m) ? x1 : -x1), oy);
			else
				move(ox, oy - (isdown(m) ? y1 : -y1));
			break;
		case ARC:
			if (fill) {
				move(ox, oy);
				fillstart(p->o_fillval);
			}
			if (p->o_attr & HEAD1)
				arrow(x1 - (y1 - oy), y1 + (x1 - ox),
				      x1, y1, p->o_val[4], p->o_val[5], p->o_val[5]/p->o_val[6]/2, p->o_nhead);
                        if (invis && !fill)
                                /* probably wrong when it's cw */
                                move(x1, y1);
                        else
				arc(ox, oy, x1, y1, p->o_val[2], p->o_val[3]);
			if (p->o_attr & HEAD2)
				arrow(p->o_val[2] + p->o_val[3] - oy, p->o_val[3] - (p->o_val[2] - ox),
				      p->o_val[2], p->o_val[3], p->o_val[4], p->o_val[5], -p->o_val[5]/p->o_val[6]/2, p->o_nhead);
			if (fill)
				fillend(vis, fill);
			if (p->o_attr & CW_ARC)
				move(x1, y1);	/* because drawn backwards */
			move(ox, oy);
			dotext(p);
			break;
		case LINE:
		case ARROW:
		case SPLINE:
			if (fill) {
				move(ox, oy);
				fillstart(p->o_fillval);
			}
			if (vis && p->o_attr & HEAD1)
				arrow(ox + p->o_val[5], oy + p->o_val[6], ox, oy, p->o_val[2], p->o_val[3], 0.0, p->o_nhead);
                        if (invis && !fill)
                                move(x1, y1);
			else if (p->o_type == SPLINE)
				spline(ox, oy, p->o_val[4], &p->o_val[5], p->o_attr & (DOTBIT|DASHBIT), p->o_ddval);
			else {
				dx = ox;
				dy = oy;
				for (k=0, j=5; k < p->o_val[4]; k++, j += 2) {
					ndx = dx + p->o_val[j];
					ndy = dy + p->o_val[j+1];
					if (p->o_attr & (DOTBIT|DASHBIT))
						dotline(dx, dy, ndx, ndy, p->o_attr, p->o_ddval);
					else
						line(dx, dy, ndx, ndy);
					dx = ndx;
					dy = ndy;
				}
			}
			if (vis && p->o_attr & HEAD2) {
				dx = ox;
				dy = oy;
				for (k = 0, j = 5; k < p->o_val[4] - 1; k++, j += 2) {
					dx += p->o_val[j];
					dy += p->o_val[j+1];
				}
				arrow(dx, dy, x1, y1, p->o_val[2], p->o_val[3], 0.0, p->o_nhead);
			}
			if (fill)
				fillend(vis, fill);
			move((ox + x1)/2, (oy + y1)/2);	/* center */
			dotext(p);
			break;
		case MOVE:
			move(ox, oy);
			break;
		case TEXT:
			move(ox, oy);
                        if (vis)
				dotext(p);
			break;
		}
	}
}

#define DOTLINE \
	do { \
		numdots = sqrt(dx*dx + dy*dy) / prevval + 0.5; \
		if (numdots > 0) \
			for (i = 0; i <= numdots; i++) { \
				a = (double) i / (double) numdots; \
				move(x0 + (a * dx), y0 + (a * dy)); \
				dot(); \
			} \
	} while (0)

void dotline(double x0, double y0, double x1, double y1, int ddtype, double ddval) /* dotted line */
{
	static double prevval = 0.05;	/* 20 per inch by default */
	int i, numdots;
	double a, b = 0, dx, dy;

	if (ddval == 0)
		ddval = prevval;
	prevval = ddval;
	/* don't save dot/dash value */
	dx = x1 - x0;
	dy = y1 - y0;
	if (ddtype & DOTBIT) {
		DOTLINE;
	} else if (ddtype & DASHBIT) {
		double d, dashsize, spacesize;
		printf(".ie n \\{\\\n");
		DOTLINE;
		printf(".\\}\n");
		printf(".el \\{\\\n");
		d = sqrt(dx*dx + dy*dy);
		if (d <= 2 * prevval) {
			line(x0, y0, x1, y1);
			return;
		}
		numdots = d / (2 * prevval) + 1;	/* ceiling */
		dashsize = prevval;
		spacesize = (d - numdots * dashsize) / (numdots - 1);
		for (i = 0; i < numdots-1; i++) {
			a = i * (dashsize + spacesize) / d;
			b = a + dashsize / d;
			line(x0 + (a*dx), y0 + (a*dy), x0 + (b*dx), y0 + (b*dy));
			a = b;
			b = a + spacesize / d;
			move(x0 + (a*dx), y0 + (a*dy));
		}
		line(x0 + (b * dx), y0 + (b * dy), x1, y1);
		printf(".\\}\n");
	}
	prevval = 0.05;
}

void dotbox(double x0, double y0, double x1, double y1, int ddtype, double ddval)	/* dotted or dashed box */
{
	dotline(x0, y0, x1, y0, ddtype, ddval);
	dotline(x1, y0, x1, y1, ddtype, ddval);
	dotline(x1, y1, x0, y1, ddtype, ddval);
	dotline(x0, y1, x0, y0, ddtype, ddval);
}

void dotext(obj *p)	/* print text strings of p in proper vertical spacing */
{
	int i, nhalf;
	void label(char *, int, int);

	nhalf = p->o_nt2 - p->o_nt1 - 1;
	for (i = p->o_nt1; i < p->o_nt2; i++) {
		label(text[i].t_val, text[i].t_type, nhalf);
		nhalf -= 2;
	}
}
