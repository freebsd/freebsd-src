/* 	$NetBSD: mountd.c,v 1.8 2013/10/19 17:45:00 christos Exp $	 */

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Herb Hasler and Rick Macklem at The University of Guelph.
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
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif				/* not lint */

#ifndef lint
#if 0
static char     sccsid[] = "@(#)mountd.c  8.15 (Berkeley) 5/1/95";
#else
__RCSID("$NetBSD: mountd.c,v 1.8 2013/10/19 17:45:00 christos Exp $");
#endif
#endif				/* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <sys/ucred.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/mount.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <nfs/nfsmount.h>

#include <arpa/inet.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <netgroup.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <util.h>
#include "pathnames.h"

#ifdef IPSEC
#include <netinet6/ipsec.h>
#ifndef IPSEC_POLICY_IPSEC	/* no ipsec support on old ipsec */
#undef IPSEC
#endif
#include "ipsec.h"
#endif

#include "svc_fdset.h"

#include <stdarg.h>

/*
 * Structures for keeping the mount list and export list
 */
struct mountlist {
	struct mountlist *ml_next;
	char ml_host[RPCMNT_NAMELEN + 1];
	char ml_dirp[RPCMNT_PATHLEN + 1];
	int ml_flag;/* XXX more flags (same as dp_flag) */
};

struct dirlist {
	struct dirlist *dp_left;
	struct dirlist *dp_right;
	int dp_flag;
	struct hostlist *dp_hosts;	/* List of hosts this dir exported to */
	char dp_dirp[1];		/* Actually malloc'd to size of dir */
};
/* dp_flag bits */
#define	DP_DEFSET	0x1
#define DP_HOSTSET	0x2
#define DP_KERB		0x4
#define DP_NORESMNT	0x8

struct exportlist {
	struct exportlist *ex_next;
	struct dirlist *ex_dirl;
	struct dirlist *ex_defdir;
	int             ex_flag;
	fsid_t          ex_fs;
	char           *ex_fsdir;
	char           *ex_indexfile;
};
/* ex_flag bits */
#define	EX_LINKED	0x1

struct netmsk {
	struct sockaddr_storage nt_net;
	int		nt_len;
	char           *nt_name;
};

union grouptypes {
	struct addrinfo *gt_addrinfo;
	struct netmsk   gt_net;
};

struct grouplist {
	int             gr_type;
	union grouptypes gr_ptr;
	struct grouplist *gr_next;
};
/* Group types */
#define	GT_NULL		0x0
#define	GT_HOST		0x1
#define	GT_NET		0x2

struct hostlist {
	int             ht_flag;/* Uses DP_xx bits */
	struct grouplist *ht_grp;
	struct hostlist *ht_next;
};

struct fhreturn {
	int             fhr_flag;
	int             fhr_vers;
	size_t		fhr_fhsize;
	union {
		uint8_t v2[NFSX_V2FH];
		uint8_t v3[NFSX_V3FHMAX];
	} fhr_fh;
};

/* Global defs */
static char    *add_expdir __P((struct dirlist **, char *, int));
static void add_dlist __P((struct dirlist **, struct dirlist *,
    struct grouplist *, int));
static void add_mlist __P((char *, char *, int));
static int check_dirpath __P((const char *, size_t, char *));
static int check_options __P((const char *, size_t, struct dirlist *));
static int chk_host __P((struct dirlist *, struct sockaddr *, int *, int *));
static int del_mlist __P((char *, char *, struct sockaddr *));
static struct dirlist *dirp_search __P((struct dirlist *, char *));
static int do_nfssvc __P((const char *, size_t, struct exportlist *,
    struct grouplist *, int, struct uucred *, char *, int, struct statvfs *));
static int do_opt __P((const char *, size_t, char **, char **,
    struct exportlist *, struct grouplist *, int *, int *, struct uucred *));
static struct exportlist *ex_search __P((fsid_t *));
static int parse_directory __P((const char *, size_t, struct grouplist *,
    int, char *, struct exportlist **, struct statvfs *));
static int parse_host_netgroup __P((const char *, size_t, struct exportlist *,
    struct grouplist *, char *, int *, struct grouplist **));
static struct exportlist *get_exp __P((void));
static void free_dir __P((struct dirlist *));
static void free_exp __P((struct exportlist *));
static void free_grp __P((struct grouplist *));
static void free_host __P((struct hostlist *));
void get_exportlist __P((int));
static int get_host __P((const char *, size_t, const char *,
    struct grouplist *));
static struct hostlist *get_ht __P((void));
static void get_mountlist __P((void));
static int get_net __P((char *, struct netmsk *, int));
static void free_exp_grp __P((struct exportlist *, struct grouplist *));
static struct grouplist *get_grp __P((void));
static void hang_dirp __P((struct dirlist *, struct grouplist *,
    struct exportlist *, int));
static void mntsrv __P((struct svc_req *, SVCXPRT *));
static void nextfield __P((char **, char **));
static void parsecred __P((char *, struct uucred *));
static int put_exlist __P((struct dirlist *, XDR *, struct dirlist *, int *));
static int scan_tree __P((struct dirlist *, struct sockaddr *));
static void send_umntall __P((int));
#if 0
static int umntall_each __P((caddr_t, struct sockaddr_in *));
#endif
static int xdr_dir __P((XDR *, char *));
static int xdr_explist __P((XDR *, caddr_t));
static int xdr_fhs __P((XDR *, caddr_t));
static int xdr_mlist __P((XDR *, caddr_t));
static int bitcmp __P((void *, void *, int));
static int netpartcmp __P((struct sockaddr *, struct sockaddr *, int));
static int sacmp __P((struct sockaddr *, struct sockaddr *));
static int allones __P((struct sockaddr_storage *, int));
static int countones __P((struct sockaddr *));
static void bind_resv_port __P((int, sa_family_t, in_port_t));
static void no_nfs(int);
static struct exportlist *exphead;
static struct mountlist *mlhead;
static struct grouplist *grphead;
static char    *exname;
static struct uucred def_anon = {
	1,
	(uid_t) -2,
	(gid_t) -2,
	0,
	{ 0 }
};

static int      opt_flags;
static int	have_v6 = 1;
static const int ninumeric = NI_NUMERICHOST;

/* Bits for above */
#define	OP_MAPROOT	0x001
#define	OP_MAPALL	0x002
#define	OP_KERB		0x004
#define	OP_MASK		0x008
#define	OP_NET		0x010
#define	OP_ALLDIRS	0x040
#define OP_NORESPORT	0x080
#define OP_NORESMNT	0x100
#define OP_MASKLEN	0x200

static int      debug = 1;
#if 0
static void SYSLOG __P((int, const char *,...));
#endif
int main __P((int, char *[]));

/*
 * If this is non-zero, -noresvport and -noresvmnt are implied for
 * each export.
 */
static int noprivports;

#define C2FD(_c_) ((int)(uintptr_t)(_c_))
static int
rumpread(void *cookie, char *buf, int count)
{

	return rump_sys_read(C2FD(cookie), buf, count);
}

static int
rumpwrite(void *cookie, const char *buf, int count)
{

	return rump_sys_write(C2FD(cookie), buf, count);
}

static off_t
rumpseek(void *cookie, off_t off, int whence)
{

	return rump_sys_lseek(C2FD(cookie), off, whence);
}

static int
rumpclose(void *cookie)
{

	return rump_sys_close(C2FD(cookie));
}

int __sflags(const char *, int *); /* XXX */
static FILE *
rumpfopen(const char *path, const char *opts)
{
	int fd, oflags;

	__sflags(opts, &oflags);
	fd = rump_sys_open(path, oflags, 0777);
	if (fd == -1)
		return NULL;

	return funopen((void *)(uintptr_t)fd,
	    rumpread, rumpwrite, rumpseek, rumpclose);
}

/*
 * Make sure mountd signal handler is executed from a thread context
 * instead of the signal handler.  This avoids the signal handler
 * ruining our kernel context.
 */
static sem_t exportsem;
static void
signal_get_exportlist(int sig)
{

	sem_post(&exportsem);
}

static void *
exportlist_thread(void *arg)
{

	for (;;) {
		sem_wait(&exportsem);
		get_exportlist(0);
	}

	return NULL;
}

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * "-d" to enable debugging
 * and "-n" to allow nonroot mount.
 */
