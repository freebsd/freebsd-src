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
static char sccsid[] = "@(#)ex_visual.c	8.7 (Berkeley) 11/29/93";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_visual -- :[line] vi[sual] [^-.+] [window_size] [flags]
 *
 *	Switch to visual mode.
 */
int
ex_visual(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	size_t len;
	int pos;
	char buf[256];

	/* If open option off, disallow visual command. */
	if (!O_ISSET(sp, O_OPEN)) {
		msgq(sp, M_ERR,
		    "The visual command requires that the open option be set.");
		return (1);
	}

	/* If a line specified, move to that line. */
	if (cmdp->addrcnt)
		sp->lno = cmdp->addr1.lno;

	/*
	 * Push a command based on the line position flags.  If no
	 * flag specified, the line goes at the top of the screen.
	 */
	switch (F_ISSET(cmdp, E_F_CARAT | E_F_DASH | E_F_DOT | E_F_PLUS)) {
	case E_F_CARAT:
		pos = '^';
		break;
	case E_F_DASH:
		pos = '-';
		break;
	case E_F_DOT:
		pos = '.';
		break;
	case E_F_PLUS:
	default:
		pos = '+';
		break;
	}

	if (F_ISSET(cmdp, E_COUNT))
		len = snprintf(buf, sizeof(buf),
		     "%luz%c%lu", sp->lno, pos, cmdp->count);
	else
		len = snprintf(buf, sizeof(buf), "%luz%c", sp->lno, pos);
	(void)term_push(sp, buf, len, 0, CH_NOMAP | CH_QUOTED);

	/*
	 * !!!
	 * Historically, if no line address was specified, the [p#l] flags
	 * caused the cursor to be moved to the last line of the file, which
	 * was then positioned as described above.  This seems useless, so
	 * I haven't implemented it.
	 */
	switch (F_ISSET(cmdp, E_F_HASH | E_F_LIST | E_F_PRINT)) {
	case E_F_HASH:
		O_SET(sp, O_NUMBER);
		break;
	case E_F_LIST:
		O_SET(sp, O_LIST);
		break;
	case E_F_PRINT:
		break;
	}

	/* Switch modes. */
	F_CLR(sp, S_SCREENS);
	F_SET(sp, sp->saved_vi_mode);

	return (0);
}
