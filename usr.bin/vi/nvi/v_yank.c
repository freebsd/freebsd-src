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
static char sccsid[] = "@(#)v_yank.c	8.11 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Yank --	[buffer][count]Y
 *	Yank lines of text into a cut buffer.
 */
int
v_Yank(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (file_gline(sp, ep, tm->lno, NULL) == NULL) {
		v_eof(sp, ep, fm);
		return (1);
	}
	if (cut(sp, ep, NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    fm, tm, CUT_LINEMODE))
		return (1);

	sp->rptlines[L_YANKED] += (tm->lno - fm->lno) + 1;
	return (0);
}

/*
 * v_yank --	[buffer][count]y[count][motion]
 *	Yank text (or lines of text) into a cut buffer.
 */
int
v_yank(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	if (F_ISSET(vp, VC_LMODE)) {
		if (file_gline(sp, ep, tm->lno, NULL) == NULL) {
			v_eof(sp, ep, fm);
			return (1);
		}
		if (cut(sp, ep,
		    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
		    fm, tm, CUT_LINEMODE))
			return (1);
	} else if (cut(sp, ep,
	    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, 0))
		return (1);

	/*
	 * !!!
	 * Historic vi moved the cursor to the from MARK if it was before the
	 * current cursor.  This makes no sense.  For example, "yj" moves the
	 * cursor but "yk" does not.  Unfortunately, it's too late to change
	 * this now.  Matching the historic semantics isn't easy.  The line
	 * number was always changed and column movement was usually relative.
	 * However, "y'a" moved the cursor to the first non-blank of the line
	 * marked by a, while "y`a" moved the cursor to the line and column
	 * marked by a.
	 */
	if (F_ISSET(vp, VC_REVMOVE)) {
		rp->lno = fm->lno;
		if (vp->mkp == &vikeys['\'']) {
			rp->cno = 0;
			(void)nonblank(sp, ep, rp->lno, &rp->cno);
		} else if (vp->mkp == &vikeys['`'])
			rp->cno = fm->cno;
		else
			rp->cno = sp->s_relative(sp, ep, rp->lno);
	}

	sp->rptlines[L_YANKED] += (tm->lno - fm->lno) + 1;
	return (0);
}
