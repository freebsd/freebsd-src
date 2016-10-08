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

/*	Sccsid @(#)movegen.c	1.2 (gritter) 10/18/05	*/
#include	<stdio.h>
#include	"pic.h"
#include	"y.tab.h"

obj *movegen(void)
{
	static double prevdx, prevdy;
	int i, some;
	double defx, defy, dx, dy;
	obj *p;
	obj *ppos;
	static int xtab[] = { 1, 0, -1, 0 };	/* R=0, U=1, L=2, D=3 */
	static int ytab[] = { 0, 1, 0, -1 };
	Attr *ap;

	defx = getfval("movewid");
	defy = getfval("moveht");
	dx = dy = some = 0;
	for (i = 0; i < nattr; i++) {
		ap = &attr[i];
		switch (ap->a_type) {
		case TEXTATTR:
			savetext(ap->a_sub, ap->a_val.p);
			break;
		case SAME:
			dx = prevdx;
			dy = prevdy;
			some++;
			break;
		case LEFT:
			dx -= (ap->a_sub==DEFAULT) ? defx : ap->a_val.f;
			some++;
			hvmode = L_DIR;
			break;
		case RIGHT:
			dx += (ap->a_sub==DEFAULT) ? defx : ap->a_val.f;
			some++;
			hvmode = R_DIR;
			break;
		case UP:
			dy += (ap->a_sub==DEFAULT) ? defy : ap->a_val.f;
			some++;
			hvmode = U_DIR;
			break;
		case DOWN:
			dy -= (ap->a_sub==DEFAULT) ? defy : ap->a_val.f;
			some++;
			hvmode = D_DIR;
			break;
		case TO:
			ppos = ap->a_val.o;
			dx = ppos->o_x - curx;
			dy = ppos->o_y - cury;
			some++;
			break;
		case BY:
			ppos = ap->a_val.o;
			dx = ppos->o_x;
			dy = ppos->o_y;
			some++;
			break;
		case FROM:
		case AT:
			ppos = ap->a_val.o;
			curx = ppos->o_x;
			cury = ppos->o_y;
			break;
		}
	}
	if (some) {
		defx = dx;
		defy = dy;
	} else {
		defx *= xtab[hvmode];
		defy *= ytab[hvmode];
	}
	prevdx = defx;
	prevdy = defy;
	extreme(curx, cury);
	curx += defx;
	cury += defy;
	extreme(curx, cury);
	p = makenode(MOVE, 0);
	dprintf("M %g %g\n", curx, cury);
	return(p);
}
