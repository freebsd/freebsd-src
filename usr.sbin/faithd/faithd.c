/*	$KAME: faithd.c,v 1.20 2000/07/01 11:40:45 itojun Exp $	*/

/*
 * Copyright (C) 1997 and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * User level translator from IPv6 to IPv4.
 *
 * Usage: faithd [<port> <progpath> <arg1(progname)> <arg2> ...]
 *   e.g. faithd telnet /usr/local/v6/sbin/telnetd telnetd
 */
#define HAVE_GETIFADDRS

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <termios.h>

#include <net/if_types.h>
#ifdef IFT_FAITH
# define USE_ROUTE
# include <net/if.h>
# include <net/route.h>
# include <net/if_dl.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
#endif

#ifdef FAITH4
#include <resolv.h>
#include <arpa/nameser.h>
#ifndef FAITH_NS
#define FAITH_NS "FAITH_NS"
#endif
#endif

#include "faithd.h"

char *serverpath = NULL;
char *serverarg[MAXARGV + 1];
static char *faithdname = NULL;
char logname[BUFSIZ];
char procname[BUFSIZ];
struct myaddrs {
	struct myaddrs *next;
	struct sockaddr *addr;
};
struct myaddrs *myaddrs = NULL;
static char *service;
#ifdef USE_ROUTE
static int sockfd = 0;
#endif
int dflag = 0;
static int pflag = 0;
static int inetd = 0;

int main __P((int, char **));
static int inetd_main __P((int, char **));
static int daemon_main __P((int, char **));
static void play_service __P((int));
static void play_child __P((int, struct sockaddr *));
static int faith_prefix __P((struct sockaddr *));
static int map6to4 __P((struct sockaddr_in6 *, struct sockaddr_in *));
#ifdef FAITH4
static int map4to6 __P((struct sockaddr_in *, struct sockaddr_in6 *));
#endif
static void sig_child __P((int));
static void sig_terminate __P((int));
static void start_daemon __P((void));
#ifndef HAVE_GETIFADDRS
static unsigned int if_maxindex __P((void));
#endif
static void grab_myaddrs __P((void));
static void free_myaddrs __P((void));
static void update_myaddrs __P((void));
static void usage __P((void));

int
main(int argc, char **argv)
{

	/*
	 * Initializing stuff
	 */

	faithdname = strrchr(argv[0], '/');
	if (faithdname)
		faithdname++;
	else
		faithdname = argv[0];

	if (strcmp(faithdname, "faithd") != 0) {
		inetd = 1;
		return inetd_main(argc, argv);
	} else
		return daemon_main(argc, argv);
}

static int
inetd_main(int argc, char **argv)
{
	char path[MAXPATHLEN];
	struct sockaddr_storage me;
	struct sockaddr_storage from;
	int melen, fromlen;
	int i;
	int error;
	const int on = 1;
	char sbuf[NI_MAXSERV], snum[NI_MAXSERV];

	if (strrchr(argv[0], '/') == NULL)
		snprintf(path, sizeof(path), "%s/%s", DEFAULT_DIR, argv[0]);
	else
		snprintf(path, sizeof(path), "%s", argv[0]);

#ifdef USE_ROUTE
	grab_myaddrs();

	sockfd = socket(PF_ROUTE, SOCK_RAW, PF_UNSPEC);
	if (sockfd < 0) {
		exit_error("socket(PF_ROUTE): %s", ERRSTR);
		/*NOTREACHED*/
	}
#endif

	melen = sizeof(me);
	if (getsockname(STDIN_FILENO, (struct sockaddr *)&me, &melen) < 0)
		exit_error("getsockname");
	fromlen = sizeof(from);
	if (getpeername(STDIN_FILENO, (struct sockaddr *)&from, &fromlen) < 0)
		exit_error("getpeername");
	if (getnameinfo((struct sockaddr *)&me, melen, NULL, 0,
	    sbuf, sizeof(sbuf), NI_NUMERICHOST) == 0)
		service = sbuf;
	else
		service = DEFAULT_PORT_NAME;
	if (getnameinfo((struct sockaddr *)&me, melen, NULL, 0,
	    snum, sizeof(snum), NI_NUMERICHOST) != 0)
		snprintf(snum, sizeof(snum), "?");

	snprintf(logname, sizeof(logname), "faithd %s", snum);
	snprintf(procname, sizeof(procname), "accepting port %s", snum);
	openlog(logname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);

	if (argc >= MAXARGV)
		exit_failure("too many arguments");
	serverarg[0] = serverpath = path;
	for (i = 1; i < argc; i++)
		serverarg[i] = argv[i];
	serverarg[i] = NULL;

	error = setsockopt(STDIN_FILENO, SOL_SOCKET, SO_OOBINLINE, &on,
	    sizeof(on));
	if (error < 0)
		exit_error("setsockopt(SO_OOBINLINE): %s", ERRSTR);

	play_child(STDIN_FILENO, (struct sockaddr *)&from);
	exit_failure("should not reach here");
	return 0;	/*dummy!*/
}

