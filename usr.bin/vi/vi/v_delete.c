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
static char sccsid[] = "@(#)v_delete.c	8.10 (Berkeley) 3/14/94";
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
 * v_Delete -- [buffer][count]D
 *	Delete line command.
 */
int
v_Delete(sp, ep, vp)
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
			return (0);
		GETLINE_ERR(sp, vp->m_start.lno);
		return (1);
	}

	if (len == 0)
		return (0);

	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = len - 1;

	/* Yank the lines. */
	if (cut(sp, ep, NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, CUT_DELETE))
		return (1);
	if (delete(sp, ep, &vp->m_start, &vp->m_stop, 0))
		return (1);

	vp->m_final.lno = vp->m_start.lno;
	vp->m_final.cno = vp->m_start.cno ? vp->m_start.cno - 1 : 0;
	return (0);
}

/*
 * v_delete -- [buffer][count]d[count]motion
 *	Delete a range of text.
 */
int
v_delete(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t nlines;
	size_t len;
	int lmode;

	/* Yank the lines. */
	lmode = F_ISSET(vp, VM_LMODE) ? CUT_LINEMODE : 0;
	if (cut(sp, ep, NULL, F_ISSET(vp, VC_BUFFER) ? &vp->buffer : NULL,
	    &vp->m_start, &vp->m_stop, lmode | CUT_DELETE))
		return (1);
	if (delete(sp, ep, &vp->m_start, &vp->m_stop, lmode))
		return (1);

	/*
	 * Check for deletion of the entire file.  Try to check a close
	 * by line so we don't go to the end of the file unnecessarily.
	 */
	if (file_gline(sp, ep, vp->m_final.lno + 1, &len) == NULL) {
		if (file_lline(sp, ep, &nlines))
			return (1);
		if (nlines == 0) {
			vp->m_final.lno = 1;
			vp->m_final.cno = 0;
			return (0);
		}
	}

	/*
	 * One special correction, in case we've deleted the current line or
	 * character.  We check it here instead of checking in every command
	 * that can be a motion component.
	 */
	if (file_gline(sp, ep, vp->m_final.lno, &len) == NULL) {
		if (file_gline(sp, ep, nlines, &len) == NULL) {
			GETLINE_ERR(sp, nlines);
			return (1);
		}
		vp->m_final.lno = nlines;
	}
	if (vp->m_final.cno >= len)
		vp->m_final.cno = len ? len - 1 : 0;
	return (0);
}
