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
static char sccsid[] = "@(#)sex_util.c	8.15 (Berkeley) 7/16/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "sex_screen.h"

/*
 * sex_bell --
 *	Ring the bell.
 */
void
sex_bell(sp)
	SCR *sp;
{
	(void)write(STDOUT_FILENO, "\07", 1);		/* \a */
}

void
sex_busy(sp, msg)
	SCR *sp;
	char const *msg;
{
	(void)fprintf(stdout, "%s\n", msg);
	(void)fflush(stdout);
}

/*
 * sex_optchange --
 *	Screen specific "option changed" routine.
 */
int
sex_optchange(sp, opt)
	SCR *sp;
	int opt;
{
	switch (opt) {
	case O_TERM:
		/* Reset the screen size. */
		if (sp->s_window(sp, 0))
			return (1);
		F_SET(sp, S_RESIZE);
		break;
	}

	(void)ex_optchange(sp, opt);

	return (0);
}

/*
 * sex_suspend --
 *	Suspend an ex screen.
 */
int
sex_suspend(sp)
	SCR *sp;
{
	struct termios t;
	GS *gp;
	int rval;

	rval = 0;

	/* Save current terminal settings, and restore the original ones. */
	gp = sp->gp;
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &t)) {
			msgq(sp, M_SYSERR, "suspend: tcgetattr");
			return (1);
		}
		if (F_ISSET(gp, G_TERMIOS_SET) && tcsetattr(STDIN_FILENO,
		    TCSASOFT | TCSADRAIN, &gp->original_termios)) {
			msgq(sp, M_SYSERR, "suspend: tcsetattr original");
			return (1);
		}
	}

	/* Push out any waiting messages. */
	(void)sex_refresh(sp, sp->ep);

	/* Stop the process group. */
	if (kill(0, SIGTSTP)) {
		msgq(sp, M_SYSERR, "suspend: kill");
		rval = 1;
	}

	/* Time passes ... */

	/* Restore current terminal settings. */
	if (F_ISSET(gp, G_STDIN_TTY) &&
	    tcsetattr(STDIN_FILENO, TCSASOFT | TCSADRAIN, &t)) {
		msgq(sp, M_SYSERR, "suspend: tcsetattr current");
		rval = 1;
	}
	return (rval);
}
