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
static char sccsid[] = "@(#)v_delete.c	8.7 (Berkeley) 1/11/94";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Delete -- [buffer][count]D
 *	Delete line command.
 */
int
v_Delete(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			return (0);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		return (0);

	tm->lno = fm->lno;
	tm->cno = len;

	/* Yank the lines. */
	if (cut(sp, ep, NULL,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, CUT_DELETE))
		return (1);
	if (delete(sp, ep, fm, tm, 0))
		return (1);

	rp->lno = fm->lno;
	rp->cno = fm->cno ? fm->cno - 1 : 0;
	return (0);
}

/*
 * v_delete -- [buffer][count]d[count]motion
 *	Delete a range of text.
 */
int
v_delete(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t nlines;
	size_t len;
	int lmode;
	
	/* Yank the lines. */
	lmode = F_ISSET(vp, VC_LMODE) ? CUT_LINEMODE : 0;
	if (cut(sp, ep, NULL,
	    F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    fm, tm, lmode | CUT_DELETE))
		return (1);
	if (delete(sp, ep, fm, tm, lmode))
		return (1);

	/* Check for deleting the file. */
	if (file_lline(sp, ep, &nlines))
		return (1);
	if (nlines == 0) {
		rp->lno = 1;
		rp->cno = 0;
		return (0);
	}

	/*
	 * If deleting lines, leave the cursor at the lowest line deleted,
	 * else, leave the cursor where it started.  Always correct for EOL.
	 *
	 * The historic vi would delete the line the cursor was on (even if
	 * not in line mode) if the motion from the cursor was past the EOF
	 * and the cursor didn't originate on the last line of the file.  A
	 * strange special case.  We never delete the line the cursor is on.
	 * We'd have to pass a flag down to the delete() routine which would
	 * have to special case it.
	 */
	if (lmode) {
		rp->lno = MIN(fm->lno, tm->lno);
		if (rp->lno > nlines)
			rp->lno = nlines;
		rp->cno = 0;
		(void)nonblank(sp, ep, rp->lno, &rp->cno);
		return (0);
	}

	rp->lno = fm->lno;
	if (file_gline(sp, ep, rp->lno, &len) == NULL) {
		GETLINE_ERR(sp, rp->lno);
		return (1);
	}
	if (fm->cno >= len)
		rp->cno = len ? len - 1 : 0;
	else
		rp->cno = fm->cno;
	return (0);
}
