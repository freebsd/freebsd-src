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

/*	Sccsid @(#)misc.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "pic.h"
#include "y.tab.h"

int whatpos(obj *p, int corner, double *px, double *py);
void makeattr(int type, int sub, YYSTYPE val);
YYSTYPE getblk(obj *, char *);

int setdir(int n)	/* set direction (hvmode) from LEFT, RIGHT, etc. */
{
	switch (n) {
	case UP:	hvmode = U_DIR; break;
	case DOWN:	hvmode = D_DIR; break;
	case LEFT:	hvmode = L_DIR; break;
	case RIGHT:	hvmode = R_DIR; break;
	}
 	return(hvmode);
}

int curdir(void)	/* convert current dir (hvmode) to RIGHT, LEFT, etc. */
{
	switch (hvmode) {
	case R_DIR:	return RIGHT;
	case L_DIR:	return LEFT;
	case U_DIR:	return UP;
	case D_DIR:	return DOWN;
	}
	FATAL("can't happen curdir");
	return 0;
}

double getcomp(obj *p, int t)	/* return component of a position */
{
	switch (t) {
	case DOTX:
		return p->o_x;
	case DOTY:
		return p->o_y;
	case DOTWID:
		switch (p->o_type) {
		case BOX:
		case BLOCK:
		case TEXT:
			return p->o_val[0];
		case CIRCLE:
		case ELLIPSE:
			return 2 * p->o_val[0];
		case LINE:
		case ARROW:
			return p->o_val[0] - p->o_x;
		case PLACE:
			return 0;
		}
	case DOTHT:
		switch (p->o_type) {
		case BOX:
		case BLOCK:
		case TEXT:
			return p->o_val[1];
		case CIRCLE:
		case ELLIPSE:
			return 2 * p->o_val[1];
		case LINE:
		case ARROW:
			return p->o_val[1] - p->o_y;
		case PLACE:
			return 0;
		}
	case DOTRAD:
		switch (p->o_type) {
		case CIRCLE:
		case ELLIPSE:
			return p->o_val[0];
		}
	}
	WARNING("you asked for a weird dimension or position");
	return 0;
}

double	exprlist[100];
int	nexpr	= 0;

void exprsave(double f)
{
	exprlist[nexpr++] = f;
}

char *sprintgen(char *fmt)
{
	char buf[1000];

	snprintf(buf, sizeof(buf), fmt, exprlist[0], exprlist[1], exprlist[2],
	    exprlist[3], exprlist[4]);
	nexpr = 0;
	free(fmt);
	return tostring(buf);
}

void makefattr(int type, int sub, double f)	/* double attr */
{
	YYSTYPE val;
	val.f = f;
	makeattr(type, sub, val);
}

void makeoattr(int type, obj *o)	/* obj* attr */
{
	YYSTYPE val;
	val.o = o;
	makeattr(type, 0, val);
}

void makeiattr(int type, int i)	/* int attr */
{
	YYSTYPE val;
	val.i = i;
	makeattr(type, 0, val);
}

void maketattr(int sub, char *p)	/* text attribute: takes two */
{
	YYSTYPE val;
	val.p = p;
	makeattr(TEXTATTR, sub, val);
}

void addtattr(int sub)		/* add text attrib to existing item */
{
	attr[nattr-1].a_sub |= sub;
}

void makevattr(char *p)	/* varname attribute */
{
	YYSTYPE val;
	val.p = p;
	makeattr(VARNAME, 0, val);
}

void makeattr(int type, int sub, YYSTYPE val)	/* add attribute type and val */
{
	if (type == 0 && val.i == 0) {	/* clear table for next stat */
		nattr = 0;
		return;
	}
	if (nattr >= nattrlist)
		attr = (Attr *) grow((char *)attr, "attr", nattrlist += 100, sizeof(Attr));
	dprintf("attr %d:  %d %d %d\n", nattr, type, sub, val.i);
	attr[nattr].a_type = type;
	attr[nattr].a_sub = sub;
	attr[nattr].a_val = val;
	nattr++;
}

void printexpr(double f)	/* print expression for debugging */
{
	printf("%g\n", f);
}

void printpos(obj *p)	/* print position for debugging */
{
	printf("%g, %g\n", p->o_x, p->o_y);
}

