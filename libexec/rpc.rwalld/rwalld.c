/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/libexec/rpc.rwalld/rwalld.c,v 1.8 1999/08/28 00:09:57 peter Exp $";
#endif /* not lint */

#include <err.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/rwall.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef OSF
#define WALL_CMD "/usr/sbin/wall"
#else
#define WALL_CMD "/usr/bin/wall -n"
#endif

void wallprog_1();
void possess();
void killkids();
static void usage __P((void));

int nodaemon = 0;
int from_inetd = 1;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	SVCXPRT *transp;
	int s, salen;
	struct sockaddr_in sa;
        int sock = 0;
        int proto = 0;

	if (argc == 2 && !strcmp(argv[1], "-n"))
		nodaemon = 1;
	if (argc != 1 && !nodaemon)
		usage();

	if (geteuid() == 0) {
		struct passwd *pep = getpwnam("nobody");
		if (pep)
			setuid(pep->pw_uid);
		else
			setuid(getuid());
	}

        /*
         * See if inetd started us
         */
	salen = sizeof(sa);
        if (getsockname(0, (struct sockaddr *)&sa, &salen) < 0) {
                from_inetd = 0;
                sock = RPC_ANYSOCK;
                proto = IPPROTO_UDP;
        }

        if (!from_inetd) {
                if (!nodaemon)
                        possess();

                (void)pmap_unset(WALLPROG, WALLVERS);
                if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
                        err(1, "socket");
                bzero((char *)&sa, sizeof sa);
                if (bind(s, (struct sockaddr *)&sa, sizeof sa) < 0)
                        err(1, "bind");

                salen = sizeof sa;
                if (getsockname(s, (struct sockaddr *)&sa, &salen))
                        err(1, "getsockname");

                pmap_set(WALLPROG, WALLVERS, IPPROTO_UDP, ntohs(sa.sin_port));
                if (dup2(s, 0) < 0)
                        err(1, "dup2");
                (void)pmap_unset(WALLPROG, WALLVERS);
        }

	(void)signal(SIGCHLD, killkids);

	openlog("rpc.rwalld", LOG_CONS|LOG_PID, LOG_DAEMON);

	transp = svcudp_create(sock);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create udp service");
		exit(1);
	}
	if (!svc_register(transp, WALLPROG, WALLVERS, wallprog_1, proto)) {
		syslog(LOG_ERR, "unable to register (WALLPROG, WALLVERS, %s)", proto?"udp":"(inetd)");
		exit(1);
	}
	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}

static void
usage()
{
	fprintf(stderr, "usage: rpc.rwalld [-n]\n");
	exit(1);
}

void possess()
{
	daemon(0, 0);
}

void killkids()
{
	while(wait4(-1, NULL, WNOHANG, NULL) > 0)
		;
}

void *wallproc_wall_1_svc(s, rqstp)
	wrapstring		*s;
	struct svc_req		*rqstp;
{
	static void		*dummy = NULL;

	/* fork, popen wall with special option, and send the message */
	if (fork() == 0) {
		FILE *pfp;

		pfp = popen(WALL_CMD, "w");
		if (pfp != NULL) {
			fprintf(pfp, "\007\007%s", *s);
			pclose(pfp);
			exit(0);
		}
	}
	return(&dummy);
}

void
wallprog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		char *wallproc_wall_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		goto leave;

	case WALLPROC_WALL:
		xdr_argument = xdr_wrapstring;
		xdr_result = xdr_void;
		local = (char *(*)()) wallproc_wall_1_svc;
		break;

	default:
		svcerr_noproc(transp);
		goto leave;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		goto leave;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
leave:
        if (from_inetd)
                exit(0);
}
