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
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /*not lint*/

#ifndef lint
#if 0
static char sccsid[] = "@(#)mountd.c	8.15 (Berkeley) 5/1/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /*not lint*/

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/mount.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <ufs/ufs/ufsmount.h>
#include <msdosfs/msdosfsmount.h>
#include <ntfs/ntfsmount.h>
#include <isofs/cd9660/cd9660_mount.h>	/* XXX need isofs in include */

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

#ifdef DEBUG
#include <stdarg.h>
#endif

#ifndef MOUNTDLOCK
#define MOUNTDLOCK "/var/run/mountd.lock"
#endif

/*
 * Structures for keeping the mount list and export list
 */
struct mountlist {
	struct mountlist *ml_next;
	char	ml_host[RPCMNT_NAMELEN+1];
	char	ml_dirp[RPCMNT_PATHLEN+1];
};

struct dirlist {
	struct dirlist	*dp_left;
	struct dirlist	*dp_right;
	int		dp_flag;
	struct hostlist	*dp_hosts;	/* List of hosts this dir exported to */
	char		dp_dirp[1];	/* Actually malloc'd to size of dir */
};
/* dp_flag bits */
#define	DP_DEFSET	0x1
#define DP_HOSTSET	0x2
#define DP_KERB		0x4

struct exportlist {
	struct exportlist *ex_next;
	struct dirlist	*ex_dirl;
	struct dirlist	*ex_defdir;
	int		ex_flag;
	fsid_t		ex_fs;
	char		*ex_fsdir;
	char		*ex_indexfile;
};
/* ex_flag bits */
#define	EX_LINKED	0x1

struct netmsk {
	struct sockaddr_storage nt_net;
	struct sockaddr_storage nt_mask;
	char		*nt_name;
};

union grouptypes {
	struct addrinfo *gt_addrinfo;
	struct netmsk	gt_net;
};

struct grouplist {
	int gr_type;
	union grouptypes gr_ptr;
	struct grouplist *gr_next;
};
/* Group types */
#define	GT_NULL		0x0
#define	GT_HOST		0x1
#define	GT_NET		0x2
#define	GT_DEFAULT	0x3
#define GT_IGNORE	0x5

struct hostlist {
	int		 ht_flag;	/* Uses DP_xx bits */
	struct grouplist *ht_grp;
	struct hostlist	 *ht_next;
};

struct fhreturn {
	int	fhr_flag;
	int	fhr_vers;
	nfsfh_t	fhr_fh;
};

/* Global defs */
char	*add_expdir __P((struct dirlist **, char *, int));
void	add_dlist __P((struct dirlist **, struct dirlist *,
				struct grouplist *, int));
void	add_mlist __P((char *, char *));
int	check_dirpath __P((char *));
int	check_options __P((struct dirlist *));
int	checkmask(struct sockaddr *sa);
int	chk_host __P((struct dirlist *, struct sockaddr *, int *, int *));
void	del_mlist(char *hostp, char *dirp);
struct dirlist *dirp_search __P((struct dirlist *, char *));
int	do_mount __P((struct exportlist *, struct grouplist *, int,
		struct xucred *, char *, int, struct statfs *));
int	do_opt __P((char **, char **, struct exportlist *, struct grouplist *,
				int *, int *, struct xucred *));
struct	exportlist *ex_search __P((fsid_t *));
struct	exportlist *get_exp __P((void));
void	free_dir __P((struct dirlist *));
void	free_exp __P((struct exportlist *));
void	free_grp __P((struct grouplist *));
void	free_host __P((struct hostlist *));
void	get_exportlist __P((void));
int	get_host __P((char *, struct grouplist *, struct grouplist *));
struct hostlist *get_ht __P((void));
int	get_line __P((void));
void	get_mountlist __P((void));
int	get_net __P((char *, struct netmsk *, int));
void	getexp_err __P((struct exportlist *, struct grouplist *));
struct grouplist *get_grp __P((void));
void	hang_dirp __P((struct dirlist *, struct grouplist *,
				struct exportlist *, int));
void	huphandler(int sig);
int	makemask(struct sockaddr_storage *ssp, int bitlen);
void	mntsrv __P((struct svc_req *, SVCXPRT *));
void	nextfield __P((char **, char **));
void	out_of_mem __P((void));
void	parsecred __P((char *, struct xucred *));
int	put_exlist __P((struct dirlist *, XDR *, struct dirlist *, int *));
void	*sa_rawaddr(struct sockaddr *sa, int *nbytes);
int	sacmp(struct sockaddr *sa1, struct sockaddr *sa2,
    struct sockaddr *samask);
int	scan_tree __P((struct dirlist *, struct sockaddr *));
static void usage __P((void));
int	xdr_dir __P((XDR *, char *));
int	xdr_explist __P((XDR *, caddr_t));
int	xdr_fhs __P((XDR *, caddr_t));
int	xdr_mlist __P((XDR *, caddr_t));
void	terminate __P((int));

struct exportlist *exphead;
struct mountlist *mlhead;
struct grouplist *grphead;
char exname[MAXPATHLEN];
struct xucred def_anon = {
	0,
	(uid_t)-2,
	1,
	{ (gid_t)-2 },
	NULL
};
int force_v2 = 0;
int resvport_only = 1;
int dir_only = 1;
int log = 0;
int got_sighup = 0;

int opt_flags;
static int have_v6 = 1;
#ifdef NI_WITHSCOPEID
static const int ninumeric = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
static const int ninumeric = NI_NUMERICHOST;
#endif

int mountdlockfd;
/* Bits for opt_flags above */
#define	OP_MAPROOT	0x01
#define	OP_MAPALL	0x02
#define	OP_KERB		0x04
#define	OP_MASK		0x08
#define	OP_NET		0x10
#define	OP_ALLDIRS	0x40
#define	OP_HAVEMASK	0x80	/* A mask was specified or inferred. */
#define OP_MASKLEN	0x200

