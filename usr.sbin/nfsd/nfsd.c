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
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif not lint

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsd.c	8.9 (Berkeley) 3/29/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif not lint

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#ifdef NFSKERB
#include <kerberosIV/des.h>
#include <kerberosIV/krb.h>
#endif

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <netdb.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s)	fprintf(stderr,(s))
int	debug = 1;
#else
int	debug = 0;
#endif

struct	nfsd_srvargs nsd;
#ifdef OLD_SETPROCTITLE
char	**Argv = NULL;		/* pointer to argument vector */
char	*LastArg = NULL;	/* end of argv */
#endif

#ifdef NFSKERB
char		lnam[ANAME_SZ];
KTEXT_ST	kt;
AUTH_DAT	kauth;
char		inst[INST_SZ];
struct nfsrpc_fullblock kin, kout;
struct nfsrpc_fullverf kverf;
NFSKERBKEY_T	kivec;
struct timeval	ktv;
NFSKERBKEYSCHED_T kerb_keysched;
#endif

#define	MAXNFSDCNT	20
#define	DEFNFSDCNT	 4
pid_t	children[MAXNFSDCNT];	/* PIDs of children */
int	nfsdcnt;		/* number of children */

void	cleanup(int);
void	killchildren(void);
void	nonfs (int);
void	reapchild (int);
int	setbindhost (struct addrinfo **ia, const char *bindhost, struct addrinfo hints);
#ifdef OLD_SETPROCTITLE
#ifdef __FreeBSD__
void	setproctitle (char *);
#endif
#endif
void	unregistration (void);
void	usage (void);

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
 *	-c - support iso cltp clients
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
	int ch, cltpflag, connect_type_cnt, i, len, maxsock, msgsock;
	int nfssvc_flag, on = 1, unregister, reregister, sock;
	int tcp6sock, ip6flag, tcpflag, tcpsock;
	int udpflag, ecode, s;
	int bindhostc = 0, bindanyflag, rpcbreg, rpcbregcnt;
	char **bindhost = NULL;
	pid_t pid;
#ifdef NFSKERB
	struct group *grp;
	struct passwd *pwd;
	struct ucred *cr;
	struct timeval ktv;
	char **cpp;
#endif
#ifdef __FreeBSD__
	struct vfsconf vfc;
	int error;

	error = getvfsbyname("nfs", &vfc);
	if (error && vfsisloadable("nfs")) {
		if (vfsload("nfs"))
			err(1, "vfsload(nfs)");
		endvfsent();	/* flush cache */
		error = getvfsbyname("nfs", &vfc);
	}
	if (error)
		errx(1, "NFS is not available in the running kernel");
#endif

#ifdef OLD_SETPROCTITLE
	/* Save start and extent of argv for setproctitle. */
	Argv = argv;
	if (envp == 0 || *envp == 0)
		envp = argv;
	while (*envp)
		envp++;
	LastArg = envp[-1] + strlen(envp[-1]);
