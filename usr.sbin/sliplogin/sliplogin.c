/*-
 * Copyright (c) 1990, 1993
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
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)sliplogin.c	8.2 (Berkeley) 2/1/94";
#endif /* not lint */

/*
 * sliplogin.c
 * [MUST BE RUN SUID, SLOPEN DOES A SUSER()!]
 *
 * This program initializes its own tty port to be an async TCP/IP interface.
 * It sets the line discipline to slip, invokes a shell script to initialize
 * the network interface, then pauses forever waiting for hangup.
 *
 * It is a remote descendant of several similar programs with incestuous ties:
 * - Kirk Smith's slipconf, modified by Richard Johnsson @ DEC WRL.
 * - slattach, probably by Rick Adams but touched by countless hordes.
 * - the original sliplogin for 4.2bsd, Doug Kingston the mover behind it.
 *
 * There are two forms of usage:
 *
 * "sliplogin"
 * Invoked simply as "sliplogin", the program looks up the username
 * in the file /etc/slip.hosts.
 * If an entry is found, the line on fd0 is configured for SLIP operation
 * as specified in the file.
 *
 * "sliplogin IPhostlogin </dev/ttyb"
 * Invoked by root with a username, the name is looked up in the
 * /etc/slip.hosts file and if found fd0 is configured as in case 1.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/file.h>
#include <sys/syslog.h>
#include <netdb.h>

#if BSD >= 199006
#define POSIX
#endif
#ifdef POSIX
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <ttyent.h>
#else
#include <sgtty.h>
#endif
#include <net/slip.h>

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include "pathnames.h"

int	unit;
int	speed;
int	uid;
char	loginargs[BUFSIZ];
char	loginfile[MAXPATHLEN];
char	loginname[BUFSIZ];

void
findid(name)
	char *name;
{
	FILE *fp;
	static char slopt[5][16];
	static char laddr[16];
	static char raddr[16];
	static char mask[16];
	char user[16];
	int i, j, n;

	(void)strcpy(loginname, name);
	if ((fp = fopen(_PATH_ACCESS, "r")) == NULL) {
		(void)fprintf(stderr, "sliplogin: %s: %s\n",
		    _PATH_ACCESS, strerror(errno));
		syslog(LOG_ERR, "%s: %m\n", _PATH_ACCESS);
		exit(1);
	}
	while (fgets(loginargs, sizeof(loginargs) - 1, fp)) {
		if (ferror(fp))
			break;
		n = sscanf(loginargs, "%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s\n",
                        user, laddr, raddr, mask, slopt[0], slopt[1], 
			slopt[2], slopt[3], slopt[4]);
		if (user[0] == '#' || isspace(user[0]))
			continue;
		if (strcmp(user, name) != 0)
			continue;

		/*
		 * we've found the guy we're looking for -- see if
		 * there's a login file we can use.  First check for
		 * one specific to this host.  If none found, try for
		 * a generic one.
		 */
		(void)sprintf(loginfile, "%s.%s", _PATH_LOGIN, name);
		if (access(loginfile, R_OK|X_OK) != 0) {
			(void)strcpy(loginfile, _PATH_LOGIN);
			if (access(loginfile, R_OK|X_OK)) {
				fputs("access denied - no login file\n",
				      stderr);
				syslog(LOG_ERR,
				       "access denied for %s - no %s\n",
				       name, _PATH_LOGIN);
				exit(5);
			}
		}

		(void) fclose(fp);
		return;
	}
	(void)fprintf(stderr, "SLIP access denied for %s\n", name);
	syslog(LOG_ERR, "SLIP access denied for %s\n", name);
	exit(4);
	/* NOTREACHED */
}

char *
sigstr(s)
	int s;
{
	static char buf[32];

	switch (s) {
	case SIGHUP:	return("HUP");
	case SIGINT:	return("INT");
	case SIGQUIT:	return("QUIT");
	case SIGILL:	return("ILL");
	case SIGTRAP:	return("TRAP");
	case SIGIOT:	return("IOT");
	case SIGEMT:	return("EMT");
	case SIGFPE:	return("FPE");
	case SIGKILL:	return("KILL");
	case SIGBUS:	return("BUS");
	case SIGSEGV:	return("SEGV");
	case SIGSYS:	return("SYS");
	case SIGPIPE:	return("PIPE");
	case SIGALRM:	return("ALRM");
	case SIGTERM:	return("TERM");
	case SIGURG:	return("URG");
	case SIGSTOP:	return("STOP");
	case SIGTSTP:	return("TSTP");
	case SIGCONT:	return("CONT");
	case SIGCHLD:	return("CHLD");
	case SIGTTIN:	return("TTIN");
	case SIGTTOU:	return("TTOU");
	case SIGIO:	return("IO");
	case SIGXCPU:	return("XCPU");
	case SIGXFSZ:	return("XFSZ");
	case SIGVTALRM:	return("VTALRM");
	case SIGPROF:	return("PROF");
	case SIGWINCH:	return("WINCH");
#ifdef SIGLOST
	case SIGLOST:	return("LOST");
#endif
	case SIGUSR1:	return("USR1");
	case SIGUSR2:	return("USR2");
	}
	(void)sprintf(buf, "sig %d", s);
	return(buf);
}

