/*
 * Copyright (c) 1983, 1991, 1993, 1994
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
"@(#) Copyright (c) 1983, 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)from: inetd.c	8.4 (Berkeley) 4/13/94";
#endif
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/inetd/inetd.c,v 1.80.2.1 2000/07/18 16:47:17 dwmalone Exp $";
#endif /* not lint */

/*
 * Inetd - Internet super-server
 *
 * This program invokes all internet services as needed.  Connection-oriented
 * services are invoked each time a connection is made, by creating a process.
 * This process is passed the connection as file descriptor 0 and is expected
 * to do a getpeername to find out the source host and port.
 *
 * Datagram oriented services are invoked when a datagram
 * arrives; a process is created and passed a pending message
 * on file descriptor 0.  Datagram servers may either connect
 * to their peer, freeing up the original socket for inetd
 * to receive further messages on, or ``take over the socket'',
 * processing all arriving datagrams and, eventually, timing
 * out.	 The first type of server is said to be ``multi-threaded'';
 * the second type of server ``single-threaded''.
 *
 * Inetd uses a configuration file which is read at startup
 * and, possibly, at some later time in response to a hangup signal.
 * The configuration file is ``free format'' with fields given in the
 * order shown below.  Continuation lines for an entry must being with
 * a space or tab.  All fields must be present in each entry.
 *
 *	service name			must be in /etc/services or must
 *					name a tcpmux service
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS (20)
 *
 * TCP services without official port numbers are handled with the
 * RFC1078-based tcpmux internal service. Tcpmux listens on port 1 for
 * requests. When a connection is made from a foreign host, the service
 * requested is passed to tcpmux, which looks it up in the servtab list
 * and returns the proper entry for the service. Tcpmux returns a
 * negative reply if the service doesn't exist, otherwise the invoked
 * server is expected to return the positive reply if the service type in
 * inetd.conf file has the prefix "tcpmux/". If the service type has the
 * prefix "tcpmux/+", tcpmux will return the positive reply for the
 * process; this is for compatibility with older server code, and also
 * allows you to invoke programs that use stdin/stdout without putting any
 * special server code in them. Services that use tcpmux are "nowait"
 * because they do not have a well-known port and hence cannot listen
 * for new requests.
 *
 * For RPC services
 *	service name/version		must be in /etc/rpc
 *	socket type			stream/dgram/raw/rdm/seqpacket
 *	protocol			must be in /etc/protocols
 *	wait/nowait			single-threaded/multi-threaded
 *	user				user to run daemon as
 *	server program			full path name
 *	server program arguments	maximum of MAXARGS
 *
 * Comment lines are indicated by a `#' in column 1.
 *
 * #ifdef IPSEC
 * Comment lines that start with "#@" denote IPsec policy string, as described
 * in ipsec_set_policy(3).  This will affect all the following items in
 * inetd.conf(8).  To reset the policy, just use "#@" line.  By default,
 * there's no IPsec policy.
 * #endif
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <tcpd.h>
#include <unistd.h>
#include <libutil.h>
#include <sysexits.h>
#include <ctype.h>

#include "inetd.h"
#include "pathnames.h"

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifndef IPSEC_POLICY_IPSEC	/* no ipsec support on old ipsec */
#undef IPSEC
#endif
#endif

/* wrapper for KAME-special getnameinfo() */
#ifndef NI_WITHSCOPEID
#define NI_WITHSCOPEID	0
#endif

#ifndef LIBWRAP_ALLOW_FACILITY
# define LIBWRAP_ALLOW_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_ALLOW_SEVERITY
# define LIBWRAP_ALLOW_SEVERITY LOG_INFO
#endif
#ifndef LIBWRAP_DENY_FACILITY
# define LIBWRAP_DENY_FACILITY LOG_AUTH
#endif
#ifndef LIBWRAP_DENY_SEVERITY
# define LIBWRAP_DENY_SEVERITY LOG_WARNING
#endif

#define ISWRAP(sep)	\
	   ( ((wrap_ex && !(sep)->se_bi) || (wrap_bi && (sep)->se_bi)) \
	&& ( ((sep)->se_accept && (sep)->se_socktype == SOCK_STREAM) \
	    || (sep)->se_socktype == SOCK_DGRAM))

#ifdef LOGIN_CAP
#include <login_cap.h>

/* see init.c */
#define RESOURCE_RC "daemon"

#endif

#ifndef	MAXCHILD
#define	MAXCHILD	-1		/* maximum number of this service
					   < 0 = no limit */
#endif

#ifndef	MAXCPM
#define	MAXCPM		-1		/* rate limit invocations from a
					   single remote address,
					   < 0 = no limit */
#endif

#define	TOOMANY		256		/* don't start more than TOOMANY */
#define	CNT_INTVL	60		/* servers in CNT_INTVL sec. */
#define	RETRYTIME	(60*10)		/* retry after bind or server fail */
#define MAX_MAXCHLD	32767		/* max allowable max children */

#define	SIGBLOCK	(sigmask(SIGCHLD)|sigmask(SIGHUP)|sigmask(SIGALRM))

int	allow_severity;
int	deny_severity;
int	wrap_ex = 0;
int	wrap_bi = 0;
int	debug = 0;
int	log = 0;
int	maxsock;			/* highest-numbered descriptor */
fd_set	allsock;
int	options;
int	timingout;
int	toomany = TOOMANY;
int	maxchild = MAXCHILD;
int	maxcpm = MAXCPM;
struct	servent *sp;
struct	rpcent *rpc;
char	*hostname = NULL;
struct	sockaddr_in *bind_sa4;
int	no_v4bind = 1;
#ifdef INET6
struct	sockaddr_in6 *bind_sa6;
int	no_v6bind = 1;
#endif
int	signalpipe[2];
#ifdef SANITY_CHECK
int	nsock;
#endif

struct	servtab *servtab;

extern struct biltin biltins[];

#define NUMINT	(sizeof(intab) / sizeof(struct inent))
char	*CONFIG = _PATH_INETDCONF;
char	*pid_file = _PATH_INETDPID;

#ifdef OLD_SETPROCTITLE
char	**Argv;
char 	*LastArg;
#endif

int
getvalue(arg, value, whine)
	char *arg, *whine;
	int  *value;
{
	int  tmp;
	char *p;

	tmp = strtol(arg, &p, 0);
	if (tmp < 1 || *p) {
		syslog(LOG_ERR, whine, arg);
		return 1;			/* failure */
	}
	*value = tmp;
	return 0;				/* success */
}

int
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	struct servtab *sep;
	struct passwd *pwd;
	struct group *grp;
	struct sigaction sa, saalrm, sachld, sahup, sapipe;
	int tmpint, ch, dofork;
	pid_t pid;
	char buf[50];
#ifdef LOGIN_CAP
	login_cap_t *lc = NULL;
#endif
	struct request_info req;
	int denied;
	char *service = NULL;
	char *pnm;
	union {
		struct sockaddr peer_un;
		struct sockaddr_in peer_un4;
		struct sockaddr_in6 peer_un6;
		struct sockaddr_storage peer_max;
	} p_un;
#define peer	p_un.peer_un
#define peer4	p_un.peer_un4
#define peer6	p_un.peer_un6
#define peermax	p_un.peer_max
	int i;
	struct addrinfo hints, *res;
	char *servname;
	int error;


#ifdef OLD_SETPROCTITLE
	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);
