/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)line.c	10.21 (Berkeley) 9/15/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "../vi/vi.h"

static int scr_update __P((SCR *, recno_t, lnop_t, int));

/*
 * db_eget --
 *	Front-end to db_get, special case handling for empty files.
 *
 * PUBLIC: int db_eget __P((SCR *, recno_t, char **, size_t *, int *));
 */
int
db_eget(sp, lno, pp, lenp, isemptyp)
	SCR *sp;
	recno_t lno;				/* Line number. */
	char **pp;				/* Pointer store. */
	size_t *lenp;				/* Length store. */
	int *isemptyp;
{
	recno_t l1;

	if (isemptyp != NULL)
		*isemptyp = 0;

	/* If the line exists, simply return it. */
	if (!db_get(sp, lno, 0, pp, lenp))
		return (0);

	/*
	 * If the user asked for line 0 or line 1, i.e. the only possible
	 * line in an empty file, find the last line of the file; db_last
	 * fails loudly.
	 */
	if ((lno == 0 || lno == 1) && db_last(sp, &l1))
		return (1);

	/* If the file isn't empty, fail loudly. */
	if (lno != 0 && lno != 1 || l1 != 0) {
		db_err(sp, lno);
		return (1);
	}

	if (isemptyp != NULL)
		*isemptyp = 1;

	return (1);
}

/*
 * db_get --
 *	Look in the text buffers for a line, followed by the cache, followed
 *	by the database.
 *
 * PUBLIC: int db_get __P((SCR *, recno_t, u_int32_t, char **, size_t *));
 */
int
db_get(sp, lno, flags, pp, lenp)
	SCR *sp;
	recno_t lno;				/* Line number. */
	u_int32_t flags;
	char **pp;				/* Pointer store. */
	size_t *lenp;				/* Length store. */
{
	DBT data, key;
	EXF *ep;
	TEXT *tp;
	recno_t l1, l2;

	/*
	 * The underlying recno stuff handles zero by returning NULL, but
	 * have to have an OOB condition for the look-aside into the input
	 * buffer anyway.
	 */
	if (lno == 0)
		goto err1;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		goto err3;
	}

	if (LF_ISSET(DBG_NOCACHE))
		goto nocache;

	/*
	 * Look-aside into the TEXT buffers and see if the line we want
	 * is there.
	 */
	if (F_ISSET(sp, SC_TINPUT)) {
		l1 = ((TEXT *)sp->tiq.cqh_first)->lno;
		l2 = ((TEXT *)sp->tiq.cqh_last)->lno;
		if (l1 <= lno && l2 >= lno) {
#if defined(DEBUG) && 0
	TRACE(sp, "retrieve TEXT buffer line %lu\n", (u_long)lno);
#endif
			for (tp = sp->tiq.cqh_first;
			    tp->lno != lno; tp = tp->q.cqe_next);
			if (lenp != NULL)
				*lenp = tp->len;
			if (pp != NULL)
				*pp = tp->lb;
			return (0);
		}
		/*
		 * Adjust the line number for the number of lines used
		 * by the text input buffers.
		 */
		if (lno > l2)
			lno -= l2 - l1;
	}

	/* Look-aside into the cache, and see if the line we want is there. */
	if (lno == ep->c_lno) {
#if defined(DEBUG) && 0
	TRACE(sp, "retrieve cached line %lu\n", (u_long)lno);
#endif
		if (lenp != NULL)
			*lenp = ep->c_len;
		if (pp != NULL)
			*pp = ep->c_lp;
		return (0);
	}
	ep->c_lno = OOBLNO;

nocache:
	/* Get the line from the underlying database. */
	key.data = &lno;
	key.size = sizeof(lno);
	switch (ep->db->get(ep->db, &key, &data, 0)) {
        case -1:
		goto err2;
	case 1:
err1:		if (LF_ISSET(DBG_FATAL))
err2:			db_err(sp, lno);
err3:		if (lenp != NULL)
			*lenp = 0;
		if (pp != NULL)
			*pp = NULL;
		return (1);
	}

	/* Reset the cache. */
	ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

