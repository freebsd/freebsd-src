/*
 * Copyright (c) 1992, 1993, 1994
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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mount_nfs.c	8.11 (Berkeley) 5/4/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfsclient/nfs.h>
#include <nfsclient/nfsargs.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <unistd.h>

#include "mntopts.h"
#include "mounttab.h"

#define	ALTF_BG		0x1
#define ALTF_NOCONN	0x2
#define ALTF_DUMBTIMR	0x4
#define ALTF_INTR	0x8
#define ALTF_NFSV3	0x20
#define ALTF_RDIRPLUS	0x40
#define ALTF_MNTUDP	0x80
#define ALTF_RESVPORT	0x100
#define ALTF_SEQPACKET	0x200
#define ALTF_SOFT	0x800
#define ALTF_TCP	0x1000
#define ALTF_PORT	0x2000
#define ALTF_NFSV2	0x4000
#define ALTF_ACREGMIN	0x8000
#define ALTF_ACREGMAX	0x10000
#define ALTF_ACDIRMIN	0x20000
#define ALTF_ACDIRMAX	0x40000
#define ALTF_NOLOCKD	0x80000
#define ALTF_NOINET4	0x100000
#define ALTF_NOINET6	0x200000

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	MOPT_ASYNC,
	{ "bg", 0, ALTF_BG, 1 },
	{ "conn", 1, ALTF_NOCONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
	{ "nfsv3", 0, ALTF_NFSV3, 1 },
	{ "rdirplus", 0, ALTF_RDIRPLUS, 1 },
	{ "mntudp", 0, ALTF_MNTUDP, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ "port=", 0, ALTF_PORT, 1 },
	{ "nfsv2", 0, ALTF_NFSV2, 1 },
	{ "acregmin=", 0, ALTF_ACREGMIN, 1 },
	{ "acregmax=", 0, ALTF_ACREGMAX, 1 },
	{ "acdirmin=", 0, ALTF_ACDIRMIN, 1 },
	{ "acdirmax=", 0, ALTF_ACDIRMAX, 1 },
	{ "lockd", 1, ALTF_NOLOCKD, 1 },
	{ "inet4", 1, ALTF_NOINET4, 1 },
	{ "inet6", 1, ALTF_NOINET6, 1 },
	{ NULL }
};

struct nfs_args nfsdefargs = {
	NFS_ARGSVERSION,
	NULL,
	sizeof (struct sockaddr_in),
	SOCK_DGRAM,
	0,
	NULL,
	0,
	NFSMNT_RESVPORT,
	NFS_WSIZE,
	NFS_RSIZE,
	NFS_READDIRSIZE,
	10,
	NFS_RETRANS,
	NFS_MAXGRPS,
	NFS_DEFRAHEAD,
	0,			/* was: NQ_DEFLEASE */
	NFS_MAXDEADTHRESH,	/* was: NQ_DEADTHRESH */
	NULL,
	/* args version 4 */
	NFS_MINATTRTIMO,
	NFS_MAXATTRTIMO,
	NFS_MINDIRATTRTIMO,
	NFS_MAXDIRATTRTIMO,
};

/* Table for af,sotype -> netid conversions. */
struct nc_protos {
	char *netid;
	int af;
	int sotype;
} nc_protos[] = {
	{"udp",		AF_INET,	SOCK_DGRAM},
	{"tcp",		AF_INET,	SOCK_STREAM},
	{"udp6",	AF_INET6,	SOCK_DGRAM},
	{"tcp6",	AF_INET6,	SOCK_STREAM},
	{NULL}
};

struct nfhret {
	u_long		stat;
	long		vers;
	long		auth;
	long		fhsize;
	u_char		nfh[NFSX_V3FHMAX];
};
#define	BGRND	1
#define	ISBGRND	2
#define	OF_NOINET4	4
#define	OF_NOINET6	8
int retrycnt = -1;
int opflags = 0;
int nfsproto = IPPROTO_UDP;
int mnttcp_ok = 1;
char *portspec = NULL;	/* Server nfs port; NULL means look up via rpcbind. */
enum mountmode {
	ANY,
	V2,
	V3
} mountmode = ANY;