#endif

	openlog("inetd", LOG_PID | LOG_NOWAIT, LOG_DAEMON);

	while ((ch = getopt(argc, argv, "dlwWR:a:c:C:p:")) != -1)
		switch(ch) {
		case 'd':
			debug = 1;
			options |= SO_DEBUG;
			break;
		case 'l':
			log = 1;
			break;
		case 'R':
			getvalue(optarg, &toomany,
				"-R %s: bad value for service invocation rate");
			break;
		case 'c':
			getvalue(optarg, &maxchild,
				"-c %s: bad value for maximum children");
			break;
		case 'C':
			getvalue(optarg, &maxcpm,
				"-C %s: bad value for maximum children/minute");
			break;
		case 'a':
			hostname = optarg;
			break;
		case 'p':
			pid_file = optarg;
			break;
		case 'w':
			wrap_ex++;
			break;
		case 'W':
			wrap_bi++;
			break;
		case '?':
		default:
			syslog(LOG_ERR,
				"usage: inetd [-dlwW] [-a address] [-R rate]"
				" [-c maximum] [-C rate]"
				" [-p pidfile] [conf-file]");
			exit(EX_USAGE);
		}
	/*
	 * Initialize Bind Addrs.
	 *   When hostname is NULL, wild card bind addrs are obtained from
	 *   getaddrinfo(). But getaddrinfo() requires at least one of
	 *   hostname or servname is non NULL.
	 *   So when hostname is NULL, set dummy value to servname.
	 */
	servname = (hostname == NULL) ? "discard" /* dummy */ : NULL;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	error = getaddrinfo(hostname, servname, &hints, &res);
	if (error != 0) {
		syslog(LOG_ERR, "-a %s: %s", hostname, gai_strerror(error));
		if (error == EAI_SYSTEM)
			syslog(LOG_ERR, "%s", strerror(errno));
		exit(EX_USAGE);
	}
	do {
		if (res->ai_addr == NULL) {
			syslog(LOG_ERR, "-a %s: getaddrinfo failed", hostname);
			exit(EX_USAGE);
		}
		switch (res->ai_addr->sa_family) {
		case AF_INET:
			if (no_v4bind == 0)
				continue;
			bind_sa4 = (struct sockaddr_in *)res->ai_addr;
			/* init port num in case servname is dummy */
			bind_sa4->sin_port = 0;
			no_v4bind = 0;
			continue;
#ifdef INET6
		case AF_INET6:
			if (no_v6bind == 0)
				continue;
			bind_sa6 = (struct sockaddr_in6 *)res->ai_addr;
			/* init port num in case servname is dummy */
			bind_sa6->sin6_port = 0;
			no_v6bind = 0;
			continue;
#endif
		}
		if (no_v4bind == 0
#ifdef INET6
		    && no_v6bind == 0
#endif
		    )
			break;
	} while ((res = res->ai_next) != NULL);
	if (no_v4bind != 0
#ifdef INET6
	    && no_v6bind != 0
#endif
	    ) {
		syslog(LOG_ERR, "-a %s: unknown address family", hostname);
		exit(EX_USAGE);
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		CONFIG = argv[0];
	if (debug == 0) {
		FILE *fp;
		if (daemon(0, 0) < 0) {
			syslog(LOG_WARNING, "daemon(0,0) failed: %m");
		}
		/*
		 * In case somebody has started inetd manually, we need to
		 * clear the logname, so that old servers run as root do not
		 * get the user's logname..
		 */
		if (setlogin("") < 0) {
			syslog(LOG_WARNING, "cannot clear logname: %m");
			/* no big deal if it fails.. */
		}
		pid = getpid();
		fp = fopen(pid_file, "w");
		if (fp) {
			fprintf(fp, "%ld\n", (long)pid);
			fclose(fp);
		} else {
			syslog(LOG_WARNING, "%s: %m", pid_file);
		}
	}
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_handler = flag_retry;
	sigaction(SIGALRM, &sa, &saalrm);
	config();
	sa.sa_handler = flag_config;
	sigaction(SIGHUP, &sa, &sahup);
	sa.sa_handler = flag_reapchild;
	sigaction(SIGCHLD, &sa, &sachld);
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, &sapipe);

	{
		/* space for daemons to overwrite environment for ps */
#define	DUMMYSIZE	100
		char dummy[DUMMYSIZE];

		(void)memset(dummy, 'x', DUMMYSIZE - 1);
		dummy[DUMMYSIZE - 1] = '\0';
		(void)setenv("inetd_dummy", dummy, 1);
	}

	if (pipe(signalpipe) != 0) {
		syslog(LOG_ERR, "pipe: %m");
		exit(EX_OSERR);
	}
	FD_SET(signalpipe[0], &allsock);
#ifdef SANITY_CHECK
	nsock++;
