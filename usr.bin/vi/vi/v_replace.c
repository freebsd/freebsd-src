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
static char sccsid[] = "@(#)v_replace.c	8.15 (Berkeley) 3/14/94";
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
 * v_replace -- [count]rc
 *
 * !!!
 * The r command in historic vi was almost beautiful in its badness.  For
 * example, "r<erase>" and "r<word erase>" beeped the terminal and deleted
 * a single character.  "Nr<carriage return>", where N was greater than 1,
 * inserted a single carriage return.  This may not be right, but at least
 * it's not insane.
 */
int
v_replace(sp, ep, vp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
{
	CH ikey;
	TEXT *tp;
	recno_t lno;
	size_t blen, len;
	u_long cnt;
	int rval;
	char *bp, *p;

	/*
	 * If the line doesn't exist, or it's empty, replacement isn't
	 * allowed.  It's not hard to implement, but:
	 *
	 *	1: It's historic practice.
	 *	2: For consistency, this change would require that the more
	 *	   general case, "Nr", when the user is < N characters from
	 *	   the end of the line, also work.
	 *	3: Replacing a newline has somewhat odd semantics.
	 */
	if ((p = file_gline(sp, ep, vp->m_start.lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno != 0) {
			GETLINE_ERR(sp, vp->m_start.lno);
			return (1);
		}
		goto nochar;
	}
	if (len == 0) {
nochar:		msgq(sp, M_BERR, "No characters to replace.");
		return (1);
	}

	/*
	 * Figure out how many characters to be replace.  For no particular
	 * reason (other than that the semantics of replacing the newline
	 * are confusing) only permit the replacement of the characters in
	 * the current line.  I suppose we could append replacement characters
	 * to the line, but I see no compelling reason to do so.
	 */
	cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1;
	vp->m_stop.lno = vp->m_start.lno;
	vp->m_stop.cno = vp->m_start.cno + cnt - 1;
	if (vp->m_stop.cno > len - 1) {
		v_eol(sp, ep, &vp->m_start);
		return (1);
	}

	/*
	 * Get the character.  Literal escapes escape any character,
	 * single escapes return.
	 */
	if (F_ISSET(vp, VC_ISDOT)) {
		ikey.ch = VIP(sp)->rlast;
		ikey.value = term_key_val(sp, ikey.ch);
	} else {
		if (term_key(sp, &ikey, 0) != INP_OK)
			return (1);
		switch (ikey.value) {
		case K_ESCAPE:
			return (0);
		case K_VLNEXT:
			if (term_key(sp, &ikey, 0) != INP_OK)
				return (1);
			break;
		}
		VIP(sp)->rlast = ikey.ch;
	}

	/* Copy the line. */
	GET_SPACE_RET(sp, bp, blen, len);
	memmove(bp, p, len);
	p = bp;

	if (ikey.value == K_CR || ikey.value == K_NL) {
		/* Set return line. */
		vp->m_stop.lno = vp->m_start.lno + cnt;
		vp->m_stop.cno = 0;

		/* The first part of the current line. */
		if (file_sline(sp, ep, vp->m_start.lno, p, vp->m_start.cno))
			goto err_ret;

		/*
		 * The rest of the current line.  And, of course, now it gets
		 * tricky.  Any white space after the replaced character is
		 * stripped, and autoindent is applied.  Put the cursor on the
		 * last indent character as did historic vi.
		 */
		for (p += vp->m_start.cno + cnt, len -= vp->m_start.cno + cnt;
		    len && isblank(*p); --len, ++p);

		if ((tp = text_init(sp, p, len, len)) == NULL)
			goto err_ret;
		if (txt_auto(sp, ep, vp->m_start.lno, NULL, 0, tp))
			goto err_ret;
		vp->m_stop.cno = tp->ai ? tp->ai - 1 : 0;
		if (file_aline(sp, ep, 1, vp->m_start.lno, tp->lb, tp->len))
			goto err_ret;
		text_free(tp);

		rval = 0;

		/* All of the middle lines. */
		while (--cnt)
			if (file_aline(sp, ep, 1, vp->m_start.lno, "", 0)) {
err_ret:			rval = 1;
				break;
			}
	} else {
		memset(bp + vp->m_start.cno, ikey.ch, cnt);
		rval = file_sline(sp, ep, vp->m_start.lno, bp, len);
	}
	FREE_SPACE(sp, bp, blen);

	vp->m_final = vp->m_stop;
	return (rval);
}
