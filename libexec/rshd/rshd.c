/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
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
"@(#) Copyright (c) 1988, 1989, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)rshd.c	8.2 (Berkeley) 4/6/94";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * remote shell server:
 *	[port]\0
 *	ruser\0
 *	luser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
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
#include <login_cap.h>

#include <security/pam_appl.h>
#include <security/openpam.h>
#include <sys/wait.h>

static struct pam_conv pamc = { openpam_nullconv, NULL };
static pam_handle_t *pamh;
static int pam_err;

#define PAM_END { \
	if ((pam_err = pam_setcred(pamh, PAM_DELETE_CRED)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_setcred(): %s", pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_close_session(pamh,0)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_close_session(): %s", pam_strerror(pamh, pam_err)); \
	if ((pam_err = pam_end(pamh, pam_err)) != PAM_SUCCESS) \
		syslog(LOG_ERR|LOG_AUTH, "pam_end(): %s", pam_strerror(pamh, pam_err)); \
}

int	keepalive = 1;
int	log_success;		/* If TRUE, log all successful accesses */
int	sent_null;
int	no_delay;

void	 doit(struct sockaddr *);
static void	 rshd_errx(int, const char *, ...) __printf0like(2, 3);
void	 getstr(char *, int, const char *);
int	 local_domain(char *);
char	*topdomain(char *);
void	 usage(void);

char	 slash[] = "/";
char	 bshell[] = _PATH_BSHELL;

#define	OPTIONS	"aDLln"

