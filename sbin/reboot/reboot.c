/*
 * Copyright (c) 1980, 1986 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1986 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)reboot.c	5.11 (Berkeley) 2/27/91";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/syscall.h>
#include <sys/reboot.h>
#include <sys/signal.h>
#include <pwd.h>
#include <stdio.h>
#include <errno.h>

main(argc, argv)
	int argc;
	char **argv;
{
	int howto;
	register char *argp;
	register i;
	register ok = 0;
	register qflag = 0;
	int needlog = 1;
	char *user, *getlogin();
	struct passwd *pw;

	openlog("reboot", 0, LOG_AUTH);
	argc--, argv++;
	howto = 0;
	while (argc > 0) {
		if (!strcmp(*argv, "-q"))
			qflag++;
		else if (!strcmp(*argv, "-n"))
			howto |= RB_NOSYNC;
		else if (!strcmp(*argv, "-l"))
			needlog = 0;
		else {
			fprintf(stderr,
			    "usage: reboot [ -n ][ -q ]\n");
			exit(1);
		}
		argc--, argv++;
	}

	if (needlog) {
		user = getlogin();
		if (user == (char *)0 && (pw = getpwuid(getuid())))
			user = pw->pw_name;
		if (user == (char *)0)
			user = "root";
		syslog(LOG_CRIT, "rebooted by %s", user);
	}

	signal(SIGHUP, SIG_IGN);	/* for remote connections */
	if (kill(1, SIGTSTP) == -1) {
		fprintf(stderr, "reboot: can't idle init\n");
		exit(1);
	}
	sleep(1);
	(void) kill(-1, SIGTERM);	/* one chance to catch it */
	sleep(5);

	if (!qflag) for (i = 1; ; i++) {
		if (kill(-1, SIGKILL) == -1) {
			extern int errno;

			if (errno == ESRCH)
				break;

			perror("reboot: kill");
			kill(1, SIGHUP);
			exit(1);
		}
		if (i > 5) {
			fprintf(stderr,
			    "CAUTION: some process(es) wouldn't die\n");
			break;
		}
		setalarm(2 * i);
		pause();
	}

	if (!qflag && (howto & RB_NOSYNC) == 0) {
		logwtmp("~", "shutdown", "");
		sync();
		setalarm(5);
		pause();
	}
	syscall(SYS_reboot, howto);
	perror("reboot");
	kill(1, SIGHUP);
	exit(1);
}

void
dingdong()
{
	/* RRRIIINNNGGG RRRIIINNNGGG */
}

setalarm(n)
	int n;
{
	signal(SIGALRM, dingdong);
	alarm(n);
}
