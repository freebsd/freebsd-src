/*
 * Copyright (c) 1980, 1993
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
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)comsat.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <sgtty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmp.h>

int	debug = 0;
#define	dsyslog	if (debug) syslog

#define MAXIDLE	120

char	hostname[MAXHOSTNAMELEN];
struct	utmp *utmp = NULL;
time_t	lastmsgtime;
int	nutmp, uf;

void jkfprintf __P((FILE *, char[], char[], off_t));
void mailfor __P((char *));
void notify __P((struct utmp *, char[], off_t, int));
void onalrm __P((int));
void reapchildren __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct sockaddr_in from;
	register int cc;
	int fromlen;
	char msgbuf[256];

	/* verify proper invocation */
	fromlen = sizeof(from);
	if (getsockname(0, (struct sockaddr *)&from, &fromlen) < 0) {
		(void)fprintf(stderr,
		    "comsat: getsockname: %s.\n", strerror(errno));
		exit(1);
	}
	openlog("comsat", LOG_PID, LOG_DAEMON);
	if (chdir(_PATH_MAILDIR)) {
		syslog(LOG_ERR, "chdir: %s: %m", _PATH_MAILDIR);
		(void) recv(0, msgbuf, sizeof(msgbuf) - 1, 0);
		exit(1);
	}
	if ((uf = open(_PATH_UTMP, O_RDONLY, 0)) < 0) {
		syslog(LOG_ERR, "open: %s: %m", _PATH_UTMP);
		(void) recv(0, msgbuf, sizeof(msgbuf) - 1, 0);
		exit(1);
	}
	(void)time(&lastmsgtime);
	(void)gethostname(hostname, sizeof(hostname));
	onalrm(0);
	(void)signal(SIGALRM, onalrm);
	(void)signal(SIGTTOU, SIG_IGN);
	(void)signal(SIGCHLD, reapchildren);
	for (;;) {
		cc = recv(0, msgbuf, sizeof(msgbuf) - 1, 0);
		if (cc <= 0) {
			if (errno != EINTR)
				sleep(1);
			errno = 0;
			continue;
		}
		if (!nutmp)		/* no one has logged in yet */
			continue;
		sigblock(sigmask(SIGALRM));
		msgbuf[cc] = '\0';
		(void)time(&lastmsgtime);
		mailfor(msgbuf);
		sigsetmask(0L);
	}
}

void
reapchildren(signo)
	int signo;
{
	while (wait3(NULL, WNOHANG, NULL) > 0);
}

void
onalrm(signo)
	int signo;
{
	static u_int utmpsize;		/* last malloced size for utmp */
	static u_int utmpmtime;		/* last modification time for utmp */
	struct stat statbf;

	if (time(NULL) - lastmsgtime >= MAXIDLE)
		exit(0);
	(void)alarm((u_int)15);
	(void)fstat(uf, &statbf);
	if (statbf.st_mtime > utmpmtime) {
		utmpmtime = statbf.st_mtime;
		if (statbf.st_size > utmpsize) {
			utmpsize = statbf.st_size + 10 * sizeof(struct utmp);
			if ((utmp = realloc(utmp, utmpsize)) == NULL) {
				syslog(LOG_ERR, "%s", strerror(errno));
				exit(1);
			}
		}
		(void)lseek(uf, (off_t)0, L_SET);
		nutmp = read(uf, utmp, (int)statbf.st_size)/sizeof(struct utmp);
	}
}

void
mailfor(name)
	char *name;
{
	register struct utmp *utp = &utmp[nutmp];
	register char *cp;
	char *file;
	off_t offset;
	int folder;
	char buf[sizeof(_PATH_MAILDIR) + sizeof(utmp[0].ut_name) + 1];
	char buf2[sizeof(_PATH_MAILDIR) + sizeof(utmp[0].ut_name) + 1];

	if (!(cp = strchr(name, '@')))
		return;
	*cp = '\0';
	offset = atoi(cp + 1);
	if (!(cp = strchr(cp + 1, ':')))
		file = name;
	else
		file = cp + 1;
	sprintf(buf, "%s/%.*s", _PATH_MAILDIR, sizeof(utmp[0].ut_name), name);
	if (*file != '/') {
		sprintf(buf2, "%s/%.*s", _PATH_MAILDIR, sizeof(utmp[0].ut_name), file);
		file = buf2;
	}
	folder = strcmp(buf, file);
	while (--utp >= utmp)
		if (!strncmp(utp->ut_name, name, sizeof(utmp[0].ut_name)))
			notify(utp, file, offset, folder);
}

