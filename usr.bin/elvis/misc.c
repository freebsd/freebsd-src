/* misc.c */

/* Author:
 *	Steve Kirkendall
 *	14407 SW Teal Blvd. #C
 *	Beaverton, OR 97005
 *	kirkenda@cs.pdx.edu
 */


/* This file contains functions which didn't seem happy anywhere else */

#include "config.h"
#include "vi.h"


/* find a particular line & return a pointer to a copy of its text */
char *fetchline(line)
	long	line;	/* line number of the line to fetch */
{
	int		i;
	REG char	*scan;	/* used to search for the line in a BLK */
	long		l;	/* line number counter */
	static BLK	buf;	/* holds ONLY the selected line (as string) */
	REG char	*cpy;	/* used while copying the line */
	static long	nextline;	/* }  These four variables are used */
	static long	chglevel;	/*  } to implement a shortcut when  */
	static char	*nextscan;	/*  } consecutive lines are fetched */
	static long	nextlnum;	/* }                                */

	/* can we do a shortcut? */
	if (changes == chglevel && line == nextline)
	{
		scan = nextscan;
	}
	else
	{
		/* scan lnum[] to determine which block its in */
		for (i = 1; line > lnum[i]; i++)
		{
		}
		nextlnum = lnum[i];

		/* fetch text of the block containing that line */
		scan = blkget(i)->c;

		/* find the line in the block */
		for (l = lnum[i - 1]; ++l < line; )
		{
			while (*scan++ != '\n')
			{
			}
		}
	}

	/* copy it into a block by itself, with no newline */
	for (cpy = buf.c; *scan != '\n'; )
	{
		*cpy++ = *scan++;
	}
	*cpy = '\0';

	/* maybe speed up the next call to fetchline() ? */
	if (line < nextlnum)
	{
		nextline = line + 1;
		chglevel = changes;
		nextscan = scan + 1;
	}
	else
	{
		nextline = 0;
	}

	/* Calls to fetchline() interfere with calls to pfetch().  Make sure
	 * that pfetch() resets itself on its next invocation.
	 */
	pchgs = 0L;

	/* Return a pointer to the line's text */
	return buf.c;
}


/* error message from the regexp code */
void regerr(txt)
	char	*txt;	/* an error message */
{
	msg("RE error: %s", txt);
}

/* This function is equivelent to the pfetch() macro */
void	pfetch(l)
	long	l;	/* line number of line to fetch */
{
	if(l != pline || changes != pchgs)
	{
		pline = (l);
		ptext = fetchline(pline);
		plen = strlen(ptext);
		pchgs = changes;
	}
}