void *mountd_main(void *);
void *
mountd_main(void *arg)
{
	SVCXPRT *udptransp, *tcptransp;
	struct netconfig *udpconf, *tcpconf;
	int udpsock, tcpsock;
	int xcreated = 0;
	int maxrec = RPC_MAXDATASIZE;
	in_port_t forcedport = 0;
	extern sem_t gensem;
	pthread_t ptdummy;

	alloc_fdset();

#if 0
	while ((c = getopt(argc, argv, "dNnrp:" ADDOPTS)) != -1)
		switch (c) {
#ifdef IPSEC
		case 'P':
			if (ipsecsetup_test(policy = optarg))
				errx(1, "Invalid ipsec policy `%s'", policy);
			break;
#endif
		case 'p':
			/* A forced port "0" will dynamically allocate a port */
			forcedport = atoi(optarg);
			break;
		case 'd':
			debug = 1;
			break;
		case 'N':
			noprivports = 1;
			break;
			/* Compatibility */
		case 'n':
		case 'r':
			break;
		default:
			fprintf(stderr, "usage: %s [-dNn]"
#ifdef IPSEC
			    " [-P policy]"
#endif
			    " [-p port] [exportsfile]\n", getprogname());
			exit(1);
		};
	argc -= optind;
	argv += optind;
#endif

	sem_init(&exportsem, 0, 0);
	pthread_create(&ptdummy, NULL, exportlist_thread, NULL);

	grphead = NULL;
	exphead = NULL;
	mlhead = NULL;
	exname = _PATH_EXPORTS;
	openlog("mountd", LOG_PID | (debug ? LOG_PERROR : 0), LOG_DAEMON);
	(void)signal(SIGSYS, no_nfs);

	if (debug)
		(void)fprintf(stderr, "Getting export list.\n");
	get_exportlist(0);
	if (debug)
		(void)fprintf(stderr, "Getting mount list.\n");
	get_mountlist();
	if (debug)
		(void)fprintf(stderr, "Here we go.\n");
	if (debug == 0) {
		daemon(0, 0);
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
	}
	(void)signal(SIGHUP, signal_get_exportlist);
	(void)signal(SIGTERM, send_umntall);
	pidfile(NULL);

	rpcb_unset(RPCPROG_MNT, RPCMNT_VER1, NULL);
	rpcb_unset(RPCPROG_MNT, RPCMNT_VER3, NULL);

	udpsock  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	tcpsock  = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	udpconf  = getnetconfigent("udp");
	tcpconf  = getnetconfigent("tcp");

	rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrec);

	if (udpsock != -1 && udpconf != NULL) {
		bind_resv_port(udpsock, AF_INET, forcedport);
#ifdef IPSEC
		if (policy)
			ipsecsetup(AF_INET, udpsock, policy);
#endif
		udptransp = svc_dg_create(udpsock, 0, 0);
		if (udptransp != NULL) {
			if (!svc_reg(udptransp, RPCPROG_MNT, RPCMNT_VER1,
				mntsrv, udpconf) ||
			    !svc_reg(udptransp, RPCPROG_MNT, RPCMNT_VER3,
				mntsrv, udpconf)) {
				syslog(LOG_WARNING, "can't register UDP service");
			}
			else {
				xcreated++;
			}
		} else {
			syslog(LOG_WARNING, "can't create UDP service");
		}
			
	}

	if (tcpsock != -1 && tcpconf != NULL) {
		bind_resv_port(tcpsock, AF_INET, forcedport);
#ifdef IPSEC
		if (policy)
			ipsecsetup(AF_INET, tcpsock, policy);
#endif
		listen(tcpsock, SOMAXCONN);
		tcptransp = svc_vc_create(tcpsock, RPC_MAXDATASIZE,
		    RPC_MAXDATASIZE);
		if (tcptransp != NULL) {
			if (!svc_reg(tcptransp, RPCPROG_MNT, RPCMNT_VER1,
				mntsrv, tcpconf) ||
			    !svc_reg(tcptransp, RPCPROG_MNT, RPCMNT_VER3,
				mntsrv, tcpconf))
				syslog(LOG_WARNING, "can't register TCP service");
			else
				xcreated++;
		} else
			syslog(LOG_WARNING, "can't create TCP service");

	}

	if (xcreated == 0) {
		syslog(LOG_ERR, "could not create any services");
		exit(1);
	}

	sem_post(&gensem);
	svc_run();
	syslog(LOG_ERR, "Mountd died");
	exit(1);
}

/*
 * The mount rpc service
 */
void
mntsrv(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct exportlist *ep;
	struct dirlist *dp;
	struct fhreturn fhr;
	struct stat     stb;
	struct statvfs   fsb;
	char host[NI_MAXHOST], numerichost[NI_MAXHOST];
	int lookup_failed = 1;
	struct sockaddr *saddr;
	u_short         sport;
	char            rpcpath[RPCMNT_PATHLEN + 1], dpath[MAXPATHLEN];
	long            bad = EACCES;
	int             defset, hostset, ret;
	sigset_t        sighup_mask;
	struct sockaddr_in6 *sin6;
	struct sockaddr_in *sin;
	size_t fh_size;
	int error = 0;

	(void)sigemptyset(&sighup_mask);
	(void)sigaddset(&sighup_mask, SIGHUP);
	saddr = svc_getrpccaller(transp)->buf;
	switch (saddr->sa_family) {
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)saddr;
		sport = ntohs(sin6->sin6_port);
		break;
	case AF_INET:
		sin = (struct sockaddr_in *)saddr;
		sport = ntohs(sin->sin_port);
		break;
	default:
		syslog(LOG_ERR, "request from unknown address family");
		return;
	}
	lookup_failed = getnameinfo(saddr, saddr->sa_len, host, sizeof host, 
	    NULL, 0, 0);
	if (getnameinfo(saddr, saddr->sa_len, numerichost,
	    sizeof numerichost, NULL, 0, ninumeric) != 0)
		strlcpy(numerichost, "?", sizeof(numerichost));
	ret = 0;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case MOUNTPROC_MNT:
		if (debug)
			fprintf(stderr,
			    "got mount request from %s\n", numerichost);
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			if (debug)
				fprintf(stderr, "-> garbage args\n");
			svcerr_decode(transp);
			return;
		}
		if (debug)
			fprintf(stderr,
			    "-> rpcpath: %s\n", rpcpath);
		/*
		 * Get the real pathname and make sure it is a file or
		 * directory that exists.
		 */
#if 0
		if (realpath(rpcpath, dpath) == 0 ||
#endif
		strcpy(dpath, rpcpath);
		if (rump_sys_stat(dpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) && !S_ISREG(stb.st_mode)) ||
		    rump_sys_statvfs1(dpath, &fsb, ST_WAIT) < 0) {
			(void)chdir("/"); /* Just in case realpath doesn't */
			if (debug)
				(void)fprintf(stderr, "-> stat failed on %s\n",
				    dpath);
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t) &bad))
				syslog(LOG_ERR, "Can't send reply");
			return;
		}
		if (debug)
			fprintf(stderr,
			    "-> dpath: %s\n", dpath);
		/* Check in the exports list */
		(void)sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		ep = ex_search(&fsb.f_fsidx);
		hostset = defset = 0;
		if (ep && (chk_host(ep->ex_defdir, saddr, &defset,
		   &hostset) || ((dp = dirp_search(ep->ex_dirl, dpath)) &&
		   chk_host(dp, saddr, &defset, &hostset)) ||
		   (defset && scan_tree(ep->ex_defdir, saddr) == 0 &&
		   scan_tree(ep->ex_dirl, saddr) == 0))) {
			if ((hostset & DP_HOSTSET) == 0) {
				hostset = defset;
			}
			if (sport >= IPPORT_RESERVED &&
			    !(hostset & DP_NORESMNT)) {
				syslog(LOG_NOTICE,
				    "Refused mount RPC from host %s port %d",
				    numerichost, sport);
				svcerr_weakauth(transp);
				goto out;
			}
			fhr.fhr_flag = hostset;
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(fhr.fhr_fh)); /* for v2 */
			fh_size = sizeof(fhr.fhr_fh);
			error = 0;
			if (rump_sys_getfh(dpath, &fhr.fhr_fh, &fh_size) < 0) {
				bad = error;
				//syslog(LOG_ERR, "Can't get fh for %s %d %d", dpath, error, fh_size);
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long,
				    (char *)&bad))
					syslog(LOG_ERR, "Can't send reply");
				goto out;
			}
			if ((fhr.fhr_vers == 1 && fh_size > NFSX_V2FH) ||
			    fh_size > NFSX_V3FHMAX) {
				bad = EINVAL; /* XXX */
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long,
				    (char *)&bad))
					syslog(LOG_ERR, "Can't send reply");
				goto out;
			}
			fhr.fhr_fhsize = fh_size;
			if (!svc_sendreply(transp, (xdrproc_t)xdr_fhs, (char *) &fhr))
				syslog(LOG_ERR, "Can't send reply");
			if (!lookup_failed)
				add_mlist(host, dpath, hostset);
			else
				add_mlist(numerichost, dpath, hostset);
			if (debug)
				(void)fprintf(stderr, "Mount successful.\n");
		} else {
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t) &bad))
				syslog(LOG_ERR, "Can't send reply");
		}
