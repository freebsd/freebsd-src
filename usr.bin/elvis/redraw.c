/* redraw.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains functions that draw text on the screen.  The major entry
 * points are:
 *	redrawrange()	- called from modify.c to give hints about what parts
 *			  of the screen need to be redrawn.
 *	redraw()	- redraws the screen (or part of it) and positions
 *			  the cursor where it belongs.
 *	idx2col()	- converts a markidx() value to a logical column number.
 */

#include "config.h"
#include "vi.h"
#ifdef CRUNCH
# define NEAR	LINES
#else
# define NEAR	(*o_nearscroll&0xff)
#endif

/* This variable contains the line number that smartdrawtext() knows best */
static long smartlno;

/* This function remembers where changes were made, so that the screen can be
 * redraw in a more efficient manner.
 */
static long	redrawafter;	/* line# of first line that must be redrawn */
static long	preredraw;	/* line# of last line changed, before change */
static long	postredraw;	/* line# of last line changed, after change */
static int	mustredraw;	/* boolean: anything forcing a screen update? */
void redrawrange(after, pre, post)
	long	after;	/* lower bound of redrawafter */
	long	pre;	/* upper bound of preredraw */
	long	post;	/* upper bound of postredraw */
{
	if (after == redrawafter)
	{
		/* multiple insertions/deletions at the same place -- combine
		 * them
		 */
		preredraw -= (post - pre);
		if (postredraw < post)
		{
			preredraw += (post - postredraw);
			postredraw = post;
		}
		if (redrawafter > preredraw)
		{
			redrawafter = preredraw;
		}
		if (redrawafter < 1L)
		{
			redrawafter = 0L;
			preredraw = postredraw = INFINITY;
		}
	}
	else if (postredraw > 0L)
	{
		/* multiple changes in different places -- redraw everything
		 * after "after".
		 */
		postredraw = preredraw = INFINITY;
		if (after < redrawafter)
			redrawafter = after;
	}
	else
	{
		/* first change */
		redrawafter = after;
		preredraw = pre;
		postredraw = post;
	}
	mustredraw = TRUE;
}


#ifndef NO_CHARATTR
/* see if a given line uses character attribute strings */
static int hasattr(lno, text)
	long		lno;	/* the line# of the cursor */
	REG char	*text;	/* the text of the line, from fetchline */
{
	static long	plno;	/* previous line number */
	static long	chgs;	/* previous value of changes counter */
	static int	panswer;/* previous answer */
	char		*scan;

	/* if charattr is off, then the answer is "no, it doesn't" */
	if (!*o_charattr)
	{
		chgs = 0; /* <- forces us to check if charattr is later set */
		return FALSE;
	}

	/* if we already know the answer, return it... */
	if (lno == plno && chgs == changes)
	{
		return panswer;
	}

	/* get the line & look for "\fX" */
	if (!text[0] || !text[1] || !text[2])
	{
		panswer = FALSE;
	}
	else
	{
		for (scan = text; scan[2] && !(scan[0] == '\\' && scan[1] == 'f'); scan++)
		{
		}
		panswer = (scan[2] != '\0');
	}

	/* save the results */
	plno = lno;
	chgs = changes;

	/* return the results */
	return panswer;
}
#endif


#ifndef NO_VISIBLE
/* This function checks to make sure that the correct lines are shown in
 * reverse-video.  This is used to handle the "v" and "V" commands.
 */
