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
static char sccsid[] = "@(#)v_scroll.c	8.8 (Berkeley) 12/16/93";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>

#include "vi.h"
#include "excmd.h"
#include "vcmd.h"

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
v_lgoto(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last;

	if (file_lline(sp, ep, &last))
		return (1);
	if (F_ISSET(vp, VC_C1SET)) {
		if (last < vp->count) {
			v_eof(sp, ep, fm);
			return (1);
		}
		rp->lno = vp->count;
	} else
		rp->lno = last ? last : 1;
	return (0);
}

/* 
 * v_home -- [count]H
 *	Move to the first non-blank character of the logical line
 *	count from the top of the screen, 1 by default.
 */
int
v_home(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->s_position(sp, ep, rp,
	    F_ISSET(vp, VC_C1SET) ? vp->count : 0, P_TOP));
}

/*
 * v_middle -- M
 *	Move to the first non-blank character of the logical line
 *	in the middle of the screen.
 */
int
v_middle(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * Yielding to none in our quest for compatibility with every
	 * historical blemish of vi, no matter how strange it might be,
	 * we permit the user to enter a count and then ignore it.
	 */
	return (sp->s_position(sp, ep, rp, 0, P_MIDDLE));
}

/*
 * v_bottom -- [count]L
 *	Move to the first non-blank character of the logical line
 *	count from the bottom of the screen, 1 by default.
 */
int
v_bottom(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	return (sp->s_position(sp, ep,
	    rp, F_ISSET(vp, VC_C1SET) ? vp->count : 0, P_BOTTOM));
}

/*
 * v_up -- [count]^P, [count]k, [count]-
 *	Move up by lines.
 */
int
v_up(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	lno = F_ISSET(vp, VC_C1SET) ? vp->count : 1;

	if (fm->lno <= lno) {
		v_sof(sp, fm);
		return (1);
	}
	rp->lno = fm->lno - lno;
	return (0);
}

/*
 * v_cr -- [count]^M
 *	In a script window, send the line to the shell.
 *	In a regular window, move down by lines.
 */
int
v_cr(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * If it's a script window, exec the line,
	 * otherwise it's the same as v_down().
	 */
	return (F_ISSET(sp, S_SCRIPT) ?
	    sscr_exec(sp, ep, fm->lno) : v_down(sp, ep, vp, fm, tm, rp));
}

/*
 * v_down -- [count]^J, [count]^N, [count]j, [count]^M, [count]+
 *	Move down by lines.
 */
int
v_down(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;

	lno = fm->lno + (F_ISSET(vp, VC_C1SET) ? vp->count : 1);

	if (file_gline(sp, ep, lno, NULL) == NULL) {
		v_eof(sp, ep, fm);
		return (1);
	}
	rp->lno = lno;
	return (0);
}

/*
 * v_hpageup -- [count]^U
 *	Page up half screens.
 */
int
v_hpageup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* 
	 * Half screens always succeed unless already at SOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	return (sp->s_down(sp, ep, rp, (recno_t)O_VAL(sp, O_SCROLL), 1));
}

/*
 * v_hpagedown -- [count]^D
 *	Page down half screens.
 */
int
v_hpagedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/* 
	 * Half screens always succeed unless already at EOF.  Half screens
	 * set the scroll value, even if the command ultimately failed, in
	 * historic vi.  It's probably a don't care.
	 */
	if (F_ISSET(vp, VC_C1SET))
		O_VAL(sp, O_SCROLL) = vp->count;
	else
		vp->count = O_VAL(sp, O_SCROLL);

	return (sp->s_up(sp, ep, rp, (recno_t)O_VAL(sp, O_SCROLL), 1));
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
v_pageup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1);

	return (sp->s_down(sp, ep, rp, count, 1));
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
v_pagedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t count;

	/* Calculation from POSIX 1003.2/D8. */
	count = (F_ISSET(vp, VC_C1SET) ? vp->count : 1) * (sp->t_rows - 1);

	return (sp->s_up(sp, ep, rp, count, 1));
}

/*
 * v_lineup -- [count]^Y
 *	Page up by lines.
 */
int
v_lineup(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * The cursor moves down, staying with its original line, unless it
	 * reaches the bottom of the screen.
	 */
	return (sp->s_down(sp, ep,
	    rp, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0));
}

/*
 * v_linedown -- [count]^E
 *	Page down by lines.
 */
int
v_linedown(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	/*
	 * The cursor moves up, staying with its original line, unless it
	 * reaches the top of the screen.
	 */
	return (sp->s_up(sp, ep,
	    rp, F_ISSET(vp, VC_C1SET) ? vp->count : 1, 0));
}
