/*
 * Copyright (C) 1984-2000  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information about less, or for information on how to 
 * contact the author, see the README file.
 */


/*
 * Routines to manipulate the "line buffer".
 * The line buffer holds a line of output as it is being built
 * in preparation for output to the screen.
 */

#include "less.h"

#define IS_CONT(c)  (((c) & 0xC0) == 0x80)
#define LINENUM_WIDTH   8       /* Chars to use for line number */

/* Buffer which holds the current output line */
public char linebuf[LINEBUF_SIZE];
public int size_linebuf = sizeof(linebuf);

public int cshift;		/* Current left-shift of output line buffer */
public int hshift;		/* Desired left-shift of output line buffer */

static char attr[LINEBUF_SIZE];	/* Extension of linebuf to hold attributes */
static int curr;		/* Index into linebuf */
static int column;		/* Printable length, accounting for
				   backspaces, etc. */
static int overstrike;		/* Next char should overstrike previous char */
static int is_null_line;	/* There is no current line */
static int lmargin;		/* Left margin */
static char pendc;
static POSITION pendpos;
static char *end_ansi_chars;

static int do_append();

extern int bs_mode;
extern int tabstop;
extern int linenums;
extern int ctldisp;
extern int twiddle;
extern int binattr;
extern int status_col;
extern int auto_wrap, ignaw;
extern int bo_s_width, bo_e_width;
extern int ul_s_width, ul_e_width;
extern int bl_s_width, bl_e_width;
extern int so_s_width, so_e_width;
extern int sc_width, sc_height;
extern int utf_mode;
extern POSITION start_attnpos;
extern POSITION end_attnpos;

/*
 * Initialize from environment variables.
 */
	public void
init_line()
{
	end_ansi_chars = lgetenv("LESSANSIENDCHARS");
	if (end_ansi_chars == NULL || *end_ansi_chars == '\0')
		end_ansi_chars = "m";
}

/*
 * Rewind the line buffer.
 */
	public void
prewind()
{
	curr = 0;
	column = 0;
	overstrike = 0;
	is_null_line = 0;
	pendc = '\0';
	lmargin = 0;
	if (status_col)
		lmargin += 1;
	if (linenums == OPT_ONPLUS)
		lmargin += LINENUM_WIDTH+1;
}

/*
 * Insert the line number (of the given position) into the line buffer.
 */
	public void
plinenum(pos)
	POSITION pos;
{
	register int lno;
	register int i;

	if (linenums == OPT_ONPLUS)
	{
		/*
		 * Get the line number and put it in the current line.
		 * {{ Note: since find_linenum calls forw_raw_line,
		 *    it may seek in the input file, requiring the caller 
		 *    of plinenum to re-seek if necessary. }}
		 * {{ Since forw_raw_line modifies linebuf, we must
		 *    do this first, before storing anything in linebuf. }}
		 */
		lno = find_linenum(pos);
	}

	/*
	 * Display a status column if the -J option is set.
	 */
	if (status_col)
	{
		linebuf[curr] = ' ';
		if (start_attnpos != NULL_POSITION &&
		    pos >= start_attnpos && pos < end_attnpos)
			attr[curr] = AT_STANDOUT;
		else
			attr[curr] = 0;
		curr++;
		column++;
	}
	/*
	 * Display the line number at the start of each line
	 * if the -N option is set.
	 */
	if (linenums == OPT_ONPLUS)
	{
		sprintf(&linebuf[curr], "%*d", LINENUM_WIDTH, lno);
		column += LINENUM_WIDTH;
		for (i = 0;  i < LINENUM_WIDTH;  i++)
			attr[curr++] = 0;
	}
	/*
	 * Append enough spaces to bring us to the lmargin.
	 */
	while (column < lmargin)
	{
		linebuf[curr] = ' ';
		attr[curr++] = AT_NORMAL;
		column++;
	}
}

/*
 *
 */
	static int
utf_len(char *s, int len)
{
	int ulen = 0;

	while (*s != '\0' && len > 0)
	{
		if (!IS_CONT(*s))
			len--;
		s++;
		ulen++;
	}
	while (IS_CONT(*s))
	{
		s++;
		ulen++;
	}
	return (ulen);
}

/*
 * Shift the input line left.
 * This means discarding N printable chars at the start of the buffer.
 */
	static void
