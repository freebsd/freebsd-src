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
 *
 *	$Id: yp_main.c,v 1.6 1996/05/31 16:01:50 wpaul Exp $
 */

/*
 * ypserv startup function.
 * We need out own main() since we have to do some additional work
 * that rpcgen won't do for us. Most of this file was generated using
 * rpcgen.new, and later modified.
 */

#include "yp.h"
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */
#include <signal.h>
#include <sys/ttycom.h> /* TIOCNOTTY */
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/wait.h>
#include "yp_extern.h"
#include <unistd.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <err.h>

#ifndef SIG_PF
#define	SIG_PF void(*)(int)
#endif

#define	_RPCSVC_CLOSEDOWN 120
#ifndef lint
static const char rcsid[] = "$Id: yp_main.c,v 1.6 1996/05/31 16:01:50 wpaul Exp $";
#endif /* not lint */
int _rpcpmstart;		/* Started by a port monitor ? */
static int _rpcfdtype;
		 /* Whether Stream or Datagram ? */
	/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1
#define	_SERVING 2

extern void ypprog_1 __P((struct svc_req, register SVCXPRT));
extern void ypprog_2 __P((struct svc_req, register SVCXPRT));
extern int _rpcsvcstate;	 /* Set when a request is serviced */
char *progname = "ypserv";
char *yp_dir = _PATH_YP;
int debug = 0;
int do_dns = 0;

static
void _msgout(char* msg)
{
	if (debug) {
		if (_rpcpmstart)
			syslog(LOG_ERR, msg);
		else
			(void) fprintf(stderr, "%s\n", msg);
	} else
		syslog(LOG_ERR, msg);
}

static void unregister()
{
	(void) pmap_unset(YPPROG, YPVERS);
	(void) pmap_unset(YPPROG, YPOLDVERS);
}

static void reaper(sig)
	int sig;
{
	int status;

	if (sig == SIGHUP) {
		load_securenets();
#ifdef DB_CACHE
		yp_flush_all();
#endif
		return;
	}

	if (sig == SIGCHLD) {
		while (wait3(&status, WNOHANG, NULL) > 0)
			children--;
	} else {
		unregister();
		exit(0);
	}
}

static void usage()
{
	fprintf(stderr, "Usage: %s [-h] [-d] [-n] [-p path]\n", progname);
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

main(argc, argv)
	int argc;
	char *argv[];
{
	register SVCXPRT *transp = NULL;
	int sock;
	int proto = 0;
	struct sockaddr_in saddr;
	int asize = sizeof (saddr);
	int ch;

	while ((ch = getopt(argc, argv, "hdnp:")) != EOF) {
		switch(ch) {
		case 'd':
			debug = ypdb_debug = 1;
			break;
		case 'n':
			do_dns = 1;
			break;
		case 'p':
			yp_dir = optarg;
			break;
		case 'h':
		default:
			usage();
		}
	}

	load_securenets();
	yp_init_async();
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
		openlog(progname, LOG_PID, LOG_DAEMON);
	} else {
		if (!debug) {
			if (daemon(0,0)) {
				err(1,"cannot fork");
			}
			openlog(progname, LOG_PID, LOG_DAEMON);
		}
		sock = RPC_ANYSOCK;
		(void) pmap_unset(YPPROG, YPVERS);
		(void) pmap_unset(YPPROG, 1);
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			_msgout("cannot create udp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
			_msgout("unable to register (YPPROG, YPOLDVERS, udp).");
			exit(1);
		}
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, udp).");
			exit(1);
		}
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			_msgout("cannot create tcp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
			_msgout("unable to register (YPPROG, YPOLDVERS, tcp).");
			exit(1);
		}
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, tcp).");
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
