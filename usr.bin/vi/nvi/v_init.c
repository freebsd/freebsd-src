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
static char sccsid[] = "@(#)v_init.c	8.18 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "vcmd.h"
#include "excmd.h"

static int v_comment __P((SCR *, EXF *));

/*
 * v_screen_copy --
 *	Copy vi screen.
 */
int
v_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	VI_PRIVATE *ovip, *nvip;

	/* Create the private vi structure. */
	CALLOC_RET(orig, nvip, VI_PRIVATE *, 1, sizeof(VI_PRIVATE));
	sp->vi_private = nvip;

	if (orig == NULL) {
		nvip->inc_lastch = '+';
		nvip->inc_lastval = 1;
	} else {
		ovip = VIP(orig);

		/* User can replay the last input, but nothing else. */
		if (ovip->rep_len != 0) {
			MALLOC(orig, nvip->rep, char *, ovip->rep_len);
			if (nvip->rep != NULL) {
				memmove(nvip->rep, ovip->rep, ovip->rep_len);
				nvip->rep_len = ovip->rep_len;
			}
		}

		nvip->inc_lastch = ovip->inc_lastch;
		nvip->inc_lastval = ovip->inc_lastval;

		if (ovip->paragraph != NULL &&
		    (nvip->paragraph = strdup(ovip->paragraph)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
	}
	return (0);
}

/*
 * v_screen_end --
 *	End a vi screen.
 */
int
v_screen_end(sp)
	SCR *sp;
{
	VI_PRIVATE *vip;

	vip = VIP(sp);

	if (vip->rep != NULL)
		FREE(vip->rep, vip->rep_len);

	if (vip->paragraph != NULL)
		FREE(vip->paragraph, vip->paragraph_len);

	/* Free private memory. */
	FREE(vip, sizeof(VI_PRIVATE));
	sp->vi_private = NULL;

	return (0);
}

/*
 * v_init --
 *	Initialize vi.
 */
int
v_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t len;

	/*
	 * The default address is line 1, column 0.  If the address set
	 * bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (sp->lno != 1 || sp->cno != 0) {
				if (file_lline(sp, ep, &sp->lno))
					return (1);
				if (sp->lno == 0)
					sp->lno = 1;
				sp->cno = 0;
			}
		} else if (sp->cno >= len)
			sp->cno = 0;

	} else {
		sp->lno = 1;
		sp->cno = 0;

		if (O_ISSET(sp, O_COMMENT) && v_comment(sp, ep))
			return (1);
	}

	/* Reset strange attraction. */
	sp->rcm = 0;
	sp->rcmflags = 0;

	/* Make ex display to a special function. */
	if ((sp->stdfp = fwopen(sp, sp->s_ex_write)) == NULL) {
		msgq(sp, M_SYSERR, "ex output");
		return (1);
	}
#ifdef MAKE_EX_OUTPUT_LINE_BUFFERED
	(void)setvbuf(sp->stdfp, NULL, _IOLBF, 0);
#endif

	/* Display the status line. */
	return (status(sp, ep, sp->lno, 0));
}

/*
 * v_end --
 *	End vi session.
 */
int
v_end(sp)
	SCR *sp;
{
	/* Close down ex output file descriptor. */
	(void)fclose(sp->stdfp);

	return (0);
}

/*
 * v_optchange --
 *	Handle change of options for vi.
 */
int
v_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_PARAGRAPHS:
	case O_SECTIONS:
		return (v_buildparagraph(sp));
	}
	return (0);
}

/*
 * v_comment --
 *	Skip the first comment.
 */
static int
v_comment(sp, ep)
	SCR *sp;
	EXF *ep;
{
	recno_t lno;
	size_t len;
	char *p;

	for (lno = 1;
	    (p = file_gline(sp, ep, lno, &len)) != NULL && len == 0; ++lno);
	if (p == NULL || len <= 1 || memcmp(p, "/*", 2))
		return (0);
	do {
		for (; len; --len, ++p)
			if (p[0] == '*' && len > 1 && p[1] == '/') {
				sp->lno = lno;
				return (0);
			}
	} while ((p = file_gline(sp, ep, ++lno, &len)) != NULL);
	return (0);
}
