/*
 * Copyright (C) 1984-2025  Mark Nudelman
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */


/*
 * Code to handle displaying line numbers.
 *
 * Finding the line number of a given file position is rather tricky.
 * We don't want to just start at the beginning of the file and
 * count newlines, because that is slow for large files (and also
 * wouldn't work if we couldn't get to the start of the file; e.g.
 * if input is a long pipe).
 *
 * So we use the function add_lnum to cache line numbers.
 * We try to be very clever and keep only the more interesting
 * line numbers when we run out of space in our table.  A line
 * number is more interesting than another when it is far from
 * other line numbers.   For example, we'd rather keep lines
 * 100,200,300 than 100,101,300.  200 is more interesting than
 * 101 because 101 can be derived very cheaply from 100, while
 * 200 is more expensive to derive from 100.
 *
 * The function currline() returns the line number of a given
 * position in the file.  As a side effect, it calls add_lnum
 * to cache the line number.  Therefore currline is occasionally
 * called to make sure we cache line numbers often enough.
 */

#include "less.h"

/*
 * Structure to keep track of a line number and the associated file position.
 * A doubly-linked circular list of line numbers is kept ordered by line number.
 */
struct linenum_info
{
	struct linenum_info *next;      /* Link to next in the list */
	struct linenum_info *prev;      /* Line to previous in the list */
	POSITION pos;                   /* File position */
	POSITION gap;                   /* Gap between prev and next */
	LINENUM line;                   /* Line number */
};
/*
 * "gap" needs some explanation: the gap of any particular line number
 * is the distance between the previous one and the next one in the list.
 * ("Distance" means difference in file position.)  In other words, the
 * gap of a line number is the gap which would be introduced if this
 * line number were deleted.  It is used to decide which one to replace
 * when we have a new one to insert and the table is full.
 */

#define LONGTIME        (2)             /* In seconds */

static struct linenum_info anchor;      /* Anchor of the list */
static struct linenum_info *freelist;   /* Anchor of the unused entries */
static struct linenum_info pool[LINENUM_POOL]; /* The pool itself */
static struct linenum_info *spare;      /* We always keep one spare entry */
public lbool scanning_eof = FALSE;

extern int linenums;
extern int sigs;
extern int sc_height;
extern int header_lines;
extern int nonum_headers;
extern POSITION header_start_pos;

/*
 * Initialize the line number structures.
 */
public void clr_linenum(void)
{
	struct linenum_info *p;

	/*
	 * Put all the entries on the free list.
	 * Leave one for the "spare".
	 */
	for (p = pool;  p < &pool[LINENUM_POOL-2];  p++)
		p->next = p+1;
	pool[LINENUM_POOL-2].next = NULL;
	freelist = pool;

	spare = &pool[LINENUM_POOL-1];

	/*
	 * Initialize the anchor.
	 */
	anchor.next = anchor.prev = &anchor;
	anchor.gap = 0;
	anchor.pos = (POSITION)0;
	anchor.line = 1;
}

/*
 * Calculate the gap for an entry.
 */
static void calcgap(struct linenum_info *p)
{
	/*
	 * Don't bother to compute a gap for the anchor.
	 * Also don't compute a gap for the last one in the list.
	 * The gap for that last one should be considered infinite,
	 * but we never look at it anyway.
	 */
	if (p == &anchor || p->next == &anchor)
		return;
	p->gap = p->next->pos - p->prev->pos;
}

/*
 * Add a new line number to the cache.
 * The specified position (pos) should be the file position of the
 * FIRST character in the specified line.
 */
public void add_lnum(LINENUM linenum, POSITION pos)
{
	struct linenum_info *p;
	struct linenum_info *new;
	struct linenum_info *nextp;
	struct linenum_info *prevp;
	POSITION mingap;

	/*
	 * Find the proper place in the list for the new one.
	 * The entries are sorted by position.
	 */
	for (p = anchor.next;  p != &anchor && p->pos < pos;  p = p->next)
		if (p->line == linenum)
			/* We already have this one. */
			return;
	nextp = p;
	prevp = p->prev;

	if (freelist != NULL)
	{
		/*
		 * We still have free (unused) entries.
		 * Use one of them.
		 */
		new = freelist;
		freelist = freelist->next;
	} else
	{
		/*
		 * No free entries.
		 * Use the "spare" entry.
		 */
		new = spare;
		spare = NULL;
	}

	/*
	 * Fill in the fields of the new entry,
	 * and insert it into the proper place in the list.
	 */
	new->next = nextp;
	new->prev = prevp;
	new->pos = pos;
	new->line = linenum;

	nextp->prev = new;
	prevp->next = new;

	/*
	 * Recalculate gaps for the new entry and the neighboring entries.
	 */
	calcgap(new);
	calcgap(nextp);
	calcgap(prevp);

	if (spare == NULL)
	{
		/*
		 * We have used the spare entry.
		 * Scan the list to find the one with the smallest
		 * gap, take it out and make it the spare.
		 * We should never remove the last one, so stop when
		 * we get to p->next == &anchor.  This also avoids
		 * looking at the gap of the last one, which is
		 * not computed by calcgap.
		 */
		mingap = anchor.next->gap;
		for (p = anchor.next;  p->next != &anchor;  p = p->next)
		{
			if (p->gap <= mingap)
			{
				spare = p;
				mingap = p->gap;
			}
		}
		spare->next->prev = spare->prev;
		spare->prev->next = spare->next;
	}
}

