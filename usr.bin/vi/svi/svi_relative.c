/*-
 * Copyright (c) 1993
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
static char sccsid[] = "@(#)svi_relative.c	8.7 (Berkeley) 12/29/93";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "svi_screen.h"

/*
 * svi_column --
 *	Return the logical column of the cursor.
 */
int
svi_column(sp, ep, cp)
	SCR *sp;
	EXF *ep;
	size_t *cp;
{
	size_t col;

	col = SVP(sp)->sc_col;
	if (O_ISSET(sp, O_NUMBER))
		col -= O_NUMBER_LENGTH;
	*cp = col;
	return (0);
}

/*
 * svi_relative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  If it's not easy, uses the underlying routine that
 *	really figures it out.  It's broken into two parts because the
 *	svi_lrelative routine handles "logical" offsets, which nobody
 *	but the screen routines understand.
 */
size_t
svi_relative(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	size_t cno;

	/* First non-blank character. */
	if (sp->rcmflags == RCM_FNB) {
		cno = 0;
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && sp->rcm == 0)
		return (0);

	return (svi_lrelative(sp, ep, lno, 1));
}

/*
 * svi_lrelative --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position.  The offset is for the commands that move logical
 *	distances, i.e. if it's a logical scroll the closest physical
 *	distance is based on the logical line, not the physical line.
 */
size_t
svi_lrelative(sp, ep, lno, off)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off;
{
	CHNAME const *cname;
	size_t len, llen, scno;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	if ((lp = file_gline(sp, ep, lno, &len)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (len == 0)
		return (0);

	/* Last character is easy, and common. */
	if (sp->rcmflags == RCM_LAST)
		return (len - 1);

	/* Discard logical lines. */
	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);
	for (scno = 0, p = lp, llen = len; --off;) {
		for (; len && scno < sp->cols; --len)
			SCNO_INCREMENT;
		if (len == 0)
			return (llen - 1);
		scno -= sp->cols;
	}

	/* Step through the line until reach the right character. */
	while (len--) {
		SCNO_INCREMENT;
		if (scno >= sp->rcm) {
			/* Get the offset of this character. */
			len = p - lp;

			/*
			 * May be the next character, not this one,
			 * so check to see if we've gone too far.
			 */
			if (scno == sp->rcm)
				return (len < llen - 1 ? len : llen - 1);
			/* It's this character. */
			return (len - 1);
		}
	}
	/* No such character; return start of last character. */
	return (llen - 1);
}

/*
 * svi_chposition --
 *	Return the physical column from the line that will display a
 *	character closest to the specified column.
 */
size_t
svi_chposition(sp, ep, lno, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t cno;
{
	CHNAME const *cname;
	size_t len, llen, scno;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	if ((lp = file_gline(sp, ep, lno, &llen)) == NULL)
		return (0);

	/* Empty lines are easy. */
	if (llen == 0)
		return (0);

	/* Step through the line until reach the right character. */
	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);
	for (scno = 0, len = llen, p = lp; len--;) {
		SCNO_INCREMENT;
		if (scno >= cno) {
			/* Get the offset of this character. */
			len = p - lp;

			/*
			 * May be the next character, not this one,
			 * so check to see if we've gone too far.
			 */
			if (scno == cno)
				return (len < llen - 1 ? len : llen - 1);
			/* It's this character. */
			return (len - 1);
		}
	}
	/* No such character; return start of last character. */
	return (llen - 1);
}