/* Return codes for nfs_tryproto. */
enum tryret {
	TRYRET_SUCCESS,
	TRYRET_TIMEOUT,		/* No response received. */
	TRYRET_REMOTEERR,	/* Error received from remote server. */
	TRYRET_LOCALERR		/* Local failure. */
};

int	getnfsargs(char *, struct nfs_args *);
/* void	set_rpc_maxgrouplist(int); */
struct netconfig *getnetconf_cached(const char *netid);
char	*netidbytype(int af, int sotype);
void	usage(void) __dead2;
int	xdr_dir(XDR *, char *);
int	xdr_fh(XDR *, struct nfhret *);
enum tryret nfs_tryproto(struct nfs_args *nfsargsp, struct addrinfo *ai,
    char *hostp, char *spec, char **errstr);
enum tryret returncode(enum clnt_stat stat, struct rpc_err *rpcerr);

/*
 * Used to set mount flags with getmntopts.  Call with dir=TRUE to
 * initialize altflags from the current mount flags.  Call with
 * dir=FALSE to update mount flags with the new value of altflags after
 * the call to getmntopts.
 */
static void
set_flags(int* altflags, int* nfsflags, int dir)
{
#define F2(af, nf)					\
	if (dir) {					\
		if (*nfsflags & NFSMNT_##nf)		\
			*altflags |= ALTF_##af;		\
		else					\
			*altflags &= ~ALTF_##af;	\
	} else {					\
		if (*altflags & ALTF_##af)		\
			*nfsflags |= NFSMNT_##nf;	\
		else					\
			*nfsflags &= ~NFSMNT_##nf;	\
	}
#define F(f)	F2(f,f)

	F(NOCONN);
	F(DUMBTIMR);
	F2(INTR, INT);
	F(RDIRPLUS);
	F(RESVPORT);
	F(SOFT);
	F(ACREGMIN);
	F(ACREGMAX);
	F(ACDIRMIN);
	F(ACDIRMAX);
	F(NOLOCKD);

#undef F
#undef F2
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;
	struct nfs_args *nfsargsp;
	struct nfs_args nfsargs;
	int mntflags, altflags, num;
	char *name, *p, *spec;
	char mntpath[MAXPATHLEN];

	mntflags = 0;
	altflags = 0;
	nfsargs = nfsdefargs;
	nfsargsp = &nfsargs;
	while ((c = getopt(argc, argv,
	    "23a:bcdD:g:I:iLl:No:PR:r:sTt:w:x:U")) != -1)
		switch (c) {
		case '2':
			mountmode = V2;
			break;
		case '3':
			mountmode = V3;
			break;
		case 'a':
			num = strtol(optarg, &p, 10);
			if (*p || num < 0)
				errx(1, "illegal -a value -- %s", optarg);
			nfsargsp->readahead = num;
			nfsargsp->flags |= NFSMNT_READAHEAD;
			break;
		case 'b':
			opflags |= BGRND;
			break;
		case 'c':
			nfsargsp->flags |= NFSMNT_NOCONN;
			break;
		case 'D':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -D value -- %s", optarg);
			nfsargsp->deadthresh = num;
			nfsargsp->flags |= NFSMNT_DEADTHRESH;
			break;
		case 'd':
			nfsargsp->flags |= NFSMNT_DUMBTIMR;
			break;
#if 0 /* XXXX */
		case 'g':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -g value -- %s", optarg);
			set_rpc_maxgrouplist(num);
			nfsargsp->maxgrouplist = num;
			nfsargsp->flags |= NFSMNT_MAXGRPS;
			break;
#endif
		case 'I':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -I value -- %s", optarg);
			nfsargsp->readdirsize = num;
			nfsargsp->flags |= NFSMNT_READDIRSIZE;
			break;
		case 'i':
			nfsargsp->flags |= NFSMNT_INT;
			break;
		case 'L':
			nfsargsp->flags |= NFSMNT_NOLOCKD;
			break;
		case 'l':
			nfsargsp->flags |= NFSMNT_RDIRPLUS;
			break;
		case 'N':
			nfsargsp->flags &= ~NFSMNT_RESVPORT;
			break;
		case 'o':
			altflags = 0;
			set_flags(&altflags, &nfsargsp->flags, TRUE);
			if (mountmode == V2)
				altflags |= ALTF_NFSV2;
			else if (mountmode == V3)
				altflags |= ALTF_NFSV3;
			getmntopts(optarg, mopts, &mntflags, &altflags);
			set_flags(&altflags, &nfsargsp->flags, FALSE);
			/*
			 * Handle altflags which don't map directly to
			 * mount flags.
			 */
			if (altflags & ALTF_BG)
				opflags |= BGRND;
			if (altflags & ALTF_NOINET4)
				opflags |= OF_NOINET4;
			if (altflags & ALTF_NOINET6)
				opflags |= OF_NOINET6;
			if (altflags & ALTF_MNTUDP)
				mnttcp_ok = 0;
			if (altflags & ALTF_TCP) {
				nfsargsp->sotype = SOCK_STREAM;
				nfsproto = IPPROTO_TCP;
			}
			if (altflags & ALTF_PORT) {
				/*
				 * XXX Converting from a string to an int
				 * and back again is silly, and we should
				 * allow /etc/services names.
				 */
				p = strstr(optarg, "port=");
				if (p) {
					asprintf(&portspec, "%d",
					    atoi(p + 5));
					if (portspec == NULL)
						err(1, "asprintf");
				}
			}
			mountmode = ANY;
			if (altflags & ALTF_NFSV2)
				mountmode = V2;
			if (altflags & ALTF_NFSV3)
				mountmode = V3;
			if (altflags & ALTF_ACREGMIN) {
				p = strstr(optarg, "acregmin=");
				if (p)
					nfsargsp->acregmin = atoi(p + 9);
			}
			if (altflags & ALTF_ACREGMAX) {
				p = strstr(optarg, "acregmax=");
				if (p)
					nfsargsp->acregmax = atoi(p + 9);
			}
			if (altflags & ALTF_ACDIRMIN) {
				p = strstr(optarg, "acdirmin=");
				if (p)
					nfsargsp->acdirmin = atoi(p + 9);
			}
			if (altflags & ALTF_ACDIRMAX) {
				p = strstr(optarg, "acdirmax=");
				if (p)
					nfsargsp->acdirmax = atoi(p + 9);
			}
			break;
		case 'P':
			/* obsolete for NFSMNT_RESVPORT, now default */
			break;
		case 'R':
			num = strtol(optarg, &p, 10);
			if (*p || num < 0)
				errx(1, "illegal -R value -- %s", optarg);
			retrycnt = num;
			break;
		case 'r':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -r value -- %s", optarg);
			nfsargsp->rsize = num;
			nfsargsp->flags |= NFSMNT_RSIZE;
			break;
		case 's':
			nfsargsp->flags |= NFSMNT_SOFT;
			break;
		case 'T':
			nfsargsp->sotype = SOCK_STREAM;
			nfsproto = IPPROTO_TCP;
			break;
		case 't':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -t value -- %s", optarg);
			nfsargsp->timeo = num;
			nfsargsp->flags |= NFSMNT_TIMEO;
			break;
		case 'w':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -w value -- %s", optarg);
			nfsargsp->wsize = num;
			nfsargsp->flags |= NFSMNT_WSIZE;
			break;
		case 'x':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -x value -- %s", optarg);
			nfsargsp->retrans = num;
			nfsargsp->flags |= NFSMNT_RETRANS;
			break;
		case 'U':
			mnttcp_ok = 0;
			break;
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (argc != 2) {
		usage();
		/* NOTREACHED */
	}

	spec = *argv++;
	name = *argv;

	if (retrycnt == -1)
		/* The default is to keep retrying forever. */
		retrycnt = 0;
	if (!getnfsargs(spec, nfsargsp))
		exit(1);

	/* resolve the mountpoint with realpath(3) */
	(void)checkpath(name, mntpath);

	if (mount("nfs", mntpath, mntflags, nfsargsp))
		err(1, "%s", mntpath);

	exit(0);
}

int
getnfsargs(spec, nfsargsp)
	char *spec;
	struct nfs_args *nfsargsp;
{
	struct addrinfo hints, *ai_nfs, *ai;
	enum tryret ret;
	int ecode, speclen, remoteerr;
	char *hostp, *delimp, *errstr;
	size_t len;
	static char nam[MNAMELEN + 1];

	if ((delimp = strrchr(spec, ':')) != NULL) {
		hostp = spec;
		spec = delimp + 1;
	} else if ((delimp = strrchr(spec, '@')) != NULL) {
		warnx("path@server syntax is deprecated, use server:path");
		hostp = delimp + 1;
	} else {
		warnx("no <host>:<dirpath> nfs-name");
		return (0);
	}
	*delimp = '\0';

	/*
	 * If there has been a trailing slash at mounttime it seems
	 * that some mountd implementations fail to remove the mount
	 * entries from their mountlist while unmounting.
	 */
	for (speclen = strlen(spec); 
		speclen > 1 && spec[speclen - 1] == '/';
		speclen--)
		spec[speclen - 1] = '\0';
	if (strlen(hostp) + strlen(spec) + 1 > MNAMELEN) {
		warnx("%s:%s: %s", hostp, spec, strerror(ENAMETOOLONG));
		return (0);
	}
	/* Make both '@' and ':' notations equal */
	if (*hostp != '\0') {
		len = strlen(hostp);
		memmove(nam, hostp, len);
		nam[len] = ':';
		memmove(nam + len + 1, spec, speclen);
		nam[len + speclen + 1] = '\0';
	}

	/*
	 * Handle an internet host address.
	 */
	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_socktype = nfsargsp->sotype;
	if (getaddrinfo(hostp, portspec, &hints, &ai_nfs) != 0) {
		hints.ai_flags = 0;
		if ((ecode = getaddrinfo(hostp, portspec, &hints, &ai_nfs))
		    != 0) {
			if (portspec == NULL)
				errx(1, "%s: %s", hostp, gai_strerror(ecode));
			else
				errx(1, "%s:%s: %s", hostp, portspec,
				    gai_strerror(ecode));
			return (0);
		}
	}

	ret = TRYRET_LOCALERR;
	for (;;) {
		/*
		 * Try each entry returned by getaddrinfo(). Note the
		 * occurence of remote errors by setting `remoteerr'.
		 */
		remoteerr = 0;
		for (ai = ai_nfs; ai != NULL; ai = ai->ai_next) {
			if ((ai->ai_family == AF_INET6) &&
			    (opflags & OF_NOINET6))
				continue;
			if ((ai->ai_family == AF_INET) && 
			    (opflags & OF_NOINET4))
				continue;
			ret = nfs_tryproto(nfsargsp, ai, hostp, spec, &errstr);
			if (ret == TRYRET_SUCCESS)
				break;
			if (ret != TRYRET_LOCALERR)
				remoteerr = 1;
			if ((opflags & ISBGRND) == 0)
				fprintf(stderr, "%s\n", errstr);
		}
		if (ret == TRYRET_SUCCESS)
			break;

		/* Exit if all errors were local. */
		if (!remoteerr)
			exit(1);

		/*
		 * If retrycnt == 0, we are to keep retrying forever.
		 * Otherwise decrement it, and exit if it hits zero.
		 */
		if (retrycnt != 0 && --retrycnt == 0)
			exit(1);

		if ((opflags & (BGRND | ISBGRND)) == BGRND) {
			warnx("Cannot immediately mount %s:%s, backgrounding",
			    hostp, spec);
			opflags |= ISBGRND;
			if (daemon(0, 0) != 0)
				err(1, "daemon");
		}
		sleep(60);
	}
	freeaddrinfo(ai_nfs);
	nfsargsp->hostname = nam;
	/* Add mounted file system to PATH_MOUNTTAB */
	if (!add_mtab(hostp, spec))
		warnx("can't update %s for %s:%s", PATH_MOUNTTAB, hostp, spec);
	return (1);
}

/*
 * Try to set up the NFS arguments according to the address
 * family, protocol (and possibly port) specified in `ai'.
 *
 * Returns TRYRET_SUCCESS if successful, or:
 *   TRYRET_TIMEOUT		The server did not respond.
 *   TRYRET_REMOTEERR		The server reported an error.
 *   TRYRET_LOCALERR		Local failure.
 *
 * In all error cases, *errstr will be set to a statically-allocated string
 * describing the error.
 */
enum tryret
nfs_tryproto(struct nfs_args *nfsargsp, struct addrinfo *ai, char *hostp,
    char *spec, char **errstr)
{
	static char errbuf[256];
	struct sockaddr_storage nfs_ss;
	struct netbuf nfs_nb;
	struct nfhret nfhret;
	struct timeval try;
	struct rpc_err rpcerr;
	CLIENT *clp;
	struct netconfig *nconf, *nconf_mnt;
	char *netid, *netid_mnt;
	int doconnect, nfsvers, mntvers;
	enum clnt_stat stat;
	enum mountmode trymntmode;

	trymntmode = mountmode;
	errbuf[0] = '\0';
	*errstr = errbuf;

	if ((netid = netidbytype(ai->ai_family, nfsargsp->sotype)) == NULL) {
		snprintf(errbuf, sizeof errbuf,
		    "af %d sotype %d not supported", ai->ai_family,
		    nfsargsp->sotype);
		return (TRYRET_LOCALERR);
	}
	if ((nconf = getnetconf_cached(netid)) == NULL) {
		snprintf(errbuf, sizeof errbuf, "%s: %s", netid, nc_sperror());
		return (TRYRET_LOCALERR);
	}
	/* The RPCPROG_MNT netid may be different. */
	if (mnttcp_ok) {
		netid_mnt = netid;
		nconf_mnt = nconf;
	} else {
		if ((netid_mnt = netidbytype(ai->ai_family, SOCK_DGRAM))
		     == NULL) {
			snprintf(errbuf, sizeof errbuf,
			    "af %d sotype SOCK_DGRAM not supported",
			     ai->ai_family);
			return (TRYRET_LOCALERR);
		}
		if ((nconf_mnt = getnetconf_cached(netid_mnt)) == NULL) {
			snprintf(errbuf, sizeof errbuf, "%s: %s", netid_mnt,
			    nc_sperror());
			return (TRYRET_LOCALERR);
		}
	}

tryagain:
	if (trymntmode == V2) {
		nfsvers = 2;
		mntvers = 1;
	} else {
		nfsvers = 3;
		mntvers = 3;
	}

	if (portspec != NULL) {
		/* `ai' contains the complete nfsd sockaddr. */
		nfs_nb.buf = ai->ai_addr;
		nfs_nb.len = nfs_nb.maxlen = ai->ai_addrlen;
	} else {
		/* Ask the remote rpcbind. */
		nfs_nb.buf = &nfs_ss;
		nfs_nb.len = nfs_nb.maxlen = sizeof nfs_ss;

		if (!rpcb_getaddr(RPCPROG_NFS, nfsvers, nconf, &nfs_nb,
		    hostp)) {
			if (rpc_createerr.cf_stat == RPC_PROGVERSMISMATCH &&
			    trymntmode == ANY) {
				trymntmode = V2;
				goto tryagain;
			}
			snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s",
			    netid, hostp, spec,
			    clnt_spcreateerror("RPCPROG_NFS"));
			return (returncode(rpc_createerr.cf_stat,
			    &rpc_createerr.cf_error));
		}
	}

	/* Check that the server (nfsd) responds on the port we have chosen. */
	clp = clnt_tli_create(RPC_ANYFD, nconf, &nfs_nb, RPCPROG_NFS, nfsvers,
	    0, 0);
	if (clp == NULL) {
		snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s", netid,
		    hostp, spec, clnt_spcreateerror("nfsd: RPCPROG_NFS"));
		return (returncode(rpc_createerr.cf_stat,
		    &rpc_createerr.cf_error));
	}
	if (nfsargsp->sotype == SOCK_DGRAM &&
	    !(nfsargsp->flags & NFSMNT_NOCONN)) {
		/*
		 * Use connect(), to match what the kernel does. This
		 * catches cases where the server responds from the
		 * wrong source address.
		 */
		doconnect = 1;
		if (!clnt_control(clp, CLSET_CONNECT, (char *)&doconnect)) {
			clnt_destroy(clp);
			snprintf(errbuf, sizeof errbuf,
			    "[%s] %s:%s: CLSET_CONNECT failed", netid, hostp,
			    spec);
			return (TRYRET_LOCALERR);
		}
	}

	try.tv_sec = 10;
	try.tv_usec = 0;
	stat = clnt_call(clp, NFSPROC_NULL, (xdrproc_t)xdr_void, NULL,
			 (xdrproc_t)xdr_void, NULL,
	    try);
	if (stat != RPC_SUCCESS) {
		if (stat == RPC_PROGVERSMISMATCH && trymntmode == ANY) {
			clnt_destroy(clp);
			trymntmode = V2;
			goto tryagain;
		}
		clnt_geterr(clp, &rpcerr);
		snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s", netid,
		    hostp, spec, clnt_sperror(clp, "NFSPROC_NULL"));
		clnt_destroy(clp);
		return (returncode(stat, &rpcerr));
	}
	clnt_destroy(clp);

	/* Send the RPCMNT_MOUNT RPC to get the root filehandle. */
	try.tv_sec = 10;
	try.tv_usec = 0;
	clp = clnt_tp_create(hostp, RPCPROG_MNT, mntvers, nconf_mnt);
	if (clp == NULL) {
		snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s", netid_mnt,
		    hostp, spec, clnt_spcreateerror("RPCMNT: clnt_create"));
		return (returncode(rpc_createerr.cf_stat,
		    &rpc_createerr.cf_error));
	}
	clp->cl_auth = authsys_create_default();
	nfhret.auth = RPCAUTH_UNIX;
	nfhret.vers = mntvers;
	stat = clnt_call(clp, RPCMNT_MOUNT, (xdrproc_t)xdr_dir, spec, 
			 (xdrproc_t)xdr_fh, &nfhret,
	    try);
	auth_destroy(clp->cl_auth);
	if (stat != RPC_SUCCESS) {
		if (stat == RPC_PROGVERSMISMATCH && trymntmode == ANY) {
			clnt_destroy(clp);
			trymntmode = V2;
			goto tryagain;
		}
		clnt_geterr(clp, &rpcerr);
		snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s", netid_mnt,
		    hostp, spec, clnt_sperror(clp, "RPCPROG_MNT"));
		clnt_destroy(clp);
		return (returncode(stat, &rpcerr));
	}
	clnt_destroy(clp);

	if (nfhret.stat != 0) {
		snprintf(errbuf, sizeof errbuf, "[%s] %s:%s: %s", netid_mnt,
		    hostp, spec, strerror(nfhret.stat));
		return (TRYRET_REMOTEERR);
	}

	/*
	 * Store the filehandle and server address in nfsargsp, making
	 * sure to copy any locally allocated structures.
	 */
	nfsargsp->addrlen = nfs_nb.len;
	nfsargsp->addr = malloc(nfsargsp->addrlen);
	nfsargsp->fhsize = nfhret.fhsize;
	nfsargsp->fh = malloc(nfsargsp->fhsize);
	if (nfsargsp->addr == NULL || nfsargsp->fh == NULL)
		err(1, "malloc");
	bcopy(nfs_nb.buf, nfsargsp->addr, nfsargsp->addrlen);
	bcopy(nfhret.nfh, nfsargsp->fh, nfsargsp->fhsize);

	if (nfsvers == 3)
		nfsargsp->flags |= NFSMNT_NFSV3;
	else
		nfsargsp->flags &= ~NFSMNT_NFSV3;

	return (TRYRET_SUCCESS);
}


