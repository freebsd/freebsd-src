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
static char sccsid[] = "@(#)v_match.c	8.10 (Berkeley) 3/10/94";
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
#include "vcmd.h"

/*
 * v_match -- %
 *	Search to matching character.
 */
int
v_match(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	VCS cs;
	MARK *mp;
	recno_t lno;
	size_t cno, len, off;
	int cnt, matchc, startc, (*gc)__P((SCR *, EXF *, VCS *));
	char *p;

	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			goto nomatch;
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	/*
	 * !!!
	 * Historical practice was to search for the initial character
	 * in the forward direction only.
	 */
	for (off = vp->m_start.cno;; ++off) {
		if (off >= len) {
nomatch:		msgq(sp, M_BERR, "No match character on this line.");
			return (1);
		}
		switch (startc = p[off]) {
		case '(':
			matchc = ')';
			gc = cs_next;
			break;
		case ')':
			matchc = '(';
			gc = cs_prev;
			break;
		case '[':
			matchc = ']';
			gc = cs_next;
			break;
		case ']':
			matchc = '[';
			gc = cs_prev;
			break;
		case '{':
			matchc = '}';
			gc = cs_next;
			break;
		case '}':
			matchc = '{';
			gc = cs_prev;
			break;
		default:
			continue;
		}
		break;
	}

	cs.cs_lno = vp->m_start.lno;
	cs.cs_cno = off;
	if (cs_init(sp, ep, &cs))
		return (1);
	for (cnt = 1;;) {
		if (gc(sp, ep, &cs))
			return (1);
		if (cs.cs_flags != 0) {
			if (cs.cs_flags == CS_EOF || cs.cs_flags == CS_SOF)
				break;
			continue;
		}
		if (cs.cs_ch == startc)
			++cnt;
		else if (cs.cs_ch == matchc && --cnt == 0)
			break;
	}
	if (cnt) {
		msgq(sp, M_BERR, "Matching character not found.");
		return (1);
	}

	vp->m_stop.lno = cs.cs_lno;
	vp->m_stop.cno = cs.cs_cno;

	/*
	 * If moving right, non-motion commands move to the end of the range.
	 * VC_D and VC_Y stay at the start.  If moving left, non-motion and
	 * VC_D commands move to the end of the range.  VC_Y remains at the
	 * start.  Ignore VC_C and VC_S.
	 *
	 * !!!
	 * Don't correct for leftward movement -- historic vi deleted the
	 * starting cursor position when deleting to a match.
	 */
	if (vp->m_start.lno < vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno < vp->m_stop.cno)
		vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	else
		vp->m_final = ISMOTION(vp) &&
		    F_ISSET(vp, VC_Y) ? vp->m_start : vp->m_stop;

	/*
	 * !!!
	 * If the motion is across lines, and the earliest cursor position
	 * is at or before any non-blank characters in its line, i.e. the
	 * movement is cutting all of the line's text, the buffer is in line
	 * mode.
	 */
	if (ISMOTION(vp) && vp->m_start.lno != vp->m_stop.lno) {
		mp = vp->m_start.lno < vp->m_stop.lno ?
		    &vp->m_start : &vp->m_stop;
		if (mp->cno == 0) {
			F_SET(vp, VM_LMODE);
			return (0);
		}
		cno = 0;
		if (nonblank(sp, ep, mp->lno, &cno))
			return (1);
		if (cno >= mp->cno)
			F_SET(vp, VM_LMODE);
	}
	return (0);
}
