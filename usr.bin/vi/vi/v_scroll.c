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
static char sccsid[] = "@(#)v_scroll.c	8.14 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

static void goto_adjust __P((VICMDARG *));

/*
 * The historic vi had a problem in that all movements were by physical
 * lines, not by logical, or screen lines.  Arguments can be made that this
 * is the right thing to do.  For example, single line movements, such as
 * 'j' or 'k', should probably work on physical lines.  Commands like "dj",
 * or "j.", where '.' is a change command, make more sense for physical lines
 * than they do for logical lines.
 *
 * These arguments, however, don't apply to scrolling commands like ^D and
 * ^F -- if the window is fairly small, using physical lines can result in
 * a half-page scroll repainting the entire screen, which is not what the
 * user wanted.  Second, if the line is larger than the screen, using physical
 * lines can make it impossible to display parts of the line -- there aren't
 * any commands that don't display the beginning of the line in historic vi,
 * and if both the beginning and end of the line can't be on the screen at
 * the same time, you lose.  This is even worse in the case of the H, L, and
 * M commands -- for large lines, they may all refer to the same line and
 * will result in no movement at all.
 *
 * This implementation does the scrolling (^B, ^D, ^F, ^U, ^Y, ^E), and the
 * cursor positioning commands (H, L, M) commands using logical lines, not
 * physical.
 *
 * Another issue is that page and half-page scrolling commands historically
 * moved to the first non-blank character in the new line.  If the line is
 * approximately the same size as the screen, this loses because the cursor
 * before and after a ^D, may refer to the same location on the screen.  In
 * this implementation, scrolling commands set the cursor to the first non-
 * blank character if the line changes because of the scroll.  Otherwise,
 * the cursor is left alone.
 */

/*
 * v_lgoto -- [count]G
 *	Go to first non-blank character of the line count, the last line
 *	of the file by default.
 */
int
v_lgoto(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t nlines;

	if (F_ISSET(vp, VC_C1SET)) {
		if (file_gline(sp, ep, vp->count, NULL) == NULL) {
			v_eof(sp, ep, &vp->m_start);
			return (1);
		}
		vp->m_stop.lno = vp->count;
	} else {
		if (file_lline(sp, ep, &nlines))
			return (1);
		vp->m_stop.lno = nlines ? nlines : 1;
	}
	goto_adjust(vp);
	return (0);
}

/*
 * v_home -- [count]H
 *	Move to the first non-blank character of the logical line
 *	count - 1 from the top of the screen, 0 by default.
 */
