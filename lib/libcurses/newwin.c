/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)newwin.c	5.4 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * allocate space for and set up defaults for a new window
 *
 */

# include	"curses.ext"

char	*malloc();

# define	SMALLOC	(short *) malloc

static WINDOW	*makenew();

# undef		nl	/* don't need it here, and it interferes	*/

WINDOW *
newwin(num_lines, num_cols, begy, begx)
int	num_lines, num_cols, begy, begx;
{
	reg WINDOW	*win;
	reg chtype      *sp;
	reg int		i, by, bx, nl, nc;
	reg int		j;

	by = begy;
	bx = begx;
	nl = num_lines;
	nc = num_cols;

	if (nl == 0)
		nl = LINES - by;
	if (nc == 0)
		nc = COLS - bx;
	if ((win = makenew(nl, nc, by, bx)) == NULL)
		return ERR;
	if ((win->_firstch = SMALLOC(nl * sizeof win->_firstch[0])) == NULL) {
		free(win->_y);
		free(win);
		return NULL;
	}
	if ((win->_lastch = SMALLOC(nl * sizeof win->_lastch[0])) == NULL) {
		free(win->_y);
		free(win->_firstch);
		free(win);
		return NULL;
	}
	win->_nextp = win;
	for (i = 0; i < nl; i++) {
		win->_firstch[i] = _NOCHANGE;
		win->_lastch[i] = _NOCHANGE;
	}
	for (i = 0; i < nl; i++)
		if ((win->_y[i] = (chtype *) malloc(nc * sizeof(chtype))) == NULL) {
			for (j = 0; j < i; j++)
				free(win->_y[j]);
			free(win->_firstch);
			free(win->_lastch);
			free(win->_y);
			free(win);
			return ERR;
		}
		else
			for (sp = win->_y[i]; sp < win->_y[i] + nc; )
				*sp++ = ' ';
	win->_ch_off = 0;
# ifdef DEBUG
	fprintf(outf, "NEWWIN: win->_ch_off = %d\n", win->_ch_off);
# endif
	return win;
}

WINDOW *
subwin(orig, num_lines, num_cols, begy, begx)
reg WINDOW	*orig;
int		num_lines, num_cols, begy, begx;
{
	reg int		i;
	reg WINDOW	*win;
	reg int		by, bx, nl, nc;

	by = begy;
	bx = begx;
	nl = num_lines;
	nc = num_cols;

	/*
	 * make sure window fits inside the original one
	 */
# ifdef	DEBUG
	fprintf(outf, "SUBWIN(%0.2o, %d, %d, %d, %d)\n", orig, nl, nc, by, bx);
# endif
	if (by < orig->_begy || bx < orig->_begx
	    || by + nl > orig->_maxy + orig->_begy
	    || bx + nc > orig->_maxx + orig->_begx)
		return ERR;
	if (nl == 0)
		nl = orig->_maxy + orig->_begy - by;
	if (nc == 0)
		nc = orig->_maxx + orig->_begx - bx;
	if ((win = makenew(nl, nc, by, bx)) == NULL)
		return ERR;
	win->_nextp = orig->_nextp;
	orig->_nextp = win;
	win->_orig = orig;
	_set_subwin_(orig, win);
	return win;
}

/*
 * this code is shared with mvwin()
 */
_set_subwin_(orig, win)
register WINDOW	*orig, *win;
{
	register int	i, j, k;

	j = win->_begy - orig->_begy;
	k = win->_begx - orig->_begx;
	win->_ch_off = k;
# ifdef DEBUG
	fprintf(outf, "_SET_SUBWIN_: win->_ch_off = %d\n", win->_ch_off);
# endif
	win->_firstch = &orig->_firstch[j];
	win->_lastch = &orig->_lastch[j];
	for (i = 0; i < win->_maxy; i++, j++)
		win->_y[i] = &orig->_y[j][k];

}

/*
 *	This routine sets up a window buffer and returns a pointer to it.
 */
static WINDOW *
makenew(num_lines, num_cols, begy, begx)
int	num_lines, num_cols, begy, begx; {

	reg int		i;
	reg WINDOW	*win;
	reg int		by, bx, nl, nc;

	by = begy;
	bx = begx;
	nl = num_lines;
	nc = num_cols;

# ifdef	DEBUG
	fprintf(outf, "MAKENEW(%d, %d, %d, %d)\n", nl, nc, by, bx);
# endif
	if ((win = (WINDOW *) malloc(sizeof *win)) == NULL)
		return NULL;
# ifdef DEBUG
	fprintf(outf, "MAKENEW: nl = %d\n", nl);
# endif
	if ((win->_y = (chtype **) malloc(nl * sizeof(chtype *))) == NULL) {
		free(win);
		return NULL;
	}
# ifdef DEBUG
	fprintf(outf, "MAKENEW: nc = %d\n", nc);
# endif
	win->_cury = win->_curx = 0;
	win->_clear = FALSE;
	win->_maxy = nl;
	win->_maxx = nc;
	win->_begy = by;
	win->_begx = bx;
	win->_flags = 0;
	win->_scroll = win->_leave = FALSE;
	_swflags_(win);
# ifdef DEBUG
	fprintf(outf, "MAKENEW: win->_clear = %d\n", win->_clear);
	fprintf(outf, "MAKENEW: win->_leave = %d\n", win->_leave);
	fprintf(outf, "MAKENEW: win->_scroll = %d\n", win->_scroll);
	fprintf(outf, "MAKENEW: win->_flags = %0.2o\n", win->_flags);
	fprintf(outf, "MAKENEW: win->_maxy = %d\n", win->_maxy);
	fprintf(outf, "MAKENEW: win->_maxx = %d\n", win->_maxx);
	fprintf(outf, "MAKENEW: win->_begy = %d\n", win->_begy);
	fprintf(outf, "MAKENEW: win->_begx = %d\n", win->_begx);
# endif
	return win;
}

_swflags_(win)
register WINDOW	*win;
{
	win->_flags &= ~(_ENDLINE|_FULLLINE|_FULLWIN|_SCROLLWIN);
	if (win->_begx + win->_maxx == COLS) {
		win->_flags |= _ENDLINE;
		if (win->_begx == 0) {
			if (AL && DL)
				win->_flags |= _FULLLINE;
			if (win->_maxy == LINES && win->_begy == 0)
				win->_flags |= _FULLWIN;
		}
		if (win->_begy + win->_maxy == LINES)
			win->_flags |= _SCROLLWIN;
	}
}