pshift(shift)
	int shift;
{
	int i;
	int real_shift;

	if (shift > column - lmargin)
		shift = column - lmargin;
	if (shift > curr - lmargin)
		shift = curr - lmargin;

	if (!utf_mode)
		real_shift = shift;
	else
	{
		real_shift = utf_len(linebuf + lmargin, shift);
		if (real_shift > curr)
			real_shift = curr;
	}
	for (i = 0;  i < curr - real_shift;  i++)
	{
		linebuf[lmargin + i] = linebuf[lmargin + i + real_shift];
		attr[lmargin + i] = attr[lmargin + i + real_shift];
	}
	column -= shift;
	curr -= real_shift;
	cshift += shift;
}

/*
 * Return the printing width of the start (enter) sequence
 * for a given character attribute.
 */
	static int
attr_swidth(a)
	int a;
{
	switch (a)
	{
	case AT_BOLD:		return (bo_s_width);
	case AT_UNDERLINE:	return (ul_s_width);
	case AT_BLINK:		return (bl_s_width);
	case AT_STANDOUT:	return (so_s_width);
	}
	return (0);
}

/*
 * Return the printing width of the end (exit) sequence
 * for a given character attribute.
 */
	static int
attr_ewidth(a)
	int a;
{
	switch (a)
	{
	case AT_BOLD:		return (bo_e_width);
	case AT_UNDERLINE:	return (ul_e_width);
	case AT_BLINK:		return (bl_e_width);
	case AT_STANDOUT:	return (so_e_width);
	}
	return (0);
}

/*
 * Return the printing width of a given character and attribute,
 * if the character were added to the current position in the line buffer.
 * Adding a character with a given attribute may cause an enter or exit
 * attribute sequence to be inserted, so this must be taken into account.
 */
	static int
pwidth(c, a)
	int c;
	int a;
{
	register int w;

	if (utf_mode && IS_CONT(c))
		return (0);

	if (c == '\b')
		/*
		 * Backspace moves backwards one position.
		 */
		return (-1);

	if (control_char(c))
		/*
		 * Control characters do unpredicatable things,
		 * so we don't even try to guess; say it doesn't move.
		 * This can only happen if the -r flag is in effect.
		 */
		return (0);

	/*
	 * Other characters take one space,
	 * plus the width of any attribute enter/exit sequence.
	 */
	w = 1;
	if (curr > 0 && attr[curr-1] != a)
		w += attr_ewidth(attr[curr-1]);
	if (a && (curr == 0 || attr[curr-1] != a))
		w += attr_swidth(a);
	return (w);
}

/*
 * Delete the previous character in the line buffer.
 */
	static void
backc()
{
	curr--;
	column -= pwidth(linebuf[curr], attr[curr]);
}

/*
 * Are we currently within a recognized ANSI escape sequence?
 */
	static int
in_ansi_esc_seq()
{
	int i;

	/*
	 * Search backwards for either an ESC (which means we ARE in a seq);
	 * or an end char (which means we're NOT in a seq).
	 */
	for (i = curr-1;  i >= 0;  i--)
	{
		if (linebuf[i] == ESC)
			return (1);
		if (strchr(end_ansi_chars, linebuf[i]) != NULL)
			return (0);
	}
	return (0);
}

/*
 * Append a character and attribute to the line buffer.
 */
	static int
storec(c, a, pos)
	int c;
	int a;
	POSITION pos;
{
	register int w;

#if HILITE_SEARCH
	if (is_hilited(pos, pos+1, 0))
		/*
		 * This character should be highlighted.
		 * Override the attribute passed in.
		 */
		a = AT_STANDOUT;
#endif
	if (ctldisp == OPT_ONPLUS && in_ansi_esc_seq())
		w = 0;
	else
		w = pwidth(c, a);
	if (ctldisp != OPT_ON && column + w + attr_ewidth(a) > sc_width)
		/*
		 * Won't fit on screen.
		 */
		return (1);

	if (curr >= sizeof(linebuf)-2)
		/*
		 * Won't fit in line buffer.
		 */
		return (1);

	/*
	 * Special handling for "magic cookie" terminals.
	 * If an attribute enter/exit sequence has a printing width > 0,
	 * and the sequence is adjacent to a space, delete the space.
	 * We just mark the space as invisible, to avoid having too
	 * many spaces deleted.
	 * {{ Note that even if the attribute width is > 1, we
	 *    delete only one space.  It's not worth trying to do more.
	 *    It's hardly worth doing this much. }}
	 */
	if (curr > 0 && a != AT_NORMAL && 
		linebuf[curr-1] == ' ' && attr[curr-1] == AT_NORMAL &&
		attr_swidth(a) > 0)
	{
		/*
		 * We are about to append an enter-attribute sequence
		 * just after a space.  Delete the space.
		 */
		attr[curr-1] = AT_INVIS;
		column--;
	} else if (curr > 0 && attr[curr-1] != AT_NORMAL && 
		attr[curr-1] != AT_INVIS && c == ' ' && a == AT_NORMAL &&
		attr_ewidth(attr[curr-1]) > 0)
	{
		/*
		 * We are about to append a space just after an 
		 * exit-attribute sequence.  Delete the space.
		 */
		a = AT_INVIS;
		column--;
	}
	/* End of magic cookie handling. */

	linebuf[curr] = c;
	attr[curr] = a;
	column += w;
	return (0);
}