char *tostring(char *s)
{
	register char *p;
	size_t l;

	l = strlen(s)+1;
	p = malloc(l);
	if (p == NULL)
		FATAL("out of space in tostring on %s", s);
	n_strcpy(p, s, l);
	return(p);
}

obj *makepos(double x, double y)	/* make a position cell */
{
	obj *p;

	p = makenode(PLACE, 0);
	p->o_x = x;
	p->o_y = y;
	return(p);
}

obj *makebetween(double f, obj *p1, obj *p2)	/* make position between p1 and p2 */
{
	obj *p;

	dprintf("fraction = %.2f\n", f);
	p = makenode(PLACE, 0);
	p->o_x = p1->o_x + f * (p2->o_x - p1->o_x);
	p->o_y = p1->o_y + f * (p2->o_y - p1->o_y);
	return(p);
}

obj *getpos(obj *p, int corner)	/* find position of point */
{
	double x, y;

	whatpos(p, corner, &x, &y);
	return makepos(x, y);
}

int whatpos(obj *p, int corner, double *px, double *py)	/* what is the position (no side effect) */
{
	double x, y, x1 = 0, y1 = 0;

	dprintf("whatpos %lo %d %d\n", (long)p, p->o_type, corner);
	x = p->o_x;
	y = p->o_y;
	if (p->o_type != PLACE && p->o_type != MOVE) {
		x1 = p->o_val[0];
		y1 = p->o_val[1];
	}
	switch (p->o_type) {
	case PLACE:
		break;
	case BOX:
	case BLOCK:
	case TEXT:
		switch (corner) {
		case NORTH:	y += y1 / 2; break;
		case SOUTH:	y -= y1 / 2; break;
		case EAST:	x += x1 / 2; break;
		case WEST:	x -= x1 / 2; break;
		case NE:	x += x1 / 2; y += y1 / 2; break;
		case SW:	x -= x1 / 2; y -= y1 / 2; break;
		case SE:	x += x1 / 2; y -= y1 / 2; break;
		case NW:	x -= x1 / 2; y += y1 / 2; break;
		case START:
			if (p->o_type == BLOCK)
				return whatpos(objlist[(int)p->o_val[2]], START, px, py);
		case END:
			if (p->o_type == BLOCK)
				return whatpos(objlist[(int)p->o_val[3]], END, px, py);
		}
		break;
	case ARC:
		switch (corner) {
		case START:
			if (p->o_attr & CW_ARC) {
				x = p->o_val[2]; y = p->o_val[3];
			} else {
				x = x1; y = y1;
			}
			break;
		case END:
			if (p->o_attr & CW_ARC) {
				x = x1; y = y1;
			} else {
				x = p->o_val[2]; y = p->o_val[3];
			}
			break;
		}
		if (corner == START || corner == END)
			break;
		x1 = y1 = sqrt((x1-x)*(x1-x) + (y1-y)*(y1-y));
		/* Fall Through! */
	case CIRCLE:
	case ELLIPSE:
		switch (corner) {
		case NORTH:	y += y1; break;
		case SOUTH:	y -= y1; break;
		case EAST:	x += x1; break;
		case WEST:	x -= x1; break;
		case NE:	x += 0.707 * x1; y += 0.707 * y1; break;
		case SE:	x += 0.707 * x1; y -= 0.707 * y1; break;
		case NW:	x -= 0.707 * x1; y += 0.707 * y1; break;
		case SW:	x -= 0.707 * x1; y -= 0.707 * y1; break;
		}
		break;
	case LINE:
	case SPLINE:
	case ARROW:
		switch (corner) {
		case START:	break;	/* already in place */
		case END:	x = x1; y = y1; break;
		default: /* change! */
		case CENTER:	x = (x+x1)/2; y = (y+y1)/2; break;
		case NORTH:	if (y1 > y) { x = x1; y = y1; } break;
		case SOUTH:	if (y1 < y) { x = x1; y = y1; } break;
		case EAST:	if (x1 > x) { x = x1; y = y1; } break;
		case WEST:	if (x1 < x) { x = x1; y = y1; } break;
		}
		break;
	case MOVE:
		/* really ought to be same as line... */
		break;
	}
	dprintf("whatpos returns %g %g\n", x, y);
	*px = x;
	*py = y;
	return 1;
}

obj *gethere(void)	/* make a place for curx,cury */
{
	dprintf("gethere %g %g\n", curx, cury);
	return(makepos(curx, cury));
}

