/*-
 * Copyright (c) 1988, 1989 The Regents of the University of California.
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
"@(#) Copyright (c) 1988, 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rshd.c	5.38 (Berkeley) 3/2/91";
#endif /* not lint */

/*
 * From:
 *	$Source: /a/cvs/386BSD/src/libexec/rshd/rshd.c,v $
 *	$Header: /mit/kerberos/ucb/mit/rshd/RCS/rshd.c,v 
 *		5.2 89/07/31 19:30:04 kfall Exp $
 */

/*
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <pwd.h>
#include <syslog.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>

int	keepalive = 1;
int	check_all = 0;
char	*index(), *rindex(), *strncat();
/*VARARGS1*/
int	error();
int	sent_null;

#ifdef	KERBEROS
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#define	VERSION_SIZE	9
#define SECURE_MESSAGE  "This rsh session is using DES encryption for all transmissions.\r\n"
#define	OPTIONS		"alknvx"
char	authbuf[sizeof(AUTH_DAT)];
char	tickbuf[sizeof(KTEXT_ST)];
int	doencrypt, use_kerberos, vacuous;
Key_schedule	schedule;
#else
#define	OPTIONS	"aln"
#endif

/*ARGSUSED*/
main(argc, argv)
	int argc;
	char **argv;
{
	extern int opterr, optind;
	extern int _check_rhosts_file;
	struct linger linger;
	int ch, on = 1, fromlen;
	struct sockaddr_in from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != EOF)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'l':
			_check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
#ifdef	KERBEROS
		case 'k':
			use_kerberos = 1;
			break;

		case 'v':
			vacuous = 1;
			break;

#ifdef CRYPT
		case 'x':
			doencrypt = 1;
			break;
#endif
#endif
		case '?':
		default:
			usage();
			exit(2);
		}

	argc -= optind;
	argv += optind;

#ifdef	KERBEROS
	if (use_kerberos && vacuous) {
		syslog(LOG_ERR, "only one of -k and -v allowed");
		exit(2);
	}
#ifdef CRYPT
	if (doencrypt && !use_kerberos) {
		syslog(LOG_ERR, "-k is required for -x");
		exit(2);
	}
#endif
#endif

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		syslog(LOG_ERR, "getpeername: %m");
		_exit(1);
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
	doit(&from);
}

char	username[20] = "USER=";
char	homedir[64] = "HOME=";
char	shell[64] = "SHELL=";
char	path[100] = "PATH=";
char	*envinit[] =
	    {homedir, shell, path, username, 0};
char	**environ;

doit(fromp)
	struct sockaddr_in *fromp;
{
	char cmdbuf[NCARGS+1], *cp;
	char locuser[16], remuser[16];
	struct passwd *pwd;
	int s;
	struct hostent *hp;
	char *hostname, *errorstr = NULL, *errorhost;
	u_short port;
	int pv[2], pid, cc;
	int nfd;
	fd_set ready, readfrom;
	char buf[BUFSIZ], sig;
	int one = 1;
	char remotehost[2 * MAXHOSTNAMELEN + 1];

#ifdef	KERBEROS
	AUTH_DAT	*kdata = (AUTH_DAT *) NULL;
	KTEXT		ticket = (KTEXT) NULL;
	char		instance[INST_SZ], version[VERSION_SIZE];
	struct		sockaddr_in	fromaddr;
	int		rc;
	long		authopts;
	int		pv1[2], pv2[2];
	fd_set		wready, writeto;

	fromaddr = *fromp;
#endif

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open(_PATH_TTY, 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	fromp->sin_port = ntohs((u_short)fromp->sin_port);
	if (fromp->sin_family != AF_INET) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)\n",
		    fromp->sin_family);
		exit(1);
	}
