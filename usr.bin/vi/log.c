/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)log.c	8.9 (Berkeley) 12/28/93";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

/*
 * The log consists of records, each containing a type byte and a variable
 * length byte string, as follows:
 *
 *	LOG_CURSOR_INIT		MARK
 *	LOG_CURSOR_END		MARK
 *	LOG_LINE_APPEND 	recno_t		char *
 *	LOG_LINE_DELETE		recno_t		char *
 *	LOG_LINE_INSERT		recno_t		char *
 *	LOG_LINE_RESET_F	recno_t		char *
 *	LOG_LINE_RESET_B	recno_t		char *
 *	LOG_MARK		MARK
 *
 * We do before image physical logging.  This means that the editor layer
 * MAY NOT modify records in place, even if simply deleting or overwriting
 * characters.  Since the smallest unit of logging is a line, we're using
 * up lots of space.  This may eventually have to be reduced, probably by
 * doing logical logging, which is a much cooler database phrase.
 *
 * The implementation of the historic vi 'u' command, using roll-forward and
 * roll-back, is simple.  Each set of changes has a LOG_CURSOR_INIT record,
 * followed by a number of other records, followed by a LOG_CURSOR_END record.
 * LOG_LINE_RESET records come in pairs.  The first is a LOG_LINE_RESET_B
 * record, and is the line before the change.  The second is LOG_LINE_RESET_F,
 * and is the line after the change.  Roll-back is done by backing up to the
 * first LOG_CURSOR_INIT record before a change.  Roll-forward is done in a
 * similar fashion.
 *
 * The 'U' command is implemented by rolling backward to a LOG_CURSOR_END
 * record for a line different from the current one.  It should be noted that
 * this means that a subsequent 'u' command will make a change based on the
 * new position of the log's cursor.  This is okay, and, in fact, historic vi
 * behaved that way.
 */

static int	log_cursor1 __P((SCR *, EXF *, int));
#if defined(DEBUG) && 0
static void	log_trace __P((SCR *, char *, recno_t, u_char *));
#endif

/* Try and restart the log on failure, i.e. if we run out of memory. */
#define	LOG_ERR {							\
	msgq(sp, M_ERR, "Error: %s/%d: put log error: %s.",		\
	    tail(__FILE__), __LINE__, strerror(errno));			\
	(void)ep->log->close(ep->log);					\
	if (!log_init(sp, ep))						\
		msgq(sp, M_ERR, "Log restarted.");			\
	return (1);							\
}

/*
 * log_init --
 *	Initialize the logging subsystem.
 */
int
log_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * Initialize the buffer.  The logging subsystem has its own
	 * buffers because the global ones are almost by definition
	 * going to be in use when the log runs.
	 */
	ep->l_lp = NULL;
	ep->l_len = 0;
	ep->l_cursor.lno = 1;		/* XXX Any valid recno. */
	ep->l_cursor.cno = 0;
	ep->l_high = ep->l_cur = 1;

	ep->log = dbopen(NULL, O_CREAT | O_NONBLOCK | O_RDWR,
	    S_IRUSR | S_IWUSR, DB_RECNO, NULL);
	if (ep->log == NULL) {
		msgq(sp, M_ERR, "log db: %s", strerror(errno));
		F_SET(ep, F_NOLOG);
		return (1);
	}

	return (0);
}

/*
 * log_end --
 *	Close the logging subsystem.
 */
int
log_end(sp, ep)
	SCR *sp;
	EXF *ep;
{
	if (ep->log != NULL) {
		(void)(ep->log->close)(ep->log);
		ep->log = NULL;
	}
	if (ep->l_lp != NULL) {
		free(ep->l_lp);
		ep->l_lp = NULL;
	}
	ep->l_len = 0;
	ep->l_cursor.lno = 1;		/* XXX Any valid recno. */
	ep->l_cursor.cno = 0;
	ep->l_high = ep->l_cur = 1;
	return (0);
}
		
/*
 * log_cursor --
 *	Log the current cursor position, starting an event.
 */
