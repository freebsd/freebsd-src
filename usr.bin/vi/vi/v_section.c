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
static char sccsid[] = "@(#)v_section.c	8.6 (Berkeley) 3/8/94";
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
 * In historic vi, the section commands ignored empty lines, unlike the
 * paragraph commands, which was probably okay.  However, they also moved
 * to the start of the last line when there where no more sections instead
 * of the end of the last line like the paragraph commands.  I've changed
 * the latter behavior to match the paragraph commands.
 *
 * In historic vi, a "function" was defined as the first character of the
 * line being an open brace, which could be followed by anything.  This
 * implementation follows that historic practice.
 *
 * !!!
 * The historic vi documentation (USD:15-10) claimed:
 *	The section commands interpret a preceding count as a different
 *	window size in which to redraw the screen at the new location,
 *	and this window size is the base size for newly drawn windows
 *	until another size is specified.  This is very useful if you are
 *	on a slow terminal ...
 *
 * I can't get the 4BSD vi to do this, it just beeps at me.  For now, a
 * count to the section commands simply repeats the command.
 */

static int section __P((SCR *, EXF *, VICMDARG *, int, enum direction));

/*
 * v_sectionf -- [count]]]
 *	Move forward count sections/functions.
 */
int
v_sectionf(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	return (section(sp, ep, vp, 1, FORWARD));
}

/*
 * v_sectionb -- [count][[
 *	Move backward count sections/functions.
 */
int
v_sectionb(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	/* An empty file or starting from line 1 is always illegal. */
	if (vp->m_start.lno <= 1) {
		v_sof(sp, NULL);
		return (1);
	}
	return (section(sp, ep, vp, -1, BACKWARD));
}

static int
section(sp, ep, vp, off, dir)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	int off;
	enum direction dir;
{
	size_t len;
	recno_t cnt, lno;
	int closeok;
	char *p, *list, *lp;

	/* Get the macro list. */
	if ((list = O_STR(sp, O_SECTIONS)) == NULL)
		return (1);

	/*
	 * !!!
	 * Using ]] as a motion command was a bit special, historically.
	 * It could match } as well as the usual { and section values.  If
	 * it matched a { or a section, it did NOT include the matched line.
	 * If it matched a }, it did include the line.  Not a clue why.
	 */
	closeok = ISMOTION(vp) && dir == FORWARD;

	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	for (lno = vp->m_start.lno;
	    (p = file_gline(sp, ep, lno += off, &len)) != NULL;) {
		if (len == 0)
			continue;
		if (p[0] == '{' || closeok && p[0] == '}') {
			if (!--cnt) {
				if (dir == FORWARD && ISMOTION(vp) &&
				    p[0] == '{' &&
				    file_gline(sp, ep, --lno, &len) == NULL)
					return (1);
				vp->m_stop.lno = lno;
				vp->m_stop.cno = len ? len - 1 : 0;
				goto ret;
			}
			continue;
		}
		if (p[0] != '.' || len < 3)
			continue;
		for (lp = list; *lp != '\0'; lp += 2 * sizeof(*lp))
			if (lp[0] == p[1] &&
			    (lp[1] == ' ' || lp[1] == p[2]) && !--cnt) {
				if (dir == FORWARD && ISMOTION(vp) &&
				    file_gline(sp, ep, --lno, &len) == NULL)
					return (1);
				vp->m_stop.lno = lno;
				vp->m_stop.cno = len ? len - 1 : 0;
				goto ret;
			}
	}

	/*
	 * If moving forward, reached EOF, if moving backward, reached SOF.
	 * Both are movement sinks.  The calling code has already checked
	 * for SOF, so all we check is EOF.
	 */
	if (dir == FORWARD) {
		if (vp->m_start.lno == lno - 1) {
			v_eof(sp, ep, NULL);
			return (1);
		}
		vp->m_stop.lno = lno - 1;
		vp->m_stop.cno = len ? len - 1 : 0;
	} else {
		vp->m_stop.lno = 1;
		vp->m_stop.cno = 0;
	}

	/*
	 * Non-motion commands go to the end of the range.  If moving backward
	 * in the file, VC_D and VC_Y move to the end of the range.  If moving
	 * forward in the file, VC_D and VC_Y stay at the start of the range.
	 * Ignore VC_C and VC_S.
	 *
	 * !!!
	 * Historic practice is the section cut was in line mode if it started
	 * from column 0 and was in the backward direction.  I don't know why
	 * you'd want to cut an entire section in character mode, so I do it in
	 * line mode in both directions if the cut starts in column 0.
	 */
ret:	if (vp->m_start.cno == 0)
		F_SET(vp, VM_LMODE);
	vp->m_final = ISMOTION(vp) && dir == FORWARD ? vp->m_start : vp->m_stop;
	return (0);
}
