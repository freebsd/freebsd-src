/*
 * Copyright (c) 1983 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)dn11.c	5.4 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * Routines for dialing up on DN-11
 */
#include "tip.h"

int dn_abort();
void alarmtr();
static jmp_buf jmpbuf;
static int child = -1, dn;

dn_dialer(num, acu)
	char *num, *acu;
{
	extern errno;
	char *p, *q, phone[40];
	int lt, nw, connected = 1;
	register int timelim;

	if (boolean(value(VERBOSE)))
		printf("\nstarting call...");
	if ((dn = open(acu, 1)) < 0) {
		if (errno == EBUSY)
			printf("line busy...");
		else
			printf("acu open error...");
		return (0);
	}
	if (setjmp(jmpbuf)) {
		kill(child, SIGKILL);
		close(dn);
		return (0);
	}
	signal(SIGALRM, alarmtr);
	timelim = 5 * strlen(num);
	alarm(timelim < 30 ? 30 : timelim);
	if ((child = fork()) == 0) {
		/*
		 * ignore this stuff for aborts
		 */
		signal(SIGALRM, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		sleep(2);
		nw = write(dn, num, lt = strlen(num));
		exit(nw != lt);
	}
	/*
	 * open line - will return on carrier
	 */
	if ((FD = open(DV, 2)) < 0) {
		if (errno == EIO)
			printf("lost carrier...");
		else
			printf("dialup line open failed...");
		alarm(0);
		kill(child, SIGKILL);
		close(dn);
		return (0);
	}
	alarm(0);
	ioctl(dn, TIOCHPCL, 0);
	signal(SIGALRM, SIG_DFL);
	while ((nw = wait(&lt)) != child && nw != -1)
		;
	fflush(stdout);
	close(dn);
	if (lt != 0) {
		close(FD);
		return (0);
	}
	return (1);
}

void
alarmtr()
{
	alarm(0);
	longjmp(jmpbuf, 1);
}

/*
 * Insurance, for some reason we don't seem to be
 *  hanging up...
 */
dn_disconnect()
{

	sleep(2);
	if (FD > 0)
		ioctl(FD, TIOCCDTR, 0);
	close(FD);
}

dn_abort()
{

	sleep(2);
	if (child > 0)
		kill(child, SIGKILL);
	if (dn > 0)
		close(dn);
	if (FD > 0)
		ioctl(FD, TIOCCDTR, 0);
	close(FD);
}