static long	vizlow, vizhigh;	/* the starting and ending lines */
static int	vizleft, vizright;	/* starting & ending indicies */
static int	vizchange;		/* boolean: must use stupid drawtext? */
static void setviz(curs)
	MARK		curs;
{
	long		newlow, newhigh;
	long		extra = 0L;

	/* for now, assume the worst... */
	vizchange = TRUE;

	/* set newlow & newhigh according to V_from and cursor */
	if (!V_from)
	{
		/* no lines should have reverse-video */
		if (vizlow)
		{
			redrawrange(vizlow, vizhigh + 1L, vizhigh + 1L);
			vizlow = vizhigh = 0L;
		}
		else
		{
			vizchange = FALSE;
		}
		return;
	}

	/* figure out which lines *SHOULD* have hilites */
	if (V_from < curs)
	{
		newlow = markline(V_from);
		newhigh = markline(curs);
		vizleft = markidx(V_from);
		vizright = markidx(curs) + 1;
	}
	else
	{
		newlow = markline(curs);
		newhigh = markline(V_from);
		vizleft = markidx(curs);
		vizright = markidx(V_from) + 1;
	}

	/* adjust for line-mode hiliting */
	if (V_linemd)
	{
		vizleft = 0;
		vizright = BLKSIZE - 1;
	}
	else
	{
		extra = 1L;
	}

	/* arrange for the necessary lines to be redrawn */
	if (vizlow == 0L)
	{
		/* just starting to redraw */
		redrawrange(newlow, newhigh, newhigh);
	}
	else
	{
		/* Were new lines added/removed at the front? */
		if (newlow != vizlow)
		{
			if (newlow < vizlow)
				redrawrange(newlow, vizlow + extra, vizlow + extra);
			else
				redrawrange(vizlow, newlow + extra, newlow + extra);
		}

		/* Were new lines added/removed at the back? */
		if (newhigh != vizhigh)
		{
			if (newhigh < vizhigh)
				redrawrange(newhigh + 1L - extra, vizhigh + 1L, vizhigh + 1L);
			else
				redrawrange(vizhigh + 1L - extra, newhigh, newhigh);
		}
	}

	/* remember which lines will contain hilighted text now */
	vizlow = newlow;
	vizhigh = newhigh;
}
#endif /* !NO_VISIBLE */


/* This function converts a MARK to a column number.  It doesn't automatically
 * adjust for leftcol; that must be done by the calling function
 */
int idx2col(curs, text, inputting)
	MARK		curs;	/* the line# & index# of the cursor */
	REG char	*text;	/* the text of the line, from fetchline */
	int		inputting;	/* boolean: called from input() ? */
{
	static MARK	pcursor;/* previous cursor, for possible shortcut */
	static MARK	pcol;	/* column number for pcol */
	static long	chgs;	/* previous value of changes counter */
	REG int		col;	/* used to count column numbers */
	REG int		idx;	/* used to count down the index */
	REG int		i;

	/* for now, assume we have to start counting at the left edge */
	col = 0;
	idx = markidx(curs);

	/* if the file hasn't changed & line number is the same & it has no
	 * embedded character attribute strings, can we do shortcuts?
	 */
	if (chgs == changes
	 && !((curs ^ pcursor) & ~(BLKSIZE - 1))
#ifndef NO_CHARATTR
	 && !hasattr(markline(curs), text)
#endif
	)
	{
		/* no movement? */
		if (curs == pcursor)
		{
			/* return the column of the char; for tabs, return its last column */
			if (text[idx] == '\t' && !inputting && !*o_list)
			{
				return pcol + *o_tabstop - (pcol % *o_tabstop) - 1;
			}
			else
			{
				return pcol;
			}
		}

		/* movement to right? */
		if (curs > pcursor)
		{
			/* start counting from previous place */
			col = pcol;
			idx = markidx(curs) - markidx(pcursor);
			text += markidx(pcursor);
		}
	}

	/* count over to the char after the idx position */
	while (idx > 0 && (i = *text)) /* yes, ASSIGNMENT! */
	{
		if (i == '\t' && !*o_list)
		{
			col += *o_tabstop;
			col -= col % *o_tabstop;
		}
		else if (i >= '\0' && i < ' ' || i == '\177')
		{
			col += 2;
		}
#ifndef NO_CHARATTR
		else if (i == '\\' && text[1] == 'f' && text[2] && *o_charattr)
		{
			text += 2; /* plus one more at bottom of loop */
			idx -= 2;
		}			
#endif
		else
		{
			col++;
		}
		text++;
		idx--;
	}

	/* save stuff to speed next call */
	pcursor = curs;
	pcol = col;
	chgs = changes;

	/* return the column of the char; for tabs, return its last column */
	if (*text == '\t' && !inputting && !*o_list)
	{
		return col + *o_tabstop - (col % *o_tabstop) - 1;
	}
	else
	{
		return col;
	}
}