int
log_cursor(sp, ep)
	SCR *sp;
	EXF *ep;
{
	/*
	 * If any changes were made since the last cursor init,
	 * put out the ending cursor record.
	 */
	if (ep->l_cursor.lno == OOBLNO) {
		ep->l_cursor.lno = sp->lno;
		ep->l_cursor.cno = sp->cno;
		return (log_cursor1(sp, ep, LOG_CURSOR_END));
	}
	ep->l_cursor.lno = sp->lno;
	ep->l_cursor.cno = sp->cno;
	return (0);
}

/*
 * log_cursor1 --
 *	Actually push a cursor record out.
 */
static int
log_cursor1(sp, ep, type)
	SCR *sp;
	EXF *ep;
	int type;
{
	DBT data, key;

	BINC_RET(sp, ep->l_lp, ep->l_len, sizeof(u_char) + sizeof(MARK));
	ep->l_lp[0] = type;
	memmove(ep->l_lp + sizeof(u_char), &ep->l_cursor, sizeof(MARK));

	key.data = &ep->l_cur;
	key.size = sizeof(recno_t);
	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(MARK);
	if (ep->log->put(ep->log, &key, &data, 0) == -1)
		LOG_ERR;

#if defined(DEBUG) && 0
	TRACE(sp, "%lu: %s: %u/%u\n", ep->l_cur,
	    type == LOG_CURSOR_INIT ? "log_cursor_init" : "log_cursor_end",
	    sp->lno, sp->cno);
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;

	return (0);
}

/*
 * log_line --
 *	Log a line change.
 */
