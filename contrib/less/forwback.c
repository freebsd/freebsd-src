/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Primitives for displaying the file on the screen,
 * scrolling either forward or backward.
 */

#include "less.h"
#include "position.h"

extern int less_is_more;

public lbool squished;
public int no_back_scroll = 0;
public int forw_prompt;
public lbool first_time = TRUE; /* We're printing the first screen of output */
public int shell_lines = 1;
/* soft_eof is set as end-of-file when a read attempt returns EOF. This can
 * differ from actual EOF (ch_length()) if & filtering is in effect. */
public POSITION soft_eof = NULL_POSITION;

extern int sigs;
extern int top_scroll;
extern int quiet;
extern int sc_width, sc_height;
extern int hshift;
extern int auto_wrap;
extern lbool plusoption;
extern int forw_scroll;
extern int back_scroll;
extern int ignore_eoi;
extern int header_lines;
extern int header_cols;
extern int full_screen;
extern int stop_on_form_feed;
extern POSITION header_start_pos;
extern lbool getting_one_screen;
#if HILITE_SEARCH
extern int hilite_search;
extern int status_col;
#endif
#if TAGS
extern char *tagoption;
#endif

/*
 * Sound the bell to indicate user is trying to move past end of file.
 */
public void eof_bell(void)
{
#if HAVE_TIME
	{
		static time_type last_eof_bell = 0;
		time_type now = get_time();
		if (now == last_eof_bell) /* max once per second */
			return;
		last_eof_bell = now;
	}
#endif
	if (quiet == NOT_QUIET)
		bell();
	else
		vbell();
}

/*
 * Check to see if the end of file is currently displayed.
 */
public lbool eof_displayed(lbool offset)
{
	POSITION pos;

	if (ignore_eoi)
		return (FALSE);

	if (ch_length() == NULL_POSITION)
		/*
		 * If the file length is not known,
		 * we can't possibly be displaying EOF.
		 */
		return (FALSE);

	/*
	 * If the bottom line is empty, we are at EOF.
	 * If the bottom line ends at the file length,
	 * we must be just at EOF.
	 */
	pos = position(offset ? BOTTOM_OFFSET : BOTTOM_PLUS_ONE);
	return (pos == NULL_POSITION || pos == ch_length() || pos == soft_eof);
}

/*
 * Check to see if the entire file is currently displayed.
 */
public lbool entire_file_displayed(void)
{
	POSITION pos;

	/* Make sure last line of file is displayed. */
	if (!eof_displayed(TRUE))
		return (FALSE);

	/* Make sure first line of file is displayed. */
	pos = position(0);
	return (pos == NULL_POSITION || pos == 0);
}

/*
 * If the screen is "squished", repaint it.
 * "Squished" means the first displayed line is not at the top
 * of the screen; this can happen when we display a short file
 * for the first time.
 */
public void squish_check(void)
{
	if (!squished)
		return;
	squished = FALSE;
	repaint();
}

/*
 * Read the first pfx columns of the next line.
 * If skipeol==0 stop there, otherwise read and discard chars to end of line.
 */
static POSITION forw_line_pfx(POSITION pos, int pfx, int skipeol)
{
	int save_sc_width = sc_width;
	int save_auto_wrap = auto_wrap;
	int save_hshift = hshift;
	/* Set fake sc_width to force only pfx chars to be read. */
	sc_width = pfx + line_pfx_width();
	auto_wrap = 0;
	hshift = 0;
	pos = forw_line_seg(pos, skipeol, FALSE, FALSE, NULL, NULL);
	sc_width = save_sc_width;
	auto_wrap = save_auto_wrap;
	hshift = save_hshift;
	return pos;
}

/*
 * Set header text color.
 * Underline last line of headers, but not at header_start_pos
 * (where there is no gap between the last header line and the next line).
 */
static void set_attr_header(int ln)
{
	set_attr_line(AT_COLOR_HEADER);
	if (ln+1 == header_lines && position(0) != header_start_pos)
		set_attr_line(AT_UNDERLINE);
}

/*
 * Display file headers, overlaying text already drawn
 * at top and left of screen.
 */
public int overlay_header(void)
{
	int ln;
	lbool moved = FALSE;

	if (header_lines > 0)
	{
		/* Draw header_lines lines from start of file at top of screen. */
		POSITION pos = header_start_pos;
		home();
		for (ln = 0; ln < header_lines; ++ln)
		{
			pos = forw_line(pos, NULL, NULL);
			set_attr_header(ln);
			clear_eol();
			put_line(FALSE);
		}
		moved = TRUE;
	}
	if (header_cols > 0)
	{
		/* Draw header_cols columns at left of each line. */
		POSITION pos = header_start_pos;
		home();
		for (ln = 0; ln < sc_height-1; ++ln)
		{
			if (ln >= header_lines) /* switch from header lines to normal lines */
				pos = position(ln);
			if (pos == NULL_POSITION)
				putchr('\n');
			else 
			{
				/* Need skipeol for all header lines except the last one. */
				pos = forw_line_pfx(pos, header_cols, ln+1 < header_lines);
				set_attr_header(ln);
				put_line(FALSE);
			}
		}
		moved = TRUE;
	}
	if (moved)
		lower_left();
	return moved;
}

