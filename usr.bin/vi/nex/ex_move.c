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
static char sccsid[] = "@(#)ex_move.c	8.6 (Berkeley) 1/9/94";
#endif /* not lint */

#include <sys/types.h>

#include <string.h>

#include "vi.h"
#include "excmd.h"

enum which {COPY, MOVE};
static int cm __P((SCR *, EXF *, EXCMDARG *, enum which));

/*
 * ex_copy -- :[line [,line]] co[py] line [flags]
 *	Copy selected lines.
 */
int
ex_copy(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (cm(sp, ep, cmdp, COPY));
}

/*
 * ex_move -- :[line [,line]] co[py] line
 *	Move selected lines.
 */
int
ex_move(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	return (cm(sp, ep, cmdp, MOVE));
}

static int
cm(sp, ep, cmdp, cmd)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
	enum which cmd;
{
	CB cb;
	MARK fm1, fm2, m, tm;
	recno_t diff;
	int rval;

	fm1 = cmdp->addr1;
	fm2 = cmdp->addr2;
	tm.lno = cmdp->lineno;
	tm.cno = 0;

	/* Make sure the destination is valid. */
	if (cmd == MOVE && tm.lno >= fm1.lno && tm.lno < fm2.lno) {
		msgq(sp, M_ERR, "Destination line is inside move range.");
		return (1);
	}

	/* Save the text to a cut buffer. */
	memset(&cb, 0, sizeof(cb));
	CIRCLEQ_INIT(&cb.textq);
	if (cut(sp, ep, &cb, NULL, &fm1, &fm2, CUT_LINEMODE))
		return (1);

	/* If we're not copying, delete the old text and adjust tm. */
	if (cmd == MOVE) {
		if (delete(sp, ep, &fm1, &fm2, 1)) {
			rval = 1;
			goto err;
		}
		if (tm.lno >= fm1.lno)
			tm.lno -= (fm2.lno - fm1.lno) + 1;
	}

	/* Add the new text. */
	if (put(sp, ep, &cb, NULL, &tm, &m, 1)) {
		rval = 1;
		goto err;
	}

	/*
	 * Move and copy put the cursor on the last line moved or copied.
	 * The returned cursor from the put routine is the first line put,
	 * not the last, because that's the semantics of vi.
	 */
	diff = (fm2.lno - fm1.lno) + 1;
	sp->lno = m.lno + (diff - 1);
	sp->cno = 0;

	sp->rptlines[cmd == COPY ? L_COPIED : L_MOVED] += diff;
	rval = 0;

err:	(void)text_lfree(&cb.textq);
	return (rval);
}