#ifdef DEBUG
int debug = 1;
void	SYSLOG __P((int, const char *, ...));
#define syslog SYSLOG
#else
int debug = 0;
#endif

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * and "-n" to allow nonroot mount.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	fd_set readfds;
	SVCXPRT *udptransp, *tcptransp, *udp6transp, *tcp6transp;
	struct netconfig *udpconf, *tcpconf, *udp6conf, *tcp6conf;
	int udpsock, tcpsock, udp6sock, tcp6sock;
	int xcreated = 0, s;
	int one = 1;
	int c, error, mib[3];
	struct vfsconf vfc;

	udp6conf = tcp6conf = NULL;
	udp6sock = tcp6sock = NULL;

	/* Check that another mountd isn't already running. */
	if ((mountdlockfd = (open(MOUNTDLOCK, O_RDONLY|O_CREAT, 0444))) == -1)
		err(1, "%s", MOUNTDLOCK);

	if(flock(mountdlockfd, LOCK_EX|LOCK_NB) == -1 && errno == EWOULDBLOCK)
		errx(1, "another rpc.mountd is already running. Aborting");
	s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0)
		have_v6 = 0;
	else
		close(s);
	error = getvfsbyname("nfs", &vfc);
	if (error && vfsisloadable("nfs")) {
		if(vfsload("nfs"))
			err(1, "vfsload(nfs)");
		endvfsent();	/* flush cache */
		error = getvfsbyname("nfs", &vfc);
	}
	if (error)
		errx(1, "NFS support is not available in the running kernel");

	while ((c = getopt(argc, argv, "2dlnr")) != -1)
		switch (c) {
		case '2':
			force_v2 = 1;
			break;
		case 'n':
			resvport_only = 0;
			break;
		case 'r':
			dir_only = 0;
			break;
		case 'd':
			debug = debug ? 0 : 1;
			break;
		case 'l':
			log = 1;
			break;
		default:
			usage();
		};
	argc -= optind;
	argv += optind;
	grphead = (struct grouplist *)NULL;
	exphead = (struct exportlist *)NULL;
	mlhead = (struct mountlist *)NULL;
	if (argc == 1) {
		strncpy(exname, *argv, MAXPATHLEN-1);
		exname[MAXPATHLEN-1] = '\0';
	} else
		strcpy(exname, _PATH_EXPORTS);
	openlog("mountd", LOG_PID, LOG_DAEMON);
	if (debug)
		warnx("getting export list");
	get_exportlist();
	if (debug)
		warnx("getting mount list");
	get_mountlist();
	if (debug)
		warnx("here we go");
	if (debug == 0) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGHUP, huphandler);
	signal(SIGTERM, terminate);
	{ FILE *pidfile = fopen(_PATH_MOUNTDPID, "w");
	  if (pidfile != NULL) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	  }
	}
	rpcb_unset(RPCPROG_MNT, RPCMNT_VER1, NULL);
	rpcb_unset(RPCPROG_MNT, RPCMNT_VER3, NULL);
	udpsock  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	tcpsock  = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	udpconf  = getnetconfigent("udp");
	tcpconf  = getnetconfigent("tcp");
	if (!have_v6)
		goto skip_v6;
	udp6sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	tcp6sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	/*
	 * We're doing host-based access checks here, so don't allow
	 * v4-in-v6 to confuse things. The kernel will disable it
	 * by default on NFS sockets too.
	 */
	if (udp6sock != -1 && setsockopt(udp6sock, IPPROTO_IPV6,
		IPV6_BINDV6ONLY, &one, sizeof one) < 0){
		syslog(LOG_ERR, "can't disable v4-in-v6 on UDP socket");
		exit(1);
	}
	if (tcp6sock != -1 && setsockopt(tcp6sock, IPPROTO_IPV6,
		IPV6_BINDV6ONLY, &one, sizeof one) < 0){
		syslog(LOG_ERR, "can't disable v4-in-v6 on UDP socket");
		exit(1);
	}
	udp6conf = getnetconfigent("udp6");
	tcp6conf = getnetconfigent("tcp6");

skip_v6:
	if (!resvport_only) {
		mib[0] = CTL_VFS;
		mib[1] = vfc.vfc_typenum;
		mib[2] = NFS_NFSPRIVPORT;
		if (sysctl(mib, 3, NULL, NULL, &resvport_only,
		    sizeof(resvport_only)) != 0 && errno != ENOENT) {
			syslog(LOG_ERR, "sysctl: %m");
			exit(1);
		}
	}
	if ((udptransp = svcudp_create(RPC_ANYSOCK)) == NULL ||
	    (tcptransp = svctcp_create(RPC_ANYSOCK, 0, 0)) == NULL) {
		syslog(LOG_ERR, "can't create socket");
		exit(1);
	}
	if (udpsock != -1 && udpconf != NULL) {
		bindresvport(udpsock, NULL);
		udptransp = svc_dg_create(udpsock, 0, 0);
		if (udptransp != NULL) {
			if (!svc_reg(udptransp, RPCPROG_MNT, RPCMNT_VER1,
			    mntsrv, udpconf))
				syslog(LOG_WARNING, "can't register UDP RPCMNT_VER1 service");
			else
				xcreated++;
			if (!force_v2) {
				if (!svc_reg(udptransp, RPCPROG_MNT, RPCMNT_VER3,
				    mntsrv, udpconf))
					syslog(LOG_WARNING, "can't register UDP RPCMNT_VER3 service");
				else
					xcreated++;
			}
		} else
			syslog(LOG_WARNING, "can't create UDP services");

	}
	if (tcpsock != -1 && tcpconf != NULL) {
		bindresvport(tcpsock, NULL);
		listen(tcpsock, SOMAXCONN);
		tcptransp = svc_vc_create(tcpsock, 0, 0);
		if (tcptransp != NULL) {
			if (!svc_reg(tcptransp, RPCPROG_MNT, RPCMNT_VER1,
			    mntsrv, tcpconf))
				syslog(LOG_WARNING, "can't register TCP RPCMNT_VER1 service");
			else
				xcreated++;
			if (!force_v2) {
				if (!svc_reg(tcptransp, RPCPROG_MNT, RPCMNT_VER3,
				    mntsrv, tcpconf))
					syslog(LOG_WARNING, "can't register TCP RPCMNT_VER3 service");
				else
					xcreated++;
			}
		} else
			syslog(LOG_WARNING, "can't create TCP service");

	}
	if (have_v6 && udp6sock != -1 && udp6conf != NULL) {
		bindresvport(udp6sock, NULL);
		udp6transp = svc_dg_create(udp6sock, 0, 0);
		if (udp6transp != NULL) {
			if (!svc_reg(udp6transp, RPCPROG_MNT, RPCMNT_VER1,
			    mntsrv, udp6conf))
				syslog(LOG_WARNING, "can't register UDP6 RPCMNT_VER1 service");
			else
				xcreated++;
			if (!force_v2) {
				if (!svc_reg(udp6transp, RPCPROG_MNT, RPCMNT_VER3,
				    mntsrv, udp6conf))
					syslog(LOG_WARNING, "can't register UDP6 RPCMNT_VER3 service");
				else
					xcreated++;
			}
		} else
			syslog(LOG_WARNING, "can't create UDP6 service");

	}
	if (have_v6 && tcp6sock != -1 && tcp6conf != NULL) {
		bindresvport(tcp6sock, NULL);
		listen(tcp6sock, SOMAXCONN);
		tcp6transp = svc_vc_create(tcp6sock, 0, 0);
		if (tcp6transp != NULL) {
			if (!svc_reg(tcp6transp, RPCPROG_MNT, RPCMNT_VER1,
			    mntsrv, tcp6conf))
				syslog(LOG_WARNING, "can't register TCP6 RPCMNT_VER1 service");
			else
				xcreated++;
			if (!force_v2) {
				if (!svc_reg(tcp6transp, RPCPROG_MNT, RPCMNT_VER3,
				    mntsrv, tcp6conf))
					syslog(LOG_WARNING, "can't register TCP6 RPCMNT_VER3 service");
					else
						xcreated++;
				}
		} else
			syslog(LOG_WARNING, "can't create TCP6 service");

	}
	if (xcreated == 0) {
		syslog(LOG_ERR, "could not create any services");
		exit(1);
	}

	/* Expand svc_run() here so that we can call get_exportlist(). */
	for (;;) {
		if (got_sighup) {
			get_exportlist();
			got_sighup = 0;
		}
		readfds = svc_fdset;
		switch (select(svc_maxfd + 1, &readfds, NULL, NULL, NULL)) {
		case -1:
			if (errno == EINTR)
                                continue;
			syslog(LOG_ERR, "mountd died: select: %m");
			exit(1);
		case 0:
			continue;
		default:
			svc_getreqset(&readfds);
		}
	}
}

