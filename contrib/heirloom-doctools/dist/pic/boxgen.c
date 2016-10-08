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

/*	Sccsid @(#)boxgen.c	1.2 (gritter) 10/18/05	*/
#include	<stdio.h>
#include	"pic.h"
#include	"y.tab.h"

obj *boxgen(void)
{
	static double prevh = HT;
	static double prevw = WID;	/* golden mean, sort of */
	int i, at, battr, with;
	double ddval, fillval, xwith, ywith;
	double h, w, x0, y0, x1, y1;
	obj *p, *ppos;
	Attr *ap;

	h = getfval("boxht");
	w = getfval("boxwid");
	at = battr = with = 0;
	ddval = fillval = xwith = ywith = 0;
	for (i = 0; i < nattr; i++) {
		ap = &attr[i];
		switch (ap->a_type) {
		case HEIGHT:
			h = ap->a_val.f;
			break;
		case WIDTH:
			w = ap->a_val.f;
			break;
		case SAME:
			h = prevh;
			w = prevw;
			break;
		case WITH:
			with = ap->a_val.i;	/* corner */
			break;
		case AT:
			ppos = ap->a_val.o;
			curx = ppos->o_x;
			cury = ppos->o_y;
			at++;
			break;
		case INVIS:
			battr |= INVIS;
			break;
		case NOEDGE:
			battr |= NOEDGEBIT;
			break;
		case DOT:
		case DASH:
			battr |= ap->a_type==DOT ? DOTBIT : DASHBIT;
			if (ap->a_sub == DEFAULT)
				ddval = getfval("dashwid");
			else
				ddval = ap->a_val.f;
			break;
		case FILL:
			battr |= FILLBIT;
			if (ap->a_sub == DEFAULT)
				fillval = getfval("fillval");
			else
				fillval = ap->a_val.f;
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
	p = makenode(BOX, 2);
	p->o_val[0] = w;
	p->o_val[1] = h;
	p->o_attr = battr;
	p->o_ddval = ddval;
	p->o_fillval = fillval;
	dprintf("B %g %g %g %g at %g %g, h=%g, w=%g\n", x0, y0, x1, y1, curx, cury, h, w);
	if (isright(hvmode))
		curx = x1;
	else if (isleft(hvmode))
		curx = x0;
	else if (isup(hvmode))
		cury = y1;
	else
		cury = y0;
	prevh = h;
	prevw = w;
	return(p);
}