static char *cr;

void
notify(utp, file, offset, folder)
	register struct utmp *utp;
	char file[];
	off_t offset;
	int folder;
{
	FILE *tp;
	struct stat stb;
	struct sgttyb gttybuf;
	char tty[20], name[sizeof(utmp[0].ut_name) + 1];

	(void)snprintf(tty, sizeof(tty), "%s%.*s",
	    _PATH_DEV, (int)sizeof(utp->ut_line), utp->ut_line);
	if (strchr(tty + sizeof(_PATH_DEV) - 1, '/')) {
		/* A slash is an attempt to break security... */
		syslog(LOG_AUTH | LOG_NOTICE, "'/' in \"%s\"", tty);
		return;
	}
	if (stat(tty, &stb) || !(stb.st_mode & S_IEXEC)) {
		dsyslog(LOG_DEBUG, "%s: wrong mode on %s", utp->ut_name, tty);
		return;
	}
	dsyslog(LOG_DEBUG, "notify %s on %s\n", utp->ut_name, tty);
	if (fork())
		return;
	(void)signal(SIGALRM, SIG_DFL);
	(void)alarm((u_int)30);
	if ((tp = fopen(tty, "w")) == NULL) {
		dsyslog(LOG_ERR, "%s: %s", tty, strerror(errno));
		_exit(-1);
	}
	(void)ioctl(fileno(tp), TIOCGETP, &gttybuf);
	cr = (gttybuf.sg_flags&CRMOD) && !(gttybuf.sg_flags&RAW) ?
	    "\n" : "\n\r";
	(void)strncpy(name, utp->ut_name, sizeof(utp->ut_name));
	name[sizeof(name) - 1] = '\0';
	(void)fprintf(tp, "%s\007New mail for %s@%.*s\007 has arrived%s%s%s:%s----%s",
	    cr, name, (int)sizeof(hostname), hostname,
	    folder ? cr : "", folder ? "to " : "", folder ? file : "",
	    cr, cr);
	jkfprintf(tp, name, file, offset);
	(void)fclose(tp);
	_exit(0);
}

void
jkfprintf(tp, user, file, offset)
	register FILE *tp;
	char user[];
	char file[];
	off_t offset;
{
	register char *cp, ch;
	register FILE *fi;
	register int linecnt, charcnt, inheader;
	register struct passwd *p;
	char line[BUFSIZ];

	/* Set effective uid to user in case mail drop is on nfs */
	if ((p = getpwnam(user)) != NULL)
		(void) setuid(p->pw_uid);

	if ((fi = fopen(file, "r")) == NULL)
		return;

	(void)fseek(fi, offset, L_SET);
	/*
	 * Print the first 7 lines or 560 characters of the new mail
	 * (whichever comes first).  Skip header crap other than
	 * From, Subject, To, and Date.
	 */
	linecnt = 7;
	charcnt = 560;
	inheader = 1;
	while (fgets(line, sizeof(line), fi) != NULL) {
		if (inheader) {
			if (line[0] == '\n') {
				inheader = 0;
				continue;
			}
			if (line[0] == ' ' || line[0] == '\t' ||
			    strncmp(line, "From:", 5) &&
			    strncmp(line, "Subject:", 8))
				continue;
		}
		if (linecnt <= 0 || charcnt <= 0) {
			(void)fprintf(tp, "...more...%s", cr);
			(void)fclose(fi);
			return;
		}
		/* strip weird stuff so can't trojan horse stupid terminals */
		for (cp = line; (ch = *cp) && ch != '\n'; ++cp, --charcnt) {
			if (!isprint(ch)) {
				if (ch & 0x80)
					(void)fputs("M-", tp);
				ch &= 0177;
				if (!isprint(ch)) {
					if (ch == 0177)
						ch = '?';
					else
				ch |= 0x40;
					(void)fputc('^', tp);
				}
			}
			(void)fputc(ch, tp);
		}
		(void)fputs(cr, tp);
		--linecnt;
	}
	(void)fprintf(tp, "----%s\n", cr);
	(void)fclose(fi);
}
