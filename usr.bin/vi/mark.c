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
static char sccsid[] = "@(#)mark.c	8.12 (Berkeley) 12/27/93";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"

static MARK *mark_find __P((SCR *, EXF *, ARG_CHAR_T));

/*
 * Marks are maintained in a key sorted doubly linked list.  We can't
 * use arrays because we have no idea how big an index key could be.
 * The underlying assumption is that users don't have more than, say,
 * 10 marks at any one time, so this will be is fast enough.
 *
 * Marks are fixed, and modifications to the line don't update the mark's
 * position in the line.  This can be hard.  If you add text to the line,
 * place a mark in that text, undo the addition and use ` to move to the
 * mark, the location will have disappeared.  It's tempting to try to adjust
 * the mark with the changes in the line, but this is hard to do, especially
 * if we've given the line to v_ntext.c:v_ntext() for editing.  Historic vi
 * would move to the first non-blank on the line when the mark location was
 * past the end of the line.  This can be complicated by deleting to a mark
 * that has disappeared using the ` command.  Historic vi vi treated this as
 * a line-mode motion and deleted the line.  This implementation complains to
 * the user.
 *
 * In historic vi, marks returned if the operation was undone, unless the
 * mark had been subsequently reset.  Tricky.  This is hard to start with,
 * but in the presence of repeated undo it gets nasty.  When a line is
 * deleted, we delete (and log) any marks on that line.  An undo will create
 * the mark.  Any mark creations are noted as to whether the user created
 * it or if it was created by an undo.  The former cannot be reset by another
 * undo, but the latter may. 
 *
 * All of these routines translate ABSMARK2 to ABSMARK1.  Setting either of
 * the absolute mark locations sets both, so that "m'" and "m`" work like
 * they, ah, for lack of a better word, "should".
 */

/*
 * mark_init --
 *	Set up the marks.
 */
int
mark_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK *mp;

	/*
	 * Make sure the marks have been set up.  If they
	 * haven't, do so, and create the absolute mark.
	 */
	MALLOC_RET(sp, mp, MARK *, sizeof(MARK));
	mp->lno = 1;
	mp->cno = 0;
	mp->name = ABSMARK1;
	mp->flags = 0;
	LIST_INSERT_HEAD(&ep->marks, mp, q);
	return (0);
}

/*
 * mark_end --
 *	Free up the marks.
 */
int
mark_end(sp, ep)
	SCR *sp;
	EXF *ep;
{
	MARK *mp;

	while ((mp = ep->marks.lh_first) != NULL) {
		LIST_REMOVE(mp, q);
		FREE(mp, sizeof(MARK));
	}
	return (0);
}

/*
 * mark_get --
 *	Get the location referenced by a mark.
 */
MARK *
mark_get(sp, ep, key)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
{
	MARK *mp;
	size_t len;
	char *p;

	if (key == ABSMARK2)
		key = ABSMARK1;

	mp = mark_find(sp, ep, key);
	if (mp == NULL || mp->name != key) {
		msgq(sp, M_BERR, "Mark %s: not set.", charname(sp, key));
                return (NULL);
	}
	if (F_ISSET(mp, MARK_DELETED)) {
		msgq(sp, M_BERR,
		    "Mark %s: the line was deleted.", charname(sp, key));
                return (NULL);
	}
	if ((p = file_gline(sp, ep, mp->lno, &len)) == NULL ||
	    mp->cno > len || mp->cno == len && len != 0) {
		msgq(sp, M_BERR, "Mark %s: cursor position no longer exists.",
		    charname(sp, key));
		return (NULL);
	}
	return (mp);
}

/*
 * mark_set --
 *	Set the location referenced by a mark.
 */
int
mark_set(sp, ep, key, value, userset)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
	MARK *value;
	int userset;
{
	MARK *mp, *mt;

	if (key == ABSMARK2)
		key = ABSMARK1;

	/*
	 * The rules are simple.  If the user is setting a mark (if it's a
	 * new mark this is always true), it always happens.  If not, it's
	 * an undo, and we set it if it's not already set or if it was set
	 * by a previous undo.
	 */
	mp = mark_find(sp, ep, key);
	if (mp == NULL || mp->name != key) {
		MALLOC_RET(sp, mt, MARK *, sizeof(MARK));
		if (mp == NULL) {
			LIST_INSERT_HEAD(&ep->marks, mt, q);
		} else
			LIST_INSERT_AFTER(mp, mt, q);
		mp = mt;
	} else if (!userset &&
	    !F_ISSET(mp, MARK_DELETED) && F_ISSET(mp, MARK_USERSET))
		return (0);

	mp->lno = value->lno;
	mp->cno = value->cno;
	mp->name = key;
	mp->flags = userset ? MARK_USERSET : 0;
	return (0);
}

/*
 * mark_find --
 *	Find the requested mark, or, the slot immediately before
 *	where it would go.
 */
static MARK *
mark_find(sp, ep, key)
	SCR *sp;
	EXF *ep;
	ARG_CHAR_T key;
{
	MARK *mp, *lastmp;

	/*
	 * Return the requested mark or the slot immediately before
	 * where it should go.
	 */
	for (lastmp = NULL, mp = ep->marks.lh_first;
	    mp != NULL; lastmp = mp, mp = mp->q.le_next)
		if (mp->name >= key)
			return (mp->name == key ? mp : lastmp);
	return (lastmp);
}

/*
 * mark_insdel --
 *	Update the marks based on an insertion or deletion.
 */
void
mark_insdel(sp, ep, op, lno)
	SCR *sp;
	EXF *ep;
	enum operation op;
	recno_t lno;
{
	MARK *mp;

	switch (op) {
	case LINE_APPEND:
		return;
	case LINE_DELETE:
		for (mp = ep->marks.lh_first; mp != NULL; mp = mp->q.le_next)
			if (mp->lno >= lno)
				if (mp->lno == lno) {
					F_SET(mp, MARK_DELETED);
					(void)log_mark(sp, ep, mp);
				} else
					--mp->lno;
		return;
	case LINE_INSERT:
		for (mp = ep->marks.lh_first; mp != NULL; mp = mp->q.le_next)
			if (mp->lno >= lno)
				++mp->lno;
		return;
	case LINE_RESET:
		return;
	}
	/* NOTREACHED */
}