#endif
	if (signalpipe[0] > maxsock)
	    maxsock = signalpipe[0];
	if (signalpipe[1] > maxsock)
	    maxsock = signalpipe[1];

	for (;;) {
	    int n, ctrl;
	    fd_set readable;

#ifdef SANITY_CHECK
	    if (nsock == 0) {
		syslog(LOG_ERR, "%s: nsock=0", __FUNCTION__);
		exit(EX_SOFTWARE);
	    }
#endif
	    readable = allsock;
	    if ((n = select(maxsock + 1, &readable, (fd_set *)0,
		(fd_set *)0, (struct timeval *)0)) <= 0) {
		    if (n < 0 && errno != EINTR) {
			syslog(LOG_WARNING, "select: %m");
			sleep(1);
		    }
		    continue;
	    }
	    /* handle any queued signal flags */
	    if (FD_ISSET(signalpipe[0], &readable)) {
		int n;
		if (ioctl(signalpipe[0], FIONREAD, &n) != 0) {
		    syslog(LOG_ERR, "ioctl: %m");
		    exit(EX_OSERR);
		}
		while (--n >= 0) {
		    char c;
		    if (read(signalpipe[0], &c, 1) != 1) {
			syslog(LOG_ERR, "read: %m");
			exit(EX_OSERR);
		    }
		    if (debug)
			warnx("handling signal flag %c", c);
		    switch(c) {
		    case 'A': /* sigalrm */
			retry();
			break;
		    case 'C': /* sigchld */
			reapchild();
			break;
		    case 'H': /* sighup */
			config();
			break;
		    }
		}
	    }
	    for (sep = servtab; n && sep; sep = sep->se_next)
	        if (sep->se_fd != -1 && FD_ISSET(sep->se_fd, &readable)) {
		    n--;
		    if (debug)
			    warnx("someone wants %s", sep->se_service);
		    if (sep->se_accept && sep->se_socktype == SOCK_STREAM) {
			    i = 1;
			    if (ioctl(sep->se_fd, FIONBIO, &i) < 0)
				    syslog(LOG_ERR, "ioctl (FIONBIO, 1): %m");
			    ctrl = accept(sep->se_fd, (struct sockaddr *)0,
				(int *)0);
			    if (debug)
				    warnx("accept, ctrl %d", ctrl);
			    if (ctrl < 0) {
				    if (errno != EINTR)
					    syslog(LOG_WARNING,
						"accept (for %s): %m",
						sep->se_service);
                                      if (sep->se_accept &&
                                          sep->se_socktype == SOCK_STREAM)
                                              close(ctrl);
				    continue;
			    }
			    i = 0;
			    if (ioctl(sep->se_fd, FIONBIO, &i) < 0)
				    syslog(LOG_ERR, "ioctl1(FIONBIO, 0): %m");
			    if (ioctl(ctrl, FIONBIO, &i) < 0)
				    syslog(LOG_ERR, "ioctl2(FIONBIO, 0): %m");
			    if (cpmip(sep, ctrl) < 0) {
				close(ctrl);
				continue;
			    }
		    } else
			    ctrl = sep->se_fd;
		    if (log && !ISWRAP(sep)) {
			    char pname[INET6_ADDRSTRLEN];
			    pnm = "unknown";
			    i = sizeof peermax;
			    if (getpeername(ctrl, (struct sockaddr *)
					    &peermax, &i)) {
				    i = sizeof peermax;
				    if (recvfrom(ctrl, buf, sizeof(buf),
					MSG_PEEK,
					(struct sockaddr *)&peermax,
					&i) >= 0) {
				      getnameinfo((struct sockaddr *)&peermax,
						  peer.sa_len,
						  pname, sizeof(pname),
						  NULL, 0, 
						  NI_NUMERICHOST|
						  NI_WITHSCOPEID);
				      pnm = pname;
				    }
			    } else {
			            getnameinfo((struct sockaddr *)&peermax,
						peer.sa_len,
						pname, sizeof(pname),
						NULL, 0, 
						NI_NUMERICHOST|
						NI_WITHSCOPEID);
				    pnm = pname;
			    }
			    syslog(LOG_INFO,"%s from %s", sep->se_service, pnm);
		    }
		    (void) sigblock(SIGBLOCK);
		    pid = 0;
		    /*
		     * Fork for all external services, builtins which need to
		     * fork and anything we're wrapping (as wrapping might
		     * block or use hosts_options(5) twist).
		     */
		    dofork = !sep->se_bi || sep->se_bi->bi_fork || ISWRAP(sep);
		    if (dofork) {
			    if (sep->se_count++ == 0)
				(void)gettimeofday(&sep->se_time, (struct timezone *)NULL);
			    else if (sep->se_count >= toomany) {
				struct timeval now;

				(void)gettimeofday(&now, (struct timezone *)NULL);
				if (now.tv_sec - sep->se_time.tv_sec >
				    CNT_INTVL) {
					sep->se_time = now;
					sep->se_count = 1;
				} else {
					syslog(LOG_ERR,
			"%s/%s server failing (looping), service terminated",
					    sep->se_service, sep->se_proto);
					close_sep(sep);
					sigsetmask(0L);
					if (!timingout) {
						timingout = 1;
						alarm(RETRYTIME);
					}
					continue;
				}
			    }
			    pid = fork();
		    }
		    if (pid < 0) {
			    syslog(LOG_ERR, "fork: %m");
			    if (sep->se_accept &&
				sep->se_socktype == SOCK_STREAM)
				    close(ctrl);
			    sigsetmask(0L);
			    sleep(1);
			    continue;
		    }
		    if (pid)
			addchild(sep, pid);
		    sigsetmask(0L);
		    if (pid == 0) {
			    if (dofork) {
				if (debug)
					warnx("+ closing from %d", maxsock);
				for (tmpint = maxsock; tmpint > 2; tmpint--)
					if (tmpint != ctrl)
						(void) close(tmpint);
				sigaction(SIGALRM, &saalrm, (struct sigaction *)0);
				sigaction(SIGCHLD, &sachld, (struct sigaction *)0);
				sigaction(SIGHUP, &sahup, (struct sigaction *)0);
				/* SIGPIPE reset before exec */
			    }
			    /*
			     * Call tcpmux to find the real service to exec.
			     */
			    if (sep->se_bi &&
				sep->se_bi->bi_fn == (void (*)()) tcpmux) {
				    sep = tcpmux(ctrl);
				    if (sep == NULL) {
					    close(ctrl);
					    _exit(0);
				    }
			    }
			    if (ISWRAP(sep)) {
				inetd_setproctitle("wrapping", ctrl);
				service = sep->se_server_name ?
				    sep->se_server_name : sep->se_service;
				request_init(&req, RQ_DAEMON, service, RQ_FILE, ctrl, NULL);
				fromhost(&req);
				deny_severity = LIBWRAP_DENY_FACILITY|LIBWRAP_DENY_SEVERITY;
				allow_severity = LIBWRAP_ALLOW_FACILITY|LIBWRAP_ALLOW_SEVERITY;
				denied = !hosts_access(&req);
				if (denied) {
				    syslog(deny_severity,
				        "refused connection from %.500s, service %s (%s)",
				        eval_client(&req), service, sep->se_proto);
				    if (sep->se_socktype != SOCK_STREAM)
					recv(ctrl, buf, sizeof (buf), 0);
				    if (dofork)
					_exit(0);
				}
				if (log) {
				    syslog(allow_severity,
				        "connection from %.500s, service %s (%s)",
					eval_client(&req), service, sep->se_proto);
				}
			    }
			    if (sep->se_bi) {
				(*sep->se_bi->bi_fn)(ctrl, sep);
			    } else {
				if (debug)
					warnx("%d execl %s",
						getpid(), sep->se_server);
				dup2(ctrl, 0);
				close(ctrl);
				dup2(0, 1);
				dup2(0, 2);
				if ((pwd = getpwnam(sep->se_user)) == NULL) {
					syslog(LOG_ERR,
					    "%s/%s: %s: no such user",
						sep->se_service, sep->se_proto,
						sep->se_user);
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof (buf), 0);
					_exit(EX_NOUSER);
				}
				grp = NULL;
				if (   sep->se_group != NULL
				    && (grp = getgrnam(sep->se_group)) == NULL
				   ) {
					syslog(LOG_ERR,
					    "%s/%s: %s: no such group",
						sep->se_service, sep->se_proto,
						sep->se_group);
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof (buf), 0);
					_exit(EX_NOUSER);
				}
				if (grp != NULL)
					pwd->pw_gid = grp->gr_gid;
#ifdef LOGIN_CAP
				if ((lc = login_getclass(sep->se_class)) == NULL) {
					/* error syslogged by getclass */
					syslog(LOG_ERR,
					    "%s/%s: %s: login class error",
						sep->se_service, sep->se_proto,
						sep->se_class);
					if (sep->se_socktype != SOCK_STREAM)
						recv(0, buf, sizeof (buf), 0);
					_exit(EX_NOUSER);
				}
#endif
				if (setsid() < 0) {
					syslog(LOG_ERR,
						"%s: can't setsid(): %m",
						 sep->se_service);
					/* _exit(EX_OSERR); not fatal yet */
				}
#ifdef LOGIN_CAP
				if (setusercontext(lc, pwd, pwd->pw_uid,
				    LOGIN_SETALL) != 0) {
					syslog(LOG_ERR,
					 "%s: can't setusercontext(..%s..): %m",
					 sep->se_service, sep->se_user);
					_exit(EX_OSERR);
				}
#else
				if (pwd->pw_uid) {
					if (setlogin(sep->se_user) < 0) {
						syslog(LOG_ERR,
						 "%s: can't setlogin(%s): %m",
						 sep->se_service, sep->se_user);
						/* _exit(EX_OSERR); not yet */
					}
					if (setgid(pwd->pw_gid) < 0) {
						syslog(LOG_ERR,
						  "%s: can't set gid %d: %m",
						  sep->se_service, pwd->pw_gid);
						_exit(EX_OSERR);
					}
					(void) initgroups(pwd->pw_name,
							pwd->pw_gid);
					if (setuid(pwd->pw_uid) < 0) {
						syslog(LOG_ERR,
						  "%s: can't set uid %d: %m",
						  sep->se_service, pwd->pw_uid);
						_exit(EX_OSERR);
					}
				}
#endif
				sigaction(SIGPIPE, &sapipe,
				    (struct sigaction *)0);
				execv(sep->se_server, sep->se_argv);
				syslog(LOG_ERR,
				    "cannot execute %s: %m", sep->se_server);
				if (sep->se_socktype != SOCK_STREAM)
					recv(0, buf, sizeof (buf), 0);
			    }
			    if (dofork)
				_exit(0);
		    }
		    if (sep->se_accept && sep->se_socktype == SOCK_STREAM)
			    close(ctrl);
		}
	}
}

/*
 * Add a signal flag to the signal flag queue for later handling
 */

void flag_signal(c)
    char c;
{
	if (write(signalpipe[1], &c, 1) != 1) {
		syslog(LOG_ERR, "write: %m");
		_exit(EX_OSERR);
	}
}

/*
 * Record a new child pid for this service. If we've reached the
 * limit on children, then stop accepting incoming requests.
 */

void
addchild(struct servtab *sep, pid_t pid)
{
#ifdef SANITY_CHECK
	if (sep->se_numchild >= sep->se_maxchild) {
		syslog(LOG_ERR, "%s: %d >= %d",
		    __FUNCTION__, sep->se_numchild, sep->se_maxchild);
		exit(EX_SOFTWARE);
	}
#endif
	if (sep->se_maxchild == 0)
		return;
	sep->se_pids[sep->se_numchild++] = pid;
	if (sep->se_numchild == sep->se_maxchild)
		disable(sep);
}

/*
 * Some child process has exited. See if it's on somebody's list.
 */

void
flag_reapchild(signo)
	int signo;
{
	flag_signal('C');
}

