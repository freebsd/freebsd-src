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

/*	Sccsid @(#)blockgen.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <stdlib.h>
#include "pic.h"
#include "y.tab.h"

#define	NBRACK	20	/* depth of [...] */
#define	NBRACE	20	/* depth of {...} */

struct pushstack stack[NBRACK];
int	nstack	= 0;
struct pushstack bracestack[NBRACE];
int	nbstack	= 0;

void blockadj(obj *);

obj *leftthing(int c)	/* called for {... or [... */
			/* really ought to be separate functions */
{
	obj *p;

	if (c == '[') {
		if (nstack >= NBRACK)
			FATAL("[...] nested too deep");
		stack[nstack].p_x = curx;
		stack[nstack].p_y = cury;
		stack[nstack].p_hvmode = hvmode;
		curx = cury = 0;
		stack[nstack].p_xmin = xmin;
		stack[nstack].p_xmax = xmax;
		stack[nstack].p_ymin = ymin;
		stack[nstack].p_ymax = ymax;
		nstack++;
		xmin = ymin = 30000;
		xmax = ymax = -30000;
		p = makenode(BLOCK, 7);
		p->o_val[4] = nobj;	/* 1st item within [...] */
		if (p->o_nobj != nobj-1)
			fprintf(stderr, "nobjs wrong%d %d\n", p->o_nobj, nobj);
	} else {
		if (nbstack >= NBRACK)
			FATAL("{...} nested too deep");
		bracestack[nbstack].p_x = curx;
		bracestack[nbstack].p_y = cury;
		bracestack[nbstack].p_hvmode = hvmode;
		nbstack++;
		p = NULL;
	}
	return(p);
}

obj *rightthing(obj *p, int c)	/* called for ... ] or ... } */
{
	obj *q;

	if (c == '}') {
		nbstack--;
		curx = bracestack[nbstack].p_x;
		cury = bracestack[nbstack].p_y;
		hvmode = bracestack[nbstack].p_hvmode;
		q = makenode(MOVE, 0);
		dprintf("M %g %g\n", curx, cury);
	} else {
		nstack--;
		curx = stack[nstack].p_x;
		cury = stack[nstack].p_y;
		hvmode = stack[nstack].p_hvmode;
		q = makenode(BLOCKEND, 7);
		q->o_val[4] = p->o_nobj + 1;	/* back pointer */
		p->o_val[5] = q->o_nobj - 1;	/* forward pointer */
		if (xmin > xmax)	/* nothing happened */
			xmin = xmax;
		if (ymin > ymax)
			ymin = ymax;
		p->o_val[0] = xmin; p->o_val[1] = ymin;
		p->o_val[2] = xmax; p->o_val[3] = ymax;
		p->o_symtab = q->o_symtab = stack[nstack+1].p_symtab;
		xmin = stack[nstack].p_xmin;
		ymin = stack[nstack].p_ymin;
		xmax = stack[nstack].p_xmax;
		ymax = stack[nstack].p_ymax;
	}
	return(q);
}