#endif

	nfsdcnt = DEFNFSDCNT;
	cltpflag = unregister = reregister = tcpflag = 0;
	bindanyflag = udpflag = ip6flag = 0;
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
	if (s < 0 && (errno == EPROTONOSUPPORT ||
	   errno == EPFNOSUPPORT || errno == EAFNOSUPPORT) ||
	   (getnetconfigent("udp6") == NULL && getnetconfigent("tcp6") == NULL))
		ip6flag = 0;
	else
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

	if (debug == 0) {
		daemon(0, 0);
		(void)signal(SIGHUP, SIG_IGN);
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGSYS, nonfs);
		(void)signal(SIGUSR1, cleanup);
		/*
		 * nfsd sits in the kernel most of the time.  It needs
		 * to ignore SIGTERM/SIGQUIT in order to stay alive as long
		 * as possible during a shutdown, otherwise loopback
		 * mounts will not be able to unmount. 
		 */
		(void)signal(SIGTERM, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
	}
	(void)signal(SIGCHLD, reapchild);
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
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo udp6: %s",
				   gai_strerror(ecode));
				exit(1);
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
		if (tcpflag) {
			memset(&hints, 0, sizeof hints);
			hints.ai_flags = AI_PASSIVE;
			hints.ai_family = AF_INET;
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
			if (ecode != 0) {
				syslog(LOG_ERR, "getaddrinfo tcp6: %s",
				   gai_strerror(ecode));
				exit(1);
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
		exit (0);
	}

	openlog("nfsd:", LOG_PID, LOG_DAEMON);

	for (i = 0; i < nfsdcnt; i++) {
		switch ((pid = fork())) {
		case -1:
			syslog(LOG_ERR, "fork: %m");
			killchildren();
			exit (1);
		case 0:
			break;
		default:
			children[i] = pid;
			continue;
		}

		setproctitle("server");
		nfssvc_flag = NFSSVC_NFSD;
		nsd.nsd_nfsd = NULL;
#ifdef NFSKERB
		if (sizeof (struct nfsrpc_fullverf) != RPCX_FULLVERF ||
		    sizeof (struct nfsrpc_fullblock) != RPCX_FULLBLOCK)
		    syslog(LOG_ERR, "Yikes NFSKERB structs not packed!");
		nsd.nsd_authstr = (u_char *)&kt;
		nsd.nsd_authlen = sizeof (kt);
		nsd.nsd_verfstr = (u_char *)&kverf;
		nsd.nsd_verflen = sizeof (kverf);
#endif
		while (nfssvc(nfssvc_flag, &nsd) < 0) {
			if (errno != ENEEDAUTH) {
				syslog(LOG_ERR, "nfssvc: %m");
				exit(1);
			}
			nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHINFAIL;
#ifdef NFSKERB
			/*
			 * Get the Kerberos ticket out of the authenticator
			 * verify it and convert the principal name to a user
			 * name. The user name is then converted to a set of
			 * user credentials via the password and group file.
			 * Finally, decrypt the timestamp and validate it.
			 * For more info see the IETF Draft "Authentication
			 * in ONC RPC".
			 */
			kt.length = ntohl(kt.length);
			if (gettimeofday(&ktv, (struct timezone *)0) == 0 &&
			    kt.length > 0 && kt.length <=
			    (RPCAUTH_MAXSIZ - 3 * NFSX_UNSIGNED)) {
			    kin.w1 = NFS_KERBW1(kt);
			    kt.mbz = 0;
			    (void)strcpy(inst, "*");
			    if (krb_rd_req(&kt, NFS_KERBSRV,
				inst, nsd.nsd_haddr, &kauth, "") == RD_AP_OK &&
				krb_kntoln(&kauth, lnam) == KSUCCESS &&
				(pwd = getpwnam(lnam)) != NULL) {
				cr = &nsd.nsd_cr;
				cr->cr_uid = pwd->pw_uid;
				cr->cr_groups[0] = pwd->pw_gid;
				cr->cr_ngroups = 1;
				setgrent();
				while ((grp = getgrent()) != NULL) {
					if (grp->gr_gid == cr->cr_groups[0])
						continue;
					for (cpp = grp->gr_mem;
					    *cpp != NULL; ++cpp)
						if (!strcmp(*cpp, lnam))
							break;
					if (*cpp == NULL)
						continue;
					cr->cr_groups[cr->cr_ngroups++]
					    = grp->gr_gid;
					if (cr->cr_ngroups == NGROUPS)
						break;
				}
				endgrent();

				/*
				 * Get the timestamp verifier out of the
				 * authenticator and verifier strings.
				 */
				kin.t1 = kverf.t1;
				kin.t2 = kverf.t2;
				kin.w2 = kverf.w2;
				bzero((caddr_t)kivec, sizeof (kivec));
				bcopy((caddr_t)kauth.session,
				    (caddr_t)nsd.nsd_key,sizeof(kauth.session));

				/*
				 * Decrypt the timestamp verifier in CBC mode.
				 */
				XXX

				/*
				 * Validate the timestamp verifier, to
				 * check that the session key is ok.
				 */
				nsd.nsd_timestamp.tv_sec = ntohl(kout.t1);
				nsd.nsd_timestamp.tv_usec = ntohl(kout.t2);
				nsd.nsd_ttl = ntohl(kout.w1);
				if ((nsd.nsd_ttl - 1) == ntohl(kout.w2))
				    nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHIN;
			}
#endif /* NFSKERB */
		}
		exit(0);
	}

	if (atexit(killchildren) == -1) {
 		syslog(LOG_ERR, "atexit: %s", strerror(errno));
 		exit(1);
 	}
	FD_ZERO(&v4bits);
	FD_ZERO(&v6bits);
 
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
					exit(1);
				}
				if (bind(sock, ai_udp->ai_addr,
				    ai_udp->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind udp addr %s: %m",
					    bindhost[i]);
					exit(1);
				}
				freeaddrinfo(ai_udp);
				nfsdargs.sock = sock;
				nfsdargs.name = NULL;
				nfsdargs.namelen = 0;
				if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
					syslog(LOG_ERR, "can't Add UDP socket");
					exit(1);
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
				exit(1);
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
					exit(1);
				}
				if (setsockopt(sock, IPPROTO_IPV6,
				    IPV6_BINDV6ONLY,
				    &on, sizeof on) < 0) {
					syslog(LOG_ERR,
					    "can't set v6-only binding for "
					    "udp6 socket: %m");
					exit(1);
				}
				if (bind(sock, ai_udp6->ai_addr,
				    ai_udp6->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind udp6 addr %s: %m",
					    bindhost[i]);
					exit(1);
				}
				freeaddrinfo(ai_udp6);
				nfsdargs.sock = sock;
				nfsdargs.name = NULL;
				nfsdargs.namelen = 0;
				if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
					syslog(LOG_ERR,
					    "can't add UDP6 socket");
					exit(1);
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
				exit(1);
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
					exit(1);
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
					exit(1);
				}
				if (listen(tcpsock, 5) < 0) {
					syslog(LOG_ERR, "listen failed");
					exit(1);
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
				exit(1);
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
					exit(1);
				}
				if (setsockopt(tcp6sock, SOL_SOCKET,
				    SO_REUSEADDR,
				    (char *)&on, sizeof(on)) < 0)
					syslog(LOG_ERR,
					    "setsockopt SO_REUSEADDR: %m");
				if (setsockopt(tcp6sock, IPPROTO_IPV6,
				    IPV6_BINDV6ONLY, &on, sizeof on) < 0) {
					syslog(LOG_ERR,
					"can't set v6-only binding for tcp6 "
					    "socket: %m");
					exit(1);
				}
				if (bind(tcp6sock, ai_tcp6->ai_addr,
				    ai_tcp6->ai_addrlen) < 0) {
					syslog(LOG_ERR,
					    "can't bind tcp6 addr %s: %m",
					    bindhost[i]);
					exit(1);
				}
				if (listen(tcp6sock, 5) < 0) {
					syslog(LOG_ERR, "listen failed");
					exit(1);
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
				exit(1);
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
		exit(1);
	}

	if ((tcpflag) && (connect_type_cnt == 0)) {
		syslog(LOG_ERR, "tcp connects == 0, nothing to do: %m");
		exit(1);
	}

	setproctitle("master");

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
				exit(1);
			}
		}
		for (tcpsock = 0; tcpsock <= maxsock; tcpsock++) {
			if (FD_ISSET(tcpsock, &ready)) {
				if (FD_ISSET(tcpsock, &v4bits)) {
					len = sizeof(inetpeer);
					if ((msgsock = accept(tcpsock,
					    (struct sockaddr *)&inetpeer, &len)) < 0) {
						syslog(LOG_ERR, "accept failed: %m");
						exit(1);
					}
					memset(inetpeer.sin_zero, 0,
						sizeof(inetpeer.sin_zero));
					if (setsockopt(msgsock, SOL_SOCKET,
					    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
						syslog(LOG_ERR,
						    "setsockopt SO_KEEPALIVE: %m");
					nfsdargs.sock = msgsock;
					nfsdargs.name = (caddr_t)&inetpeer;
					nfsdargs.namelen = sizeof(inetpeer);
					nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
					(void)close(msgsock);
				} else if (FD_ISSET(tcpsock, &v6bits)) {
					len = sizeof(inet6peer);
					if ((msgsock = accept(tcpsock,
					    (struct sockaddr *)&inet6peer,
					    &len)) < 0) {
						syslog(LOG_ERR,
						     "accept failed: %m");
						exit(1);
					}
					if (setsockopt(msgsock, SOL_SOCKET,
					    SO_KEEPALIVE, (char *)&on,
					    sizeof(on)) < 0)
						syslog(LOG_ERR, "setsockopt "
						    "SO_KEEPALIVE: %m");
					nfsdargs.sock = msgsock;
					nfsdargs.name = (caddr_t)&inet6peer;
					nfsdargs.namelen = sizeof(inet6peer);
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

	while (wait3(NULL, WNOHANG, NULL) > 0);
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
	sigset_t sigs;

	sigemptyset(&sigs);
	/*
	* Block SIGCHLD to avoid killing a reaped process (although it is
	* unlikely, the pid might have been reused).
	*/
	sigaddset(&sigs, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &sigs, NULL) == -1) {
		syslog(LOG_ERR, "sigprocmask: %s",
		   strerror(errno));
		return;
	}
	for (i = 0; i < nfsdcnt; i++) {
		if (children[i] > 0)
			kill(children[i], SIGKILL);
	}
	if (sigprocmask(SIG_UNBLOCK, &sigs, NULL) == -1) {
		syslog(LOG_ERR, "sigprocmask: %s", strerror(errno));
	}
	unregistration();
}

void
cleanup(signo)
{
	killchildren();
	exit (0);
}

#ifdef OLD_SETPROCTITLE
#ifdef __FreeBSD__
void
setproctitle(a)
	char *a;
{
	register char *cp;
	char buf[80];

	cp = Argv[0];
	(void)snprintf(buf, sizeof(buf), "nfsd-%s", a);
	(void)strncpy(cp, buf, LastArg - cp);
	cp += strlen(cp);
	while (cp < LastArg)
		*cp++ = '\0';
	Argv[1] = NULL;
}
#endif	/* __FreeBSD__ */
#endif
