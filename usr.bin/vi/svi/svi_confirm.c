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
static char sccsid[] = "@(#)svi_confirm.c	8.8 (Berkeley) 4/13/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "svi_screen.h"

enum confirm
svi_confirm(sp, ep, fp, tp)
	SCR *sp;
	EXF *ep;
	MARK *fp, *tp;
{
	CH ikey;
	size_t oldy, oldx;

	/*
	 * Refresh the cursor first -- this means that we won't have to
	 * set S_UPDATE_MODE to keep refresh from erasing the mode line
	 * or SVI_CUR_INVALID because we sneaked the cursor off somewhere
	 * else.
	 */
	sp->lno = fp->lno;
	sp->cno = fp->cno;
	if (svi_paint(sp, ep))
		return (CONF_QUIT);

	getyx(stdscr, oldy, oldx);
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	ADDNSTR(STR_CONFIRM, sizeof(STR_CONFIRM) - 1);
	MOVEA(sp, oldy, oldx);
	refresh();

	if (term_key(sp, &ikey, 0) != INP_OK)
		return (CONF_QUIT);
	switch (ikey.ch) {
	case CH_YES:
		return (CONF_YES);
	case CH_QUIT:
		return (CONF_QUIT);
	default:
	case CH_NO:
		return (CONF_NO);
	}
	/* NOTREACHED */
}
