/* modify.c */

/* This file contains the low-level file modification functions:
 *	delete(frommark, tomark)	- removes line or portions of lines
 *	add(frommark, text)		- inserts new text
 *	change(frommark, tomark, text)	- delete, then add
 */

#include "config.h"
#include "vi.h"

#ifdef DEBUG2
# include <stdio.h>
static FILE *dbg;

/*VARARGS1*/
debout(msg, arg1, arg2, arg3, arg4, arg5)
	char	*msg, *arg1, *arg2, *arg3, *arg4, *arg5;
{
	if (!dbg)
	{
		dbg = fopen("debug.out", "w");
		if (!dbg)
			return;
		setbuf(dbg, (FILE *)0);
	}
	fprintf(dbg, msg, arg1, arg2, arg3, arg4, arg5);
}
#endif /* DEBUG2 */

/* delete a range of text from the file */
void delete(frommark, tomark)
	MARK		frommark;	/* first char to be deleted */
	MARK		tomark;		/* AFTER last char to be deleted */
{
	int		i;		/* used to move thru logical blocks */
	REG char	*scan;		/* used to scan thru text of the blk */
	REG char	*cpy;		/* used when copying chars */
	BLK		*blk;		/* a text block */
	long		l;		/* a line number */
	MARK		m;		/* a traveling version of frommark */

#ifdef DEBUG2
	debout("delete(%ld.%d, %ld.%d)\n", markline(frommark), markidx(frommark), markline(tomark), markidx(tomark));
#endif

	/* if not deleting anything, quit now */
	if (frommark == tomark)
	{
		return;
	}

	/* This is a change */
	changes++;
	significant = TRUE;

	/* supply clues to the redraw module */
	redrawrange(markline(frommark), markline(tomark), markline(frommark));

	/* adjust marks 'a through 'z and '' as needed */
	l = markline(tomark);
	for (i = 0; i < NMARKS; i++)
	{
		if (mark[i] < frommark)
		{
			continue;
		}
		else if (mark[i] < tomark)
		{
			mark[i] = MARK_UNSET;
		}
		else if (markline(mark[i]) == l)
		{
			if (markline(frommark) == l)
			{
				mark[i] -= markidx(tomark) - markidx(frommark);
			}
			else
			{
				mark[i] -= markidx(tomark);
			}
		}
		else
		{
			mark[i] -= MARK_AT_LINE(l - markline(frommark));
		}
	}

	/* Reporting... */
	if (markidx(frommark) == 0 && markidx(tomark) == 0)
	{
		rptlines = markline(tomark) - markline(frommark);
		rptlabel = "deleted";
	}

	/* find the block containing frommark */
	l = markline(frommark);
	for (i = 1; lnum[i] < l; i++)
	{
	}

	/* process each affected block... */
	for (m = frommark;
	     m < tomark && lnum[i] < INFINITY;
	     m = MARK_AT_LINE(lnum[i - 1] + 1))
	{
		/* fetch the block */
		blk = blkget(i);

		/* find the mark in the block */
		scan = blk->c;
		for (l = markline(m) - lnum[i - 1] - 1; l > 0; l--)
		{
			while (*scan++ != '\n')
			{
			}
		}
		scan += markidx(m);

		/* figure out where the changes to this block end */
		if (markline(tomark) > lnum[i])
		{
			cpy = blk->c + BLKSIZE;
		}
		else if (markline(tomark) == markline(m))
		{
			cpy = scan - markidx(m) + markidx(tomark);
		}
		else
		{
			cpy = scan;
			for (l = markline(tomark) - markline(m);
			     l > 0;
			     l--)
			{
				while (*cpy++ != '\n')
				{
				}
			}
			cpy += markidx(tomark);
		}

		/* delete the stuff by moving chars within this block */
		while (cpy < blk->c + BLKSIZE)
		{
			*scan++ = *cpy++;
		}
		while (scan < blk->c + BLKSIZE)
		{
			*scan++ = '\0';
		}

		/* adjust tomark to allow for lines deleted from this block */
		tomark -= MARK_AT_LINE(lnum[i] + 1 - markline(m));

		/* if this block isn't empty now, then advance i */
		if (*blk->c)
		{
			i++;
		}

		/* the buffer has changed.  Update hdr and lnum. */
		blkdirty(blk);
	}

	/* must have at least 1 line */
	if (nlines == 0)
	{
		blk = blkadd(1);
		blk->c[0] = '\n';
		blkdirty(blk);
		cursor = MARK_FIRST;
	}
}