obj *blockgen(obj *p, obj *q)	/* handles [...] */
{
	int i, invis, at, with;
	double ddval, h, w, xwith, ywith;
	double x0, y0, x1, y1, cx, cy;
	obj *ppos;
	Attr *ap;

	invis = at = 0;
	with = xwith = ywith = 0;
	ddval = 0;
	w = p->o_val[2] - p->o_val[0];
	h = p->o_val[3] - p->o_val[1];
	cx = (p->o_val[2] + p->o_val[0]) / 2;	/* geom ctr of [] wrt local orogin */
	cy = (p->o_val[3] + p->o_val[1]) / 2;
	dprintf("cx,cy=%g,%g\n", cx, cy);
	for (i = 0; i < nattr; i++) {
		ap = &attr[i];
		switch (ap->a_type) {
		case HEIGHT:
			h = ap->a_val.f;
			break;
		case WIDTH:
			w = ap->a_val.f;
			break;
		case WITH:
			with = ap->a_val.i;	/* corner */
			break;
		case PLACE:	/* actually with position ... */
			ppos = ap->a_val.o;
			xwith = cx - ppos->o_x;
			ywith = cy - ppos->o_y;
			with = PLACE;
			break;
		case AT:
		case FROM:
			ppos = ap->a_val.o;
			curx = ppos->o_x;
			cury = ppos->o_y;
			at++;
			break;
		case INVIS:
			invis = INVIS;
			break;
		case TEXTATTR:
			savetext(ap->a_sub, ap->a_val.p);
			break;
		}
	}
	if (with) {
		switch (with) {
		case NORTH:	ywith = -h / 2; break;
		case SOUTH:	ywith = h / 2; break;
		case EAST:	xwith = -w / 2; break;
		case WEST:	xwith = w / 2; break;
		case NE:	xwith = -w / 2; ywith = -h / 2; break;
		case SE:	xwith = -w / 2; ywith = h / 2; break;
		case NW:	xwith = w / 2; ywith = -h / 2; break;
		case SW:	xwith = w / 2; ywith = h / 2; break;
		}
		curx += xwith;
		cury += ywith;
	}
	if (!at) {
		if (isright(hvmode))
			curx += w / 2;
		else if (isleft(hvmode))
			curx -= w / 2;
		else if (isup(hvmode))
			cury += h / 2;
		else
			cury -= h / 2;
	}
	x0 = curx - w / 2;
	y0 = cury - h / 2;
	x1 = curx + w / 2;
	y1 = cury + h / 2;
	extreme(x0, y0);
	extreme(x1, y1);
	p->o_x = curx;
	p->o_y = cury;
	p->o_nt1 = ntext1;
	p->o_nt2 = ntext;
	ntext1 = ntext;
	p->o_val[0] = w;
	p->o_val[1] = h;
	p->o_val[2] = cx;
	p->o_val[3] = cy;
	p->o_val[5] = q->o_nobj - 1;		/* last item in [...] */
	p->o_ddval = ddval;
	p->o_attr = invis;
	dprintf("[] %g %g %g %g at %g %g, h=%g, w=%g\n", x0, y0, x1, y1, curx, cury, h, w);
	if (isright(hvmode))
		curx = x1;
	else if (isleft(hvmode))
		curx = x0;
	else if (isup(hvmode))
		cury = y1;
	else
		cury = y0;
	for (i = 0; i <= 5; i++)
		q->o_val[i] = p->o_val[i];
	stack[nstack+1].p_symtab = NULL;	/* so won't be found again */
	blockadj(p);	/* fix up coords for enclosed blocks */
	return(p);
}

void blockadj(obj *p)	/* adjust coords in block starting at p */
{
	double dx, dy;
	int n, lev;

	dx = p->o_x - p->o_val[2];
	dy = p->o_y - p->o_val[3];
	n = p->o_nobj + 1;
	dprintf("into blockadj: dx,dy=%g,%g\n", dx, dy);
	for (lev = 1; lev > 0; n++) {
		p = objlist[n];
		if (p->o_type == BLOCK)
			lev++;
		else if (p->o_type == BLOCKEND)
			lev--;
		dprintf("blockadj: type=%d o_x,y=%g,%g;", p->o_type, p->o_x, p->o_y);
		p->o_x += dx;
		p->o_y += dy;
		dprintf(" becomes %g,%g\n", p->o_x, p->o_y);
		switch (p->o_type) {	/* other absolute coords */
		case LINE:
		case ARROW:
		case SPLINE:
			p->o_val[0] += dx;
			p->o_val[1] += dy;
			break;
		case ARC:
			p->o_val[0] += dx;
			p->o_val[1] += dy;
			p->o_val[2] += dx;
			p->o_val[3] += dy;
			break;
		}
	}
}