out:
		(void)sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case MOUNTPROC_DUMP:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_mlist, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case MOUNTPROC_UMNT:
		if (!svc_getargs(transp, xdr_dir, dpath)) {
			svcerr_decode(transp);
			return;
		}
		if (!lookup_failed)
			ret = del_mlist(host, dpath, saddr);
		ret |= del_mlist(numerichost, dpath, saddr);
		if (ret) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case MOUNTPROC_UMNTALL:
		if (!lookup_failed)
			ret = del_mlist(host, NULL, saddr);
		ret |= del_mlist(numerichost, NULL, saddr);
		if (ret) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;
	case MOUNTPROC_EXPORT:
	case MOUNTPROC_EXPORTALL:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_explist, NULL))
			syslog(LOG_ERR, "Can't send reply");
		return;


	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dpath string
 */
static int
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{

	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
static int
xdr_fhs(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct fhreturn *fhrp = (struct fhreturn *) cp;
	long ok = 0, len, auth;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, NFSX_V2FH));
	case 3:
		len = fhrp->fhr_fhsize;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, len))
			return (0);
		if (fhrp->fhr_flag & DP_KERB)
			auth = RPCAUTH_KERB4;
		else
			auth = RPCAUTH_UNIX;
		len = 1;
		if (!xdr_long(xdrsp, &len))
			return (0);
		return (xdr_long(xdrsp, &auth));
	};
	return (0);
}