/* add some text at a specific place in the file */
void add(atmark, newtext)
	MARK		atmark;		/* where to insert the new text */
	char		*newtext;	/* NUL-terminated string to insert */
{
	REG char	*scan;		/* used to move through string */
	REG char	*build;		/* used while copying chars */
	int		addlines;	/* number of lines we're adding */
	int		lastpart;	/* size of last partial line */
	BLK		*blk;		/* the block to be modified */
	int		blkno;		/* the logical block# of (*blk) */
	REG char	*newptr;	/* where new text starts in blk */
	BLK		buf;		/* holds chars from orig blk */
	BLK		linebuf;	/* holds part of line that didn't fit */
	BLK		*following;	/* the BLK following the last BLK */
	int		i;
	long		l;

#ifdef DEBUG2
	debout("add(%ld.%d, \"%s\")\n", markline(atmark), markidx(atmark), newtext);
#endif
#ifdef lint
	buf.c[0] = 0;
#endif
	/* if not adding anything, return now */
	if (!*newtext)
	{
		return;
	}

	/* This is a change */
	changes++;
	significant = TRUE;

	/* count the number of lines in the new text */
	for (scan = newtext, lastpart = addlines = 0; *scan; )
	{
		if (*scan++ == '\n')
		{
			addlines++;
			lastpart = 0;
		}
		else
		{
			lastpart++;
		}
	}

	/* Reporting... */
	if (lastpart == 0 && markidx(atmark) == 0)
	{
		rptlines = addlines;
		rptlabel = "added";
	}

	/* extract the line# from atmark */
	l = markline(atmark);

	/* supply clues to the redraw module */
	if ((markidx(atmark) == 0 && lastpart == 0) || addlines == 0)
	{
		redrawrange(l, l, l + addlines);
	}
	else
	{
		/* make sure the last line gets redrawn -- it was
		 * split, so its appearance has changed
		 */
		redrawrange(l, l + 1L, l + addlines + 1L);
	}

	/* adjust marks 'a through 'z and '' as needed */
	for (i = 0; i < NMARKS; i++)
	{
		if (mark[i] < atmark)
		{
			/* earlier line, or earlier in same line: no change */
			continue;
		}
		else if (markline(mark[i]) > l)
		{
			/* later line: move down a whole number of lines */
			mark[i] += MARK_AT_LINE(addlines);
		}
		else
		{
			/* later in same line */
			if (addlines > 0)
			{
				/* multi-line add, which split this line:
				 * move down, and possibly left or right,
				 * depending on where the split was and how
				 * much text was inserted after the last \n
				 */
				mark[i] += MARK_AT_LINE(addlines) + lastpart - markidx(atmark);
			}
			else
			{
				/* totally within this line: move right */
				mark[i] += lastpart;
			}
		}
	}

	/* get the block to be modified */
	for (blkno = 1; lnum[blkno] < l && lnum[blkno + 1] < INFINITY; blkno++)
	{
	}
	blk = blkget(blkno);
	buf = *blk;

	/* figure out where the new text starts */
	for (newptr = buf.c, l = markline(atmark) - lnum[blkno - 1] - 1;
	     l > 0;
	     l--)
	{
		while (*newptr++ != '\n')
		{
		}
	}
	newptr += markidx(atmark);

	/* keep start of old block */
	build = blk->c + (int)(newptr - buf.c);

	/* fill this block (or blocks) from the newtext string */
	while (*newtext)
	{
		while (*newtext && build < blk->c + BLKSIZE - 1)
		{
			*build++ = *newtext++;
		}
		if (*newtext)
		{
			/* save the excess */
			for (scan = linebuf.c + BLKSIZE;
			     build > blk->c && build[-1] != '\n';
			     )
			{
				*--scan = *--build;
			}

			/* write the block */
			while (build < blk->c + BLKSIZE)
			{
				*build++ = '\0';
			}
			blkdirty(blk);

			/* add another block */
			blkno++;
			blk = blkadd(blkno);

			/* copy in the excess from last time */
			for (build = blk->c; scan < linebuf.c + BLKSIZE; )
			{
				*build++ = *scan++;
			}
		}
	}

	/* fill this block(s) from remainder of orig block */
	while (newptr < buf.c + BLKSIZE && *newptr)
	{
		while (newptr < buf.c + BLKSIZE
		    && *newptr
		    && build < blk->c + BLKSIZE - 1)
		{
			*build++ = *newptr++;
		}
		if (newptr < buf.c + BLKSIZE && *newptr)
		{
			/* save the excess */
			for (scan = linebuf.c + BLKSIZE;
			     build > blk->c && build[-1] != '\n';
			     )
			{
				*--scan = *--build;
			}

			/* write the block */
			while (build < blk->c + BLKSIZE)
			{
				*build++ = '\0';
			}
			blkdirty(blk);

			/* add another block */
			blkno++;
			blk = blkadd(blkno);

			/* copy in the excess from last time */
			for (build = blk->c; scan < linebuf.c + BLKSIZE; )
			{
				*build++ = *scan++;
			}
		}
	}

	/* see if we can combine our last block with the following block */
	if (lnum[blkno] < nlines && lnum[blkno + 1] - lnum[blkno] < (BLKSIZE >> 6))
	{
		/* hey, we probably can!  Get the following block & see... */
		following = blkget(blkno + 1);
		if (strlen(following->c) + (build - blk->c) < (unsigned)(BLKSIZE - 1))
		{
			/* we can!  Copy text from following to blk */
			for (scan = following->c; *scan; )
			{
				*build++ = *scan++;
			}
			while (build < blk->c + BLKSIZE)
			{
				*build++ = '\0';
			}
			blkdirty(blk);

			/* pretend the following was the last blk */
			blk = following;
			build = blk->c;
		}
	}

	/* that last block is dirty by now */
	while (build < blk->c + BLKSIZE)
	{
		*build++ = '\0';
	}
	blkdirty(blk);
}


