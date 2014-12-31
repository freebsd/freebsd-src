/*	$NetBSD: nfsd.c,v 1.4 2013/10/19 17:45:00 christos Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1989, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsd.c	8.9 (Berkeley) 3/29/95";
#else
__RCSID("$NetBSD: nfsd.c,v 1.4 2013/10/19 17:45:00 christos Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <poll.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s, args...)						\
do {									\
    fprintf(stderr,(s), ## args); 					\
    fprintf(stderr, "\n");						\
} while (/*CONSTCOND*/0)
int	debug = 1;
#else
int	debug = 0;
#endif

int	main __P((int, char **));
void	nonfs __P((int));
void	usage __P((void));

static void *
child(void *arg)
{
	struct	nfsd_srvargs nsd;
	int nfssvc_flag;

	nfssvc_flag = NFSSVC_NFSD;
	memset(&nsd, 0, sizeof(nsd));
	while (rump_sys_nfssvc(nfssvc_flag, &nsd) < 0) {
		if (errno != ENEEDAUTH) {
			syslog(LOG_ERR, "nfssvc: %m %d", errno);
			exit(1);
		}
		nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHINFAIL;
	}

	return NULL;
}

/*
 * Nfs server daemon mostly just a user context for nfssvc()
 *
 * 1 - do file descriptor and signal cleanup
 * 2 - create the nfsd thread(s)
 * 3 - create server socket(s)
 * 4 - register socket with portmap
 *
 * For connectionless protocols, just pass the socket into the kernel via
 * nfssvc().
 * For connection based sockets, loop doing accepts. When you get a new
 * socket from accept, pass the msgsock into the kernel via nfssvc().
 * The arguments are:
 *	-c - support iso cltp clients
 *	-r - reregister with portmapper
 *	-t - support tcp nfs clients
 *	-u - support udp nfs clients
 * followed by "n" which is the number of nfsd threads to create
 */