int
xdr_mlist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct mountlist *mlp;
	int trueval = 1;
	int falseval = 0;
	char *strp;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &trueval))
			return (0);
		strp = &mlp->ml_host[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = &mlp->ml_dirp[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
int
xdr_explist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct exportlist *ep;
	int falseval = 0;
	int putdef;
	sigset_t sighup_mask;

	(void)sigemptyset(&sighup_mask);
	(void)sigaddset(&sighup_mask, SIGHUP);
	(void)sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
	ep = exphead;
	while (ep) {
		putdef = 0;
		if (put_exlist(ep->ex_dirl, xdrsp, ep->ex_defdir, &putdef))
			goto errout;
		if (ep->ex_defdir && putdef == 0 &&
		    put_exlist(ep->ex_defdir, xdrsp, NULL, &putdef))
			goto errout;
		ep = ep->ex_next;
	}
	(void)sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
errout:
	(void)sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to traverse the tree and export the
 * directory paths.  Assumes SIGHUP has already been masked.
 */
int
put_exlist(dp, xdrsp, adp, putdefp)
	struct dirlist *dp;
	XDR *xdrsp;
	struct dirlist *adp;
	int *putdefp;
{
	struct grouplist *grp;
	struct hostlist *hp;
	int trueval = 1;
	int falseval = 0;
	int gotalldir = 0;
	char *strp;

	if (dp) {
		if (put_exlist(dp->dp_left, xdrsp, adp, putdefp))
			return (1);
		if (!xdr_bool(xdrsp, &trueval))
			return (1);
		strp = dp->dp_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (1);
		if (adp && !strcmp(dp->dp_dirp, adp->dp_dirp)) {
			gotalldir = 1;
			*putdefp = 1;
		}
		if ((dp->dp_flag & DP_DEFSET) == 0 &&
		    (gotalldir == 0 || (adp->dp_flag & DP_DEFSET) == 0)) {
			hp = dp->dp_hosts;
			while (hp) {
				grp = hp->ht_grp;
				if (grp->gr_type == GT_HOST) {
					if (!xdr_bool(xdrsp, &trueval))
						return (1);
					strp =
					  grp->gr_ptr.gt_addrinfo->ai_canonname;
					if (!xdr_string(xdrsp, &strp,
							RPCMNT_NAMELEN))
						return (1);
				} else if (grp->gr_type == GT_NET) {
					if (!xdr_bool(xdrsp, &trueval))
						return (1);
					strp = grp->gr_ptr.gt_net.nt_name;
					if (!xdr_string(xdrsp, &strp,
							RPCMNT_NAMELEN))
						return (1);
				}
				hp = hp->ht_next;
				if (gotalldir && hp == NULL) {
					hp = adp->dp_hosts;
					gotalldir = 0;
				}
			}
		}
		if (!xdr_bool(xdrsp, &falseval))
			return (1);
		if (put_exlist(dp->dp_right, xdrsp, adp, putdefp))
			return (1);
	}
	return (0);
}

static int
parse_host_netgroup(line, lineno, ep, tgrp, cp, has_host, grp)
	const char *line;
	size_t lineno;
	struct exportlist *ep;
	struct grouplist *tgrp;
	char *cp;
	int *has_host;
	struct grouplist **grp;
{
	const char *hst, *usr, *dom;
	int netgrp;

	if (ep == NULL) {
		syslog(LOG_ERR, "\"%s\", line %ld: No current export",
		    line, (unsigned long)lineno);
		return 0;
	}
	setnetgrent(cp);
	netgrp = getnetgrent(&hst, &usr, &dom);
	do {
		if (*has_host) {
			(*grp)->gr_next = get_grp();
			*grp = (*grp)->gr_next;
		}
		if (netgrp) {
			if (hst == NULL) {
				syslog(LOG_ERR,
				    "\"%s\", line %ld: No host in netgroup %s",
				    line, (unsigned long)lineno, cp);
				goto bad;
			}
			if (get_host(line, lineno, hst, *grp))
				goto bad;
		} else if (get_host(line, lineno, cp, *grp))
			goto bad;
		*has_host = TRUE;
	} while (netgrp && getnetgrent(&hst, &usr, &dom));

	endnetgrent();
	return 1;
bad:
	endnetgrent();
	return 0;
	
}

static int
parse_directory(line, lineno, tgrp, got_nondir, cp, ep, fsp)
	const char *line;
	size_t lineno;
	struct grouplist *tgrp;
	int got_nondir;
	char *cp;
	struct exportlist **ep;
	struct statvfs *fsp;
{
	int error = 0;

	if (!check_dirpath(line, lineno, cp))
		return 0;

	if (rump_sys_statvfs1(cp, fsp, ST_WAIT) == -1) {
		syslog(LOG_ERR, "\"%s\", line %ld: statvfs for `%s' failed: %m %d",
		    line, (unsigned long)lineno, cp, error);
		return 0;
	}

	if (got_nondir) {
		syslog(LOG_ERR,
		    "\"%s\", line %ld: Directories must precede files",
		    line, (unsigned long)lineno);
		return 0;
	}
	if (*ep) {
		if ((*ep)->ex_fs.__fsid_val[0] != fsp->f_fsidx.__fsid_val[0] ||
		    (*ep)->ex_fs.__fsid_val[1] != fsp->f_fsidx.__fsid_val[1]) {
			syslog(LOG_ERR,
			    "\"%s\", line %ld: filesystem ids disagree",
			    line, (unsigned long)lineno);
			return 0;
		}
	} else {
		/*
		 * See if this directory is already
		 * in the list.
		 */
		*ep = ex_search(&fsp->f_fsidx);
		if (*ep == NULL) {
			*ep = get_exp();
			(*ep)->ex_fs = fsp->f_fsidx;
			(*ep)->ex_fsdir = estrdup(fsp->f_mntonname);
			if (debug)
				(void)fprintf(stderr,
				    "Making new ep fs=0x%x,0x%x\n",
				    fsp->f_fsidx.__fsid_val[0], fsp->f_fsidx.__fsid_val[1]);
		} else {
			if (debug)
				(void)fprintf(stderr,
				    "Found ep fs=0x%x,0x%x\n",
				    fsp->f_fsidx.__fsid_val[0], fsp->f_fsidx.__fsid_val[1]);
		}
	}

	return 1;
}


/*
 * Get the export list
 */
/* ARGSUSED */
void
get_exportlist(n)
	int n;
{
	struct exportlist *ep, *ep2;
	struct grouplist *grp, *tgrp;
	struct exportlist **epp;
	struct dirlist *dirhead;
	struct statvfs fsb, *fsp;
	struct addrinfo *ai;
	struct uucred anon;
	char *cp, *endcp, *dirp, savedc;
	int has_host, exflags, got_nondir, dirplen, num, i;
	FILE *exp_file;
	char *line;
	size_t lineno = 0, len;


	/*
	 * First, get rid of the old list
	 */
	ep = exphead;
	while (ep) {
		ep2 = ep;
		ep = ep->ex_next;
		free_exp(ep2);
	}
	exphead = NULL;

	dirp = NULL;
	dirplen = 0;
	grp = grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	grphead = NULL;

	/*
	 * And delete exports that are in the kernel for all local
	 * file systems.
	 */
	num = getmntinfo(&fsp, MNT_NOWAIT);
	for (i = 0; i < num; i++) {
		struct mountd_exports_list mel;

		/* Delete all entries from the export list. */
		mel.mel_path = fsp->f_mntonname;
		mel.mel_nexports = 0;
		if (rump_sys_nfssvc(NFSSVC_SETEXPORTSLIST, &mel) == -1 &&
		    errno != EOPNOTSUPP)
			syslog(LOG_ERR, "Can't delete exports for %s (%m)",
			    fsp->f_mntonname);

		fsp++;
	}

	/*
	 * Read in the exports file and build the list, calling
	 * mount() as we go along to push the export rules into the kernel.
	 */
	exname = _PATH_EXPORTS;
	if ((exp_file = rumpfopen(exname, "r")) == NULL) {
		/*
		 * Don't exit here; we can still reload the config
		 * after a SIGHUP.
		 */
		if (debug)
			(void)fprintf(stderr, "Can't open %s: %s\n", exname,
			    strerror(errno));
		return;
	}
	dirhead = NULL;
	while ((line = fparseln(exp_file, &len, &lineno, NULL, 0)) != NULL) {
		if (debug)
			(void)fprintf(stderr, "Got line %s\n", line);
		cp = line;
		nextfield(&cp, &endcp);
		if (cp == endcp)
			goto nextline;	/* skip empty line */
		/*
		 * Set defaults.
		 */
		has_host = FALSE;
		anon = def_anon;
		exflags = MNT_EXPORTED;
		got_nondir = 0;
		opt_flags = 0;
		ep = NULL;

		if (noprivports) {
			opt_flags |= OP_NORESMNT | OP_NORESPORT;
			exflags |= MNT_EXNORESPORT;
		}

		/*
		 * Create new exports list entry
		 */
		len = endcp - cp;
		tgrp = grp = get_grp();
		while (len > 0) {
			if (len > RPCMNT_NAMELEN) {
				*endcp = '\0';
				syslog(LOG_ERR,
				    "\"%s\", line %ld: name `%s' is too long",
				    line, (unsigned long)lineno, cp);
				goto badline;
			}
			switch (*cp) {
			case '-':
				/*
				 * Option
				 */
				if (ep == NULL) {
					syslog(LOG_ERR,
				"\"%s\", line %ld: No current export list",
					    line, (unsigned long)lineno);
					goto badline;
				}
				if (debug)
					(void)fprintf(stderr, "doing opt %s\n",
					    cp);
				got_nondir = 1;
				if (do_opt(line, lineno, &cp, &endcp, ep, grp,
				    &has_host, &exflags, &anon))
					goto badline;
				break;

			case '/':
				/*
				 * Directory
				 */
				savedc = *endcp;
				*endcp = '\0';

				if (!parse_directory(line, lineno, tgrp,
				    got_nondir, cp, &ep, &fsb))
					goto badline;
				/*
				 * Add dirpath to export mount point.
				 */
				dirp = add_expdir(&dirhead, cp, len);
				dirplen = len;

				*endcp = savedc;
				break;

			default:
				/*
				 * Host or netgroup.
				 */
				savedc = *endcp;
				*endcp = '\0';

				if (!parse_host_netgroup(line, lineno, ep,
				    tgrp, cp, &has_host, &grp))
					goto badline;

				got_nondir = 1;

				*endcp = savedc;
				break;
			}

			cp = endcp;
			nextfield(&cp, &endcp);
			len = endcp - cp;
		}
		if (check_options(line, lineno, dirhead))
			goto badline;

		if (!has_host) {
			grp->gr_type = GT_HOST;
			if (debug)
				(void)fprintf(stderr,
				    "Adding a default entry\n");
			/* add a default group and make the grp list NULL */
			ai = emalloc(sizeof(struct addrinfo));
			ai->ai_flags = 0;
			ai->ai_family = AF_INET;	/* XXXX */
			ai->ai_socktype = SOCK_DGRAM;
			/* setting the length to 0 will match anything */
			ai->ai_addrlen = 0;
			ai->ai_flags = AI_CANONNAME;
			ai->ai_canonname = estrdup("Default");
			ai->ai_addr = NULL;
			ai->ai_next = NULL;
			grp->gr_ptr.gt_addrinfo = ai;

		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			/*
			 * Don't allow a network export coincide with a list of
			 * host(s) on the same line.
			 */
			syslog(LOG_ERR,
			    "\"%s\", line %ld: Mixed exporting of networks and hosts is disallowed",
			    line, (unsigned long)lineno);
			goto badline;
		}
		/*
		 * Loop through hosts, pushing the exports into the kernel.
		 * After loop, tgrp points to the start of the list and
		 * grp points to the last entry in the list.
		 */
		grp = tgrp;
		do {
			if (do_nfssvc(line, lineno, ep, grp, exflags, &anon,
			    dirp, dirplen, &fsb))
				goto badline;
		} while (grp->gr_next && (grp = grp->gr_next));

		/*
		 * Success. Update the data structures.
		 */
		if (has_host) {
			hang_dirp(dirhead, tgrp, ep, opt_flags);
			grp->gr_next = grphead;
			grphead = tgrp;
		} else {
			hang_dirp(dirhead, NULL, ep, opt_flags);
			free_grp(tgrp);
		}
		tgrp = NULL;
		dirhead = NULL;
		if ((ep->ex_flag & EX_LINKED) == 0) {
			ep2 = exphead;
			epp = &exphead;

			/*
			 * Insert in the list in alphabetical order.
			 */
			while (ep2 && strcmp(ep2->ex_fsdir, ep->ex_fsdir) < 0) {
				epp = &ep2->ex_next;
				ep2 = ep2->ex_next;
			}
			if (ep2)
				ep->ex_next = ep2;
			*epp = ep;
			ep->ex_flag |= EX_LINKED;
		}
		goto nextline;
badline:
		free_exp_grp(ep, grp);
nextline:
		if (dirhead) {
			free_dir(dirhead);
			dirhead = NULL;
		}
		free(line);
	}
	(void)fclose(exp_file);
}

/*
 * Allocate an export list element
 */
static struct exportlist *
get_exp()
{
	struct exportlist *ep;

	ep = emalloc(sizeof(struct exportlist));
	(void)memset(ep, 0, sizeof(struct exportlist));
	return (ep);
}

/*
 * Allocate a group list element
 */
static struct grouplist *
get_grp()
{
	struct grouplist *gp;

	gp = emalloc(sizeof(struct grouplist));
	(void)memset(gp, 0, sizeof(struct grouplist));
	return (gp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
static void
free_exp_grp(ep, grp)
	struct exportlist *ep;
	struct grouplist *grp;
{
	struct grouplist *tgrp;

	if (ep && (ep->ex_flag & EX_LINKED) == 0)
		free_exp(ep);
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
}

/*
 * Search the export list for a matching fs.
 */
static struct exportlist *
ex_search(fsid)
	fsid_t *fsid;
{
	struct exportlist *ep;

	ep = exphead;
	return ep;
	while (ep) {
		if (ep->ex_fs.__fsid_val[0] == fsid->__fsid_val[0] &&
		    ep->ex_fs.__fsid_val[1] == fsid->__fsid_val[1])
			return (ep);
		ep = ep->ex_next;
	}
	return (ep);
}

/*
 * Add a directory path to the list.
 */
static char *
add_expdir(dpp, cp, len)
	struct dirlist **dpp;
	char *cp;
	int len;
{
	struct dirlist *dp;

	dp = emalloc(sizeof(struct dirlist) + len);
	dp->dp_left = *dpp;
	dp->dp_right = NULL;
	dp->dp_flag = 0;
	dp->dp_hosts = NULL;
	(void)strcpy(dp->dp_dirp, cp);
	*dpp = dp;
	return (dp->dp_dirp);
}

/*
 * Hang the dir list element off the dirpath binary tree as required
 * and update the entry for host.
 */
void
hang_dirp(dp, grp, ep, flags)
	struct dirlist *dp;
	struct grouplist *grp;
	struct exportlist *ep;
	int flags;
{
	struct hostlist *hp;
	struct dirlist *dp2;

	if (flags & OP_ALLDIRS) {
		if (ep->ex_defdir)
			free(dp);
		else
			ep->ex_defdir = dp;
		if (grp == NULL) {
			ep->ex_defdir->dp_flag |= DP_DEFSET;
			if (flags & OP_KERB)
				ep->ex_defdir->dp_flag |= DP_KERB;
			if (flags & OP_NORESMNT)
				ep->ex_defdir->dp_flag |= DP_NORESMNT;
		} else
			while (grp) {
				hp = get_ht();
				if (flags & OP_KERB)
					hp->ht_flag |= DP_KERB;
				if (flags & OP_NORESMNT)
					hp->ht_flag |= DP_NORESMNT;
				hp->ht_grp = grp;
				hp->ht_next = ep->ex_defdir->dp_hosts;
				ep->ex_defdir->dp_hosts = hp;
				grp = grp->gr_next;
			}
	} else {

		/*
		 * Loop through the directories adding them to the tree.
		 */
		while (dp) {
			dp2 = dp->dp_left;
			add_dlist(&ep->ex_dirl, dp, grp, flags);
			dp = dp2;
		}
	}
}

/*
 * Traverse the binary tree either updating a node that is already there
 * for the new directory or adding the new node.
 */
static void
add_dlist(dpp, newdp, grp, flags)
	struct dirlist **dpp;
	struct dirlist *newdp;
	struct grouplist *grp;
	int flags;
{
	struct dirlist *dp;
	struct hostlist *hp;
	int cmp;

	dp = *dpp;
	if (dp) {
		cmp = strcmp(dp->dp_dirp, newdp->dp_dirp);
		if (cmp > 0) {
			add_dlist(&dp->dp_left, newdp, grp, flags);
			return;
		} else if (cmp < 0) {
			add_dlist(&dp->dp_right, newdp, grp, flags);
			return;
		} else
			free(newdp);
	} else {
		dp = newdp;
		dp->dp_left = NULL;
		*dpp = dp;
	}
	if (grp) {

		/*
		 * Hang all of the host(s) off of the directory point.
		 */
		do {
			hp = get_ht();
			if (flags & OP_KERB)
				hp->ht_flag |= DP_KERB;
			if (flags & OP_NORESMNT)
				hp->ht_flag |= DP_NORESMNT;
			hp->ht_grp = grp;
			hp->ht_next = dp->dp_hosts;
			dp->dp_hosts = hp;
			grp = grp->gr_next;
		} while (grp);
	} else {
		dp->dp_flag |= DP_DEFSET;
		if (flags & OP_KERB)
			dp->dp_flag |= DP_KERB;
		if (flags & OP_NORESMNT)
			dp->dp_flag |= DP_NORESMNT;
	}
}

/*
 * Search for a dirpath on the export point.
 */
static struct dirlist *
dirp_search(dp, dirp)
	struct dirlist *dp;
	char *dirp;
{
	int cmp;

	if (dp) {
		cmp = strcmp(dp->dp_dirp, dirp);
		if (cmp > 0)
			return (dirp_search(dp->dp_left, dirp));
		else if (cmp < 0)
			return (dirp_search(dp->dp_right, dirp));
		else
			return (dp);
	}
	return (dp);
}

/*
 * Some helper functions for netmasks. They all assume masks in network
 * order (big endian).
 */
static int
bitcmp(void *dst, void *src, int bitlen)
{
	int i;
	u_int8_t *p1 = dst, *p2 = src;
	u_int8_t bitmask;
	int bytelen, bitsleft;

	bytelen = bitlen / 8;
	bitsleft = bitlen % 8;

	if (debug) {
		printf("comparing:\n");
		for (i = 0; i < (bitsleft ? bytelen + 1 : bytelen); i++)
			printf("%02x", p1[i]);
		printf("\n");
		for (i = 0; i < (bitsleft ? bytelen + 1 : bytelen); i++)
			printf("%02x", p2[i]);
		printf("\n");
	}

	for (i = 0; i < bytelen; i++) {
		if (*p1 != *p2)
			return 1;
		p1++;
		p2++;
	}

	for (i = 0; i < bitsleft; i++) {
		bitmask = 1 << (7 - i);
		if ((*p1 & bitmask) != (*p2 & bitmask))
			return 1;
	}

	return 0;
}

static int
netpartcmp(struct sockaddr *s1, struct sockaddr *s2, int bitlen)
{
	void *src, *dst;

	if (s1->sa_family != s2->sa_family)
		return 1;

	switch (s1->sa_family) {
	case AF_INET:
		src = &((struct sockaddr_in *)s1)->sin_addr;
		dst = &((struct sockaddr_in *)s2)->sin_addr;
		if (bitlen > sizeof(((struct sockaddr_in *)s1)->sin_addr) * 8)
			return 1;
		break;
	case AF_INET6:
		src = &((struct sockaddr_in6 *)s1)->sin6_addr;
		dst = &((struct sockaddr_in6 *)s2)->sin6_addr;
		if (((struct sockaddr_in6 *)s1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)s2)->sin6_scope_id)
			return 1;
		if (bitlen > sizeof(((struct sockaddr_in6 *)s1)->sin6_addr) * 8)
			return 1;
		break;
	default:
		return 1;
	}

	return bitcmp(src, dst, bitlen);
}

static int
allones(struct sockaddr_storage *ssp, int bitlen)
{
	u_int8_t *p;
	int bytelen, bitsleft, i;
	int zerolen;

	switch (ssp->ss_family) {
	case AF_INET:
		p = (u_int8_t *)&((struct sockaddr_in *)ssp)->sin_addr;
		zerolen = sizeof (((struct sockaddr_in *)ssp)->sin_addr);
		break;
	case AF_INET6:
		p = (u_int8_t *)&((struct sockaddr_in6 *)ssp)->sin6_addr;
		zerolen = sizeof (((struct sockaddr_in6 *)ssp)->sin6_addr);
		break;
	default:
		return -1;
	}

	memset(p, 0, zerolen);

	bytelen = bitlen / 8;
	bitsleft = bitlen % 8;

	if (bytelen > zerolen)
		return -1;

	for (i = 0; i < bytelen; i++)
		*p++ = 0xff;

	for (i = 0; i < bitsleft; i++)
		*p |= 1 << (7 - i);
	
	return 0;
}

static int
countones(struct sockaddr *sa)
{
	void *mask;
	int i, bits = 0, bytelen;
	u_int8_t *p;

	switch (sa->sa_family) {
	case AF_INET:
		mask = (u_int8_t *)&((struct sockaddr_in *)sa)->sin_addr;
		bytelen = 4;
		break;
	case AF_INET6:
		mask = (u_int8_t *)&((struct sockaddr_in6 *)sa)->sin6_addr;
		bytelen = 16;
		break;
	default:
		return 0;
	}

	p = mask;

	for (i = 0; i < bytelen; i++, p++) {
		if (*p != 0xff) {
			for (bits = 0; bits < 8; bits++) {
				if (!(*p & (1 << (7 - bits))))
					break;
			}
			break;
		}
	}

	return (i * 8 + bits);
}

static int
sacmp(struct sockaddr *sa1, struct sockaddr *sa2)
{
	void *p1, *p2;
	int len;

	if (sa1->sa_family != sa2->sa_family)
		return 1;

	switch (sa1->sa_family) {
	case AF_INET:
		p1 = &((struct sockaddr_in *)sa1)->sin_addr;
		p2 = &((struct sockaddr_in *)sa2)->sin_addr;
		len = 4;
		break;
	case AF_INET6:
		p1 = &((struct sockaddr_in6 *)sa1)->sin6_addr;
		p2 = &((struct sockaddr_in6 *)sa2)->sin6_addr;
		len = 16;
		if (((struct sockaddr_in6 *)sa1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)sa2)->sin6_scope_id)
			return 1;
		break;
	default:
		return 1;
	}

	return memcmp(p1, p2, len);
}

/*
 * Scan for a host match in a directory tree.
 */
static int
chk_host(dp, saddr, defsetp, hostsetp)
	struct dirlist *dp;
	struct sockaddr *saddr;
	int *defsetp;
	int *hostsetp;
{
	struct hostlist *hp;
	struct grouplist *grp;
	struct addrinfo *ai;

	if (dp) {
		if (dp->dp_flag & DP_DEFSET)
			*defsetp = dp->dp_flag;
		hp = dp->dp_hosts;
		while (hp) {
			grp = hp->ht_grp;
			switch (grp->gr_type) {
			case GT_HOST:
				ai = grp->gr_ptr.gt_addrinfo;
				for (; ai; ai = ai->ai_next) {
					if (!sacmp(ai->ai_addr, saddr)) {
						*hostsetp =
						    (hp->ht_flag | DP_HOSTSET);
						return (1);
					}
				}
				break;
			case GT_NET:
				if (!netpartcmp(saddr,
				    (struct sockaddr *)
					&grp->gr_ptr.gt_net.nt_net,
				    grp->gr_ptr.gt_net.nt_len)) {
					*hostsetp = (hp->ht_flag | DP_HOSTSET);
					return (1);
				}
				break;
			};
			hp = hp->ht_next;
		}
	}
	return (0);
}

/*
 * Scan tree for a host that matches the address.
 */
static int
scan_tree(dp, saddr)
	struct dirlist *dp;
	struct sockaddr *saddr;
{
	int defset, hostset;

	if (dp) {
		if (scan_tree(dp->dp_left, saddr))
			return (1);
		if (chk_host(dp, saddr, &defset, &hostset))
			return (1);
		if (scan_tree(dp->dp_right, saddr))
			return (1);
	}
	return (0);
}

/*
 * Traverse the dirlist tree and free it up.
 */
static void
free_dir(dp)
	struct dirlist *dp;
{

	if (dp) {
		free_dir(dp->dp_left);
		free_dir(dp->dp_right);
		free_host(dp->dp_hosts);
		free(dp);
	}
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
static int
do_opt(line, lineno, cpp, endcpp, ep, grp, has_hostp, exflagsp, cr)
	const char *line;
	size_t lineno;
	char **cpp, **endcpp;
	struct exportlist *ep;
	struct grouplist *grp;
	int *has_hostp;
	int *exflagsp;
	struct uucred *cr;
{
	char *cpoptarg, *cpoptend;
	char *cp, *cpopt, savedc, savedc2;
	char *endcp = NULL;	/* XXX: GCC */
	int allflag, usedarg;

	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		savedc2 = '\0';
		if ((cpoptend = strchr(cpopt, ',')) != NULL) {
			*cpoptend++ = '\0';
			if ((cpoptarg = strchr(cpopt, '=')) != NULL)
				*cpoptarg++ = '\0';
		} else {
			if ((cpoptarg = strchr(cpopt, '=')) != NULL)
				*cpoptarg++ = '\0';
			else {
				*cp = savedc;
				nextfield(&cp, &endcp);
				**endcpp = '\0';
				if (endcp > cp && *cp != '-') {
					cpoptarg = cp;
					savedc2 = *endcp;
					*endcp = '\0';
					usedarg = 0;
				}
			}
		}
		if (!strcmp(cpopt, "ro") || !strcmp(cpopt, "o")) {
			*exflagsp |= MNT_EXRDONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
		    !(allflag = strcmp(cpopt, "mapall")) ||
		    !strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			parsecred(cpoptarg, cr);
			if (allflag == 0) {
				*exflagsp |= MNT_EXPORTANON;
				opt_flags |= OP_MAPALL;
			} else
				opt_flags |= OP_MAPROOT;
		} else if (!strcmp(cpopt, "kerb") || !strcmp(cpopt, "k")) {
			*exflagsp |= MNT_EXKERB;
			opt_flags |= OP_KERB;
		} else if (cpoptarg && (!strcmp(cpopt, "mask") ||
		    !strcmp(cpopt, "m"))) {
			if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 1)) {
				syslog(LOG_ERR,
				    "\"%s\", line %ld: Bad mask: %s",
				    line, (unsigned long)lineno, cpoptarg);
				return (1);
			}
			usedarg++;
			opt_flags |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
		    !strcmp(cpopt, "n"))) {
			if (strchr(cpoptarg, '/') != NULL) {
				if (debug)
					fprintf(stderr, "setting OP_MASKLEN\n");
				opt_flags |= OP_MASKLEN;
			}
			if (grp->gr_type != GT_NULL) {
				syslog(LOG_ERR,
				    "\"%s\", line %ld: Network/host conflict",
				    line, (unsigned long)lineno);
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 0)) {
				syslog(LOG_ERR,
				    "\"%s\", line %ld: Bad net: %s",
				    line, (unsigned long)lineno, cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			opt_flags |= OP_ALLDIRS;
		} else if (!strcmp(cpopt, "noresvmnt")) {
			opt_flags |= OP_NORESMNT;
		} else if (!strcmp(cpopt, "noresvport")) {
			opt_flags |= OP_NORESPORT;
			*exflagsp |= MNT_EXNORESPORT;
		} else if (!strcmp(cpopt, "public")) {
			*exflagsp |= (MNT_EXNORESPORT | MNT_EXPUBLIC);
			opt_flags |= OP_NORESPORT;
		} else if (!strcmp(cpopt, "webnfs")) {
			*exflagsp |= (MNT_EXNORESPORT | MNT_EXPUBLIC |
			    MNT_EXRDONLY | MNT_EXPORTANON);
			opt_flags |= (OP_MAPALL | OP_NORESPORT);
		} else if (cpoptarg && !strcmp(cpopt, "index")) {
			ep->ex_indexfile = strdup(cpoptarg);
		} else {
			syslog(LOG_ERR, 
			    "\"%s\", line %ld: Bad opt %s",
			    line, (unsigned long)lineno, cpopt);
			return (1);
		}
		if (usedarg >= 0) {
			*endcp = savedc2;
			**endcpp = savedc;
			if (usedarg > 0) {
				*cpp = cp;
				*endcpp = endcp;
			}
			return (0);
		}
		cpopt = cpoptend;
	}
	**endcpp = savedc;
	return (0);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
static int
get_host(line, lineno, cp, grp)
	const char *line;
	size_t lineno;
	const char *cp;
	struct grouplist *grp;
{
	struct addrinfo *ai, hints;
	int ecode;
	char host[NI_MAXHOST];

	if (grp->gr_type != GT_NULL) {
		syslog(LOG_ERR,
		    "\"%s\", line %ld: Bad netgroup type for ip host %s",
		    line, (unsigned long)lineno, cp);
		return (1);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_CANONNAME;
	hints.ai_protocol = IPPROTO_UDP;
	ecode = getaddrinfo(cp, NULL, &hints, &ai);
	if (ecode != 0) {
		syslog(LOG_ERR, "\"%s\", line %ld: can't get address info for "
				"host %s",
		    line, (long)lineno, cp);
		return 1;
	}
	grp->gr_type = GT_HOST;
	grp->gr_ptr.gt_addrinfo = ai;
	while (ai != NULL) {
		if (ai->ai_canonname == NULL) {
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen, host,
			    sizeof host, NULL, 0, ninumeric) != 0)
				strlcpy(host, "?", sizeof(host));
			ai->ai_canonname = estrdup(host);
			ai->ai_flags |= AI_CANONNAME;
		} else
			ai->ai_flags &= ~AI_CANONNAME;
		if (debug)
			(void)fprintf(stderr, "got host %s\n", ai->ai_canonname);
		ai = ai->ai_next;
	}
	return (0);
}

