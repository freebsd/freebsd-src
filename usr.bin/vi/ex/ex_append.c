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
static char sccsid[] = "@(#)ex_append.c	8.24 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../sex/sex_screen.h"

enum which {APPEND, CHANGE, INSERT};

static int aci __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_append -- :[line] a[ppend][!]
 *	Append one or more lines of new text after the specified line,
 *	or the current line if no address is specified.
 */
int
ex_append(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, APPEND));
}

/*
 * ex_change -- :[line[,line]] c[hange][!] [count]
 *	Change one or more lines to the input text.
 */
int
ex_change(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, CHANGE));
}

/*
 * ex_insert -- :[line] i[nsert][!]
 *	Insert one or more lines of new text before the specified line,
 *	or the current line if no address is specified.
 */
int
ex_insert(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (aci(sp, ep, cmdp, INSERT));
}

static int
aci(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	MARK m;
	TEXTH *sv_tiqp, tiq;
	TEXT *tp;
	struct termios t;
	u_int flags;
	int rval;

	rval = 0;

	/*
	 * Set input flags; the ! flag turns off autoindent for append,
	 * change and insert.
	 */
	LF_INIT(TXT_DOTTERM | TXT_NLECHO);
	if (!F_ISSET(cmdp, E_FORCE) && O_ISSET(sp, O_AUTOINDENT))
		LF_SET(TXT_AUTOINDENT);
	if (O_ISSET(sp, O_BEAUTIFY))
		LF_SET(TXT_BEAUTIFY);

	/* Input is interruptible. */
	F_SET(sp, S_INTERRUPTIBLE);

	/*
	 * If this code is called by vi, the screen TEXTH structure (sp->tiqp)
	 * may already be in use, e.g. ":append|s/abc/ABC/" would fail as we're
	 * only halfway through the line when the append code fires.  Use the
	 * local structure instead.
	 *
	 * If this code is called by vi, we want to reset the terminal and use
	 * ex's s_get() routine.  It actually works fine if we use vi's s_get()
	 * routine, but it doesn't look as nice.  Maybe if we had a separate
	 * window or something, but getting a line at a time looks awkward.
	 */
	if (IN_VI_MODE(sp)) {
		memset(&tiq, 0, sizeof(TEXTH));
		CIRCLEQ_INIT(&tiq);
		sv_tiqp = sp->tiqp;
		sp->tiqp = &tiq;

		if (F_ISSET(sp->gp, G_STDIN_TTY))
			SEX_RAW(t);
		(void)write(STDOUT_FILENO, "\n", 1);
		LF_SET(TXT_NLECHO);

	}

	/* Set the line number, so that autoindent works correctly. */
	sp->lno = cmdp->addr1.lno;

	if (sex_get(sp, ep, sp->tiqp, 0, flags) != INP_OK)
		goto err;

	/*
	 * If doing a change, replace lines for as long as possible.  Then,
	 * append more lines or delete remaining lines.  Changes to an empty
	 * file are just appends, and inserts are the same as appends to the
	 * previous line.
	 *
	 * !!!
	 * Adjust the current line number for the commands to match historic
	 * practice if the user doesn't enter anything, and set the address
	 * to which we'll append.  This is safe because an address of 0 is
	 * illegal for change and insert.
	 */
	m = cmdp->addr1;
	switch (cmd) {
	case INSERT:
		--m.lno;
		/* FALLTHROUGH */
	case APPEND:
		if (sp->lno == 0)
			sp->lno = 1;
		break;
	case CHANGE:
		--m.lno;
		if (sp->lno != 1)
			--sp->lno;
		break;
	}

	/*
	 * !!!
	 * Cut into the unnamed buffer.
	 */
	if (cmd == CHANGE &&
	    (cut(sp, ep, NULL, &cmdp->addr1, &cmdp->addr2, CUT_LINEMODE) ||
	    delete(sp, ep, &cmdp->addr1, &cmdp->addr2, 1)))
		goto err;

	for (tp = sp->tiqp->cqh_first;
	    tp != (TEXT *)sp->tiqp; tp = tp->q.cqe_next) {
		if (file_aline(sp, ep, 1, m.lno, tp->lb, tp->len)) {
err:			rval = 1;
			break;
		}
		sp->lno = ++m.lno;
	}

	if (IN_VI_MODE(sp)) {
		sp->tiqp = sv_tiqp;
		text_lfree(&tiq);

		/* Reset the terminal state. */
		if (F_ISSET(sp->gp, G_STDIN_TTY)) {
			if (SEX_NORAW(t))
				rval = 1;
			F_SET(sp, S_REFRESH);
		}
	}
	return (rval);
}