int nfsd_main(int, char**);
int
nfsd_main(argc, argv)
	int argc;
	char *argv[];
{
	struct nfsd_args nfsdargs;
	struct addrinfo *ai_udp, *ai_tcp, *ai_udp6, *ai_tcp6, hints;
	struct netconfig *nconf_udp, *nconf_tcp, *nconf_udp6, *nconf_tcp6;
	struct netbuf nb_udp, nb_tcp, nb_udp6, nb_tcp6;
	struct sockaddr_in inetpeer;
	struct pollfd set[4];
	socklen_t len;
	int ch, connect_type_cnt, i, msgsock;
	int nfsdcnt, on = 1, reregister, sock, tcpflag, tcpsock;
	int tcp6sock, ip6flag;
	int tp4cnt, tp4flag, tpipcnt, udpflag, ecode, s;
	int error = 0;

#define	DEFNFSDCNT	 4
	nfsdcnt = DEFNFSDCNT;
	reregister = tcpflag = tp4cnt = tp4flag = tpipcnt = 0;
	udpflag = ip6flag = 0;
	nconf_udp = nconf_tcp = nconf_udp6 = nconf_tcp6 = NULL;
	tcpsock = tcp6sock = -1;
#define	GETOPT	"6n:rtu"
#define	USAGE	"[-rtu] [-n num_servers]"
	while ((ch = getopt(argc, argv, GETOPT)) != -1) {
		switch (ch) {
		case '6':
			ip6flag = 1;
			s = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			if (s < 0 && (errno == EPROTONOSUPPORT ||
			    errno == EPFNOSUPPORT || errno == EAFNOSUPPORT))
				ip6flag = 0;
			else
				close(s);
			break;
		case 'n':
			nfsdcnt = atoi(optarg);
			if (nfsdcnt < 1) {
				warnx("nfsd count %d; reset to %d", nfsdcnt, DEFNFSDCNT);
				nfsdcnt = DEFNFSDCNT;
			}
			break;
		case 'r':
			reregister = 1;
			break;
		case 't':
			tcpflag = 1;
			break;
		case 'u':
			udpflag = 1;
			break;
		default:
		case '?':
			usage();
		};
	}
	argv += optind;
	argc -= optind;

	/*
	 * XXX
	 * Backward compatibility, trailing number is the count of daemons.
	 */
	if (argc > 1)
		usage();
	if (argc == 1) {
		nfsdcnt = atoi(argv[0]);
		if (nfsdcnt < 1) {
			warnx("nfsd count %d; reset to %d", nfsdcnt, DEFNFSDCNT);
			nfsdcnt = DEFNFSDCNT;
		}
	}

	/*
	 * If none of TCP or UDP are specified, default to UDP only.
	 */
	if (tcpflag == 0 && udpflag == 0)
		udpflag = 1;

	if (debug == 0) {
		fprintf(stderr, "non-debug not supported here\n");
		exit(1);

#ifdef not_the_debug_man
		daemon(0, 0);
		(void)signal(SIGHUP, SIG_IGN);
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
		(void)signal(SIGSYS, nonfs);
#endif
	}

	if (udpflag) {
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = PF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp);
		if (ecode != 0) {
			syslog(LOG_ERR, "getaddrinfo udp: %s",
			    gai_strerror(ecode));
			exit(1);
		}

		nconf_udp = getnetconfigent("udp");

		if (nconf_udp == NULL)
			err(1, "getnetconfigent udp failed");

		nb_udp.buf = ai_udp->ai_addr;
		nb_udp.len = nb_udp.maxlen = ai_udp->ai_addrlen;
		if (reregister)
			if (!rpcb_set(RPCPROG_NFS, 2, nconf_udp, &nb_udp))
				err(1, "rpcb_set udp failed");
	}

	if (tcpflag) {
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = PF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		ecode = getaddrinfo(NULL, "nfs", &hints, &ai_tcp);
		if (ecode != 0) {
			syslog(LOG_ERR, "getaddrinfo tcp: %s",
			    gai_strerror(ecode));
			exit(1);
		}

		nconf_tcp = getnetconfigent("tcp");

		if (nconf_tcp == NULL)
			err(1, "getnetconfigent tcp failed");

		nb_tcp.buf = ai_tcp->ai_addr;
		nb_tcp.len = nb_tcp.maxlen = ai_tcp->ai_addrlen;
		if (reregister)
			if (!rpcb_set(RPCPROG_NFS, 2, nconf_tcp, &nb_tcp))
				err(1, "rpcb_set tcp failed");
	}

	if (udpflag && ip6flag) {
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = PF_INET6;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;

		ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp6);
		if (ecode != 0) {
			syslog(LOG_ERR, "getaddrinfo udp: %s",
			    gai_strerror(ecode));
			exit(1);
		}

		nconf_udp6 = getnetconfigent("udp6");

		if (nconf_udp6 == NULL)
			err(1, "getnetconfigent udp6 failed");

		nb_udp6.buf = ai_udp6->ai_addr;
		nb_udp6.len = nb_udp6.maxlen = ai_udp6->ai_addrlen;
		if (reregister)
			if (!rpcb_set(RPCPROG_NFS, 2, nconf_udp6, &nb_udp6))
				err(1, "rpcb_set udp6 failed");
	}

	if (tcpflag && ip6flag) {
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = PF_INET6;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		ecode = getaddrinfo(NULL, "nfs", &hints, &ai_tcp6);
		if (ecode != 0) {
			syslog(LOG_ERR, "getaddrinfo tcp: %s",
			    gai_strerror(ecode));
			exit(1);
		}

		nconf_tcp6 = getnetconfigent("tcp6");

		if (nconf_tcp6 == NULL)
			err(1, "getnetconfigent tcp6 failed");

		nb_tcp6.buf = ai_tcp6->ai_addr;
		nb_tcp6.len = nb_tcp6.maxlen = ai_tcp6->ai_addrlen;
		if (reregister)
			if (!rpcb_set(RPCPROG_NFS, 2, nconf_tcp6, &nb_tcp6))
				err(1, "rpcb_set tcp6 failed");
	}

	openlog("nfsd", LOG_PID, LOG_DAEMON);

	for (i = 0; i < nfsdcnt; i++) {
		pthread_t t;
		pthread_create(&t, NULL, child, NULL);
	}

	/* If we are serving udp, set up the socket. */
	if (udpflag) {
		if ((sock = rump_sys_socket(ai_udp->ai_family, ai_udp->ai_socktype,
		    ai_udp->ai_protocol)) < 0) {
			syslog(LOG_ERR, "can't create udp socket");
			exit(1);
		}
		if (bind(sock, ai_udp->ai_addr, ai_udp->ai_addrlen) < 0) {
			syslog(LOG_ERR, "can't bind udp addr");
			exit(1);
		}
		if (!rpcb_set(RPCPROG_NFS, 2, nconf_udp, &nb_udp) ||
		    !rpcb_set(RPCPROG_NFS, 3, nconf_udp, &nb_udp)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			exit(1);
		}
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't add UDP socket");
			exit(1);
		}
		(void)rump_sys_close(sock);
	}

	if (udpflag &&ip6flag) {
		if ((sock = rump_sys_socket(ai_udp6->ai_family, ai_udp6->ai_socktype,
		    ai_udp6->ai_protocol)) < 0) {
			syslog(LOG_ERR, "can't create udp socket");
			exit(1);
		}
		if (rump_sys_setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
		    &on, sizeof on) < 0) {
			syslog(LOG_ERR, "can't set v6-only binding for udp6 "
					"socket: %m");
			exit(1);
		}
		if (rump_sys_bind(sock, ai_udp6->ai_addr, ai_udp6->ai_addrlen) < 0) {
			syslog(LOG_ERR, "can't bind udp addr");
			exit(1);
		}
		if (!rpcb_set(RPCPROG_NFS, 2, nconf_udp6, &nb_udp6) ||
		    !rpcb_set(RPCPROG_NFS, 3, nconf_udp6, &nb_udp6)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			exit(1);
		}
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't add UDP6 socket");
			exit(1);
		}
		(void)rump_sys_close(sock);
	}

	/* Now set up the master server socket waiting for tcp connections. */
	on = 1;
	connect_type_cnt = 0;
	if (tcpflag) {
		if ((tcpsock = rump_sys_socket(ai_tcp->ai_family, ai_tcp->ai_socktype,
		    ai_tcp->ai_protocol)) < 0) {
			syslog(LOG_ERR, "can't create tcp socket");
			exit(1);
		}
		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		if (bind(tcpsock, ai_tcp->ai_addr, ai_tcp->ai_addrlen) < 0) {
			syslog(LOG_ERR, "can't bind tcp addr");
			exit(1);
		}
		if (rump_sys_listen(tcpsock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			exit(1);
		}
		if (!rpcb_set(RPCPROG_NFS, 2, nconf_tcp, &nb_tcp) ||
		    !rpcb_set(RPCPROG_NFS, 3, nconf_tcp, &nb_tcp)) {
			syslog(LOG_ERR, "can't register tcp with rpcbind");
			exit(1);
		}
		set[0].fd = tcpsock;
		set[0].events = POLLIN;
		connect_type_cnt++;
	} else
		set[0].fd = -1;

	if (tcpflag && ip6flag) {
		if ((tcp6sock = socket(ai_tcp6->ai_family, ai_tcp6->ai_socktype,
		    ai_tcp6->ai_protocol)) < 0) {
			syslog(LOG_ERR, "can't create tcp socket");
			exit(1);
		}
		if (setsockopt(tcp6sock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		if (setsockopt(tcp6sock, IPPROTO_IPV6, IPV6_V6ONLY,
		    &on, sizeof on) < 0) {
			syslog(LOG_ERR, "can't set v6-only binding for tcp6 "
					"socket: %m");
			exit(1);
		}
		if (bind(tcp6sock, ai_tcp6->ai_addr, ai_tcp6->ai_addrlen) < 0) {
			syslog(LOG_ERR, "can't bind tcp6 addr");
			exit(1);
		}
		if (listen(tcp6sock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			exit(1);
		}
		if (!rpcb_set(RPCPROG_NFS, 2, nconf_tcp6, &nb_tcp6) ||
		    !rpcb_set(RPCPROG_NFS, 3, nconf_tcp6, &nb_tcp6)) {
			syslog(LOG_ERR, "can't register tcp6 with rpcbind");
			exit(1);
		}
		set[1].fd = tcp6sock;
		set[1].events = POLLIN;
		connect_type_cnt++;
	} else
		set[1].fd = -1;

	set[2].fd = -1;
	set[3].fd = -1;

	if (connect_type_cnt == 0) {
		pause();
		exit(0);
	}

	/*
	 * Loop forever accepting connections and passing the sockets
	 * into the kernel for the mounts.
	 */
	for (;;) {
		if (rump_sys_poll(set, 4, INFTIM) < 1) {
			syslog(LOG_ERR, "poll failed: %m");
			exit(1);
		}

			len = sizeof(inetpeer);
			if ((msgsock = accept(tcpsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %d", error);
				exit(1);
			}
			memset(inetpeer.sin_zero, 0, sizeof(inetpeer.sin_zero));
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = sizeof(inetpeer);
			rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			(void)rump_sys_close(msgsock);

#ifdef notyet
		if (set[1].revents & POLLIN) {
			len = sizeof(inet6peer);
			if ((msgsock = rump_sys_accept(tcp6sock,
			    (struct sockaddr *)&inet6peer, &len, &error)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			if (rump_sys_setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on), &error) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inet6peer;
			nfsdargs.namelen = sizeof(inet6peer);
			rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs, &error);
			(void)rump_sys_close(msgsock, &error);
		}

		if (set[2].revents & POLLIN) {
			len = sizeof(isopeer);
			if ((msgsock = rump_sys_accept(tp4sock,
			    (struct sockaddr *)&isopeer, &len, &error)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			if (rump_sys_setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on), &error) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&isopeer;
			nfsdargs.namelen = len;
			rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs, &error);
			(void)rump_sys_close(msgsock, &error);
		}

		if (set[3].revents & POLLIN) {
			len = sizeof(inetpeer);
			if ((msgsock = rump_sys_accept(tpipsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				exit(1);
			}
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR, "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = len;
			rump_sys_nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			(void)rump_sys_close(msgsock);
		}
#endif /* notyet */
	}
}

void
usage()
{
	(void)fprintf(stderr, "usage: nfsd %s\n", USAGE);
	exit(1);
}

void
nonfs(signo)
	int signo;
{
	syslog(LOG_ERR, "missing system call: NFS not available.");
}