/* change the text of a file */
void change(frommark, tomark, newtext)
	MARK	frommark, tomark;
	char	*newtext;
{
	int	i;
	long	l;
	char	*text;
	BLK	*blk;

#ifdef DEBUG2
	debout("change(%ld.%d, %ld.%d, \"%s\")\n", markline(frommark), markidx(frommark), markline(tomark), markidx(tomark), newtext);
#endif

	/* optimize for single-character replacement */
	if (frommark + 1 == tomark && newtext[0] && !newtext[1] && newtext[0] != '\n')
	{
		/* find the block containing frommark */
		l = markline(frommark);
		for (i = 1; lnum[i] < l; i++)
		{
		}

		/* get the block */
		blk = blkget(i);

		/* find the line within the block */
		for (text = blk->c, i = l - lnum[i - 1] - 1; i > 0; text++)
		{
			if (*text == '\n')
			{
				i--;
			}
		}

		/* replace the char */
		text += markidx(frommark);
		if (*text == newtext[0])
		{
			/* no change was needed - same char */
			return;
		}
		else if (*text != '\n')
		{
			/* This is a change */
			changes++;
			significant = TRUE;
			ChangeText
			{
				*text = newtext[0];
				blkdirty(blk);
			}
			redrawrange(markline(frommark), markline(tomark), markline(frommark));
			return;
		}
		/* else it is a complex change involving newline... */
	}

	/* couldn't optimize, so do delete & add */
	ChangeText
	{
		delete(frommark, tomark);
		add(frommark, newtext);
		rptlabel = "changed";
	}
}
