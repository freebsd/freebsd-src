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
static char sccsid[] = "@(#)ex_display.c	8.15 (Berkeley) 3/8/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "tag.h"
#include "excmd.h"

static int	bdisplay __P((SCR *, EXF *));
static void	db __P((SCR *, CB *, char *));

/*
 * ex_display -- :display b[uffers] | s[creens] | t[ags]
 *
 *	Display buffers, tags or screens.
 */
int
ex_display(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	switch (cmdp->argv[0]->bp[0]) {
	case 'b':
		return (bdisplay(sp, ep));
	case 's':
		return (ex_sdisplay(sp, ep));
	case 't':
		return (ex_tagdisplay(sp, ep));
	}
	msgq(sp, M_ERR,
	    "Unknown display argument %s, use b[uffers], s[creens], or t[ags].",
	    cmdp->argv[0]);
	return (1);
}

/*
 * bdisplay --
 *
 *	Display buffers.
 */
static int
bdisplay(sp, ep)
	SCR *sp;
	EXF *ep;
{
	CB *cbp;

	if (sp->gp->cutq.lh_first == NULL && sp->gp->dcbp == NULL) {
		(void)ex_printf(EXCOOKIE, "No cut buffers to display.");
		return (0);
	}

	/* Buffers can be infinitely long, make it interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	/* Display regular cut buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp, NULL);
		if (F_ISSET(sp, S_INTERRUPTED))
			return (0);
	}
	/* Display numbered buffers. */
	for (cbp = sp->gp->cutq.lh_first; cbp != NULL; cbp = cbp->q.le_next) {
		if (!isdigit(cbp->name))
			continue;
		if (cbp->textq.cqh_first != (void *)&cbp->textq)
			db(sp, cbp, NULL);
		if (F_ISSET(sp, S_INTERRUPTED))
			return (0);
	}
	/* Display default buffer. */
	if ((cbp = sp->gp->dcbp) != NULL)
		db(sp, cbp, "default buffer");
	return (0);
}

/*
 * db --
 *	Display a buffer.
 */
static void
db(sp, cbp, name)
	SCR *sp;
	CB *cbp;
	char *name;
{
	TEXT *tp;
	size_t len;
	char *p;

	(void)ex_printf(EXCOOKIE, "********** %s%s\n",
	    name == NULL ? charname(sp, cbp->name) : name,
	    F_ISSET(cbp, CB_LMODE) ? " (line mode)" : "");
	for (tp = cbp->textq.cqh_first;
	    tp != (void *)&cbp->textq; tp = tp->q.cqe_next) {
		for (len = tp->len, p = tp->lb; len--;) {
			(void)ex_printf(EXCOOKIE, "%s", charname(sp, *p++));
			if (F_ISSET(sp, S_INTERRUPTED))
				return;
		}
		(void)ex_printf(EXCOOKIE, "\n");
	}
}
