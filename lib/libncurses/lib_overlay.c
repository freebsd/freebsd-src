
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_overlay.c
**
**	The routines overlay(), copywin(), and overwrite().
**
*/

#include "curses.priv.h"

static void overlap(WINDOW *s, WINDOW *d, int flag)
{ 
int sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol;

	T(("overlap : sby %d, sbx %d, smy %d, smx %d, dby %d, dbx %d, dmy %d, dmx %d",
		s->_begy, s->_begx, s->_maxy, s->_maxx, 
		d->_begy, d->_begx, d->_maxy, d->_maxx));
	sminrow = max(s->_begy, d->_begy) - s->_begy;
	smincol = max(s->_begx, d->_begx) - s->_begx;
	dminrow = max(s->_begy, d->_begy) - d->_begy;
	dmincol = max(s->_begx, d->_begx) - d->_begx;
	dmaxrow = min(s->_maxy+s->_begy, d->_maxy+d->_begy) - d->_begy;
	dmaxcol = min(s->_maxx+s->_begx, d->_maxx+d->_begx) - d->_begx;

	copywin(s, d, sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol, flag);
}

/*
**
**	overlay(win1, win2)
**
**
**	overlay() writes the overlapping area of win1 behind win2
**	on win2 non-destructively.
**
**/

int overlay(WINDOW *win1, WINDOW *win2)
{
	overlap(win1, win2, TRUE);
	return OK;
}

/*
**
**	overwrite(win1, win2)
**
**
**	overwrite() writes the overlapping area of win1 behind win2
**	on win2 destructively.
**
**/

int overwrite(WINDOW *win1, WINDOW *win2)
{
	overlap(win1, win2, FALSE);
	return OK;
}

int copywin(WINDOW *src, WINDOW *dst, 
	int sminrow, int smincol,
	int dminrow, int dmincol, int dmaxrow, int dmaxcol, 
	int over)
{
int sx, sy, dx, dy;

	T(("copywin(%x, %x, %d, %d, %d, %d, %d, %d, %d)",
	    	src, dst, sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol, over));
	
	/* make sure rectangle exists in source */
	if ((sminrow + dmaxrow - dminrow) > (src->_maxy + 1) ||
	    (smincol + dmaxcol - dmincol) > (src->_maxx + 1)) {
		return ERR;
	}

	T(("rectangle exists in source"));

	/* make sure rectangle fits in destination */
	if (dmaxrow > dst->_maxy || dmaxcol > dst->_maxx) {
		return ERR;
	}

	T(("rectangle fits in destination"));

	for (dy = dminrow, sy = sminrow; dy <= dmaxrow; sy++, dy++) {
		dst->_firstchar[dy] = dmincol;
		dst->_lastchar[dy] = dmincol;
		for (dx = dmincol, sx = smincol; dx <= dmaxcol; sx++, dx++) {
			if (over == TRUE ) {
				if (((src->_line[sy][sx] & A_CHARTEXT) != ' ') && (dst->_line[dy][dx] != src->_line[sy][sx]))  {	
					dst->_line[dy][dx] = src->_line[sy][sx];
					dst->_lastchar[dy] = dx;
				} else
					dst->_firstchar[dy]++;
			} else {
				if (dst->_line[dy][dx] != src->_line[sy][sx]) {  	
					dst->_line[dy][dx] = src->_line[sy][sx];
					dst->_lastchar[dy] = dx;
				} else
					dst->_firstchar[dy]++;
			}
		}
	}
	T(("finished copywin"));
	return OK;
}
