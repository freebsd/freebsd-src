/*
 * Copyright (c) 1995, 1996
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
 *	$Id: yppasswdd_main.c,v 1.5 1996/10/22 14:58:10 wpaul Exp $
 */

#include "yppasswd.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h> /* getenv, exit */
#include <unistd.h>
#include <string.h>
#include <sys/param.h>
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/ttycom.h> /* TIOCNOTTY */
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <err.h>
#include <errno.h>
#include <rpcsvc/yp.h>
struct dom_binding {};
#include <rpcsvc/ypclnt.h>
#include "yppasswdd_extern.h"
#include "yppasswd_comm.h"
#include "ypxfr_extern.h"

#ifndef SIG_PF
#define	SIG_PF void(*)(int)
#endif

#ifdef DEBUG
#define	RPC_SVC_FG
#endif

#define	_RPCSVC_CLOSEDOWN 120
#ifndef lint
static const char rcsid[] = "$Id: yppasswdd_main.c,v 1.5 1996/10/22 14:58:10 wpaul Exp $";
#endif /* not lint */
int _rpcpmstart = 0;		/* Started by a port monitor ? */
static int _rpcfdtype;
		 /* Whether Stream or Datagram ? */
	/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1
#define	_SERVING 2

extern int _rpcsvcstate;	 /* Set when a request is serviced */
char *progname = "rpc.yppasswdd";
char *yp_dir = _PATH_YP;
char *passfile_default = _PATH_YP "master.passwd";
char *passfile;
char *yppasswd_domain = NULL;
int no_chsh = 0;
int no_chfn = 0;
int allow_additions = 0;
int multidomain = 0;
int verbose = 0;
int resvport = 1;
int inplace = 0;
int yp_sock;


static void
my_svc_run()
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else
      int readfds;
#endif /* def FD_SETSIZE */
	extern int errno;

	for (;;) {


#ifdef FD_SETSIZE
		readfds = svc_fdset;
#else
		readfds = svc_fds;
#endif /* def FD_SETSIZE */
		FD_SET(yp_sock, &readfds);

		switch (select(_rpc_dtablesize(), &readfds, (fd_set *)0, (fd_set *)0,
			       (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			perror("svc_run: - select failed");
			return;
		case 0:
			continue;
		default:
			if (FD_ISSET(yp_sock, &readfds)) {
				do_master();
				FD_CLR(yp_sock, &readfds);
			}
			svc_getreqset(&readfds);
		}
	}
}

static void terminate(sig)
	int sig;
{
	svc_unregister(YPPASSWDPROG, YPPASSWDVERS);
	close(yp_sock);
	unlink(sockname);
	exit(0);
}

static void reload(sig)
	int sig;
{
	load_securenets();
}

static void
closedown(int sig)
{
	if (_rpcsvcstate == _IDLE) {
		extern fd_set svc_fdset;
		static int size;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM) {
			close(yp_sock);
			unlink(sockname);
			exit(0);
		}
		if (size == 0) {
			size = getdtablesize();
		}
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1) {
			close(yp_sock);
			unlink(sockname);
			exit(0);
		}
	}
	if (_rpcsvcstate == _SERVED)
		_rpcsvcstate = _IDLE;

	(void) signal(SIGALRM, (SIG_PF) closedown);
	(void) alarm(_RPCSVC_CLOSEDOWN/2);
}

static void usage()
{
	fprintf(stderr, "Usage: %s [-t master.passwd file] [-d domain] \
[-p path] [-s] [-f] [-m] [-i] [-a] [-v] [-u] [-h]\n",
		progname);
	exit(1);
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
	int rval;
	char *mastername;
	char myname[MAXHOSTNAMELEN + 2];
	extern int errno;
	extern int debug;

	debug = 1;

	while ((ch = getopt(argc, argv, "t:d:p:sfamuivh")) != EOF) {
		switch(ch) {
		case 't':
			passfile_default = optarg;
			break;
		case 'd':
			yppasswd_domain = optarg;
			break;
		case 's':
			no_chsh++;
			break;
		case 'f':
			no_chfn++;
			break;
		case 'p':
			yp_dir = optarg;
			break;
		case 'a':
			allow_additions++;
			break;
		case 'm':
			multidomain++;
			break;
		case 'i':
			inplace++;
			break;
		case 'v':
			verbose++;
			break;
		case 'u':
			resvport = 0;
			break;
		default:
		case 'h':
			usage();
			break;
		}
	}

	if (yppasswd_domain == NULL) {
		if (yp_get_default_domain(&yppasswd_domain)) {
			yp_error("no domain specified and system domain \
name isn't set -- aborting");
		usage();
		}
	}

	load_securenets();

	if (getrpcport("localhost", YPPROG, YPVERS, IPPROTO_UDP) <= 0) {
		yp_error("no ypserv processes registered with local portmap");
		yp_error("this host is not an NIS server -- aborting");
		exit(1);
	}

	if ((mastername = ypxfr_get_master(yppasswd_domain, "passwd.byname",
						"localhost",0)) == NULL) {
		yp_error("can't get name of NIS master server for domain %s",
			 				yppasswd_domain);
		exit(1);
	}

	if (gethostname((char *)&myname, sizeof(myname)) == -1) {
		yp_error("can't get local hostname: %s", strerror(errno));
		exit(1);
	}

	if (strncmp(mastername, (char *)&myname, sizeof(myname))) {
		yp_error("master of %s is %s, but we are %s",
			"passwd.byname", mastername, myname);
		yp_error("this host is not the NIS master server for \
the %s domain -- aborting", yppasswd_domain);
		exit(1);
	}

	debug = 0;

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
		}
		openlog(progname, LOG_PID, LOG_DAEMON);
		sock = RPC_ANYSOCK;
		(void) pmap_unset(YPPASSWDPROG, YPPASSWDVERS);
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			yp_error("cannot create udp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS, yppasswdprog_1, proto)) {
			yp_error("unable to register (YPPASSWDPROG, YPPASSWDVERS, udp).");
			exit(1);
		}
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			yp_error("cannot create tcp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS, yppasswdprog_1, proto)) {
			yp_error("unable to register (YPPASSWDPROG, YPPASSWDVERS, tcp).");
			exit(1);
		}
	}

	if (transp == (SVCXPRT *)NULL) {
		yp_error("could not create a handle");
		exit(1);
	}
	if (_rpcpmstart) {
		(void) signal(SIGALRM, (SIG_PF) closedown);
		(void) alarm(_RPCSVC_CLOSEDOWN/2);
	}
	/* set up resource limits and block signals */
	pw_init();

	/* except SIGCHLD, which we need to catch */
	install_reaper(1);
	signal(SIGTERM, (SIG_PF) terminate);

	signal(SIGHUP, (SIG_PF) reload);

	unlink(sockname);
	yp_sock = makeservsock();
	if (chmod(sockname, 0))
		err(1, "chmod of %s failed", sockname);

	my_svc_run();
	yp_error("svc_run returned");
	exit(1);
	/* NOTREACHED */
}