#if defined(DEBUG) && 0
	TRACE(sp, "retrieve DB line %lu\n", (u_long)lno);
#endif
	if (lenp != NULL)
		*lenp = data.size;
	if (pp != NULL)
		*pp = ep->c_lp;
	return (0);
}

/*
 * db_delete --
 *	Delete a line from the file.
 *
 * PUBLIC: int db_delete __P((SCR *, recno_t));
 */
int
db_delete(sp, lno)
	SCR *sp;
	recno_t lno;
{
	DBT key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp, "delete line %lu\n", (u_long)lno);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update marks, @ and global commands. */
	if (mark_insdel(sp, LINE_DELETE, lno))
		return (1);
	if (ex_g_insdel(sp, LINE_DELETE, lno))
		return (1);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_DELETE);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	SIGBLOCK;
	if (ep->db->del(ep->db, &key, 0) == 1) {
		msgq(sp, M_SYSERR,
		    "003|unable to delete line %lu", (u_long)lno);
		return (1);
	}
	SIGUNBLOCK;

	/* Flush the cache, update line count, before screen update. */
	if (lno <= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		--ep->c_nlines;

	/* File now modified. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_DELETE, 1));
}

/*
 * db_append --
 *	Append a line into the file.
 *
 * PUBLIC: int db_append __P((SCR *, int, recno_t, char *, size_t));
 */
int
db_append(sp, update, lno, p, len)
	SCR *sp;
	int update;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;
	int rval;

#if defined(DEBUG) && 0
	TRACE(sp, "append to %lu: len %u {%.*s}\n", lno, len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, R_IAFTER) == -1) {
		msgq(sp, M_SYSERR,
		    "004|unable to append to line %lu", (u_long)lno);
		return (1);
	}
	SIGUNBLOCK;

	/* Flush the cache, update line count, before screen update. */
	if (lno < ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, lno + 1, LOG_LINE_APPEND);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno + 1))
		rval = 1;

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
	return (scr_update(sp, lno, LINE_APPEND, update) || rval);
}

/*
 * db_insert --
 *	Insert a line into the file.
 *
 * PUBLIC: int db_insert __P((SCR *, recno_t, char *, size_t));
 */
int
db_insert(sp, lno, p, len)
	SCR *sp;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;
	int rval;

#if defined(DEBUG) && 0
	TRACE(sp, "insert before %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif
	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, R_IBEFORE) == -1) {
		msgq(sp, M_SYSERR,
		    "005|unable to insert at line %lu", (u_long)lno);
		return (1);
	}
	SIGUNBLOCK;

	/* Flush the cache, update line count, before screen update. */
	if (lno >= ep->c_lno)
		ep->c_lno = OOBLNO;
	if (ep->c_nlines != OOBLNO)
		++ep->c_nlines;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log change. */
	log_line(sp, lno, LOG_LINE_INSERT);

	/* Update marks, @ and global commands. */
	rval = 0;
	if (mark_insdel(sp, LINE_INSERT, lno))
		rval = 1;
	if (ex_g_insdel(sp, LINE_INSERT, lno))
		rval = 1;

	/* Update screen. */
	return (scr_update(sp, lno, LINE_INSERT, 1) || rval);
}

/*
 * db_set --
 *	Store a line in the file.
 *
 * PUBLIC: int db_set __P((SCR *, recno_t, char *, size_t));
 */
int
db_set(sp, lno, p, len)
	SCR *sp;
	recno_t lno;
	char *p;
	size_t len;
{
	DBT data, key;
	EXF *ep;

#if defined(DEBUG) && 0
	TRACE(sp, "replace line %lu: len %lu {%.*s}\n",
	    (u_long)lno, (u_long)len, MIN(len, 20), p);
#endif

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/* Log before change. */
	log_line(sp, lno, LOG_LINE_RESET_B);

	/* Update file. */
	key.data = &lno;
	key.size = sizeof(lno);
	data.data = p;
	data.size = len;
	SIGBLOCK;
	if (ep->db->put(ep->db, &key, &data, 0) == -1) {
		msgq(sp, M_SYSERR,
		    "006|unable to store line %lu", (u_long)lno);
		return (1);
	}
	SIGUNBLOCK;

	/* Flush the cache, before logging or screen update. */
	if (lno == ep->c_lno)
		ep->c_lno = OOBLNO;

	/* File now dirty. */
	if (F_ISSET(ep, F_FIRSTMODIFY))
		(void)rcv_init(sp);
	F_SET(ep, F_MODIFIED);

	/* Log after change. */
	log_line(sp, lno, LOG_LINE_RESET_F);

	/* Update screen. */
	return (scr_update(sp, lno, LINE_RESET, 1));
}

