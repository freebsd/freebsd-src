/* move2.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This function contains the movement functions that perform RE searching */

#include "config.h"
#include "vi.h"
#ifdef REGEX
# include <regex.h>
#else
# include "regexp.h"
#endif

extern long	atol();

#ifdef REGEX
static regex_t	*re = NULL;	/* compiled version of the pattern to search for */
#else
static regexp	*re;	/* compiled version of the pattern to search for */
#endif
static		prevsf;	/* boolean: previous search direction was forward? */

#ifndef NO_EXTENSIONS
/*ARGSUSED*/
MARK m_wsrch(word, m, cnt)
	char	*word;	/* the word to search for */
	MARK	m;	/* the starting point */
	int	cnt;	/* ignored */
{
	char	buffer[30];

	/* wrap \< and \> around the word */
	strcpy(buffer, "/\\<");
	strcat(buffer, word);
	strcat(buffer, "\\>");

	/* show the searched-for word on the bottom line */
	move(LINES - 1, 0);
	qaddstr(buffer);
	clrtoeol();
	refresh();

	/* search for the word */
	return m_fsrch(m, buffer);
}
#endif

MARK	m_nsrch(m, cnt, cmd)
	MARK	m;	/* where to start searching */
	long	cnt;	/* number of searches to do */
	int	cmd;	/* command character -- 'n' or 'N' */
{
	int	oldprevsf; /* original value of prevsf, so we can fix any changes */

	DEFAULT(1L);

	/* clear the bottom line.  In particular, we want to loose any
	 * "(wrapped)" notice.
	 */
	move(LINES - 1, 0);
	clrtoeol();

	/* if 'N' command, then invert the "prevsf" variable */
	oldprevsf = prevsf;
	if (cmd == 'N')
	{
		prevsf = !prevsf;
	}

	/* search forward if prevsf -- i.e., if previous search was forward */
	while (--cnt >= 0L && m != MARK_UNSET)
	{
		if (prevsf)
		{
			m = m_fsrch(m, (char *)0);
		}
		else
		{
			m = m_bsrch(m, (char *)0);
		}
	}

	/* restore the old value of prevsf -- if cmd=='N' then it was inverted,
	 * and the m_fsrch() and m_bsrch() functions force it to a (possibly
	 * incorrect) value.  The value of prevsf isn't supposed to be changed
	 * at all here!
	 */
	prevsf = oldprevsf;
	return m;
}


MARK	m_fsrch(m, ptrn)
	MARK	m;	/* where to start searching */
	char	*ptrn;	/* pattern to search for */
{
	long	l;	/* line# of line to be searched */
	char	*line;	/* text of line to be searched */
	int	wrapped;/* boolean: has our search wrapped yet? */
	int	pos;	/* where we are in the line */
#ifdef REGEX
	regex_t *optpat();
	regmatch_t rm[SE_MAX];
	int	n;
#endif
#ifndef CRUNCH
	long	delta = INFINITY;/* line offset, for things like "/foo/+1" */
#endif

	/* remember: "previous search was forward" */
	prevsf = TRUE;

	if (ptrn && *ptrn)
	{
		/* locate the closing '/', if any */
		line = parseptrn(ptrn);
#ifndef CRUNCH
		if (*line)
		{
			delta = atol(line);
		}
#endif
		ptrn++;


#ifdef REGEX
		/* XXX where to free re? */
		re = optpat(ptrn);
#else
		/* free the previous pattern */
		if (re) _free_(re);

		/* compile the pattern */
		re = regcomp(ptrn);
#endif
		if (!re)
		{
			return MARK_UNSET;
		}
	}
	else if (!re)
	{
		msg("No previous expression");
		return MARK_UNSET;
	}

	/* search forward for the pattern */
	pos = markidx(m) + 1;
	pfetch(markline(m));
	if (pos >= plen)
	{
		pos = 0;
		m = (m | (BLKSIZE - 1)) + 1;
	}
	wrapped = FALSE;
	for (l = markline(m); l != markline(m) + 1 || !wrapped; l++)
	{
		/* wrap search */
		if (l > nlines)
		{
			/* if we wrapped once already, then the search failed */
			if (wrapped)
			{
				break;
			}

			/* else maybe we should wrap now? */
			if (*o_wrapscan)
			{
				l = 0;
				wrapped = TRUE;
				continue;
			}
			else
			{
				break;
			}
		}

		/* get this line */
		line = fetchline(l);

		/* check this line */
#ifdef REGEX
		if (!regexec(re, &line[pos], SE_MAX, rm, (pos == 0) ? 0 : REG_NOTBOL))
#else
		if (regexec(re, &line[pos], (pos == 0)))
#endif
		{
			/* match! */
			if (wrapped && *o_warn)
				msg("(wrapped)");
#ifndef CRUNCH
			if (delta != INFINITY)
			{
				l += delta;
				if (l < 1 || l > nlines)
				{
					msg("search offset too big");
					return MARK_UNSET;
				}
				force_flags = LNMD|INCL;
				return MARK_AT_LINE(l);
			}
#endif
#ifdef REGEX
			return MARK_AT_LINE(l) + pos + rm[0].rm_so;
#else
			return MARK_AT_LINE(l) + (int)(re->startp[0] - line);
#endif
		}
		pos = 0;
	}

	/* not found */
	msg(*o_wrapscan ? "Not found" : "Hit bottom without finding RE");
	return MARK_UNSET;
}

