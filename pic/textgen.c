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

/*	Sccsid @(#)textgen.c	1.2 (gritter) 10/18/05	*/
#include	<stdio.h>
#include	"pic.h"
#include	"y.tab.h"

obj *textgen(void)
{
	int i, sub, nstr, at, with, hset, invis;
	double xwith, ywith, h, w, x0, y0, x1, y1;
	obj *p, *ppos;
	Attr *ap;

	at = with = nstr = hset = invis = 0;
	h = getfval("textht");
	w = getfval("textwid");
	for (i = 0; i < nattr; i++) {
		ap = &attr[i];
		switch (ap->a_type) {
		case HEIGHT:
			h = ap->a_val.f;
			hset++;
			break;
		case WIDTH:
			w = ap->a_val.f;
			break;
		case WITH:
			with = ap->a_val.i;
			break;
		case INVIS:
			invis = INVIS;
			break;
		case AT:
			ppos = ap->a_val.o;
			curx = ppos->o_x;
			cury = ppos->o_y;
			at++;
			break;
		case TEXTATTR:
			sub = ap->a_sub;
			if (ap->a_val.p == NULL)	/* an isolated modifier */
				text[ntext-1].t_type = sub;
			else {
				savetext(sub, ap->a_val.p);
				nstr++;
			}
			break;
		}
	}
	if (hset == 0)		/* no explicit ht cmd */
		h *= nstr;
	if (with) {
		xwith = ywith = 0.0;
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
	dprintf("Text h %g w %g at %g,%g\n", h, w, curx, cury);
	p = makenode(TEXT, 2);
	p->o_attr = invis;
	p->o_val[0] = w;
	p->o_val[1] = h;
	if (isright(hvmode))
		curx = x1;
	else if (isleft(hvmode))
		curx = x0;
	else if (isup(hvmode))
		cury = y1;
	else
		cury = y0;
	return(p);
}

obj *troffgen(char *s)	/* save away a string of troff commands */
{
	savetext(CENTER, s);	/* use the existing text mechanism */
	return makenode(TROFF, 0);
}

void savetext(int t, char *s)	/* record text elements for current object */
{
	if (ntext >= ntextlist)
		text = (Text *) grow((char *) text, "text", ntextlist += 200, sizeof(Text));
	text[ntext].t_type = t;
	text[ntext].t_val = s;
	dprintf("saving %d text %s at %d\n", t, s, ntext);
	ntext++;
}