/*
 * db_exist --
 *	Return if a line exists.
 *
 * PUBLIC: int db_exist __P((SCR *, recno_t));
 */
int
db_exist(sp, lno)
	SCR *sp;
	recno_t lno;
{
	EXF *ep;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}

	if (lno == OOBLNO)
		return (0);
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO)
		return (lno <= (F_ISSET(sp, SC_TINPUT) ?
		    ep->c_nlines + (((TEXT *)sp->tiq.cqh_last)->lno -
		    ((TEXT *)sp->tiq.cqh_first)->lno) : ep->c_nlines));

	/* Go get the line. */
	return (!db_get(sp, lno, 0, NULL, NULL));
}

/*
 * db_last --
 *	Return the number of lines in the file.
 *
 * PUBLIC: int db_last __P((SCR *, recno_t *));
 */
int
db_last(sp, lnop)
	SCR *sp;
	recno_t *lnop;
{
	DBT data, key;
	EXF *ep;
	recno_t lno;

	/* Check for no underlying file. */
	if ((ep = sp->ep) == NULL) {
		ex_emsg(sp, NULL, EXM_NOFILEYET);
		return (1);
	}
		
	/*
	 * Check the last-line number cache.  Adjust the cached line
	 * number for the lines used by the text input buffers.
	 */
	if (ep->c_nlines != OOBLNO) {
		*lnop = ep->c_nlines;
		if (F_ISSET(sp, SC_TINPUT))
			*lnop += ((TEXT *)sp->tiq.cqh_last)->lno -
			    ((TEXT *)sp->tiq.cqh_first)->lno;
		return (0);
	}

	key.data = &lno;
	key.size = sizeof(lno);

	switch (ep->db->seq(ep->db, &key, &data, R_LAST)) {
        case -1:
		msgq(sp, M_SYSERR, "007|unable to get last line");
		*lnop = 0;
		return (1);
        case 1:
		*lnop = 0;
		return (0);
	default:
		break;
	}

	/* Fill the cache. */
	memcpy(&lno, key.data, sizeof(lno));
	ep->c_nlines = ep->c_lno = lno;
	ep->c_len = data.size;
	ep->c_lp = data.data;

	/* Return the value. */
	*lnop = (F_ISSET(sp, SC_TINPUT) &&
	    ((TEXT *)sp->tiq.cqh_last)->lno > lno ?
	    ((TEXT *)sp->tiq.cqh_last)->lno : lno);
	return (0);
}

/*
 * db_err --
 *	Report a line error.
 *
 * PUBLIC: void db_err __P((SCR *, recno_t));
 */
void
db_err(sp, lno)
	SCR *sp;
	recno_t lno;
{
	msgq(sp, M_ERR,
	    "008|Error: unable to retrieve line %lu", (u_long)lno);
}

/*
 * scr_update --
 *	Update all of the screens that are backed by the file that
 *	just changed.
 */
static int
scr_update(sp, lno, op, current)
	SCR *sp;
	recno_t lno;
	lnop_t op;
	int current;
{
	EXF *ep;
	SCR *tsp;

	if (F_ISSET(sp, SC_EX))
		return (0);

	ep = sp->ep;
	if (ep->refcnt != 1)
		for (tsp = sp->gp->dq.cqh_first;
		    tsp != (void *)&sp->gp->dq; tsp = tsp->q.cqe_next)
			if (sp != tsp && tsp->ep == ep)
				if (vs_change(tsp, lno, op))
					return (1);
	return (current ? vs_change(sp, lno, op) : 0);
}