/* This function is similar to idx2col except that it takes care of sideways
 * scrolling - for the given line, at least.
 */
int mark2phys(m, text, inputting)
	MARK	m;		/* a mark to convert */
	char	*text;		/* the line that m refers to */
	int	inputting;	/* boolean: caled from input() ? */
{
	int	i;

	i = idx2col(m, text, inputting);
	while (i < leftcol)
	{
		leftcol -= *o_sidescroll;
		mustredraw = TRUE;
		redrawrange(1L, INFINITY, INFINITY);
	}
	while (i > rightcol)
	{
		leftcol += *o_sidescroll;
		mustredraw = TRUE;
		redrawrange(1L, INFINITY, INFINITY);
	}
	physrow = markline(m) - topline;
	physcol = i - leftcol;
	if (*o_number)
		physcol += 8;

	return physcol;
}

/* This function draws a single line of text on the screen.  The screen's
 * cursor is assumed to be located at the leftmost column of the appropriate
 * row.
 */
static void drawtext(text, lno, clr)
	REG char	*text;	/* the text to draw */
	long		lno;	/* the number of the line to draw */
	int		clr;	/* boolean: do a clrtoeol? */
{
	REG int		col;	/* column number */
	REG int		i;
	REG int		tabstop;	/* *o_tabstop */
	REG int		limitcol;	/* leftcol or leftcol + COLS */
	int		abnormal;	/* boolean: charattr != A_NORMAL? */
#ifndef NO_VISIBLE
	int		rev;		/* boolean: standout mode, too? */
	int		idx = 0;
#endif
	char		numstr[9];

	/* show the line number, if necessary */
	if (*o_number)
	{
		sprintf(numstr, "%6ld  ", lno);
		qaddstr(numstr);
	}

#ifndef NO_SENTENCE
	/* if we're hiding format lines, and this is one of them, then hide it */
	if (*o_hideformat && *text == '.')
	{
		clrtoeol();
#if OSK
		qaddch('\l');
#else
		qaddch('\n');
#endif
		return;
	}
#endif

	/* move some things into registers... */
	limitcol = leftcol;
	tabstop = *o_tabstop;
	abnormal = FALSE;

#ifndef CRUNCH
	if (clr)
		clrtoeol();
#endif

	/* skip stuff that was scrolled off left edge */
	for (col = 0;
	     (i = *text) && col < limitcol; /* yes, ASSIGNMENT! */
	     text++)
	{
#ifndef NO_VISIBLE
		idx++;
#endif
		if (i == '\t' && !*o_list)
		{
			col = col + tabstop - (col % tabstop);
		}
		else if (i >= 0 && i < ' ' || i == '\177')
		{
			col += 2;
		}
#ifndef NO_CHARATTR
		else if (i == '\\' && text[1] == 'f' && text[2] && *o_charattr)
		{
			text += 2; /* plus one more as part of "for" loop */

			/* since this attribute might carry over, we need it */
			switch (*text)
			{
			  case 'R':
			  case 'P':
				attrset(A_NORMAL);
				abnormal = FALSE;
				break;

			  case 'B':
				attrset(A_BOLD);
				abnormal = TRUE;
				break;

			  case 'U':
				attrset(A_UNDERLINE);
				abnormal = TRUE;
				break;

			  case 'I':
				attrset(A_ALTCHARSET);
				abnormal = TRUE;
				break;
			}
		}
#endif
		else
		{
			col++;
		}
	}

#ifndef NO_VISIBLE
	/* Should we start hiliting at the first char of this line? */
	if ((lno > vizlow && lno <= vizhigh
	    || lno == vizlow && vizleft < idx)
	   && !(lno == vizhigh && vizright < idx))
	{
		do_VISIBLE();
		rev = TRUE;
	}
#endif

	/* adjust for control char that was partially visible */
	while (col > limitcol)
	{
		qaddch(' ');
		limitcol++;
	}

	/* now for the visible characters */
	limitcol = leftcol + COLS;
	if (*o_number)
		limitcol -= 8;
	for (; (i = *text) && col < limitcol; text++)
	{
#ifndef NO_VISIBLE
		/* maybe turn hilite on/off in the middle of the line */
		if (lno == vizlow && vizleft == idx)
		{
			do_VISIBLE();
			rev = TRUE;
		}
		if (lno == vizhigh && vizright == idx)
		{
			do_SE();
			rev = FALSE;
		}
		idx++;

		/* if hiliting, never emit physical tabs */
		if (rev && i == '\t' && !*o_list)
		{
			i = col + tabstop - (col % tabstop);
			do
			{
				qaddch(' ');
				col++;
			} while (col < i && col < limitcol);
		}
		else
#endif /* !NO_VISIBLE */
		if (i == '\t' && !*o_list)
		{
			i = col + tabstop - (col % tabstop);
			if (i < limitcol)
			{
#ifdef CRUNCH
				if (!clr && has_PT && !((i - leftcol) & 7))
#else
				if (has_PT && !((i - leftcol) & 7))
#endif
				{
					do
					{
						qaddch('\t');
						col += 8; /* not exact! */
					} while (col < i);
					col = i; /* NOW it is exact */
				}
				else
				{
					do
					{
						qaddch(' ');
						col++;
					} while (col < i && col < limitcol);
				}
			}
			else /* tab ending after screen? next line! */
			{
#ifdef CRUNCH
				/* needed at least when scrolling the screen right  -nox */
				if (clr && col < limitcol)
					clrtoeol();
#endif
				col = limitcol;
				if (has_AM)
				{
					addch('\n');	/* GB */
				}
			}
		}
		else if (i >= 0 && i < ' ' || i == '\177')
		{
			col += 2;
			qaddch('^');
			if (col <= limitcol)
			{
				qaddch(i ^ '@');
			}
		}
#ifndef NO_CHARATTR
		else if (i == '\\' && text[1] == 'f' && text[2] && *o_charattr)
		{
			text += 2; /* plus one more as part of "for" loop */
			switch (*text)
			{
			  case 'R':
			  case 'P':
				attrset(A_NORMAL);
				abnormal = FALSE;
				break;

			  case 'B':
				attrset(A_BOLD);
				abnormal = TRUE;
				break;

			  case 'U':
				attrset(A_UNDERLINE);
				abnormal = TRUE;
				break;

			  case 'I':
				attrset(A_ALTCHARSET);
				abnormal = TRUE;
				break;
			}
		}
#endif
		else
		{
			col++;
			qaddch(i);
		}
	}

	/* get ready for the next line */
#ifndef NO_CHARATTR
	if (abnormal)
	{
		attrset(A_NORMAL);
	}
#endif
	if (*o_list && col < limitcol)
	{
		qaddch('$');
		col++;
	}

#ifndef NO_VISIBLE
	/* did we hilite this whole line?  If so, STOP! */
	if (rev)
	{
		do_SE();
	}
#endif

#ifdef CRUNCH
	if (clr && col < limitcol)
	{
		clrtoeol();
	}
#endif
	if (!has_AM || col < limitcol)
	{
		addch('\n');
	}

	wqrefresh();
}


