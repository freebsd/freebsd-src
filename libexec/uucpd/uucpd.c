/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static const char copyright[] =
"@(#) Copyright (c) 1985, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)uucpd.c	8.1 (Berkeley) 6/4/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * 4.2BSD TCP/IP server for uucico
 * uucico's TCP channel causes this server to be run at the remote end.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/param.h>
#include "pathnames.h"

#if (MAXLOGNAME-1) > UT_NAMESIZE
#define LOGNAMESIZE UT_NAMESIZE
#else
#define LOGNAMESIZE (MAXLOGNAME-1)
#endif

#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))

struct sockaddr_storage hisctladdr;
int hisaddrlen = sizeof hisctladdr;
struct sockaddr_storage myctladdr;
int mypid;

char Username[64], Logname[64];
char *nenv[] = {
	Username,
	Logname,
	NULL,
};
extern char **environ;
extern void logwtmp(char *line, char *name, char *host);

void doit(struct sockaddr *sinp);
void dologout(void);
int readline(char start[], int num, int passw);
void dologin(struct passwd *pw, struct sockaddr *sin);

int main(int argc, char **argv)
{
	environ = nenv;
	close(1); close(2);
	dup(0); dup(0);
	hisaddrlen = sizeof (hisctladdr);
	openlog("uucpd", LOG_PID, LOG_DAEMON);
	if (getpeername(0, (struct sockaddr *)&hisctladdr, &hisaddrlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		_exit(1);
	}
	doit((struct sockaddr *)&hisctladdr);
	dologout();
	exit(0);
}

void badlogin(char *name, struct sockaddr *sin)
{
	char remotehost[MAXHOSTNAMELEN];

	realhostname_sa(remotehost, sizeof(remotehost) - 1, sin, sin->sa_len);
	remotehost[sizeof(remotehost) - 1] = '\0';

	syslog(LOG_NOTICE, "LOGIN FAILURE FROM %s", remotehost);
	syslog(LOG_AUTHPRIV|LOG_NOTICE,
	    "LOGIN FAILURE FROM %s, %s", remotehost, name);

	fprintf(stderr, "Login incorrect.\n");
	exit(1);
}

void doit(struct sockaddr *sinp)
{
	char user[64], passwd[64];
	char *xpasswd, *crypt();
	struct passwd *pw;
	pid_t s;
	int pwdok =0;

	alarm(60);
	do {
		printf("login: "); fflush(stdout);
		errno = 0;
		if (readline(user, sizeof user, 0) < 0) {
			syslog(LOG_WARNING, "login read: %m");
			_exit(1);
		}
	} while (user[0] == '\0');
	/* truncate username to LOGNAMESIZE characters */
	user[LOGNAMESIZE] = '\0';
	pw = getpwnam(user);
	/*
	 * Fail after password if:
	 * 1. Invalid user
	 * 2. Shell is not uucico
	 * 3. Account has expired
	 * 4. Password is incorrect
	 */
	if (pw != NULL && strcmp(pw->pw_shell, _PATH_UUCICO) == 0 &&
	    (!pw->pw_expire || time(NULL) >= pw->pw_expire))
		pwdok = 1;
	/* always ask for passwords to deter account guessing */
	if (!pwdok || (pw->pw_passwd && *pw->pw_passwd != '\0')) {
		printf("Password: "); fflush(stdout);
		errno = 0;
		if (readline(passwd, sizeof passwd, 1) < 0) {
			syslog(LOG_WARNING, "passwd for '%s' read: %m", user);
			_exit(1);
		}
		if (pwdok) {
			xpasswd = crypt(passwd, pw->pw_passwd);
			if (strcmp(xpasswd, pw->pw_passwd))
				pwdok = 0;
		}
		if (!pwdok)
			badlogin(user, sinp);
	}
	alarm(0);
	sprintf(Username, "USER=%s", pw->pw_name);
	sprintf(Logname, "LOGNAME=%s", pw->pw_name);
	if ((s = fork()) < 0) {
		syslog(LOG_ERR, "fork: %m");
		_exit(1);
	} else if (s == 0) {
		dologin(pw, sinp);
		setgid(pw->pw_gid);
		initgroups(pw->pw_name, pw->pw_gid);
		chdir(pw->pw_dir);
		setuid(pw->pw_uid);
		execl(pw->pw_shell, "uucico", (char *)NULL);
		syslog(LOG_ERR, "execl: %m");
		_exit(1);
	}
}

int readline(char start[], int num, int passw)
{
	char c;
	register char *p = start;
	register int n = num;

	while (n-- > 0) {
		if (read(0, &c, 1) <= 0)
			return(-1);
		c &= 0177;
		if (c == '\n' || c == '\r' || c == '\0') {
			if (p == start && passw) {
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

void dologout(void)
{
	union wait status;
	pid_t pid;
	char line[32];

	while ((pid=wait((int *)&status)) > 0) {
		sprintf(line, "uucp%ld", (long)pid);
		logwtmp(line, "", "");
	}
}

/*
 * Record login in wtmp file.
 */
void dologin(struct passwd *pw, struct sockaddr *sin)
{
	char line[32];
	char remotehost[UT_HOSTSIZE + 1];
	int f;
	time_t cur_time;

	realhostname_sa(remotehost, sizeof(remotehost) - 1, sin, sin->sa_len);
	remotehost[sizeof remotehost - 1] = '\0';

	/* hack, but must be unique and no tty line */
	sprintf(line, "uucp%ld", (long)getpid());
	time(&cur_time);
	if ((f = open(_PATH_LASTLOG, O_RDWR)) >= 0) {
		struct lastlog ll;

		ll.ll_time = cur_time;
		lseek(f, (off_t)pw->pw_uid * sizeof(struct lastlog), L_SET);
		SCPYN(ll.ll_line, line);
		SCPYN(ll.ll_host, remotehost);
		(void) write(f, (char *) &ll, sizeof ll);
		(void) close(f);
	}
	logwtmp(line, pw->pw_name, remotehost);
}