static void
usage()
{
	fprintf(stderr,
		"usage: mountd [-2] [-d] [-l] [-n] [-r] [export_file]\n");
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
	struct stat stb;
	struct statfs fsb;
	struct addrinfo *ai;
	char host[NI_MAXHOST], numerichost[NI_MAXHOST];
	int lookup_failed = 1;
	struct sockaddr *saddr;
	u_short sport;
	char rpcpath[RPCMNT_PATHLEN + 1], dirpath[MAXPATHLEN];
	int bad = 0, defset, hostset;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	saddr = svc_getrpccaller(transp)->buf;
	switch (saddr->sa_family) {
	case AF_INET6:
		sport = ntohs(((struct sockaddr_in6 *)saddr)->sin6_port);
		break;
	case AF_INET:
		sport = ntohs(((struct sockaddr_in *)saddr)->sin_port);
		break;
	default:
		syslog(LOG_ERR, "request from unknown address family");
		return;
	}
	lookup_failed = getnameinfo(saddr, saddr->sa_len, host, sizeof host, 
	    NULL, 0, 0);
	getnameinfo(saddr, saddr->sa_len, numerichost,
	    sizeof numerichost, NULL, 0, NI_NUMERICHOST);
	ai = NULL;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		return;
	case RPCMNT_MOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "mount request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			syslog(LOG_NOTICE, "undecodable mount request from %s",
			    numerichost);
			svcerr_decode(transp);
			return;
		}

		/*
		 * Get the real pathname and make sure it is a directory
		 * or a regular file if the -r option was specified
		 * and it exists.
		 */
		if (realpath(rpcpath, dirpath) == NULL ||
		    stat(dirpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) &&
		    (dir_only || !S_ISREG(stb.st_mode))) ||
		    statfs(dirpath, &fsb) < 0) {
			chdir("/");	/* Just in case realpath doesn't */
			syslog(LOG_NOTICE,
			    "mount request from %s for non existent path %s",
			    numerichost, dirpath);
			if (debug)
				warnx("stat failed on %s", dirpath);
			bad = ENOENT;	/* We will send error reply later */
		}

		/* Check in the exports list */
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		ep = ex_search(&fsb.f_fsid);
		hostset = defset = 0;
		if (ep && (chk_host(ep->ex_defdir, saddr, &defset, &hostset) ||
		    ((dp = dirp_search(ep->ex_dirl, dirpath)) &&
		      chk_host(dp, saddr, &defset, &hostset)) ||
		    (defset && scan_tree(ep->ex_defdir, saddr) == 0 &&
		     scan_tree(ep->ex_dirl, saddr) == 0))) {
			if (bad) {
				if (!svc_sendreply(transp, xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (hostset & DP_HOSTSET)
				fhr.fhr_flag = hostset;
			else
				fhr.fhr_flag = defset;
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(nfsfh_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				bad = errno;
				syslog(LOG_ERR, "can't get fh for %s", dirpath);
				if (!svc_sendreply(transp, xdr_long,
				    (caddr_t)&bad))
					syslog(LOG_ERR, "can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (!svc_sendreply(transp, xdr_fhs, (caddr_t)&fhr))
				syslog(LOG_ERR, "can't send reply");
			if (!lookup_failed)
				add_mlist(host, dirpath);
			else
				add_mlist(numerichost, dirpath);
			if (debug)
				warnx("mount successful");
			if (log)
				syslog(LOG_NOTICE,
				    "mount request succeeded from %s for %s",
				    numerichost, dirpath);
		} else {
			bad = EACCES;
			syslog(LOG_NOTICE,
			    "mount request denied from %s for %s",
			    numerichost, dirpath);
		}

		if (bad && !svc_sendreply(transp, xdr_long, (caddr_t)&bad))
			syslog(LOG_ERR, "can't send reply");
		sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case RPCMNT_DUMP:
		if (!svc_sendreply(transp, xdr_mlist, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		else if (log)
			syslog(LOG_NOTICE,
			    "dump request succeeded from %s",
			    numerichost);
		return;
	case RPCMNT_UMOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "umount request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			syslog(LOG_NOTICE, "undecodable umount request from %s",
			    numerichost);
			svcerr_decode(transp);
			return;
		}
		if (realpath(rpcpath, dirpath) == NULL) {
			syslog(LOG_NOTICE, "umount request from %s "
			    "for non existent path %s",
			    numerichost, dirpath);
		}
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		if (!lookup_failed)
			del_mlist(host, dirpath);
		del_mlist(numerichost, dirpath);
		if (log)
			syslog(LOG_NOTICE,
			    "umount request succeeded from %s for %s",
			    numerichost, dirpath);
		return;
	case RPCMNT_UMNTALL:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			syslog(LOG_NOTICE,
			    "umountall request from %s from unprivileged port",
			    numerichost);
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		if (!lookup_failed)
			del_mlist(host, NULL);
		del_mlist(numerichost, NULL);
		if (log)
			syslog(LOG_NOTICE,
			    "umountall request succeeded from %s",
			    numerichost);
		return;
	case RPCMNT_EXPORT:
		if (!svc_sendreply(transp, xdr_explist, (caddr_t)NULL))
			syslog(LOG_ERR, "can't send reply");
		if (log)
			syslog(LOG_NOTICE,
			    "export request succeeded from %s",
			    numerichost);
		return;
	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dirpath string
 */
int
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
int
xdr_fhs(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	register struct fhreturn *fhrp = (struct fhreturn *)cp;
	u_long ok = 0, len, auth;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, NFSX_V2FH));
	case 3:
		len = NFSX_V3FH;
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
	int true = 1;
	int false = 0;
	char *strp;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &true))
			return (0);
		strp = &mlp->ml_host[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = &mlp->ml_dirp[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &false))
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
	int false = 0;
	int putdef;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
	ep = exphead;
	while (ep) {
		putdef = 0;
		if (put_exlist(ep->ex_dirl, xdrsp, ep->ex_defdir, &putdef))
			goto errout;
		if (ep->ex_defdir && putdef == 0 &&
			put_exlist(ep->ex_defdir, xdrsp, (struct dirlist *)NULL,
			&putdef))
			goto errout;
		ep = ep->ex_next;
	}
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
errout:
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to traverse the tree and export the
 * directory paths.
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
	int true = 1;
	int false = 0;
	int gotalldir = 0;
	char *strp;

	if (dp) {
		if (put_exlist(dp->dp_left, xdrsp, adp, putdefp))
			return (1);
		if (!xdr_bool(xdrsp, &true))
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
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_addrinfo->ai_canonname;
					if (!xdr_string(xdrsp, &strp,
					    RPCMNT_NAMELEN))
						return (1);
				} else if (grp->gr_type == GT_NET) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_net.nt_name;
					if (!xdr_string(xdrsp, &strp,
					    RPCMNT_NAMELEN))
						return (1);
				}
				hp = hp->ht_next;
				if (gotalldir && hp == (struct hostlist *)NULL) {
					hp = adp->dp_hosts;
					gotalldir = 0;
				}
			}
		}
		if (!xdr_bool(xdrsp, &false))
			return (1);
		if (put_exlist(dp->dp_right, xdrsp, adp, putdefp))
			return (1);
	}
	return (0);
}

