/*-
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)svi_relative.c	8.12 (Berkeley) 3/15/94";
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
 * svi_opt_screens --
 *	Return the screen columns necessary to display the line, or
 *	if specified, the physical character column within the line,
 *	including space required for the O_NUMBER and O_LIST options.
 */
size_t
svi_opt_screens(sp, ep, lno, cnop)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t *cnop;
{
	size_t cols, screens;

	/*
	 * Check for a cached value.  We maintain a cache because, if the
	 * line is large, this routine gets called repeatedly.  One other
	 * hack, lots of time the cursor is on column one, which is an easy
	 * one.
	 */
	if (cnop == NULL) {
		if (SVP(sp)->ss_lno == lno)
			return (SVP(sp)->ss_screens);
	} else if (*cnop == 0)
		return (1);

	/* Figure out how many columns the line/column needs. */
	cols = svi_screens(sp, ep, NULL, 0, lno, cnop);

	/* Leading number if O_NUMBER option set. */
	if (O_ISSET(sp, O_NUMBER))
		cols += O_NUMBER_LENGTH;

	/* Trailing '$' if O_LIST option set. */
	if (O_ISSET(sp, O_LIST) && cnop == NULL)
		cols += sp->gp->cname['$'].len;

	screens = (cols / sp->cols + (cols % sp->cols ? 1 : 0));
	if (screens == 0)
		screens = 1;

	/* Cache the value. */
	if (cnop == NULL) {
		SVP(sp)->ss_lno = lno;
		SVP(sp)->ss_screens = screens;
	}
	return (screens);
}

/*
 * svi_screens --
 *	Return the screen columns necessary to display the line, or,
 *	if specified, the physical character column within the line.
 */
size_t
svi_screens(sp, ep, lp, llen, lno, cnop)
	SCR *sp;
	EXF *ep;
	char *lp;
	size_t llen;
	recno_t lno;
	size_t *cnop;
{
	CHNAME const *cname;
	size_t chlen, cno, len, scno, tab_off;
	int ch, listset;
	char *p;

	/* Need the line to go any further. */
	if (lp == NULL)
		lp = file_gline(sp, ep, lno, &llen);

	/* Missing or empty lines are easy. */
	if (lp == NULL || llen == 0)
		return (0);

	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);

#define	SET_CHLEN {							\
	chlen = (ch = *(u_char *)p++) == '\t' &&			\
	    !listset ? TAB_OFF(sp, tab_off) : cname[ch].len;		\
}
#define	TAB_RESET {							\
	/*								\
	 * If past the end of the screen, and the character was a tab,	\
	 * reset the screen column to 0.  Otherwise, display the rest	\
	 * of the character on the next line.				\
	 */								\
	if ((tab_off += chlen) >= sp->cols)				\
		if (ch == '\t') {					\
			tab_off = 0;					\
			scno -= scno % sp->cols;			\
		} else							\
			tab_off -= sp->cols;				\
}
	p = lp;
	len = llen;
	scno = tab_off = 0;
	if (cnop == NULL)
		while (len--) {
			SET_CHLEN;
			scno += chlen;
			TAB_RESET;
		}
	else
		for (cno = *cnop; len--; --cno) {
			SET_CHLEN;
			scno += chlen;
			TAB_RESET;
			if (cno == 0)
				break;
		}
	return (scno);
}

/*
 * svi_rcm --
 *	Return the physical column from the line that will display a
 *	character closest to the currently most attractive character
 *	position (which is stored as a screen column).
 */
