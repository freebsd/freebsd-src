/*
 * Changes by Gunnar Ritter, Freiburg i. Br., Germany, October 2005.
 *
 * Derived from Plan 9 v4 /sys/src/cmd/grap/
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
#include "grap.h"
#include "y.tab.h"

int	nnum	= 0;	/* number of saved numbers */
double	num[MAXNUM];

int	just;		/* current justification mode (RJUST, etc.) */
int	sizeop;		/* current optional operator for size change */
double	sizexpr;	/* current size change expression */

void savenum(int n, double f)	/* save f in num[n] */
{
	num[n] = f;
	nnum = n+1;
	if (nnum >= MAXNUM)
		WARNING("too many numbers");
}

void setjust(int j)
{
	just |= j;
}

void setsize(int op, double expr)
{
	sizeop = op;
	sizexpr = expr;
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

void range(Point pt)	/* update the range for point pt */
{
	Obj *p = pt.obj;

	if (!(p->coord & XFLAG)) {
		if (pt.x > p->pt1.x)
			p->pt1.x = pt.x;
		if (pt.x < p->pt.x)
			p->pt.x = pt.x;
	}
	if (!(p->coord & YFLAG)) {
		if (pt.y > p->pt1.y)
			p->pt1.y = pt.y;
		if (pt.y < p->pt.y)
			p->pt.y = pt.y;
	}
}

void halfrange(Obj *p, int side, double val)	/* record max and min for one direction */
{
	if (!(p->coord&XFLAG) && (side == LEFT || side == RIGHT)) {
		if (val < p->pt.y)
			p->pt.y = val;
		if (val > p->pt1.y)
			p->pt1.y = val;
	} else if (!(p->coord&YFLAG) && (side == TOP || side == BOT)) {
		if (val < p->pt.x)
			p->pt.x = val;
		if (val > p->pt1.x)
			p->pt1.x = val;
	}
}


Obj *lookup(char *s, int inst)	/* find s in objlist, install if inst */
{
	Obj *p;
	int found = 0;

	for (p = objlist; p; p = p->next){
		if (strcmp(s, p->name) == 0) {
			found = 1;
			break;
		}
	}
	if (p == NULL && inst != 0) {
		p = (Obj *) calloc(1, sizeof(Obj));
		if (p == NULL)
			FATAL("out of space in lookup");
		p->name = tostring(s);
		p->type = NAME;
		p->pt = ptmax;
		p->pt1 = ptmin;
		p->fval = 0.0;
		p->next = objlist;
		objlist = p;
	}
	dprintf("lookup(%s,%d) = %d\n", s, inst, found);
	return p;
}

double getvar(Obj *p)	/* return value of variable */
{
	return p->fval;
}

double setvar(Obj *p, double f)	/* set value of variable to f */
{
	if (strcmp(p->name, "pointsize") == 0) {	/* kludge */
		pointsize = f;
		ps_set = 1;
	}
	p->type = VARNAME;
	return p->fval = f;
}

Point makepoint(Obj *s, double x, double y)	/* make a Point */
{
	Point p;
	
	dprintf("makepoint: %s, %g,%g\n", s->name, x, y);
	p.obj = s;
	p.x = x;
	p.y = y;
	return p;
}

Attr *makefattr(int type, double fval)	/* set double in attribute */
{
	return makeattr(type, fval, (char *) 0, 0, 0);
}

Attr *makesattr(char *s)		/* make an Attr cell containing s */
{
	Attr *ap = makeattr(STRING, sizexpr, s, just, sizeop);
	just = sizeop = 0;
	sizexpr = 0.0;
	return ap;
}

Attr *makeattr(int type, double fval, char *sval, int just, int op)
{
	Attr *a;

	a = (Attr *) malloc(sizeof(Attr));
	if (a == NULL)
		FATAL("out of space in makeattr");
	a->type = type;
	a->fval = fval;
	a->sval = sval;
	a->just = just;
	a->op = op;
	a->next = NULL;
	return a;
}

Attr *addattr(Attr *a1, Attr *ap)	/* add attr ap to end of list a1 */
{
	Attr *p;

	if (a1 == 0)
		return ap;
	if (ap == 0)
		return a1;
	for (p = a1; p->next; p = p->next)
		;
	p->next = ap;
	return a1;
}

void freeattr(Attr *ap)	/* free an attribute list */
{
	Attr *p;

	while (ap) {
		p = ap->next;	/* save next */
		if (ap->sval)
			free(ap->sval);
		free((char *) ap);
		ap = p;
	}
}

char *slprint(Attr *stringlist)	/* print strings from stringlist */
{
	int ntext, n, last_op, last_just;
	double last_fval;
	static char buf[1000];
	Attr *ap;

	buf[0] = '\0';
	last_op = last_just = 0;
	last_fval = 0.0;
	for (ntext = 0, ap = stringlist; ap != NULL; ap = ap->next)
		ntext++;
	snprintf(buf, sizeof(buf), "box invis wid 0 ht %d*textht", ntext);
	n = strlen(buf);
	for (ap = stringlist; ap != NULL; ap = ap->next) {
		if (ap->op == 0) {	/* propagate last value */
			ap->op = last_op;
			ap->fval = last_fval;
		} else {
			last_op = ap->op;
			last_fval = ap->fval;
		}
		snprintf(buf+n, sizeof(buf) - n, " \"%s\"",
		    ps_set || ap->op ? sizeit(ap) : ap->sval);
		if (ap->just)
			last_just = ap->just;
		if (last_just)
			n_strcat(buf+n, juststr(last_just), sizeof(buf) - n);
		n = strlen(buf);
	}
	return buf;	/* watch it:  static */
}

char *juststr(int j)	/* convert RJUST, etc., into string */
{
	static char buf[50];

	buf[0] = '\0';
	if (j & RJUST)
		n_strcat(buf, " rjust", sizeof(buf));
	if (j & LJUST)
		n_strcat(buf, " ljust", sizeof(buf));
	if (j & ABOVE)
		n_strcat(buf, " above", sizeof(buf));
	if (j & BELOW)
		n_strcat(buf, " below", sizeof(buf));
	return buf;	/* watch it:  static */
}

char *sprntf(char *s, Attr *ap)	/* sprintf(s, attrlist ap) */
{
	char buf[500];
	int n;
	Attr *p;

	for (n = 0, p = ap; p; p = p->next)
		n++;
	switch (n) {
	case 0:
		return s;
	case 1:
		snprintf(buf, sizeof(buf), s, ap->fval);
		break;
	case 2:
		snprintf(buf, sizeof(buf), s, ap->fval, ap->next->fval);
		break;
	case 3:
		snprintf(buf, sizeof(buf), s, ap->fval, ap->next->fval,
		    ap->next->next->fval);
		break;
	case 5:
		WARNING("too many expressions in sprintf");
	case 4:
		snprintf(buf, sizeof(buf), s, ap->fval, ap->next->fval,
		    ap->next->next->fval, ap->next->next->next->fval);
		break;
	}
	free(s);
	return tostring(buf);
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
