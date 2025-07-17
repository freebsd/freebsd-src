/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Routines dealing with the "position" table.
 * This is a table which tells the position (in the input file) of the
 * first char on each currently displayed line.
 *
 * {{ The position table is scrolled by moving all the entries.
 *    Would be better to have a circular table 
 *    and just change a couple of pointers. }}
 */

#include "less.h"
#include "position.h"

static POSITION *table = NULL;  /* The position table */
static int table_size = 0;

extern int sc_width, sc_height;
extern int hshift;
extern int shell_lines;

/*
 * Return the starting file position of a line displayed on the screen.
 * The line may be specified as a line number relative to the top
 * of the screen, but is usually one of these special cases:
 *      the top (first) line on the screen
 *      the second line on the screen
 *      the bottom line on the screen
 *      the line after the bottom line on the screen
 */
public POSITION position(int sindex)
{
	switch (sindex)
	{
	case BOTTOM:
		sindex = sc_height - 2;
		break;
	case BOTTOM_PLUS_ONE:
		sindex = sc_height - 1;
		break;
	case BOTTOM_OFFSET:
		sindex = sc_height - shell_lines;
		break;
	case MIDDLE:
		sindex = (sc_height - 1) / 2;
		break;
	}
	return (table[sindex]);
}

/*
 * Add a new file position to the bottom of the position table.
 */
public void add_forw_pos(POSITION pos, lbool no_scroll)
{
	int i;

	/*
	 * Scroll the position table up.
	 */
	if (!no_scroll)
	{
		for (i = 1;  i < sc_height;  i++)
			table[i-1] = table[i];
	}
	table[sc_height - 1] = pos;
}

/*
 * Add a new file position to the top of the position table.
 */
public void add_back_pos(POSITION pos)
{
	int i;

	/*
	 * Scroll the position table down.
	 */
	for (i = sc_height - 1;  i > 0;  i--)
		table[i] = table[i-1];
	table[0] = pos;
}

/*
 * Initialize the position table, done whenever we clear the screen.
 */
public void pos_clear(void)
{
	int i;

	for (i = 0;  i < sc_height;  i++)
		table[i] = NULL_POSITION;
}

/*
 * Allocate or reallocate the position table.
 */
public void pos_init(void)
{
	struct scrpos scrpos;

	if (sc_height <= table_size)
		return;
	/*
	 * If we already have a table, remember the first line in it
	 * before we free it, so we can copy that line to the new table.
	 */
	if (table != NULL)
	{
		get_scrpos(&scrpos, TOP);
		free((char*)table);
	} else
		scrpos.pos = NULL_POSITION;
	table = (POSITION *) ecalloc((size_t) sc_height, sizeof(POSITION)); /*{{type-issue}}*/
	table_size = sc_height;
	pos_clear();
	if (scrpos.pos != NULL_POSITION)
		table[scrpos.ln-1] = scrpos.pos;
}

/*
 * See if the byte at a specified position is currently on the screen.
 * Check the position table to see if the position falls within its range.
 * Return the position table entry if found, -1 if not.
 */
public int onscreen(POSITION pos)
{
	int i;

	if (pos < table[0])
		return (-1);
	for (i = 1;  i < sc_height;  i++)
		if (pos < table[i])
			return (i-1);
	return (-1);
}

/*
 * See if the entire screen is empty.
 */
public int empty_screen(void)
{
	return (empty_lines(0, sc_height-1));
}

public int empty_lines(int s, int e)
{
	int i;

	for (i = s;  i <= e;  i++)
		if (table[i] != NULL_POSITION && table[i] != 0)
			return (0);
	return (1);
}

/*
 * Get the current screen position.
 * The screen position consists of both a file position and
 * a screen line number where the file position is placed on the screen.
 * Normally the screen line number is 0, but if we are positioned
 * such that the top few lines are empty, we may have to set
 * the screen line to a number > 0.
 */
