/* move5.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains the word-oriented movement functions */

#include "config.h"
#include "ctype.h"
#include "vi.h"

MARK	m_fword(m, cnt, cmd, prevkey)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either 'w' or 'W' */
	int	prevkey;/* previous command... if 'c' then exclude whitespace */
{
	REG long	l;
	REG char	*text;
	REG int		i;

	DEFAULT(1);

	l = markline(m);
	pfetch(l);
	text = ptext + markidx(m);

#ifndef CRUNCH
	/* As a special case, "cw" or "cW" on whitespace without a count
	 * treats the single whitespace character under the cursor as a word.
	 */
	if (cnt == 1L && prevkey == 'c' && isspace(*text))
	{
		return m;
	}
#endif

	while (cnt-- > 0) /* yes, ASSIGNMENT! */
	{
		i = *text++;

		if (cmd == 'W')
		{
			/* include any non-whitespace */
			while (i && !isspace(i))
			{
				i = *text++;
			}
		}
		else if (isalnum(i) || i == '_')
		{
			/* include an alphanumeric word */
			while (i && isalnum(i))
			{
				i = *text++;
			}
		}
		else
		{
			/* include contiguous punctuation */
			while (i && !isalnum(i) && !isspace(i))
			{
				i = *text++;
			}
		}

		/* if not part of "cw" or "cW" command... */
		if (prevkey != 'c' || cnt > 0)
		{
			/* include trailing whitespace */
			while (!i || isspace(i))
			{
				/* did we hit the end of this line? */
				if (!i)
				{
					/* "dw" shouldn't delete newline after word */
					if (prevkey && cnt == 0)
					{
						break;
					}

					/* move to next line, if there is one */
					l++;
					if (l > nlines)
					{
						return MARK_UNSET;
					}
					pfetch(l);
					text = ptext;
				}

				i = *text++;
			}
		}
		text--;
	}

	/* if argument to operator, then back off 1 char since "w" and "W"
	 * include the last char in the affected text.
	 */
	if (prevkey)
	{
		text--;
	}

	/* construct a MARK for this place */
	m = buildmark(text);
	return m;
}


MARK	m_bword(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either 'b' or 'B' */
{
	REG long	l;
	REG char	*text;

	DEFAULT(1);

	l = markline(m);
	pfetch(l);
	text = ptext + markidx(m);
	while (cnt-- > 0) /* yes, ASSIGNMENT! */
	{
		text--;

		/* include preceding whitespace */
		while (text < ptext || isspace(*text))
		{
			/* did we hit the end of this line? */
			if (text < ptext)
			{
				/* move to preceding line, if there is one */
				l--;
				if (l <= 0)
				{
					return MARK_UNSET;
				}
				pfetch(l);
				text = ptext + plen - 1;
			}
			else
			{
				text--;
			}
		}

		if (cmd == 'B')
		{
			/* include any non-whitespace */
			while (text >= ptext && !isspace(*text))
			{
				text--;
			}
		}
		else if (isalnum(*text) || *text == '_')
		{
			/* include an alphanumeric word */
			while (text >= ptext && isalnum(*text))
			{
				text--;
			}
		}
		else
		{
			/* include contiguous punctuation */
			while (text >= ptext && !isalnum(*text) && !isspace(*text))
			{
				text--;
			}
		}
		text++;
	}

	/* construct a MARK for this place */
	m = buildmark(text);
	return m;
}

MARK	m_eword(m, cnt, cmd)
	MARK	m;	/* movement is relative to this mark */
	long	cnt;	/* a numeric argument */
	int	cmd;	/* either 'e' or 'E' */
{
	REG long	l;
	REG char	*text;
	REG int		i;

	DEFAULT(1);

	l = markline(m);
	pfetch(l);
	text = ptext + markidx(m);
	while (cnt-- > 0) /* yes, ASSIGNMENT! */
	{
		if (*text)
			text++;
		i = *text++;

		/* include preceding whitespace */
		while (!i || isspace(i))
		{
			/* did we hit the end of this line? */
			if (!i)
			{
				/* move to next line, if there is one */
				l++;
				if (l > nlines)
				{
					return MARK_UNSET;
				}
				pfetch(l);
				text = ptext;
			}

			i = *text++;
		}

		if (cmd == 'E')
		{
			/* include any non-whitespace */
			while (i && !isspace(i))
			{
				i = *text++;
			}
		}
		else if (isalnum(i) || i == '_')
		{
			/* include an alphanumeric word */
			while (i && isalnum(i))
			{
				i = *text++;
			}
		}
		else
		{
			/* include contiguous punctuation */
			while (i && !isalnum(i) && !isspace(i))
			{
				i = *text++;
			}
		}
		text -= 2;
	}

	/* construct a MARK for this place */
	m = buildmark(text);
	return m;
}
