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
static char sccsid[] = "@(#)v_xchar.c	8.4 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

#define	NODEL(sp) {							\
	msgq(sp, M_BERR, "No characters to delete.");			\
	return (1);							\
}

/*
 * v_xchar --
 *	Deletes the character(s) on which the cursor sits.
 */
int
v_xchar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	MARK m;
	recno_t lno;
	u_long cnt;
	size_t len;

	if (file_gline(sp, ep, fm->lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NODEL(sp);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NODEL(sp);

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	/*
	 * Deleting from the cursor toward the end of line, w/o moving the
	 * cursor.  Note, "2x" at EOL isn't the same as "xx" because the
	 * left movement of the cursor as part of the 'x' command isn't
	 * taken into account.  Historically correct.
	 */
	tm->lno = fm->lno;
	if (cnt < len - fm->cno) {
		tm->cno = fm->cno + cnt;
		m = *fm;
	} else {
		tm->cno = len;
		m.lno = fm->lno;
		m.cno = fm->cno ? fm->cno - 1 : 0;
	}

	if (cut(sp, ep,
	    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, 0))
		return (1);
	if (delete(sp, ep, fm, tm, 0))
		return (1);

	*rp = m;
	return (0);
}

/*
 * v_Xchar --
 *	Deletes the character(s) immediately before the current cursor
 *	position.
 */
int
v_Xchar(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	u_long cnt;

	if (fm->cno == 0) {
		msgq(sp, M_BERR, "Already at the left-hand margin.");
		return (1);
	}

	*tm = *fm;
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	fm->cno = cnt >= tm->cno ? 0 : tm->cno - cnt;

	if (cut(sp, ep,
	    NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL, fm, tm, 0))
		return (1);
	if (delete(sp, ep, fm, tm, 0))
		return (1);

	*rp = *fm;
	return (0);
}