/*
 * Catagorise a RPC return status and error into an `enum tryret'
 * return code.
 */
enum tryret
returncode(enum clnt_stat stat, struct rpc_err *rpcerr)
{
	switch (stat) {
	case RPC_TIMEDOUT:
		return (TRYRET_TIMEOUT);
	case RPC_PMAPFAILURE:
	case RPC_PROGNOTREGISTERED:
	case RPC_PROGVERSMISMATCH:
	/* XXX, these can be local or remote. */
	case RPC_CANTSEND:
	case RPC_CANTRECV:
		return (TRYRET_REMOTEERR);
	case RPC_SYSTEMERROR:
		switch (rpcerr->re_errno) {
		case ETIMEDOUT:
			return (TRYRET_TIMEOUT);
		case ENOMEM:
			break;
		default:
			return (TRYRET_REMOTEERR);
		}
		/* FALLTHROUGH */
	default:
		break;
	}
	return (TRYRET_LOCALERR);
}

/*
 * Look up a netid based on an address family and socket type.
 * `af' is the address family, and `sotype' is SOCK_DGRAM or SOCK_STREAM.
 *
 * XXX there should be a library function for this.
 */
char *
netidbytype(int af, int sotype) {
	struct nc_protos *p;

	for (p = nc_protos; p->netid != NULL; p++) {
		if (af != p->af || sotype != p->sotype)
			continue;
		return (p->netid);
	}
	return (NULL);
}

