/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines which jump to a new location in the file.
 */

#include "less.h"
#include "position.h"

extern int jump_sline;
extern lbool squished;
extern int sc_width, sc_height;
extern int show_attn;
extern int top_scroll;
extern POSITION header_start_pos;

/*
 * Jump to the end of the file.
 */
public void jump_forw(void)
{
	POSITION pos;
	POSITION end_pos;

	if (ch_end_seek())
	{
		error("Cannot seek to end of file", NULL_PARG);
		return;
	}
	end_pos = ch_tell();
	if (position(sc_height-1) == end_pos)
	{
		eof_bell();
		return;
	}
	/* 
	 * Note; lastmark will be called later by jump_loc, but it fails
	 * because the position table has been cleared by pos_clear below.
	 * So call it here before calling pos_clear.
	 */
	lastmark();
	/*
	 * Position the last line in the file at the last screen line.
	 * Go back one line from the end of the file
	 * to get to the beginning of the last line.
	 */
	pos_clear();
	pos = back_line(end_pos, NULL);
	if (pos == NULL_POSITION)
		jump_loc(ch_zero(), sc_height-1);
	else
	{
		jump_loc(pos, sc_height-1);
		if (position(sc_height-1) != end_pos)
			repaint();
	}
}

/*
 * Jump to the last buffered line in the file.
 */
public void jump_forw_buffered(void)
{
	POSITION end;

	if (ch_end_buffer_seek())
	{
		error("Cannot seek to end of buffers", NULL_PARG);
		return;
	}
	end = ch_tell();
	if (end != NULL_POSITION && end > 0)
		jump_line_loc(end-1, sc_height-1);
}

/*
 * Jump to line n in the file.
 */
public void jump_back(LINENUM linenum)
{
	POSITION pos;
	PARG parg;

	/*
	 * Find the position of the specified line.
	 * If we can seek there, just jump to it.
	 * If we can't seek, but we're trying to go to line number 1,
	 * use ch_beg_seek() to get as close as we can.
	 */
	pos = find_pos(linenum);
	if (pos != NULL_POSITION && ch_seek(pos) == 0)
	{
		if (show_attn)
			set_attnpos(pos);
		jump_loc(pos, jump_sline);
	} else if (linenum <= 1 && ch_beg_seek() == 0)
	{
		jump_loc(ch_tell(), jump_sline);
		error("Cannot seek to beginning of file", NULL_PARG);
	} else
	{
		parg.p_linenum = linenum;
		error("Cannot seek to line number %n", &parg);
	}
}

/*
 * Repaint the screen.
 */
public void repaint(void)
{
	struct scrpos scrpos;
	/*
	 * Start at the line currently at the top of the screen
	 * and redisplay the screen.
	 */
	get_scrpos(&scrpos, TOP);
	pos_clear();
	if (scrpos.pos == NULL_POSITION)
		/* Screen hasn't been drawn yet. */
		jump_loc(ch_zero(), 1);
	else
		jump_loc(scrpos.pos, scrpos.ln);
}

/*
 * Jump to a specified percentage into the file.
 */
public void jump_percent(int percent, long fraction)
{
	POSITION pos, len;

	/*
	 * Determine the position in the file
	 * (the specified percentage of the file's length).
	 */
	if ((len = ch_length()) == NULL_POSITION)
	{
		ierror("Determining length of file", NULL_PARG);
		ch_end_seek();
	}
	if ((len = ch_length()) == NULL_POSITION)
	{
		error("Don't know length of file", NULL_PARG);
		return;
	}
	pos = percent_pos(len, percent, fraction);
	if (pos >= len)
		pos = len-1;

	jump_line_loc(pos, jump_sline);
}

/*
 * Jump to a specified position in the file.
 * Like jump_loc, but the position need not be 
 * the first character in a line.
 */
public void jump_line_loc(POSITION pos, int sline)
{
	int c;

	if (ch_seek(pos) == 0)
	{
		/*
		 * Back up to the beginning of the line.
		 */
		while ((c = ch_back_get()) != '\n' && c != EOI)
			;
		if (c == '\n')
			(void) ch_forw_get();
		pos = ch_tell();
	}
	if (show_attn)
		set_attnpos(pos);
	jump_loc(pos, sline);
}

