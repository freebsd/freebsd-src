/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
"@(#) Copyright (c) 1989 Regents of the University of California.\n\
 All rights reserved.\n";
#endif not lint

#ifndef lint
static char sccsid[] = "@(#)nfsd.c	5.10 (Berkeley) 4/24/91";
#endif not lint

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <stdio.h>
#include <syslog.h>
#include <fcntl.h>
#include <string.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s)	fprintf(stderr,(s))
int	debug = 1;
#else
int	debug = 0;
#endif
struct hadr {
	u_long	ha_sad;
	struct hadr *ha_next;
};
struct	hadr hphead;
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArg = NULL;	/* end of argv */
void	reapchild();

/*
 * Nfs server daemon mostly just a user context for nfssvc()
 * 1 - do file descriptor and signal cleanup
 * 2 - create server socket
 * 3 - register socket with portmap
 * For SOCK_DGRAM, just fork children and send them into the kernel
 * by calling nfssvc()
 * For connection based sockets, loop doing accepts. When you get a new socket
 * from accept, fork a child that drops into the kernel via. nfssvc.
 * This child will return from nfssvc when the connection is closed, so
 * just shutdown() and exit().
 * The arguments are:
 * -t - support tcp nfs clients
 * -u - support udp nfs clients
 */
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	register int i;
	register char *cp, *cp2;
	register struct hadr *hp;
	int udpcnt, sock, msgsock, tcpflag = 0, udpflag = 0, ret, len;
	int reregister = 0;
	char opt;
	extern int optind;
	extern char *optarg;
	struct sockaddr_in saddr, msk, mtch, peername;


	/*
	 *  Save start and extent of argv for setproctitle.
	 */

	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);
	while ((opt = getopt(argc, argv, "rt:u:")) != EOF)
		switch (opt) {
		case 'r':
			reregister++;
			break;
		case 't':
			tcpflag++;
			if (cp = index(optarg, ',')) {
				*cp++ = '\0';
				msk.sin_addr.s_addr = inet_addr(optarg);
				if (msk.sin_addr.s_addr == -1)
					usage();
				if (cp2 = index(cp, ','))
					*cp2++ = '\0';
				mtch.sin_addr.s_addr = inet_addr(cp);
				if (mtch.sin_addr.s_addr == -1)
					usage();
				cp = cp2;
				hphead.ha_next = (struct hadr *)0;
				while (cp) {
					if (cp2 = index(cp, ','))
						*cp2++ = '\0';
					hp = (struct hadr *)
						malloc(sizeof (struct hadr));
					hp->ha_sad = inet_addr(cp);
					if (hp->ha_sad == -1)
						usage();
					hp->ha_next = hphead.ha_next;
					hphead.ha_next = hp;
					cp = cp2;
				}
			} else
				usage();
			break;
		case 'u':
			udpflag++;
			if (cp = index(optarg, ',')) {
				*cp++ = '\0';
				msk.sin_addr.s_addr = inet_addr(optarg);
				if (msk.sin_addr.s_addr == -1)
					usage();
				if (cp2 = index(cp, ','))
					*cp2++ = '\0';
				mtch.sin_addr.s_addr = inet_addr(cp);
				if (mtch.sin_addr.s_addr == -1)
					usage();
				if (cp2)
					udpcnt = atoi(cp2);
				if (udpcnt < 1 || udpcnt > 20)
					udpcnt = 1;
			} else
				usage();
			break;
		default:
		case '?':
			usage();
		}

	/*
	 * Default, if neither UDP nor TCP is specified,
	 * is to support UDP only; a numeric argument indicates
	 * the number of server daemons to run.
	 */
	if (udpflag == 0 && tcpflag == 0) {
		if (argc > 1)
			udpcnt = atoi(*++argv);
		if (udpcnt < 1 || udpcnt > 20)
			udpcnt = 1;
		msk.sin_addr.s_addr = mtch.sin_addr.s_addr = 0;
		udpflag++;
	}

	if (debug == 0) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		signal(SIGTERM, SIG_IGN);
		signal(SIGHUP, SIG_IGN);
	}
	signal(SIGCHLD, reapchild);

	if (reregister) {
		if (udpflag && !pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_UDP,
		    NFS_PORT)) {
			fprintf(stderr,
			    "Can't register with portmap for UDP\n");
			exit(1);
		}
		if (tcpflag && !pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP,
		    NFS_PORT)) {
			fprintf(stderr,
			    "Can't register with portmap for TCP\n");
			exit(1);
		}
		exit(0);
	}
	openlog("nfsd:", LOG_PID, LOG_DAEMON);
