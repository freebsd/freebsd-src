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
static char sccsid[] = "@(#)ex_move.c	8.11 (Berkeley) 3/15/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_copy -- :[line [,line]] co[py] line [flags]
 *	Copy selected lines.
 */
int
ex_copy(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	CB cb;
	MARK fm1, fm2, m, tm;
	recno_t cnt;
	int rval;

	/*
	 * It's possible to copy things into the area that's being
	 * copied, e.g. "2,5copy3" is legitimate.  Save the text to
	 * a cut buffer.
	 */
	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	memset(&cb, 0, sizeof(cb));
	CIRCLEQ_INIT(&cb.textq);
	if (cut(sp, ep, &cb, NULL, &fm1, &fm2, CUT_LINEMODE))
		return (1);

	/* Put the text into place. */
	tm.lno = cmdp->lineno;
	tm.cno = 0;
	if (put(sp, ep, &cb, NULL, &tm, &m, 1))
		rval = 1;
	else {
		/*
		 * Copy puts the cursor on the last line copied.  The cursor
		 * returned by the put routine is the first line put, not the
		 * last, because that's the historic semantic of vi.
		 */
		cnt = (fm2.lno - fm1.lno) + 1;
		sp->lno = m.lno + (cnt - 1);
		sp->cno = 0;

		sp->rptlines[L_COPIED] += cnt;
		rval = 0;
	}
	text_lfree(&cb.textq);
	return (rval);
}

/*
 * ex_move -- :[line [,line]] mo[ve] line
 *	Move selected lines.
 */
int
ex_move(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	LMARK *lmp;
	MARK fm1, fm2;
	recno_t cnt, diff, fl, tl, mfl, mtl;
	size_t len;
	int mark_reset;
	char *p;

	/*
	 * It's not possible to move things into the area that's being
	 * moved.
	 */
	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	if (cmdp->lineno >= fm1.lno && cmdp->lineno < fm2.lno) {
		msgq(sp, M_ERR, "Destination line is inside move range.");
		return (1);
	}

	/*
	 * Log the positions of any marks in the to-be-deleted lines.  This
	 * has to work with the logging code.  What happens is that we log
	 * the old mark positions, make the changes, then log the new mark
	 * positions.  Then the marks end up in the right positions no matter
	 * which way the log is traversed.
	 *
	 * XXX
	 * Reset the MARK_USERSET flag so that the log can undo the mark.
	 * This isn't very clean, and should probably be fixed.
	 */
	fl = fm1.lno;
	tl = cmdp->lineno;

	/* Log the old positions of the marks. */
	mark_reset = 0;
	for (lmp = ep->marks.lh_first; lmp != NULL; lmp = lmp->q.le_next)
		if (lmp->name != ABSMARK1 &&
		    lmp->lno >= fl && lmp->lno <= tl) {
			mark_reset = 1;
			F_CLR(lmp, MARK_USERSET);
			(void)log_mark(sp, ep, lmp);
		}

	/* Move the lines. */
	diff = (fm2.lno - fm1.lno) + 1;
	if (tl > fl) {				/* Destination > source. */
		mfl = tl - diff;
		mtl = tl;
		for (cnt = diff; cnt--;) {
			if ((p = file_gline(sp, ep, fl, &len)) == NULL)
				return (1);
			if (file_aline(sp, ep, 1, tl, p, len))
				return (1);
			if (mark_reset)
				for (lmp = ep->marks.lh_first;
				    lmp != NULL; lmp = lmp->q.le_next)
					if (lmp->name != ABSMARK1 &&
					    lmp->lno == fl)
						lmp->lno = tl + 1;
			if (file_dline(sp, ep, fl))
				return (1);
		}
	} else {				/* Destination < source. */
		mfl = tl;
		mtl = tl + diff;
		for (cnt = diff; cnt--;) {
			if ((p = file_gline(sp, ep, fl, &len)) == NULL)
				return (1);
			if (file_aline(sp, ep, 1, tl++, p, len))
				return (1);
			if (mark_reset)
				for (lmp = ep->marks.lh_first;
				    lmp != NULL; lmp = lmp->q.le_next)
					if (lmp->name != ABSMARK1 &&
					    lmp->lno == fl)
						lmp->lno = tl;
			++fl;
			if (file_dline(sp, ep, fl))
				return (1);
		}
	}
	sp->lno = tl;				/* Last line moved. */
	sp->cno = 0;

	/* Log the new positions of the marks. */
	if (mark_reset)
		for (lmp = ep->marks.lh_first;
		    lmp != NULL; lmp = lmp->q.le_next)
			if (lmp->name != ABSMARK1 &&
			    lmp->lno >= mfl && lmp->lno <= mtl)
				(void)log_mark(sp, ep, lmp);


	sp->rptlines[L_MOVED] += diff;
	return (0);
}