int
log_line(sp, ep, lno, action)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	u_int action;
{
	DBT data, key;
	size_t len;
	char *lp;

	if (F_ISSET(ep, F_NOLOG))
		return (0);

	/*
	 * XXX
	 *
	 * Kluge for vi.  Clear the EXF undo flag so that the
	 * next 'u' command does a roll-back, regardless.
	 */
	F_CLR(ep, F_UNDO);

	/* Put out one initial cursor record per set of changes. */
	if (ep->l_cursor.lno != OOBLNO) {
		if (log_cursor1(sp, ep, LOG_CURSOR_INIT))
			return (1);
		ep->l_cursor.lno = OOBLNO;
	}
		
	/*
	 * Put out the changes.  If it's a LOG_LINE_RESET_B call, it's a
	 * special case, avoid the caches.  Also, if it fails and it's
	 * line 1, it just means that the user started with an empty file,
	 * so fake an empty length line.
	 */
	if (action == LOG_LINE_RESET_B) {
		if ((lp = file_rline(sp, ep, lno, &len)) == NULL) {
			if (lno != 1) {
				GETLINE_ERR(sp, lno);
				return (1);
			}
			len = 0;
			lp = "";
		}
	} else
		if ((lp = file_gline(sp, ep, lno, &len)) == NULL) {
			GETLINE_ERR(sp, lno);
			return (1);
		}
	BINC_RET(sp,
	    ep->l_lp, ep->l_len, len + sizeof(u_char) + sizeof(recno_t));
	ep->l_lp[0] = action;
	memmove(ep->l_lp + sizeof(u_char), &lno, sizeof(recno_t));
	memmove(ep->l_lp + sizeof(u_char) + sizeof(recno_t), lp, len);

	key.data = &ep->l_cur;
	key.size = sizeof(recno_t);
	data.data = ep->l_lp;
	data.size = len + sizeof(u_char) + sizeof(recno_t);
	if (ep->log->put(ep->log, &key, &data, 0) == -1)
		LOG_ERR;

#if defined(DEBUG) && 0
	switch (action) {
	case LOG_LINE_APPEND:
		TRACE(sp, "%u: log_line: append: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_DELETE:
		TRACE(sp, "%lu: log_line: delete: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_INSERT:
		TRACE(sp, "%lu: log_line: insert: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_RESET_F:
		TRACE(sp, "%lu: log_line: reset_f: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	case LOG_LINE_RESET_B:
		TRACE(sp, "%lu: log_line: reset_b: %lu {%u}\n",
		    ep->l_cur, lno, len);
		break;
	}
#endif
	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;

	return (0);
}

/*
 * log_mark --
 *	Log a mark position.  For the log to work, we assume that there
 *	aren't any operations that just put out a log record -- this
 *	would mean that undo operations would only reset marks, and not
 *	cause any other change.
 */
int
log_mark(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;
{
	DBT data, key;

	if (F_ISSET(ep, F_NOLOG))
		return (0);

	/* Put out one initial cursor record per set of changes. */
	if (ep->l_cursor.lno != OOBLNO) {
		if (log_cursor1(sp, ep, LOG_CURSOR_INIT))
			return (1);
		ep->l_cursor.lno = OOBLNO;
	}

	BINC_RET(sp, ep->l_lp,
	    ep->l_len, sizeof(u_char) + sizeof(MARK));
	ep->l_lp[0] = LOG_MARK;
	memmove(ep->l_lp + sizeof(u_char), mp, sizeof(MARK));

	key.data = &ep->l_cur;
	key.size = sizeof(recno_t);
	data.data = ep->l_lp;
	data.size = sizeof(u_char) + sizeof(MARK);
	if (ep->log->put(ep->log, &key, &data, 0) == -1)
		LOG_ERR;

	/* Reset high water mark. */
	ep->l_high = ++ep->l_cur;
	return (0);
}

/*
 * Log_backward --
 *	Roll the log backward one operation.
 */
int
log_backward(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop;
	u_char *p;

	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
		    "Logging not being performed, undo not possible.");
		return (1);
	}

	if (ep->l_cur == 1) {
		msgq(sp, M_BERR, "No changes to undo.");
		return (1);
	}

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	key.data = &ep->l_cur;		/* Initialize db request. */
	key.size = sizeof(recno_t);
	for (didop = 0;;) {
		--ep->l_cur;
		if (ep->log->get(ep->log, &key, &data, 0))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_backward", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)data.data)) {
		case LOG_CURSOR_INIT:
			if (didop) {
				memmove(rp, p + sizeof(u_char), sizeof(MARK));
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_CURSOR_END:
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_dline(sp, ep, lno))
				goto err;
			++sp->rptlines[L_DELETED];
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_iline(sp, ep, lno, p + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			++sp->rptlines[L_ADDED];
			break;
		case LOG_LINE_RESET_F:
			break;
		case LOG_LINE_RESET_B:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_sline(sp, ep, lno, p + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			++sp->rptlines[L_CHANGED];
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (mark_set(sp, ep, m.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	return (1);
}

/*
 * Log_setline --
 *	Reset the line to its original appearance.
 *
 * XXX
 * There's a bug in this code due to our not logging cursor movements
 * unless a change was made.  If you do a change, move off the line,
 * then move back on and do a 'U', the line will be restored to the way
 * it was before the original change.
 */
int
log_setline(sp, ep)
	SCR *sp;
	EXF *ep;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	u_char *p;

	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
		    "Logging not being performed, undo not possible.");
		return (1);
	}

	if (ep->l_cur == 1)
		return (1);

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	key.data = &ep->l_cur;		/* Initialize db request. */
	key.size = sizeof(recno_t);

	for (;;) {
		--ep->l_cur;
		if (ep->log->get(ep->log, &key, &data, 0))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_setline", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)data.data)) {
		case LOG_CURSOR_INIT:
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (m.lno != sp->lno || ep->l_cur == 1) {
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_CURSOR_END:
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (m.lno != sp->lno) {
				++ep->l_cur;
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
		case LOG_LINE_DELETE:
		case LOG_LINE_RESET_F:
			break;
		case LOG_LINE_RESET_B:
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (lno == sp->lno &&
			    file_sline(sp, ep, lno, p + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			++sp->rptlines[L_CHANGED];
		case LOG_MARK:
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (mark_set(sp, ep, m.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	return (1);
}

/*
 * Log_forward --
 *	Roll the log forward one operation.
 */
int
log_forward(sp, ep, rp)
	SCR *sp;
	EXF *ep;
	MARK *rp;
{
	DBT key, data;
	MARK m;
	recno_t lno;
	int didop;
	u_char *p;

	if (F_ISSET(ep, F_NOLOG)) {
		msgq(sp, M_ERR,
		    "Logging not being performed, roll-forward not possible.");
		return (1);
	}

	if (ep->l_cur == ep->l_high) {
		msgq(sp, M_BERR, "No changes to re-do.");
		return (1);
	}

	F_SET(ep, F_NOLOG);		/* Turn off logging. */

	key.data = &ep->l_cur;		/* Initialize db request. */
	key.size = sizeof(recno_t);
	for (didop = 0;;) {
		++ep->l_cur;
		if (ep->log->get(ep->log, &key, &data, 0))
			LOG_ERR;
#if defined(DEBUG) && 0
		log_trace(sp, "log_forward", ep->l_cur, data.data);
#endif
		switch (*(p = (u_char *)data.data)) {
		case LOG_CURSOR_END:
			if (didop) {
				++ep->l_cur;
				memmove(rp, p + sizeof(u_char), sizeof(MARK));
				F_CLR(ep, F_NOLOG);
				return (0);
			}
			break;
		case LOG_CURSOR_INIT:
			break;
		case LOG_LINE_APPEND:
		case LOG_LINE_INSERT:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_iline(sp, ep, lno, p + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			++sp->rptlines[L_ADDED];
			break;
		case LOG_LINE_DELETE:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_dline(sp, ep, lno))
				goto err;
			++sp->rptlines[L_DELETED];
			break;
		case LOG_LINE_RESET_B:
			break;
		case LOG_LINE_RESET_F:
			didop = 1;
			memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
			if (file_sline(sp, ep, lno, p + sizeof(u_char) +
			    sizeof(recno_t), data.size - sizeof(u_char) -
			    sizeof(recno_t)))
				goto err;
			++sp->rptlines[L_CHANGED];
			break;
		case LOG_MARK:
			didop = 1;
			memmove(&m, p + sizeof(u_char), sizeof(MARK));
			if (mark_set(sp, ep, m.name, &m, 0))
				goto err;
			break;
		default:
			abort();
		}
	}

err:	F_CLR(ep, F_NOLOG);
	return (1);
}

#if defined(DEBUG) && 0
static void
log_trace(sp, msg, rno, p)
	SCR *sp;
	char *msg;
	recno_t rno;
	u_char *p;
{
	MARK m;
	recno_t lno;

	switch (*p) {
	case LOG_CURSOR_INIT:
		memmove(&m, p + sizeof(u_char), sizeof(MARK));
		TRACE(sp, "%lu: %s:  C_INIT: %u/%u\n", rno, msg, m.lno, m.cno);
		break;
	case LOG_CURSOR_END:
		memmove(&m, p + sizeof(u_char), sizeof(MARK));
		TRACE(sp, "%lu: %s:   C_END: %u/%u\n", rno, msg, m.lno, m.cno);
		break;
	case LOG_LINE_APPEND:
		memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
		TRACE(sp, "%lu: %s:  APPEND: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_INSERT:
		memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
		TRACE(sp, "%lu: %s:  INSERT: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_DELETE:
		memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
		TRACE(sp, "%lu: %s:  DELETE: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_RESET_F:
		memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
		TRACE(sp, "%lu: %s: RESET_F: %lu\n", rno, msg, lno);
		break;
	case LOG_LINE_RESET_B:
		memmove(&lno, p + sizeof(u_char), sizeof(recno_t));
		TRACE(sp, "%lu: %s: RESET_B: %lu\n", rno, msg, lno);
		break;
	case LOG_MARK:
		memmove(&m, p + sizeof(u_char), sizeof(MARK));
		TRACE(sp, "%lu: %s:    MARK: %u/%u\n", rno, msg, m.lno, m.cno);
		break;
	default:
		abort();
	}
}
#endif
