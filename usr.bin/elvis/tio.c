/* tio.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains terminal I/O functions */

#include "config.h"
#include "vi.h"
#include "ctype.h"

static int showmsg P_((void));

/* This function reads in a line from the terminal. */
int vgets(prompt, buf, bsize)
	int	prompt;	/* the prompt character, or '\0' for none */
	char	*buf;	/* buffer into which the string is read */
	int	bsize;	/* size of the buffer */
{
	int	len;	/* how much we've read so far */
	int	ch;	/* a character from the user */
	int	quoted;	/* is the next char quoted? */
	int	tab;	/* column position of cursor */
	char	widths[132];	/* widths of characters */
	int	word;	/* index of first letter of word */
#ifndef NO_DIGRAPH
	int	erased;	/* 0, or first char of a digraph */
#endif

	/* show the prompt */
	move(LINES - 1, 0);
	tab = 0;
	if (prompt)
	{
		addch(prompt);
		tab = 1;
	}
	clrtoeol();
	refresh();

	/* read in the line */
#ifndef NO_DIGRAPH
	erased =
#endif
	quoted = len = 0;
	for (;;)
	{
#ifndef NO_ABBR
		if (quoted || mode == MODE_EX)
		{
			ch = getkey(0);
		}
		else
		{
			/* maybe expand an abbreviation while getting key */
			for (word = len; --word >= 0 && !isspace(buf[word]); )
			{
			}
			word++;
			ch = getabkey(WHEN_EX, &buf[word], len - word);
		}
#else
		ch = getkey(0);
#endif
#ifndef NO_EXTENSIONS
		if (ch == ctrl('O'))
		{
			ch = getkey(quoted ? 0 : WHEN_EX);
		}
#endif

		/* some special conversions */
#if 0
		if (ch == ctrl('D') && len == 0)
			ch = ctrl('[');
#endif
#ifndef NO_DIGRAPH
		if (*o_digraph && erased != 0 && ch != '\b')
		{
			ch = digraph(erased, ch);
			erased = 0;
		}
#endif

		/* inhibit detection of special chars (except ^J) after a ^V */
		if (quoted && ch != '\n')
		{
			ch |= 256;
		}

		/* process the character */
		switch(ch)
		{
		  case ctrl('V'):
			qaddch('^');
			qaddch('\b');
			quoted = TRUE;
			break;

		  case ctrl('D'):
			return -1;

		  case ctrl('['):
		  case '\n':
#if OSK
		  case '\l':
#else
		  case '\r':
#endif
			clrtoeol();
			goto BreakBreak;

#ifndef CRUNCH
		  case ctrl('U'):
			while (len > 0)
			{
				len--;
				while (widths[len]-- > 0)
				{
					qaddch('\b');
					qaddch(' ');
					qaddch('\b');
				}
			}
			break;
#endif

		  case '\b':
			if (len > 0)
			{
				len--;
#ifndef NO_DIGRAPH
				erased = buf[len];
#endif
				for (ch = widths[len]; ch > 0; ch--)
					addch('\b');
				if (mode == MODE_EX)
				{
					clrtoeol();
				}
				tab -= widths[len];
			}
			else
			{
				return -1;
			}
			break;

		  default:
			/* strip off quotation bit */
			if (ch & 256)
			{
				ch &= ~256;
				qaddch(' ');
				qaddch('\b');
			}

			/* add & echo the char */
			if (len < bsize - 1)
			{
				if (ch == '\t' && !quoted)
				{
					widths[len] = *o_tabstop - (tab % *o_tabstop);
					addstr("        " + 8 - widths[len]);
					tab += widths[len];
				}
				else if (ch > 0 && ch < ' ') /* > 0 by GB */
				{
					addch('^');
					addch(ch + '@');
					widths[len] = 2;
					tab += 2;
				}
				else if (ch == '\177')
				{
					addch('^');
					addch('?');
					widths[len] = 2;
					tab += 2;
				}
				else
				{
					addch(ch);
					widths[len] = 1;
					tab++;
				}
				buf[len++] = ch;
			}
			else
			{
				beep();
			}
			quoted = FALSE;
		}
	}
BreakBreak:
	refresh();
	buf[len] = '\0';
	return len;
}


