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
static char sccsid[] = "@(#)v_util.c	8.8 (Berkeley) 3/14/94";
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
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"

/*
 * v_eof --
 *	Vi end-of-file error.
 */
void
v_eof(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;
{
	u_long lno;

	if (mp == NULL)
		msgq(sp, M_BERR, "Already at end-of-file.");
	else {
		if (file_lline(sp, ep, &lno))
			return;
		if (mp->lno >= lno)
			msgq(sp, M_BERR, "Already at end-of-file.");
		else
			msgq(sp, M_BERR,
			    "Movement past the end-of-file.");
	}
}

/*
 * v_eol --
 *	Vi end-of-line error.
 */
void
v_eol(sp, ep, mp)
	SCR *sp;
	EXF *ep;
	MARK *mp;
{
	size_t len;

	if (mp == NULL)
		msgq(sp, M_BERR, "Already at end-of-line.");
	else {
		if (file_gline(sp, ep, mp->lno, &len) == NULL) {
			GETLINE_ERR(sp, mp->lno);
			return;
		}
		if (mp->cno == len - 1)
			msgq(sp, M_BERR, "Already at end-of-line.");
		else
			msgq(sp, M_BERR, "Movement past the end-of-line.");
	}
}

/*
 * v_nomove --
 *	Vi no cursor movement error.
 */
void
v_nomove(sp)
	SCR *sp;
{
	msgq(sp, M_BERR, "No cursor movement made.");
}

/*
 * v_sof --
 *	Vi start-of-file error.
 */
void
v_sof(sp, mp)
	SCR *sp;
	MARK *mp;
{
	if (mp == NULL || mp->lno == 1)
		msgq(sp, M_BERR, "Already at the beginning of the file.");
	else
		msgq(sp, M_BERR, "Movement past the beginning of the file.");
}

/*
 * v_sol --
 *	Vi start-of-line error.
 */
void
v_sol(sp)
	SCR *sp;
{
	msgq(sp, M_BERR, "Already in the first column.");
}

/*
 * v_isempty --
 *	Return if the line contains nothing but white-space characters.
 */
int
v_isempty(p, len)
	char *p;
	size_t len;
{
	for (; len--; ++p)
		if (!isblank(*p))
			return (0);
	return (1);
}
