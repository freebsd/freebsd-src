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
static char sccsid[] = "@(#)v_ulcase.c	8.6 (Berkeley) 3/18/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_ulcase -- [count]~
 *	Toggle upper & lower case letters.
 *
 * !!!
 * In historic vi, the count was ignored.  It would have been better
 * if there had been an associated motion, but it's too late to change
 * it now.
 */
int
v_ulcase(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	recno_t lno;
	size_t blen, lcnt, len;
	u_long cnt;
	int ch, change, rval;
	char *bp, *p;

	/* Get some memory. */
	GET_SPACE_RET(sp, bp, blen, 256);

	/*
	 * !!!
	 * Historic vi didn't permit ~ to cross newline boundaries.  I can
	 * think of no reason why it shouldn't, which at least lets the user
	 * auto-repeat through a paragraph.
	 */
	rval = 0;
	for (change = -1, cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt;) {
		/* Get the line; EOF is an infinite sink. */
		if ((p = file_gline(sp, ep, vp->m_stop.lno, &len)) == NULL) {
			if (file_lline(sp, ep, &lno)) {
				rval = 1;
				goto ret;
			}
			if (lno >= vp->m_stop.lno) {
				GETLINE_ERR(sp, vp->m_stop.lno);
				rval = 1;
				goto ret;
			}
			if (change == -1) {
				v_eof(sp, ep, NULL);
				rval = 1;
				goto ret;
			}
			break;
		}

		/* Set current line number. */
		lno = vp->m_stop.lno;

		/* Empty lines just decrement the count. */
		if (len == 0) {
			--cnt;
			++vp->m_stop.lno;
			vp->m_stop.cno = 0;
			change = 0;
			continue;
		}

		/* Get a copy of the line. */
		ADD_SPACE_RET(sp, bp, blen, len);
		memmove(bp, p, len);

		/* Set starting pointer. */
		if (change == -1)
			p = bp + vp->m_stop.cno;
		else
			p = bp;

		/*
		 * Figure out how many characters get changed in this
		 * line.  Set the final cursor column.
		 */
		if (vp->m_stop.cno + cnt >= len) {
			lcnt = len - vp->m_stop.cno;
			++vp->m_stop.lno;
			vp->m_stop.cno = 0;
		} else
			vp->m_stop.cno += lcnt = cnt;
		cnt -= lcnt;

		/* Change the line. */
		for (change = 0; lcnt--; ++p) {
			ch = *(u_char *)p;
			if (islower(ch)) {
				*p = toupper(ch);
				change = 1;
			} else if (isupper(ch)) {
				*p = tolower(ch);
				change = 1;
			}
		}

		/* Update the line if necessary. */
		if (change && file_sline(sp, ep, lno, bp, len)) {
			rval = 1;
			break;
		}
	}

	/* If changed lines, could be on an illegal line. */
	if (vp->m_stop.lno != lno &&
	    file_gline(sp, ep, vp->m_stop.lno, &len) == NULL) {
		--vp->m_stop.lno;
		vp->m_stop.cno = len ? len - 1 : 0;
	}
	vp->m_final = vp->m_stop;

ret:	FREE_SPACE(sp, bp, blen);
	return (rval);
}