static int	manymsgs; /* This variable keeps msgs from overwriting each other */
static char	pmsg[80]; /* previous message (waiting to be displayed) */


static int showmsg()
{
	/* if there is no message to show, then don't */
	if (!manymsgs)
		return FALSE;

	/* display the message */
	move(LINES - 1, 0);
	if (*pmsg)
	{
		standout();
		qaddch(' ');
		qaddstr(pmsg);
		qaddch(' ');
		standend();
	}
	clrtoeol();

	manymsgs = FALSE;
	return TRUE;
}


void endmsgs()
{
	if (manymsgs)
	{
		showmsg();
		addch('\n');
	}
}

/* Write a message in an appropriate way.  This should really be a varargs
 * function, but there is no such thing as vwprintw.  Hack!!!
 *
 * In MODE_EX or MODE_COLON, the message is written immediately, with a
 * newline at the end.
 *
 * In MODE_VI, the message is stored in a character buffer.  It is not
 * displayed until getkey() is called.  msg() will call getkey() itself,
 * if necessary, to prevent messages from being lost.
 *
 * msg("")		- clears the message line
 * msg("%s %d", ...)	- does a printf onto the message line
 */
#ifdef	__STDC__
void msg (char *fmt, ...)
{
	va_list	ap;
	va_start (ap, fmt);
#else
void msg(fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
	char	*fmt;
	long	arg1, arg2, arg3, arg4, arg5, arg6, arg7;
{
#endif
	if (mode != MODE_VI)
	{
#ifdef	__STDC__
		vsprintf (pmsg, fmt, ap);
#else
		sprintf(pmsg, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
#endif
		qaddstr(pmsg);
		addch('\n');
		exrefresh();
	}
	else
	{
		/* wait for keypress between consecutive msgs */
		if (manymsgs)
		{
			getkey(WHEN_MSG);
		}

		/* real message */
#ifdef	__STDC__
		vsprintf (pmsg, fmt, ap);
#else
		sprintf(pmsg, fmt, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
#endif
		if (*fmt)
		{
			manymsgs = TRUE;
		}
	}
#ifdef	__STDC__
	va_end (ap);
#endif
}


/* This function calls refresh() if the option exrefresh is set */
void exrefresh()
{
	char	*scan;

	/* If this ex command wrote ANYTHING set exwrote so vi's  :  command
	 * can tell that it must wait for a user keystroke before redrawing.
	 */
	for (scan=kbuf; scan<stdscr; scan++)
		if (*scan == '\n')
			exwrote = TRUE;

	/* now we do the refresh thing */
	if (*o_exrefresh)
	{
		refresh();
	}
	else
	{
		wqrefresh();
	}
	if (mode != MODE_VI)
	{
		manymsgs = FALSE;
	}
}


/* This structure is used to store maps and abbreviations.  The distinction
 * between them is that maps are stored in the list referenced by the "maps"
 * pointer, while abbreviations are referenced by the "abbrs" pointer.
 */
typedef struct _map
{
	struct _map	*next;	/* another abbreviation */
	short		len;	/* length of the "rawin" characters */
	short		flags;	/* various flags */
	char		*label;	/* label of the map/abbr, or NULL */
	char		*rawin;	/* the "rawin" characters */
	char		*cooked;/* the "cooked" characters */
} MAP;

static char	keybuf[KEYBUFSIZE];
static int	cend;	/* end of input characters */
static int	user;	/* from user through end are chars typed by user */
static int	next;	/* index of the next character to be returned */
static MAP	*match;	/* the matching map, found by countmatch() */
static MAP	*maps;	/* the map table */
#ifndef NO_ABBR
static MAP	*abbrs;	/* the abbreviation table */
#endif



/* ring the terminal's bell */
void beep()
{
	/* do a visible/audible bell */
	if (*o_flash)
	{
		do_VB();
		refresh();
	}
	else if (*o_errorbells)
	{
		tputs("\007", 1, faddch);
	}

	/* discard any buffered input, and abort macros */
	next = user = cend;
}



/* This function replaces a "rawin" character sequence with the "cooked" version,
 * by modifying the internal type-ahead buffer.
 */
void execmap(rawlen, cookedstr, visual)
	int	rawlen;		/* length of rawin text -- string to delete */
	char	*cookedstr;	/* the cooked text -- string to insert */
	int	visual;		/* boolean -- chars to be executed in visual mode? */
{
	int	cookedlen;
	char	*src, *dst;
	int	i;

	/* find the length of the cooked string */
	cookedlen = strlen(cookedstr);
#ifndef NO_EXTENSIONS
	if (visual)
	{
		cookedlen *= 2;
	}
#endif

	/* if too big to fit in type-ahead buffer, then don't do it */
	if (cookedlen + (cend - next) - rawlen > KEYBUFSIZE)
	{
		return;
	}

	/* shift to make room for cookedstr at the front of keybuf */
	src = &keybuf[next + rawlen];
	dst = &keybuf[cookedlen];
	i = cend - (next + rawlen);
	if (src >= dst)
	{
		while (i-- > 0)
		{
			*dst++ = *src++;
		}
	}
	else
	{
		src += i;
		dst += i;
		while (i-- > 0)
		{
			*--dst = *--src;
		}
	}

	/* insert cookedstr, and adjust offsets */
	cend += cookedlen - rawlen - next;
	user += cookedlen - rawlen - next;
	next = 0;
	for (dst = keybuf, src = cookedstr; *src; )
	{
#ifndef NO_EXTENSIONS
		if (visual)
		{
			*dst++ = ctrl('O');
			cookedlen--;
		}
#endif
		*dst++ = *src++;
	}

#ifdef DEBUG2
	{
#include <stdio.h>
		FILE	*debout;
		int		i;

		debout = fopen("debug.out", "a");
		fprintf(debout, "After execmap(%d, \"%s\", %d)...\n", rawlen, cookedstr, visual);
		for (i = 0; i < cend; i++)
		{
			if (i == next) fprintf(debout, "(next)");
			if (i == user) fprintf(debout, "(user)");
			if (UCHAR(keybuf[i]) < ' ')
				fprintf(debout, "^%c", keybuf[i] ^ '@');
			else
				fprintf(debout, "%c", keybuf[i]);
		}
		fprintf(debout, "(end)\n");
		fclose(debout);
	}
#endif
}

#ifndef NO_CURSORSHAPE
/* made global so that suspend_curses() can reset it.  -nox */
int	oldcurs;
#endif
/* This function calls ttyread().  If necessary, it will also redraw the screen,
 * change the cursor shape, display the mode, and update the ruler.  If the
 * number of characters read is 0, and we didn't time-out, then it exits because
 * we've apparently reached the end of an EX script.
 */
static int fillkeybuf(when, timeout)
	int	when;	/* mixture of WHEN_XXX flags */
	int	timeout;/* timeout in 1/10 second increments, or 0 */
{
	int	nkeys;
#ifndef NO_SHOWMODE
	static int	oldwhen;	/* "when" from last time */
	static int	oldleft;
	static long	oldtop;
	static long	oldnlines;
	char		*str;
#endif

#ifdef DEBUG
	watch();
#endif


#ifndef NO_CURSORSHAPE
	/* make sure the cursor is the right shape */
	if (has_CQ)
	{
		if (when != oldcurs)
		{
			switch (when)
			{
			  case WHEN_EX:		do_CX();	break;
			  case WHEN_VICMD:	do_CV();	break;
			  case WHEN_VIINP:	do_CI();	break;
			  case WHEN_VIREP:	do_CR();	break;
			}
			oldcurs = when;
		}
	}
#endif

#ifndef NO_SHOWMODE
	/* if "showmode" then say which mode we're in */
	if (*o_smd && (when & WHENMASK))
	{
		/* redraw the screen before we check to see whether the
		 * "showmode" message needs to be redrawn.
		 */
		redraw(cursor, !(when & WHEN_VICMD));

		/* now the "topline" test should be valid */
		if (when != oldwhen
		 || topline != oldtop
		 || leftcol != oldleft
		 || nlines != oldnlines)
		{
			oldwhen = when;
			oldtop = topline;
			oldleft = leftcol;
			oldnlines = nlines;

			if (when & WHEN_VICMD)	    str = "Command";
			else if (when & WHEN_VIINP) str = " Input ";
			else if (when & WHEN_VIREP) str = "Replace";
			else if (when & WHEN_REP1)  str = "Replc 1";
			else if (when & WHEN_CUT)   str = "Buffer ";
			else if (when & WHEN_MARK)  str = " Mark  ";
			else if (when & WHEN_CHAR)  str = "Dest Ch";
			else			    str = (char *)0;

			if (str)
			{
				move(LINES - 1, COLS - 10);
				standout();
				qaddstr(str);
				standend();
			}
		}
	}
#endif

#ifndef NO_EXTENSIONS
	/* maybe display the ruler */
	if (*o_ruler && (when & (WHEN_VICMD|WHEN_VIINP|WHEN_VIREP)))
	{
		char	buf[20];

		redraw(cursor, !(when & WHEN_VICMD));
		pfetch(markline(cursor));
		sprintf(buf, "%7ld,%-4d", markline(cursor), 1 + idx2col(cursor, ptext, when & (WHEN_VIINP|WHEN_VIREP)));
		move(LINES - 1, COLS - 22);
		addstr(buf);
	}
#endif

	/* redraw, so the cursor is in the right place */
	if (when & WHENMASK)
	{
		redraw(cursor, !(when & (WHENMASK & ~(WHEN_VIREP|WHEN_VIINP))));
	}

	/* Okay, now we can finally read the rawin keystrokes */
	refresh();
	nkeys = ttyread(keybuf + cend, sizeof keybuf - cend, timeout);

	/* if nkeys == 0 then we've reached EOF of an ex script. */
	if (nkeys == 0 && timeout == 0)
	{
		tmpabort(TRUE);
		move(LINES - 1, 0);
		clrtoeol();
		refresh();
		endwin();
		exit(exitcode);
	}

	cend += nkeys;
	user += nkeys;
	return nkeys;
}


/* This function counts the number of maps that could match the characters
 * between &keybuf[next] and &keybuf[cend], including incomplete matches.
 * The longest comlete match is remembered via the "match" variable.
 */
static int countmatch(when)
	int	when;	/* mixture of WHEN_XXX flags */
{
	MAP	*map;
	int	count;

	/* clear the "match" variable */
	match = (MAP *)0;

	/* check every map */
	for (count = 0, map = maps; map; map = map->next)
	{
		/* can't match if wrong mode */
		if ((map->flags & when) == 0)
		{
			continue;
		}

		/* would this be a complete match? */
		if (map->len <= cend - next)
		{
			/* Yes, it would be.  Now does it really match? */
			if (!strncmp(map->rawin, &keybuf[next], map->len))
			{
				count++;

				/* if this is the longest complete match,
				 * then remember it.
				 */
				if (!match || match->len < map->len)
				{
					match = map;
				}
			}
		}
		else
		{
			/* No, it wouldn't.  But check for partial match */
			if (!strncmp(map->rawin, &keybuf[next], cend - next))
			{
				/* increment by 2 instead of 1 so that, in the
				 * event that we have a partial match with a
				 * single map, we don't mistakenly assume we
				 * have resolved the map yet.
				 */
				count += 2;
			}
		}
	}
	return count;
}


#ifndef NO_ABBR
/* This function checks to see whether a word is an abbreviation.  If it is,
 * then an appropriate number of backspoace characters is inserted into the
 * type-ahead buffer, followed by the expanded form of the abbreviation.
 */
static void expandabbr(word, wlen)
	char	*word;
	int	wlen;
{
	MAP	*abbr;

	/* if the next character wouldn't end the word, then don't expand */
	if (isalnum(keybuf[next]) || keybuf[next] == ctrl('V') || keybuf[next] == '\b')
	{
		return;
	}

	/* find the abbreviation, if any */
	for (abbr = abbrs;
	     abbr && (abbr->len != wlen || strncmp(abbr->rawin, word, wlen));
	     abbr = abbr->next)
	{
	}

	/* If an abbreviation was found, then expand it by inserting the long
	 * version into the type-ahead buffer, and then inserting (in front of
	 * the long version) enough backspaces to erase to the short version.
	 */
	if (abbr)
	{
		execmap(0, abbr->cooked, FALSE);
		while (wlen > 15)
		{
			execmap(0, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b", FALSE);
			wlen -= 15;
		}
		if (wlen > 0)
		{
			execmap(0, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b" + 15 - wlen, FALSE);
		}
	}
}
#endif


/* This function calls getabkey() without attempting to expand abbreviations */
int getkey(when)
	int	when;	/* mixture of WHEN_XXX flags */
{
	return getabkey(when, "", 0);
}


/* This is it.  This function returns keystrokes one-at-a-time, after mapping
 * and abbreviations have been taken into account.
 */
int getabkey(when, word, wlen)
	int	when;	/* mixture of WHEN_XXX flags */
	char	*word;	/* a word that may need to be expanded as an abbr */
	int	wlen;	/* length of "word" -- since "word" might not have \0 */
{
	int	matches;

	/* if this key is needed for delay between multiple error messages,
	 * then reset the manymsgs flag and abort any mapped key sequence.
	 */
	if (showmsg())
	{
		if (when == WHEN_MSG)
		{
#ifndef CRUNCH
			if (!*o_more)
			{
				refresh();
				return ' ';
			}
#endif
			qaddstr("[More...]");
			refresh();
			execmap(user, "", FALSE);
		}
	}

#ifdef DEBUG
	/* periodically check for screwed up internal tables */
	watch();
#endif

	/* if buffer empty, read some characters without timeout */
	if (next >= cend)
	{
		next = user = cend = 0;
		fillkeybuf(when, 0);
	}

	/* try to map the key, unless already mapped and not ":set noremap" */
	if (next <= user || *o_remap)
	{
		do
		{
			do
			{
				matches = countmatch(when);
			} while (matches > 1 && fillkeybuf(when, *o_keytime) > 0);
			if (matches == 1)
			{
				execmap(match->len, match->cooked,
					(match->flags & WHEN_INMV) != 0 
					 && (when & (WHEN_VIINP|WHEN_VIREP)) != 0);
			}
		} while (*o_remap && matches == 1);
	}

	/* ERASEKEY should always be mapped to '\b'. */
	if (keybuf[next] == ERASEKEY)
	{
		keybuf[next] = '\b';
	}

#ifndef NO_ABBR
	/* try to expand an abbreviation, except in visual command mode */
	if (wlen > 0 && (mode & (WHEN_EX|WHEN_VIINP|WHEN_VIREP)) != 0)
	{
		expandabbr(word, wlen);
	}
#endif

	/* return the next key */
	return keybuf[next++];
}

/* This function maps or unmaps a key */
void mapkey(rawin, cooked, when, name)
	char	*rawin;	/* the input key sequence, before mapping */
	char	*cooked;/* after mapping -- or NULL to remove map */
	int	when;	/* bitmap of when mapping should happen */
	char	*name;	/* name of the key, NULL for no name, "abbr" for abbr */
{
	MAP	**head;	/* head of list of maps or abbreviations */
	MAP	*scan;	/* used for scanning through the list */
	MAP	*prev;	/* used during deletions */

	/* Is this a map or an abbreviation?  Choose the right list. */
#ifndef NO_ABBR
	head = ((!name || strcmp(name, "abbr")) ? &maps : &abbrs);
#else
	head = &maps;
#endif

	/* try to find the map in the list */
	for (scan = *head, prev = (MAP *)0;
	     scan && (strcmp(rawin, scan->rawin) && strcmp(rawin, scan->cooked) ||
		!(scan->flags & when & (WHEN_EX|WHEN_VICMD|WHEN_VIINP|WHEN_VIREP)));
	     prev = scan, scan = scan->next)
	{
	}

	/* trying to map? (not unmap) */
	if (cooked && *cooked)
	{
		/* if map starts with "visual ", then mark it as a visual map */
		if (head == &maps && !strncmp(cooked, "visual ", 7))
		{
			cooked += 7;
			when |= WHEN_INMV;
		}

		/* "visual" maps always work in input mode */
		if (when & WHEN_INMV)
		{
			when |= WHEN_VIINP|WHEN_VIREP|WHEN_POPUP;
		}

		/* if not already in the list, then allocate a new structure */
		if (!scan)
		{
			scan = (MAP *)malloc(sizeof(MAP));
			scan->len = strlen(rawin);
			scan->rawin = malloc((unsigned)(scan->len + 1));
			strcpy(scan->rawin, rawin);
			scan->flags = when;
			scan->label = name;
			if (*head)
			{
				prev->next = scan;
			}
			else
			{
				*head = scan;
			}
			scan->next = (MAP *)0;
		}
		else /* recycle old structure */
		{
			_free_(scan->cooked);
		}
		scan->cooked = malloc((unsigned)(strlen(cooked) + 1));
		strcpy(scan->cooked, cooked);
	}
	else /* unmapping */
	{
		/* if nothing to unmap, then exit silently */
		if (!scan)
		{
			return;
		}

		/* unlink the structure from the list */
		if (prev)
		{
			prev->next = scan->next;
		}
		else
		{
			*head = scan->next;
		}

		/* free it, and the strings that it refers to */
		_free_(scan->rawin);
		_free_(scan->cooked);
		_free_(scan);
	}
}


/* This function returns a printable version of a string.  It uses tmpblk.c */
char *printable(str)
	char	*str;	/* the string to convert */
{
	char	*build;	/* used for building the string */

	for (build = tmpblk.c; *str; str++)
	{
#if AMIGA
		if (*str == '\233')
		{
			*build++ = '<';
			*build++ = 'C';
			*build++ = 'S';
			*build++ = 'I';
			*build++ = '>';
		} else 
#endif
		if (UCHAR(*str) < ' ' || *str == '\177')
		{
			*build++ = '^';
			*build++ = *str ^ '@';
		}
		else
		{
			*build++ = *str;
		}
	}
	*build = '\0';
	return tmpblk.c;
}

/* This function displays the contents of either the map table or the
 * abbreviation table.  User commands call this function as follows:
 *	:map	dumpkey(WHEN_VICMD, FALSE);
 *	:map!	dumpkey(WHEN_VIREP|WHEN_VIINP, FALSE);
 *	:abbr	dumpkey(WHEN_VIINP|WHEN_VIREP, TRUE);
 *	:abbr!	dumpkey(WHEN_EX|WHEN_VIINP|WHEN_VIREP, TRUE);
 */
void dumpkey(when, abbr)
	int	when;	/* WHEN_XXXX of mappings to be dumped */
	int	abbr;	/* boolean: dump abbreviations instead of maps? */
{
	MAP	*scan;
	char	*str;
	int	len;

#ifndef NO_ABBR
	for (scan = (abbr ? abbrs : maps); scan; scan = scan->next)
#else
	for (scan = maps; scan; scan = scan->next)
#endif
	{
		/* skip entries that don't match "when" */
		if ((scan->flags & when) == 0)
		{
			continue;
		}

		/* dump the key label, if any */
		if (!abbr)
		{
			len = 8;
			if (scan->label)
			{
				qaddstr(scan->label);
				len -= strlen(scan->label);
			}
			do
			{
				qaddch(' ');
			} while (len-- > 0);
		}

		/* dump the rawin version */
		str = printable(scan->rawin);
		qaddstr(str);
		len = strlen(str);
		do
		{
			qaddch(' ');
		} while (len++ < 8);
			
		/* dump the mapped version */
#ifndef NO_EXTENSIONS
		if ((scan->flags & WHEN_INMV) && (when & (WHEN_VIINP|WHEN_VIREP)))
		{
			qaddstr("visual ");
		}
#endif
		str = printable(scan->cooked);
		qaddstr(str);
		addch('\n');
		exrefresh();
	}
}

#ifndef NO_MKEXRC

static void safequote(str)
	char	*str;
{
	char	*build;

	build = tmpblk.c + strlen(tmpblk.c);
	while (*str)
	{
		if (*str <= ' ' && *str >= 1 || *str == '|')
		{
			*build++ = ctrl('V');
		}
		*build++ = *str++;
	}
	*build = '\0';
}

/* This function saves the contents of either the map table or the
 * abbreviation table into a file.  Both the "bang" and "no bang" versions
 * are saved.
 *	:map	dumpkey(WHEN_VICMD, FALSE);
 *	:map!	dumpkey(WHEN_VIREP|WHEN_VIINP, FALSE);
 *	:abbr	dumpkey(WHEN_VIINP|WHEN_VIREP, TRUE);
 *	:abbr!	dumpkey(WHEN_EX|WHEN_VIINP|WHEN_VIREP, TRUE);
 */
void
savemaps(fd, abbr)
	int	fd;	/* file descriptor of an open file to write to */
	int	abbr;	/* boolean: do abbr table? (else do map table) */
{
	MAP	*scan;
	int	bang;
	int	when;

# ifndef NO_ABBR
	for (scan = (abbr ? abbrs : maps); scan; scan = scan->next)
# else
	for (scan = maps; scan; scan = scan->next)
# endif
	{
		/* skip maps that have labels, except for function keys */
		if (scan->label && *scan->label != '#')
		{
			continue;
		}

		for (bang = 0; bang < 2; bang++)
		{
			/* decide which "when" flags we want */
# ifndef NO_ABBR
			if (abbr)
				when = (bang ? WHEN_EX|WHEN_VIINP|WHEN_VIREP : WHEN_VIINP|WHEN_VIREP);
			else
# endif
				when = (bang ? WHEN_VIREP|WHEN_VIINP : WHEN_VICMD);

			/* skip entries that don't match "when" */
			if ((scan->flags & when) == 0)
			{
				continue;
			}

			/* write a "map" or "abbr" command name */
# ifndef NO_ABBR
			if (abbr)
				strcpy(tmpblk.c, "abbr");
			else
# endif
				strcpy(tmpblk.c, "map");

			/* maybe write a bang.  Definitely write a space */
			if (bang)
				strcat(tmpblk.c, "! ");
			else
				strcat(tmpblk.c, " ");

			/* write the rawin version */
# ifndef NO_FKEY
			if (scan->label)
				strcat(tmpblk.c, scan->label);
			else
# endif
				safequote(scan->rawin);
			strcat(tmpblk.c, " ");
				
			/* dump the mapped version */
# ifndef NO_EXTENSIONS
			if ((scan->flags & WHEN_INMV) && (when & (WHEN_VIINP|WHEN_VIREP)))
			{
				strcat(tmpblk.c, "visual ");
			}
# endif
			safequote(scan->cooked);
			strcat(tmpblk.c, "\n");
			twrite(fd, tmpblk.c, strlen(tmpblk.c));
		}
	}
}
#endif
