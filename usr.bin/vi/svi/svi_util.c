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
static const char sccsid[] = "@(#)svi_util.c	8.54 (Berkeley) 8/17/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "../vi/vcmd.h"
#include "excmd.h"
#include "svi_screen.h"
#include "../sex/sex_screen.h"

/*
 * svi_bell --
 *	Ring the bell.
 */
void
svi_bell(sp)
	SCR *sp;
{
#ifdef SYSV_CURSES
	if (O_ISSET(sp, O_FLASH))
		flash();
	else
		beep();
#else
	if (O_ISSET(sp, O_FLASH) && SVP(sp)->VB != NULL) {
		(void)tputs(SVP(sp)->VB, 1, vi_putchar);
		(void)fflush(stdout);
	} else
		(void)write(STDOUT_FILENO, "\007", 1);	/* '\a' */
#endif
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

		/* Reset the screen size. */
		if (sp->s_window(sp, 0))
			return (1);
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
	 *
	 * If the terminal isn't initialized, there's nothing to do.
	 */
	if (!F_ISSET(SVP(sp), SVI_CURSES_INIT))
		return (0);

	MOVE(sp, INFOLINE(sp), 0);
	if (msg) {
		ADDSTR(msg);
		clrtoeol();
	}
	refresh();
	F_SET(SVP(sp), SVI_CUR_INVALID);
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
#ifdef SYSV_CURSES
	keypad(stdscr, on ? TRUE : FALSE);
#else
	char *sbp, *t, sbuf[128];

	sbp = sbuf;
	if ((t = tgetstr(on ? "ks" : "ke", &sbp)) == NULL)
		return;
	(void)tputs(t, 0, vi_putchar);
	(void)fflush(stdout);
#endif
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
 *	Suspend an svi screen.
 *
 * See signal.c for a long discussion of what's going on here.  Let
 * me put it this way, it's NOT my fault.
 */
int
svi_suspend(sp)
	SCR *sp;
{
	struct termios sv_term;
	sigset_t set;
	int oldx, oldy, rval;
	char *sbp, *t, sbuf[128];

	rval = 0;

	/*
	 * Block SIGALRM, because vi uses timers to decide when to paint
	 * busy messages on the screen.
	 */
	(void)sigemptyset(&set);
	(void)sigaddset(&set, SIGALRM);
	if (sigprocmask(SIG_BLOCK, &set, NULL)) {
		msgq(sp, M_SYSERR, "suspend: sigblock");
		return (1);
	}

	/* Save the current cursor position. */
	getyx(stdscr, oldy, oldx);

	/*
	 * Move the cursor to the bottom of the screen.
	 *
	 * XXX
	 * Some curses implementations don't turn off inverse video when
	 * standend() is called, waiting to see what the next character is
	 * going to be, instead.  Write a character to force inverse video
	 * off, and then clear the line.
	 */
	MOVE(sp, INFOLINE(sp), 0);
	ADDCH('.');
	refresh();
	MOVE(sp, INFOLINE(sp), 0);
	clrtoeol();
	refresh();

	/* Restore the cursor keys to normal mode. */
	svi_keypad(sp, 0);

	/* Send VE/TE. */
#ifdef SYSV_CURSES
	if ((t = tigetstr("cnorm")) != NULL && t != (char *)-1)
		(void)tputs(t, 0, vi_putchar);
	if ((t = tigetstr("rmcup")) != NULL && t != (char *)-1)
		(void)tputs(t, 0, vi_putchar);
#else
	sbp = sbuf;
	if ((t = tgetstr("ve", &sbp)) != NULL)
		(void)tputs(t, 0, vi_putchar);
	sbp = sbuf;
	if ((t = tgetstr("te", &sbp)) != NULL)
		(void)tputs(t, 0, vi_putchar);
#endif
	(void)fflush(stdout);

	/* Save current terminal settings, and restore the original ones. */
	if (tcgetattr(STDIN_FILENO, &sv_term)) {
		msgq(sp, M_SYSERR, "suspend: tcgetattr");
		return (1);
	}
	if (tcsetattr(STDIN_FILENO,
	    TCSASOFT | TCSADRAIN, &sp->gp->original_termios)) {
		msgq(sp, M_SYSERR, "suspend: tcsetattr original");
		return (1);
	}

	/* Push out any waiting messages. */
	(void)write(STDOUT_FILENO, "\n", 1);
	(void)sex_refresh(sp, sp->ep);

	/* Stop the process group. */
	if (kill(0, SIGTSTP)) {
		msgq(sp, M_SYSERR, "suspend: kill");
		rval = 1;
	}

	/* Time passes ... */

	/* Restore current terminal settings. */
	if (tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &sv_term)) {
		msgq(sp, M_SYSERR, "suspend: tcsetattr current");
		rval = 1;
	}

	/* Send TI/VS. */
#ifdef SYSV_CURSES
	if ((t = tigetstr("smcup")) != NULL && t != (char *)-1)
		(void)tputs(t, 0, vi_putchar);
	if ((t = tigetstr("cvvis")) != NULL && t != (char *)-1)
		(void)tputs(t, 0, vi_putchar);
#else
	sbp = sbuf;
	if ((t = tgetstr("ti", &sbp)) != NULL)
		(void)tputs(t, 0, vi_putchar);
	sbp = sbuf;
	if ((t = tgetstr("vs", &sbp)) != NULL)
		(void)tputs(t, 0, vi_putchar);
#endif
	(void)fflush(stdout);

	/* Put the cursor keys into application mode. */
	svi_keypad(sp, 1);

	/*
	 * If the screen changed size, do a full refresh.  Otherwise,
	 * System V has curses repaint it.  4BSD curses will repaint
	 * it in the wrefresh() call below.
	 */
	if (!sp->s_window(sp, 1))
		(void)sp->s_refresh(sp, sp->ep);
#ifdef SYSV_CURSES
	else
		redrawwin(stdscr);
#endif

	/*
	 * Restore the cursor.
	 *
	 * !!!
	 * Don't use MOVE/MOVEA, we don't want to return without resetting
	 * the signals, regardless.
	 */
	(void)move(oldy, oldx);
	(void)wrefresh(curscr);

	/* Reset the signals. */
	if (sigprocmask(SIG_UNBLOCK, &set, NULL)) {
		msgq(sp, M_SYSERR, "suspend: sigblock");
		rval = 1;
	}
	return (rval);
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
