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
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsd.c	8.9 (Berkeley) 3/29/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/linker.h>
#include <sys/module.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsserver/nfs.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s...)	fprintf(stderr,s)
int	debug = 1;
#else
int	debug = 0;
#endif

#define	MAXNFSDCNT	256
#define	DEFNFSDCNT	 4
pid_t	children[MAXNFSDCNT];	/* PIDs of children */
int	nfsdcnt;		/* number of children */

void	cleanup(int);
void	child_cleanup(int);
void	killchildren(void);
void	nfsd_exit(int);
void	nonfs(int);
void	reapchild(int);
int	setbindhost(struct addrinfo **ia, const char *bindhost,
	    struct addrinfo hints);
void	start_server(int);
void	unregistration(void);
void	usage(void);

/*
 * Nfs server daemon mostly just a user context for nfssvc()
 *
 * 1 - do file descriptor and signal cleanup
 * 2 - fork the nfsd(s)
 * 3 - create server socket(s)
 * 4 - register socket with rpcbind
 *
 * For connectionless protocols, just pass the socket into the kernel via.
 * nfssvc().
 * For connection based sockets, loop doing accepts. When you get a new
 * socket from accept, pass the msgsock into the kernel via. nfssvc().
 * The arguments are:
 *	-r - reregister with rpcbind
 *	-d - unregister with rpcbind
 *	-t - support tcp nfs clients
 *	-u - support udp nfs clients
 * followed by "n" which is the number of nfsds' to fork off
 */
