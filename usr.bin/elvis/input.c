/* input.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains the input() function, which implements vi's INPUT mode.
 * It also contains the code that supports digraphs.
 */

#include "config.h"
#include "ctype.h"
#include "vi.h"


#ifndef NO_DIGRAPH
static struct _DIG
{
	struct _DIG	*next;
	char		key1;
	char		key2;
	char		dig;
	char		save;
} *digs;

char digraph(key1, key2)
	int	key1;	/* the underlying character */
	int	key2;	/* the second character */
{
	int		newkey;
	REG struct _DIG	*dp;

	/* if digraphs are disabled, then just return the new char */
	if (!*o_digraph)
	{
		return key2;
	}

	/* remember the new key, so we can return it if this isn't a digraph */
	newkey = key2;

	/* sort key1 and key2, so that their original order won't matter */
	if (key1 > key2)
	{
		key2 = key1;
		key1 = newkey;
	}

	/* scan through the digraph chart */
	for (dp = digs;
	     dp && (dp->key1 != key1 || dp->key2 != key2);
	     dp = dp->next)
	{
	}

	/* if this combination isn't in there, just use the new key */
	if (!dp)
	{
		return newkey;
	}

	/* else use the digraph key */
	return dp->dig;
}

/* this function lists or defines digraphs */
void do_digraph(bang, extra)
	int	bang;
	char	extra[];
{
	int		dig;
	REG struct _DIG	*dp;
	struct _DIG	*prev;
	static int	user_defined = FALSE; /* boolean: are all later digraphs user-defined? */
	char		listbuf[8];

	/* if "extra" is NULL, then we've reached the end of the built-ins */
	if (!extra)
	{
		user_defined = TRUE;
		return;
	}

	/* if no args, then display the existing digraphs */
	if (*extra < ' ')
	{
		listbuf[0] = listbuf[1] = listbuf[2] = listbuf[5] = ' ';
		listbuf[7] = '\0';
		for (dig = 0, dp = digs; dp; dp = dp->next)
		{
			if (dp->save || bang)
			{
				dig += 7;
				if (dig >= COLS)
				{
					addch('\n');
					exrefresh();
					dig = 7;
				}
				listbuf[3] = dp->key1;
				listbuf[4] = dp->key2;
				listbuf[6] = dp->dig;
				qaddstr(listbuf);
			}
		}
		addch('\n');
		exrefresh();
		return;
	}

	/* make sure we have at least two characters */
	if (!extra[1])
	{
		msg("Digraphs must be composed of two characters");
		return;
	}

	/* sort key1 and key2, so that their original order won't matter */
	if (extra[0] > extra[1])
	{
		dig = extra[0];
		extra[0] = extra[1];
		extra[1] = dig;
	}

	/* locate the new digraph character */
	for (dig = 2; extra[dig] == ' ' || extra[dig] == '\t'; dig++)
	{
	}
	dig = extra[dig];
	if (!bang && dig)
	{
		dig |= 0x80;
	}

	/* search for the digraph */
	for (prev = (struct _DIG *)0, dp = digs;
	     dp && (dp->key1 != extra[0] || dp->key2 != extra[1]);
	     prev = dp, dp = dp->next)
	{
	}

	/* deleting the digraph? */
	if (!dig)
	{
		if (!dp)
		{
#ifndef CRUNCH
			msg("%c%c not a digraph", extra[0], extra[1]);
#endif
			return;
		}
		if (prev)
			prev->next = dp->next;
		else
			digs = dp->next;
		_free_(dp);
		return;
	}

	/* if necessary, create a new digraph struct for the new digraph */
	if (dig && !dp)
	{
		dp = (struct _DIG *)malloc(sizeof *dp);
		if (!dp)
		{
			msg("Out of space in the digraph table");
			return;
		}
		if (prev)
			prev->next = dp;
		else
			digs = dp;
		dp->next = (struct _DIG *)0;
	}

	/* assign it the new digraph value */
	dp->key1 = extra[0];
	dp->key2 = extra[1];
	dp->dig = dig;
	dp->save = user_defined;
}

