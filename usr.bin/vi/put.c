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
static char sccsid[] = "@(#)put.c	8.3 (Berkeley) 3/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"

/*
 * put --
 *	Put text buffer contents into the file.
 *
 * !!!
 * Historically, pasting into a file with no lines in vi would preserve
 * the single blank line.  This is almost certainly a result of the fact
 * that historic vi couldn't deal with a file that had no lines in it.
 * This implementation treats that as a bug, and does not retain the blank
 * line.
 */
int
put(sp, ep, cbp, namep, cp, rp, append)
	SCR *sp;
	EXF *ep;
	CB *cbp;
	CHAR_T *namep;
	MARK *cp, *rp;
	int append;
{
	CHAR_T name;
	TEXT *ltp, *tp;
	recno_t lno;
	size_t blen, clen, len;
	char *bp, *p, *t;

	if (cbp == NULL)
		if (namep == NULL) {
			cbp = sp->gp->dcbp;
			if (cbp == NULL) {
				msgq(sp, M_ERR, "The default buffer is empty.");
				return (1);
			}
		} else {
			name = *namep;
			CBNAME(sp, cbp, name);
			if (cbp == NULL) {
				msgq(sp, M_ERR,
				    "Buffer %s is empty.", charname(sp, name));
				return (1);
			}
		}
	tp = cbp->textq.cqh_first;

	/*
	 * It's possible to do a put into an empty file, meaning that the
	 * cut buffer simply becomes the file.  It's a special case so
	 * that we can ignore it in general.
	 *
	 * Historical practice is that the cursor ends up on the first
	 * non-blank character of the first line inserted.
	 */
	if (cp->lno == 1) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0) {
			for (; tp != (void *)&cbp->textq;
			     ++lno, tp = tp->q.cqe_next)
				if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
					return (1);
			rp->lno = 1;
			rp->cno = 0;
			(void)nonblank(sp, ep, rp->lno, &rp->cno);
			goto ret;
		}
	}

	/* If a line mode buffer, append each new line into the file. */
	if (F_ISSET(cbp, CB_LMODE)) {
		lno = append ? cp->lno : cp->lno - 1;
		rp->lno = lno + 1;
		for (; tp != (void *)&cbp->textq; ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				return (1);
		rp->cno = 0;
		(void)nonblank(sp, ep, rp->lno, &rp->cno);
		goto ret;
	}

	/*
	 * If buffer was cut in character mode, replace the current line with
	 * one built from the portion of the first line to the left of the
	 * split plus the first line in the CB.  Append each intermediate line
	 * in the CB.  Append a line built from the portion of the first line
	 * to the right of the split plus the last line in the CB.
	 *
	 * Get the first line.
	 */
	lno = cp->lno;
	if ((p = file_gline(sp, ep, lno, &len)) == NULL) {
		GETLINE_ERR(sp, lno);
		return (1);
	}

	GET_SPACE_RET(sp, bp, blen, tp->len + len + 1);
	t = bp;

	/* Original line, left of the split. */
	if (len > 0 && (clen = cp->cno + (append ? 1 : 0)) > 0) {
		memmove(bp, p, clen);
		p += clen;
		t += clen;
	}

	/* First line from the CB. */
	memmove(t, tp->lb, tp->len);
	t += tp->len;

	/* Calculate length left in original line. */
	clen = len ? len - cp->cno - (append ? 1 : 0) : 0;

	/*
	 * If no more lines in the CB, append the rest of the original
	 * line and quit.  Otherwise, build the last line before doing
	 * the intermediate lines, because the line changes will lose
	 * the cached line.
	 */
	if (tp->q.cqe_next == (void *)&cbp->textq) {
		/*
		 * Historical practice is that if a non-line mode put
		 * is inside a single line, the cursor ends up on the
		 * last character inserted.
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		if (clen > 0) {
			memmove(t, p, clen);
			t += clen;
		}
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;
	} else {
		/*
		 * Have to build both the first and last lines of the
		 * put before doing any sets or we'll lose the cached
		 * line.  Build both the first and last lines in the
		 * same buffer, so we don't have to have another buffer
		 * floating around.
		 *
		 * Last part of original line; check for space, reset
		 * the pointer into the buffer.
		 */
		ltp = cbp->textq.cqh_last;
		len = t - bp;
		ADD_SPACE_RET(sp, bp, blen, ltp->len + clen);
		t = bp + len;

		/* Add in last part of the CB. */
		memmove(t, ltp->lb, ltp->len);
		if (clen)
			memmove(t + ltp->len, p, clen);
		clen += ltp->len;

		/*
		 * Now: bp points to the first character of the first
		 * line, t points to the last character of the last
		 * line, t - bp is the length of the first line, and
		 * clen is the length of the last.  Just figured you'd
		 * want to know.
		 *
		 * Output the line replacing the original line.
		 */
		if (file_sline(sp, ep, lno, bp, t - bp))
			goto mem;

		/*
		 * Historical practice is that if a non-line mode put
		 * covers multiple lines, the cursor ends up on the
		 * first character inserted.  (Of course.)
		 */
		rp->lno = lno;
		rp->cno = (t - bp) - 1;

		/* Output any intermediate lines in the CB. */
		for (tp = tp->q.cqe_next;
		    tp->q.cqe_next != (void *)&cbp->textq;
		    ++lno, tp = tp->q.cqe_next)
			if (file_aline(sp, ep, 1, lno, tp->lb, tp->len))
				goto mem;

		if (file_aline(sp, ep, 1, lno, t, clen)) {
mem:			FREE_SPACE(sp, bp, blen);
			return (1);
		}
	}
	FREE_SPACE(sp, bp, blen);

	/* Reporting... */
ret:	sp->rptlines[L_PUT] += lno - cp->lno;

	return (0);
}