/*
 * If we get stuck in a long loop trying to figure out the
 * line number, print a message to tell the user what we're doing.
 */
static void longloopmessage(void)
{
	ierror("Calculating line numbers", NULL_PARG);
}

struct delayed_msg
{
	void (*message)(void);
	int loopcount;
#if HAVE_TIME
	time_type startime;
#endif
};

static void start_delayed_msg(struct delayed_msg *dmsg, void (*message)(void))
{
	dmsg->loopcount = 0;
	dmsg->message = message;
#if HAVE_TIME
	dmsg->startime = get_time();
#endif
}

static void delayed_msg(struct delayed_msg *dmsg)
{
#if HAVE_TIME
	if (dmsg->loopcount >= 0 && ++(dmsg->loopcount) > 100)
	{
		dmsg->loopcount = 0;
		if (get_time() >= dmsg->startime + LONGTIME)
		{
			dmsg->message();
			dmsg->loopcount = -1;
		}
	}
#else
	if (dmsg->loopcount >= 0 && ++(dmsg->loopcount) > LONGLOOP)
	{
		dmsg->message();
		dmsg->loopcount = -1;
	}
#endif
}

/*
 * Turn off line numbers because the user has interrupted
 * a lengthy line number calculation.
 */
static void abort_delayed_msg(struct delayed_msg *dmsg)
{
	if (dmsg->loopcount >= 0)
		return;
	if (linenums == OPT_ONPLUS)
		/*
		 * We were displaying line numbers, so need to repaint.
		 */
		screen_trashed();
	linenums = 0;
	error("Line numbers turned off", NULL_PARG);
}

/*
 * Find the line number associated with a given position.
 * Return 0 if we can't figure it out.
 */
public LINENUM find_linenum(POSITION pos)
{
	struct linenum_info *p;
	LINENUM linenum;
	POSITION cpos;
	struct delayed_msg dmsg;

	if (!linenums)
		/*
		 * We're not using line numbers.
		 */
		return (0);
	if (pos == NULL_POSITION)
		/*
		 * Caller doesn't know what he's talking about.
		 */
		return (0);
	if (pos <= ch_zero())
		/*
		 * Beginning of file is always line number 1.
		 */
		return (1);

	/*
	 * Find the entry nearest to the position we want.
	 */
	for (p = anchor.next;  p != &anchor && p->pos < pos;  p = p->next)
		continue;
	if (p->pos == pos)
		/* Found it exactly. */
		return (p->line);

	/*
	 * This is the (possibly) time-consuming part.
	 * We start at the line we just found and start
	 * reading the file forward or backward till we
	 * get to the place we want.
	 *
	 * First decide whether we should go forward from the 
	 * previous one or backwards from the next one.
	 * The decision is based on which way involves 
	 * traversing fewer bytes in the file.
	 */
	start_delayed_msg(&dmsg, longloopmessage);
	if (p == &anchor || pos - p->prev->pos < p->pos - pos)
	{
		/*
		 * Go forward.
		 */
		p = p->prev;
		if (ch_seek(p->pos))
			return (0);
		for (linenum = p->line, cpos = p->pos;  cpos < pos;  linenum++)
		{
			/*
			 * Allow a signal to abort this loop.
			 */
			cpos = forw_raw_line(cpos, NULL, NULL);
			if (ABORT_SIGS()) {
				abort_delayed_msg(&dmsg);
				return (0);
			}
			if (cpos == NULL_POSITION)
				return (0);
			delayed_msg(&dmsg);
		}
		/*
		 * We might as well cache it.
		 */
		add_lnum(linenum, cpos);
		/*
		 * If the given position is not at the start of a line,
		 * make sure we return the correct line number.
		 */
		if (cpos > pos)
			linenum--;
	} else
	{
		/*
		 * Go backward.
		 */
		if (ch_seek(p->pos))
			return (0);
		for (linenum = p->line, cpos = p->pos;  cpos > pos;  linenum--)
		{
			/*
			 * Allow a signal to abort this loop.
			 */
			cpos = back_raw_line(cpos, NULL, NULL);
			if (ABORT_SIGS()) {
				abort_delayed_msg(&dmsg);
				return (0);
			}
			if (cpos == NULL_POSITION)
				return (0);
			delayed_msg(&dmsg);
		}
		/*
		 * We might as well cache it.
		 */
		add_lnum(linenum, cpos);
	}
	return (linenum);
}