public void get_scrpos(struct scrpos *scrpos, int where)
{
	int i;
	int dir;
	int last;

	switch (where)
	{
	case TOP:
		i = 0; dir = +1; last = sc_height-2;
		break;
	case BOTTOM: case BOTTOM_PLUS_ONE:
		i = sc_height-2; dir = -1; last = 0;
		break;
	default:
		i = where;
		if (table[i] == NULL_POSITION) {
			scrpos->pos = NULL_POSITION;
			return;
		}
		/* Values of dir and last don't matter after this. */
		break;
	}

	/*
	 * Find the first line on the screen which has something on it,
	 * and return the screen line number and the file position.
	 */
	for (;; i += dir)
	{
		if (table[i] != NULL_POSITION)
		{
			scrpos->ln = i+1;
			scrpos->pos = table[i];
			return;
		}
		if (i == last) break;
	}
	/*
	 * The screen is empty.
	 */
	scrpos->pos = NULL_POSITION;
}

/*
 * Adjust a screen line number to be a simple positive integer
 * in the range { 0 .. sc_height-2 }.
 * (The bottom line, sc_height-1, is reserved for prompts, etc.)
 * The given "sline" may be in the range { 1 .. sc_height-1 }
 * to refer to lines relative to the top of the screen (starting from 1),
 * or it may be in { -1 .. -(sc_height-1) } to refer to lines
 * relative to the bottom of the screen.
 */
public int sindex_from_sline(int sline)
{
	/*
	 * Negative screen line number means
	 * relative to the bottom of the screen.
	 */
	if (sline < 0)
		sline += sc_height;
	/*
	 * Can't be less than 1 or greater than sc_height.
	 */
	if (sline <= 0)
		sline = 1;
	if (sline > sc_height)
		sline = sc_height;
	/*
	 * Return zero-based line number, not one-based.
	 */
	return (sline-1);
}

/*
 * Given a line that starts at linepos,
 * and the character at byte offset choff into that line,
 * return the number of characters (not bytes) between the
 * beginning of the line and the first byte of the choff character.
 */
static int pos_shift(POSITION linepos, size_t choff)
{
	constant char *line;
	size_t line_len;
	POSITION pos;
	int cvt_ops;
	char *cline;

	pos = forw_raw_line_len(linepos, choff, &line, &line_len);
	if (pos == NULL_POSITION || line_len != choff)
		return -1;
	cvt_ops = get_cvt_ops(0); /* {{ Passing 0 ignores SRCH_NO_REGEX; does it matter? }} */
	/* {{ It would be nice to be able to call cvt_text with dst=NULL, to avoid need to alloc a useless cline. }} */
	cline = (char *) ecalloc(1, cvt_length(line_len, cvt_ops));
	cvt_text(cline, line, NULL, &line_len, cvt_ops);
	free(cline);
	return (int) line_len;  /*{{type-issue}}*/
}

/*
 * Return the position of the first char of the line containing tpos.
 * Thus if tpos is the first char of its line, just return tpos.
 */
static POSITION beginning_of_line(POSITION tpos)
{
	ch_seek(tpos);
	while (ch_tell() != ch_zero())
	{
		int ch = ch_back_get();
		if (ch == '\n')
		{
			(void) ch_forw_get();
			break;
		}
	}
	return ch_tell();
}

/*
 * When viewing long lines, it may be that the first char in the top screen
 * line is not the first char in its (file) line (the table is "beheaded").
 * This function sets that entry to the position of the first char in the line,
 * and sets hshift so that the first char in the first line is unchanged.
 */
public void pos_rehead(void)
{
	POSITION linepos;
	POSITION tpos = table[TOP];
	if (tpos == NULL_POSITION)
		return;
	linepos = beginning_of_line(tpos);
	if (linepos == tpos)
		return;
	table[TOP] = linepos;
	hshift = pos_shift(linepos, (size_t) (tpos - linepos));
	screen_trashed();
}
