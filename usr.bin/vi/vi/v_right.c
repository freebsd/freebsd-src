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
static char sccsid[] = "@(#)v_right.c	8.6 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_right -- [count]' ', [count]l
 *	Move right by columns.
 */
int
v_right(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;

	if (file_gline(sp, ep, vp->m_start.lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	/* It's always illegal to move right on empty lines. */
	if (len == 0) {
		v_eol(sp, ep, NULL);
		return (1);
	}

	/*
	 * Non-motion commands move to the end of the range.  VC_D and
	 * VC_Y stay at the start.  Ignore VC_C and VC_S.  Adjust the
	 * end of the range for motion commands.
	 *
	 * !!!
	 * Historically, "[cdsy]l" worked at the end of a line.  Also,
	 * EOL is a count sink.
	 */
	vp->m_stop.cno = vp->m_start.cno +
	    (F_ISSET(vp, VC_C1SET) ? vp->count : 1);
	if (vp->m_start.cno == len - 1) {
		if (!ISMOTION(vp)) {
			v_eol(sp, ep, NULL);
			return (1);
		}
		vp->m_stop.cno = vp->m_start.cno;
	} else if (vp->m_stop.cno > len - 1)
		vp->m_stop.cno = len - 1;

	if (ISMOTION(vp)) {
		--vp->m_stop.cno;
		vp->m_final = vp->m_start;
	} else
		vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_dollar -- [count]$
 *	Move to the last column.
 */
int
v_dollar(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t len;

	/*
	 * !!!
	 * A count moves down count - 1 rows, so, "3$" is the same as "2j$".
	 */
	if ((F_ISSET(vp, VC_C1SET) ? vp->count : 1) != 1) {
		/*
		 * !!!
		 * Historically, if the $ is a motion, and deleting from
		 * at or before the first non-blank of the line, it's a
		 * line motion, and the line motion flag is set.
		 */
		vp->m_stop.cno = 0;
		if (nonblank(sp, ep, vp->m_start.lno, &vp->m_stop.cno))
			return (1);
		if (ISMOTION(vp) && vp->m_start.cno <= vp->m_stop.cno)
			F_SET(vp, VM_LMODE);

		--vp->count;
		if (v_down(sp, ep, vp))
			return (1);
	}

	if (file_gline(sp, ep, vp->m_stop.lno, &len) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			v_eol(sp, ep, NULL);
		else
			GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	/*
	 * Non-motion commands move to the end of the range.
	 * VC_D and VC_Y stay at the start.  Ignore VC_C and VC_S.
	 */
	vp->m_stop.cno = len ? len - 1 : 0;
	vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	return (0);
}