#ifndef CRUNCH
static void nudgecursor(same, scan, new, lno)
	int	same;	/* number of chars to be skipped over */
	char	*scan;	/* where the same chars end */
	char	*new;	/* where the visible part of the line starts */
	long	lno;	/* line number of this line */
{
	int	col;

	if (same > 0)
	{
		if (same < 5)
		{
			/* move the cursor by overwriting */
			while (same > 0)
			{
				qaddch(scan[-same]);
				same--;
			}
		}
		else
		{
			/* move the cursor by calling move() */
			col = (int)(scan - new);
			if (*o_number)
				col += 8;
			move((int)(lno - topline), col);
		}
	}
}
#endif /* not CRUNCH */

/* This function draws a single line of text on the screen, possibly with
 * some cursor optimization.  The cursor is repositioned before drawing
 * begins, so its position before doesn't really matter.
 */
static void smartdrawtext(text, lno, showit)
	REG char	*text;	/* the text to draw */
	long		lno;	/* line number of the text */
	int		showit;	/* boolean: output line? (else just remember it) */
{
#ifdef CRUNCH
	move((int)(lno - topline), 0);
	if (showit)
	{
		drawtext(text, lno, TRUE);
	}
#else /* not CRUNCH */
	static char	old[256];	/* how the line looked last time */
	char		new[256];	/* how it looks now */
	char		*build;		/* used to put chars into new[] */
	char		*scan;		/* used for moving thru new[] or old[] */
	char		*end;		/* last non-blank changed char */
	char		*shift;		/* used to insert/delete chars */
	int		same;		/* length of a run of unchanged chars */
	int		limitcol;
	int		col;
	int		i;
	char		numstr[9];

# ifndef NO_CHARATTR
	/* if this line has attributes, do it the dumb way instead */
	if (hasattr(lno, text))
	{
		move((int)(lno - topline), 0);
		drawtext(text, lno, TRUE);
		return;
	}
# endif
# ifndef NO_SENTENCE
	/* if this line is a format line, & we're hiding format lines, then
	 * let the dumb drawtext() function handle it
	 */
	if (*o_hideformat && *text == '.')
	{
		move((int)(lno - topline), 0);
		drawtext(text, lno, TRUE);
		return;
	}
# endif
# ifndef NO_VISIBLE
	if (vizchange)
	{
		move((int)(lno - topline), 0);
		drawtext(text, lno, TRUE);
		smartlno = 0L;
		return;
	}
# endif

	/* skip stuff that was scrolled off left edge */
	limitcol = leftcol;
	for (col = 0;
	     (i = *text) && col < limitcol; /* yes, ASSIGNMENT! */
	     text++)
	{
		if (i == '\t' && !*o_list)
		{
			col = col + *o_tabstop - (col % *o_tabstop);
		}
		else if (i >= 0 && i < ' ' || i == '\177')
		{
			col += 2;
		}
		else
		{
			col++;
		}
	}

	/* adjust for control char that was partially visible */
	build = new;
	while (col > limitcol)
	{
		*build++ = ' ';
		limitcol++;
	}

	/* now for the visible characters */
	limitcol = leftcol + COLS;
	if (*o_number)
		limitcol -= 8;
	for (; (i = *text) && col < limitcol; text++)
	{
		if (i == '\t' && !*o_list)
		{
			i = col + *o_tabstop - (col % *o_tabstop);
			while (col < i && col < limitcol)
			{
				*build++ = ' ';
				col++;
			}
		}
		else if (i >= 0 && i < ' ' || i == '\177')
		{
			col += 2;
			*build++ = '^';
			if (col <= limitcol)
			{
				*build++ = (i ^ '@');
			}
		}
		else
		{
			col++;
			*build++ = i;
		}
	}
	if (col < limitcol && *o_list)
	{
		*build++ = '$';
		col++;
	}
	end = build;
	while (col < limitcol)
	{
		*build++ = ' ';
		col++;
	}

	/* if we're just supposed to remember this line, then remember it */
	if (!showit)
	{
		smartlno = lno;
		strncpy(old, new, COLS);
		return;
	}

	/* locate the last non-blank character */
	while (end > new && end[-1] == ' ')
	{
		end--;
	}

	/* can we optimize the displaying of this line? */
	if (lno != smartlno)
	{
		/* nope, can't optimize - different line */
		move((int)(lno - topline), 0);

		/* show the line number, if necessary */
		if (*o_number)
		{
			sprintf(numstr, "%6ld  ", lno);
			qaddstr(numstr);
		}

		/* show the new line */
		for (scan = new, build = old; scan < end; )
		{
			qaddch(*scan);
			*build++ = *scan++;
		}
		if (end < new + COLS - (*o_number ? 8 : 0))
		{
			clrtoeol();
			while (build < old + COLS)
			{
				*build++ = ' ';
			}
		}
		smartlno = lno;
		return;
	}

	/* skip any initial unchanged characters */
	for (scan = new, build = old; scan < end && *scan == *build; scan++, build++)
	{
	}
	i = (scan - new);
	if (*o_number)
		i += 8;
	move((int)(lno - topline), i);

	/* The in-between characters must be changed */
	same = 0;
	while (scan < end)
	{
		/* is this character a match? */
		if (scan[0] == build[0])
		{
			same++;
		}
		else /* do we want to insert? */
		if (scan < end - 1 && scan[1] == build[0] && (has_IC || has_IM))
		{
			nudgecursor(same, scan, new, lno);
			same = 0;

			insch(*scan);
			for (shift = old + COLS; --shift > build; )
			{
				shift[0] = shift[-1];
			}
			*build = *scan;
		}
		else /* do we want to delete? */
		if (build < old + COLS - 1 && scan[0] == build[1] && has_DC)
		{
			nudgecursor(same, scan, new, lno);
			same = 0;

			delch();
			same++;
			for (shift = build; shift < old + COLS - 1; shift++)
			{
				shift[0] = shift[1];
			}
			if (*o_number)
				shift -= 8;
			*shift = ' ';
		}
		else /* we must overwrite */
		{
			nudgecursor(same, scan, new, lno);
			same = 0;

			addch(*scan);
			*build = *scan;
		}

		build++;
		scan++;
	}

	/* maybe clear to EOL */
	end = old + COLS - (*o_number ? 8 : 0);
	while (build < end && *build == ' ')
	{
		build++;
	}
	if (build < end)
	{
		nudgecursor(same, scan, new, lno);
		same = 0;

		clrtoeol();
		while (build < old + COLS)
		{
			*build++ = ' ';
		}
	}
#endif /* not CRUNCH */
}