#ifdef IP_OPTIONS
      {
	u_char optbuf[BUFSIZ/3], *cp;
	char lbuf[BUFSIZ], *lp;
	int optsize = sizeof(optbuf), ipproto;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (!getsockopt(0, ipproto, IP_OPTIONS, (char *)optbuf, &optsize) &&
	    optsize != 0) {
		lp = lbuf;
		for (cp = optbuf; optsize > 0; cp++, optsize--, lp += 3)
			sprintf(lp, " %2.2x", *cp);
		syslog(LOG_NOTICE,
		    "Connection received from %s using IP options (ignored):%s",
		    inet_ntoa(fromp->sin_addr), lbuf);
		if (setsockopt(0, ipproto, IP_OPTIONS,
		    (char *)NULL, optsize) != 0) {
			syslog(LOG_ERR, "setsockopt IP_OPTIONS NULL: %m");
			exit(1);
		}
	}
      }
#endif

#ifdef	KERBEROS
	if (!use_kerberos)
#endif
		if (fromp->sin_port >= IPPORT_RESERVED ||
		    fromp->sin_port < IPPORT_RESERVED/2) {
			syslog(LOG_NOTICE|LOG_AUTH,
			    "Connection from %s on illegal port",
			    inet_ntoa(fromp->sin_addr));
			exit(1);
		}

	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if ((cc = read(0, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(0, 1+1);
			exit(1);
		}
		if (c== 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport = IPPORT_RESERVED - 1;
		s = rresvport(&lport);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
#ifdef	KERBEROS
		if (!use_kerberos)
#endif
			if (port >= IPPORT_RESERVED) {
				syslog(LOG_ERR, "2nd port not reserved\n");
				exit(1);
			}
		fromp->sin_port = htons(port);
		if (connect(s, (struct sockaddr *)fromp, sizeof (*fromp)) < 0) {
			syslog(LOG_INFO, "connect second port: %m");
			exit(1);
		}
	}

#ifdef	KERBEROS
	if (vacuous) {
		error("rshd: remote host requires Kerberos authentication\n");
		exit(1);
	}
#endif

#ifdef notdef
	/* from inetd, socket is already on 0, 1, 2 */
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
#endif
	hp = gethostbyaddr((char *)&fromp->sin_addr, sizeof (struct in_addr),
		fromp->sin_family);
	if (hp) {
		/*
		 * If name returned by gethostbyaddr is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		hostname = hp->h_name;
#ifdef	KERBEROS
		if (!use_kerberos)
#endif
		if (check_all || local_domain(hp->h_name)) {
			strncpy(remotehost, hp->h_name, sizeof(remotehost) - 1);
			remotehost[sizeof(remotehost) - 1] = 0;
			errorhost = remotehost;
#ifdef	RES_DNSRCH
			_res.options &= ~RES_DNSRCH;
#endif
			hp = gethostbyname(remotehost);
			if (hp == NULL) {
				syslog(LOG_INFO,
				    "Couldn't look up address for %s",
				    remotehost);
				errorstr =
				"Couldn't look up address for your host (%s)\n";
				hostname = inet_ntoa(fromp->sin_addr);
			} else for (; ; hp->h_addr_list++) {
				if (hp->h_addr_list[0] == NULL) {
					syslog(LOG_NOTICE,
					  "Host addr %s not listed for host %s",
					    inet_ntoa(fromp->sin_addr),
					    hp->h_name);
					errorstr =
					    "Host address mismatch for %s\n";
					hostname = inet_ntoa(fromp->sin_addr);
					break;
				}
				if (!bcmp(hp->h_addr_list[0],
				    (caddr_t)&fromp->sin_addr,
				    sizeof(fromp->sin_addr))) {
					hostname = hp->h_name;
					break;
				}
			}
		}
	} else
		errorhost = hostname = inet_ntoa(fromp->sin_addr);

#ifdef	KERBEROS
	if (use_kerberos) {
		kdata = (AUTH_DAT *) authbuf;
		ticket = (KTEXT) tickbuf;
		authopts = 0L;
		strcpy(instance, "*");
		version[VERSION_SIZE - 1] = '\0';
#ifdef CRYPT
		if (doencrypt) {
			struct sockaddr_in local_addr;
			rc = sizeof(local_addr);
			if (getsockname(0, (struct sockaddr *)&local_addr,
			    &rc) < 0) {
				syslog(LOG_ERR, "getsockname: %m");
				error("rlogind: getsockname: %m");
				exit(1);
			}
			authopts = KOPT_DO_MUTUAL;
			rc = krb_recvauth(authopts, 0, ticket,
				"rcmd", instance, &fromaddr,
				&local_addr, kdata, "", schedule,
				version);
			des_set_key(kdata->session, schedule);
		} else
#endif
			rc = krb_recvauth(authopts, 0, ticket, "rcmd",
				instance, &fromaddr,
				(struct sockaddr_in *) 0,
				kdata, "", (bit_64 *) 0, version);
		if (rc != KSUCCESS) {
			error("Kerberos authentication failure: %s\n",
				  krb_err_txt[rc]);
			exit(1);
		}
	} else
#endif
		getstr(remuser, sizeof(remuser), "remuser");

	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	setpwent();
	pwd = getpwnam(locuser);
	if (pwd == NULL) {
		if (errorstr == NULL)
			errorstr = "Login incorrect.\n";
		goto fail;
	}
	if (chdir(pwd->pw_dir) < 0) {
		(void) chdir("/");
#ifdef notdef
		error("No remote directory.\n");
		exit(1);
#endif
	}

#ifdef	KERBEROS
	if (use_kerberos) {
		if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0') {
			if (kuserok(kdata, locuser) != 0) {
				syslog(LOG_NOTICE|LOG_AUTH,
				    "Kerberos rsh denied to %s.%s@%s",
				    kdata->pname, kdata->pinst, kdata->prealm);
				error("Permission denied.\n");
				exit(1);
			}
		}
	} else
#endif

		if (errorstr ||
		    pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
		    ruserok(hostname, pwd->pw_uid == 0, remuser, locuser) < 0) {
fail:
			if (errorstr == NULL)
				errorstr = "Permission denied.\n";
			error(errorstr, errorhost);
			exit(1);
		}

	if (pwd->pw_uid && !access(_PATH_NOLOGIN, F_OK)) {
		error("Logins currently disabled.\n");
		exit(1);
	}

	(void) write(2, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			exit(1);
		}
#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			if (pipe(pv1) < 0) {
				error("Can't make 2nd pipe.\n");
				exit(1);
			}
			if (pipe(pv2) < 0) {
				error("Can't make 3rd pipe.\n");
				exit(1);
			}
		}
#endif
#endif
		pid = fork();
		if (pid == -1)  {
			error("Can't fork; try again.\n");
			exit(1);
		}
		if (pid) {
#ifdef CRYPT
#ifdef KERBEROS
			if (doencrypt) {
				static char msg[] = SECURE_MESSAGE;
				(void) close(pv1[1]);
				(void) close(pv2[1]);
				des_write(s, msg, sizeof(msg));

			} else
#endif
#endif
			{
				(void) close(0); (void) close(1);
			}
			(void) close(2); (void) close(pv[1]);

			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			if (pv[0] > s)
				nfd = pv[0];
			else
				nfd = s;
#ifdef CRYPT
#ifdef KERBEROS
			if (doencrypt) {
				FD_ZERO(&writeto);
				FD_SET(pv2[0], &writeto);
				FD_SET(pv1[0], &readfrom);

				nfd = MAX(nfd, pv2[0]);
				nfd = MAX(nfd, pv1[0]);
			} else
#endif
#endif
				ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			nfd++;
			do {
				ready = readfrom;
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt) {
					wready = writeto;
					if (select(nfd, &ready,
					    &wready, (fd_set *) 0,
					    (struct timeval *) 0) < 0)
						break;
				} else
#endif
#endif
					if (select(nfd, &ready, (fd_set *)0,
					  (fd_set *)0, (struct timeval *)0) < 0)
						break;
				if (FD_ISSET(s, &ready)) {
					int	ret;
#ifdef CRYPT
#ifdef KERBEROS
					if (doencrypt)
						ret = des_read(s, &sig, 1);
					else
#endif
#endif
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
#ifdef CRYPT
#ifdef KERBEROS
						if (doencrypt)
							(void)
							  des_write(s, buf, cc);
						else
#endif
#endif
							(void)
							  write(s, buf, cc);
					}
				}
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt && FD_ISSET(pv1[0], &ready)) {
					errno = 0;
					cc = read(pv1[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv1[0], 1+1);
						FD_CLR(pv1[0], &readfrom);
					} else
						(void) des_write(1, buf, cc);
				}

				if (doencrypt && FD_ISSET(pv2[0], &wready)) {
					errno = 0;
					cc = des_read(0, buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv2[0], 1+1);
						FD_CLR(pv2[0], &writeto);
					} else
						(void) write(pv2[0], buf, cc);
				}
#endif
#endif

			} while (FD_ISSET(s, &readfrom) ||
#ifdef CRYPT
#ifdef KERBEROS
			    (doencrypt && FD_ISSET(pv1[0], &readfrom)) ||
#endif
#endif
			    FD_ISSET(pv[0], &readfrom));
			exit(0);
		}
		setpgrp(0, getpid());
		(void) close(s); (void) close(pv[0]);
#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			close(pv1[0]); close(pv2[0]);
			dup2(pv1[1], 1);
			dup2(pv2[1], 0);
			close(pv1[1]);
			close(pv2[1]);
		}
#endif
#endif
		dup2(pv[1], 2);
		close(pv[1]);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
#if	BSD > 43
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");
#endif
	(void) setgid((gid_t)pwd->pw_gid);
	initgroups(pwd->pw_name, pwd->pw_gid);
	(void) setuid((uid_t)pwd->pw_uid);
	environ = envinit;
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	strcat(path, _PATH_DEFPATH);
	strncat(shell, pwd->pw_shell, sizeof(shell)-7);
	strncat(username, pwd->pw_name, sizeof(username)-6);
	cp = rindex(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	endpwent();
	if (pwd->pw_uid == 0) {
#ifdef	KERBEROS
		if (use_kerberos)
			syslog(LOG_INFO|LOG_AUTH,
				"ROOT Kerberos shell from %s.%s@%s on %s, comm: %s\n",
				kdata->pname, kdata->pinst, kdata->prealm,
				hostname, cmdbuf);
		else
#endif
			syslog(LOG_INFO|LOG_AUTH,
				"ROOT shell from %s@%s, comm: %s\n",
				remuser, hostname, cmdbuf);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, 0);
	perror(pwd->pw_shell);
	exit(1);
}

/*
 * Report error to client.
 * Note: can't be used until second socket has connected
 * to client, or older clients will hang waiting
 * for that connection first.
 */
/*VARARGS1*/
error(fmt, a1, a2, a3)
	char *fmt;
	int a1, a2, a3;
{
	char buf[BUFSIZ], *bp = buf;

	if (sent_null == 0)
		*bp++ = 1;
	(void) sprintf(bp, fmt, a1, a2, a3);
	(void) write(2, buf, strlen(buf));
}

getstr(buf, cnt, err)
	char *buf;
	int cnt;
	char *err;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
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
local_domain(h)
	char *h;
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2, *topdomain();

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost));
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return(1);
	return(0);
}

char *
topdomain(h)
	char *h;
{
	register char *p;
	char *maybe = NULL;
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

usage()
{
	syslog(LOG_ERR, "usage: rshd [-%s]", OPTIONS);
}