static void after_header_message(void)
{
#if HAVE_TIME
#define MSG_FREQ 1 /* seconds */
    static time_type last_msg = (time_type) 0;
    time_type now = get_time();
    if (now < last_msg + MSG_FREQ)
        return;
    last_msg = now;
#endif
    bell();
    /* {{ This message displays before the file text is updated, which is not a good UX. }} */
    /** error("Cannot display text before header; use --header=- to disable header", NULL_PARG); */
}

/*
 * Ensure that a position is not before the header.
 * If it is, print a message and return the position of the start of the header.
 * {{ This is probably not being used correctly in all cases. 
 *    It does not account for the location of pos on the screen, 
 *    so lines before pos could be displayed. }}
 */
public POSITION after_header_pos(POSITION pos)
{
	if (header_start_pos != NULL_POSITION && pos < header_start_pos)
	{
        after_header_message();
        pos = header_start_pos;
	}
	return pos;
}

/*
 * Jump to a specified position in the file.
 * The position must be the first character in a line.
 * Place the target line on a specified line on the screen.
 */
public void jump_loc(POSITION pos, int sline)
{
	int nline;
	int sindex;
	POSITION tpos;
	POSITION bpos;

	/*
	 * Normalize sline.
	 */
	pos = after_header_pos(pos);
	pos = next_unfiltered(pos);
	sindex = sindex_from_sline(sline);

	if ((nline = onscreen(pos)) >= 0)
	{
		/*
		 * The line is currently displayed.  
		 * Just scroll there.
		 */
		nline -= sindex;
		if (nline > 0)
			forw(nline, position(BOTTOM_PLUS_ONE), TRUE, FALSE, FALSE, 0);
		else
			back(-nline, position(TOP), TRUE, FALSE, FALSE);
#if HILITE_SEARCH
		if (show_attn)
			repaint_hilite(TRUE);
#endif
		return;
	}

	/*
	 * Line is not on screen.
	 * Seek to the desired location.
	 */
	if (ch_seek(pos))
	{
		error("Cannot seek to that file position", NULL_PARG);
		return;
	}

	/*
	 * See if the desired line is before or after 
	 * the currently displayed screen.
	 */
	tpos = position(TOP);
	bpos = position(BOTTOM_PLUS_ONE);
	if (tpos == NULL_POSITION || pos >= tpos)
	{
		/*
		 * The desired line is after the current screen.
		 * Move back in the file far enough so that we can
		 * call forw() and put the desired line at the 
		 * sline-th line on the screen.
		 */
		for (nline = 0;  nline < sindex;  nline++)
		{
			if (bpos != NULL_POSITION && pos <= bpos)
			{
				/*
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				forw(sc_height-sindex+nline-1, bpos, TRUE, FALSE, FALSE, 0);
#if HILITE_SEARCH
				if (show_attn)
					repaint_hilite(TRUE);
#endif
				return;
			}
			pos = back_line(pos, NULL);
			if (pos == NULL_POSITION)
			{
				/*
				 * Oops.  Ran into the beginning of the file.
				 * Exit the loop here and rely on forw()
				 * below to draw the required number of
				 * blank lines at the top of the screen.
				 */
				break;
			}
		}
		lastmark();
		squished = FALSE;
		screen_trashed_num(0);
		forw(sc_height-1, pos, TRUE, FALSE, FALSE, sindex-nline);
	} else
	{
		/*
		 * The desired line is before the current screen.
		 * Move forward in the file far enough so that we
		 * can call back() and put the desired line at the 
		 * sindex-th line on the screen.
		 */
		for (nline = sindex;  nline < sc_height - 1;  nline++)
		{
			POSITION linepos;
			pos = forw_line(pos, &linepos, NULL);
			if (pos == NULL_POSITION)
			{
				/*
				 * Ran into end of file.
				 * This shouldn't normally happen, 
				 * but may if there is some kind of read error.
				 */
				break;
			}
			if (linepos >= tpos)
			{
				/* 
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				back(nline, tpos, TRUE, FALSE, FALSE);
#if HILITE_SEARCH
				if (show_attn)
					repaint_hilite(TRUE);
#endif
				return;
			}
		}
		lastmark();
		if (!top_scroll)
			clear();
		else
			home();
		screen_trashed_num(0);
		add_back_pos(pos);
		back(sc_height-1, pos, TRUE, FALSE, FALSE);
	}
}
