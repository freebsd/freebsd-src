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
static char sccsid[] = "@(#)v_z.c	8.8 (Berkeley) 12/2/93";
#endif /* not lint */

#include <sys/types.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_z -- [count]z[count][-.+^<CR>]
 *	Move the screen.
 */
int
v_z(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t last, lno;
	u_int value;

	/*
	 * The first count is the line to use.  If the value doesn't
	 * exist, use the last line.
	 */
	if (F_ISSET(vp, VC_C1SET)) {
		lno = vp->count;
		if (file_lline(sp, ep, &last))
			return (1);
		if (lno > last)
			lno = last;
	} else
		lno = fm->lno;

	/* Set return cursor values. */
	rp->lno = lno;
	rp->cno = fm->cno;

	/*
	 * The second count is the displayed window size, i.e. the 'z'
	 * command is another way to get artificially small windows.
	 *
	 * !!!
	 * A window size of 0 was historically allowed, and simply ignored.
	 * Also, this could be much more simply done by modifying the value
	 * of the O_WINDOW option, but that's not how it worked historically.
	 */
	if (F_ISSET(vp, VC_C2SET) &&
	    vp->count2 != 0 && sp->s_rrel(sp, vp->count2))
		return (1);

	switch (vp->character) {
	case '-':		/* Put the line at the bottom. */
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		break;
	case '.':		/* Put the line in the middle. */
		if (sp->s_fill(sp, ep, lno, P_MIDDLE))
			return (1);
		break;
	default:		/* Put the line at the top for <cr>. */
		value = term_key_val(sp, vp->character);
		if (value != K_CR && value != K_NL) {
			msgq(sp, M_ERR, "usage: %s.", vp->kp->usage);
			return (1);
		}
		/* FALLTHROUGH */
	case '+':		/* Put the line at the top. */
		if (sp->s_fill(sp, ep, lno, P_TOP))
			return (1);
		break;
	case '^':		/* Print the screen before the z- screen. */
		/*
		 * !!!
		 * Historic practice isn't real clear on this one.  It seems
		 * that the command "70z^" is the same as ":70<cr>z-z^" with
		 * an off-by-one difference.  So, until I find documentation
		 * to the contrary, the z^ command in this implementation
		 * displays the screen immediately before the current one.
		 * Fill the screen with the selected line at the bottom, then,
		 * scroll the screen down a page, and move to the middle line
		 * of the screen.  Historic vi moved the cursor to some random
		 * place in the screen, as far as I can tell.
		 */
		if (sp->s_fill(sp, ep, lno, P_BOTTOM))
			return (1);
		if (sp->s_down(sp, ep, rp, sp->t_maxrows - 1, 1))
			return (1);
		if (sp->s_position(sp, ep, rp, 0, P_MIDDLE))
			return (1);
		break;
	}

	/* If the map changes, have to redraw the entire screen. */
	F_SET(sp, S_REDRAW);

	return (0);
}