/*
 * Append a character to the line buffer.
 * Expand tabs into spaces, handle underlining, boldfacing, etc.
 * Returns 0 if ok, 1 if couldn't fit in buffer.
 */
	public int
pappend(c, pos)
	register int c;
	POSITION pos;
{
	int r;

	if (pendc)
	{
		if (do_append(pendc, pendpos))
			/*
			 * Oops.  We've probably lost the char which
			 * was in pendc, since caller won't back up.
			 */
			return (1);
		pendc = '\0';
	}

	if (c == '\r' && bs_mode == BS_SPECIAL)
	{
		/*
		 * Don't put the CR into the buffer until we see 
		 * the next char.  If the next char is a newline,
		 * discard the CR.
		 */
		pendc = c;
		pendpos = pos;
		return (0);
	}

	r = do_append(c, pos);
	/*
	 * If we need to shift the line, do it.
	 * But wait until we get to at least the middle of the screen,
	 * so shifting it doesn't affect the chars we're currently
	 * pappending.  (Bold & underline can get messed up otherwise.)
	 */
	if (cshift < hshift && column > sc_width / 2)
		pshift(hshift - cshift);
	return (r);
}

	static int
do_append(c, pos)
	int c;
	POSITION pos;
{
	register char *s;
	register int a;

#define	STOREC(c,a) \
	if (storec((c),(a),pos)) return (1); else curr++

	if (c == '\b')
	{
		switch (bs_mode)
		{
		case BS_NORMAL:
			STOREC(c, AT_NORMAL);
			break;
		case BS_CONTROL:
			goto do_control_char;
		case BS_SPECIAL:
			if (curr == 0)
				break;
			backc();
			overstrike = 1;
			break;
		}
	} else if (overstrike)
	{
		/*
		 * Overstrike the character at the current position
		 * in the line buffer.  This will cause either 
		 * underline (if a "_" is overstruck), 
		 * bold (if an identical character is overstruck),
		 * or just deletion of the character in the buffer.
		 */
		overstrike = 0;
		if ((char)c == linebuf[curr])
			STOREC(linebuf[curr], AT_BOLD);
		else if (c == '_')
			STOREC(linebuf[curr], AT_UNDERLINE);
		else if (linebuf[curr] == '_')
			STOREC(c, AT_UNDERLINE);
		else if (control_char(c))
			goto do_control_char;
		else
			STOREC(c, AT_NORMAL);
	} else if (c == '\t') 
	{
		/*
		 * Expand a tab into spaces.
		 */
		if (tabstop == 0)
			tabstop = 1;
		switch (bs_mode)
		{
		case BS_CONTROL:
			goto do_control_char;
		case BS_NORMAL:
		case BS_SPECIAL:
			do
			{
				STOREC(' ', AT_NORMAL);
			} while (((column + cshift - lmargin) % tabstop) != 0);
			break;
		}
	} else if (control_char(c))
	{
	do_control_char:
		if (ctldisp == OPT_ON || (ctldisp == OPT_ONPLUS && c == ESC))
		{
			/*
			 * Output as a normal character.
			 */
			STOREC(c, AT_NORMAL);
		} else 
		{
			/*
			 * Convert to printable representation.
			 */
			s = prchar(c);  
			a = binattr;

			/*
			 * Make sure we can get the entire representation
			 * of the character on this line.
			 */
			if (column + (int) strlen(s) + 
			    attr_swidth(a) + attr_ewidth(a) > sc_width)
				return (1);

			for ( ;  *s != 0;  s++)
				STOREC(*s, a);
		}
	} else
	{
		STOREC(c, AT_NORMAL);
	}

	return (0);
}