#ifdef notdef
	/* why? unregisters both protocols even if we restart only one */
	pmap_unset(RPCPROG_NFS, NFS_VER2);
#endif
	if (udpflag) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "Can't create socket");
			exit(1);
		}
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = INADDR_ANY;
		saddr.sin_port = htons(NFS_PORT);
		if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
			syslog(LOG_ERR, "Can't bind addr");
			exit(1);
		}
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_UDP, NFS_PORT)) {
			syslog(LOG_ERR, "Can't register with portmap");
			exit(1);
		}
	
		/*
		 * Send the nfs datagram servers
		 * right down into the kernel
		 */
		for (i = 0; i < udpcnt; i++)
			if (fork() == 0) {
				setproctitle("nfsd-udp",
				    (struct sockaddr_in *)NULL);
				ret = nfssvc(sock, &msk, sizeof(msk),
						&mtch, sizeof(mtch));
				if (ret < 0)
					syslog(LOG_ERR, "nfssvc() failed %m");
				exit(1);
			}
		close(sock);
	}

	/*
	 * Now set up the master STREAM server waiting for tcp connections.
	 */
	if (tcpflag) {
		int on = 1;

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_ERR, "Can't create socket");
			exit(1);
		}
		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		saddr.sin_family = AF_INET;
		saddr.sin_addr.s_addr = INADDR_ANY;
		saddr.sin_port = htons(NFS_PORT);
		if (bind(sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
			syslog(LOG_ERR, "Can't bind addr");
			exit(1);
		}
		if (listen(sock, 5) < 0) {
			syslog(LOG_ERR, "Listen failed");
			exit(1);
		}
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "Can't register with portmap");
			exit(1);
		}
		setproctitle("nfsd-listen", (struct sockaddr_in *)NULL);
		/*
		 * Loop forever accepting connections and sending the children
		 * into the kernel to service the mounts.
		 */
		for (;;) {
			len = sizeof(peername);
			if ((msgsock = accept(sock,
			    (struct sockaddr *)&peername, &len)) < 0) {
				syslog(LOG_ERR, "Accept failed: %m");
				exit(1);
			}
			if ((peername.sin_addr.s_addr & msk.sin_addr.s_addr) !=
			   mtch.sin_addr.s_addr) {
				hp = hphead.ha_next;
				while (hp) {
					if (peername.sin_addr.s_addr ==
						hp->ha_sad)
						break;
					hp = hp->ha_next;
				}
				if (hp == NULL) {
					shutdown(msgsock, 2);
					close(msgsock);
					continue;
				}
			}
			if (fork() == 0) {
				close(sock);
				setproctitle("nfsd-tcp", &peername);
				if (setsockopt(msgsock, SOL_SOCKET,
				    SO_KEEPALIVE, (char *) &on, sizeof(on)) < 0)
					syslog(LOG_ERR,
					    "setsockopt SO_KEEPALIVE: %m");
				ret = nfssvc(msgsock, &msk, sizeof(msk),
						&mtch, sizeof(mtch));
				shutdown(msgsock, 2);
				if (ret < 0)
					syslog(LOG_NOTICE,
					    "Nfssvc STREAM Failed");
				exit(1);
			}
			close(msgsock);
		}
	}
}

usage()
{
	fprintf(stderr, "nfsd [-t msk,mtch[,addrs]] [-u msk,mtch,numprocs]\n");
	exit(1);
}

void
reapchild()
{

	while (wait3((int *) NULL, WNOHANG, (struct rusage *) NULL))
		;
}

setproctitle(a, sin)
	char *a;
	struct sockaddr_in *sin;
{
	register char *cp;
	char buf[80];

	cp = Argv[0];
	if (sin)
		(void) sprintf(buf, "%s [%s]", a, inet_ntoa(sin->sin_addr));
	else
		(void) sprintf(buf, "%s", a);
	(void) strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = ' ';
}
