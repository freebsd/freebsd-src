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
static char sccsid[] = "@(#)v_right.c	8.3 (Berkeley) 12/16/93";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 *
 * Special case: the 'c' and 'd' commands can move past the end of line.
 */
int
v_right(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	u_long cnt;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, fm->lno);
		return (1);
	}
		
	rp->lno = fm->lno;
	if (len == 0 || fm->cno == len - 1) {
		if (F_ISSET(vp, VC_C | VC_D | VC_Y)) {
			rp->cno = len;
			return (0);
		}
		v_eol(sp, ep, NULL);
		return (1);
	}

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	rp->cno = fm->cno + cnt;
	if (rp->cno > len - 1)
		if (F_ISSET(vp, VC_C | VC_D | VC_Y))
			rp->cno = len;
		else
			rp->cno = len - 1;
	return (0);
}

/*
 * v_dollar -- [count]$
 *	Move to the last column.
 */
int
v_dollar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;
	u_long cnt;

	/*
	 * A count moves down count - 1 rows, so, "3$" is the same as "2j$".
	 *
	 * !!!
	 * Historically, if the $ is a motion, and deleting from at or before
	 * the first non-blank of the line, it's a line motion, and the line
	 * motion flag is set.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if (cnt != 1) {
		--vp->count;
		if (v_down(sp, ep, vp, fm, tm, rp))
			return (1);
		rp->cno = 0;
		if (nonblank(sp, ep, rp->lno, &rp->cno))
			return (1);
		if (fm->cno <= rp->cno && F_ISSET(vp, VC_C | VC_D | VC_Y))
			F_SET(vp, VC_LMODE);
	} else
		rp->lno = fm->lno;

	if (file_gline(sp, ep, rp->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, rp->lno);
		return (1);
	}
		
	if (len == 0) {
		v_eol(sp, ep, NULL);
		return (1);
	}

	/* If it's a motion component, move one past the end of the line. */
	rp->cno = F_ISSET(vp, VC_C | VC_D | VC_Y) ? len : len - 1;
	return (0);
}
