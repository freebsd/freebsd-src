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
static char sccsid[] = "@(#)v_undo.c	8.6 (Berkeley) 1/8/94";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_Undo -- U
 *	Undo changes to this line.
 */
int
v_Undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * Historically, U reset the cursor to the first column in the line
	 * (not the first non-blank).  This seems a bit non-intuitive, but,
	 * considering that we may have undone multiple changes, anything
	 * else (including the cursor position stored in the logging records)
	 * is going to appear random.
	 */
	rp->lno = fm->lno;
	rp->cno = 0;

	/*
	 * !!!
	 * Set up the flags so that an immediately subsequent 'u' will roll
	 * forward, instead of backward.  In historic vi, a 'u' following a
	 * 'U' redid all of the changes to the line.  Given that the user has
	 * explicitly discarded those changes by entering 'U', it seems likely
	 * that the user wants something between the original and end forms of
	 * the line, so starting to replay the changes seems the best way to
	 * get to there.
	 */
	F_SET(ep, F_UNDO);
	ep->lundo = BACKWARD;

	return (log_setline(sp, ep));
}
	
/*
 * v_undo -- u
 *	Undo the last change.
 */
int
v_undo(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* Set the command count. */
	VIP(sp)->u_ccnt = sp->ccnt;

	/*
	 * !!!
	 * In historic vi, 'u' toggled between "undo" and "redo", i.e. 'u'
	 * undid the last undo.  However, if there has been a change since
	 * the last undo/redo, we always do an undo.  To make this work when
	 * the user can undo multiple operations, we leave the old semantic
	 * unchanged, but make '.' after a 'u' do another undo/redo operation.
	 * This has two problems.
	 *
	 * The first is that 'u' didn't set '.' in historic vi.  So, if a
	 * user made a change, realized it was in the wrong place, does a
	 * 'u' to undo it, moves to the right place and then does '.', the
	 * change was reapplied.  To make this work, we only apply the '.'
	 * to the undo command if it's the command immediately following an
	 * undo command.  See vi/vi.c:getcmd() for the details.
	 *
	 * The second is that the traditional way to view the numbered cut
	 * buffers in vi was to enter the commands "1pu.u.u.u. which will
	 * no longer work because the '.' immediately follows the 'u' command.
	 * Since we provide a much better method of viewing buffers, and
	 * nobody can think of a better way of adding in multiple undo, this
	 * remains broken.
	 */
	if (!F_ISSET(ep, F_UNDO)) {
		F_SET(ep, F_UNDO);
		ep->lundo = BACKWARD;
	} else if (!F_ISSET(vp, VC_ISDOT))
		ep->lundo = ep->lundo == BACKWARD ? FORWARD : BACKWARD;

	switch (ep->lundo) {
	case BACKWARD:
		return (log_backward(sp, ep, rp));
	case FORWARD:
		return (log_forward(sp, ep, rp));
	default:
		abort();
	}
	/* NOTREACHED */
}