#define LINESIZ	10240
char line[LINESIZ];
FILE *exp_file;

/*
 * Get the export list
 */
void
get_exportlist()
{
	struct exportlist *ep, *ep2;
	struct grouplist *grp, *tgrp;
	struct exportlist **epp;
	struct dirlist *dirhead;
	struct statfs fsb, *fsp;
	struct xucred anon;
	char *cp, *endcp, *dirp, *hst, *usr, *dom, savedc;
	int len, has_host, exflags, got_nondir, dirplen, num, i, netgrp;

	dirp = NULL;
	dirplen = 0;

	/*
	 * First, get rid of the old list
	 */
	ep = exphead;
	while (ep) {
		ep2 = ep;
		ep = ep->ex_next;
		free_exp(ep2);
	}
	exphead = (struct exportlist *)NULL;

	grp = grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	grphead = (struct grouplist *)NULL;

	/*
	 * And delete exports that are in the kernel for all local
	 * file systems.
	 * XXX: Should know how to handle all local exportable file systems
	 *      instead of just "ufs".
	 */
	num = getmntinfo(&fsp, MNT_NOWAIT);
	for (i = 0; i < num; i++) {
		union {
			struct ufs_args ua;
			struct iso_args ia;
			struct mfs_args ma;
			struct msdosfs_args da;
			struct ntfs_args na;
		} targs;

		if (!strcmp(fsp->f_fstypename, "mfs") ||
		    !strcmp(fsp->f_fstypename, "ufs") ||
		    !strcmp(fsp->f_fstypename, "msdos") ||
		    !strcmp(fsp->f_fstypename, "ntfs") ||
		    !strcmp(fsp->f_fstypename, "cd9660")) {
			targs.ua.fspec = NULL;
			targs.ua.export.ex_flags = MNT_DELEXPORT;
			if (mount(fsp->f_fstypename, fsp->f_mntonname,
				  fsp->f_flags | MNT_UPDATE,
				  (caddr_t)&targs) < 0)
				syslog(LOG_ERR, "can't delete exports for %s",
				    fsp->f_mntonname);
		}
		fsp++;
	}

	/*
	 * Read in the exports file and build the list, calling
	 * mount() as we go along to push the export rules into the kernel.
	 */
	if ((exp_file = fopen(exname, "r")) == NULL) {
		syslog(LOG_ERR, "can't open %s", exname);
		exit(2);
	}
	dirhead = (struct dirlist *)NULL;
	while (get_line()) {
		if (debug)
			warnx("got line %s", line);
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;

		/*
		 * Set defaults.
		 */
		has_host = FALSE;
		anon = def_anon;
		exflags = MNT_EXPORTED;
		got_nondir = 0;
		opt_flags = 0;
		ep = (struct exportlist *)NULL;

		/*
		 * Create new exports list entry
		 */
		len = endcp-cp;
		tgrp = grp = get_grp();
		while (len > 0) {
			if (len > RPCMNT_NAMELEN) {
			    getexp_err(ep, tgrp);
			    goto nextline;
			}
			if (*cp == '-') {
			    if (ep == (struct exportlist *)NULL) {
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			    if (debug)
				warnx("doing opt %s", cp);
			    got_nondir = 1;
			    if (do_opt(&cp, &endcp, ep, grp, &has_host,
				&exflags, &anon)) {
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			} else if (*cp == '/') {
			    savedc = *endcp;
			    *endcp = '\0';
			    if (check_dirpath(cp) &&
				statfs(cp, &fsb) >= 0) {
				if (got_nondir) {
				    syslog(LOG_ERR, "dirs must be first");
				    getexp_err(ep, tgrp);
				    goto nextline;
				}
				if (ep) {
				    if (ep->ex_fs.val[0] != fsb.f_fsid.val[0] ||
					ep->ex_fs.val[1] != fsb.f_fsid.val[1]) {
					getexp_err(ep, tgrp);
					goto nextline;
				    }
				} else {
				    /*
				     * See if this directory is already
				     * in the list.
				     */
				    ep = ex_search(&fsb.f_fsid);
				    if (ep == (struct exportlist *)NULL) {
					ep = get_exp();
					ep->ex_fs = fsb.f_fsid;
					ep->ex_fsdir = (char *)
					    malloc(strlen(fsb.f_mntonname) + 1);
					if (ep->ex_fsdir)
					    strcpy(ep->ex_fsdir,
						fsb.f_mntonname);
					else
					    out_of_mem();
					if (debug)
						warnx("making new ep fs=0x%x,0x%x",
						    fsb.f_fsid.val[0],
						    fsb.f_fsid.val[1]);
				    } else if (debug)
					warnx("found ep fs=0x%x,0x%x",
					    fsb.f_fsid.val[0],
					    fsb.f_fsid.val[1]);
				}

				/*
				 * Add dirpath to export mount point.
				 */
				dirp = add_expdir(&dirhead, cp, len);
				dirplen = len;
			    } else {
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			    *endcp = savedc;
			} else {
			    savedc = *endcp;
			    *endcp = '\0';
			    got_nondir = 1;
			    if (ep == (struct exportlist *)NULL) {
				getexp_err(ep, tgrp);
				goto nextline;
			    }

			    /*
			     * Get the host or netgroup.
			     */
			    setnetgrent(cp);
			    netgrp = getnetgrent(&hst, &usr, &dom);
			    do {
				if (has_host) {
				    grp->gr_next = get_grp();
				    grp = grp->gr_next;
				}
				if (netgrp) {
				    if (hst == 0) {
					syslog(LOG_ERR,
				"null hostname in netgroup %s, skipping", cp);
					grp->gr_type = GT_IGNORE;
				    } else if (get_host(hst, grp, tgrp)) {
					syslog(LOG_ERR,
			"bad host %s in netgroup %s, skipping", hst, cp);
					grp->gr_type = GT_IGNORE;
				    }
				} else if (get_host(cp, grp, tgrp)) {
				    syslog(LOG_ERR, "bad host %s, skipping", cp);
				    grp->gr_type = GT_IGNORE;
				}
				has_host = TRUE;
			    } while (netgrp && getnetgrent(&hst, &usr, &dom));
			    endnetgrent();
			    *endcp = savedc;
			}
			cp = endcp;
			nextfield(&cp, &endcp);
			len = endcp - cp;
		}
		if (check_options(dirhead)) {
			getexp_err(ep, tgrp);
			goto nextline;
		}
		if (!has_host) {
			grp->gr_type = GT_DEFAULT;
			if (debug)
				warnx("adding a default entry");

		/*
		 * Don't allow a network export coincide with a list of
		 * host(s) on the same line.
		 */
		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			syslog(LOG_ERR, "network/host conflict");
			getexp_err(ep, tgrp);
			goto nextline;

		/*
		 * If an export list was specified on this line, make sure
		 * that we have at least one valid entry, otherwise skip it.
		 */
		} else {
			grp = tgrp;
			while (grp && grp->gr_type == GT_IGNORE)
				grp = grp->gr_next;
			if (! grp) {
			    getexp_err(ep, tgrp);
			    goto nextline;
			}
		}

		/*
		 * Loop through hosts, pushing the exports into the kernel.
		 * After loop, tgrp points to the start of the list and
		 * grp points to the last entry in the list.
		 */
		grp = tgrp;
		do {
			if (do_mount(ep, grp, exflags, &anon, dirp, dirplen,
			    &fsb)) {
				getexp_err(ep, tgrp);
				goto nextline;
			}
		} while (grp->gr_next && (grp = grp->gr_next));

		/*
		 * Success. Update the data structures.
		 */
		if (has_host) {
			hang_dirp(dirhead, tgrp, ep, opt_flags);
			grp->gr_next = grphead;
			grphead = tgrp;
		} else {
			hang_dirp(dirhead, (struct grouplist *)NULL, ep,
				opt_flags);
			free_grp(grp);
		}
		dirhead = (struct dirlist *)NULL;
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
nextline:
		if (dirhead) {
			free_dir(dirhead);
			dirhead = (struct dirlist *)NULL;
		}
	}
	fclose(exp_file);
}

/*
 * Allocate an export list element
 */
struct exportlist *
get_exp()
{
	struct exportlist *ep;

	ep = (struct exportlist *)malloc(sizeof (struct exportlist));
	if (ep == (struct exportlist *)NULL)
		out_of_mem();
	memset(ep, 0, sizeof(struct exportlist));
	return (ep);
}

/*
 * Allocate a group list element
 */
struct grouplist *
get_grp()
{
	struct grouplist *gp;

	gp = (struct grouplist *)malloc(sizeof (struct grouplist));
	if (gp == (struct grouplist *)NULL)
		out_of_mem();
	memset(gp, 0, sizeof(struct grouplist));
	return (gp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
void
getexp_err(ep, grp)
	struct exportlist *ep;
	struct grouplist *grp;
{
	struct grouplist *tgrp;

	syslog(LOG_ERR, "bad exports list line %s", line);
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
struct exportlist *
ex_search(fsid)
	fsid_t *fsid;
{
	struct exportlist *ep;

	ep = exphead;
	while (ep) {
		if (ep->ex_fs.val[0] == fsid->val[0] &&
		    ep->ex_fs.val[1] == fsid->val[1])
			return (ep);
		ep = ep->ex_next;
	}
	return (ep);
}

/*
 * Add a directory path to the list.
 */
char *
add_expdir(dpp, cp, len)
	struct dirlist **dpp;
	char *cp;
	int len;
{
	struct dirlist *dp;

	dp = (struct dirlist *)malloc(sizeof (struct dirlist) + len);
	if (dp == (struct dirlist *)NULL)
		out_of_mem();
	dp->dp_left = *dpp;
	dp->dp_right = (struct dirlist *)NULL;
	dp->dp_flag = 0;
	dp->dp_hosts = (struct hostlist *)NULL;
	strcpy(dp->dp_dirp, cp);
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
			free((caddr_t)dp);
		else
			ep->ex_defdir = dp;
		if (grp == (struct grouplist *)NULL) {
			ep->ex_defdir->dp_flag |= DP_DEFSET;
			if (flags & OP_KERB)
				ep->ex_defdir->dp_flag |= DP_KERB;
		} else while (grp) {
			hp = get_ht();
			if (flags & OP_KERB)
				hp->ht_flag |= DP_KERB;
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
void
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
			free((caddr_t)newdp);
	} else {
		dp = newdp;
		dp->dp_left = (struct dirlist *)NULL;
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
			hp->ht_grp = grp;
			hp->ht_next = dp->dp_hosts;
			dp->dp_hosts = hp;
			grp = grp->gr_next;
		} while (grp);
	} else {
		dp->dp_flag |= DP_DEFSET;
		if (flags & OP_KERB)
			dp->dp_flag |= DP_KERB;
	}
}

/*
 * Search for a dirpath on the export point.
 */
struct dirlist *
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
 * Scan for a host match in a directory tree.
 */
int
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
					if (!sacmp(ai->ai_addr, saddr, NULL)) {
						*hostsetp =
						    (hp->ht_flag | DP_HOSTSET);
						return (1);
					}
				}
				break;
			case GT_NET:
				if (!sacmp(saddr, (struct sockaddr *)
				    &grp->gr_ptr.gt_net.nt_net,
				    (struct sockaddr *)
				    &grp->gr_ptr.gt_net.nt_mask)) {
					*hostsetp = (hp->ht_flag | DP_HOSTSET);
					return (1);
				}
				break;
			}
			hp = hp->ht_next;
		}
	}
	return (0);
}

