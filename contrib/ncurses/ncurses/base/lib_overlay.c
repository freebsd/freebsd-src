/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
**	lib_overlay.c
**
**	The routines overlay(), copywin(), and overwrite().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_overlay.c,v 1.12 1998/02/11 12:13:59 tom Exp $")

static int overlap(const WINDOW *const s, WINDOW *const d, int const flag)
{
int sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol;

	T(("overlap : sby %d, sbx %d, smy %d, smx %d, dby %d, dbx %d, dmy %d, dmx %d",
		s->_begy, s->_begx, s->_maxy, s->_maxx,
		d->_begy, d->_begx, d->_maxy, d->_maxx));
	
	if (!s || !d)
		returnCode(ERR);

	sminrow = max(s->_begy, d->_begy) - s->_begy;
	smincol = max(s->_begx, d->_begx) - s->_begx;
	dminrow = max(s->_begy, d->_begy) - d->_begy;
	dmincol = max(s->_begx, d->_begx) - d->_begx;
	dmaxrow = min(s->_maxy+s->_begy, d->_maxy+d->_begy) - d->_begy;
	dmaxcol = min(s->_maxx+s->_begx, d->_maxx+d->_begx) - d->_begx;

	return(copywin(s, d,
		       sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol,
		       flag));
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

int overlay(const WINDOW *win1, WINDOW *win2)
{
	T((T_CALLED("overlay(%p,%p)"), win1, win2));
	returnCode(overlap(win1, win2, TRUE));
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

int overwrite(const WINDOW *win1, WINDOW *win2)
{
	T((T_CALLED("overwrite(%p,%p)"), win1, win2));
	returnCode(overlap(win1, win2, FALSE));
}

int copywin(const WINDOW *src, WINDOW *dst,
	int sminrow, int smincol,
	int dminrow, int dmincol, int dmaxrow, int dmaxcol,
	int over)
{
int sx, sy, dx, dy;
bool touched;
chtype bk = AttrOf(dst->_bkgd);
chtype mask = ~(chtype)((bk&A_COLOR) ? A_COLOR : 0);

	T((T_CALLED("copywin(%p, %p, %d, %d, %d, %d, %d, %d, %d)"),
		src, dst, sminrow, smincol, dminrow, dmincol, dmaxrow, dmaxcol, over));

	if (!src || !dst)
	  returnCode(ERR);

	/* make sure rectangle exists in source */
	if ((sminrow + dmaxrow - dminrow) > (src->_maxy + 1) ||
	    (smincol + dmaxcol - dmincol) > (src->_maxx + 1)) {
		returnCode(ERR);
	}

	T(("rectangle exists in source"));

	/* make sure rectangle fits in destination */
	if (dmaxrow > dst->_maxy || dmaxcol > dst->_maxx) {
		returnCode(ERR);
	}

	T(("rectangle fits in destination"));

	for (dy = dminrow, sy = sminrow; dy <= dmaxrow; sy++, dy++) {
	   touched = FALSE;
	   for(dx=dmincol, sx=smincol; dx <= dmaxcol; sx++, dx++)
	   {
		if (over)
		{
		   if ((TextOf(src->_line[sy].text[sx]) != ' ') &&
                       (dst->_line[dy].text[dx]!=src->_line[sy].text[sx]))
		   {
			dst->_line[dy].text[dx] =
					(src->_line[sy].text[sx] & mask) | bk;
			touched = TRUE;
		   }
	        }
		else {
		   if (dst->_line[dy].text[dx] != src->_line[sy].text[sx])
		   {
			dst->_line[dy].text[dx] = src->_line[sy].text[sx];
			touched = TRUE;
                   }
                }
           }
	   if (touched)
	   {
	      touchline(dst,0,getmaxy(dst));
	   }
	}
	T(("finished copywin"));
	returnCode(OK);
}