/*
 * Display n lines, scrolling forward, 
 * starting at position pos in the input file.
 * "force" means display the n lines even if we hit end of file.
 * "only_last" means display only the last screenful if n > screen size.
 * "nblank" is the number of blank lines to draw before the first
 *   real line.  If nblank > 0, the pos must be NULL_POSITION.
 *   The first real line after the blanks will start at ch_zero().
 * "to_newline" means count file lines rather than screen lines.
 */
public void forw(int n, POSITION pos, lbool force, lbool only_last, lbool to_newline, int nblank)
{
	int nlines = 0;
	lbool do_repaint;
	lbool newline;
	lbool first_line = TRUE;

	if (pos != NULL_POSITION)
		pos = after_header_pos(pos);
	squish_check();

	/*
	 * do_repaint tells us not to display anything till the end, 
	 * then just repaint the entire screen.
	 * We repaint if we are supposed to display only the last 
	 * screenful and the request is for more than a screenful.
	 * Also if the request exceeds the forward scroll limit
	 * (but not if the request is for exactly a screenful, since
	 * repainting itself involves scrolling forward a screenful).
	 */
	do_repaint = (only_last && n > sc_height-1) || 
		(forw_scroll >= 0 && n > forw_scroll && n != sc_height-1);
	if (!do_repaint)
	{
		if (top_scroll && n >= sc_height - 1 && pos != ch_length())
		{
			/*
			 * Start a new screen.
			 * {{ This is not really desirable if we happen
			 *    to hit eof in the middle of this screen,
			 *    but we don't yet know if that will happen. }}
			 */
			pos_clear();
			force = TRUE;
			if (less_is_more == 0) {
				clear();
				home();
			}
		}

		if (pos != position(BOTTOM_PLUS_ONE) || empty_screen())
		{
			/*
			 * This is not contiguous with what is
			 * currently displayed.  Clear the screen image 
			 * (position table) and start a new screen.
			 */
			pos_clear();
			force = TRUE;
			if (top_scroll)
			{
				clear();
				home();
			} else if (!first_time && !is_filtering() && full_screen)
			{
				putstr("...skipping...\n");
			}
		}
	}

	while (--n >= 0)
	{
		POSITION linepos = NULL_POSITION;
		/*
		 * Read the next line of input.
		 */
		if (nblank > 0)
		{
			/*
			 * Still drawing blanks; don't get a line 
			 * from the file yet.
			 * If this is the last blank line, get ready to
			 * read a line starting at ch_zero() next time.
			 */
			if (--nblank == 0)
				pos = ch_zero();
		} else
		{
			/* 
			 * Get the next line from the file.
			 */
			POSITION opos = pos;
			pos = forw_line(pos, &linepos, &newline);
			if (to_newline && !newline)
				++n;
			if (pos == NULL_POSITION)
			{
				/*
				 * End of file: stop here unless the top line 
				 * is still empty, or "force" is true.
				 * Even if force is true, stop when the last
				 * line in the file reaches the top of screen.
				 */
				soft_eof = opos;
				linepos = opos;
				if (ABORT_SIGS() ||
				   (!force && position(TOP) != NULL_POSITION) ||
				   (!empty_lines(0, 0) && !empty_lines(1, 1) && empty_lines(2, sc_height-1)))
				{
					pos = opos;
					break;
				}
			}
		}
		/*
		 * Add the position of the next line to the position table.
		 * Display the current line on the screen.
		 */
		add_forw_pos(linepos, first_line);
		first_line = FALSE;
		nlines++;
		if (do_repaint)
			continue;
		/*
		 * If this is the first screen displayed and
		 * we hit an early EOF (i.e. before the requested
		 * number of lines), we "squish" the display down
		 * at the bottom of the screen.
		 * But don't do this if a + option or a -t option
		 * was given.  These options can cause us to
		 * start the display after the beginning of the file,
		 * and it is not appropriate to squish in that case.
		 */
		if ((first_time || less_is_more) &&
		    pos == NULL_POSITION && !top_scroll && 
		    header_lines == 0 && header_cols == 0 &&
#if TAGS
		    tagoption == NULL &&
#endif
		    !plusoption)
		{
			squished = TRUE;
			continue;
		}
		put_line(TRUE);
		if (stop_on_form_feed && !do_repaint && line_is_ff() && position(TOP) != NULL_POSITION)
			break;
		forw_prompt = 1;
	}
	if (!first_line)
		add_forw_pos(pos, FALSE);
	if (nlines == 0 && !ignore_eoi)
		eof_bell();
	else if (do_repaint)
		repaint();
	else
	{
		overlay_header();
		/* lower_left(); {{ considered harmful? }} */
	}
	first_time = FALSE;
	(void) currline(BOTTOM);
}