/*
 * Scan tree for a host that matches the address.
 */
int
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
void
free_dir(dp)
	struct dirlist *dp;
{

	if (dp) {
		free_dir(dp->dp_left);
		free_dir(dp->dp_right);
		free_host(dp->dp_hosts);
		free((caddr_t)dp);
	}
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
int
do_opt(cpp, endcpp, ep, grp, has_hostp, exflagsp, cr)
	char **cpp, **endcpp;
	struct exportlist *ep;
	struct grouplist *grp;
	int *has_hostp;
	int *exflagsp;
	struct xucred *cr;
{
	char *cpoptarg, *cpoptend;
	char *cp, *endcp, *cpopt, savedc, savedc2;
	int allflag, usedarg;

	savedc2 = '\0';
	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		if ((cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if ((cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
		} else {
			if ((cpoptarg = strchr(cpopt, '=')))
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
				syslog(LOG_ERR, "bad mask: %s", cpoptarg);
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
				syslog(LOG_ERR, "network/host conflict");
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 0)) {
				syslog(LOG_ERR, "bad net: %s", cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			opt_flags |= OP_ALLDIRS;
		} else if (!strcmp(cpopt, "public")) {
			*exflagsp |= MNT_EXPUBLIC;
		} else if (!strcmp(cpopt, "webnfs")) {
			*exflagsp |= (MNT_EXPUBLIC|MNT_EXRDONLY|MNT_EXPORTANON);
			opt_flags |= OP_MAPALL;
		} else if (cpoptarg && !strcmp(cpopt, "index")) {
			ep->ex_indexfile = strdup(cpoptarg);
		} else {
			syslog(LOG_ERR, "bad opt %s", cpopt);
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
int
get_host(cp, grp, tgrp)
	char *cp;
	struct grouplist *grp;
	struct grouplist *tgrp;
{
	struct grouplist *checkgrp;
	struct addrinfo *ai, *tai, hints;
	int ecode;
	char host[NI_MAXHOST];

	if (grp->gr_type != GT_NULL) {
		syslog(LOG_ERR, "Bad netgroup type for ip host %s", cp);
		return (1);
	}
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_CANONNAME;
	hints.ai_protocol = IPPROTO_UDP;
	ecode = getaddrinfo(cp, NULL, &hints, &ai);
	if (ecode != 0) {
		syslog(LOG_ERR,"can't get address info for host %s", cp);
		return 1;
	}
	grp->gr_ptr.gt_addrinfo = ai;
	while (ai != NULL) {
		if (ai->ai_canonname == NULL) {
			if (getnameinfo(ai->ai_addr, ai->ai_addrlen, host,
			    sizeof host, NULL, 0, ninumeric) != 0)
				strlcpy(host, "?", sizeof(host));
			ai->ai_canonname = strdup(host);
			ai->ai_flags |= AI_CANONNAME;
		}
		if (debug)
			fprintf(stderr, "got host %s\n", ai->ai_canonname);
		/*
		 * Sanity check: make sure we don't already have an entry
		 * for this host in the grouplist.
		 */
		for (checkgrp = tgrp; checkgrp != NULL;
		    checkgrp = checkgrp->gr_next) {
			if (checkgrp->gr_type != GT_HOST)
				continue;
			for (tai = checkgrp->gr_ptr.gt_addrinfo; tai != NULL;
			    tai = tai->ai_next) {
				if (sacmp(tai->ai_addr, ai->ai_addr, NULL) != 0)
					continue;
				if (debug)
					fprintf(stderr,
					    "ignoring duplicate host %s\n",
					    ai->ai_canonname);
				grp->gr_type = GT_IGNORE;
				return (0);
			}
		}
		ai = ai->ai_next;
	}
	grp->gr_type = GT_HOST;
	return (0);
}

/*
 * Free up an exports list component
 */
void
free_exp(ep)
	struct exportlist *ep;
{

	if (ep->ex_defdir) {
		free_host(ep->ex_defdir->dp_hosts);
		free((caddr_t)ep->ex_defdir);
	}
	if (ep->ex_fsdir)
		free(ep->ex_fsdir);
	if (ep->ex_indexfile)
		free(ep->ex_indexfile);
	free_dir(ep->ex_dirl);
	free((caddr_t)ep);
}

/*
 * Free hosts.
 */
void
free_host(hp)
	struct hostlist *hp;
{
	struct hostlist *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free((caddr_t)hp2);
	}
}

struct hostlist *
get_ht()
{
	struct hostlist *hp;

	hp = (struct hostlist *)malloc(sizeof (struct hostlist));
	if (hp == (struct hostlist *)NULL)
		out_of_mem();
	hp->ht_next = (struct hostlist *)NULL;
	hp->ht_flag = 0;
	return (hp);
}

/*
 * Out of memory, fatal
 */
void
out_of_mem()
{

	syslog(LOG_ERR, "out of memory");
	exit(2);
}

/*
 * Do the mount syscall with the update flag to push the export info into
 * the kernel.
 */
int
do_mount(ep, grp, exflags, anoncrp, dirp, dirplen, fsb)
	struct exportlist *ep;
	struct grouplist *grp;
	int exflags;
	struct xucred *anoncrp;
	char *dirp;
	int dirplen;
	struct statfs *fsb;
{
	struct statfs fsb1;
	struct addrinfo *ai;
	struct export_args *eap;
	char *cp = NULL;
	int done;
	char savedc = '\0';
	union {
		struct ufs_args ua;
		struct iso_args ia;
		struct mfs_args ma;
		struct msdosfs_args da;
		struct ntfs_args na;
	} args;

	bzero(&args, sizeof args);
	/* XXX, we assume that all xx_args look like ufs_args. */
	args.ua.fspec = 0;
	eap = &args.ua.export;

	eap->ex_flags = exflags;
	eap->ex_anon = *anoncrp;
	eap->ex_indexfile = ep->ex_indexfile;
	if (grp->gr_type == GT_HOST)
		ai = grp->gr_ptr.gt_addrinfo;
	else
		ai = NULL;
	done = FALSE;
	while (!done) {
		switch (grp->gr_type) {
		case GT_HOST:
			if (ai->ai_addr->sa_family == AF_INET6 && have_v6 == 0)
				goto skip;
			eap->ex_addr = ai->ai_addr;
			eap->ex_addrlen = ai->ai_addrlen;
			eap->ex_masklen = 0;
			break;
		case GT_NET:
			if (grp->gr_ptr.gt_net.nt_net.ss_family == AF_INET6 &&
			    have_v6 == 0)
				goto skip;
			eap->ex_addr =
			    (struct sockaddr *)&grp->gr_ptr.gt_net.nt_net;
			eap->ex_addrlen = args.ua.export.ex_addr->sa_len;
			eap->ex_mask =
			    (struct sockaddr *)&grp->gr_ptr.gt_net.nt_mask;
			eap->ex_masklen = args.ua.export.ex_mask->sa_len;
			break;
		case GT_DEFAULT:
			eap->ex_addr = NULL;
			eap->ex_addrlen = 0;
			eap->ex_mask = NULL;
			eap->ex_masklen = 0;
			break;
		case GT_IGNORE:
			return(0);
			break;
		default:
			syslog(LOG_ERR, "bad grouptype");
			if (cp)
				*cp = savedc;
			return (1);
		};

		/*
		 * XXX:
		 * Maybe I should just use the fsb->f_mntonname path instead
		 * of looping back up the dirp to the mount point??
		 * Also, needs to know how to export all types of local
		 * exportable file systems and not just "ufs".
		 */
		while (mount(fsb->f_fstypename, dirp,
		    fsb->f_flags | MNT_UPDATE, (caddr_t)&args) < 0) {
			if (cp)
				*cp-- = savedc;
			else
				cp = dirp + dirplen - 1;
			if (errno == EPERM) {
				if (debug)
					warnx("can't change attributes for %s",
					    dirp);
				syslog(LOG_ERR,
				   "can't change attributes for %s", dirp);
				return (1);
			}
			if (opt_flags & OP_ALLDIRS) {
				syslog(LOG_ERR, "could not remount %s: %m",
					dirp);
				return (1);
			}
			/* back up over the last component */
			while (*cp == '/' && cp > dirp)
				cp--;
			while (*(cp - 1) != '/' && cp > dirp)
				cp--;
			if (cp == dirp) {
				if (debug)
					warnx("mnt unsucc");
				syslog(LOG_ERR, "can't export %s", dirp);
				return (1);
			}
			savedc = *cp;
			*cp = '\0';
			/* Check that we're still on the same filesystem. */
			if (statfs(dirp, &fsb1) != 0 || bcmp(&fsb1.f_fsid,
			    &fsb->f_fsid, sizeof(fsb1.f_fsid)) != 0) {
				*cp = savedc;
				syslog(LOG_ERR, "can't export %s", dirp);
				return (1);
			}
		}
skip:
		if (ai != NULL)
			ai = ai->ai_next;
		if (ai == NULL)
			done = TRUE;
	}
	if (cp)
		*cp = savedc;
	return (0);
}

/*
 * Translate a net address.
 *
 * If `maskflg' is nonzero, then `cp' is a netmask, not a network address.
 */
int
get_net(cp, net, maskflg)
	char *cp;
	struct netmsk *net;
	int maskflg;
{
	struct netent *np;
	char *name, *p, *prefp;
	struct sockaddr_in sin;
	struct sockaddr *sa;
	struct addrinfo hints, *ai = NULL;
	char netname[NI_MAXHOST];
	long preflen;

	p = prefp = NULL;
	if ((opt_flags & OP_MASKLEN) && !maskflg) {
		p = strchr(cp, '/');
		*p = '\0';
		prefp = p + 1;
	}

	if ((np = getnetbyname(cp)) != NULL) {
		bzero(&sin, sizeof sin);
		sin.sin_family = AF_INET;
		sin.sin_len = sizeof sin;
		sin.sin_addr = inet_makeaddr(np->n_net, 0);
		sa = (struct sockaddr *)&sin;
	} else if (isxdigit(*cp) || *cp == ':') {
		memset(&hints, 0, sizeof hints);
		/* Ensure the mask and the network have the same family. */
		if (maskflg && (opt_flags & OP_NET))
			hints.ai_family = net->nt_net.ss_family;
		else if (!maskflg && (opt_flags & OP_HAVEMASK))
			hints.ai_family = net->nt_mask.ss_family;
		else
			hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;
		if (getaddrinfo(cp, NULL, &hints, &ai) != 0)
			goto fail;
		if (ai->ai_family == AF_INET) {
			/*
			 * The address in `cp' is really a network address, so
			 * use inet_network() to re-interpret this correctly.
			 * e.g. "127.1" means 127.1.0.0, not 127.0.0.1.
			 */
			bzero(&sin, sizeof sin);
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof sin;
			sin.sin_addr = inet_makeaddr(inet_network(cp), 0);
			if (debug)
				fprintf(stderr, "get_net: v4 addr %s\n",
				    inet_ntoa(sin.sin_addr));
			sa = (struct sockaddr *)&sin;
		} else
			sa = ai->ai_addr;
	} else
		goto fail;

	if (maskflg) {
		/* The specified sockaddr is a mask. */
		if (checkmask(sa) != 0)
			goto fail;
		bcopy(sa, &net->nt_mask, sa->sa_len);
		opt_flags |= OP_HAVEMASK;
	} else {
		/* The specified sockaddr is a network address. */
		bcopy(sa, &net->nt_net, sa->sa_len);

		/* Get a network name for the export list. */
		if (np) {
			name = np->n_name;
		} else if (getnameinfo(sa, sa->sa_len, netname, sizeof netname,
		   NULL, 0, ninumeric) == 0) {
			name = netname;
		} else {
			goto fail;
		}
		if ((net->nt_name = strdup(name)) == NULL)
			out_of_mem();

		/*
		 * Extract a mask from either a "/<masklen>" suffix, or
		 * from the class of an IPv4 address.
		 */
		if (opt_flags & OP_MASKLEN) {
			preflen = strtol(prefp, NULL, 10);
			if (preflen < 0L || preflen == LONG_MAX)
				goto fail;
			bcopy(sa, &net->nt_mask, sa->sa_len);
			if (makemask(&net->nt_mask, (int)preflen) != 0)
				goto fail;
			opt_flags |= OP_HAVEMASK;
			*p = '/';
		} else if (sa->sa_family == AF_INET &&
		    (opt_flags & OP_MASK) == 0) {
			in_addr_t addr;

			addr = ((struct sockaddr_in *)sa)->sin_addr.s_addr;
			if (IN_CLASSA(addr))
				preflen = 8;
			else if (IN_CLASSB(addr))
				preflen = 16;
			else if (IN_CLASSC(addr))
				preflen = 24;
			else if (IN_CLASSD(addr))
				preflen = 28;
			else
				preflen = 32;	/* XXX */

			bcopy(sa, &net->nt_mask, sa->sa_len);
			makemask(&net->nt_mask, (int)preflen);
			opt_flags |= OP_HAVEMASK;
		}
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
void
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
 * Get an exports file line. Skip over blank lines and handle line
 * continuations.
 */
int
get_line()
{
	char *p, *cp;
	int len;
	int totlen, cont_line;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	p = line;
	totlen = 0;
	do {
		if (fgets(p, LINESIZ - totlen, exp_file) == NULL)
			return (0);
		len = strlen(p);
		cp = p + len - 1;
		cont_line = 0;
		while (cp >= p &&
		    (*cp == ' ' || *cp == '\t' || *cp == '\n' || *cp == '\\')) {
			if (*cp == '\\')
				cont_line = 1;
			cp--;
			len--;
		}
		*++cp = '\0';
		if (len > 0) {
			totlen += len;
			if (totlen >= LINESIZ) {
				syslog(LOG_ERR, "exports line too long");
				exit(2);
			}
			p = cp;
		}
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
void
parsecred(namelist, cr)
	char *namelist;
	struct xucred *cr;
{
	char *name;
	int cnt;
	char *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups, groups[NGROUPS + 1];

	/*
	 * Set up the unprivileged user.
	 */
	cr->cr_uid = -2;
	cr->cr_groups[0] = -2;
	cr->cr_ngroups = 1;
	/*
	 * Get the user's password table entry.
	 */
	names = strsep(&namelist, " \t\n");
	name = strsep(&names, ":");
	if (isdigit(*name) || *name == '-')
		pw = getpwuid(atoi(name));
	else
		pw = getpwnam(name);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			syslog(LOG_ERR, "unknown user: %s", name);
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups))
			syslog(LOG_ERR, "too many groups");
		/*
		 * Convert from int's to gid_t's and compress out duplicate
		 */
		cr->cr_ngroups = ngroups - 1;
		cr->cr_groups[0] = groups[0];
		for (cnt = 2; cnt < ngroups; cnt++)
			cr->cr_groups[cnt - 1] = groups[cnt];
		return;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL)
		cr->cr_uid = pw->pw_uid;
	else if (isdigit(*name) || *name == '-')
		cr->cr_uid = atoi(name);
	else {
		syslog(LOG_ERR, "unknown user: %s", name);
		return;
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS) {
		name = strsep(&names, ":");
		if (isdigit(*name) || *name == '-') {
			cr->cr_groups[cr->cr_ngroups++] = atoi(name);
		} else {
			if ((gr = getgrnam(name)) == NULL) {
				syslog(LOG_ERR, "unknown group: %s", name);
				continue;
			}
			cr->cr_groups[cr->cr_ngroups++] = gr->gr_gid;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS)
		syslog(LOG_ERR, "too many groups");
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
void
get_mountlist()
{
	struct mountlist *mlp, **mlpp;
	char *host, *dirp, *cp;
	char str[STRSIZ];
	FILE *mlfile;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		if (errno == ENOENT)
			return;
		else {
			syslog(LOG_ERR, "can't open %s", _PATH_RMOUNTLIST);
			return;
		}
	}
	mlpp = &mlhead;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dirp = strsep(&cp, " \t\n");
		if (host == NULL || dirp == NULL)
			continue;
		mlp = (struct mountlist *)malloc(sizeof (*mlp));
		if (mlp == (struct mountlist *)NULL)
			out_of_mem();
		strncpy(mlp->ml_host, host, RPCMNT_NAMELEN);
		mlp->ml_host[RPCMNT_NAMELEN] = '\0';
		strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
		mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
		mlp->ml_next = (struct mountlist *)NULL;
		*mlpp = mlp;
		mlpp = &mlp->ml_next;
	}
	fclose(mlfile);
}

void
del_mlist(char *hostp, char *dirp)
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	FILE *mlfile;
	int fnd = 0;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) &&
		    (!dirp || !strcmp(mlp->ml_dirp, dirp))) {
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free((caddr_t)mlp2);
		} else {
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			syslog(LOG_ERR,"can't update %s", _PATH_RMOUNTLIST);
			return;
		}
		mlp = mlhead;
		while (mlp) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
			mlp = mlp->ml_next;
		}
		fclose(mlfile);
	}
}

void
add_mlist(hostp, dirp)
	char *hostp, *dirp;
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
	mlp = (struct mountlist *)malloc(sizeof (*mlp));
	if (mlp == (struct mountlist *)NULL)
		out_of_mem();
	strncpy(mlp->ml_host, hostp, RPCMNT_NAMELEN);
	mlp->ml_host[RPCMNT_NAMELEN] = '\0';
	strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
	mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
	mlp->ml_next = (struct mountlist *)NULL;
	*mlpp = mlp;
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		syslog(LOG_ERR, "can't update %s", _PATH_RMOUNTLIST);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
	fclose(mlfile);
}

