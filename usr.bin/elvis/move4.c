/* move4.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains movement functions which are screen-relative */

#include "config.h"
#include "vi.h"

/* This moves the cursor to a particular row on the screen */
/*ARGSUSED*/
MARK m_row(m, cnt, key)
	MARK	m;	/* the cursor position */
	long	cnt;	/* the row we'll move to */
	int	key;	/* the keystroke of this move - H/L/M */
{
	DEFAULT(1);

	/* calculate destination line based on key */
	cnt--;
	switch (key)
	{
	  case 'H':
		cnt = topline + cnt;
		break;

	  case 'M':
		cnt = topline + (LINES - 1) / 2;
		break;

	  case 'L':
		cnt = botline - cnt;
		break;
	}

	/* return the mark of the destination line */
	return MARK_AT_LINE(cnt);
}


/* This function repositions the current line to show on a given row */
MARK m_z(m, cnt, key)
	MARK	m;	/* the cursor */
	long	cnt;	/* the line number we're repositioning */
	int	key;	/* key struck after the z */
{
	long	newtop;
	int	i;

	/* Which line are we talking about? */
	if (cnt < 0 || cnt > nlines)
	{
		return MARK_UNSET;
	}
	if (cnt)
	{
		m = MARK_AT_LINE(cnt);
		newtop = cnt;
	}
	else
	{
		newtop = markline(m);
	}

	/* allow a "window size" number to be entered */
	for (i = 0; key >= '0' && key <= '9'; key = getkey(0))
	{
		i = i * 10 + key - '0';
	}
#ifndef CRUNCH
	if (i > 0 && i <= LINES - 1)
	{
		*o_window = i;
		wset = TRUE;
	}
#else
	/* the number is ignored if -DCRUNCH */
#endif

	/* figure out which line will have to be at the top of the screen */
	switch (key)
	{
	  case '\n':
#if OSK
	  case '\l':
#else
	  case '\r':
#endif
	  case '+':
		break;

	  case '.':
	  case 'z':
		newtop -= LINES / 2;
		break;

	  case '-':
		newtop -= LINES - 1;
		break;

	  default:
		return MARK_UNSET;
	}

	/* make the new topline take effect */
	redraw(MARK_UNSET, FALSE);
	if (newtop >= 1)
	{
		topline = newtop;
	}
	else
	{
		topline = 1L;
	}
	redrawrange(0L, INFINITY, INFINITY);

	/* The cursor doesn't move */
	return m;
}


/* This function scrolls the screen.  It does this by calling redraw() with
 * an off-screen line as the argument.  It will move the cursor if necessary
 * so that the cursor is on the new screen.
 */
/*ARGSUSED*/
MARK m_scroll(m, cnt, key)
	MARK	m;	/* the cursor position */
	long	cnt;	/* for some keys: the number of lines to scroll */
	int	key;	/* keystroke that causes this movement */
{
	MARK	tmp;	/* a temporary mark, used as arg to redraw() */
#ifndef CRUNCH
	int	savenearscroll;

	savenearscroll = *o_nearscroll;
	*o_nearscroll = LINES;
#endif

	/* adjust cnt, and maybe *o_scroll, depending of key */
	switch (key)
	{
	  case ctrl('F'):
	  case ctrl('B'):
		DEFAULT(1);
		redrawrange(0L, INFINITY, INFINITY); /* force complete redraw */
		cnt = cnt * (LINES - 1) - 2; /* keeps two old lines on screen */
		break;

	  case ctrl('E'):
	  case ctrl('Y'):
		DEFAULT(1);
		break;

	  case ctrl('U'):
	  case ctrl('D'):
		if (cnt == 0) /* default */
		{
			cnt = *o_scroll;
		}
		else
		{
			if (cnt > LINES - 1)
			{
				cnt = LINES - 1;
			}
			*o_scroll = cnt;
		}
		break;
	}

	/* scroll up or down, depending on key */
	switch (key)
	{
	  case ctrl('B'):
	  case ctrl('Y'):
	  case ctrl('U'):
		cnt = topline - cnt;
		if (cnt < 1L)
		{
			cnt = 1L;
			m = MARK_FIRST;
		}
		tmp = MARK_AT_LINE(cnt) + markidx(m);
		redraw(tmp, FALSE);
		if (markline(m) > botline)
		{
			m = MARK_AT_LINE(botline);
		}
		break;

	  case ctrl('F'):
	  case ctrl('E'):
	  case ctrl('D'):
		cnt = botline + cnt;
		if (cnt > nlines)
		{
			cnt = nlines;
			m = MARK_LAST;
		}
		tmp = MARK_AT_LINE(cnt) + markidx(m);
		redraw(tmp, FALSE);
		if (markline(m) < topline)
		{
			m = MARK_AT_LINE(topline);
		}
		break;
	}

#ifndef CRUNCH
	*o_nearscroll = savenearscroll;
#endif
	return m;
}
