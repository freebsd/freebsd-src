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
static char sccsid[] = "@(#)v_mark.c	8.6 (Berkeley) 3/8/94";
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
 * v_mark -- m[a-z]
 *	Set a mark.
 */
int
v_mark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark_set(sp, ep, vp->character, &vp->m_start, 1));
}

static int mark __P((SCR *, EXF *, VICMDARG *, enum direction));

/*
 * v_bmark -- `['`a-z]
 *	Move to a mark.
 *
 * Moves to a mark, setting both row and column.
 *
 * !!!
 * Although not commonly known, the "'`" and "'`" forms are historically
 * valid.  The behavior is determined by the first character, so "`'" is
 * the same as "``".  Remember this fact -- you'll be amazed at how many
 * people don't know it and will be delighted that you are able to tell
 * them.
 */
int
v_bmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, BACKWARD));
}

/*
 * v_fmark -- '['`a-z]
 *	Move to a mark.
 *
 * Move to the first nonblank character of the line containing the mark.
 */
int
v_fmark(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (mark(sp, ep, vp, FORWARD));
}

static int
mark(sp, ep, vp, dir)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	enum direction dir;
{
	if (mark_get(sp, ep, vp->character, &vp->m_stop))
		return (1);

	/* Forward marks move to the first non-blank. */
	if (dir == FORWARD) {
		vp->m_stop.cno = 0;
		if (nonblank(sp, ep, vp->m_stop.lno, &vp->m_stop.cno))
			return (1);
	}

	/* Non-motion commands move to the end of the range. */
	if (!ISMOTION(vp)) {
		vp->m_final = vp->m_stop;
		return (0);
	}

	/*
	 * !!!
	 * If a motion component, the cursor has to move.
	 */
	if (vp->m_stop.lno == vp->m_start.lno &&
	    vp->m_stop.cno == vp->m_start.cno) {
		v_nomove(sp);
		return (1);
	}

	/*
	 * If moving right, VC_D and VC_Y stay at the start.  If moving left,
	 * VC_D commands move to the end of the range and VC_Y remains at the
	 * start.  Ignore VC_C and VC_S.  Motion left commands adjust the
	 * starting point to the character before the current one.
	 */
	if (vp->m_start.lno > vp->m_stop.lno ||
	    vp->m_start.lno == vp->m_stop.lno &&
	    vp->m_start.cno > vp->m_stop.cno) {
		if (F_ISSET(vp, VC_D))
			vp->m_final = vp->m_stop;
		else
			vp->m_final = vp->m_start;
		--vp->m_start.cno;
	} else
		vp->m_final = vp->m_start;
	return (0);
}
