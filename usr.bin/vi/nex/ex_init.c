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
static char sccsid[] = "@(#)ex_init.c	8.11 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"
#include "tag.h"

/*
 * ex_screen_copy --
 *	Copy ex screen.
 */
int
ex_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	EX_PRIVATE *oexp, *nexp;

	/* Create the private ex structure. */
	CALLOC_RET(orig, nexp, EX_PRIVATE *, 1, sizeof(EX_PRIVATE));
	sp->ex_private = nexp;

	/* Initialize queues. */
	TAILQ_INIT(&nexp->tagq);
	TAILQ_INIT(&nexp->tagfq);
	CIRCLEQ_INIT(&nexp->rangeq);

	if (orig == NULL) {
		nexp->at_lbuf_set = 0;
	} else {
		oexp = EXP(orig);

		nexp->at_lbuf = oexp->at_lbuf;
		nexp->at_lbuf_set = oexp->at_lbuf_set;

		if (oexp->lastbcomm != NULL &&
		    (nexp->lastbcomm = strdup(oexp->lastbcomm)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return(1);
		}

		if (ex_tagcopy(orig, sp))
			return (1);
	}

	nexp->lastcmd = &cmds[C_PRINT];
	return (0);
}

/*
 * ex_screen_end --
 *	End a vi screen.
 */
int
ex_screen_end(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	int rval;

	rval = 0;
	exp = EXP(sp);

	if (argv_free(sp))
		rval = 1;

	if (exp->ibp != NULL)
		FREE(exp->ibp, exp->ibp_len);

	if (exp->lastbcomm != NULL)
		FREE(exp->lastbcomm, strlen(exp->lastbcomm) + 1);

	/* Free private memory. */
	FREE(exp, sizeof(EX_PRIVATE));
	sp->ex_private = NULL;

	return (rval);
}

/*
 * ex_init --
 *	Initialize ex.
 */
int
ex_init(sp, ep)
	SCR *sp;
	EXF *ep;
{
	size_t len;

	/*
	 * The default address is the last line of the file.  If the address
	 * set bit is on for this file, load the address, ensuring that it
	 * exists.
	 */
	if (F_ISSET(sp->frp, FR_CURSORSET)) {
		sp->lno = sp->frp->lno;
		sp->cno = sp->frp->cno;

		if (file_gline(sp, ep, sp->lno, &len) == NULL) {
			if (file_lline(sp, ep, &sp->lno))
				return (1);
			if (sp->lno == 0)
				sp->lno = 1;
			sp->cno = 0;
		} else if (sp->cno >= len)
			sp->cno = 0;
	} else {
		if (file_lline(sp, ep, &sp->lno))
			return (1);
		if (sp->lno == 0)
			sp->lno = 1;
		sp->cno = 0;
	}

	/* Display the status line. */
	return (status(sp, ep, sp->lno, 0));
}

/*
 * ex_end --
 *	End ex session.
 */
int
ex_end(sp)
	SCR *sp;
{
	/* Save the cursor location. */
	sp->frp->lno = sp->lno;
	sp->frp->cno = sp->cno;
	F_SET(sp->frp, FR_CURSORSET);

	return (0);
}

/*
 * ex_optchange --
 *	Handle change of options for vi.
 */
int
ex_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_TAGS:
		return (ex_tagalloc(sp, O_STR(sp, O_TAGS)));
	}
	return (0);
}