int
v_home(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	if (sp->s_position(sp, ep, &vp->m_stop,
	    F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0, P_TOP))
		return (1);
	goto_adjust(vp);
	return (0);
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the logical line
 *	in the middle of the screen.
 */
int
v_middle(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * Yielding to none in our quest for compatibility with every
	 * historical blemish of vi, no matter how strange it might be,
	 * we permit the user to enter a count and then ignore it.
	 */
	if (sp->s_position(sp, ep, &vp->m_stop, 0, P_MIDDLE))
		return (1);
	goto_adjust(vp);
	return (0);
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the logical line
 *	count - 1 from the bottom of the screen, 0 by default.
 */
int
v_bottom(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	if (sp->s_position(sp, ep, &vp->m_stop,
	    F_ISSET(vp, VC_C1SET) ? vp->count - 1 : 0, P_BOTTOM))
		return (1);
	goto_adjust(vp);
	return (0);
}

static void
goto_adjust(vp)
	VICMDARG *vp;
{
	/*
	 * !!!
	 * If it's not a yank to the current line or greater, and we've
	 * changed lines, move to the first non-blank of the line.
	 */
	if (!F_ISSET(vp, VC_Y) || vp->m_stop.lno < vp->m_start.lno) {
		F_CLR(vp, VM_RCM_MASK);
		F_SET(vp, VM_RCM_SETLFNB);
	}

	/* Non-motion commands go to the end of the range. */
	vp->m_final = vp->m_stop;
	if (!ISMOTION(vp))
		return;

	/*
	 * If moving backward in the file, VC_D and VC_Y move to the end
	 * of the range, unless the line didn't change, in which case VC_Y
	 * doesn't move.  If moving forward in the file, VC_D and VC_Y stay
	 * at the start of the range.  Ignore VC_C and VC_S.
	 */
	if (vp->m_stop.lno < vp->m_start.lno ||
	    vp->m_stop.lno == vp->m_start.lno &&
	    vp->m_stop.cno < vp->m_start.cno) {
		if (F_ISSET(vp, VC_Y) && vp->m_stop.lno == vp->m_start.lno)
			vp->m_final = vp->m_start;
	} else
		vp->m_final = vp->m_start;
}

/*
 * v_up -- [count]^P, [count]k, [count]-
 *	Move up by lines.
 */
int
v_up(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;

	lno = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	if (vp->m_start.lno <= lno) {
		v_sof(sp, &vp->m_start);
		return (1);
	}
	vp->m_stop.lno = vp->m_start.lno - lno;
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_cr -- [count]^M
 *	In a script window, send the line to the shell.
 *	In a regular window, move down by lines.
 */
int
v_cr(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * If it's a script window, exec the line,
	 * otherwise it's the same as v_down().
	 */
	return (F_ISSET(sp, S_SCRIPT) ?
	    sscr_exec(sp, ep, vp->m_start.lno) : v_down(sp, ep, vp));
}

/*
 * v_down -- [count]^J, [count]^N, [count]j, [count]^M, [count]+
 *	Move down by lines.
 */
int
v_down(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;

	lno = vp->m_start.lno + (F_ISSET(vp, VC_C1SET) ? vp->count : 1);
	if (file_gline(sp, ep, lno, NULL) == NULL) {
		v_eof(sp, ep, &vp->m_start);
		return (1);
	}
	vp->m_stop.lno = lno;
	vp->m_final = ISMOTION(vp) ? vp->m_start : vp->m_stop;
	return (0);
}

/*
 * v_hpageup -- [count]^U
 *	Page up half screens.
 */
int
v_hpageup(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * Half screens always succeed unless already at SOF.
	 *
	 * !!!
	 * Half screens set the scroll value, even if the command ultimately
	 * failed, in historic vi.  Probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	if (sp->s_down(sp, ep, &vp->m_stop, (recno_t)O_VAL(sp, O_SCROLL), 1))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * Half screens always succeed unless already at EOF.
	 *
	 * !!!
	 * Half screens set the scroll value, even if the command ultimately
	 * failed, in historic vi.  Probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	if (sp->s_up(sp, ep, &vp->m_stop, (recno_t)O_VAL(sp, O_SCROLL), 1))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_pageup -- [count]^B
 *	Page up full screens.
 *
 * !!!
 * Historic vi did not move to the SOF if the screen couldn't move, i.e.
 * if SOF was already displayed on the screen.  This implementation does
 * move to SOF in that case, making ^B more like the the historic ^U.
 */
int
v_pageup(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/* Calculation from POSIX 1003.2/D8. */
	if (sp->s_down(sp, ep, &vp->m_stop,
	    (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1), 1))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_pagedown -- [count]^F
 *	Page down full screens.
 * !!!
 * Historic vi did not move to the EOF if the screen couldn't move, i.e.
 * if EOF was already displayed on the screen.  This implementation does
 * move to EOF in that case, making ^F more like the the historic ^D.
 */
int
v_pagedown(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/* Calculation from POSIX 1003.2/D8. */
	if (sp->s_up(sp, ep, &vp->m_stop,
	    (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1), 1))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_lineup -- [count]^Y
 *	Page up by lines.
 */
int
v_lineup(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * The cursor moves down, staying with its original line, unless it
	 * reaches the bottom of the screen.
	 */
	if (sp->s_down(sp, ep,
	    &vp->m_stop, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}

/*
 * v_linedown -- [count]^E
 *	Page down by lines.
 */
int
v_linedown(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.
	 */
	if (sp->s_up(sp, ep,
	    &vp->m_stop, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0))
		return (1);
	vp->m_final = vp->m_stop;
	return (0);
}