obj *getlast(int n, int t)	/* find n-th previous occurrence of type t */
{
	int i, k;
	obj *p;

	k = n;
	for (i = nobj-1; i >= 0; i--) {
		p = objlist[i];
		if (p->o_type == BLOCKEND) {
			i = p->o_val[4];
			continue;
		}
		if (p->o_type != t)
			continue;
		if (--k > 0)
			continue;	/* not there yet */
		dprintf("got a last of x,y= %g,%g\n", p->o_x, p->o_y);
		return(p);
	}
	FATAL("there is no %dth last", n);
	return(NULL);
}

obj *getfirst(int n, int t)	/* find n-th occurrence of type t */
{
	int i, k;
	obj *p;

	k = n;
	for (i = 0; i < nobj; i++) {
		p = objlist[i];
		if (p->o_type == BLOCK && t != BLOCK) {	/* skip whole block */
			i = p->o_val[5] + 1;
			continue;
		}
		if (p->o_type != t)
			continue;
		if (--k > 0)
			continue;	/* not there yet */
		dprintf("got a first of x,y= %g,%g\n", p->o_x, p->o_y);
		return(p);
	}
	FATAL("there is no %dth ", n);
	return(NULL);
}

double getblkvar(obj *p, char *s)	/* find variable s2 in block p */
{
	YYSTYPE y;

	y = getblk(p, s);
	return y.f;
}

obj *getblock(obj *p, char *s)	/* find variable s in block p */
{
	YYSTYPE y;

	y = getblk(p, s);
	return y.o;
}

YYSTYPE getblk(obj *p, char *s)	/* find union type for s in p */
{
	static YYSTYPE bug;
	struct symtab *stp;

	if (p->o_type != BLOCK) {
		WARNING(".%s is not in that block", s);
		return(bug);
	}
	for (stp = p->o_symtab; stp != NULL; stp = stp->s_next)
		if (strcmp(s, stp->s_name) == 0) {
			dprintf("getblk %s found x,y= %g,%g\n",
				s, (stp->s_val.o)->o_x, (stp->s_val.o)->o_y);
			return(stp->s_val);
		}
	WARNING("there is no .%s in that []", s);
	return(bug);
}

obj *fixpos(obj *p, double x, double y)
{
	dprintf("fixpos returns %g %g\n", p->o_x + x, p->o_y + y);
	return makepos(p->o_x + x, p->o_y + y);
}

obj *addpos(obj *p, obj *q)
{
	dprintf("addpos returns %g %g\n", p->o_x+q->o_x, p->o_y+q->o_y);
	return makepos(p->o_x+q->o_x, p->o_y+q->o_y);
}

obj *subpos(obj *p, obj *q)
{
	dprintf("subpos returns %g %g\n", p->o_x-q->o_x, p->o_y-q->o_y);
	return makepos(p->o_x-q->o_x, p->o_y-q->o_y);
}

obj *makenode(int type, int n)
{
	obj *p;

	p = (obj *) calloc(1, sizeof(obj) + (n-1)*sizeof(ofloat));
	if (p == NULL)
		FATAL("out of space in makenode");
	p->o_type = type;
	p->o_count = n;
	p->o_nobj = nobj;
	p->o_mode = hvmode;
	p->o_x = curx;
	p->o_y = cury;
	p->o_nt1 = ntext1;
	p->o_nt2 = ntext;
	ntext1 = ntext;	/* ready for next caller */
	if (nobj >= nobjlist)
		objlist = (obj **) grow((char *) objlist, "objlist",
			nobjlist *= 2, sizeof(obj *));
	objlist[nobj++] = p;
	return(p);
}

void extreme(double x, double y)	/* record max and min x and y values */
{
	if (x > xmax)
		xmax = x;
	if (y > ymax)
		ymax = y;
	if (x < xmin)
		xmin = x;
	if (y < ymin)
		ymin = y;
}

static void
verror(const char *fmt, va_list ap)
{
	char	errbuf[4096];

	vsnprintf(errbuf, sizeof errbuf, fmt, ap);
	yyerror(errbuf);
}

void FATAL(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
	exit(1);
}

void WARNING(const char *fmt, ...)
{
	va_list	ap;
	va_start(ap, fmt);
	verror(fmt, ap);
	va_end(ap);
}