/* This function is used in visual mode for drawing the screen (or just parts
 * of the screen, if that's all thats needed).  It also takes care of
 * scrolling.
 */
void redraw(curs, inputting)
	MARK	curs;		/* where to leave the screen's cursor */
	int	inputting;	/* boolean: being called from input() ? */
{
	char		*text;		/* a line of text to display */
	static long	chgs;		/* previous changes level */
	long		l;
	int		i;
#ifndef CRUNCH
	static long	showtop;	/* top line in window */
	static long	showbottom;	/* bottom line in window */
#endif

	/* if curs == MARK_UNSET, then we should reset internal vars */
	if (curs == MARK_UNSET)
	{
		if (topline < 1 || topline > nlines)
		{
			topline = 1L;
		}
		else
		{
			move(LINES - 1, 0);
			clrtoeol();
		}
		leftcol = 0;
		mustredraw = TRUE;
		redrawafter = INFINITY;
		preredraw = 0L;
		postredraw = 0L;
		chgs = 0;
		smartlno = 0L;
#ifndef NO_VISIBLE
		vizlow = vizhigh = 0L;
		vizchange = FALSE;
#endif
#ifndef CRUNCH
		showtop = 0;
		showbottom = INFINITY;
#endif
		return;
	}

#ifndef NO_VISIBLE
	/* adjustments to hilited area may force extra lines to be redrawn. */
	setviz(curs);
#endif

	/* figure out which column the cursor will be in */
	l = markline(curs);
	text = fetchline(l);
	mark2phys(curs, text, inputting);

#ifndef NO_COLOR
	fixcolor();
#endif

	/* adjust topline, if necessary, to get the cursor on the screen */
	if (l >= topline && l <= botline)
	{
		/* it is on the screen already */

		/* if the file was changed but !mustredraw, then redraw line */
		if (!mustredraw && (chgs != changes
#ifndef NO_VISIBLE
			|| V_from
#endif
#ifndef CRUNCH
			|| l < showtop || l > showbottom
#endif
							))
		{
			smartdrawtext(text, l, (chgs != changes));
		}
	}
	else if (l < topline && l >= topline - NEAR && (has_SR || has_AL))
	{
		/* near top - scroll down */
		if (!mustredraw)
		{
			move(0,0);
			while (l < topline)
			{
				topline--;
				if (has_SR)
				{
					do_SR();
				}
				else
				{
					insertln();
				}
				text = fetchline(topline);
				drawtext(text, topline, FALSE);
				do_UP();
			}

			/* blank out the last line */
			move(LINES - 1, 0);
			clrtoeol();
		}
		else
		{
			topline = l;
			redrawrange(0L, INFINITY, INFINITY);
		}
	}
	else if (l > topline && l <= botline + NEAR)
	{
		/* near bottom -- scroll up */
		if (!mustredraw)
		{
			move(LINES - 1,0);
			clrtoeol();
			while (l > botline)
			{
				topline++; /* <-- also adjusts botline */
				text = fetchline(botline);
				drawtext(text, botline, FALSE);
			}
#ifndef CRUNCH
			showbottom = l;
#endif
		}
		else
		{
			topline = l - (LINES - 2);
			redrawrange(0L, INFINITY, INFINITY);
		}
	}
	else
	{
		/* distant line - center it & force a redraw */
		topline = l - (LINES - 1) / 2;
		if (topline < 1)
		{
			topline = 1;
		}
		redrawrange(0L, INFINITY, INFINITY);
		smartlno = 0L;
		changes++;
	}

#ifndef CRUNCH
	/* make sure the current line is included in the "window" */
	if (l < showtop)
	{
		redrawrange(l, showtop, showtop);
		showtop = l;
	}
	if (l > showbottom)
	{
		redrawrange(showbottom, l, l);
		showbottom = l;
	}
#endif


	/* Now... do we really have to redraw? */
	if (mustredraw)
	{
		/* If redrawfter (and friends) aren't set, assume we should
		 * redraw everything.
		 */
		if (redrawafter == INFINITY)
		{
			redrawafter = 0L;
			preredraw = postredraw = INFINITY;
		}

#ifndef CRUNCH
		/* shrink the window, if possible */
		if (showtop < topline)
		{
			showtop = topline;
		}
		if (showbottom > botline)
		{
			showbottom = botline;
		}
		if (postredraw == INFINITY)
		{
			/* these will be set to more reasonable values later */
			showtop = INFINITY;
			showbottom = 0L;
		}
#endif

		/* adjust smartlno to correspond with inserted/deleted lines */
		if (smartlno >= redrawafter)
		{
			if (smartlno < preredraw && postredraw != preredraw) /*!!!*/
			{
				smartlno = 0L;
			}
			else
			{
				smartlno += (postredraw - preredraw);
			}
		}

		/* should we insert some lines into the screen? */
		if (preredraw < postredraw && preredraw <= botline)
		{
			/* lines were inserted into the file */

			/* decide where insertion should start */
			if (preredraw < topline)
			{
				l = topline;
			}
			else
			{
				l = preredraw;
			}

			/* insert the lines... maybe */
			if (l + postredraw - preredraw > botline || !has_AL || *o_number)
			{
				/* Whoa!  a whole screen full - just redraw */
				preredraw = postredraw = INFINITY;
			}
			else
			{
				/* really insert 'em */
				move((int)(l - topline), 0);
				for (i = postredraw - preredraw; i > 0; i--)
				{
					insertln();
				}

				/* NOTE: the contents of those lines will be
				 * drawn as part of the regular redraw loop.
				 */

				/* clear the last line */
				move(LINES - 1, 0);
				clrtoeol();
			}
		}

		/* do we want to delete some lines from the screen? */
		if (preredraw > postredraw && postredraw <= botline)
		{
			if (preredraw > botline || !has_DL || *o_number)
			{
				postredraw = preredraw = INFINITY;
			}
			else /* we'd best delete some lines from the screen */
			{
				/* clear the last line, so it doesn't look
				 * ugly as it gets pulled up into the screen
				 */
				move(LINES - 1, 0);
				clrtoeol();

				/* delete the lines */
				move((int)(postredraw - topline), 0);
			 	for (l = postredraw;
				     l < preredraw && l <= botline;
				     l++)
				{
					deleteln();
				}

				/* draw the lines that are now newly visible
				 * at the bottom of the screen
				 */
				i = LINES - 1 + (postredraw - preredraw);
				move(i, 0);
				for (l = topline + i; l <= botline; l++)
				{
					/* clear this line */
					clrtoeol();

					/* draw the line, or ~ for non-lines */
					if (l <= nlines)
					{
						text = fetchline(l);
						drawtext(text, l, FALSE);
					}
					else
					{
						addstr("~\n");
					}
				}
			}
		}

		/* redraw the current line */
		l = markline(curs);
		pfetch(l);
		smartdrawtext(ptext, l, TRUE);

#ifndef CRUNCH
		/* decide which lines must be in the "window" around the cursor */
		l = markline(curs);
		if ((*o_window & 0xff) + 1 == LINES)
		{
			showtop = 1;
			showbottom = INFINITY;
		}
		else if (l < showtop || l > showbottom)
		{
			l -= (*o_window & 0xff) / 2;
			if (l < topline)
			{
				l = topline;
			}
			if (l < showtop)
			{
				showtop = l;
			}
			l += (*o_window & 0xff) - 1;
			if (l > botline)
			{
				showtop = showtop - l + botline;
				l = botline;
			}
			if (l > showbottom)
			{
				showbottom = l;
			}
		}
#endif

		/* decide where we should start redrawing from */
		if (redrawafter < topline)
		{
			l = topline;
		}
		else
		{
			l = redrawafter;
		}
		if (l <= botline && l < postredraw && (l != smartlno || botline != smartlno))
		{
			/* draw the other lines */
			move((int)(l - topline), 0);
			for (; l <= botline && l < postredraw; l++)
			{
				/* we already drew the current line, so skip it now */
				if (l == smartlno)
				{
#if OSK
					qaddch('\l');
#else
					qaddch('\n');
#endif
					continue;
				}

				/* draw the line, or ~ for non-lines */
				if (l > nlines)
				{
					qaddch('~');
					clrtoeol();
					addch('\n');
				}
#ifndef CRUNCH
				else if (l < showtop || l > showbottom)
				{
					qaddch('@');
					clrtoeol();
					addch('\n');
				}
#endif
				else
				{
					text = fetchline(l);
					drawtext(text, l, TRUE);
				}
			}
		}

		mustredraw = FALSE;
	}

	/* force total (non-partial) redraw next time if not set */
	redrawafter = INFINITY;
	preredraw = 0L;
	postredraw = 0L;

	/* move the cursor to where it belongs */
	move((int)(markline(curs) - topline), physcol);
	wqrefresh();

	chgs = changes;
}
