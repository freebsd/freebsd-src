/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)line.c	8.23 (Berkeley) 3/15/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

static inline int scr_update __P((SCR *, EXF *, recno_t, enum operation, int));

/*
 * file_gline --
 *	Look in the text buffers for a line; if it's not there
 *	call file_rline to retrieve it from the database.
 */
char *
file_gline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	TEXT *tp;
	recno_t l1, l2;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an oob condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		return (NULL);

	/*
	 * Look-aside into the TEXT buffers and see if the line we want
	 * is there.
	 */
	if (F_ISSET(sp, S_INPUT)) {
		l1 = ((TEXT *)sp->tiq.cqh_first)->lno;
		l2 = ((TEXT *)sp->tiq.cqh_last)->lno;
		if (l1 <= lno && l2 >= lno) {
			for (tp = sp->tiq.cqh_first;
			    tp->lno != lno; tp = tp->q.cqe_next);
			if (lenp)
				*lenp = tp->len;
			return (tp->lb);
		}
		/*
		 * Adjust the line number for the number of lines used
		 * by the text input buffers.
		 */
		if (lno > l2)
			lno -= l2 - l1;
	}
	return (file_rline(sp, ep, lno, lenp));
}

/*
 * file_rline --
 *	Look in the cache for a line; if it's not there retrieve
 *	it from the file.
 */
char *
file_rline(sp, ep, lno, lenp)
	SCR *sp;
	EXF *ep;
	recno_t lno;				/* Line number. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;

	/* Check the cache. */
	if (lno == ep->c_lno) {
		if (lenp)
			*lenp = ep->c_len;
		return (ep->c_lp);
	}
	ep->c_lno = OOBLNO;

	/* Get the line from the underlying database. */
	key.data = &lno;
	key.size = sizeof(lno);
	switch (ep->db->get(ep->db, &key, &data, 0)) {
        case -1:
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to get line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		/* FALLTHROUGH */
        case 1:
		return (NULL);
		/* NOTREACHED */
	}
	if (lenp)
		*lenp = data.size;

	/* Fill the cache. */
	ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

	return (data.data);
}

/*
 * file_dline --
 *	Delete a line from the file.
 */
int
file_dline(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	DBT key;

#if defined(DEBUG) && 0
	TRACE(sp, "delete line %lu\n", lno);
#endif
	/*
	 * XXX
	 *
	 * Marks and global commands have to know when lines are
	 * inserted or deleted.
	 */
	mark_insdel(sp, ep, LINE_DELETE, lno);
	global_insdel(sp, ep, LINE_DELETE, lno);

	/* Log change. */
	log_line(sp, ep, lno, LOG_LINE_DELETE);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	if (ep->db->del(ep->db, &key, 0) == 1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to delete line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno <= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		--ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_DELETE, 1));
}

/*
 * file_aline --
 *	Append a line into the file.
 */
int
file_aline(sp, ep, update, lno, p, len)
	SCR *sp;
	EXF *ep;
	int update;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	recno_t lline;

#if defined(DEBUG) && 0
	TRACE(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/*
	 * Very nasty special case.  The historic vi code displays a single
	 * space (or a '$' if the list option is set) for the first line in
	 * an "empty" file.  If we "insert" a line, that line gets scrolled
	 * down, not repainted, so it's incorrect when we refresh the the
	 * screen.  This is really hard to find and fix in the vi code -- the
	 * text input functions detect it explicitly and don't insert a new
	 * line.  The hack here is to repaint the screen if we're appending
	 * to an empty file.
	 */
	if (lno == 0) {
		if (file_lline(sp, ep, &lline))
			return (1);
		if (lline == 0)
			F_SET(sp, S_REDRAW);
	}

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, R_IAFTER) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to append to line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno < ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno + 1, LOG_LINE_APPEND);

	/*
	 * XXX
	 *
	 * Marks and global commands have to know when lines are
	 * inserted or deleted.
	 */
	mark_insdel(sp, ep, LINE_INSERT, lno + 1);
	global_insdel(sp, ep, LINE_INSERT, lno + 1);

	/*
	 * Update screen.
	 *
	 * XXX
	 * Nasty hack.  If multiple lines are input by the user, they aren't
	 * committed until an <ESC> is entered.  The problem is the screen was
	 * updated/scrolled as each line was entered.  So, when this routine
	 * is called to copy the new lines from the cut buffer into the file,
	 * it has to know not to update the screen again.
	 */
	return (scr_update(sp, ep, lno, LINE_APPEND, update));
}