/*
 * Free up an exports list component
 */
static void
free_exp(ep)
	struct exportlist *ep;
{

	if (ep->ex_defdir) {
		free_host(ep->ex_defdir->dp_hosts);
		free(ep->ex_defdir);
	}
	if (ep->ex_fsdir)
		free(ep->ex_fsdir);
	if (ep->ex_indexfile)
		free(ep->ex_indexfile);
	free_dir(ep->ex_dirl);
	free(ep);
}

/*
 * Free hosts.
 */
static void
free_host(hp)
	struct hostlist *hp;
{
	struct hostlist *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free(hp2);
	}
}

static struct hostlist *
get_ht()
{
	struct hostlist *hp;

	hp = emalloc(sizeof(struct hostlist));
	hp->ht_next = NULL;
	hp->ht_flag = 0;
	return (hp);
}

/*
 * Do the nfssvc syscall to push the export info into the kernel.
 */
static int
do_nfssvc(line, lineno, ep, grp, exflags, anoncrp, dirp, dirplen, fsb)
	const char *line;
	size_t lineno;
	struct exportlist *ep;
	struct grouplist *grp;
	int exflags;
	struct uucred *anoncrp;
	char *dirp;
	int dirplen;
	struct statvfs *fsb;
{
	struct sockaddr *addrp;
	struct sockaddr_storage ss;
	struct addrinfo *ai;
	int addrlen;
	int done;
	struct export_args export;