# ifndef NO_MKEXRC
void savedigs(fd)
	int		fd;
{
	static char	buf[] = "digraph! XX Y\n";
	REG struct _DIG	*dp;

	for (dp = digs; dp; dp = dp->next)
	{
		if (dp->save)
		{
			buf[9] = dp->key1;
			buf[10] = dp->key2;
			buf[12] = dp->dig;
			write(fd, buf, (unsigned)14);
		}
	}
}
# endif
#endif


/* This function allows the user to replace an existing (possibly zero-length)
 * chunk of text with typed-in text.  It returns the MARK of the last character
 * that the user typed in.
 */
MARK input(from, to, when, delta)
	MARK	from;	/* where to start inserting text */
	MARK	to;	/* extent of text to delete */
	int	when;	/* either WHEN_VIINP or WHEN_VIREP */
	int	delta;	/* 1 to take indent from lower line, -1 for upper, 0 for none */
{
	char	key[2];	/* key char followed by '\0' char */
	char	*build;	/* used in building a newline+indent string */
	char	*scan;	/* used while looking at the indent chars of a line */
	MARK	m;	/* some place in the text */
#ifndef NO_EXTENSIONS
	int	quit = FALSE;	/* boolean: are we exiting after this? */
	int	inchg;	/* boolean: have we done a "beforedo()" yet? */
#endif

#ifdef DEBUG
	/* if "from" and "to" are reversed, complain */
	if (from > to)
	{
		msg("ERROR: input(%ld:%d, %ld:%d)",
			markline(from), markidx(from),
			markline(to), markidx(to));
		return MARK_UNSET;
	}
#endif

	key[1] = 0;

	/* if we're replacing text with new text, save the old stuff */
	/* (Alas, there is no easy way to save text for replace mode) */
	if (from != to)
	{
		cut(from, to);
	}

	/* if doing a dot command, then reuse the previous text */
	if (doingdot)
	{
		ChangeText
		{
			/* delete the text that's there now */
			if (from != to)
			{
				delete(from, to);
			}

			/* insert the previous text */
			cutname('.');
			cursor = paste(from, FALSE, TRUE) + 1L;
		}
	}
	else /* interactive version */
	{
		/* assume that whoever called this already did a beforedo() */
#ifndef NO_EXTENSIONS
		inchg = TRUE;
#endif

		/* if doing a change within the line... */
		if (from != to && markline(from) == markline(to))
		{
			/* mark the end of the text with a "$" */
			change(to - 1, to, "$");
		}
		else
		{
			/* delete the old text right off */
			if (from != to)
			{
				delete(from, to);
			}
			to = from;
		}

		/* handle autoindent of the first line, maybe */
		cursor = from;
		m = cursor + MARK_AT_LINE(delta);
		if (delta != 0 && *o_autoindent && markidx(m) == 0
		 && markline(m) >= 1L && markline(m) <= nlines)
		{
			/* Only autoindent blank lines. */
			pfetch(markline(cursor));
			if (plen == 0)
			{
				/* Okay, we really want to autoindent */
				pfetch(markline(m));
				for (scan = ptext, build = tmpblk.c;
				     *scan == ' ' || *scan == '\t';
				     )
				{
					*build++ = *scan++;
				}
				if (build > tmpblk.c)
				{
					*build = '\0';
					add(cursor, tmpblk.c);
					cursor += (int)(build - tmpblk.c);
					if (cursor > to)
						to = cursor;
				}
			}
		}

		/* repeatedly add characters from the user */
		for (;;)
		{
			/* Get a character */
			redraw(cursor, TRUE);
#ifdef DEBUG2
			msg("cursor=%ld.%d, to=%ld.%d",
				markline(cursor), markidx(cursor),
				markline(to), markidx(to));
#endif
#ifndef NO_ABBR
			pfetch(markline(cursor));
			build = ptext;
			if (pline == markline(from))
				build += markidx(from);
			for (scan = ptext + markidx(cursor); --scan >= build && !isspace(*scan); )
			{
			}
			scan++;
			key[0] = getabkey(when, scan, (int)(ptext + markidx(cursor) - scan));
#else
			key[0] = getkey(when);
#endif
#ifndef NO_VISIBLE
			if (key[0] != ctrl('O') && V_from != MARK_UNSET)
			{
				msg("Can't modify text during a selection");
				beep();
				continue;
			}
#endif

#ifndef NO_EXTENSIONS
			if (key[0] == ctrl('O'))
			{
				if (inchg)
				{
					if (cursor < to)
					{
						delete(cursor, to);
						redraw(cursor, TRUE);
					}
					afterdo();
					inchg = FALSE;
				}
			}
			else if (key[0] != ctrl('['))
			{
				if (!inchg)
				{
					beforedo(FALSE);
					inchg = TRUE;
				}
			}
#endif

#ifndef CRUNCH
			/* if wrapmargin is set & we're past the
			 * warpmargin, then change the last whitespace
			 * characters on line into a newline
			 */
			if (*o_wrapmargin)
			{
				pfetch(markline(cursor));
				if (plen == idx2col(cursor, ptext, TRUE)
				 && plen > COLS - (*o_wrapmargin & 0xff))
				{
					build = tmpblk.c;
					*build++ = '\n';
					if (*o_autoindent)
					{
						/* figure out indent for next line */
						for (scan = ptext; *scan == ' ' || *scan == '\t'; )
						{
							*build++ = *scan++;
						}
					}
					*build = '\0';

					scan = ptext + plen;
					m = cursor & ~(BLKSIZE - 1);
					while (ptext < scan)
					{
						scan--;
						if (*scan != ' ' && *scan != '\t')
							continue;

						/*break up line, and we do autoindent if needed*/
						change(m + (int)(scan - ptext), m + (int)(scan - ptext) + 1, tmpblk.c);

						/* NOTE: for some reason, MSC 5.10 doesn't
						 * like for these lines to be combined!!!
						 */
						cursor = (cursor & ~(BLKSIZE - 1));
						cursor += BLKSIZE;
						cursor += strlen(tmpblk.c) - 1;
						cursor += plen - (int)(scan - ptext) - 1;

						/*remove trailing spaces on previous line*/
						pfetch(markline(m));
						scan = ptext + plen;
						while (ptext < scan)
						{
							scan--;
							if (*scan != ' ' && *scan != '\t')
								break;
						}
						delete(m + (int)(scan - ptext) + 1, m + plen);

						break;
					}
				}
			}
#endif /* !CRUNCH */

			/* process it */
			switch (*key)
			{
#ifndef NO_EXTENSIONS
			  case ctrl('O'): /* special movement mapped keys */
				*key = getkey(0);
				switch (*key)
				{
				  case 'h':	m = m_left(cursor, 0L);		break;
				  case 'j':
				  case 'k':	m = m_updnto(cursor, 0L, *key);	break;
				  case 'l':	m = cursor + 1;			break;
				  case 'B':
				  case 'b':	m = m_bword(cursor, 0L, *key);	break;
				  case 'W':
				  case 'w':	m = m_fword(cursor, 0L, *key, '\0');	break;
				  case '^':	m = m_front(cursor, 0L);	break;
				  case '$':	m = m_rear(cursor, 0L);		break;
				  case ctrl('B'):
				  case ctrl('F'):
						m = m_scroll(cursor, 0L, *key); break;
				  case 'x':
#ifndef NO_VISIBLE
						if (V_from)
							beep();
						else
#endif
						ChangeText
						{
							m = v_xchar(cursor, 0L, 'x');
						}
						break;
				  case 'i':	m = to = from = cursor;
						when = WHEN_VIINP + WHEN_VIREP - when;
										break;
				  case 'K':
					pfetch(markline(cursor));
					changes++; /* <- after this, we can alter ptext */
					ptext[markidx(cursor)] = 0;
					for (scan = ptext + markidx(cursor) - 1;
					     scan >= ptext && isalnum(*scan);
					     scan--)
					{
					}
					scan++;
					m = (*scan ? v_keyword(scan, cursor, 0L) : cursor);
					break;

# ifndef NO_VISIBLE
				  case 'v':
				  case 'V':
					if (V_from)
						V_from = MARK_UNSET;
					else
						V_from = cursor;
					m = from = to = cursor;
					V_linemd = (*key == 'V');
					break;

				  case 'd':
				  case 'y':
				  case '\\':
					/* do nothing if unmarked */
					if (!V_from)
					{
						msg("You must mark the text first");
						beep();
						break;
					}

					/* "from" must come before "to" */
					if (V_from < cursor)
					{
						from = V_from;
						to = cursor;
					}
					else
					{
						from = cursor;
						to = V_from;
					}

					/* we don't need V_from anymore */
					V_from = MARK_UNSET;

					if (V_linemd)
					{
						/* adjust for line mode */
						from &= ~(BLKSIZE - 1);
						to |= (BLKSIZE - 1);
					}
					else
					{
						/* in character mode, we must
						 * worry about deleting the newline
						 * at the end of the last line
						 */
						pfetch(markline(to));
						if (markidx(to) == plen)
							to |= (BLKSIZE - 1);
					}
					to++;

					switch (*key)
					{
					  case 'y':
						cut(from, to);
						break;

					  case 'd':
						ChangeText
						{
							cut(from, to);
							delete(from, to);
						}
						cursor = from;
						break;

#ifndef NO_POPUP
					  case '\\':
						ChangeText
						{
							cursor = v_popup(from, to);
						}
						break;
#endif
					}
					m = from = to = cursor;
					break;

				  case 'p':
				  case 'P':
					V_from = MARK_UNSET;
					ChangeText
					{
						m = paste(cursor, (*key == 'p'), FALSE);
					}
					break;
# endif /* !NO_VISIBLE */
				  default:	m = MARK_UNSET;
				}

				/* adjust the moved cursor */
				if (m != cursor)
				{
					m = adjmove(cursor, m, (*key == 'j' || *key == 'k' ? NCOL|FINL : FINL));
					if (plen && (*key == '$' || (*key == 'l' && m <= cursor)))
					{
						m++;
					}
				}

				/* if the cursor is reasonable, use it */
				if (m == MARK_UNSET)
				{
					beep();
				}
				else
				{
					from = to = cursor = m;
				}
				break;

			  case ctrl('Z'):
				if (getkey(0) == ctrl('Z'))
				{
					quit = TRUE;
					goto BreakBreak;
				}
				break;
#endif

			  case ctrl('['):
				/* if last line contains only whitespace, then remove whitespace */
				if (*o_autoindent)
				{
					pfetch(markline(cursor));
					for (scan = ptext; isspace(*scan); scan++)
					{
					}
					if (scan > ptext && !*scan)
					{
						cursor &= ~(BLKSIZE - 1L);
						if (to < cursor + plen)
						{
							to = cursor + plen;
						}
					}
				}
				goto BreakBreak;

			  case ctrl('U'):
				if (markline(cursor) == markline(from))
				{
					cursor = from;
				}
				else
				{
					cursor &= ~(BLKSIZE - 1);
				}
				break;

			  case ctrl('D'):
			  case ctrl('T'):
				if (to > cursor)
				{
					delete(cursor, to);
				}
				mark[27] = cursor;
				cmd_shift(cursor, cursor, *key == ctrl('D') ? CMD_SHIFTL : CMD_SHIFTR, TRUE, "");
				if (mark[27])
				{
					cursor = mark[27];
				}
				else
				{
					cursor = m_front(cursor, 0L);
				}
				to = cursor;
				break;

			  case '\b':
				if (cursor <= from)
				{
					beep();
				}
				else if (markidx(cursor) == 0)
				{
					cursor -= BLKSIZE;
					pfetch(markline(cursor));
					cursor += plen;
				}
				else
				{
					cursor--;
				}
				break;

			  case ctrl('W'):
				m = m_bword(cursor, 1L, 'b');
				if (markline(m) == markline(cursor) && m >= from)
				{
					cursor = m;
					if (from > cursor)
					{
						from = cursor;
					}
				}
				else
				{
					beep();
				}
				break;

			  case '\n':
#if OSK
			  case '\l':
#else				  
			  case '\r':
#endif
				build = tmpblk.c;
				*build++ = '\n';
				if (*o_autoindent)
				{
					/* figure out indent for next line */
					pfetch(markline(cursor));
					for (scan = ptext; *scan == ' ' || *scan == '\t'; )
					{
						*build++ = *scan++;
					}

					/* remove indent from this line, if blank */
					if ((int)(scan - ptext) >= markidx(cursor) && plen > 0)
					{
						to = cursor &= ~(BLKSIZE - 1);
						delete(cursor, cursor + (int)(scan - ptext));
					}

#if 0
					/* advance "to" past whitespace at the cursor */
					if (to >= cursor)
					{
						for (scan = ptext + markidx(cursor), to = cursor; *scan == ' ' || *scan == '\t'; scan++, to++)
						{
						}
					}
#endif
				}
				*build = 0;
				if (cursor >= to && when != WHEN_VIREP)
				{
					add(cursor, tmpblk.c);
				}
				else
				{
					change(cursor, to, tmpblk.c);
				}
				redraw(cursor, TRUE);
				to = cursor = (cursor & ~(BLKSIZE - 1))
						+ BLKSIZE
						+ (int)(build - tmpblk.c) - 1;
				break;

			  case ctrl('A'):
			  case ctrl('P'):
				if (cursor < to)
				{
					delete(cursor, to);
				}
				if (*key == ctrl('A'))
				{
					cutname('.');
				}
				m = paste(cursor, FALSE, TRUE);
				if (m != MARK_UNSET)
				{
					to = cursor = m + 1L;
				}
				break;

			  case ctrl('V'):
				if (cursor >= to && when != WHEN_VIREP)
				{
					add(cursor, "^");
				}
				else
				{
					change(cursor, to, "^");
					to = cursor + 1;
				}
				redraw(cursor, TRUE);
				*key = getkey(0);
				if (*key == '\n')
				{
					/* '\n' too hard to handle */
#if OSK
					*key = '\l';
#else
					*key = '\r';
#endif
				}
				change(cursor, cursor + 1, key);
				cursor++;
				if (cursor > to)
				{
					to = cursor;
				}
				break;

			  case ctrl('L'):
			  case ctrl('R'):
				redraw(MARK_UNSET, FALSE);
				break;

			  default:
				if (cursor >= to && when != WHEN_VIREP)
				{
					add(cursor, key);
					cursor++;
					to = cursor;
				}
				else
				{
					pfetch(markline(cursor));
					if (markidx(cursor) == plen)
					{
						add(cursor, key);
					}
					else
					{
#ifndef NO_DIGRAPH
						*key = digraph(ptext[markidx(cursor)], *key);
#endif
						change(cursor, cursor + 1, key);
					}
					cursor++;
				}
#ifndef NO_SHOWMATCH
				/* show matching "({[" if necessary */
				if (*o_showmatch && strchr(")}]", *key))
				{
					redraw(cursor, TRUE);
					m = m_match(cursor - 1, 0L);
					if (markline(m) >= topline
					 && markline(m) <= botline)
					{
						redraw(m, TRUE);
						refresh();
						sleep(1);
					}
				}
#endif
			} /* end switch(*key) */
		} /* end for(;;) */
BreakBreak:;
		/* delete any excess characters */
		if (cursor < to)
		{
#ifndef NO_EXTENSIONS
			/* if we aren't in the middle of a change, start one! */
			if (!inchg)
			{
				beforedo(FALSE);
				inchg = TRUE;
			}
#endif
			delete(cursor, to);
		}

	} /* end if doingdot else */

	/* put the new text into a cut buffer for possible reuse */
	if (!doingdot)
	{
		blksync();
		cutname('.');
		cut(from, cursor);
	}

	/* move to last char that we inputted, unless it was newline */
	if (markidx(cursor) != 0)
	{
		cursor--;
	}
	redraw(cursor, FALSE);

#ifndef NO_EXTENSIONS
	if (quit)
	{
		/* if this is a nested "do", then cut it short */
		abortdo();

		/* exit, unless we can't write out the file */
		cursor = v_xit(cursor, 0L, 'Z');
	}
#endif

	rptlines = 0L;
	return cursor;
}
