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
static char sccsid[] = "@(#)sex_util.c	8.11 (Berkeley) 3/8/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
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
 * sex_suspend --
 *	Suspend an ex screen.
 */
int
sex_suspend(sp)
	SCR *sp;
{
	GS *gp;
	struct termios t;
	int rval;

	/* Save ex/vi terminal settings, and restore the original ones. */
	gp = sp->gp;
	if (F_ISSET(gp, G_STDIN_TTY)) {
		(void)tcgetattr(STDIN_FILENO, &t);
		if (F_ISSET(gp, G_TERMIOS_SET))
			(void)tcsetattr(STDIN_FILENO,
			    TCSADRAIN, &gp->original_termios);
	}

	/* Kill the process group. */
	F_SET(gp, G_SLEEPING);
	if (rval = kill(0, SIGTSTP))
		msgq(sp, M_SYSERR, "SIGTSTP");
	F_CLR(gp, G_SLEEPING);

	/* Restore ex/vi terminal settings. */
	if (F_ISSET(gp, G_STDIN_TTY))
		(void)tcsetattr(STDIN_FILENO, TCSADRAIN, &t);

	return (rval);
}