int
main(argc, argv, envp)
	int argc;
	char *argv[], *envp[];
{
	struct nfsd_args nfsdargs;
	struct addrinfo *ai_udp, *ai_tcp, *ai_udp6, *ai_tcp6, hints;
	struct netconfig *nconf_udp, *nconf_tcp, *nconf_udp6, *nconf_tcp6;
	struct netbuf nb_udp, nb_tcp, nb_udp6, nb_tcp6;
	struct sockaddr_in inetpeer;
	struct sockaddr_in6 inet6peer;
	fd_set ready, sockbits;
	fd_set v4bits, v6bits;
	int ch, connect_type_cnt, i, len, maxsock, msgsock;
	int on = 1, unregister, reregister, sock;
	int tcp6sock, ip6flag, tcpflag, tcpsock;
	int udpflag, ecode, s, srvcnt;
	int bindhostc, bindanyflag, rpcbreg, rpcbregcnt;
	char **bindhost = NULL;
	pid_t pid;

	if (modfind("nfsserver") < 0) {
		/* Not present in kernel, try loading it */
		if (kldload("nfsserver") < 0 || modfind("nfsserver") < 0)
			errx(1, "NFS server is not available");
	}

	nfsdcnt = DEFNFSDCNT;
	unregister = reregister = tcpflag = maxsock = 0;
	bindanyflag = udpflag = connect_type_cnt = bindhostc = 0;
#define	GETOPT	"ah:n:rdtu"
#define	USAGE	"[-ardtu] [-n num_servers] [-h bindip]"
	while ((ch = getopt(argc, argv, GETOPT)) != -1)
		switch (ch) {
		case 'a':
			bindanyflag = 1;
			break;
		case 'n':
			nfsdcnt = atoi(optarg);
			if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
				warnx("nfsd count %d; reset to %d", nfsdcnt,
				    DEFNFSDCNT);
				nfsdcnt = DEFNFSDCNT;
			}
			break;
		case 'h':
			bindhostc++;
			bindhost = realloc(bindhost,sizeof(char *)*bindhostc);
			if (bindhost == NULL) 
				errx(1, "Out of memory");
			bindhost[bindhostc-1] = strdup(optarg);
			if (bindhost[bindhostc-1] == NULL)
				errx(1, "Out of memory");
			break;
		case 'r':
			reregister = 1;
			break;
		case 'd':
			unregister = 1;
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
	if (!tcpflag && !udpflag)
		udpflag = 1;
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
		if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
			warnx("nfsd count %d; reset to %d", nfsdcnt,
			    DEFNFSDCNT);
			nfsdcnt = DEFNFSDCNT;
		}
	}

	ip6flag = 1;
	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		if (errno != EPROTONOSUPPORT)
			err(1, "socket");
		ip6flag = 0;
	} else if (getnetconfigent("udp6") == NULL ||
		getnetconfigent("tcp6") == NULL) {
		ip6flag = 0;
	}
	if (s != -1)
		close(s);

	if (bindhostc == 0 || bindanyflag) {
		bindhostc++;
		bindhost = realloc(bindhost,sizeof(char *)*bindhostc);
		if (bindhost == NULL) 
			errx(1, "Out of memory");
		bindhost[bindhostc-1] = strdup("*");
		if (bindhost[bindhostc-1] == NULL) 
			errx(1, "Out of memory");
	}

	if (unregister) {
		unregistration();
		exit (0);
	}
	if (reregister) {
		if (udpflag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp);
			if (ecode != 0)
				err(1, "getaddrinfo udp: %s", gai_strerror(ecode));
			nconf_udp = getnetconfigent("udp");
			if (nconf_udp == NULL)
				err(1, "getnetconfigent udp failed");
			nb_udp.buf = ai_udp->ai_addr;
			nb_udp.len = nb_udp.maxlen = ai_udp->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_udp, &nb_udp)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_udp, &nb_udp)))
				err(1, "rpcb_set udp failed");
			freeaddrinfo(ai_udp);
		}
		if (udpflag && ip6flag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp6);
			if (ecode != 0)
				err(1, "getaddrinfo udp6: %s", gai_strerror(ecode));
			nconf_udp6 = getnetconfigent("udp6");
			if (nconf_udp6 == NULL)
				err(1, "getnetconfigent udp6 failed");
			nb_udp6.buf = ai_udp6->ai_addr;
			nb_udp6.len = nb_udp6.maxlen = ai_udp6->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_udp6, &nb_udp6)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_udp6, &nb_udp6)))
				err(1, "rpcb_set udp6 failed");
			freeaddrinfo(ai_udp6);
		}
		if (tcpflag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_tcp);
			if (ecode != 0)
				err(1, "getaddrinfo tcp: %s", gai_strerror(ecode));
			nconf_tcp = getnetconfigent("tcp");
			if (nconf_tcp == NULL)
				err(1, "getnetconfigent tcp failed");
			nb_tcp.buf = ai_tcp->ai_addr;
			nb_tcp.len = nb_tcp.maxlen = ai_tcp->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_tcp, &nb_tcp)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_tcp, &nb_tcp)))
				err(1, "rpcb_set tcp failed");
			freeaddrinfo(ai_tcp);
		}
		if (tcpflag && ip6flag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_tcp6);
			if (ecode != 0)
				err(1, "getaddrinfo tcp6: %s", gai_strerror(ecode));
			nconf_tcp6 = getnetconfigent("tcp6");
			if (nconf_tcp6 == NULL)
				err(1, "getnetconfigent tcp6 failed");
			nb_tcp6.buf = ai_tcp6->ai_addr;
			nb_tcp6.len = nb_tcp6.maxlen = ai_tcp6->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_tcp6, &nb_tcp6)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_tcp6, &nb_tcp6)))
				err(1, "rpcb_set tcp6 failed");
			freeaddrinfo(ai_tcp6);
		}
		exit (0);
	}
	if (debug == 0) {
		daemon(0, 0);
		(void)signal(SIGHUP, SIG_IGN);
		(void)signal(SIGINT, SIG_IGN);
		/*
		 * nfsd sits in the kernel most of the time.  It needs
		 * to ignore SIGTERM/SIGQUIT in order to stay alive as long
		 * as possible during a shutdown, otherwise loopback
		 * mounts will not be able to unmount. 
		 */
		(void)signal(SIGTERM, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
	}
	(void)signal(SIGSYS, nonfs);
	(void)signal(SIGCHLD, reapchild);

	openlog("nfsd", LOG_PID, LOG_DAEMON);

	/* If we use UDP only, we start the last server below. */
	srvcnt = tcpflag ? nfsdcnt : nfsdcnt - 1;
	for (i = 0; i < srvcnt; i++) {
		switch ((pid = fork())) {
		case -1:
			syslog(LOG_ERR, "fork: %m");
			nfsd_exit(1);
		case 0:
			break;
		default:
			children[i] = pid;
			continue;
		}
		(void)signal(SIGUSR1, child_cleanup);
		setproctitle("server");

		start_server(0);
	}

	(void)signal(SIGUSR1, cleanup);
	FD_ZERO(&v4bits);
	FD_ZERO(&v6bits);
	FD_ZERO(&sockbits);
 
	rpcbregcnt = 0;
	/* Set up the socket for udp and rpcb register it. */
	if (udpflag) {
		rpcbreg = 0;
		for (i = 0; i < bindhostc; i++) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			if (setbindhost(&ai_udp, bindhost[i], hints) == 0) {
				rpcbreg = 1;
				rpcbregcnt++;
				if ((sock = socket(ai_udp->ai_family,
				    ai_udp->ai_socktype,
				    ai_udp->ai_protocol)) < 0) {
					syslog(LOG_ERR,
					    "can't create udp socket");
					nfsd_exit(1);
				}
				if (bind(sock, ai_udp->ai_addr,
				    ai_udp->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind udp addr %s: %m",
					    bindhost[i]);
					nfsd_exit(1);
				}
				freeaddrinfo(ai_udp);
				nfsdargs.sock = sock;
				nfsdargs.name = NULL;
				nfsdargs.namelen = 0;
				if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
					syslog(LOG_ERR, "can't Add UDP socket");
					nfsd_exit(1);
				}
				(void)close(sock);
			}
		}
		if (rpcbreg == 1) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp);
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo udp: %s",
				   gai_strerror(ecode));
				nfsd_exit(1);
			}
			nconf_udp = getnetconfigent("udp");
			if (nconf_udp == NULL)
				err(1, "getnetconfigent udp failed");
			nb_udp.buf = ai_udp->ai_addr;
			nb_udp.len = nb_udp.maxlen = ai_udp->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_udp, &nb_udp)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_udp, &nb_udp)))
				err(1, "rpcb_set udp failed");
			freeaddrinfo(ai_udp);
		}
	}

	/* Set up the socket for udp6 and rpcb register it. */
	if (udpflag && ip6flag) {
		rpcbreg = 0;
		for (i = 0; i < bindhostc; i++) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			if (setbindhost(&ai_udp6, bindhost[i], hints) == 0) {
				rpcbreg = 1;
				rpcbregcnt++;
				if ((sock = socket(ai_udp6->ai_family,
				    ai_udp6->ai_socktype,
				    ai_udp6->ai_protocol)) < 0) {
					syslog(LOG_ERR,
						"can't create udp6 socket");
					nfsd_exit(1);
				}
				if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY,
				    &on, sizeof on) < 0) {
					syslog(LOG_ERR,
					    "can't set v6-only binding for "
					    "udp6 socket: %m");
					nfsd_exit(1);
				}
				if (bind(sock, ai_udp6->ai_addr,
				    ai_udp6->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind udp6 addr %s: %m",
					    bindhost[i]);
					nfsd_exit(1);
				}
				freeaddrinfo(ai_udp6);
				nfsdargs.sock = sock;
				nfsdargs.name = NULL;
				nfsdargs.namelen = 0;
				if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
					syslog(LOG_ERR,
					    "can't add UDP6 socket");
					nfsd_exit(1);
				}
				(void)close(sock);    
			}
		}
		if (rpcbreg == 1) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_udp6);
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo udp6: %s",
				   gai_strerror(ecode));
				nfsd_exit(1);
			}
			nconf_udp6 = getnetconfigent("udp6");
			if (nconf_udp6 == NULL)
				err(1, "getnetconfigent udp6 failed");
			nb_udp6.buf = ai_udp6->ai_addr;
			nb_udp6.len = nb_udp6.maxlen = ai_udp6->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_udp6, &nb_udp6)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_udp6, &nb_udp6)))
				err(1, "rpcb_set udp6 failed");
			freeaddrinfo(ai_udp6);
		}
	}

	/* Set up the socket for tcp and rpcb register it. */
	if (tcpflag) {
		rpcbreg = 0;
		for (i = 0; i < bindhostc; i++) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			if (setbindhost(&ai_tcp, bindhost[i], hints) == 0) {
				rpcbreg = 1;
				rpcbregcnt++;
				if ((tcpsock = socket(AF_INET, SOCK_STREAM,
				    0)) < 0) {
					syslog(LOG_ERR,
					    "can't create tpc socket");
					nfsd_exit(1);
				}
				if (setsockopt(tcpsock, SOL_SOCKET,
				    SO_REUSEADDR,
				    (char *)&on, sizeof(on)) < 0)
					syslog(LOG_ERR,
					     "setsockopt SO_REUSEADDR: %m");
				if (bind(tcpsock, ai_tcp->ai_addr,
				    ai_tcp->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind tcp addr %s: %m",
					    bindhost[i]);
					nfsd_exit(1);
				}
				if (listen(tcpsock, 5) < 0) {
					syslog(LOG_ERR, "listen failed");
					nfsd_exit(1);
				}
				freeaddrinfo(ai_tcp);
				FD_SET(tcpsock, &sockbits);
				FD_SET(tcpsock, &v4bits); 
				maxsock = tcpsock;
				connect_type_cnt++;
			}
		}
		if (rpcbreg == 1) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			ecode = getaddrinfo(NULL, "nfs", &hints,
			     &ai_tcp);
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo tcp: %s",
				   gai_strerror(ecode));
				nfsd_exit(1);
			}
			nconf_tcp = getnetconfigent("tcp");
			if (nconf_tcp == NULL)
				err(1, "getnetconfigent tcp failed");
			nb_tcp.buf = ai_tcp->ai_addr;
			nb_tcp.len = nb_tcp.maxlen = ai_tcp->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_tcp,
			    &nb_tcp)) || (!rpcb_set(RPCPROG_NFS, 3,
			    nconf_tcp, &nb_tcp)))
				err(1, "rpcb_set tcp failed");
			freeaddrinfo(ai_tcp);
		}
	}

	/* Set up the socket for tcp6 and rpcb register it. */
	if (tcpflag && ip6flag) {
		rpcbreg = 0;
		for (i = 0; i < bindhostc; i++) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			if (setbindhost(&ai_tcp6, bindhost[i], hints) == 0) {
				rpcbreg = 1;
				rpcbregcnt++;
				if ((tcp6sock = socket(ai_tcp6->ai_family,
				    ai_tcp6->ai_socktype,
				    ai_tcp6->ai_protocol)) < 0) {
					syslog(LOG_ERR,
					    "can't create tcp6 socket");
					nfsd_exit(1);
				}
				if (setsockopt(tcp6sock, SOL_SOCKET,
				    SO_REUSEADDR,
				    (char *)&on, sizeof(on)) < 0)
					syslog(LOG_ERR,
					    "setsockopt SO_REUSEADDR: %m");
				if (setsockopt(tcp6sock, IPPROTO_IPV6,
				    IPV6_V6ONLY, &on, sizeof on) < 0) {
					syslog(LOG_ERR,
					"can't set v6-only binding for tcp6 "
					    "socket: %m");
					nfsd_exit(1);
				}
				if (bind(tcp6sock, ai_tcp6->ai_addr,
				    ai_tcp6->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind tcp6 addr %s: %m",
					    bindhost[i]);
					nfsd_exit(1);
				}
				if (listen(tcp6sock, 5) < 0) {
					syslog(LOG_ERR, "listen failed");
					nfsd_exit(1);
				}
				freeaddrinfo(ai_tcp6);
				FD_SET(tcp6sock, &sockbits);
				FD_SET(tcp6sock, &v6bits);
				if (maxsock < tcp6sock)
					maxsock = tcp6sock;
				connect_type_cnt++;
			}
		}
		if (rpcbreg == 1) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_protocol = IPPROTO_TCP;
			ecode = getaddrinfo(NULL, "nfs", &hints, &ai_tcp6);
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo tcp6: %s",
				   gai_strerror(ecode));
				nfsd_exit(1);
			}
			nconf_tcp6 = getnetconfigent("tcp6");
			if (nconf_tcp6 == NULL)
				err(1, "getnetconfigent tcp6 failed");
			nb_tcp6.buf = ai_tcp6->ai_addr;
			nb_tcp6.len = nb_tcp6.maxlen = ai_tcp6->ai_addrlen;
			if ((!rpcb_set(RPCPROG_NFS, 2, nconf_tcp6, &nb_tcp6)) ||
			    (!rpcb_set(RPCPROG_NFS, 3, nconf_tcp6, &nb_tcp6)))
				err(1, "rpcb_set tcp6 failed");
			freeaddrinfo(ai_tcp6);
		}
	}

	if (rpcbregcnt == 0) {
		syslog(LOG_ERR, "rpcb_set() failed, nothing to do: %m");
		nfsd_exit(1);
	}

	if (tcpflag && connect_type_cnt == 0) {
		syslog(LOG_ERR, "tcp connects == 0, nothing to do: %m");
		nfsd_exit(1);
	}

	setproctitle("master");
	/*
	 * We always want a master to have a clean way to to shut nfsd down
	 * (with unregistration): if the master is killed, it unregisters and
	 * kills all children. If we run for UDP only (and so do not have to
	 * loop waiting waiting for accept), we instead make the parent
	 * a "server" too. start_server will not return.
	 */
	if (!tcpflag)
		start_server(1);

	/*
	 * Loop forever accepting connections and passing the sockets
	 * into the kernel for the mounts.
	 */
	for (;;) {
		ready = sockbits;
		if (connect_type_cnt > 1) {
			if (select(maxsock + 1,
			    &ready, NULL, NULL, NULL) < 1) {
				syslog(LOG_ERR, "select failed: %m");
				if (errno == EINTR)
					continue;
				nfsd_exit(1);
			}
		}
		for (tcpsock = 0; tcpsock <= maxsock; tcpsock++) {
			if (FD_ISSET(tcpsock, &ready)) {
				if (FD_ISSET(tcpsock, &v4bits)) {
					len = sizeof(inetpeer);
					if ((msgsock = accept(tcpsock,
					    (struct sockaddr *)&inetpeer, &len)) < 0) {
						syslog(LOG_ERR, "accept failed: %m");
						if (errno == ECONNABORTED ||
						    errno == EINTR)
							continue;
						nfsd_exit(1);
					}
					memset(inetpeer.sin_zero, 0,
						sizeof(inetpeer.sin_zero));
					if (setsockopt(msgsock, SOL_SOCKET,
					    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
						syslog(LOG_ERR,
						    "setsockopt SO_KEEPALIVE: %m");
					nfsdargs.sock = msgsock;
					nfsdargs.name = (caddr_t)&inetpeer;
					nfsdargs.namelen = len;
					nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
					(void)close(msgsock);
				} else if (FD_ISSET(tcpsock, &v6bits)) {
					len = sizeof(inet6peer);
					if ((msgsock = accept(tcpsock,
					    (struct sockaddr *)&inet6peer,
					    &len)) < 0) {
						syslog(LOG_ERR,
						     "accept failed: %m");
						if (errno == ECONNABORTED ||
						    errno == EINTR)
							continue;
						nfsd_exit(1);
					}
					if (setsockopt(msgsock, SOL_SOCKET,
					    SO_KEEPALIVE, (char *)&on,
					    sizeof(on)) < 0)
						syslog(LOG_ERR, "setsockopt "
						    "SO_KEEPALIVE: %m");
					nfsdargs.sock = msgsock;
					nfsdargs.name = (caddr_t)&inet6peer;
					nfsdargs.namelen = len;
					nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
					(void)close(msgsock);
				}
			}
		}
	}
}

int
setbindhost(struct addrinfo **ai, const char *bindhost, struct addrinfo hints)
{
	int ecode;
	u_int32_t host_addr[4];  /* IPv4 or IPv6 */
	const char *hostptr;

	if (bindhost == NULL || strcmp("*", bindhost) == 0)
		hostptr = NULL;
	else
		hostptr = bindhost;

	if (hostptr != NULL) {
		switch (hints.ai_family) {
		case AF_INET:
			if (inet_pton(AF_INET, hostptr, host_addr) == 1) {
				hints.ai_flags = AI_NUMERICHOST;
			} else {
				if (inet_pton(AF_INET6, hostptr,
				    host_addr) == 1)
					return (1);
			}
			break;
		case AF_INET6:
			if (inet_pton(AF_INET6, hostptr, host_addr) == 1) {
				hints.ai_flags = AI_NUMERICHOST;
			} else {
				if (inet_pton(AF_INET, hostptr,
				    host_addr) == 1)
					return (1);
			}
			break;
		default:
			break;
		}
	}
	
	ecode = getaddrinfo(hostptr, "nfs", &hints, ai);
	if (ecode != 0) {
		syslog(LOG_ERR, "getaddrinfo %s: %s", bindhost,
		    gai_strerror(ecode));
		return (1);
	}
	return (0);
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
	syslog(LOG_ERR, "missing system call: NFS not available");
}

void
reapchild(signo)
	int signo;
{
	pid_t pid;
	int i;

	while ((pid = wait3(NULL, WNOHANG, NULL)) > 0) {
		for (i = 0; i < nfsdcnt; i++)
			if (pid == children[i])
				children[i] = -1;
	}
}

void
unregistration()
{
	if ((!rpcb_unset(RPCPROG_NFS, 2, NULL)) ||
	    (!rpcb_unset(RPCPROG_NFS, 3, NULL)))
		syslog(LOG_ERR, "rpcb_unset failed");
}

void
killchildren()
{
	int i;

	for (i = 0; i < nfsdcnt; i++) {
		if (children[i] > 0)
			kill(children[i], SIGKILL);
	}
}

/*
 * Cleanup master after SIGUSR1.
 */
void
cleanup(signo)
{
	nfsd_exit(0);
}

/*
 * Cleanup child after SIGUSR1.
 */
void
child_cleanup(signo)
{
	exit(0);
}

void
nfsd_exit(int status)
{
	killchildren();
	unregistration();
	exit(status);
}

void
start_server(int master)
{
	int status;

	status = 0;
	if (nfssvc(NFSSVC_NFSD, NULL) < 0) {
		syslog(LOG_ERR, "nfssvc: %m");
		status = 1;
	}
	if (master)
		nfsd_exit(status);
	else
		exit(status);
}