static int
daemon_main(int argc, char **argv)
{
	struct addrinfo hints, *res;
	int s_wld, error, i, serverargc, on = 1;
	int family = AF_INET6;
	int c;
#ifdef FAITH_NS
	char *ns;
#endif /* FAITH_NS */

	while ((c = getopt(argc, argv, "dp46")) != -1) {
		switch (c) {
		case 'd':
			dflag++;
			break;
		case 'p':
			pflag++;
			break;
#ifdef FAITH4
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
#endif
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

#ifdef FAITH_NS
	if ((ns = getenv(FAITH_NS)) != NULL) {
		struct sockaddr_storage ss;
		struct addrinfo hints, *res;
		char serv[NI_MAXSERV];

		memset(&ss, 0, sizeof(ss));
		memset(&hints, 0, sizeof(hints));
		snprintf(serv, sizeof(serv), "%u", NAMESERVER_PORT);
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(ns, serv, &hints, &res) ==  0) {
			res_init();
			memcpy(&_res_ext.nsaddr, res->ai_addr, res->ai_addrlen);
			_res.nscount = 1;
		}
	}
#endif /* FAITH_NS */

#ifdef USE_ROUTE
	grab_myaddrs();
#endif

	switch (argc) {
	case 0:
		serverpath = DEFAULT_PATH;
		serverarg[0] = DEFAULT_NAME;
		serverarg[1] = NULL;
		service = DEFAULT_PORT_NAME;
		break;
	default:
		serverargc = argc - NUMARG;
		if (serverargc >= MAXARGV)
			exit_error("too many augments");

		serverpath = malloc(strlen(argv[NUMPRG]) + 1);
		strcpy(serverpath, argv[NUMPRG]);
		for (i = 0; i < serverargc; i++) {
			serverarg[i] = malloc(strlen(argv[i + NUMARG]) + 1);
			strcpy(serverarg[i], argv[i + NUMARG]);
		}
		serverarg[i] = NULL;
		/* fall throuth */
	case 1:	/* no local service */
		service = argv[NUMPRT];
		break;
	}

	/*
	 * Opening wild card socket for this service.
	 */

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	error = getaddrinfo(NULL, service, &hints, &res);
	if (error)
		exit_error("getaddrinfo: %s", gai_strerror(error));

	s_wld = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (s_wld == -1)
		exit_error("socket: %s", ERRSTR);

#ifdef IPV6_FAITH
	if (res->ai_family == AF_INET6) {
		error = setsockopt(s_wld, IPPROTO_IPV6, IPV6_FAITH, &on, sizeof(on));
		if (error == -1)
			exit_error("setsockopt(IPV6_FAITH): %s", ERRSTR);
	}
#endif
#ifdef FAITH4
#ifdef IP_FAITH
	if (res->ai_family == AF_INET) {
		error = setsockopt(s_wld, IPPROTO_IP, IP_FAITH, &on, sizeof(on));
		if (error == -1)
			exit_error("setsockopt(IP_FAITH): %s", ERRSTR);
	}
#endif
#endif /* FAITH4 */

	error = setsockopt(s_wld, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (error == -1)
		exit_error("setsockopt(SO_REUSEADDR): %s", ERRSTR);
	
	error = setsockopt(s_wld, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on));
	if (error == -1)
		exit_error("setsockopt(SO_OOBINLINE): %s", ERRSTR);

	error = bind(s_wld, (struct sockaddr *)res->ai_addr, res->ai_addrlen);
	if (error == -1)
		exit_error("bind: %s", ERRSTR);

	error = listen(s_wld, 5);
	if (error == -1)
		exit_error("listen: %s", ERRSTR);

#ifdef USE_ROUTE
	sockfd = socket(PF_ROUTE, SOCK_RAW, PF_UNSPEC);
	if (sockfd < 0) {
		exit_error("socket(PF_ROUTE): %s", ERRSTR);
		/*NOTREACHED*/
	}
#endif

	/*
	 * Everything is OK.
	 */

	start_daemon();

	snprintf(logname, sizeof(logname), "faithd %s", service);
	snprintf(procname, sizeof(procname), "accepting port %s", service);
	openlog(logname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	syslog(LOG_INFO, "Staring faith daemon for %s port", service);

	play_service(s_wld);
	/*NOTREACHED*/
	exit(1);	/*pacify gcc*/
}

static void
play_service(int s_wld)
{
	struct sockaddr_storage srcaddr;
	int len;
	int s_src;
	pid_t child_pid;
	fd_set rfds;
	int error;
	int maxfd;

	/*
	 * Wait, accept, fork, faith....
	 */
again:
	setproctitle("%s", procname);

	FD_ZERO(&rfds);
	FD_SET(s_wld, &rfds);
	maxfd = s_wld;
#ifdef USE_ROUTE
	if (sockfd) {
		FD_SET(sockfd, &rfds);
		maxfd = (maxfd < sockfd) ? sockfd : maxfd;
	}
#endif

	error = select(maxfd + 1, &rfds, NULL, NULL, NULL);
	if (error < 0) {
		if (errno == EINTR)
			goto again;
		exit_failure("select: %s", ERRSTR);
		/*NOTREACHED*/
	}

#ifdef USE_ROUTE
	if (FD_ISSET(sockfd, &rfds)) {
		update_myaddrs();
	}
#endif
	if (FD_ISSET(s_wld, &rfds)) {
		len = sizeof(srcaddr);
		s_src = accept(s_wld, (struct sockaddr *)&srcaddr,
			&len);
		if (s_src == -1)
			exit_failure("socket: %s", ERRSTR);

		child_pid = fork();
		
		if (child_pid == 0) {
			/* child process */
			close(s_wld);
			closelog();
			openlog(logname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
			play_child(s_src, (struct sockaddr *)&srcaddr);
			exit_failure("should never reach here");
		} else {
			/* parent process */
			close(s_src);
			if (child_pid == -1)
				syslog(LOG_ERR, "can't fork");
		}
	}
	goto again;
}

static void
play_child(int s_src, struct sockaddr *srcaddr)
{
	struct sockaddr_storage dstaddr6;
	struct sockaddr_storage dstaddr4;
	char src[MAXHOSTNAMELEN];
	char dst6[MAXHOSTNAMELEN];
	char dst4[MAXHOSTNAMELEN];
	int len = sizeof(dstaddr6);
	int s_dst, error, hport, nresvport, on = 1;
	struct timeval tv;
	struct sockaddr *sa4;
	
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	getnameinfo(srcaddr, srcaddr->sa_len,
		src, sizeof(src), NULL, 0, NI_NUMERICHOST);
	syslog(LOG_INFO, "accepted a client from %s", src);

	error = getsockname(s_src, (struct sockaddr *)&dstaddr6, &len);
	if (error == -1)
		exit_failure("getsockname: %s", ERRSTR);

	getnameinfo((struct sockaddr *)&dstaddr6, len,
		dst6, sizeof(dst6), NULL, 0, NI_NUMERICHOST);
	syslog(LOG_INFO, "the client is connecting to %s", dst6);
	
	if (!faith_prefix((struct sockaddr *)&dstaddr6)) {
		if (serverpath) {
			/*
			 * Local service
			 */
			syslog(LOG_INFO, "executing local %s", serverpath);
			if (!inetd) {
				dup2(s_src, 0);
				close(s_src);
				dup2(0, 1);
				dup2(0, 2);
			}
			execv(serverpath, serverarg);
			syslog(LOG_ERR, "execv %s: %s", serverpath, ERRSTR);
			_exit(EXIT_FAILURE);
		} else {
			close(s_src);
			exit_success("no local service for %s", service);
		}
	}

	/*
	 * Act as a translator
	 */

	switch (((struct sockaddr *)&dstaddr6)->sa_family) {
	case AF_INET6:
		if (!map6to4((struct sockaddr_in6 *)&dstaddr6,
		    (struct sockaddr_in *)&dstaddr4)) {
			close(s_src);
			exit_error("map6to4 failed");
		}
		syslog(LOG_INFO, "translating from v6 to v4");
		break;
#ifdef FAITH4
	case AF_INET:
		if (!map4to6((struct sockaddr_in *)&dstaddr6,
		    (struct sockaddr_in6 *)&dstaddr4)) {
			close(s_src);
			exit_error("map4to6 failed");
		}
		syslog(LOG_INFO, "translating from v4 to v6");
		break;
#endif
	default:
		close(s_src);
		exit_error("family not supported");
		/*NOTREACHED*/
	}

	sa4 = (struct sockaddr *)&dstaddr4;
	getnameinfo(sa4, sa4->sa_len,
		dst4, sizeof(dst4), NULL, 0, NI_NUMERICHOST);
	syslog(LOG_INFO, "the translator is connecting to %s", dst4);

	setproctitle("port %s, %s -> %s", service, src, dst4);

	if (sa4->sa_family == AF_INET6)
		hport = ntohs(((struct sockaddr_in6 *)&dstaddr4)->sin6_port);
	else /* AF_INET */
		hport = ntohs(((struct sockaddr_in *)&dstaddr4)->sin_port);

	switch (hport) {
	case RLOGIN_PORT:
	case RSH_PORT:
		s_dst = rresvport_af(&nresvport, sa4->sa_family);
		break;
	default:
		if (pflag)
			s_dst = rresvport_af(&nresvport, sa4->sa_family);
		else
			s_dst = socket(sa4->sa_family, SOCK_STREAM, 0);
		break;
	}
	if (s_dst == -1)
		exit_failure("socket: %s", ERRSTR);

	error = setsockopt(s_dst, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on));
	if (error == -1)
		exit_error("setsockopt(SO_OOBINLINE): %s", ERRSTR);

	error = setsockopt(s_src, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (error == -1)
		exit_error("setsockopt(SO_SNDTIMEO): %s", ERRSTR);
	error = setsockopt(s_dst, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (error == -1)
		exit_error("setsockopt(SO_SNDTIMEO): %s", ERRSTR);

	error = connect(s_dst, sa4, sa4->sa_len);
	if (error == -1)
		exit_failure("connect: %s", ERRSTR);

	switch (hport) {
	case FTP_PORT:
		ftp_relay(s_src, s_dst);
		break;
	case RSH_PORT:
		rsh_relay(s_src, s_dst);
		break;
	default:
		tcp_relay(s_src, s_dst, service);
		break;
	}

	/* NOTREACHED */
}

/* 0: non faith, 1: faith */
static int
faith_prefix(struct sockaddr *dst)
{
#ifndef USE_ROUTE
	int mib[4], size;
	struct in6_addr faith_prefix;
	struct sockaddr_in6 *dst6 = (struct sockaddr_in *)dst;

	if (dst->sa_family != AF_INET6)
		return 0;

	mib[0] = CTL_NET;
	mib[1] = PF_INET6;
	mib[2] = IPPROTO_IPV6;
	mib[3] = IPV6CTL_FAITH_PREFIX;
	size = sizeof(struct in6_addr);
	if (sysctl(mib, 4, &faith_prefix, &size, NULL, 0) < 0)
		exit_error("sysctl: %s", ERRSTR);

	if (memcmp(dst, &faith_prefix,
			sizeof(struct in6_addr) - sizeof(struct in_addr) == 0) {
		return 1;
	}
	return 0;
#else
	struct myaddrs *p;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *dst6;
	struct sockaddr_in *dst4;
	struct sockaddr_in dstmap;

	dst6 = (struct sockaddr_in6 *)dst;
	if (dst->sa_family == AF_INET6
	 && IN6_IS_ADDR_V4MAPPED(&dst6->sin6_addr)) {
		/* ugly... */
		memset(&dstmap, 0, sizeof(dstmap));
		dstmap.sin_family = AF_INET;
		dstmap.sin_len = sizeof(dstmap);
		memcpy(&dstmap.sin_addr, &dst6->sin6_addr.s6_addr[12],
			sizeof(dstmap.sin_addr));
		dst = (struct sockaddr *)&dstmap;
	}

	dst6 = (struct sockaddr_in6 *)dst;
	dst4 = (struct sockaddr_in *)dst;

	for (p = myaddrs; p; p = p->next) {
		sin6 = (struct sockaddr_in6 *)p->addr;
		sin4 = (struct sockaddr_in *)p->addr;

		if (p->addr->sa_len != dst->sa_len
		 || p->addr->sa_family != dst->sa_family)
			continue;

		switch (dst->sa_family) {
		case AF_INET6:
			if (sin6->sin6_scope_id == dst6->sin6_scope_id
			 && IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr, &dst6->sin6_addr))
				return 0;
			break;
		case AF_INET:
			if (sin4->sin_addr.s_addr == dst4->sin_addr.s_addr)
				return 0;
			break;
		}
	}
	return 1;
#endif
}

/* 0: non faith, 1: faith */
static int
map6to4(struct sockaddr_in6 *dst6, struct sockaddr_in *dst4)
{
	memset(dst4, 0, sizeof(*dst4));
	dst4->sin_len = sizeof(*dst4);
	dst4->sin_family = AF_INET;
	dst4->sin_port = dst6->sin6_port;
	memcpy(&dst4->sin_addr, &dst6->sin6_addr.s6_addr[12],
		sizeof(dst4->sin_addr));

	if (dst4->sin_addr.s_addr == INADDR_ANY
	 || dst4->sin_addr.s_addr == INADDR_BROADCAST
	 || IN_MULTICAST(dst4->sin_addr.s_addr))
		return 0;

	return 1;
}

#ifdef FAITH4
/* 0: non faith, 1: faith */
static int
map4to6(struct sockaddr_in *dst4, struct sockaddr_in6 *dst6)
{
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
	struct addrinfo hints, *res;
	int ai_errno;

	if (getnameinfo((struct sockaddr *)dst4, dst4->sin_len, host, sizeof(host),
			serv, sizeof(serv), NI_NAMEREQD|NI_NUMERICSERV) != 0)
		return 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = 0;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	if ((ai_errno = getaddrinfo(host, serv, &hints, &res)) != 0) {
		syslog(LOG_INFO, "%s %s: %s", host, serv, gai_strerror(ai_errno));
		return 0;
	}

	memcpy(dst6, res->ai_addr, res->ai_addrlen);

	freeaddrinfo(res);

	return 1;
}
#endif /* FAITH4 */

static void
sig_child(int sig)
{
	int status;
	pid_t pid;

	pid = wait3(&status, WNOHANG, (struct rusage *)0);
	if (pid && status)
		syslog(LOG_WARNING, "child %d exit status 0x%x", pid, status);
}

void
sig_terminate(int sig)
{
	syslog(LOG_INFO, "Terminating faith daemon");	
	exit(EXIT_SUCCESS);
}

static void
start_daemon(void)
{
	if (daemon(0, 0) == -1)
		exit_error("daemon: %s", ERRSTR);

	if (signal(SIGCHLD, sig_child) == SIG_ERR)
		exit_failure("signal CHLD: %s", ERRSTR);

	if (signal(SIGTERM, sig_terminate) == SIG_ERR)
		exit_failure("signal TERM: %s", ERRSTR);
}

void
exit_error(const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	fprintf(stderr, "%s\n", buf);
	exit(EXIT_FAILURE);
}

void
exit_failure(const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(LOG_ERR, "%s", buf);
	exit(EXIT_FAILURE);
}

void
exit_success(const char *fmt, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	syslog(LOG_INFO, "%s", buf);
	exit(EXIT_SUCCESS);
}

#ifdef USE_ROUTE
#ifndef HAVE_GETIFADDRS
static unsigned int
if_maxindex()
{
	struct if_nameindex *p, *p0;
	unsigned int max = 0;

	p0 = if_nameindex();
	for (p = p0; p && p->if_index && p->if_name; p++) {
		if (max < p->if_index)
			max = p->if_index;
	}
	if_freenameindex(p0);
	return max;
}
#endif

static void
grab_myaddrs()
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifap, *ifa;
	struct myaddrs *p;
	struct sockaddr_in6 *sin6;

	if (getifaddrs(&ifap) != 0) {
		exit_failure("getifaddrs");
		/*NOTREACHED*/
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
		case AF_INET6:
			break;
		default:
			continue;
		}

		p = (struct myaddrs *)malloc(sizeof(struct myaddrs) +
		    ifa->ifa_addr->sa_len);
		if (!p) {
			exit_failure("not enough core");
			/*NOTREACHED*/
		}
		memcpy(p + 1, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		p->next = myaddrs;
		p->addr = (struct sockaddr *)(p + 1);
#ifdef __KAME__
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)p->addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)
			 || IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {
				sin6->sin6_scope_id =
					ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
				sin6->sin6_addr.s6_addr[2] = 0;
				sin6->sin6_addr.s6_addr[3] = 0;
			}
		}
#endif
		myaddrs = p;
		if (dflag) {
			char hbuf[NI_MAXHOST];
			getnameinfo(p->addr, p->addr->sa_len,
				hbuf, sizeof(hbuf), NULL, 0,
				NI_NUMERICHOST);
			syslog(LOG_INFO, "my interface: %s %s", hbuf,
			    ifa->ifa_name);
		}
	}

	freeifaddrs(ifap);
#else
	int s;
	unsigned int maxif;
	struct ifreq *iflist;
	struct ifconf ifconf;
	struct ifreq *ifr, *ifrp, *ifr_end;
	struct myaddrs *p;
	struct sockaddr_in6 *sin6;
	size_t siz;
	char ifrbuf[sizeof(struct ifreq) + 1024];

	maxif = if_maxindex() + 1;
	iflist = (struct ifreq *)malloc(maxif * BUFSIZ);	/* XXX */
	if (!iflist) {
		exit_failure("not enough core");
		/*NOTREACHED*/
	}

	if ((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		exit_failure("socket(SOCK_DGRAM)");
		/*NOTREACHED*/
	}
	memset(&ifconf, 0, sizeof(ifconf));
	ifconf.ifc_req = iflist;
	ifconf.ifc_len = maxif * BUFSIZ;	/* XXX */
	if (ioctl(s, SIOCGIFCONF, &ifconf) < 0) {
		exit_failure("ioctl(SIOCGIFCONF)");
		/*NOTREACHED*/
	}
	close(s);

	/* Look for this interface in the list */
	ifr_end = (struct ifreq *) (ifconf.ifc_buf + ifconf.ifc_len);
	for (ifrp = ifconf.ifc_req;
	     ifrp < ifr_end;
	     ifrp = (struct ifreq *)((char *)ifrp + siz)) {
		memcpy(ifrbuf, ifrp, sizeof(*ifrp));
		ifr = (struct ifreq *)ifrbuf;
		siz = ifr->ifr_addr.sa_len;
		if (siz < sizeof(ifr->ifr_addr))
			siz = sizeof(ifr->ifr_addr);
		siz += (sizeof(*ifrp) - sizeof(ifr->ifr_addr));
		if (siz > sizeof(ifrbuf)) {
			/* ifr too big */
			break;
		}
		memcpy(ifrbuf, ifrp, siz);

		switch (ifr->ifr_addr.sa_family) {
		case AF_INET:
		case AF_INET6:
			p = (struct myaddrs *)malloc(sizeof(struct myaddrs)
				+ ifr->ifr_addr.sa_len);
			if (!p) {
				exit_failure("not enough core");
				/*NOTREACHED*/
			}
			memcpy(p + 1, &ifr->ifr_addr, ifr->ifr_addr.sa_len);
			p->next = myaddrs;
			p->addr = (struct sockaddr *)(p + 1);
#ifdef __KAME__
			if (ifr->ifr_addr.sa_family == AF_INET6) {
				sin6 = (struct sockaddr_in6 *)p->addr;
				if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)
				 || IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr)) {
					sin6->sin6_scope_id =
						ntohs(*(u_int16_t *)&sin6->sin6_addr.s6_addr[2]);
					sin6->sin6_addr.s6_addr[2] = 0;
					sin6->sin6_addr.s6_addr[3] = 0;
				}
			}
#endif
			myaddrs = p;
			if (dflag) {
				char hbuf[NI_MAXHOST];
				getnameinfo(p->addr, p->addr->sa_len,
					hbuf, sizeof(hbuf), NULL, 0,
					NI_NUMERICHOST);
				syslog(LOG_INFO, "my interface: %s %s", hbuf, ifr->ifr_name);
			}
			break;
		default:
			break;
		}
	}

	free(iflist);
