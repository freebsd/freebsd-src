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
static char sccsid[] = "@(#)v_paragraph.c	8.9 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * Paragraphs are empty lines after text or values from the paragraph
 * or section options.
 */

/*
 * v_paragraphf -- [count]}
 *	Move forward count paragraphs.
 */
int
v_paragraphf(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	enum { P_INTEXT, P_INBLANK } pstate;
	size_t lastlen, len;
	recno_t cnt, lastlno, lno;
	char *p, *lp;

	/*
	 * !!!
	 * If the starting cursor position is at or before any non-blank
	 * characters in the line, i.e. the movement is cutting all of the
	 * line's text, the buffer is in line mode.  It's a lot easier to
	 * check here, because we know that the end is going to be the start
	 * or end of a line.
	 *
	 * This was historical practice in vi, with a single exception.  If
	 * the paragraph movement was from the start of the last line to EOF,
	 * then all the characters were deleted from the last line, but the
	 * line itself remained.  If somebody complains, don't pause, don't
	 * hesitate, just hit them.
	 */
	if (ISMOTION(vp))
		if (vp->m_start.cno == 0)
			F_SET(vp, VM_LMODE);
		else {
			vp->m_stop = vp->m_start;
			vp->m_stop.cno = 0;
			if (nonblank(sp, ep, vp->m_stop.lno, &vp->m_stop.cno))
				return (1);
			if (vp->m_start.cno <= vp->m_stop.cno)
				F_SET(vp, VM_LMODE);
		}

	/* Figure out what state we're currently in. */
	lno = vp->m_start.lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL)
		goto eof;

	/*
	 * If we start in text, we want to switch states
	 * (2 * N - 1) times, in non-text, (2 * N) times.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	cnt *= 2;
	if (len == 0 || v_isempty(p, len))
		pstate = P_INBLANK;
	else {
		--cnt;
		pstate = P_INTEXT;
	}

	for (;;) {
		lastlno = lno;
		lastlen = len;
		if ((p = file_gline(sp, ep, ++lno, &len)) == NULL)
			goto eof;
		switch (pstate) {
		case P_INTEXT:
			if (p[0] == '.' && len >= 2)
				for (lp = VIP(sp)->paragraph; *lp; lp += 2)
					if (lp[0] == p[1] &&
					    (lp[1] == ' ' || lp[1] == p[2]) &&
					    !--cnt)
						goto found;
			if (len == 0 || v_isempty(p, len)) {
				if (!--cnt)
					goto found;
				pstate = P_INBLANK;
			}
			break;
		case P_INBLANK:
			if (len == 0 || v_isempty(p, len))
				break;
			if (--cnt) {
				pstate = P_INTEXT;
				break;
			}
			/*
			 * !!!
			 * Non-motion commands move to the end of the range,
			 * VC_D and VC_Y stay at the start.  Ignore VC_C and
			 * VC_S.  Adjust end of the range for motion commands;
			 * historically, a motion component was to the end of
			 * the previous line, whereas the movement command was
			 * to the start of the new "paragraph".
			 */
found:			if (ISMOTION(vp)) {
				vp->m_stop.lno = lastlno;
				vp->m_stop.cno = lastlen ? lastlen - 1 : 0;
				vp->m_final = vp->m_start;
			} else {
				vp->m_stop.lno = lno;
				vp->m_stop.cno = 0;
				vp->m_final = vp->m_stop;
			}
			return (0);
		default:
			abort();
		}
	}

	/*
	 * !!!
	 * Adjust end of the range for motion commands; EOF is a movement
	 * sink.  The } command historically moved to the end of the last
	 * line, not the beginning, from any position before the end of the
	 * last line.
	 */
eof:	if (vp->m_start.lno == lno - 1) {
		if (file_gline(sp, ep, vp->m_start.lno, &len) == NULL) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		if (vp->m_start.cno == (len ? len - 1 : 0)) {
			v_eof(sp, ep, NULL);
			return (1);
		}
	}
	/*
	 * !!!
	 * Non-motion commands move to the end of the range, VC_D and
	 * VC_Y stay at the start.  Ignore VC_C and VC_S.
	 *
	 * If deleting the line (which happens if deleting to EOF),
	 * then cursor movement is to the first nonblank.
	 */
	if (F_ISSET(vp, VC_D)) {
		F_CLR(vp, VM_RCM_MASK);
		F_SET(vp, VM_RCM_SETFNB);
	}
	vp->m_stop.lno = lno - 1;
	vp->m_stop.cno = len ? len - 1 : 0;
	vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	return (0);
}

