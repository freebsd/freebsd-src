/*-
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)svi_util.c	8.33 (Berkeley) 3/10/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
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
#include "../vi/vcmd.h"
#include "excmd.h"
#include "svi_screen.h"

/*
 * vbell --
 *	Set up the visual bell information.  Broken out into a
 *	separate routine so don't allocate 4K every time we beep.
 */
static int
vbell(sp)
	SCR *sp;
{
	size_t len;
	char *s, *t, b1[2048], b2[2048];

	/* Get the termcap information. */
	s = O_STR(sp, O_TERM);
	if (tgetent(b1, s) != 1) {
		msgq(sp, M_ERR, "No termcap entry for %s", s);
		return (1);
	}

	/* Get the visual bell string. */
	t = b2;
	if (tgetstr("vb", &t) == NULL) {
		msgq(sp, M_VINFO,
		    "No visual bell for %s terminal type", s);
		return (1);
	}
	if ((len = t - b2) == 0)
		return (1);

	/* Free the old one, save the new one. */
	MALLOC_RET(sp, s, char *, len);
	memmove(s, b2, len);
	if (SVP(sp)->VB != NULL)
		free(SVP(sp)->VB);
	SVP(sp)->VB = s;
	return (0);
}

/*
 * svi_bell --
 *	Ring the bell.
 */
void
svi_bell(sp)
	SCR *sp;
{
	if (O_ISSET(sp, O_FLASH) && !F_ISSET(SVP(sp), SVI_NO_VBELL))
		if (SVP(sp)->VB != NULL) {
			(void)tputs(SVP(sp)->VB, 1, svi_putchar);
			(void)fflush(stdout);
		} else {
			if (vbell(sp))
				F_SET(SVP(sp), SVI_NO_VBELL);
			svi_bell(sp);
		}
	else
		(void)write(STDOUT_FILENO, "\007", 1);	/* '\a' */
	F_CLR(sp, S_BELLSCHED);
}

/*
 * svi_optchange --
 *	Screen specific "option changed" routine.
 */
int
svi_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_TERM:
		/* Toss any saved visual bell information. */
		if (SVP(sp)->VB != NULL) {
			FREE(SVP(sp)->VB, strlen(SVP(sp)->VB) + 1);
			SVP(sp)->VB = NULL;
		}
		F_CLR(SVP(sp), SVI_NO_VBELL);
		F_SET(sp, S_RESIZE);
		break;
	case O_WINDOW:
		if (svi_crel(sp, O_VAL(sp, O_WINDOW)))
			return (1);
		break;
	}

	(void)v_optchange(sp, opt);
	(void)ex_optchange(sp, opt);

	return (0);
}

/*
 * svi_busy --
 *	Put the cursor somewhere so the user will think we're busy.
 */
int
svi_busy(sp, msg)
	SCR *sp;
	char const *msg;
{
	/*
	 * search.c:f_search() is called from ex/ex_tag.c:ex_tagfirst(),
	 * which runs before the screen really exists.  Make sure we don't
	 * step on anything.
	 */
	if (F_ISSET(sp->gp, G_CURSES_INIT)) {
		MOVE(sp, INFOLINE(sp), 0);
		if (msg) {
			ADDSTR(msg);
			clrtoeol();
		}
		refresh();
		F_SET(SVP(sp), SVI_CUR_INVALID);
	}
	return (0);
}

/*
 * svi_keypad --
 *	Put the keypad/cursor arrows into or out of application mode.
 */
void
svi_keypad(sp, on)
	SCR *sp;
	int on;
{
	char *sbp, *t, kbuf[2048], sbuf[128];

	if (tgetent(kbuf, O_STR(sp, O_TERM)) != 1)
		return;
	sbp = sbuf;
	if ((t = tgetstr(on ? "ks" : "ke", &sbp)) == NULL)
		return;
	(void)tputs(t, 0, svi_putchar);
	(void)fflush(stdout);
}

/*
 * svi_clear --
 *	Clear from the row down to the end of the screen.
 */
int
svi_clear(sp)
	SCR *sp;
{
	size_t oldy, oldx, row;

	getyx(stdscr, oldy, oldx);
	for (row = SVP(sp)->srows - 1; row >= oldy; --row) {
		MOVEA(sp, row, 0);
		clrtoeol();
	}
	MOVEA(sp, oldy, oldx);
	refresh();
	return (0);
}

/*
 * svi_suspend --
 *	Suspend the svi screen; don't kill the process group, curses is
 *	expected to do that for us.
 */
int
svi_suspend(sp)
	SCR *sp;
{
	struct termios t;
	int rval;

	/* Restore the cursor keys to normal mode. */
	svi_keypad(sp, 0);

	/*
	 * XXX
	 * See comment in svi_curses_init().
	 */
	if (F_ISSET(sp->gp, G_CURSES_S5CB)) {
		(void)tcgetattr(STDIN_FILENO, &t);
		(void)tcsetattr(STDIN_FILENO,
		    TCSASOFT | TCSADRAIN, &sp->gp->s5_curses_botch);
	}

	F_SET(sp->gp, G_SLEEPING);
	if (rval = kill(getpid(), SIGTSTP))
		msgq(sp, M_SYSERR, "SIGTSTP");
	F_CLR(sp->gp, G_SLEEPING);

	if (F_ISSET(sp->gp, G_CURSES_S5CB))
		(void)tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t);

	/* Put the cursor keys into application mode. */
	svi_keypad(sp, 0);

	return (rval);
}

/*
 * svi_putchar --
 *	Functional version of putchar, for tputs.
 */
int
svi_putchar(ch)
	int ch;
{
	return putchar(ch);
}

/*
 * svi_gdbrefresh --
 *	Stub routine so can flush out screen changes using gdb.
 */
#ifdef DEBUG
int
svi_gdbrefresh()
{
	refresh();
	return (0);
}
#endif