void
reapchild()
{
	int k, status;
	pid_t pid;
	struct servtab *sep;

	for (;;) {
		pid = wait3(&status, WNOHANG, (struct rusage *)0);
		if (pid <= 0)
			break;
		if (debug)
			warnx("%d reaped, status %#x", pid, status);
		for (sep = servtab; sep; sep = sep->se_next) {
			for (k = 0; k < sep->se_numchild; k++)
				if (sep->se_pids[k] == pid)
					break;
			if (k == sep->se_numchild)
				continue;
			if (sep->se_numchild == sep->se_maxchild)
				enable(sep);
			sep->se_pids[k] = sep->se_pids[--sep->se_numchild];
			if (status)
				syslog(LOG_WARNING,
				    "%s[%d]: exit status 0x%x",
				    sep->se_server, pid, status);
			break;
		}
	}
}

void
flag_config(signo)
	int signo;
{
	flag_signal('H');
}

void config()
{
	struct servtab *sep, *new, **sepp;
	long omask;

	if (!setconfig()) {
		syslog(LOG_ERR, "%s: %m", CONFIG);
		return;
	}
	for (sep = servtab; sep; sep = sep->se_next)
		sep->se_checked = 0;
	while ((new = getconfigent())) {
		if (getpwnam(new->se_user) == NULL) {
			syslog(LOG_ERR,
				"%s/%s: no such user '%s', service ignored",
				new->se_service, new->se_proto, new->se_user);
			continue;
		}
		if (new->se_group && getgrnam(new->se_group) == NULL) {
			syslog(LOG_ERR,
				"%s/%s: no such group '%s', service ignored",
				new->se_service, new->se_proto, new->se_group);
			continue;
		}
#ifdef LOGIN_CAP
		if (login_getclass(new->se_class) == NULL) {
			/* error syslogged by getclass */
			syslog(LOG_ERR,
				"%s/%s: %s: login class error, service ignored",
				new->se_service, new->se_proto, new->se_class);
			continue;
		}
#endif
		for (sep = servtab; sep; sep = sep->se_next)
			if (strcmp(sep->se_service, new->se_service) == 0 &&
			    strcmp(sep->se_proto, new->se_proto) == 0 &&
			    sep->se_family == new->se_family)
				break;
		if (sep != 0) {
			int i;

#define SWAP(a, b) { typeof(a) c = a; a = b; b = c; }
			omask = sigblock(SIGBLOCK);
			if (sep->se_nomapped != new->se_nomapped) {
				sep->se_nomapped = new->se_nomapped;
				sep->se_reset = 1;
			}
			/* copy over outstanding child pids */
			if (sep->se_maxchild && new->se_maxchild) {
				new->se_numchild = sep->se_numchild;
				if (new->se_numchild > new->se_maxchild)
					new->se_numchild = new->se_maxchild;
				memcpy(new->se_pids, sep->se_pids,
				    new->se_numchild * sizeof(*new->se_pids));
			}
			SWAP(sep->se_pids, new->se_pids);
			sep->se_maxchild = new->se_maxchild;
			sep->se_numchild = new->se_numchild;
			sep->se_maxcpm = new->se_maxcpm;
			/* might need to turn on or off service now */
			if (sep->se_fd >= 0) {
			      if (sep->se_maxchild
				  && sep->se_numchild == sep->se_maxchild) {
				      if (FD_ISSET(sep->se_fd, &allsock))
					  disable(sep);
			      } else {
				      if (!FD_ISSET(sep->se_fd, &allsock))
					  enable(sep);
			      }
			}
			sep->se_accept = new->se_accept;
			SWAP(sep->se_user, new->se_user);
			SWAP(sep->se_group, new->se_group);
#ifdef LOGIN_CAP
			SWAP(sep->se_class, new->se_class);
#endif
			SWAP(sep->se_server, new->se_server);
			SWAP(sep->se_server_name, new->se_server_name);
			for (i = 0; i < MAXARGV; i++)
				SWAP(sep->se_argv[i], new->se_argv[i]);
#ifdef IPSEC
			SWAP(sep->se_policy, new->se_policy);
			ipsecsetup(sep);
#endif
			sigsetmask(omask);
			freeconfig(new);
			if (debug)
				print_service("REDO", sep);
		} else {
			sep = enter(new);
			if (debug)
				print_service("ADD ", sep);
		}
		sep->se_checked = 1;
		if (ISMUX(sep)) {
			sep->se_fd = -1;
			continue;
		}
		switch (sep->se_family) {
		case AF_INET:
			if (no_v4bind != 0) {
				sep->se_fd = -1;
				continue;
			}
			break;
#ifdef INET6
		case AF_INET6:
			if (no_v6bind != 0) {
				sep->se_fd = -1;
				continue;
			}
			break;
#endif
		}
		if (!sep->se_rpc) {
			sp = getservbyname(sep->se_service, sep->se_proto);
			if (sp == 0) {
				syslog(LOG_ERR, "%s/%s: unknown service",
				sep->se_service, sep->se_proto);
				sep->se_checked = 0;
				continue;
			}
			switch (sep->se_family) {
			case AF_INET:
				if (sep->se_ctladdrinitok == 0) {
					memcpy(&sep->se_ctrladdr4, bind_sa4,
					       sizeof(sep->se_ctrladdr4));
					sep->se_ctrladdr_size =
						sizeof(sep->se_ctrladdr4);
				}
				if (sp->s_port != sep->se_ctrladdr4.sin_port) {
					sep->se_ctrladdr4.sin_port =
						sp->s_port;
					sep->se_reset = 1;
				}
				break;
#ifdef INET6
			case AF_INET6:
				if (sep->se_ctladdrinitok == 0) {
					memcpy(&sep->se_ctrladdr6, bind_sa6,
					       sizeof(sep->se_ctrladdr6));
					sep->se_ctrladdr_size =
						sizeof(sep->se_ctrladdr6);
				}
				if (sp->s_port !=
				    sep->se_ctrladdr6.sin6_port) {
					sep->se_ctrladdr6.sin6_port =
						sp->s_port;
					sep->se_reset = 1;
				}
				break;
#endif
			}
			if (sep->se_reset != 0 && sep->se_fd >= 0)
				close_sep(sep);
		} else {
			rpc = getrpcbyname(sep->se_service);
			if (rpc == 0) {
				syslog(LOG_ERR, "%s/%s unknown RPC service",
					sep->se_service, sep->se_proto);
				if (sep->se_fd != -1)
					(void) close(sep->se_fd);
				sep->se_fd = -1;
					continue;
			}
			if (rpc->r_number != sep->se_rpc_prog) {
				if (sep->se_rpc_prog)
					unregisterrpc(sep);
				sep->se_rpc_prog = rpc->r_number;
				if (sep->se_fd != -1)
					(void) close(sep->se_fd);
				sep->se_fd = -1;
			}
		}
		if (sep->se_fd == -1)
			setup(sep);
	}
	endconfig();
	/*
	 * Purge anything not looked at above.
	 */
	omask = sigblock(SIGBLOCK);
	sepp = &servtab;
	while ((sep = *sepp)) {
		if (sep->se_checked) {
			sepp = &sep->se_next;
			continue;
		}
		*sepp = sep->se_next;
		if (sep->se_fd >= 0)
			close_sep(sep);
		if (debug)
			print_service("FREE", sep);
		if (sep->se_rpc && sep->se_rpc_prog > 0)
			unregisterrpc(sep);
		freeconfig(sep);
		free((char *)sep);
	}
	(void) sigsetmask(omask);
}

void
unregisterrpc(sep)
	struct servtab *sep;
{
        int i;
        struct servtab *sepp;
	long omask;

	omask = sigblock(SIGBLOCK);
        for (sepp = servtab; sepp; sepp = sepp->se_next) {
                if (sepp == sep)
                        continue;
		if (sep->se_checked == 0 ||
                    !sepp->se_rpc ||
                    sep->se_rpc_prog != sepp->se_rpc_prog)
			continue;
                return;
        }
        if (debug)
                print_service("UNREG", sep);
        for (i = sep->se_rpc_lowvers; i <= sep->se_rpc_highvers; i++)
                pmap_unset(sep->se_rpc_prog, i);
        if (sep->se_fd != -1)
                (void) close(sep->se_fd);
        sep->se_fd = -1;
	(void) sigsetmask(omask);
}

void
flag_retry(signo)
	int signo;
{
	flag_signal('A');
}

void
retry()
{
	struct servtab *sep;

	timingout = 0;
	for (sep = servtab; sep; sep = sep->se_next)
		if (sep->se_fd == -1 && !ISMUX(sep))
			setup(sep);
}