/*
 * v_paragraphb -- [count]{
 *	Move backward count paragraphs.
 */
int
v_paragraphb(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	enum { P_INTEXT, P_INBLANK } pstate;
	size_t len;
	recno_t cnt, lno;
	char *p, *lp;

	/*
	 * !!!
	 * Check for SOF.  The historic vi didn't complain if users hit SOF
	 * repeatedly, unless it was part of a motion command.  There is no
	 * question but that Emerson's editor of choice was vi.
	 *
	 * The { command historically moved to the beginning of the first
	 * line if invoked on the first line.
	 *
	 * !!!
	 * If the starting cursor position is in the first column (backward
	 * paragraph movements did NOT historically pay attention to non-blank
	 * characters) i.e. the movement is cutting the entire line, the buffer
	 * is in line mode.  Cuts from the beginning of the line also did not
	 * cut the current line, but started at the previous EOL.
	 *
	 * Correct for a left motion component while we're thinking about it.
	 */
	lno = vp->m_start.lno;
	if (ISMOTION(vp))
		if (vp->m_start.cno == 0) {
			if (vp->m_start.lno == 1) {
				v_sof(sp, &vp->m_start);
				return (1);
			} else
				--vp->m_start.lno;
			F_SET(vp, VM_LMODE);
		} else
			--vp->m_start.cno;

	if (vp->m_start.lno <= 1)
		goto sof;

	/* Figure out what state we're currently in. */
	if ((p = file_gline(sp, ep, lno, &len)) == NULL)
		goto sof;

	/*
	 * If we start in text, we want to switch states
	 * (2 * N - 1) times, in non-text, (2 * N) times.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	cnt *= 2;
	if (len == 0 || v_isempty(p, len))
		pstate = P_INBLANK;
	else {
		--cnt;
		pstate = P_INTEXT;
	}

	for (;;) {
		if ((p = file_gline(sp, ep, --lno, &len)) == NULL)
			goto sof;
		switch (pstate) {
		case P_INTEXT:
			if (p[0] == '.' && len >= 2)
				for (lp = VIP(sp)->paragraph; *lp; lp += 2)
					if (lp[0] == p[1] &&
					    (lp[1] == ' ' || lp[1] == p[2]) &&
					    !--cnt)
						goto ret;
			if (len == 0 || v_isempty(p, len)) {
				if (!--cnt)
					goto ret;
				pstate = P_INBLANK;
			}
			break;
		case P_INBLANK:
			if (len != 0 && !v_isempty(p, len)) {
				if (!--cnt)
					goto ret;
				pstate = P_INTEXT;
			}
			break;
		default:
			abort();
		}
	}

	/* SOF is a movement sink. */
sof:	lno = 1;

ret:	vp->m_stop.lno = lno;
	vp->m_stop.cno = 0;

	/*
	 * VC_D and non-motion commands move to the end of the range,
	 * VC_Y stays at the start.  Ignore VC_C and VC_S.
	 */
	vp->m_final = F_ISSET(vp, VC_Y) ? vp->m_start : vp->m_stop;
	return (0);
}

/*
 * v_buildparagraph --
 *	Build the paragraph command search pattern.
 */
int
v_buildparagraph(sp)
	SCR *sp;
{
	VI_PRIVATE *vip;
	size_t p_len, s_len;
	char *p, *p_p, *s_p;

	/*
	 * The vi paragraph command searches for either a paragraph or
	 * section option macro.
	 */
	p_len = (p_p = O_STR(sp, O_PARAGRAPHS)) == NULL ? 0 : strlen(p_p);
	s_len = (s_p = O_STR(sp, O_SECTIONS)) == NULL ? 0 : strlen(s_p);

	if (p_len == 0 && s_len == 0)
		return (0);

	MALLOC_RET(sp, p, char *, p_len + s_len + 1);

	vip = VIP(sp);
	if (vip->paragraph != NULL)
		FREE(vip->paragraph, vip->paragraph_len);

	if (p_p != NULL)
		memmove(p, p_p, p_len + 1);
	if (s_p != NULL)
		memmove(p + p_len, s_p, s_len + 1);
	vip->paragraph = p;
	vip->paragraph_len = p_len + s_len + 1;
	return (0);
}