size_t
svi_rcm(sp, ep, lno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
{
	size_t cno, len;

	/* First non-blank character. */
	if (sp->rcmflags == RCM_FNB) {
		cno = 0;
		(void)nonblank(sp, ep, lno, &cno);
		return (cno);
	}

	/* First character is easy, and common. */
	if (sp->rcmflags != RCM_LAST && HMAP->off == 1 && sp->rcm == 0)
		return (0);

	/* Last character is easy, and common. */
	if (sp->rcmflags == RCM_LAST)
		return (file_gline(sp,
		    ep, lno, &len) == NULL || len == 0 ? 0 : len - 1);

	/*
	 * Get svi_cm_private() to do the hard work.  If doing leftright
	 * scrolling, we use the current screen offset, otherwise, use
	 * the first screen, i.e. an offset of 1.
	 *
	 * XXX
	 * I'm not sure that an offset of 1 is right.  What happens is that
	 * the vi main loop calls us for the VM_RCM case.  By using an offset
	 * of 1, we're assuming that every VM_RCM command changes lines, and
	 * that we want to position on the first screen for that line.  This
	 * is currently the way it works, but it's not clean. I'd prefer it if
	 * we could find the SMAP entry the cursor references, and use that
	 * screen offset.  Unfortunately, that's not going to be easy, as we
	 * don't keep that information around and it may be expensive to get.
	 */
	return (svi_cm_private(sp, ep, lno,
	    O_ISSET(sp, O_LEFTRIGHT) ? HMAP->off : 1, sp->rcm));
}

/*
 * svi_cm_public --
 *	Return the physical column from the line that will display a
 *	character closest to the specified screen column.
 *
 *	The extra interface is because it's called by vi, which doesn't
 *	have a handle on the SMAP structure.
 */
size_t
svi_cm_public(sp, ep, lno, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t cno;
{
	return (svi_cm_private(sp, ep, lno, HMAP->off, cno));
}

/*
 * svi_cm_private --
 *	Return the physical column from the line that will display a
 *	character closest to the specified screen column, taking into
 *	account the screen offset.
 *
 *	The offset is for the commands that move logical distances, i.e.
 *	if it's a logical scroll the closest physical distance is based
 *	on the logical line, not the physical line.
 */
size_t
svi_cm_private(sp, ep, lno, off, cno)
	SCR *sp;
	EXF *ep;
	recno_t lno;
	size_t off, cno;
{
	CHNAME const *cname;
	size_t chlen, len, llen, scno, tab_off;
	int ch, listset;
	char *lp, *p;

	/* Need the line to go any further. */
	lp = file_gline(sp, ep, lno, &llen);

	/* Missing or empty lines are easy. */
	if (lp == NULL || llen == 0)
		return (0);

	cname = sp->gp->cname;
	listset = O_ISSET(sp, O_LIST);

	/* Discard screen (logical) lines. */
	for (scno = 0, p = lp, len = llen; --off;) {
		while (len-- && scno < sp->cols)
			scno += (ch = *(u_char *)p++) == '\t' &&
			    !listset ? TAB_OFF(sp, scno) : cname[ch].len;

		/*
		 * If reached the end of the physical line, return
		 * the last physical character in the line.
		 */
		if (len == 0)
			return (llen - 1);

		/*
		 * If the character was a tab, reset the screen column to 0.
		 * Otherwise, the rest of the character is displayed on the
		 * next line.
		 */
		if (ch == '\t')
			scno = 0;
		else
			scno -= sp->cols;
	}

	/* Step through the line until reach the right character or EOL. */
	for (tab_off = scno; len--;) {
		SET_CHLEN;

		/*
		 * If we've reached the specific character, there are three
		 * cases.
		 *
		 * 1: scno == cno, i.e. the current character ends at the
		 *    screen character we care about.
		 *	a: off < llen - 1, i.e. not the last character in
		 *	   the line, return the offset of the next character.
		 *	b: else return the offset of the last character.
		 * 2: scno != cno, i.e. this character overruns the character
		 *    we care about, return the offset of this character.
		 */
		if ((scno += chlen) >= cno) {
			off = p - lp;
			return (scno == cno ?
			    (off < llen - 1 ? off : llen - 1) : off - 1);
		}

		TAB_RESET;
	}

	/* No such character; return the start of the last character. */
	return (llen - 1);
}
