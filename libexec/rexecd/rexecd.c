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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rexecd.c	8.1 (Berkeley) 6/4/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/openpam.h>

static pam_handle_t *pamh;
static struct pam_conv pamc = {
	openpam_nullconv,
	NULL
};
static int pam_flags = PAM_SILENT|PAM_DISALLOW_NULL_AUTHTOK;
static int pam_err;
#define pam_ok(err) ((pam_err = (err)) == PAM_SUCCESS)

char	**environ;
char	remote[MAXHOSTNAMELEN];

struct	sockaddr_storage asin;

static void doit(struct sockaddr *);
static void getstr(char *, int, char *);
static void error(const char *fmt, ...);

int no_uid_0 = 1;

/*
 * remote execute server:
 *	username\0
 *	password\0
 *	command\0
 *	data
 */
/*ARGSUSED*/
int
main(int argc, char *argv[])
{
	struct sockaddr_storage from;
	socklen_t fromlen;
	int ch;

	openlog("rexecd", LOG_PID, LOG_AUTH);

	while ((ch = getopt(argc, argv, "i")) != -1)
		switch (ch) {
		case 'i':
			no_uid_0 = 0;
			break;
		default:
			syslog(LOG_ERR, "usage: rexecd [-i]");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0)
		err(1, "getpeername");

	realhostname_sa(remote, sizeof(remote) - 1,
			(struct sockaddr *)&from, fromlen);

	doit((struct sockaddr *)&from);
	return(0);
}

static void
doit(struct sockaddr *fromp)
{
	char cmdbuf[NCARGS+1], *cp;
	const char *namep;
	char user[16], pass[16];
	struct passwd *pwd;
	int fd, r, sd;
	u_short port;
	int pv[2], pid, cc, nfds;
	fd_set rfds, fds;
	char buf[BUFSIZ], sig;
	int one = 1;
	char **envlist, **env;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
	dup2(STDIN_FILENO, STDOUT_FILENO);
	dup2(STDIN_FILENO, STDOUT_FILENO);
	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}
	if (port != 0) {
		sd = socket(fromp->sa_family, SOCK_STREAM, 0);
		if (sd < 0)
			exit(1);
		bzero(&asin, sizeof(asin));
		asin.ss_family = fromp->sa_family;
		asin.ss_len = fromp->sa_len;
		if (bind(sd, (struct sockaddr *)&asin, asin.ss_len) < 0)
			exit(1);
		switch (fromp->sa_family) {
		case AF_INET:
			((struct sockaddr_in *)fromp)->sin_port = htons(port);
			break;
		case AF_INET6:
			((struct sockaddr_in6 *)fromp)->sin6_port = htons(port);
			break;
		default:
			exit(1);
		}
		if (connect(sd, fromp, fromp->sa_len) < 0)
			exit(1);
	}
	getstr(user, sizeof(user), "username");
	getstr(pass, sizeof(pass), "password");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	(void) alarm(0);

	if ((pwd = getpwnam(user))  == NULL || (pwd->pw_uid = 0 && no_uid_0) ||
	    !pam_ok(pam_start("rexecd", user, &pamc, &pamh)) ||
	    !pam_ok(pam_set_item(pamh, PAM_RHOST, remote)) ||
	    !pam_ok(pam_set_item(pamh, PAM_AUTHTOK, pass)) ||
	    !pam_ok(pam_authenticate(pamh, pam_flags)) ||
	    !pam_ok(pam_acct_mgmt(pamh, pam_flags))) {
		syslog(LOG_ERR, "%s LOGIN REFUSED from %s", user, remote);
		error("Login incorrect.\n");
		exit(1);
	}

	syslog(LOG_INFO, "login from %s as %s", remote, user);

	(void) write(STDERR_FILENO, "\0", 1);
	if (port != 0) {
		(void) pipe(pv);

		pid = fork();
		if (pid == -1)  {
			error("Try again.\n");
			exit(1);
		}
		if (pid) {
			/* parent */
			(void) pam_end(pamh, pam_err);
			(void) close(STDIN_FILENO);
			(void) close(STDOUT_FILENO);
			(void) close(STDERR_FILENO);
			(void) close(pv[1]);
			ioctl(pv[0], FIONBIO, (char *)&one);
			/* should set sd nbio! */
			FD_ZERO(&fds);
			FD_SET(sd, &fds);
			nfds = sd + 1;
			FD_SET(pv[0], &fds);
			if (pv[0] >= nfds)
				nfds = pv[0] + 1;
			do {
				rfds = fds;
				for (;;) {
					r = select(nfds, &rfds, NULL, NULL, NULL);
					if (r > 0)
						break;
					if (r < 0 && errno != EINTR)
						exit(0);
				}
				if (FD_ISSET(sd, &rfds)) {
					if (read(sd, &sig, 1) <= 0)
						FD_CLR(sd, &fds);
					else
						killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &fds)) {
					cc = read(pv[0], buf, sizeof (buf));
					if (cc <= 0) {
						shutdown(sd, SHUT_RDWR);
						FD_CLR(pv[0], &fds);
					} else {
						(void) write(sd, buf, cc);
					}
				}
			} while (FD_ISSET(sd, &fds) || FD_ISSET(pv[0], &fds));
			exit(0);
		}
		/* child */
		(void) close(sd);
		(void) close(pv[0]);
		dup2(pv[1], 2);
	}
	for (fd = getdtablesize(); fd > 2; fd--)
		(void) close(fd);
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
	if (setsid() == -1)
		syslog(LOG_ERR, "setsid() failed: %m");
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");
	(void) setgid((gid_t)pwd->pw_gid);
	initgroups(pwd->pw_name, pwd->pw_gid);
	if (!pam_ok(pam_setcred(pamh, PAM_ESTABLISH_CRED)))
		syslog(LOG_ERR, "pam_setcred() failed: %s",
		    pam_strerror(pamh, pam_err));
	(void) pam_setenv(pamh, "HOME", pwd->pw_dir, 1);
	(void) pam_setenv(pamh, "SHELL", pwd->pw_shell, 1);
	(void) pam_setenv(pamh, "USER", pwd->pw_name, 1);
	(void) pam_setenv(pamh, "PATH", _PATH_DEFPATH, 1);
	environ = pam_getenvlist(pamh);
	(void) pam_end(pamh, pam_err);
	(void) setuid((uid_t)pwd->pw_uid);
	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	if (chdir(pwd->pw_dir) < 0) {
		error("No remote directory.\n");
		exit(1);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, (char *)0);
	err(1, "%s", pwd->pw_shell);
}

static void
error(const char *fmt, ...)
{
	char buf[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
	buf[0] = 1;
	(void)vsnprintf(buf + 1, sizeof(buf) - 1, fmt, ap);
	(void)write(STDERR_FILENO, buf, strlen(buf));
	va_end(ap);
}

static void
getstr(char *buf, int cnt, char *err)
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}