	export.ex_flags = exflags;
	export.ex_anon = *anoncrp;
	export.ex_indexfile = ep->ex_indexfile;
	if (grp->gr_type == GT_HOST) {
		ai = grp->gr_ptr.gt_addrinfo;
		addrp = ai->ai_addr;
		addrlen = ai->ai_addrlen;
	} else {
		addrp = NULL;
		ai = NULL;	/* XXXGCC -Wuninitialized */
		addrlen = 0;	/* XXXGCC -Wuninitialized */
	}
	done = FALSE;
	while (!done) {
		struct mountd_exports_list mel;

		switch (grp->gr_type) {
		case GT_HOST:
			if (addrp != NULL && addrp->sa_family == AF_INET6 &&
			    have_v6 == 0)
				goto skip;
			export.ex_addr = addrp;
			export.ex_addrlen = addrlen;
			export.ex_masklen = 0;
			break;
		case GT_NET:
			export.ex_addr = (struct sockaddr *)
			    &grp->gr_ptr.gt_net.nt_net;
			if (export.ex_addr->sa_family == AF_INET6 &&
			    have_v6 == 0)
				goto skip;
			export.ex_addrlen = export.ex_addr->sa_len;
			memset(&ss, 0, sizeof ss);
			ss.ss_family = export.ex_addr->sa_family;
			ss.ss_len = export.ex_addr->sa_len;
			if (allones(&ss, grp->gr_ptr.gt_net.nt_len) != 0) {
				syslog(LOG_ERR,
				    "\"%s\", line %ld: Bad network flag",
				    line, (unsigned long)lineno);
				return (1);
			}
			export.ex_mask = (struct sockaddr *)&ss;
			export.ex_masklen = ss.ss_len;
			break;
		default:
			syslog(LOG_ERR, "\"%s\", line %ld: Bad netgroup type",
			    line, (unsigned long)lineno);
			return (1);
		};

		/*
		 * XXX:
		 * Maybe I should just use the fsb->f_mntonname path?
		 */

		mel.mel_path = dirp;
		mel.mel_nexports = 1;
		mel.mel_exports = &export;

		if (rump_sys_nfssvc(NFSSVC_SETEXPORTSLIST, &mel) != 0) {
			syslog(LOG_ERR,
	    "\"%s\", line %ld: Can't change attributes for %s to %s: %m %d",
			    line, (unsigned long)lineno,
			    dirp, (grp->gr_type == GT_HOST) ?
			    grp->gr_ptr.gt_addrinfo->ai_canonname :
			    (grp->gr_type == GT_NET) ?
			    grp->gr_ptr.gt_net.nt_name :
			    "Unknown", errno);
			return (1);
		}
skip:
		if (addrp) {
			ai = ai->ai_next;
			if (ai == NULL)
				done = TRUE;
			else {
				addrp = ai->ai_addr;
				addrlen = ai->ai_addrlen;
			}
		} else
			done = TRUE;
	}
	return (0);
}