/*
 * Terminate the line in the line buffer.
 */
	public void
pdone(endline)
	int endline;
{
	if (pendc && (pendc != '\r' || !endline))
		/*
		 * If we had a pending character, put it in the buffer.
		 * But discard a pending CR if we are at end of line
		 * (that is, discard the CR in a CR/LF sequence).
		 */
		(void) do_append(pendc, pendpos);

	/*
	 * Make sure we've shifted the line, if we need to.
	 */
	if (cshift < hshift)
		pshift(hshift - cshift);

	/*
	 * Add a newline if necessary,
	 * and append a '\0' to the end of the line.
	 */
	if (column < sc_width || !auto_wrap || ignaw || ctldisp == OPT_ON)
	{
		linebuf[curr] = '\n';
		attr[curr] = AT_NORMAL;
		curr++;
	}
	linebuf[curr] = '\0';
	attr[curr] = AT_NORMAL;
	/*
	 * If we are done with this line, reset the current shift.
	 */
	if (endline)
		cshift = 0;
}

/*
 * Get a character from the current line.
 * Return the character as the function return value,
 * and the character attribute in *ap.
 */
	public int
gline(i, ap)
	register int i;
	register int *ap;
{
	char *s;
	
	if (is_null_line)
	{
		/*
		 * If there is no current line, we pretend the line is
		 * either "~" or "", depending on the "twiddle" flag.
		 */
		*ap = AT_BOLD;
		s = (twiddle) ? "~\n" : "\n";
		return (s[i]);
	}

	*ap = attr[i];
	return (linebuf[i] & 0377);
}

/*
 * Indicate that there is no current line.
 */
	public void
null_line()
{
	is_null_line = 1;
	cshift = 0;
}

/*
 * Analogous to forw_line(), but deals with "raw lines":
 * lines which are not split for screen width.
 * {{ This is supposed to be more efficient than forw_line(). }}
 */
	public POSITION
forw_raw_line(curr_pos, linep)
	POSITION curr_pos;
	char **linep;
{
	register char *p;
	register int c;
	POSITION new_pos;

	if (curr_pos == NULL_POSITION || ch_seek(curr_pos) ||
		(c = ch_forw_get()) == EOI)
		return (NULL_POSITION);

	p = linebuf;

	for (;;)
	{
		if (c == '\n' || c == EOI)
		{
			new_pos = ch_tell();
			break;
		}
		if (p >= &linebuf[sizeof(linebuf)-1])
		{
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 * {{ The line buffer is supposed to be big
			 *    enough that this never happens. }}
			 */
			new_pos = ch_tell() - 1;
			break;
		}
		*p++ = c;
		c = ch_forw_get();
	}
	*p = '\0';
	if (linep != NULL)
		*linep = linebuf;
	return (new_pos);
}

/*
 * Analogous to back_line(), but deals with "raw lines".
 * {{ This is supposed to be more efficient than back_line(). }}
 */
	public POSITION
back_raw_line(curr_pos, linep)
	POSITION curr_pos;
	char **linep;
{
	register char *p;
	register int c;
	POSITION new_pos;

	if (curr_pos == NULL_POSITION || curr_pos <= ch_zero() ||
		ch_seek(curr_pos-1))
		return (NULL_POSITION);

	p = &linebuf[sizeof(linebuf)];
	*--p = '\0';

	for (;;)
	{
		c = ch_back_get();
		if (c == '\n')
		{
			/*
			 * This is the newline ending the previous line.
			 * We have hit the beginning of the line.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		if (c == EOI)
		{
			/*
			 * We have hit the beginning of the file.
			 * This must be the first line in the file.
			 * This must, of course, be the beginning of the line.
			 */
			new_pos = ch_zero();
			break;
		}
		if (p <= linebuf)
		{
			/*
			 * Overflowed the input buffer.
			 * Pretend the line ended here.
			 */
			new_pos = ch_tell() + 1;
			break;
		}
		*--p = c;
	}
	if (linep != NULL)
		*linep = p;
	return (new_pos);
}
