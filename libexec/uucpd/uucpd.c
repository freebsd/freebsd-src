/*
 * Copyright (c) 1985 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
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
"@(#) Copyright (c) 1985 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)uucpd.c	5.10 (Berkeley) 2/26/91";
#endif /* not lint */

/*
 * 4.2BSD TCP/IP server for uucico
 * uucico's TCP channel causes this server to be run at the remote end.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utmp.h>
#include <syslog.h>
#include <varargs.h>
#include "pathnames.h"

#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))

struct	utmp utmp;

struct  sockaddr hisctladdr;
int hisaddrlen = sizeof hisctladdr;
struct  sockaddr myctladdr;
int mypid;

char Username[64];
char *nenv[] = {
	Username,
	NULL,
};
extern char **environ;

main(argc, argv)
int argc;
char **argv;
{
	extern int errno;
	int dologout();

	environ = nenv;
	close(1); close(2);
	dup(0); dup(0);
	hisaddrlen = sizeof (hisctladdr);
	openlog("uucpd", LOG_PID, LOG_DAEMON);
	if (getpeername(0, &hisctladdr, &hisaddrlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		_exit(1);
	}
	doit(&hisctladdr);
	dologout();
	_exit(0);
}

login_incorrect()
{
	char passwd[64];

	printf("Password: "); fflush(stdout);
	if (readline(passwd, sizeof passwd, 1) < 0) {
		syslog(LOG_WARNING, "passwd read");
		_exit(1);
	}
	fprintf(stderr, "Login incorrect.\n");
	exit(1);
}

doit(sinp)
struct sockaddr *sinp;
{
	char user[64], passwd[64], ubuf[64];
	char *xpasswd, *crypt();
	struct passwd *pw, *getpwnam();
	int s;

	alarm(60);
	printf("login: "); fflush(stdout);
	if (readline(user, sizeof user, 0) < 0) {
		syslog(LOG_WARNING, "login read");
		_exit(1);
	}
	/* truncate username to 8 characters */
	user[8] = '\0';
	pw = getpwnam(user);
	if (pw == NULL)
		login_incorrect();
	if (strcmp(pw->pw_shell, _PATH_UUCICO))
		login_incorrect();
	if (pw->pw_passwd && *pw->pw_passwd != '\0') {
		printf("Password: "); fflush(stdout);
		if (readline(passwd, sizeof passwd, 1) < 0) {
			syslog(LOG_WARNING, "passwd read");
			_exit(1);
		}
		xpasswd = crypt(passwd, pw->pw_passwd);
		if (strcmp(xpasswd, pw->pw_passwd)) {
			fprintf(stderr, "Login incorrect.\n");
			exit(1);
		}
	}
	alarm(0);
	sprintf(Username, "USER=%s", pw->pw_name);
	sprintf(ubuf, "-u%s", pw->pw_name);
	if ((s = fork()) < 0) {
		syslog(LOG_ERR, "fork: %m");
		_exit(1);
	} else if (s == 0) {
		dologin(pw, sinp);
		setgid(pw->pw_gid);
		initgroups(pw->pw_name, pw->pw_gid);
		chdir(pw->pw_dir);
		setuid(pw->pw_uid);
		execl(pw->pw_shell, "uucico", ubuf, NULL);
		syslog(LOG_ERR, "execl: %m");
		_exit(1);
	}
}

readline(start, num, pass)
char start[];
int num, pass;
{
	char c;
	register char *p = start;
	register int n = num;

	while (n-- > 0) {
		if (read(0, &c, 1) <= 0)
			return(-1);
		c &= 0177;
		if (c == '\n' || c == '\r' || c == '\0') {
			if (p == start && pass) {
				n++;
				continue;
			}
			*p = '\0';
			return(0);
		}
		if (c == 025) {
			n = num;
			p = start;
			continue;
		}
		*p++ = c;
	}
	return(-1);
}


dologout()
{
	union wait status;
	int pid;
	char line[32];

	while ((pid=wait((int *)&status)) > 0) {
		sprintf(line, "uu%d", pid);
		logout(line);
		logwtmp(line, "", "");
	}
}

/*
 * Record login in wtmp file.
 */
dologin(pw, sin)
struct passwd *pw;
struct sockaddr_in *sin;
{
	char line[32];
	char remotehost[32];
	int f;
	time_t cur_time;
	struct hostent *hp = gethostbyaddr((char *)&sin->sin_addr,
		sizeof (struct in_addr), AF_INET);

	if (hp) {
		strncpy(remotehost, hp->h_name, sizeof (remotehost));
		endhostent();
	} else
		strncpy(remotehost, inet_ntoa(sin->sin_addr),
		    sizeof (remotehost));
	sprintf(line, "uu%d", getpid());
	/* hack, but must be unique and no tty line */
	time(&cur_time);
	if ((f = open(_PATH_LASTLOG, O_RDWR)) >= 0) {
		struct lastlog ll;

		ll.ll_time = cur_time;
		lseek(f, (long)pw->pw_uid * sizeof(struct lastlog), L_SET);
		SCPYN(ll.ll_line, line);
		SCPYN(ll.ll_host, remotehost);
		(void) write(f, (char *) &ll, sizeof ll);
		(void) close(f);
	}
	utmp.ut_time = cur_time;
	SCPYN(utmp.ut_line, line);
	SCPYN(utmp.ut_name, pw->pw_name);
	SCPYN(utmp.ut_host, remotehost);
	login(&utmp);
}
