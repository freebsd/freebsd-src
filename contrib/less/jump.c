/*
 * Copyright (C) 1984-2002  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Routines which jump to a new location in the file.
 */

#include "less.h"
#include "position.h"

extern int hit_eof;
extern int jump_sline;
extern int squished;
extern int screen_trashed;
extern int sc_width, sc_height;
extern int show_attn;

/*
 * Jump to the end of the file.
 */
	public void
jump_forw()
{
	POSITION pos;

	if (ch_end_seek())
	{
		error("Cannot seek to end of file", NULL_PARG);
		return;
	}
	/*
	 * Position the last line in the file at the last screen line.
	 * Go back one line from the end of the file
	 * to get to the beginning of the last line.
	 */
	pos = back_line(ch_tell());
	if (pos == NULL_POSITION)
		jump_loc((POSITION)0, sc_height-1);
	else
		jump_loc(pos, sc_height-1);
}

/*
 * Jump to line n in the file.
 */
	public void
jump_back(linenum)
	LINENUM linenum;
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
	public void
repaint()
{
	struct scrpos scrpos;
	/*
	 * Start at the line currently at the top of the screen
	 * and redisplay the screen.
	 */
	get_scrpos(&scrpos);
	pos_clear();
	jump_loc(scrpos.pos, scrpos.ln);
}

/*
 * Jump to a specified percentage into the file.
 */
	public void
jump_percent(percent)
	int percent;
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
	pos = percent_pos(len, percent);
	if (pos >= len)
		pos = len-1;

	jump_line_loc(pos, jump_sline);
}

/*
 * Jump to a specified position in the file.
 * Like jump_loc, but the position need not be 
 * the first character in a line.
 */
	public void
jump_line_loc(pos, sline)
	POSITION pos;
	int sline;
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

/*
 * Jump to a specified position in the file.
 * The position must be the first character in a line.
 * Place the target line on a specified line on the screen.
 */
	public void
jump_loc(pos, sline)
	POSITION pos;
	int sline;
{
	register int nline;
	POSITION tpos;
	POSITION bpos;

	/*
	 * Normalize sline.
	 */
	sline = adjsline(sline);

	if ((nline = onscreen(pos)) >= 0)
	{
		/*
		 * The line is currently displayed.  
		 * Just scroll there.
		 */
		nline -= sline;
		if (nline > 0)
			forw(nline, position(BOTTOM_PLUS_ONE), 1, 0, 0);
		else
			back(-nline, position(TOP), 1, 0);
		if (show_attn)
			repaint_hilite(1);
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
		for (nline = 0;  nline < sline;  nline++)
		{
			if (bpos != NULL_POSITION && pos <= bpos)
			{
				/*
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				forw(sc_height-sline+nline-1, bpos, 1, 0, 0);
				if (show_attn)
					repaint_hilite(1);
				return;
			}
			pos = back_line(pos);
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
		hit_eof = 0;
		squished = 0;
		screen_trashed = 0;
		forw(sc_height-1, pos, 1, 0, sline-nline);
	} else
	{
		/*
		 * The desired line is before the current screen.
		 * Move forward in the file far enough so that we
		 * can call back() and put the desired line at the 
		 * sline-th line on the screen.
		 */
		for (nline = sline;  nline < sc_height - 1;  nline++)
		{
			pos = forw_line(pos);
			if (pos == NULL_POSITION)
			{
				/*
				 * Ran into end of file.
				 * This shouldn't normally happen, 
				 * but may if there is some kind of read error.
				 */
				break;
			}
			if (pos >= tpos)
			{
				/* 
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				back(nline+1, tpos, 1, 0);
				if (show_attn)
					repaint_hilite(1);
				return;
			}
		}
		lastmark();
		clear();
		screen_trashed = 0;
		add_back_pos(pos);
		back(sc_height-1, pos, 1, 0);
	}
}