void
hup_handler(s)
	int s;
{
	char logoutfile[MAXPATHLEN];

	(void)sprintf(logoutfile, "%s.%s", _PATH_LOGOUT, loginname);
	if (access(logoutfile, R_OK|X_OK) != 0)
		(void)strcpy(logoutfile, _PATH_LOGOUT);
	if (access(logoutfile, R_OK|X_OK) == 0) {
		char logincmd[2*MAXPATHLEN+32];

		(void) sprintf(logincmd, "%s %d %d %s", logoutfile, unit, speed,
			      loginargs);
		(void) system(logincmd);
	}
	(void) close(0);
	syslog(LOG_INFO, "closed %s slip unit %d (%s)\n", loginname, unit,
	       sigstr(s));
	exit(1);
	/* NOTREACHED */
}

main(argc, argv)
	int argc;
	char *argv[];
{
	int fd, s, ldisc, odisc;
	char *name;
#ifdef POSIX
	struct termios tios, otios;
#else
	struct sgttyb tty, otty;
#endif
	char logincmd[2*BUFSIZ+32];
	extern uid_t getuid();

	if ((name = strrchr(argv[0], '/')) == NULL)
		name = argv[0];
	s = getdtablesize();
	for (fd = 3 ; fd < s ; fd++)
		(void) close(fd);
	openlog(name, LOG_PID, LOG_DAEMON);
	uid = getuid();
	if (argc > 1) {
		findid(argv[1]);

		/*
		 * Disassociate from current controlling terminal, if any,
		 * and ensure that the slip line is our controlling terminal.
		 */
#ifdef POSIX
		if (fork() > 0)
			exit(0);
		if (setsid() != 0)
			perror("setsid");
#else
		if ((fd = open("/dev/tty", O_RDONLY, 0)) >= 0) {
			extern char *ttyname();

			(void) ioctl(fd, TIOCNOTTY, (caddr_t)0);
			(void) close(fd);
			/* open slip tty again to acquire as controlling tty? */
			fd = open(ttyname(0), O_RDWR, 0);
			if (fd >= 0)
				(void) close(fd);
		}
		(void) setpgrp(0, getpid());
#endif
		if (argc > 2) {
			if ((fd = open(argv[2], O_RDWR)) == -1) {
				perror(argv[2]);
				exit(2);
			}
			(void) dup2(fd, 0);
			if (fd > 2)
				close(fd);
		}
#ifdef TIOCSCTTY
		if (ioctl(0, TIOCSCTTY, (caddr_t)0) != 0)
			perror("ioctl (TIOCSCTTY)");
#endif
	} else {
		extern char *getlogin();

		if ((name = getlogin()) == NULL) {
			(void) fprintf(stderr, "access denied - no username\n");
			syslog(LOG_ERR, "access denied - getlogin returned 0\n");
			exit(1);
		}
		findid(name);
	}
	(void) fchmod(0, 0600);
	(void) fprintf(stderr, "starting slip login for %s\n", loginname);
#ifdef POSIX
	/* set up the line parameters */
	if (tcgetattr(0, &tios) < 0) {
		syslog(LOG_ERR, "tcgetattr: %m");
		exit(1);
	}
	otios = tios;
	cfmakeraw(&tios);
	tios.c_iflag &= ~IMAXBEL;
	if (tcsetattr(0, TCSAFLUSH, &tios) < 0) {
		syslog(LOG_ERR, "tcsetattr: %m");
		exit(1);
	}
	speed = cfgetispeed(&tios);
#else
	/* set up the line parameters */
	if (ioctl(0, TIOCGETP, (caddr_t)&tty) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCGETP): %m");
		exit(1);
	}
	otty = tty;
	speed = tty.sg_ispeed;
	tty.sg_flags = RAW | ANYP;
	if (ioctl(0, TIOCSETP, (caddr_t)&tty) < 0) {
		syslog(LOG_ERR, "ioctl (TIOCSETP): %m");
		exit(1);
	}
#endif
	/* find out what ldisc we started with */
	if (ioctl(0, TIOCGETD, (caddr_t)&odisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCGETD) (1): %m");
		exit(1);
	}
	ldisc = SLIPDISC;
	if (ioctl(0, TIOCSETD, (caddr_t)&ldisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit(1);
	}
	/* find out what unit number we were assigned */
	if (ioctl(0, SLIOCGUNIT, (caddr_t)&unit) < 0) {
		syslog(LOG_ERR, "ioctl (SLIOCGUNIT): %m");
		exit(1);
	}
	(void) signal(SIGHUP, hup_handler);
	(void) signal(SIGTERM, hup_handler);

	syslog(LOG_INFO, "attaching slip unit %d for %s\n", unit, loginname);
	(void)sprintf(logincmd, "%s %d %d %s", loginfile, unit, speed,
		      loginargs);
	/*
	 * aim stdout and errout at /dev/null so logincmd output won't
	 * babble into the slip tty line.
	 */
	(void) close(1);
	if ((fd = open(_PATH_DEVNULL, O_WRONLY)) != 1) {
		if (fd < 0) {
			syslog(LOG_ERR, "open /dev/null: %m");
			exit(1);
		}
		(void) dup2(fd, 1);
		(void) close(fd);
	}
	(void) dup2(1, 2);

	/*
	 * Run login and logout scripts as root (real and effective);
	 * current route(8) is setuid root, and checks the real uid
	 * to see whether changes are allowed (or just "route get").
	 */
	(void) setuid(0);
	if (s = system(logincmd)) {
		syslog(LOG_ERR, "%s login failed: exit status %d from %s",
		       loginname, s, loginfile);
		(void) ioctl(0, TIOCSETD, (caddr_t)&odisc);
		exit(6);
	}

	/* twiddle thumbs until we get a signal */
	while (1)
		sigpause(0);

	/* NOTREACHED */
}
