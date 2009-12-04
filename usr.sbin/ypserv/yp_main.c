/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * ypserv startup function.
 * We need out own main() since we have to do some additional work
 * that rpcgen won't do for us. Most of this file was generated using
 * rpcgen.new, and later modified.
 */

#include "yp.h"
#include <err.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h> /* getenv, exit */
#include <string.h> /* strcmp */
#include <syslog.h>
#include <unistd.h>
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include "yp_extern.h"
#include <rpc/rpc.h>

#ifndef SIG_PF
#define	SIG_PF void(*)(int)
#endif

#define	_RPCSVC_CLOSEDOWN 120
int _rpcpmstart;		/* Started by a port monitor ? */
static int _rpcfdtype;
		 /* Whether Stream or Datagram ? */
	/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1
#define	_SERVING 2

extern void ypprog_1(struct svc_req *, register SVCXPRT *);
extern void ypprog_2(struct svc_req *, register SVCXPRT *);
extern int _rpc_dtablesize(void);
extern int _rpcsvcstate;	 /* Set when a request is serviced */
char *progname = "ypserv";
char *yp_dir = _PATH_YP;
/*int debug = 0;*/
int do_dns = 0;
int resfd;

struct socktype {
	const char *st_name;
	int	   st_type;
};
static struct socktype stlist[] = {
	{ "tcp", SOCK_STREAM },
	{ "udp", SOCK_DGRAM },
	{ NULL, 0 }
};

static
void _msgout(char* msg)
{
	if (debug) {
		if (_rpcpmstart)
			syslog(LOG_ERR, "%s", msg);
		else
			warnx("%s", msg);
	} else
		syslog(LOG_ERR, "%s", msg);
}

pid_t	yp_pid;

static void
yp_svc_run(void)
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else
	int readfds;
#endif /* def FD_SETSIZE */
	int fd_setsize = _rpc_dtablesize();
	struct timeval timeout;

	/* Establish the identity of the parent ypserv process. */
	yp_pid = getpid();

	for (;;) {
#ifdef FD_SETSIZE
		readfds = svc_fdset;
#else
		readfds = svc_fds;
#endif /* def FD_SETSIZE */

		FD_SET(resfd, &readfds);

		timeout.tv_sec = RESOLVER_TIMEOUT;
		timeout.tv_usec = 0;
		switch (select(fd_setsize, &readfds, NULL, NULL,
			       &timeout)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			warn("svc_run: - select failed");
			return;
		case 0:
			if (getpid() == yp_pid)
				yp_prune_dnsq();
			break;
		default:
			if (getpid() == yp_pid) {
				if (FD_ISSET(resfd, &readfds)) {
					yp_run_dnsq();
					FD_CLR(resfd, &readfds);
				}
				svc_getreqset(&readfds);
			}
		}
		if (yp_pid != getpid())
			_exit(0);
	}
}

static void
unregister(void)
{
	(void) pmap_unset(YPPROG, YPVERS);
	(void) pmap_unset(YPPROG, YPOLDVERS);
}

static void
reaper(int sig)
{
	int			status;
	int			saved_errno;

	saved_errno = errno;

	if (sig == SIGHUP) {
		load_securenets();
#ifdef DB_CACHE
		yp_flush_all();
#endif
		errno = saved_errno;
		return;
	}

	if (sig == SIGCHLD) {
		while (wait3(&status, WNOHANG, NULL) > 0)
			children--;
	} else {
		unregister();
		exit(0);
	}
	errno = saved_errno;
	return;
}

static void
usage(void)
{
	fprintf(stderr, "usage: ypserv [-h] [-d] [-n] [-p path] [-P port]\n");
	exit(1);
}

static void
closedown(int sig)
{
	if (_rpcsvcstate == _IDLE) {
		extern fd_set svc_fdset;
		static int size;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM) {
			unregister();
			exit(0);
		}
		if (size == 0) {
			size = getdtablesize();
		}
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1) {
			unregister();
			exit(0);
		}
	}
	if (_rpcsvcstate == _SERVED)
		_rpcsvcstate = _IDLE;

	(void) signal(SIGALRM, (SIG_PF) closedown);
	(void) alarm(_RPCSVC_CLOSEDOWN/2);
}