/*
 * file_iline --
 *	Insert a line into the file.
 */
int
file_iline(sp, ep, lno, p, len)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	recno_t lline;

#if defined(DEBUG) && 0
	TRACE(sp,
	    "insert before %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif

	/* Very nasty special case.  See comment in file_aline(). */
	if (lno == 1) {
		if (file_lline(sp, ep, &lline))
			return (1);
		if (lline == 0)
			F_SET(sp, S_REDRAW);
	}

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, R_IBEFORE) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to insert at line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, update line count, before screen update. */
	if (lno >= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, ep, lno, LOG_LINE_INSERT);

	/*
	 * XXX
	 *
	 * Marks and global commands have to know when lines are
	 * inserted or deleted.
	 */
	mark_insdel(sp, ep, LINE_INSERT, lno);
	global_insdel(sp, ep, LINE_INSERT, lno);

	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_INSERT, 1));
}

/*
 * file_sline --
 *	Store a line in the file.
 */
int
file_sline(sp, ep, lno, p, len)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;

#if defined(DEBUG) && 0
	TRACE(sp,
	    "replace line %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Log before change. */
	log_line(sp, ep, lno, LOG_LINE_RESET_B);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	if (ep->db->put(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to store line %u: %s.",
		    tail(__FILE__), __LINE__, lno, strerror(errno));
		return (1);
	}

	/* Flush the cache, before logging or screen update. */
	if (lno == ep->c_lno)
		ep->c_lno = OOBLNO;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp, ep);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, ep, lno, LOG_LINE_RESET_F);

	/* Update screen. */
	return (scr_update(sp, ep, lno, LINE_RESET, 1));
}

/*
 * file_lline --
 *	Return the number of lines in the file.
 */
int
file_lline(sp, ep, lnop)
	SCR *sp;
	EXF *ep;
	recno_t *lnop;
{
	DBT data, key;
	recno_t lno;

	/* Check the cache. */
	if (ep->c_nlines != OOBLNO) {
		*lnop = (F_ISSET(sp, S_INPUT) &&
		    ((TEXT *)sp->tiq.cqh_last)->lno > ep->c_nlines ?
		    ((TEXT *)sp->tiq.cqh_last)->lno : ep->c_nlines);
		return (0);
	}

	key.data = &lno;
	key.size = sizeof(lno);

	switch (ep->db->seq(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_ERR,
		    "Error: %s/%d: unable to get last line: %s.",
		    tail(__FILE__), __LINE__, strerror(errno));
		*lnop = 0;
		return (1);
        case 1:
		*lnop = 0;
		return (0);
	default:
		break;
	}

	/* Fill the cache. */
	memmove(&lno, key.data, sizeof(lno));
	ep->c_nlines = ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

	/* Return the value. */
	*lnop = (F_ISSET(sp, S_INPUT) &&
	    ((TEXT *)sp->tiq.cqh_last)->lno > lno ?
	    ((TEXT *)sp->tiq.cqh_last)->lno : lno);
	return (0);
}

/*
 * scr_update --
 *	Update all of the screens that are backed by the file that
 *	just changed.
 */
static inline int
scr_update(sp, ep, lno, op, current)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	enum operation op;
	int current;
{
	SCR *tsp;

	if (ep->refcnt != 1)
		for (tsp = sp->gp->dq.cqh_first;
		    tsp != (void *)&sp->gp->dq; tsp = tsp->q.cqe_next)
			if (sp != tsp && tsp->ep == ep)
				(void)sp->s_change(tsp, ep, lno, op);
	return (current && sp->s_change(sp, ep, lno, op));
}