/*
 * Look up a netconfig entry based on a netid, and cache the result so
 * that we don't need to remember to call freenetconfigent().
 *
 * Otherwise it behaves just like getnetconfigent(), so nc_*error()
 * work on failure.
 */
struct netconfig *
getnetconf_cached(const char *netid) {
	static struct nc_entry {
		struct netconfig *nconf;
		struct nc_entry *next;
	} *head;
	struct nc_entry *p;
	struct netconfig *nconf;

	for (p = head; p != NULL; p = p->next)
		if (strcmp(netid, p->nconf->nc_netid) == 0)
			return (p->nconf);

	if ((nconf = getnetconfigent(netid)) == NULL)
		return (NULL);
	if ((p = malloc(sizeof(*p))) == NULL)
		err(1, "malloc");
	p->nconf = nconf;
	p->next = head;
	head = p;

	return (p->nconf);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

int
xdr_fh(xdrsp, np)
	XDR *xdrsp;
	struct nfhret *np;
{
	int i;
	long auth, authcnt, authfnd = 0;

	if (!xdr_u_long(xdrsp, &np->stat))
		return (0);
	if (np->stat)
		return (1);
	switch (np->vers) {
	case 1:
		np->fhsize = NFSX_V2FH;
		return (xdr_opaque(xdrsp, (caddr_t)np->nfh, NFSX_V2FH));
	case 3:
		if (!xdr_long(xdrsp, &np->fhsize))
			return (0);
		if (np->fhsize <= 0 || np->fhsize > NFSX_V3FHMAX)
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)np->nfh, np->fhsize))
			return (0);
		if (!xdr_long(xdrsp, &authcnt))
			return (0);
		for (i = 0; i < authcnt; i++) {
			if (!xdr_long(xdrsp, &auth))
				return (0);
			if (auth == np->auth)
				authfnd++;
		}
		/*
		 * Some servers, such as DEC's OSF/1 return a nil authenticator
		 * list to indicate RPCAUTH_UNIX.
		 */
		if (!authfnd && (authcnt > 0 || np->auth != RPCAUTH_UNIX))
			np->stat = EAUTH;
		return (1);
	};
	return (0);
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n%s\n%s\n",
"usage: mount_nfs [-23KNPTUbcdilqs] [-D deadthresh] [-I readdirsize]",
"                 [-R retrycnt] [-a maxreadahead]",
"                 [-g maxgroups] [-m realm] [-o options] [-r readsize]",
"                 [-t timeout] [-w writesize] [-x retrans] rhost:path node");
	exit(1);
}