/*
 * Translate a net address.
 */
static int
get_net(cp, net, maskflg)
	char *cp;
	struct netmsk *net;
	int maskflg;
{
	struct netent *np;
	char *thename, *p, *prefp;
	struct sockaddr_in sin, *sinp;
	struct sockaddr *sa;
	struct addrinfo hints, *ai = NULL;
	char netname[NI_MAXHOST];
	long preflen;
	int ecode;

	(void)memset(&sin, 0, sizeof(sin));
	if ((opt_flags & OP_MASKLEN) && !maskflg) {
		p = strchr(cp, '/');
		*p = '\0';
		prefp = p + 1;
	} else {
		p = NULL;	/* XXXGCC -Wuninitialized */
		prefp = NULL;	/* XXXGCC -Wuninitialized */
	}

	if ((np = getnetbyname(cp)) != NULL) {
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof sin;
		sin.sin_addr = inet_makeaddr(np->n_net, 0);
		sa = (struct sockaddr *)&sin;
	} else if (isdigit((unsigned char)*cp)) {
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(cp, NULL, &hints, &ai) != 0) {
			/*
			 * If getaddrinfo() failed, try the inet4 network
			 * notation with less than 3 dots.
			 */
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = inet_makeaddr(inet_network(cp),0);
			if (debug)
				fprintf(stderr, "get_net: v4 addr %x\n",
				    sin.sin_addr.s_addr);
			sa = (struct sockaddr *)&sin;
		} else
			sa = ai->ai_addr;
	} else if (isxdigit((unsigned char)*cp) || *cp == ':') {
		memset(&hints, 0, sizeof hints);
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(cp, NULL, &hints, &ai) == 0)
			sa = ai->ai_addr;
		else
			goto fail;
	} else
		goto fail;

	/*
	 * Only allow /pref notation for v6 addresses.
	 */
	if (sa->sa_family == AF_INET6 && (!(opt_flags & OP_MASKLEN) || maskflg))
		return 1;

	ecode = getnameinfo(sa, sa->sa_len, netname, sizeof netname,
	    NULL, 0, ninumeric);
	if (ecode != 0)
		goto fail;

	if (maskflg)
		net->nt_len = countones(sa);
	else {
		if (opt_flags & OP_MASKLEN) {
			errno = 0;
			preflen = strtol(prefp, NULL, 10);
			if (preflen == LONG_MIN && errno == ERANGE)
				goto fail;
			net->nt_len = (int)preflen;
			*p = '/';
		}

		if (np)
			thename = np->n_name;
		else {
			if (getnameinfo(sa, sa->sa_len, netname, sizeof netname,
			    NULL, 0, ninumeric) != 0)
				strlcpy(netname, "?", sizeof(netname));
			thename = netname;
		}
		net->nt_name = estrdup(thename);
		memcpy(&net->nt_net, sa, sa->sa_len);
	}

	if (!maskflg && sa->sa_family == AF_INET &&
	    !(opt_flags & (OP_MASK|OP_MASKLEN))) {
		sinp = (struct sockaddr_in *)sa;
		if (IN_CLASSA(sinp->sin_addr.s_addr))
			net->nt_len = 8;
		else if (IN_CLASSB(sinp->sin_addr.s_addr))
			net->nt_len = 16;
		else if (IN_CLASSC(sinp->sin_addr.s_addr))
			net->nt_len = 24;
		else if (IN_CLASSD(sinp->sin_addr.s_addr))
			net->nt_len = 28;
		else
			net->nt_len = 32;	/* XXX */
	}

	if (ai)
		freeaddrinfo(ai);
	return 0;

fail:
	if (ai)
		freeaddrinfo(ai);
	return 1;
}

/*
 * Parse out the next white space separated field
 */
static void
nextfield(cp, endcp)
	char **cp;
	char **endcp;
{
	char *p;

	p = *cp;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '\n' || *p == '\0')
		*cp = *endcp = p;
	else {
		*cp = p++;
		while (*p != ' ' && *p != '\t' && *p != '\n' && *p != '\0')
			p++;
		*endcp = p;
	}
}

/*
 * Parse a description of a credential.
 */
static void
parsecred(namelist, cr)
	char *namelist;
	struct uucred *cr;
{
	char *thename;
	int cnt;
	char *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups;
	gid_t grps[NGROUPS + 1];