int
main(int argc, char *argv[])
{
	extern int __check_rhosts_file;
	struct linger linger;
	int ch, on = 1, fromlen;
	struct sockaddr_storage from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch (ch) {
		case 'a':
			/* ignored for compatibility */
			break;
		case 'l':
			__check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
		case 'D':
			no_delay = 1;
			break;
		case 'L':
			log_success = 1;
			break;
		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		exit(1);
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	if (no_delay &&
	    setsockopt(0, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (TCP_NODELAY): %m");
	doit((struct sockaddr *)&from);
	/* NOTREACHED */
	return(0);
}

extern char **environ;

void
doit(struct sockaddr *fromp)
{
	extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
	struct passwd *pwd;
	u_short port;
	fd_set ready, readfrom;
	int cc, fd, nfd, pv[2], pid, s;
	int one = 1;
	const char *cp, *errorstr;
	char sig, buf[BUFSIZ];
	char cmdbuf[NCARGS+1], luser[16], ruser[16];
	char rhost[2 * MAXHOSTNAMELEN + 1];
	char numericname[INET6_ADDRSTRLEN];
	int af, srcport;
	login_cap_t *lc;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
	af = fromp->sa_family;
	srcport = ntohs(*((in_port_t *)&fromp->sa_data));
	if (af == AF_INET) {
		inet_ntop(af, &((struct sockaddr_in *)fromp)->sin_addr,
		    numericname, sizeof numericname);
	} else if (af == AF_INET6) {
		inet_ntop(af, &((struct sockaddr_in6 *)fromp)->sin6_addr,
		    numericname, sizeof numericname);
	} else {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)", af);
		exit(1);
	}
#ifdef IP_OPTIONS
	if (af == AF_INET) {
		u_char optbuf[BUFSIZ/3];
		int optsize = sizeof(optbuf), ipproto, i;
		struct protoent *ip;

		if ((ip = getprotobyname("ip")) != NULL)
			ipproto = ip->p_proto;
		else
			ipproto = IPPROTO_IP;
		if (!getsockopt(0, ipproto, IP_OPTIONS, optbuf, &optsize) &&
		    optsize != 0) {
			for (i = 0; i < optsize; ) {
				u_char c = optbuf[i];
				if (c == IPOPT_LSRR || c == IPOPT_SSRR) {
					syslog(LOG_NOTICE,
					    "connection refused from %s with IP option %s",
					    numericname,
					    c == IPOPT_LSRR ? "LSRR" : "SSRR");
					exit(1);
				}
				if (c == IPOPT_EOL)
					break;
				i += (c == IPOPT_NOP) ? 1 : optbuf[i+1];
			}
		}
	}
#endif

	if (srcport >= IPPORT_RESERVED ||
	    srcport < IPPORT_RESERVED/2) {
		syslog(LOG_NOTICE|LOG_AUTH,
		    "connection from %s on illegal port %u",
		    numericname,
		    srcport);
		exit(1);
	}

	(void) alarm(60);
	port = 0;
	s = 0;		/* not set or used if port == 0 */
	for (;;) {
		char c;
		if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(0, 1+1);
			exit(1);
		}
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport = IPPORT_RESERVED - 1;
		s = rresvport_af(&lport, af);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
		if (port >= IPPORT_RESERVED ||
		    port < IPPORT_RESERVED/2) {
			syslog(LOG_NOTICE|LOG_AUTH,
			    "2nd socket from %s on unreserved port %u",
			    numericname,
			    port);
			exit(1);
		}
		*((in_port_t *)&fromp->sa_data) = htons(port);
		if (connect(s, fromp, fromp->sa_len) < 0) {
			syslog(LOG_INFO, "connect second port %d: %m", port);
			exit(1);
		}
	}

	errorstr = NULL;
	realhostname_sa(rhost, sizeof(rhost) - 1, fromp, fromp->sa_len);
	rhost[sizeof(rhost) - 1] = '\0';
	/* XXX truncation! */

	(void) alarm(60);
	getstr(ruser, sizeof(ruser), "ruser");
	getstr(luser, sizeof(luser), "luser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	(void) alarm(0);

	pam_err = pam_start("rsh", luser, &pamc, &pamh);
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_ERR|LOG_AUTH, "pam_start(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(1, "Login incorrect.");
	}

	if ((pam_err = pam_set_item(pamh, PAM_RUSER, ruser)) != PAM_SUCCESS ||
	    (pam_err = pam_set_item(pamh, PAM_RHOST, rhost) != PAM_SUCCESS)) {
		syslog(LOG_ERR|LOG_AUTH, "pam_set_item(): %s",
		    pam_strerror(pamh, pam_err));
		rshd_errx(1, "Login incorrect.");
	}

	pam_err = pam_authenticate(pamh, 0);
	if (pam_err == PAM_SUCCESS) {
		if ((pam_err = pam_get_user(pamh, &cp, NULL)) == PAM_SUCCESS) {
			strncpy(luser, cp, sizeof(luser));
			luser[sizeof(luser) - 1] = '\0';
			/* XXX truncation! */
		}
		pam_err = pam_acct_mgmt(pamh, 0);
	}
	if (pam_err != PAM_SUCCESS) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
		    ruser, rhost, luser, pam_strerror(pamh, pam_err), cmdbuf);
		rshd_errx(1, "Login incorrect.");
	}

	setpwent();
	pwd = getpwnam(luser);
	if (pwd == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    ruser, rhost, luser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Login incorrect.";
		rshd_errx(1, errorstr, rhost);
	}

	lc = login_getpwclass(pwd);
	if (pwd->pw_uid)
		auth_checknologin(lc);

	if (chdir(pwd->pw_dir) < 0) {
		if (chdir("/") < 0 ||
		    login_getcapbool(lc, "requirehome", !!pwd->pw_uid)) {
			syslog(LOG_INFO|LOG_AUTH,
			"%s@%s as %s: no home directory. cmd='%.80s'",
			ruser, rhost, luser, cmdbuf);
			rshd_errx(0, "No remote home directory.");
		}
		pwd->pw_dir = slash;
	}

	if (lc != NULL && fromp->sa_family == AF_INET) {	/*XXX*/
		char	remote_ip[MAXHOSTNAMELEN];

		strncpy(remote_ip, numericname,
			sizeof(remote_ip) - 1);
		remote_ip[sizeof(remote_ip) - 1] = 0;
		/* XXX truncation! */
		if (!auth_hostok(lc, rhost, remote_ip)) {
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
			    ruser, rhost, luser, __rcmd_errstr,
			    cmdbuf);
			rshd_errx(1, "Login incorrect.");
		}
		if (!auth_timeok(lc, time(NULL)))
			rshd_errx(1, "Logins not available right now");
	}

	/*
	 * PAM modules might add supplementary groups in
	 * pam_setcred(), so initialize them first.
	 * But we need to open the session as root.
	 */
	if (setusercontext(lc, pwd, pwd->pw_uid, LOGIN_SETGROUP) != 0) {
		syslog(LOG_ERR, "setusercontext: %m");
		exit(1);
	}

	if ((pam_err = pam_open_session(pamh, 0)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_open_session: %s", pam_strerror(pamh, pam_err));
	} else if ((pam_err = pam_setcred(pamh, PAM_ESTABLISH_CRED)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_setcred: %s", pam_strerror(pamh, pam_err));
	}

	(void) write(STDERR_FILENO, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0)
			rshd_errx(1, "Can't make pipe.");
		pid = fork();
		if (pid == -1)
			rshd_errx(1, "Can't fork; try again.");
		if (pid) {
			(void) close(0);
			(void) close(1);
			(void) close(2);
			(void) close(pv[1]);

			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			if (pv[0] > s)
				nfd = pv[0];
			else
				nfd = s;
				ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			nfd++;
			do {
				ready = readfrom;
				if (select(nfd, &ready, (fd_set *)0,
				  (fd_set *)0, (struct timeval *)0) < 0)
					break;
				if (FD_ISSET(s, &ready)) {
					int	ret;
						ret = read(s, &sig, 1);
				if (ret <= 0)
					FD_CLR(s, &readfrom);
				else
					killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					errno = 0;
					cc = read(pv[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else {
						(void)write(s, buf, cc);
					}
				}

			} while (FD_ISSET(s, &readfrom) ||
			    FD_ISSET(pv[0], &readfrom));
			PAM_END;
			exit(0);
		}
		(void) close(s);
		(void) close(pv[0]);
		dup2(pv[1], 2);
		close(pv[1]);
	}
	else {
		pid = fork();
		if (pid == -1)
			rshd_errx(1, "Can't fork; try again.");
		if (pid) {
			/* Parent. */
			while (wait(NULL) > 0 || errno == EINTR)
				/* nothing */ ;
			PAM_END;
			exit(0);
		}
	}

	for (fd = getdtablesize(); fd > 2; fd--)
		(void) close(fd);
	if (setsid() == -1)
		syslog(LOG_ERR, "setsid() failed: %m");
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");

	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = bshell;
	(void) pam_setenv(pamh, "HOME", pwd->pw_dir, 1);
	(void) pam_setenv(pamh, "SHELL", pwd->pw_shell, 1);
	(void) pam_setenv(pamh, "USER", pwd->pw_name, 1);
	(void) pam_setenv(pamh, "PATH", _PATH_DEFPATH, 1);
	environ = pam_getenvlist(pamh);
	(void) pam_end(pamh, pam_err);
	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;

	if (setusercontext(lc, pwd, pwd->pw_uid,
		LOGIN_SETALL & ~LOGIN_SETGROUP) < 0) {
		syslog(LOG_ERR, "setusercontext(): %m");
		exit(1);
	}
	login_close(lc);
	endpwent();
	if (log_success || pwd->pw_uid == 0) {
		    syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
			ruser, rhost, luser, cmdbuf);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, (char *)NULL);
	err(1, "%s", pwd->pw_shell);
	exit(1);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */

static void
rshd_errx(int errcode, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if (sent_null == 0)
		write(STDERR_FILENO, "\1", 1);

	verrx(errcode, fmt, ap);
	/* NOTREACHED */
}

void
getstr(char *buf, int cnt, const char *error)
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0)
			rshd_errx(1, "%s too long", error);
	} while (c != 0);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
int
local_domain(char *h)
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2;

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost) - 1);
	localhost[sizeof(localhost) - 1] = '\0';
	/* XXX truncation! */
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return (1);
	return (0);
}

char *
topdomain(char *h)
{
	char *p, *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

void
usage(void)
{

	syslog(LOG_ERR, "usage: rshd [-%s]", OPTIONS);
	exit(2);
}