int
main(int argc, char *argv[])
{
	register SVCXPRT *transp = NULL;
	int sock;
	int proto = 0;
	struct sockaddr_in saddr;
	socklen_t asize = sizeof (saddr);
	int ch;
	in_port_t yp_port = 0;
	char *errstr;
	struct socktype *st;

	while ((ch = getopt(argc, argv, "hdnp:P:")) != -1) {
		switch (ch) {
		case 'd':
			debug = ypdb_debug = 1;
			break;
		case 'n':
			do_dns = 1;
			break;
		case 'p':
			yp_dir = optarg;
			break;
		case 'P':
			yp_port = (in_port_t)strtonum(optarg, 1, 65535,
			    (const char **)&errstr);
			if (yp_port == 0 && errstr != NULL) {
				_msgout("invalid port number provided");
				exit(1);
			}
			break;
		case 'h':
		default:
			usage();
		}
	}

	load_securenets();
	yp_init_resolver();
#ifdef DB_CACHE
	yp_init_dbs();
#endif
	if (getsockname(0, (struct sockaddr *)&saddr, &asize) == 0) {
		int ssize = sizeof (int);

		if (saddr.sin_family != AF_INET)
			exit(1);
		if (getsockopt(0, SOL_SOCKET, SO_TYPE,
				(char *)&_rpcfdtype, &ssize) == -1)
			exit(1);
		sock = 0;
		_rpcpmstart = 1;
		proto = 0;
		openlog("ypserv", LOG_PID, LOG_DAEMON);
	} else {
		if (!debug) {
			if (daemon(0,0)) {
				err(1,"cannot fork");
			}
			openlog("ypserv", LOG_PID, LOG_DAEMON);
		}
		sock = RPC_ANYSOCK;
		(void) pmap_unset(YPPROG, YPVERS);
		(void) pmap_unset(YPPROG, YPOLDVERS);
	}

	/*
	 * Initialize TCP/UDP sockets.
	 */
	memset((char *)&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr.sin_port = htons(yp_port);
	for (st = stlist; st->st_name != NULL; st++) {
		/* Do not bind the socket if the user didn't specify a port */
		if (yp_port == 0)
			break;

		sock = socket(AF_INET, st->st_type, 0);
		if (sock == -1) {
			if ((asprintf(&errstr, "cannot create a %s socket",
			    st->st_name)) == -1)
				err(1, "unexpected failure in asprintf()");
			_msgout(errstr);
			free((void *)errstr);
			exit(1);
		}
		if (bind(sock, (struct sockaddr *) &saddr, sizeof(saddr))
		    == -1) {
			if ((asprintf(&errstr, "cannot bind %s socket",
			    st->st_name)) == -1)
				err(1, "unexpected failure in asprintf()");
			_msgout(errstr);
			free((void *)errstr);
			exit(1);
		}
		errstr = NULL;
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			_msgout("cannot create udp service");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
			_msgout("unable to register (YPPROG, YPOLDVERS, udp)");
			exit(1);
		}
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, udp)");
			exit(1);
		}
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			_msgout("cannot create tcp service");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
			_msgout("unable to register (YPPROG, YPOLDVERS, tcp)");
			exit(1);
		}
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, tcp)");
			exit(1);
		}
	}

	if (transp == (SVCXPRT *)NULL) {
		_msgout("could not create a handle");
		exit(1);
	}
	if (_rpcpmstart) {
		(void) signal(SIGALRM, (SIG_PF) closedown);
		(void) alarm(_RPCSVC_CLOSEDOWN/2);
	}
/*
 * Make sure SIGPIPE doesn't blow us away while servicing TCP
 * connections.
 */
	(void) signal(SIGPIPE, SIG_IGN);
	(void) signal(SIGCHLD, (SIG_PF) reaper);
	(void) signal(SIGTERM, (SIG_PF) reaper);
	(void) signal(SIGINT, (SIG_PF) reaper);
	(void) signal(SIGHUP, (SIG_PF) reaper);
	yp_svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}