#endif
}

static void
free_myaddrs()
{
	struct myaddrs *p, *q;

	p = myaddrs;
	while (p) {
		q = p->next;
		free(p);
		p = q;
	}
	myaddrs = NULL;
}

static void
update_myaddrs()
{
	char msg[BUFSIZ];
	int len;
	struct rt_msghdr *rtm;

	len = read(sockfd, msg, sizeof(msg));
	if (len < 0) {
		syslog(LOG_ERR, "read(PF_ROUTE) failed");
		return;
	}
	rtm = (struct rt_msghdr *)msg;
	if (len < 4 || len < rtm->rtm_msglen) {
		syslog(LOG_ERR, "read(PF_ROUTE) short read");
		return;
	}
	if (rtm->rtm_version != RTM_VERSION) {
		syslog(LOG_ERR, "routing socket version mismatch");
		close(sockfd);
		sockfd = 0;
		return;
	}
	switch (rtm->rtm_type) {
	case RTM_NEWADDR:
	case RTM_DELADDR:
	case RTM_IFINFO:
		break;
	default:
		return;
	}
	/* XXX more filters here? */

	syslog(LOG_INFO, "update interface address list");
	free_myaddrs();
	grab_myaddrs();
}
#endif /*USE_ROUTE*/

static void
usage()
{
	fprintf(stderr, "usage: %s [-dp] [service [serverpath [serverargs]]]\n",
		faithdname);
	exit(0);
}
