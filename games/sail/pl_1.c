/*
 * Copyright (c) 1983, 1993
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
#if 0
static char sccsid[] = "@(#)pl_1.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/sail/pl_1.c,v 1.2 1999/11/30 03:49:36 billf Exp $";
#endif /* not lint */

#include "player.h"
#include <sys/types.h>
#include <sys/wait.h>

/*
 * If we get here before a ship is chosen, then ms == 0 and
 * we don't want to update the score file, or do any Write's either.
 * We can assume the sync file is already created and may need
 * to be removed.
 * Of course, we don't do any more Sync()'s if we got here
 * because of a Sync() failure.
 */
leave(conditions)
int conditions;
{
	(void) signal(SIGHUP, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGALRM, SIG_IGN);
	(void) signal(SIGCHLD, SIG_IGN);

	if (done_curses) {
		Signal("It looks like you've had it!",
			(struct ship *)0);
		switch (conditions) {
		case LEAVE_QUIT:
			break;
		case LEAVE_CAPTURED:
			Signal("Your ship was captured.",
				(struct ship *)0);
			break;
		case LEAVE_HURRICAN:
			Signal("Hurricane!  All ships destroyed.",
				(struct ship *)0);
			break;
		case LEAVE_DRIVER:
			Signal("The driver died.", (struct ship *)0);
			break;
		case LEAVE_SYNC:
			Signal("Synchronization error.", (struct ship *)0);
			break;
		default:
			Signal("A funny thing happened (%d).",
				(struct ship *)0, conditions);
		}
	} else {
		switch (conditions) {
		case LEAVE_QUIT:
			break;
		case LEAVE_DRIVER:
			printf("The driver died.\n");
			break;
		case LEAVE_FORK:
			perror("fork");
			break;
		case LEAVE_SYNC:
			printf("Synchronization error\n.");
			break;
		default:
			printf("A funny thing happened (%d).\n",
				conditions);
		}
	}

	if (ms != 0) {
		log(ms);
		if (conditions != LEAVE_SYNC) {
			makesignal(ms, "Captain %s relinquishing.",
				(struct ship *)0, mf->captain);
			Write(W_END, ms, 0, 0, 0, 0, 0);
			(void) Sync();
		}
	}
	sync_close(!hasdriver);
	cleanupscreen();
	exit(0);
}

void
choke()
{
	leave(LEAVE_QUIT);
}

void
child()
{
	union wait status;
	int pid;

	(void) signal(SIGCHLD, SIG_IGN);
	do {
		pid = wait3((int *)&status, WNOHANG, (struct rusage *)0);
		if (pid < 0 || pid > 0 && !WIFSTOPPED(status))
			hasdriver = 0;
	} while (pid > 0);
	(void) signal(SIGCHLD, child);
}
