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
static char sccsid[] = "@(#)svi_get.c	8.22 (Berkeley) 3/24/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "vcmd.h"
#include "svi_screen.h"

/*
 * svi_get --
 *	Fill a buffer from the terminal for vi.
 */
enum input
svi_get(sp, ep, tiqh, prompt, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	int prompt;
	u_int flags;
{
	MARK save;
	SMAP *esmp;
	recno_t bot_lno;
	size_t bot_off, cnt;
	int eval;

	/*
	 * The approach used is to fake like the user is doing input on
	 * the last line of the screen.  This makes all of the scrolling
	 * work correctly, and allows us the use of the vi text editing
	 * routines, not to mention practically infinite length ex commands.
	 *
	 * Save the current location.
	 */
	bot_lno = TMAP->lno;
	bot_off = TMAP->off;
	save.lno = sp->lno;
	save.cno = sp->cno;

	/*
	 * If it's a small screen, TMAP may be small for the screen.
	 * Fix it, filling in fake lines as we go.
	 */
	if (ISSMALLSCREEN(sp))
		for (esmp = HMAP + (sp->t_maxrows - 1); TMAP < esmp; ++TMAP) {
			TMAP[1].lno = TMAP[0].lno + 1;
			TMAP[1].off = 1;
		}

	/* Build the fake entry. */
	TMAP[1].lno = TMAP[0].lno + 1;
	TMAP[1].off = 1;
	SMAP_FLUSH(&TMAP[1]);
	++TMAP;

	/* Move to it. */
	sp->lno = TMAP[0].lno;
	sp->cno = 0;

	if (O_ISSET(sp, O_ALTWERASE))
		LF_SET(TXT_ALTWERASE);
	if (O_ISSET(sp, O_TTYWERASE))
		LF_SET(TXT_TTYWERASE);
	LF_SET(TXT_APPENDEOL |
	    TXT_CR | TXT_ESCAPE | TXT_INFOLINE | TXT_MAPINPUT);

	/* Don't update the modeline for now. */
	F_SET(SVP(sp), SVI_INFOLINE);

	eval = v_ntext(sp, ep, tiqh, NULL, NULL, 0, NULL, prompt, 0, flags);

	F_CLR(SVP(sp), SVI_INFOLINE);

	/* Put it all back. */
	--TMAP;
	sp->lno = save.lno;
	sp->cno = save.cno;

	/*
	 * If it's a small screen, TMAP may be wrong.  Clear any
	 * lines that might have been overwritten.
	 */
	if (ISSMALLSCREEN(sp)) {
		for (cnt = sp->t_rows; cnt <= sp->t_maxrows; ++cnt) {
			MOVE(sp, cnt, 0);
			clrtoeol();
		}
		TMAP = HMAP + (sp->t_rows - 1);
	}

	/*
	 * The map may be wrong if the user entered more than one
	 * (logical) line.  Fix it.  If the user entered a whole
	 * screen, this will be slow, but it's not worth caring.
	 */
	while (bot_lno != TMAP->lno || bot_off != TMAP->off)
		if (svi_sm_1down(sp, ep))
			return (INP_ERR);

	/*
	 * Invalidate the cursor and the line size cache, the line never
	 * really existed.  This fixes bugs where the user searches for
	 * the last line on the screen + 1 and the refresh routine thinks
	 * that's where we just were.
	 */
	F_SET(SVP(sp), SVI_CUR_INVALID);
	SVI_SCR_CFLUSH(SVP(sp));

	return (eval ? INP_ERR : INP_OK);
}