void
setup(sep)
	struct servtab *sep;
{
	int on = 1;

	if ((sep->se_fd = socket(sep->se_family, sep->se_socktype, 0)) < 0) {
		if (debug)
			warn("socket failed on %s/%s",
				sep->se_service, sep->se_proto);
		syslog(LOG_ERR, "%s/%s: socket: %m",
		    sep->se_service, sep->se_proto);
		return;
	}
#define	turnon(fd, opt) \
setsockopt(fd, SOL_SOCKET, opt, (char *)&on, sizeof (on))
	if (strcmp(sep->se_proto, "tcp") == 0 && (options & SO_DEBUG) &&
	    turnon(sep->se_fd, SO_DEBUG) < 0)
		syslog(LOG_ERR, "setsockopt (SO_DEBUG): %m");
	if (turnon(sep->se_fd, SO_REUSEADDR) < 0)
		syslog(LOG_ERR, "setsockopt (SO_REUSEADDR): %m");
#ifdef SO_PRIVSTATE
	if (turnon(sep->se_fd, SO_PRIVSTATE) < 0)
		syslog(LOG_ERR, "setsockopt (SO_PRIVSTATE): %m");
#endif
	/* tftpd opens a new connection then needs more infos */
	if ((sep->se_family == AF_INET6) &&
	    (strcmp(sep->se_proto, "udp") == 0) &&
	    (sep->se_accept == 0) &&
	    (setsockopt(sep->se_fd, IPPROTO_IPV6, IPV6_PKTINFO,
			(char *)&on, sizeof (on)) < 0))
		syslog(LOG_ERR, "setsockopt (IPV6_RECVPKTINFO): %m");
#ifdef IPV6_BINDV6ONLY
	if (sep->se_family == AF_INET6) {
		int flag = sep->se_nomapped ? 1 : 0;
		if (setsockopt(sep->se_fd, IPPROTO_IPV6, IPV6_BINDV6ONLY,
			       (char *)&flag, sizeof (flag)) < 0)
			syslog(LOG_ERR, "setsockopt (IPV6_BINDV6ONLY): %m");
	}
#endif /* IPV6_BINDV6ONLY */
#undef turnon
	if (sep->se_type == TTCP_TYPE)
		if (setsockopt(sep->se_fd, IPPROTO_TCP, TCP_NOPUSH,
		    (char *)&on, sizeof (on)) < 0)
			syslog(LOG_ERR, "setsockopt (TCP_NOPUSH): %m");
#ifdef IPV6_FAITH
	if (sep->se_type == FAITH_TYPE) {
		if (setsockopt(sep->se_fd, IPPROTO_IPV6, IPV6_FAITH, &on,
				sizeof(on)) < 0) {
			syslog(LOG_ERR, "setsockopt (IPV6_FAITH): %m");
		}
	}
#endif
#ifdef IPSEC
	ipsecsetup(sep);
#endif
	if (bind(sep->se_fd, (struct sockaddr *)&sep->se_ctrladdr,
	    sep->se_ctrladdr_size) < 0) {
		if (debug)
			warn("bind failed on %s/%s",
				sep->se_service, sep->se_proto);
		syslog(LOG_ERR, "%s/%s: bind: %m",
		    sep->se_service, sep->se_proto);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
		if (!timingout) {
			timingout = 1;
			alarm(RETRYTIME);
		}
		return;
	}
        if (sep->se_rpc) {
                int i, len = sep->se_ctrladdr_size;

		if (sep->se_family != AF_INET) {
                        syslog(LOG_ERR,
			       "%s/%s: unsupported address family for rpc",
                               sep->se_service, sep->se_proto);
                        (void) close(sep->se_fd);
                        sep->se_fd = -1;
                        return;
		}
                if (getsockname(sep->se_fd,
				(struct sockaddr*)&sep->se_ctrladdr, &len) < 0){
                        syslog(LOG_ERR, "%s/%s: getsockname: %m",
                               sep->se_service, sep->se_proto);
                        (void) close(sep->se_fd);
                        sep->se_fd = -1;
                        return;
                }
                if (debug)
                        print_service("REG ", sep);
                for (i = sep->se_rpc_lowvers; i <= sep->se_rpc_highvers; i++) {
                        pmap_unset(sep->se_rpc_prog, i);
                        pmap_set(sep->se_rpc_prog, i,
                                 (sep->se_socktype == SOCK_DGRAM)
                                 ? IPPROTO_UDP : IPPROTO_TCP,
				 ntohs(sep->se_ctrladdr4.sin_port));
                }
        }
	if (sep->se_socktype == SOCK_STREAM)
		listen(sep->se_fd, 64);
	enable(sep);
	if (debug) {
		warnx("registered %s on %d",
			sep->se_server, sep->se_fd);
	}
}

#ifdef IPSEC
void
ipsecsetup(sep)
	struct servtab *sep;
{
	char *buf;
	char *policy_in = NULL;
	char *policy_out = NULL;
	int level;
	int opt;

	switch (sep->se_family) {
	case AF_INET:
		level = IPPROTO_IP;
		opt = IP_IPSEC_POLICY;
		break;
#ifdef INET6
	case AF_INET6:
		level = IPPROTO_IPV6;
		opt = IPV6_IPSEC_POLICY;
		break;
#endif
	default:
		return;
	}

	if (!sep->se_policy || sep->se_policy[0] == '\0') {
		policy_in = "in entrust";
		policy_out = "out entrust";
	} else {
		if (!strncmp("in", sep->se_policy, 2))
			policy_in = sep->se_policy;
		else if (!strncmp("out", sep->se_policy, 3))
			policy_out = sep->se_policy;
		else {
			syslog(LOG_ERR, "invalid security policy \"%s\"",
				sep->se_policy);
			return;
		}
	}

	if (policy_in != NULL) {
		buf = ipsec_set_policy(policy_in, strlen(policy_in));
		if (buf != NULL) {
			if (setsockopt(sep->se_fd, level, opt,
					buf, ipsec_get_policylen(buf)) < 0 &&
			    debug != 0)
				warnx("%s/%s: ipsec initialization failed; %s",
				      sep->se_service, sep->se_proto,
				      policy_in);
			free(buf);
		} else
			syslog(LOG_ERR, "invalid security policy \"%s\"",
				policy_in);
	}
	if (policy_out != NULL) {
		buf = ipsec_set_policy(policy_out, strlen(policy_out));
		if (buf != NULL) {
			if (setsockopt(sep->se_fd, level, opt,
					buf, ipsec_get_policylen(buf)) < 0 &&
			    debug != 0)
				warnx("%s/%s: ipsec initialization failed; %s",
				      sep->se_service, sep->se_proto,
				      policy_out);
			free(buf);
		} else
			syslog(LOG_ERR, "invalid security policy \"%s\"",
				policy_out);
	}
}
#endif

/*
 * Finish with a service and its socket.
 */
void
close_sep(sep)
	struct servtab *sep;
{
	if (sep->se_fd >= 0) {
		if (FD_ISSET(sep->se_fd, &allsock))
			disable(sep);
		(void) close(sep->se_fd);
		sep->se_fd = -1;
	}
	sep->se_count = 0;
	sep->se_numchild = 0;	/* forget about any existing children */
}

int
matchservent(name1, name2, proto)
	char *name1, *name2, *proto;
{
	char **alias;
	struct servent *se;

	if (strcmp(name1, name2) == 0)
		return(1);
	if ((se = getservbyname(name1, proto)) != NULL) {
		if (strcmp(name2, se->s_name) == 0)
			return(1);
		for (alias = se->s_aliases; *alias; alias++)
			if (strcmp(name2, *alias) == 0)
				return(1);
	}
	return(0);
}

struct servtab *
enter(cp)
	struct servtab *cp;
{
	struct servtab *sep;
	long omask;

	sep = (struct servtab *)malloc(sizeof (*sep));
	if (sep == (struct servtab *)0) {
		syslog(LOG_ERR, "malloc: %m");
		exit(EX_OSERR);
	}
	*sep = *cp;
	sep->se_fd = -1;
	omask = sigblock(SIGBLOCK);
	sep->se_next = servtab;
	servtab = sep;
	sigsetmask(omask);
	return (sep);
}

