/*
 * Copyright (c) 1980 Regents of the University of California.
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
"@(#) Copyright (c) 1980 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)script.c	5.13 (Berkeley) 3/5/91";
#endif /* not lint */

/*
 * script
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/signal.h>
#include <stdio.h>
#include <paths.h>

char	*shell;
FILE	*fscript;
int	master;
int	slave;
int	child;
int	subchild;
char	*fname;

struct	termios tt;
struct	winsize win;
int	lb;
int	l;
char	line[] = "/dev/ptyXX";
int	aflg;

main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	extern int optind;
	int ch;
	void finish();
	char *getenv();

	while ((ch = getopt(argc, argv, "a")) != EOF)
		switch((char)ch) {
		case 'a':
			aflg++;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: script [-a] [file]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		fname = argv[0];
	else
		fname = "typescript";
	if ((fscript = fopen(fname, aflg ? "a" : "w")) == NULL) {
		perror(fname);
		fail();
	}

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	getmaster();
	printf("Script started, file is %s\n", fname);
	fixtty();

	(void) signal(SIGCHLD, finish);
	child = fork();
	if (child < 0) {
		perror("fork");
		fail();
	}
	if (child == 0) {
		subchild = child = fork();
		if (child < 0) {
			perror("fork");
			fail();
		}
		if (child)
			dooutput();
		else
			doshell();
	}
	doinput();
}

doinput()
{
	register int cc;
	char ibuf[BUFSIZ];

	(void) fclose(fscript);
	while ((cc = read(0, ibuf, BUFSIZ)) > 0)
		(void) write(master, ibuf, cc);
	done();
}

#include <sys/wait.h>

void
finish()
{
	union wait status;
	register int pid;
	register int die = 0;

	while ((pid = wait3((int *)&status, WNOHANG, 0)) > 0)
		if (pid == child)
			die = 1;

	if (die)
		done();
}

dooutput()
{
	register int cc;
	time_t tvec, time();
	char obuf[BUFSIZ], *ctime();

	(void) close(0);
	tvec = time((time_t *)NULL);
	fprintf(fscript, "Script started on %s", ctime(&tvec));
	for (;;) {
		cc = read(master, obuf, sizeof (obuf));
		if (cc <= 0)
			break;
		(void) write(1, obuf, cc);
		(void) fwrite(obuf, 1, cc, fscript);
	}
	done();
}

doshell()
{
	int t;

	/***
	t = open(_PATH_TTY, O_RDWR);
	if (t >= 0) {
		(void) ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	}
	***/
	getslave();
	(void) close(master);
	(void) fclose(fscript);
	(void) dup2(slave, 0);
	(void) dup2(slave, 1);
	(void) dup2(slave, 2);
	(void) close(slave);
	execl(shell, "sh", "-i", 0);
	perror(shell);
	fail();
}

fixtty()
{
	struct termios rtt;

	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	(void) tcsetattr(0, TCSAFLUSH, &rtt);
}

fail()
{

	(void) kill(0, SIGTERM);
	done();
}

done()
{
	time_t tvec, time();
	char *ctime();

	if (subchild) {
		tvec = time((time_t *)NULL);
		fprintf(fscript,"\nScript done on %s", ctime(&tvec));
		(void) fclose(fscript);
		(void) close(master);
	} else {
		(void) tcsetattr(0, TCSAFLUSH, &tt);
		printf("Script done, file is %s\n", fname);
	}
	exit(0);
}

getmaster()
{
	char *pty, *bank, *cp;
	struct stat stb;

	pty = &line[strlen("/dev/ptyp")];
	for (bank = "pqrs"; *bank; bank++) {
		line[strlen("/dev/pty")] = *bank;
		*pty = '0';
		if (stat(line, &stb) < 0)
			break;
		for (cp = "0123456789abcdef"; *cp; cp++) {
			*pty = *cp;
			master = open(line, O_RDWR);
			if (master >= 0) {
				char *tp = &line[strlen("/dev/")];
				int ok;

				/* verify slave side is usable */
				*tp = 't';
				ok = access(line, R_OK|W_OK) == 0;
				*tp = 'p';
				if (ok) {
					(void) tcgetattr(0, &tt);
				    	(void) ioctl(0, TIOCGWINSZ, 
						(char *)&win);
					return;
				}
				(void) close(master);
			}
		}
	}
	fprintf(stderr, "Out of pty's\n");
	fail();
}

getslave()
{

	line[strlen("/dev/")] = 't';
	slave = open(line, O_RDWR);
	if (slave < 0) {
		perror(line);
		fail();
	}
	(void) tcsetattr(slave, TCSAFLUSH, &tt);
	(void) ioctl(slave, TIOCSWINSZ, (char *)&win);
	(void) setsid();
	(void) ioctl(slave, TIOCSCTTY, 0);
}