/*
 * Find the position of a given line number.
 * Return NULL_POSITION if we can't figure it out.
 */
public POSITION find_pos(LINENUM linenum)
{
	struct linenum_info *p;
	POSITION cpos;
	LINENUM clinenum;

	if (linenum <= 1)
		/*
		 * Line number 1 is beginning of file.
		 */
		return (ch_zero());

	/*
	 * Find the entry nearest to the line number we want.
	 */
	for (p = anchor.next;  p != &anchor && p->line < linenum;  p = p->next)
		continue;
	if (p->line == linenum)
		/* Found it exactly. */
		return (p->pos);

	if (p == &anchor || linenum - p->prev->line < p->line - linenum)
	{
		/*
		 * Go forward.
		 */
		p = p->prev;
		if (ch_seek(p->pos))
			return (NULL_POSITION);
		for (clinenum = p->line, cpos = p->pos;  clinenum < linenum;  clinenum++)
		{
			/*
			 * Allow a signal to abort this loop.
			 */
			cpos = forw_raw_line(cpos, NULL, NULL);
			if (ABORT_SIGS())
				return (NULL_POSITION);
			if (cpos == NULL_POSITION)
				return (NULL_POSITION);
		}
	} else
	{
		/*
		 * Go backward.
		 */
		if (ch_seek(p->pos))
			return (NULL_POSITION);
		for (clinenum = p->line, cpos = p->pos;  clinenum > linenum;  clinenum--)
		{
			/*
			 * Allow a signal to abort this loop.
			 */
			cpos = back_raw_line(cpos, NULL, NULL);
			if (ABORT_SIGS())
				return (NULL_POSITION);
			if (cpos == NULL_POSITION)
				return (NULL_POSITION);
		}
	}
	/*
	 * We might as well cache it.
	 */
	add_lnum(clinenum, cpos);
	return (cpos);
}

/*
 * Return the line number of the "current" line.
 * The argument "where" tells which line is to be considered
 * the "current" line (e.g. TOP, BOTTOM, MIDDLE, etc).
 */
public LINENUM currline(int where)
{
	POSITION pos;
	POSITION len;
	LINENUM linenum;

	pos = position(where);
	len = ch_length();
	while (pos == NULL_POSITION && where >= 0 && where < sc_height)
		pos = position(++where);
	if (pos == NULL_POSITION)
		pos = len;
	linenum = find_linenum(pos);
	if (pos == len)
		linenum--;
	return (linenum);
}

static void detlenmessage(void)
{
	ierror("Determining length of file", NULL_PARG);
}

/*
 * Scan entire file, counting line numbers.
 */
public void scan_eof(void)
{
	POSITION pos = ch_zero();
	LINENUM linenum = 0;
	struct delayed_msg dmsg;

	if (ch_seek(0))
		return;
	/*
	 * scanning_eof prevents the "Waiting for data" message from 
	 * overwriting "Determining length of file".
	 */
	start_delayed_msg(&dmsg, detlenmessage);
	scanning_eof = TRUE;
	while (pos != NULL_POSITION)
	{
		/* For efficiency, only add one every 256 line numbers. */
		if ((linenum++ % 256) == 0)
			add_lnum(linenum, pos);
		pos = forw_raw_line(pos, NULL, NULL);
		if (ABORT_SIGS())
		{
			abort_delayed_msg(&dmsg);
			break;
		}
		delayed_msg(&dmsg);
	}
	scanning_eof = FALSE;
}

/*
 * Return a line number adjusted for display
 * (handles the --no-number-headers option).
 */
public LINENUM vlinenum(LINENUM linenum)
{
	if (nonum_headers && header_lines > 0)
	{
		LINENUM header_start_line = find_linenum(header_start_pos);
		if (header_start_line != 0)
		{
			LINENUM header_end_line = header_start_line + header_lines; /* first line after header */
			linenum = (linenum < header_end_line) ? 0 : linenum - header_end_line + 1;
		}
	}
	return linenum;
}