void
enable(struct servtab *sep)
{
	if (debug)
		warnx(
		    "enabling %s, fd %d", sep->se_service, sep->se_fd);
#ifdef SANITY_CHECK
	if (sep->se_fd < 0) {
		syslog(LOG_ERR,
		    "%s: %s: bad fd", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	if (ISMUX(sep)) {
		syslog(LOG_ERR,
		    "%s: %s: is mux", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	if (FD_ISSET(sep->se_fd, &allsock)) {
		syslog(LOG_ERR,
		    "%s: %s: not off", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	nsock++;
#endif
	FD_SET(sep->se_fd, &allsock);
	if (sep->se_fd > maxsock)
		maxsock = sep->se_fd;
}

void
disable(struct servtab *sep)
{
	if (debug)
		warnx(
		    "disabling %s, fd %d", sep->se_service, sep->se_fd);
#ifdef SANITY_CHECK
	if (sep->se_fd < 0) {
		syslog(LOG_ERR,
		    "%s: %s: bad fd", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	if (ISMUX(sep)) {
		syslog(LOG_ERR,
		    "%s: %s: is mux", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	if (!FD_ISSET(sep->se_fd, &allsock)) {
		syslog(LOG_ERR,
		    "%s: %s: not on", __FUNCTION__, sep->se_service);
		exit(EX_SOFTWARE);
	}
	if (nsock == 0) {
		syslog(LOG_ERR, "%s: nsock=0", __FUNCTION__);
		exit(EX_SOFTWARE);
	}
	nsock--;
#endif
	FD_CLR(sep->se_fd, &allsock);
	if (sep->se_fd == maxsock)
		maxsock--;
}

FILE	*fconfig = NULL;
struct	servtab serv;
char	line[LINE_MAX];

int
setconfig()
{

	if (fconfig != NULL) {
		fseek(fconfig, 0L, SEEK_SET);
		return (1);
	}
	fconfig = fopen(CONFIG, "r");
	return (fconfig != NULL);
}

void
endconfig()
{
	if (fconfig) {
		(void) fclose(fconfig);
		fconfig = NULL;
	}
}

struct servtab *
getconfigent()
{
	struct servtab *sep = &serv;
	int argc;
	char *cp, *arg, *s;
	char *versp;
	static char TCPMUX_TOKEN[] = "tcpmux/";
#define MUX_LEN		(sizeof(TCPMUX_TOKEN)-1)
#ifdef IPSEC
	char *policy = NULL;
#endif
	int v4bind = 0;
#ifdef INET6
	int v6bind = 0;
#endif

more:
	while ((cp = nextline(fconfig)) != NULL) {
#ifdef IPSEC
		/* lines starting with #@ is not a comment, but the policy */
		if (cp[0] == '#' && cp[1] == '@') {
			char *p;
			for (p = cp + 2; p && *p && isspace(*p); p++)
				;
			if (*p == '\0') {
				if (policy)
					free(policy);
				policy = NULL;
			} else if (ipsec_get_policylen(p) >= 0) {
				if (policy)
					free(policy);
				policy = newstr(p);
			} else {
				syslog(LOG_ERR,
					"%s: invalid ipsec policy \"%s\"",
					CONFIG, p);
				exit(EX_CONFIG);
			}
		}
#endif
		if (*cp == '#' || *cp == '\0')
			continue;
		break;
	}
	if (cp == NULL)
		return ((struct servtab *)0);
	/*
	 * clear the static buffer, since some fields (se_ctrladdr,
	 * for example) don't get initialized here.
	 */
	memset((caddr_t)sep, 0, sizeof *sep);
	arg = skip(&cp);
	if (cp == NULL) {
		/* got an empty line containing just blanks/tabs. */
		goto more;
	}
	if (strncmp(arg, TCPMUX_TOKEN, MUX_LEN) == 0) {
		char *c = arg + MUX_LEN;
		if (*c == '+') {
			sep->se_type = MUXPLUS_TYPE;
			c++;
		} else
			sep->se_type = MUX_TYPE;
		sep->se_service = newstr(c);
	} else {
		sep->se_service = newstr(arg);
		sep->se_type = NORM_TYPE;
	}
	arg = sskip(&cp);
	if (strcmp(arg, "stream") == 0)
		sep->se_socktype = SOCK_STREAM;
	else if (strcmp(arg, "dgram") == 0)
		sep->se_socktype = SOCK_DGRAM;
	else if (strcmp(arg, "rdm") == 0)
		sep->se_socktype = SOCK_RDM;
	else if (strcmp(arg, "seqpacket") == 0)
		sep->se_socktype = SOCK_SEQPACKET;
	else if (strcmp(arg, "raw") == 0)
		sep->se_socktype = SOCK_RAW;
	else
		sep->se_socktype = -1;

	arg = sskip(&cp);
	if (strncmp(arg, "tcp", 3) == 0) {
		sep->se_proto = newstr(strsep(&arg, "/"));
		if (arg != NULL) {
			if (strcmp(arg, "ttcp") == 0)
				sep->se_type = TTCP_TYPE;
			else if (strcmp(arg, "faith") == 0)
				sep->se_type = FAITH_TYPE;
		}
	} else
		sep->se_proto = newstr(arg);
        if (strncmp(sep->se_proto, "rpc/", 4) == 0) {
		if (no_v4bind != 0) {
			syslog(LOG_INFO, "IPv4 bind is ignored for %s",
			       sep->se_service);
			freeconfig(sep);
			goto more;
		}
                memmove(sep->se_proto, sep->se_proto + 4,
                    strlen(sep->se_proto) + 1 - 4);
                sep->se_rpc = 1;
                sep->se_rpc_prog = sep->se_rpc_lowvers =
			sep->se_rpc_lowvers = 0;
		memcpy(&sep->se_ctrladdr4, bind_sa4,
		       sizeof(sep->se_ctrladdr4));
                if ((versp = rindex(sep->se_service, '/'))) {
                        *versp++ = '\0';
                        switch (sscanf(versp, "%d-%d",
                                       &sep->se_rpc_lowvers,
                                       &sep->se_rpc_highvers)) {
                        case 2:
                                break;
                        case 1:
                                sep->se_rpc_highvers =
                                        sep->se_rpc_lowvers;
                                break;
                        default:
                                syslog(LOG_ERR,
					"bad RPC version specifier; %s",
					sep->se_service);
                                freeconfig(sep);
                                goto more;
                        }
                }
                else {
                        sep->se_rpc_lowvers =
                                sep->se_rpc_highvers = 1;
                }
        }
	sep->se_nomapped = 0;
	while (isdigit(sep->se_proto[strlen(sep->se_proto) - 1])) {
#ifdef INET6
		if (sep->se_proto[strlen(sep->se_proto) - 1] == '6') {
			if (no_v6bind != 0) {
				syslog(LOG_INFO, "IPv6 bind is ignored for %s",
				       sep->se_service);
				freeconfig(sep);
				goto more;
			}
			sep->se_proto[strlen(sep->se_proto) - 1] = '\0';
			v6bind = 1;
			continue;
		}
#endif
		if (sep->se_proto[strlen(sep->se_proto) - 1] == '4') {
			sep->se_proto[strlen(sep->se_proto) - 1] = '\0';
			v4bind = 1;
			continue;
		}
		/* illegal version num */
		syslog(LOG_ERR,	"bad IP version for %s", sep->se_proto);
		freeconfig(sep);
		goto more;
	}
#ifdef INET6
	if (v6bind != 0) {
		sep->se_family = AF_INET6;
		if (v4bind == 0 || no_v4bind != 0)
			sep->se_nomapped = 1;
	} else
#endif
	{ /* default to v4 bind if not v6 bind */
		if (no_v4bind != 0) {
			syslog(LOG_INFO, "IPv4 bind is ignored for %s",
			       sep->se_service);
			freeconfig(sep);
			goto more;
		}
		sep->se_family = AF_INET;
	}
	/* init ctladdr */
	switch(sep->se_family) {
	case AF_INET:
		memcpy(&sep->se_ctrladdr4, bind_sa4,
		       sizeof(sep->se_ctrladdr4));
		sep->se_ctrladdr_size =	sizeof(sep->se_ctrladdr4);
		sep->se_ctladdrinitok = 1;
		break;
#ifdef INET6
	case AF_INET6:
		memcpy(&sep->se_ctrladdr6, bind_sa6,
		       sizeof(sep->se_ctrladdr6));
		sep->se_ctrladdr_size =	sizeof(sep->se_ctrladdr6);
		sep->se_ctladdrinitok = 1;
		break;
#endif
	}
	arg = sskip(&cp);
	if (!strncmp(arg, "wait", 4))
		sep->se_accept = 0;
	else if (!strncmp(arg, "nowait", 6))
		sep->se_accept = 1;
	else {
		syslog(LOG_ERR,
			"%s: bad wait/nowait for service %s",
			CONFIG, sep->se_service);
		goto more;
	}
	sep->se_maxchild = -1;
	sep->se_maxcpm = -1;
	if ((s = strchr(arg, '/')) != NULL) {
		char *eptr;
		u_long val;

		val = strtoul(s + 1, &eptr, 10);
		if (eptr == s + 1 || val > MAX_MAXCHLD) {
			syslog(LOG_ERR,
				"%s: bad max-child for service %s",
				CONFIG, sep->se_service);
			goto more;
		}
		if (debug)
			if (!sep->se_accept && val != 1)
				warnx("maxchild=%lu for wait service %s"
				    " not recommended", val, sep->se_service);
		sep->se_maxchild = val;
		if (*eptr == '/')
			sep->se_maxcpm = strtol(eptr + 1, &eptr, 10);
		/*
		 * explicitly do not check for \0 for future expansion /
		 * backwards compatibility
		 */
	}
	if (ISMUX(sep)) {
		/*
		 * Silently enforce "nowait" mode for TCPMUX services
		 * since they don't have an assigned port to listen on.
		 */
		sep->se_accept = 1;
		if (strcmp(sep->se_proto, "tcp")) {
			syslog(LOG_ERR,
				"%s: bad protocol for tcpmux service %s",
				CONFIG, sep->se_service);
			goto more;
		}
		if (sep->se_socktype != SOCK_STREAM) {
			syslog(LOG_ERR,
				"%s: bad socket type for tcpmux service %s",
				CONFIG, sep->se_service);
			goto more;
		}
	}
	sep->se_user = newstr(sskip(&cp));
#ifdef LOGIN_CAP
	if ((s = strrchr(sep->se_user, '/')) != NULL) {
		*s = '\0';
		sep->se_class = newstr(s + 1);
	} else
		sep->se_class = newstr(RESOURCE_RC);
#endif
	if ((s = strrchr(sep->se_user, ':')) != NULL) {
		*s = '\0';
		sep->se_group = newstr(s + 1);
	} else
		sep->se_group = NULL;
	sep->se_server = newstr(sskip(&cp));
	if ((sep->se_server_name = rindex(sep->se_server, '/')))
		sep->se_server_name++;
	if (strcmp(sep->se_server, "internal") == 0) {
		struct biltin *bi;

		for (bi = biltins; bi->bi_service; bi++)
			if (bi->bi_socktype == sep->se_socktype &&
			    matchservent(bi->bi_service, sep->se_service,
			    sep->se_proto))
				break;
		if (bi->bi_service == 0) {
			syslog(LOG_ERR, "internal service %s unknown",
				sep->se_service);
			goto more;
		}
		sep->se_accept = 1;	/* force accept mode for built-ins */
		sep->se_bi = bi;
	} else
		sep->se_bi = NULL;
	if (sep->se_maxcpm < 0)
		sep->se_maxcpm = maxcpm;
	if (sep->se_maxchild < 0) {	/* apply default max-children */
		if (sep->se_bi && sep->se_bi->bi_maxchild >= 0)
			sep->se_maxchild = sep->se_bi->bi_maxchild;
		else if (sep->se_accept) 
			sep->se_maxchild = maxchild > 0 ? maxchild : 0;
		else
			sep->se_maxchild = 1;
	}
	if (sep->se_maxchild) {
		sep->se_pids = malloc(sep->se_maxchild * sizeof(*sep->se_pids));
		if (sep->se_pids == NULL) {
			syslog(LOG_ERR, "malloc: %m");
			exit(EX_OSERR);
		}
	}
	argc = 0;
	for (arg = skip(&cp); cp; arg = skip(&cp))
		if (argc < MAXARGV) {
			sep->se_argv[argc++] = newstr(arg);
		} else {
			syslog(LOG_ERR,
				"%s: too many arguments for service %s",
				CONFIG, sep->se_service);
			goto more;
		}
	while (argc <= MAXARGV)
		sep->se_argv[argc++] = NULL;
#ifdef IPSEC
	sep->se_policy = policy ? newstr(policy) : NULL;
#endif
	return (sep);
}

void
freeconfig(cp)
	struct servtab *cp;
{
	int i;

	if (cp->se_service)
		free(cp->se_service);
	if (cp->se_proto)
		free(cp->se_proto);
	if (cp->se_user)
		free(cp->se_user);
	if (cp->se_group)
		free(cp->se_group);
#ifdef LOGIN_CAP
	if (cp->se_class)
		free(cp->se_class);
#endif
	if (cp->se_server)
		free(cp->se_server);
	if (cp->se_pids)
		free(cp->se_pids);
	for (i = 0; i < MAXARGV; i++)
		if (cp->se_argv[i])
			free(cp->se_argv[i]);
#ifdef IPSEC
	if (cp->se_policy)
		free(cp->se_policy);
#endif
}


/*
 * Safe skip - if skip returns null, log a syntax error in the
 * configuration file and exit.
 */
char *
sskip(cpp)
	char **cpp;
{
	char *cp;

	cp = skip(cpp);
	if (cp == NULL) {
		syslog(LOG_ERR, "%s: syntax error", CONFIG);
		exit(EX_DATAERR);
	}
	return (cp);
}

char *
skip(cpp)
	char **cpp;
{
	char *cp = *cpp;
	char *start;
	char quote = '\0';

again:
	while (*cp == ' ' || *cp == '\t')
		cp++;
	if (*cp == '\0') {
		int c;

		c = getc(fconfig);
		(void) ungetc(c, fconfig);
		if (c == ' ' || c == '\t')
			if ((cp = nextline(fconfig)))
				goto again;
		*cpp = (char *)0;
		return ((char *)0);
	}
	if (*cp == '"' || *cp == '\'')
		quote = *cp++;
	start = cp;
	if (quote)
		while (*cp && *cp != quote)
			cp++;
	else
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
	if (*cp != '\0')
		*cp++ = '\0';
	*cpp = cp;
	return (start);
}

char *
nextline(fd)
	FILE *fd;
{
	char *cp;

	if (fgets(line, sizeof (line), fd) == NULL)
		return ((char *)0);
	cp = strchr(line, '\n');
	if (cp)
		*cp = '\0';
	return (line);
}

char *
newstr(cp)
	char *cp;
{
	if ((cp = strdup(cp ? cp : "")))
		return (cp);
	syslog(LOG_ERR, "strdup: %m");
	exit(EX_OSERR);
}

#ifdef OLD_SETPROCTITLE
void
inetd_setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	char *cp;
	struct sockaddr_storage ss;
	char buf[80], pbuf[INET6_ADDRSTRLEN];

	cp = Argv[0];
	size = sizeof(ss);
	if (getpeername(s, (struct sockaddr *)&ss, &size) == 0) {
		getnameinfo((struct sockaddr *)&ss, size, pbuf, sizeof(pbuf),
			    NULL, 0, NI_NUMERICHOST|NI_WITHSCOPEID);
		(void) sprintf(buf, "-%s [%s]", a, pbuf);
	} else
		(void) sprintf(buf, "-%s", a);
	strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}
#else
void
inetd_setproctitle(a, s)
	char *a;
	int s;
{
	int size;
	struct sockaddr_storage ss;
	char buf[80], pbuf[INET6_ADDRSTRLEN];

	size = sizeof(ss);
	if (getpeername(s, (struct sockaddr *)&ss, &size) == 0) {
		getnameinfo((struct sockaddr *)&ss, size, pbuf, sizeof(pbuf),
			    NULL, 0, NI_NUMERICHOST|NI_WITHSCOPEID);
		(void) sprintf(buf, "%s [%s]", a, pbuf);
	} else
		(void) sprintf(buf, "%s", a);
	setproctitle("%s", buf);
}
#endif


/*
 * Internet services provided internally by inetd:
 */

int check_loop(sa, sep)
	struct sockaddr *sa;
	struct servtab *sep;
{
	struct servtab *se2;
	char pname[INET6_ADDRSTRLEN];

	for (se2 = servtab; se2; se2 = se2->se_next) {
		if (!se2->se_bi || se2->se_socktype != SOCK_DGRAM)
			continue;

		switch (se2->se_family) {
		case AF_INET:
			if (((struct sockaddr_in *)sa)->sin_port ==
			    se2->se_ctrladdr4.sin_port)
				goto isloop;
			continue;
#ifdef INET6
		case AF_INET6:
			if (((struct sockaddr_in *)sa)->sin_port ==
			    se2->se_ctrladdr4.sin_port)
				goto isloop;
			continue;
#endif
		default:
			continue;
		}
	isloop:
		getnameinfo(sa, sa->sa_len, pname, sizeof(pname), NULL, 0,
			    NI_NUMERICHOST|NI_WITHSCOPEID);
		syslog(LOG_WARNING, "%s/%s:%s/%s loop request REFUSED from %s",
		       sep->se_service, sep->se_proto,
		       se2->se_service, se2->se_proto,
		       pname);
		return 1;
	}
	return 0;
}

/*
 * print_service:
 *	Dump relevant information to stderr
 */
void
print_service(action, sep)
	char *action;
	struct servtab *sep;
{
	fprintf(stderr,
	    "%s: %s proto=%s accept=%d max=%d user=%s group=%s"
#ifdef LOGIN_CAP
	    "class=%s"
#endif
	    " builtin=%p server=%s"
#ifdef IPSEC
	    " policy=\"%s\""
#endif
	    "\n",
	    action, sep->se_service, sep->se_proto,
	    sep->se_accept, sep->se_maxchild, sep->se_user, sep->se_group,
#ifdef LOGIN_CAP
	    sep->se_class,
#endif
	    (void *) sep->se_bi, sep->se_server
#ifdef IPSEC
	    , (sep->se_policy ? sep->se_policy : "")
#endif
	    );
}

#define CPMHSIZE	256
#define CPMHMASK	(CPMHSIZE-1)
#define CHTGRAN		10
#define CHTSIZE		6

typedef struct CTime {
	unsigned long 	ct_Ticks;
	int		ct_Count;
} CTime;

typedef struct CHash {
	union {
		struct in_addr	c4_Addr;
		struct in6_addr	c6_Addr;
	} cu_Addr;
#define	ch_Addr4	cu_Addr.c4_Addr
#define	ch_Addr6	cu_Addr.c6_Addr
	int		ch_Family;
	time_t		ch_LTime;
	char		*ch_Service;
	CTime		ch_Times[CHTSIZE];
} CHash;

CHash	CHashAry[CPMHSIZE];

int
cpmip(sep, ctrl)
	struct servtab *sep;
	int ctrl;
{
	struct sockaddr_storage rss;
	int rssLen = sizeof(rss);
	int r = 0;

	/*
	 * If getpeername() fails, just let it through (if logging is
	 * enabled the condition is caught elsewhere)
	 */

	if (sep->se_maxcpm > 0 && 
	    getpeername(ctrl, (struct sockaddr *)&rss, &rssLen) == 0 ) {
		time_t t = time(NULL);
		int hv = 0xABC3D20F;
		int i;
		int cnt = 0;
		CHash *chBest = NULL;
		unsigned int ticks = t / CHTGRAN;
		struct sockaddr_in *sin;
#ifdef INET6
		struct sockaddr_in6 *sin6;
#endif

		sin = (struct sockaddr_in *)&rss;
#ifdef INET6
		sin6 = (struct sockaddr_in6 *)&rss;
#endif
		{
			char *p;
			int i, addrlen;

			switch (rss.ss_family) {
			case AF_INET:
				p = (char *)&sin->sin_addr;
				addrlen = sizeof(struct in_addr);
				break;
#ifdef INET6
			case AF_INET6:
				p = (char *)&sin6->sin6_addr;
				addrlen = sizeof(struct in6_addr);
				break;
#endif
			default:
				/* should not happen */
				return -1;
			}

			for (i = 0; i < addrlen; ++i, ++p) {
				hv = (hv << 5) ^ (hv >> 23) ^ *p;
			}
			hv = (hv ^ (hv >> 16));
		}
		for (i = 0; i < 5; ++i) {
			CHash *ch = &CHashAry[(hv + i) & CPMHMASK];

			if (rss.ss_family == AF_INET &&
			    ch->ch_Family == AF_INET &&
			    sin->sin_addr.s_addr == ch->ch_Addr4.s_addr &&
			    ch->ch_Service && strcmp(sep->se_service,
			    ch->ch_Service) == 0) {
				chBest = ch;
				break;
			}
#ifdef INET6
			if (rss.ss_family == AF_INET6 &&
			    ch->ch_Family == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					       &ch->ch_Addr6) != 0 &&
			    ch->ch_Service && strcmp(sep->se_service,
			    ch->ch_Service) == 0) {
				chBest = ch;
				break;
			}
#endif
			if (chBest == NULL || ch->ch_LTime == 0 || 
			    ch->ch_LTime < chBest->ch_LTime) {
				chBest = ch;
			}
		}
		if ((rss.ss_family == AF_INET &&
		     (chBest->ch_Family != AF_INET ||
		      sin->sin_addr.s_addr != chBest->ch_Addr4.s_addr)) ||
		    chBest->ch_Service == NULL ||
		    strcmp(sep->se_service, chBest->ch_Service) != 0) {
			chBest->ch_Family = sin->sin_family;
			chBest->ch_Addr4 = sin->sin_addr;
			if (chBest->ch_Service)
				free(chBest->ch_Service);
			chBest->ch_Service = strdup(sep->se_service);
			bzero(chBest->ch_Times, sizeof(chBest->ch_Times));
		} 
#ifdef INET6
		if ((rss.ss_family == AF_INET6 &&
		     (chBest->ch_Family != AF_INET6 ||
		      IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
					 &chBest->ch_Addr6) == 0)) ||
		    chBest->ch_Service == NULL ||
		    strcmp(sep->se_service, chBest->ch_Service) != 0) {
			chBest->ch_Family = sin6->sin6_family;
			chBest->ch_Addr6 = sin6->sin6_addr;
			if (chBest->ch_Service)
				free(chBest->ch_Service);
			chBest->ch_Service = strdup(sep->se_service);
			bzero(chBest->ch_Times, sizeof(chBest->ch_Times));
		}
#endif
		chBest->ch_LTime = t;
		{
			CTime *ct = &chBest->ch_Times[ticks % CHTSIZE];
			if (ct->ct_Ticks != ticks) {
				ct->ct_Ticks = ticks;
				ct->ct_Count = 0;
			}
			++ct->ct_Count;
		}
		for (i = 0; i < CHTSIZE; ++i) {
			CTime *ct = &chBest->ch_Times[i];
			if (ct->ct_Ticks <= ticks &&
			    ct->ct_Ticks >= ticks - CHTSIZE) {
				cnt += ct->ct_Count;
			}
		}
		if (cnt * (CHTSIZE * CHTGRAN) / 60 > sep->se_maxcpm) {
			char pname[INET6_ADDRSTRLEN];

			getnameinfo((struct sockaddr *)&rss,
				    ((struct sockaddr *)&rss)->sa_len,
				    pname, sizeof(pname), NULL, 0,
				    NI_NUMERICHOST|NI_WITHSCOPEID);
			r = -1;
			syslog(LOG_ERR,
			    "%s from %s exceeded counts/min (limit %d/min)",
			    sep->se_service, pname,
			    sep->se_maxcpm);
		}
	}
	return(r);
}