/*
 * Free up a group list.
 */
void
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
	free((caddr_t)grp);
}

#ifdef DEBUG
void
SYSLOG(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}
#endif /* DEBUG */

/*
 * Check options for consistency.
 */
int
check_options(dp)
	struct dirlist *dp;
{

	if (dp == (struct dirlist *)NULL)
	    return (1);
	if ((opt_flags & (OP_MAPROOT | OP_MAPALL)) == (OP_MAPROOT | OP_MAPALL) ||
	    (opt_flags & (OP_MAPROOT | OP_KERB)) == (OP_MAPROOT | OP_KERB) ||
	    (opt_flags & (OP_MAPALL | OP_KERB)) == (OP_MAPALL | OP_KERB)) {
	    syslog(LOG_ERR, "-mapall, -maproot and -kerb mutually exclusive");
	    return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		syslog(LOG_ERR, "-mask requires -network");
		return (1);
	}
	if ((opt_flags & OP_NET) && (opt_flags & OP_HAVEMASK) == 0) {
		syslog(LOG_ERR, "-network requires mask specification");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_MASKLEN)) {
		syslog(LOG_ERR, "-mask and /masklen are mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_ALLDIRS) && dp->dp_left) {
	    syslog(LOG_ERR, "-alldirs has multiple directories");
	    return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 */
int
check_dirpath(dirp)
	char *dirp;
{
	char *cp;
	int ret = 1;
	struct stat sb;

	cp = dirp + 1;
	while (*cp && ret) {
		if (*cp == '/') {
			*cp = '\0';
			if (lstat(dirp, &sb) < 0 || !S_ISDIR(sb.st_mode))
				ret = 0;
			*cp = '/';
		}
		cp++;
	}
	if (lstat(dirp, &sb) < 0 || !S_ISDIR(sb.st_mode))
		ret = 0;
	return (ret);
}

/*
 * Make a netmask according to the specified prefix length. The ss_family
 * and other non-address fields must be initialised before calling this.
 */
int
makemask(struct sockaddr_storage *ssp, int bitlen)
{
	u_char *p;
	int bits, i, len;

	if ((p = sa_rawaddr((struct sockaddr *)ssp, &len)) == NULL)
		return (-1);
	if (bitlen > len * NBBY)
		return (-1);

	for (i = 0; i < len; i++) {
		bits = (bitlen > NBBY) ? NBBY : bitlen;
		*p++ = (1 << bits) - 1;
		bitlen -= bits;
	}
	return 0;
}

/*
 * Check that the sockaddr is a valid netmask. Returns 0 if the mask
 * is acceptable (i.e. of the form 1...10....0).
 */
int
checkmask(struct sockaddr *sa)
{
	u_char *mask;
	int i, len;

	if ((mask = sa_rawaddr(sa, &len)) == NULL)
		return (-1);

	for (i = 0; i < len; i++)
		if (mask[i] != 0xff)
			break;
	if (i < len) {
		if (~mask[i] & (u_char)(~mask[i] + 1))
			return (-1);
		i++;
	}
	for (; i < len; i++)
		if (mask[i] != 0)
			return (-1);
	return (0);
}

/*
 * Compare two sockaddrs according to a specified mask. Return zero if
 * `sa1' matches `sa2' when filtered by the netmask in `samask'.
 * If samask is NULL, perform a full comparision.
 */
int
sacmp(struct sockaddr *sa1, struct sockaddr *sa2, struct sockaddr *samask)
{
	unsigned char *p1, *p2, *mask;
	int len, i;

	if (sa1->sa_family != sa2->sa_family ||
	    (p1 = sa_rawaddr(sa1, &len)) == NULL ||
	    (p2 = sa_rawaddr(sa2, NULL)) == NULL)
		return (1);

	switch (sa1->sa_family) {
	case AF_INET6:
		if (((struct sockaddr_in6 *)sa1)->sin6_scope_id !=
		    ((struct sockaddr_in6 *)sa2)->sin6_scope_id)
			return (1);
		break;
	}

	/* Simple binary comparison if no mask specified. */
	if (samask == NULL)
		return (memcmp(p1, p2, len));

	/* Set up the mask, and do a mask-based comparison. */
	if (sa1->sa_family != samask->sa_family ||
	    (mask = sa_rawaddr(samask, NULL)) == NULL)
		return (1);

	for (i = 0; i < len; i++)
		if ((p1[i] & mask[i]) != (p2[i] & mask[i]))
			return (1);
	return (0);
}

/*
 * Return a pointer to the part of the sockaddr that contains the
 * raw address, and set *nbytes to its length in bytes. Returns
 * NULL if the address family is unknown.
 */
void *
sa_rawaddr(struct sockaddr *sa, int *nbytes) {
	void *p;
	int len;

	switch (sa->sa_family) {
	case AF_INET:
		len = sizeof(((struct sockaddr_in *)sa)->sin_addr);
		p = &((struct sockaddr_in *)sa)->sin_addr;
		break;
	case AF_INET6:
		len = sizeof(((struct sockaddr_in6 *)sa)->sin6_addr);
		p = &((struct sockaddr_in6 *)sa)->sin6_addr;
		break;
	default:
		p = NULL;
		len = 0;
	}

	if (nbytes != NULL)
		*nbytes = len;
	return (p);
}

void
huphandler(int sig)
{
	got_sighup = 1;
}

void terminate(sig)
int sig;
{
	close(mountdlockfd);
	unlink(MOUNTDLOCK);
	rpcb_unset(RPCPROG_MNT, RPCMNT_VER1, NULL);
	rpcb_unset(RPCPROG_MNT, RPCMNT_VER3, NULL);
	exit (0);
}
