/*-
 * Copyright (c) 1983, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#if 0
#ifndef lint
static const char sccsid[] = "From: @(#)rsh.c	8.3 (Berkeley) 4/6/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * rsh - remote shell
 */
int	rfd2;

int family = PF_UNSPEC;
char rlogin[] = "rlogin";

void	connect_timeout(int);
char   *copyargs(char * const *);
void	sendsig(int);
void	talk(int, long, pid_t, int, int);
void	usage(void);

int
main(int argc, char *argv[])
{
	struct passwd const *pw;
	struct servent const *sp;
	long omask;
	int argoff, asrsh, ch, dflag, nflag, one, rem;
	pid_t pid = 0;
	uid_t uid;
	char *args, *host, *p, *user;
	int timeout = 0;

	argoff = asrsh = dflag = nflag = 0;
	one = 1;
	host = user = NULL;

	/* if called as something other than "rsh", use it as the host name */
	if ((p = strrchr(argv[0], '/')))
		++p;
	else
		p = argv[0];
	if (strcmp(p, "rsh"))
		host = p;
	else
		asrsh = 1;

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#define	OPTIONS	"468Lde:l:nt:w"
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != -1)
		switch(ch) {
		case '4':
			family = PF_INET;
			break;

		case '6':
			family = PF_INET6;
			break;

		case 'L':	/* -8Lew are ignored to allow rlogin aliases */
		case 'e':
		case 'w':
		case '8':
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			user = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 't':
			timeout = atoi(optarg);
			break;
		case '?':
		default:
			usage();
		}
	optind += argoff;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	/* if no further arguments, must have been called as rlogin. */
	if (!argv[optind]) {
		if (asrsh)
			*argv = rlogin;
		execv(_PATH_RLOGIN, argv);
		err(1, "can't exec %s", _PATH_RLOGIN);
	}

	argc -= optind;
	argv += optind;

	if (!(pw = getpwuid(uid = getuid())))
		errx(1, "unknown user id");
	if (!user)
		user = pw->pw_name;

	args = copyargs(argv);

	sp = NULL;
	if (sp == NULL)
		sp = getservbyname("shell", "tcp");
	if (sp == NULL)
		errx(1, "shell/tcp: unknown service");

	if (timeout) {
		signal(SIGALRM, connect_timeout);
		alarm(timeout);
	}
	rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args, &rfd2,
		      family);
	if (timeout) {
		signal(SIGALRM, SIG_DFL);
		alarm(0);
	}

	if (rem < 0)
		exit(1);

	if (rfd2 < 0)
		errx(1, "can't establish stderr");
	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
	}

	(void)setuid(uid);
	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGTERM));
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)signal(SIGTERM, sendsig);

	if (!nflag) {
		pid = fork();
		if (pid < 0)
			err(1, "fork");
	}
	else
		(void)shutdown(rem, 1);

	(void)ioctl(rfd2, FIONBIO, &one);
	(void)ioctl(rem, FIONBIO, &one);

	talk(nflag, omask, pid, rem, timeout);

	if (!nflag)
		(void)kill(pid, SIGKILL);
	exit(0);
}

void
talk(int nflag, long omask, pid_t pid, int rem, int timeout)
{
	int cc, wc;
	fd_set readfrom, ready, rembits;
	char buf[BUFSIZ];
	const char *bp;
	struct timeval tvtimeout;
	int nfds, srval;

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(0, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

rewrite:
		if (rem >= FD_SETSIZE)
			errx(1, "descriptor too big");
		FD_ZERO(&rembits);
		FD_SET(rem, &rembits);
		nfds = rem + 1;
		if (select(nfds, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR)
				err(1, "select");
			goto rewrite;
		}
		if (!FD_ISSET(rem, &rembits))
			goto rewrite;
		wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		bp += wc;
		cc -= wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
done:
		(void)shutdown(rem, 1);
		exit(0);
	}

	tvtimeout.tv_sec = timeout;
	tvtimeout.tv_usec = 0;

	(void)sigsetmask(omask);
	if (rfd2 >= FD_SETSIZE || rem >= FD_SETSIZE)
		errx(1, "descriptor too big");
	FD_ZERO(&readfrom);
	FD_SET(rfd2, &readfrom);
	FD_SET(rem, &readfrom);
	nfds = MAX(rfd2+1, rem+1);
	do {
		ready = readfrom;
		if (timeout) {
			srval = select(nfds, &ready, 0, 0, &tvtimeout);
		} else {
			srval = select(nfds, &ready, 0, 0, 0);
		}

		if (srval < 0) {
			if (errno != EINTR)
				err(1, "select");
			continue;
		}
		if (srval == 0)
			errx(1, "timeout reached (%d seconds)", timeout);
		if (FD_ISSET(rfd2, &ready)) {
			errno = 0;
			cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rfd2, &readfrom);
			} else
				(void)write(STDERR_FILENO, buf, cc);
		}
		if (FD_ISSET(rem, &ready)) {
			errno = 0;
			cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rem, &readfrom);
			} else
				(void)write(STDOUT_FILENO, buf, cc);
		}
	} while (FD_ISSET(rfd2, &readfrom) || FD_ISSET(rem, &readfrom));
}

void
connect_timeout(int sig)
{
	char message[] = "timeout reached before connection completed.\n";

	write(STDERR_FILENO, message, sizeof(message) - 1);
	_exit(1);
}

void
sendsig(int sig)
{
	char signo;

	signo = sig;
	(void)write(rfd2, &signo, 1);
}

char *
copyargs(char * const *argv)
{
	int cc;
	char *args, *p;
	char * const *ap;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if (!(args = malloc((u_int)cc)))
		err(1, NULL);
	for (p = args, ap = argv; *ap; ++ap) {
		(void)strcpy(p, *ap);
		for (p = strcpy(p, *ap); *p; ++p);
		if (ap[1])
			*p++ = ' ';
	}
	return (args);
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "usage: rsh [-46dn] [-l login] [-t timeout] host [command]\n");
	exit(1);
}