/*
 * Display n lines, scrolling backward.
 */
public void back(int n, POSITION pos, lbool force, lbool only_last, lbool to_newline)
{
	int nlines = 0;
	lbool do_repaint;
	lbool newline;

	squish_check();
	do_repaint = (n > get_back_scroll() || (only_last && n > sc_height-1) || header_lines > 0);

	while (--n >= 0)
	{
		/*
		 * Get the previous line of input.
		 */
		pos = back_line(pos, &newline);
		if (to_newline && !newline)
			++n;
		if (pos == NULL_POSITION)
		{
			/*
			 * Beginning of file: stop here unless "force" is true.
			 */
			if (!force)
				break;
		}
		if (pos != after_header_pos(pos))
		{
			/* 
			 * Don't allow scrolling back to before the current header line.
			 */
			break;
		}
		/*
		 * Add the position of the previous line to the position table.
		 * Display the line on the screen.
		 */
		add_back_pos(pos);
		nlines++;
		if (!do_repaint)
		{
			home();
			add_line();
			put_line(FALSE);
			if (stop_on_form_feed && line_is_ff())
				break;
		}
	}
	if (nlines == 0)
		eof_bell();
	else if (do_repaint)
		repaint();
	else
	{
		overlay_header();
		lower_left();
	}
	(void) currline(BOTTOM);
}

/*
 * Display n more lines, forward.
 * Start just after the line currently displayed at the bottom of the screen.
 */
public void forward(int n, lbool force, lbool only_last, lbool to_newline)
{
	POSITION pos;

	if (get_quit_at_eof() && eof_displayed(FALSE) && !(ch_getflags() & CH_HELPFILE))
	{
		/*
		 * If the -e flag is set and we're trying to go
		 * forward from end-of-file, go on to the next file.
		 */
		if (edit_next(1))
			quit(QUIT_OK);
		return;
	}

	pos = position(BOTTOM_PLUS_ONE);
	if (pos == NULL_POSITION && (!force || empty_lines(2, sc_height-1)))
	{
		if (ignore_eoi)
		{
			/*
			 * ignore_eoi is to support A_F_FOREVER.
			 * Back up until there is a line at the bottom
			 * of the screen.
			 */
			if (empty_screen())
				pos = ch_zero();
			else
			{
				do
				{
					back(1, position(TOP), TRUE, FALSE, FALSE);
					pos = position(BOTTOM_PLUS_ONE);
				} while (pos == NULL_POSITION && !ABORT_SIGS());
			}
		} else
		{
			eof_bell();
			return;
		}
	}
	forw(n, pos, force, only_last, to_newline, 0);
}

/*
 * Display n more lines, backward.
 * Start just before the line currently displayed at the top of the screen.
 */
public void backward(int n, lbool force, lbool only_last, lbool to_newline)
{
	POSITION pos;

	pos = position(TOP);
	if (pos == NULL_POSITION && (!force || position(BOTTOM) == 0))
	{
		eof_bell();
		return;
	}
	back(n, pos, force, only_last, to_newline);
}

/*
 * Get the backwards scroll limit.
 * Must call this function instead of just using the value of
 * back_scroll, because the default case depends on sc_height and
 * top_scroll, as well as back_scroll.
 */
public int get_back_scroll(void)
{
	if (no_back_scroll)
		return (0);
	if (back_scroll >= 0)
		return (back_scroll);
	if (top_scroll)
		return (sc_height - 2);
	return (10000); /* infinity */
}

/*
 * Will the entire file fit on one screen?
 */
public lbool get_one_screen(void)
{
	int nlines;
	POSITION pos = ch_zero();
	lbool ret = FALSE;

	/* Disable polling until we know whether we will exit early due to -F. */
	getting_one_screen = TRUE;
	for (nlines = 0;  nlines + shell_lines <= sc_height;  nlines++)
	{
		pos = forw_line(pos, NULL, NULL);
		if (ABORT_SIGS())
			break;
		if (pos == NULL_POSITION)
		{
			ret = TRUE;
			break;
		}
	}
	getting_one_screen = FALSE;
	return ret;
}
