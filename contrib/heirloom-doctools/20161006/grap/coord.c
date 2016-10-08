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

/*	Sccsid @(#)coord.c	1.3 (gritter) 10/18/05	*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "grap.h"
#include "y.tab.h"

char	*dflt_coord = "gg";
char	*curr_coord = "gg";
int	ncoord	= 0;	/* number of explicit coord's given */

Point	xcoord;
Point	ycoord;
int	xcflag	= 0;	/* 1 if xcoord set */
int	ycflag	= 0;
int	logcoord = 0;

void coord_x(Point pt)	/* remember x coord */
{
	xcoord = pt;
	xcflag = 1;
	margin = 0;	/* no extra space around picture if explicit coords */
}

void coord_y(Point pt)
{
	ycoord = pt;
	ycflag = 1;
	margin = 0;	/* no extra space if explicit coords */
}

void coordlog(int n)	/* remember log scaling */
{
	logcoord = n;
}

void coord(Obj *p)	/* set coord range */
{
	static char buf[10];

	ncoord++;
	if (ncoord > 1 && strcmp(p->name, dflt_coord) == 0) {
		/* resetting default coordinate by implication */
		snprintf(buf, sizeof(buf), "gg%d", ncoord);
		dflt_coord = buf;
		p = lookup(dflt_coord, 1);
	}
	if (xcflag) {
		p->coord |= XFLAG;
		p->pt.x = min(xcoord.x,xcoord.y);	/* "xcoord" is xmin, xmax */
		p->pt1.x = max(xcoord.x,xcoord.y);
		if ((logcoord&XFLAG) && p->pt.x <= 0.0)
			FATAL("can't have log of x coord %g,%g", p->pt.x, p->pt1.x);
		xcflag = 0;
	}
	if (ycflag) {
		p->coord |= YFLAG;
		p->pt.y = min(ycoord.x,ycoord.y);	/* "ycoord" is ymin, ymax */
		p->pt1.y = max(ycoord.x,ycoord.y);
		if ((logcoord&YFLAG) && p->pt.y <= 0.0)
			FATAL("can't have log of y coord %g,%g", p->pt.y, p->pt1.y);
		ycflag = 0;
	}
	p->log = logcoord;
	logcoord = 0;
	auto_x = 0;
}

void resetcoord(Obj *p)	/* reset current coordinate */
{
	curr_coord = p->name;
}