	/*
	 * Set up the unprivileged user.
	 */
	*cr = def_anon;
	/*
	 * Get the user's password table entry.
	 */
	names = strsep(&namelist, " \t\n");
	thename = strsep(&names, ":");
	if (isdigit((unsigned char)*thename) || *thename == '-')
		pw = getpwuid(atoi(thename));
	else
		pw = getpwnam(thename);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			syslog(LOG_ERR, "Unknown user: %s", thename);
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, grps, &ngroups))
			syslog(LOG_ERR, "Too many groups for user %s", thename);
		/*
		 * Convert from int's to gid_t's and compress out duplicate
		 */
		cr->cr_ngroups = ngroups - 1;
		cr->cr_gid = grps[0];
		for (cnt = 1; cnt < ngroups; cnt++)
			cr->cr_groups[cnt - 1] = grps[cnt];
		return;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL)
		cr->cr_uid = pw->pw_uid;
	else if (isdigit((unsigned char)*thename) || *thename == '-')
		cr->cr_uid = atoi(thename);
	else {
		syslog(LOG_ERR, "Unknown user: %s", thename);
		return;
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS) {
		thename = strsep(&names, ":");
		if (isdigit((unsigned char)*thename) || *thename == '-') {
			cr->cr_groups[cr->cr_ngroups++] = atoi(thename);
		} else {
			if ((gr = getgrnam(thename)) == NULL) {
				syslog(LOG_ERR, "Unknown group: %s", thename);
				continue;
			}
			cr->cr_groups[cr->cr_ngroups++] = gr->gr_gid;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS)
		syslog(LOG_ERR, "Too many groups");
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
static void
get_mountlist()
{
	struct mountlist *mlp, **mlpp;
	char *host, *dirp, *cp;
	char str[STRSIZ];
	FILE *mlfile;

	if ((mlfile = rumpfopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		syslog(LOG_ERR, "Can't open %s: %m", _PATH_RMOUNTLIST);
		return;
	}
	mlpp = &mlhead;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dirp = strsep(&cp, " \t\n");
		if (host == NULL || dirp == NULL)
			continue;
		mlp = emalloc(sizeof(*mlp));
		(void)strncpy(mlp->ml_host, host, RPCMNT_NAMELEN);
		mlp->ml_host[RPCMNT_NAMELEN] = '\0';
		(void)strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
		mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
		mlp->ml_next = NULL;
		*mlpp = mlp;
		mlpp = &mlp->ml_next;
	}
	(void)fclose(mlfile);
}

static int
del_mlist(hostp, dirp, saddr)
	char *hostp, *dirp;
	struct sockaddr *saddr;
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	u_short sport;
	FILE *mlfile;
	int fnd = 0, ret = 0;
	char host[NI_MAXHOST];

	switch (saddr->sa_family) {
	case AF_INET6:
		sport = ntohs(((struct sockaddr_in6 *)saddr)->sin6_port);
		break;
	case AF_INET:
		sport = ntohs(((struct sockaddr_in *)saddr)->sin_port);
		break;
	default:
		return -1;
	}
	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) &&
		    (!dirp || !strcmp(mlp->ml_dirp, dirp))) {
			if (!(mlp->ml_flag & DP_NORESMNT) &&
			    sport >= IPPORT_RESERVED) {
				if (getnameinfo(saddr, saddr->sa_len, host,
				    sizeof host, NULL, 0, ninumeric) != 0)
					strlcpy(host, "?", sizeof(host));
				syslog(LOG_NOTICE,
				"Umount request for %s:%s from %s refused\n",
				    mlp->ml_host, mlp->ml_dirp, host);
				ret = -1;
				goto cont;
			}
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free(mlp2);
		} else {
cont:
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = rumpfopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			syslog(LOG_ERR, "Can't update %s: %m",
			    _PATH_RMOUNTLIST);
			return ret;
		}
		mlp = mlhead;
		while (mlp) {
			(void)fprintf(mlfile, "%s %s\n", mlp->ml_host,
		            mlp->ml_dirp);
			mlp = mlp->ml_next;
		}
		(void)fclose(mlfile);
	}
	return ret;
}

static void
add_mlist(hostp, dirp, flags)
	char *hostp, *dirp;
	int flags;
{
	struct mountlist *mlp, **mlpp;
	FILE *mlfile;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) && !strcmp(mlp->ml_dirp, dirp))
			return;
		mlpp = &mlp->ml_next;
		mlp = mlp->ml_next;
	}
	mlp = emalloc(sizeof(*mlp));
	strncpy(mlp->ml_host, hostp, RPCMNT_NAMELEN);
	mlp->ml_host[RPCMNT_NAMELEN] = '\0';
	strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
	mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
	mlp->ml_flag = flags;
	mlp->ml_next = NULL;
	*mlpp = mlp;
	if ((mlfile = rumpfopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		syslog(LOG_ERR, "Can't update %s: %m", _PATH_RMOUNTLIST);
		return;
	}
	(void)fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
	(void)fclose(mlfile);
}

/*
 * This function is called via. SIGTERM when the system is going down.
 * It sends a broadcast RPCMNT_UMNTALL.
 */
/* ARGSUSED */
static void
send_umntall(n)
	int n;
{
#if 0
	(void)clnt_broadcast(RPCPROG_MNT, RPCMNT_VER1, RPCMNT_UMNTALL,
	    xdr_void, NULL, xdr_void, NULL, (resultproc_t)umntall_each);
#endif
	exit(0);
}

#if 0
static int
umntall_each(resultsp, raddr)
	caddr_t resultsp;
	struct sockaddr_in *raddr;
{
	return (1);
}
#endif

/*
 * Free up a group list.
 */
static void
free_grp(grp)
	struct grouplist *grp;
{

	if (grp->gr_type == GT_HOST) {
		if (grp->gr_ptr.gt_addrinfo != NULL)
			freeaddrinfo(grp->gr_ptr.gt_addrinfo);
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_ptr.gt_net.nt_name)
			free(grp->gr_ptr.gt_net.nt_name);
	}
	free(grp);
}

#if 0
static void
SYSLOG(int pri, const char *fmt,...)
{
	va_list ap;

	va_start(ap, fmt);

	if (debug)
		vfprintf(stderr, fmt, ap);
	else
		vsyslog(pri, fmt, ap);

	va_end(ap);
}
#endif

/*
 * Check options for consistency.
 */
static int
check_options(line, lineno, dp)
	const char *line;
	size_t lineno;
	struct dirlist *dp;
{

	if (dp == NULL) {
		syslog(LOG_ERR,
		    "\"%s\", line %ld: missing directory list",
		    line, (unsigned long)lineno);
		return (1);
	}
	if ((opt_flags & (OP_MAPROOT|OP_MAPALL)) == (OP_MAPROOT|OP_MAPALL) ||
	    (opt_flags & (OP_MAPROOT|OP_KERB)) == (OP_MAPROOT|OP_KERB) ||
	    (opt_flags & (OP_MAPALL|OP_KERB)) == (OP_MAPALL|OP_KERB)) {
		syslog(LOG_ERR,
		    "\"%s\", line %ld: -mapall, -maproot and -kerb mutually exclusive",
		    line, (unsigned long)lineno);
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		syslog(LOG_ERR, "\"%s\", line %ld: -mask requires -net",
		    line, (unsigned long)lineno);
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_MASKLEN) != 0) {
		syslog(LOG_ERR, "\"%s\", line %ld: /pref and -mask mutually"
		    " exclusive",
		    line, (unsigned long)lineno);
		return (1);
	}
	if ((opt_flags & OP_ALLDIRS) && dp->dp_left) {
		syslog(LOG_ERR,
		    "\"%s\", line %ld: -alldirs has multiple directories",
		    line, (unsigned long)lineno);
		return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 * if no symbolic links are found.
 */
static int
check_dirpath(line, lineno, dirp)
	const char *line;
	size_t lineno;
	char *dirp;
{
	char *cp;
	struct stat sb;
	char *file = "";

	for (cp = dirp + 1; *cp; cp++) {
		if (*cp == '/') {
			*cp = '\0';
			if (rump_sys_lstat(dirp, &sb) == -1)
				goto bad;
			if (!S_ISDIR(sb.st_mode))
				goto bad1;
			*cp = '/';
		}
	}

	cp = NULL;
	if (rump_sys_lstat(dirp, &sb) == -1)
		goto bad;

	if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode)) {
		file = " file or a";
		goto bad1;
	}

	return 1;

bad:
	syslog(LOG_ERR,
	    "\"%s\", line %ld: lstat for `%s' failed: %m",
	    line, (unsigned long)lineno, dirp);
	if (cp)
		*cp = '/';
	return 0;

bad1:
	syslog(LOG_ERR,
	    "\"%s\", line %ld: `%s' is not a%s directory",
	    line, (unsigned long)lineno, dirp, file);
	abort();
	if (cp)
		*cp = '/';
	return 0;
}

static void
bind_resv_port(int sock, sa_family_t family, in_port_t port)
{
	struct sockaddr *sa;
	struct sockaddr_in sasin;
	struct sockaddr_in6 sasin6;

	switch (family) {
	case AF_INET:
		(void)memset(&sasin, 0, sizeof(sasin));
		sasin.sin_len = sizeof(sasin);
		sasin.sin_family = family;
		sasin.sin_port = htons(port);
		sa = (struct sockaddr *)(void *)&sasin;
		break;
	case AF_INET6:
		(void)memset(&sasin6, 0, sizeof(sasin6));
		sasin6.sin6_len = sizeof(sasin6);
		sasin6.sin6_family = family;
		sasin6.sin6_port = htons(port);
		sa = (struct sockaddr *)(void *)&sasin6;
		break;
	default:
		syslog(LOG_ERR, "Unsupported address family %d", family);
		return;
	}
	if (bindresvport_sa(sock, sa) == -1)
		syslog(LOG_ERR, "Cannot bind to reserved port %d (%m)", port);
}

/* ARGSUSED */
static void
no_nfs(int sig)
{
	syslog(LOG_ERR, "kernel NFS support not present; exiting");
	exit(1);
}