MARK	m_bsrch(m, ptrn)
	MARK	m;	/* where to start searching */
	char	*ptrn;	/* pattern to search for */
{
	long	l;	/* line# of line to be searched */
	char	*line;	/* text of line to be searched */
	int	wrapped;/* boolean: has our search wrapped yet? */
	int	pos;	/* last acceptable idx for a match on this line */
	int	last;	/* remembered idx of the last acceptable match on this line */
	int	try;	/* an idx at which we strat searching for another match */
#ifdef REGEX
	regex_t *optpat();
	regmatch_t rm[SE_MAX];
	int	n;
#endif
#ifndef CRUNCH
	long	delta = INFINITY;/* line offset, for things like "/foo/+1" */
#endif

	/* remember: "previous search was not forward" */
	prevsf = FALSE;

	if (ptrn && *ptrn)
	{
		/* locate the closing '?', if any */
		line = parseptrn(ptrn);
#ifndef CRUNCH
		if (*line)
		{
			delta = atol(line);
		}
#endif
		ptrn++;

#ifdef REGEX
		/* XXX where to free re? */
		re = optpat(ptrn);
#else
		/* free the previous pattern, if any */
		if (re) _free_(re);

		/* compile the pattern */
		re = regcomp(ptrn);
#endif
		if (!re)
		{
			return MARK_UNSET;
		}
	}
	else if (!re)
	{
		msg("No previous expression");
		return MARK_UNSET;
	}

	/* search backward for the pattern */
	pos = markidx(m);
	wrapped = FALSE;
	for (l = markline(m); l != markline(m) - 1 || !wrapped; l--)
	{
		/* wrap search */
		if (l < 1)
		{
			if (*o_wrapscan)
			{
				l = nlines + 1;
				wrapped = TRUE;
				continue;
			}
			else
			{
				break;
			}
		}

		/* get this line */
		line = fetchline(l);

		/* check this line */
#ifdef REGEX
		if (!regexec(re, line, SE_MAX, rm, 0) && rm[0].rm_so < pos)
#else
		if (regexec(re, line, 1) && (int)(re->startp[0] - line) < pos)
#endif
		{
			try = 0;
			/* match!  now find the last acceptable one in this line */
			do
			{
#ifdef REGEX
				last = try + rm[0].rm_so;
				try += rm[0].rm_eo;
#else
				last = (int)(re->startp[0] - line);
				try = (int)(re->endp[0] - line);
#endif
			} while (try > 0
#ifdef REGEX
				 && !regexec(re, &line[try], SE_MAX, rm, REG_NOTBOL)
				 && try + rm[0].rm_so < pos);
#else
				 && regexec(re, &line[try], FALSE)
				 && (int)(re->startp[0] - line) < pos);
#endif

			if (wrapped && *o_warn)
				msg("(wrapped)");
#ifndef CRUNCH
			if (delta != INFINITY)
			{
				l += delta;
				if (l < 1 || l > nlines)
				{
					msg("search offset too big");
					return MARK_UNSET;
				}
				force_flags = LNMD|INCL;
				return MARK_AT_LINE(l);
			}
#endif
			return MARK_AT_LINE(l) + last;
		}
		pos = BLKSIZE;
	}

	/* not found */
	msg(*o_wrapscan ? "Not found" : "Hit top without finding RE");
	return MARK_UNSET;
}

