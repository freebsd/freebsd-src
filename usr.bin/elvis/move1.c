/* move1.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains most movement functions */

#include "config.h"
#include "vi.h"
#include "ctype.h"

MARK	m_updnto(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* the command character */
{
	DEFAULT(cmd == 'G' ? nlines : 1L);

	/* move up or down 'cnt' lines */
	switch (cmd)
	{
	  case ctrl('P'):
	  case '-':
	  case 'k':
		m -= MARK_AT_LINE(cnt);
		break;

	  case 'G':
		if (cnt < 1L || cnt > nlines)
		{
			msg("Only %ld lines", nlines);
			return MARK_UNSET;
		}
		m = MARK_AT_LINE(cnt);
		break;

	  case '_':
		cnt--;
		/* fall through... */

	  default:
		m += MARK_AT_LINE(cnt);
	}

	/* if that left us screwed up, then fail */
	if (m < MARK_FIRST || markline(m) > nlines)
	{
		return MARK_UNSET;
	}

	return m;
}

/*ARGSUSED*/
MARK	m_right(m, cnt, key, prevkey)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	key;	/* movement keystroke */
	int	prevkey;/* operator keystroke, or 0 if none */
{
	int		idx;	/* index of the new cursor position */

	DEFAULT(1);

	/* If used with an operator, then move 1 less character, since the 'l'
	 * command includes the character that it moves onto.
	 */
	if (prevkey != '\0')
	{
		cnt--;
	}

	/* move to right, if that's OK */
	pfetch(markline(m));
	idx = markidx(m) + cnt;
	if (idx < plen)
	{
		m += cnt;
	}
	else
	{
		return MARK_UNSET;
	}

	return m;
}

/*ARGSUSED*/
MARK	m_left(m, cnt)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
{
	DEFAULT(1);

	/* move to the left, if that's OK */
	if (markidx(m) >= cnt)
	{
		m -= cnt;
	}
	else
	{
		return MARK_UNSET;
	}

	return m;
}

/*ARGSUSED*/
MARK	m_tocol(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either ctrl('X') or '|' */
{
	char	*text;	/* text of the line */
	int	col;	/* column number */
	int	idx;	/* index into the line */


	/* if doing ^X, then adjust for sideways scrolling */
	if (cmd == ctrl('X'))
	{
		DEFAULT(*o_columns & 0xff);
		cnt += leftcol;
	}
	else
	{
		DEFAULT(1);
	}

	/* internally, columns are numbered 0..COLS-1, not 1..COLS */
	cnt--;

	/* if 0, that's easy */
	if (cnt == 0)
	{
		m &= ~(BLKSIZE - 1);
		return m;
	}

	/* find that column within the line */
	pfetch(markline(m));
	text = ptext;
	for (col = idx = 0; col < cnt && *text; text++, idx++)
	{
		if (*text == '\t' && !*o_list)
		{
			col += *o_tabstop;
			col -= col % *o_tabstop;
		}
		else if (UCHAR(*text) < ' ' || *text == '\177')
		{
			col += 2;
		}
#ifndef NO_CHARATTR
		else if (text[0] == '\\' && text[1] == 'f' && text[2] && *o_charattr)
		{
			text += 2; /* plus one more as part of for loop */
		}
#endif
		else
		{
			col++;
		}
	}
	if (!*text)
	{
		/* the desired column was past the end of the line, so
		 * act like the user pressed "$" instead.
		 */
		return m | (BLKSIZE - 1);
	}
	else
	{
		m = (m & ~(BLKSIZE - 1)) + idx;
	}
	return m;
}

/*ARGSUSED*/
MARK	m_front(m, cnt)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument (ignored) */
{
	char	*scan;

	/* move to the first non-whitespace character */
	pfetch(markline(m));
	scan = ptext;
	m &= ~(BLKSIZE - 1);
	while (*scan == ' ' || *scan == '\t')
	{
		scan++;
		m++;
	}

	return m;
}

/*ARGSUSED*/
MARK	m_rear(m, cnt)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument (ignored) */
{
	/* Try to move *EXTREMELY* far to the right.  It is fervently hoped
	 * that other code will convert this to a more reasonable MARK before
	 * anything tries to actually use it.  (See adjmove() in vi.c)
	 */
	return m | (BLKSIZE - 1);
}

#ifndef NO_SENTENCE
static int isperiod(ptr)
	char	*ptr;	/* pointer to possible sentence-ender */
{
	/* if not '.', '?', or '!', then it isn't a sentence ender */
	if (*ptr != '.' && *ptr != '?' && *ptr != '!')
	{
		return FALSE;
	}

	/* skip any intervening ')', ']', or '"' characters */
	do
	{
		ptr++;
	} while (*ptr == ')' || *ptr == ']' || *ptr == '"');

	/* do we have two spaces or EOL? */
	if (!*ptr || ptr[0] == ' ' && ptr[1] == ' ')
	{
		return TRUE;
	}
	return FALSE;
}

/*ARGSUSED*/
MARK	m_sentence(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either '(' or ')' */
{
	REG char	*text;
	REG long	l;
#ifndef CRUNCH
	/* figure out where the paragraph boundary is */
	MARK		pp = m_paragraph(m, 1L, cmd=='(' ? '{' : '}');
#endif

	DEFAULT(1);

	/* If '(' command, then move back one word, so that if we hit '(' at
	 * the start of a sentence we don't simply stop at the end of the
	 * previous sentence and bounce back to the start of this one again.
	 */
	if (cmd == '(')
	{
		m = m_bword(m, 1L, 'b');
		if (!m)
		{
			return m;
		}
	}

	/* get the current line */
	l = markline(m);
	pfetch(l);
	text = ptext + markidx(m);

	/* for each requested sentence... */
	while (cnt-- > 0)
	{
		/* search forward for one of [.?!] followed by spaces or EOL */
		do
		{
			if (cmd == ')')
			{
				/* move forward, wrap at end of line */
				if (!text[0])
				{
					if (l == nlines)
					{
						goto BreakBreak;
					}
					l++;
					pfetch(l);
					text = ptext;
				}
				else
				{
					text++;
				}
			}
			else
			{
				/* move backward, wrap at beginning of line */
				if (text == ptext)
				{
					do
					{
						if (l == 1L)
						{
							goto BreakBreak;
						}
						l--;
						pfetch(l);
					} while (!*ptext);
					text = ptext + plen - 1;
				}
				else
				{
					text--;
				}
			}
		} while (!isperiod(text));
	}
BreakBreak:

	/* construct a mark for this location */
	m = buildmark(text);

	/* move forward to the first word of the next sentence */
	m = m_fword(m, 1L, 'w', '\0');
	if (m == MARK_UNSET)
	{
		m = MARK_EOF;
	}

#ifndef CRUNCH
	/* don't cross the paragraph boundary */
	if (pp && ((cmd=='(') ? (m<pp) : (m>pp)))
	{
		m = pp;
	}
#endif

	return m;
}
#endif

MARK	m_paragraph(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either '{' or '}' */
{
	char	*text;	/* text of the current line */
	char	*pscn;	/* used to scan thru value of "paragraphs" option */
	long	l, ol;	/* current line number, original line number */
	int	dir;	/* -1 if we're moving up, or 1 if down */
	char	col0;	/* character to expect in column 0 */
	long	limit;	/* line where searching must stop */
#ifndef NO_SENTENCE
# define SENTENCE(x)	(x)
	char	*list;	/* either o_sections or o_paragraph */
#else
# define SENTENCE(x)
#endif
#ifndef CRUNCH
	MARK	ss;
#endif

	DEFAULT(1);

	/* set the direction, based on the command */
	switch (cmd)
	{
	  case '{':
		dir = -1;
		col0 = '\0';
		SENTENCE(list = o_paragraphs); 
#ifndef CRUNCH
		ss = m_paragraph(m, 1L, '<');
		if (ss)
			limit = markline(ss);
		else
#endif
			limit = 1L;
		break;

	  case '}':
		dir = 1;
		col0 = '\0';
		SENTENCE(list = o_paragraphs); 
#ifndef CRUNCH
		ss = m_paragraph(m, 1L, '>');
		if (ss)
			limit = markline(ss);
		else
#endif
			limit = nlines;
		break;

	  case '[':
		if (getkey(0) != '[')
		{
			return MARK_UNSET;
		}
		/* fall through... */
	  case '<':
		dir = -1;
		col0 = '{';
		SENTENCE(list = o_sections); 
		limit = 1L;
		break;

	  case ']':
		if (getkey(0) != ']')
		{
			return MARK_UNSET;
		}
		/* fall through... */
	  case '>':
		dir = 1;
		col0 = '{';
		SENTENCE(list = o_sections); 
		limit = nlines;
		break;
	}
	ol = l = markline(m);

	/* for each paragraph that we want to travel through... */
	while (l != limit && cnt-- > 0)
	{
		/* skip blank lines between paragraphs */
		while (l != limit && col0 == *(text = fetchline(l)))
		{
			l += dir;
		}

		/* skip non-blank lines that aren't paragraph separators
		 */
		do
		{
#ifndef NO_SENTENCE
			if (*text == '.' && l != ol)
			{
				for (pscn = list; pscn[0] && pscn[1]; pscn += 2)
				{
					if (pscn[0] == text[1] && pscn[1] == text[2])
					{
						pscn = (char *)0;
						goto BreakBreak;
					}
				}
			}
#endif
			l += dir;
		} while (l != limit && col0 != *(text = fetchline(l)));
BreakBreak:	;
	}

	m = MARK_AT_LINE(l);
#ifdef DEBUG2
	debout("m_paragraph() returning %ld.%d\n", markline(m), markidx(m));
#endif
	return m;
}


/*ARGSUSED*/
MARK	m_match(m, cnt)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument (normally 0) */
{
	long	l;
	REG char	*text;
	REG char	match;
	REG char	nest;
	REG int		count;

#ifndef NO_EXTENSIONS
	/* if we're given a number, then treat it as a percentage of the file */
	if (cnt > 0)
	{
		/* make sure it is a reasonable number */
		if (cnt > 100)
		{
			msg("can only be from 1%% to 100%%");
			return MARK_UNSET;
		}

		/* return the appropriate line number */
		l = (nlines - 1L) * cnt / 100L + 1L;
		return MARK_AT_LINE(l);
	}
#endif /* undef NO_EXTENSIONS */

	/* get the current line */
	l = markline(m);
	pfetch(l);
	text = ptext + markidx(m);

	/* search forward within line for one of "[](){}" */
	for (match = '\0'; !match && *text; text++)
	{
		/* tricky way to recognize 'em in ASCII */
		nest = *text;
		if ((nest & 0xdf) == ']' || (nest & 0xdf) == '[')
		{
			match = nest ^ ('[' ^ ']');
		}
		else if ((nest & 0xfe) == '(')
		{
			match = nest ^ ('(' ^ ')');
		}
		else
		{
			match = 0;
		}
	}
	if (!match)
	{
		return MARK_UNSET;
	}
	text--;

	/* search forward or backward for match */
	if (match == '(' || match == '[' || match == '{')
	{
		/* search backward */
		for (count = 1; count > 0; )
		{
			/* wrap at beginning of line */
			if (text == ptext)
			{
				do
				{
					if (l <= 1L)
					{
						return MARK_UNSET;
					}
					l--;
					pfetch(l);
				} while (!*ptext);
				text = ptext + plen - 1;
			}
			else
			{
				text--;
			}

			/* check the char */
			if (*text == match)
				count--;
			else if (*text == nest)
				count++;
		}
	}
	else
	{
		/* search forward */
		for (count = 1; count > 0; )
		{
			/* wrap at end of line */
			if (!*text)
			{
				if (l >= nlines)
				{
					return MARK_UNSET;
				}
				l++;
				pfetch(l);
				text = ptext;
			}
			else
			{
				text++;
			}

			/* check the char */
			if (*text == match)
				count--;
			else if (*text == nest)
				count++;
		}
	}

	/* construct a mark for this place */
	m = buildmark(text);
	return m;
}

/*ARGSUSED*/
MARK	m_tomark(m, cnt, key)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* (ignored) */
	int	key;	/* keystroke - the mark to move to */
{
	/* mark '' is a special case */
	if (key == '\'' || key == '`')
	{
		if (mark[26] == MARK_UNSET)
		{
			return MARK_FIRST;
		}
		else
		{
			return mark[26];
		}
	}

	/* if not a valid mark number, don't move */
	if (key < 'a' || key > 'z')
	{
		return MARK_UNSET;
	}

	/* return the selected mark -- may be MARK_UNSET */
	if (!mark[key - 'a'])
	{
		msg("mark '%c is unset", key);
	}
	return mark[key - 'a'];
}

