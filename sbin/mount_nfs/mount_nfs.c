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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mount_nfs.c	8.3 (Berkeley) 3/27/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/syslog.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifdef ISO
#include <netiso/iso.h>
#endif

#ifdef KERBEROS
#include <des.h>
#include <kerberosIV/krb.h>
#endif

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#define KERNEL
#include <nfs/nfs.h>
#undef KERNEL
#include <nfs/nqnfs.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include "mntopts.h"

#define	ALTF_BG		0x1
#define ALTF_NOCONN	0x2
#define ALTF_DUMBTIMR	0x4
#define ALTF_INTR	0x8
#define ALTF_KERB	0x10
#define ALTF_NQLOOKLSE	0x20
#define ALTF_RDIRALOOK	0x40
#define	ALTF_MYWRITE	0x80
#define ALTF_RESVPORT	0x100
#define ALTF_SEQPACKET	0x200
#define ALTF_NQNFS	0x400
#define ALTF_SOFT	0x800
#define ALTF_TCP	0x1000

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_FORCE,
	MOPT_UPDATE,
	{ "bg", 0, ALTF_BG, 1 },
	{ "conn", 1, ALTF_NOCONN, 1 },
	{ "dumbtimer", 0, ALTF_DUMBTIMR, 1 },
	{ "intr", 0, ALTF_INTR, 1 },
#ifdef KERBEROS
	{ "kerb", 0, ALTF_KERB, 1 },
#endif
	{ "nqlooklease", 0, ALTF_NQLOOKLSE, 1 },
	{ "rdiralook", 0, ALTF_RDIRALOOK, 1 },
	{ "mywrite", 0, ALTF_MYWRITE, 1 },
	{ "resvport", 0, ALTF_RESVPORT, 1 },
#ifdef ISO
	{ "seqpacket", 0, ALTF_SEQPACKET, 1 },
#endif
	{ "nqnfs", 0, ALTF_NQNFS, 1 },
	{ "soft", 0, ALTF_SOFT, 1 },
	{ "tcp", 0, ALTF_TCP, 1 },
	{ NULL }
};

struct nfs_args nfsdefargs = {
	(struct sockaddr *)0,
	sizeof (struct sockaddr_in),
	SOCK_DGRAM,
	0,
	(nfsv2fh_t *)0,
	0,
	NFS_WSIZE,
	NFS_RSIZE,
	NFS_TIMEO,
	NFS_RETRANS,
	NFS_MAXGRPS,
	NFS_DEFRAHEAD,
	NQ_DEFLEASE,
	NQ_DEADTHRESH,
	(char *)0,
};

struct nfhret {
	u_long	stat;
	nfsv2fh_t nfh;
};
#define	DEF_RETRY	10000
#define	BGRND	1
#define	ISBGRND	2
int retrycnt = DEF_RETRY;
int opflags = 0;

#ifdef KERBEROS
char inst[INST_SZ];
char realm[REALM_SZ];
KTEXT_ST kt;
#endif

int	getnfsargs __P((char *, struct nfs_args *));
#ifdef ISO
struct	iso_addr *iso_addr __P((const char *));
#endif
void	set_rpc_maxgrouplist __P((int));
__dead	void usage __P((void));
int	xdr_dir __P((XDR *, char *));
int	xdr_fh __P((XDR *, struct nfhret *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int c;
	register struct nfs_args *nfsargsp;
	struct nfs_args nfsargs;
	struct nfsd_cargs ncd;
	int mntflags, altflags, i, nfssvc_flag, num;
	char *name, *p, *spec;
	struct vfsconf *vfc;
#ifdef KERBEROS
	uid_t last_ruid;
#endif

#ifdef KERBEROS
	last_ruid = -1;
	(void)strcpy(realm, KRB_REALM);
#endif
	retrycnt = DEF_RETRY;

	mntflags = 0;
	altflags = 0;
	nfsargs = nfsdefargs;
	nfsargsp = &nfsargs;
	while ((c = getopt(argc, argv,
	    "a:bcdD:g:iKklL:Mm:o:PpqR:r:sTt:w:x:")) != EOF)
		switch (c) {
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
		case 'g':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
				errx(1, "illegal -g value -- %s", optarg);
			set_rpc_maxgrouplist(num);
			nfsargsp->maxgrouplist = num;
			nfsargsp->flags |= NFSMNT_MAXGRPS;
			break;
		case 'i':
			nfsargsp->flags |= NFSMNT_INT;
			break;
#ifdef KERBEROS
		case 'K':
			nfsargsp->flags |= NFSMNT_KERB;
			break;
#endif
		case 'k':
			nfsargsp->flags |= NFSMNT_NQLOOKLEASE;
			break;
		case 'L':
			num = strtol(optarg, &p, 10);
			if (*p || num < 2)
				errx(1, "illegal -L value -- %s", optarg);
			nfsargsp->leaseterm = num;
			nfsargsp->flags |= NFSMNT_LEASETERM;
			break;
		case 'l':
			nfsargsp->flags |= NFSMNT_RDIRALOOK;
			break;
		case 'M':
			nfsargsp->flags |= NFSMNT_MYWRITE;
			break;
#ifdef KERBEROS
		case 'm':
			(void)strncpy(realm, optarg, REALM_SZ - 1);
			realm[REALM_SZ - 1] = '\0';
			break;
#endif
		case 'o':
			getmntopts(optarg, mopts, &mntflags, &altflags);
			if(altflags & ALTF_BG)
				opflags |= BGRND;
			if(altflags & ALTF_NOCONN)
				nfsargsp->flags |= NFSMNT_NOCONN;
			if(altflags & ALTF_DUMBTIMR)
				nfsargsp->flags |= NFSMNT_DUMBTIMR;
			if(altflags & ALTF_INTR)
				nfsargsp->flags |= NFSMNT_INT;
#ifdef KERBEROS
			if(altflags & ALTF_KERB)
				nfsargsp->flags |= NFSMNT_KERB;
#endif
			if(altflags & ALTF_NQLOOKLSE)
				nfsargsp->flags |= NFSMNT_NQLOOKLEASE;
			if(altflags & ALTF_RDIRALOOK)
				nfsargsp->flags |= NFSMNT_RDIRALOOK;
			if(altflags & ALTF_MYWRITE)
				nfsargsp->flags |= NFSMNT_MYWRITE;
			if(altflags & ALTF_RESVPORT)
				nfsargsp->flags |= NFSMNT_RESVPORT;
#ifdef ISO
			if(altflags & ALTF_SEQPACKET)
				nfsargsp->sotype = SOCK_SEQPACKET;
#endif
			if(altflags & ALTF_NQNFS)
				nfsargsp->flags |= NFSMNT_NQNFS;
			if(altflags & ALTF_SOFT)
				nfsargsp->flags |= NFSMNT_SOFT;
			if(altflags & ALTF_TCP)
				nfsargsp->sotype = SOCK_STREAM;
			altflags = 0;
			break;
		case 'P':
			nfsargsp->flags |= NFSMNT_RESVPORT;
			break;
#ifdef ISO
		case 'p':
			nfsargsp->sotype = SOCK_SEQPACKET;
			break;
#endif
		case 'q':
			nfsargsp->flags |= NFSMNT_NQNFS;
			break;
		case 'R':
			num = strtol(optarg, &p, 10);
			if (*p || num <= 0)
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
		default:
			usage();
			break;
		}
	argc -= optind;
	argv += optind;

	if (argc != 2)
		usage();

	spec = *argv++;
	name = *argv;

	if (!getnfsargs(spec, nfsargsp))
		exit(1);

	vfc = getvfsbyname("nfs");
	if(!vfc && vfsisloadable("nfs")) {
		if(vfsload("nfs"))
			err(1, "vfsload(nfs)");
		endvfsent();	/* flush cache */
		vfc = getvfsbyname("nfs");
	}

	if (mount(vfc ? vfc->vfc_index : MOUNT_NFS, name, mntflags, nfsargsp))
		err(1, "%s", name);
	if (nfsargsp->flags & (NFSMNT_NQNFS | NFSMNT_KERB)) {
		if ((opflags & ISBGRND) == 0) {
			if (i = fork()) {
				if (i == -1)
					err(1, "nqnfs 1");
				exit(0);
			}
			(void) setsid();
			(void) close(STDIN_FILENO);
			(void) close(STDOUT_FILENO);
			(void) close(STDERR_FILENO);
			(void) chdir("/");
		}
		openlog("mount_nfs:", LOG_PID, LOG_DAEMON);
		nfssvc_flag = NFSSVC_MNTD;
		ncd.ncd_dirp = name;
		while (nfssvc(nfssvc_flag, (caddr_t)&ncd) < 0) {
			if (errno != ENEEDAUTH) {
				syslog(LOG_ERR, "nfssvc err %m");
				continue;
			}
			nfssvc_flag =
			    NFSSVC_MNTD | NFSSVC_GOTAUTH | NFSSVC_AUTHINFAIL;
#ifdef KERBEROS
			/*
			 * Set up as ncd_authuid for the kerberos call.
			 * Must set ruid to ncd_authuid and reset the
			 * ticket name iff ncd_authuid is not the same
			 * as last time, so that the right ticket file
			 * is found.
			 */
			if (ncd.ncd_authuid != last_ruid) {
				char buf[512];
				(void)sprintf(buf, "%s%d",
					      TKT_ROOT, ncd.ncd_authuid);
				krb_set_tkt_string(buf);
				last_ruid = ncd.ncd_authuid;
			}
			if (krb_mk_req(&kt, "rcmd", inst, realm, 0) ==
			    KSUCCESS &&
			    kt.length <= (RPCAUTH_MAXSIZ - 2 * NFSX_UNSIGNED)) {
				ncd.ncd_authtype = RPCAUTH_NQNFS;
				ncd.ncd_authlen = kt.length;
				ncd.ncd_authstr = (char *)kt.dat;
				nfssvc_flag = NFSSVC_MNTD | NFSSVC_GOTAUTH;
			}
#endif /* KERBEROS */
		}
	}
	exit(0);
}

int
getnfsargs(spec, nfsargsp)
	char *spec;
	struct nfs_args *nfsargsp;
{
	register CLIENT *clp;
	struct hostent *hp;
	static struct sockaddr_in saddr;
#ifdef ISO
	static struct sockaddr_iso isoaddr;
	struct iso_addr *isop;
	int isoflag = 0;
#endif
	struct timeval pertry, try;
	enum clnt_stat clnt_stat;
	int so = RPC_ANYSOCK, i;
	char *hostp, *delimp;
#ifdef KERBEROS
	char *cp;
#endif
	u_short tport;
	static struct nfhret nfhret;
	static char nam[MNAMELEN + 1];

	strncpy(nam, spec, MNAMELEN);
	nam[MNAMELEN] = '\0';
	if ((delimp = strchr(spec, '@')) != NULL) {
		hostp = delimp + 1;
	} else if ((delimp = strchr(spec, ':')) != NULL) {
		hostp = spec;
		spec = delimp + 1;
	} else {
		warnx("no <host>:<dirpath> or <dirpath>@<host> spec");
		return (0);
	}
	*delimp = '\0';
	/*
	 * DUMB!! Until the mount protocol works on iso transport, we must
	 * supply both an iso and an inet address for the host.
	 */
#ifdef ISO
	if (!strncmp(hostp, "iso=", 4)) {
		u_short isoport;

		hostp += 4;
		isoflag++;
		if ((delimp = strchr(hostp, '+')) == NULL) {
			warnx("no iso+inet address");
			return (0);
		}
		*delimp = '\0';
		if ((isop = iso_addr(hostp)) == NULL) {
			warnx("bad ISO address");
			return (0);
		}
		bzero((caddr_t)&isoaddr, sizeof (isoaddr));
		bcopy((caddr_t)isop, (caddr_t)&isoaddr.siso_addr,
			sizeof (struct iso_addr));
		isoaddr.siso_len = sizeof (isoaddr);
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		isoport = htons(NFS_PORT);
		bcopy((caddr_t)&isoport, TSEL(&isoaddr), isoaddr.siso_tlen);
		hostp = delimp + 1;
	}
#endif /* ISO */

	/*
	 * Handle an internet host address and reverse resolve it if
	 * doing Kerberos.
	 */
	if (isdigit(*hostp)) {
		if ((saddr.sin_addr.s_addr = inet_addr(hostp)) == -1) {
			warnx("bad net address %s", hostp);
			return (0);
		}
	} else if ((hp = gethostbyname(hostp)) != NULL) {
		bcopy(hp->h_addr, (caddr_t)&saddr.sin_addr, hp->h_length);
	} else {
		warnx("can't get net id for host");
		return (0);
        }
#ifdef KERBEROS
	if ((nfsargsp->flags & NFSMNT_KERB)) {
		if ((hp = gethostbyaddr((char *)&saddr.sin_addr.s_addr,
		    sizeof (u_long), AF_INET)) == (struct hostent *)0) {
			warnx("can't reverse resolve net address");
			return (0);
		}
		bcopy(hp->h_addr, (caddr_t)&saddr.sin_addr, hp->h_length);
		strncpy(inst, hp->h_name, INST_SZ);
		inst[INST_SZ - 1] = '\0';
		if (cp = strchr(inst, '.'))
			*cp = '\0';
	}
#endif /* KERBEROS */

	nfhret.stat = EACCES;	/* Mark not yet successful */
	while (retrycnt > 0) {
		saddr.sin_family = AF_INET;
		saddr.sin_port = htons(PMAPPORT);
		if ((tport = pmap_getport(&saddr, RPCPROG_NFS,
		    NFS_VER2, nfsargsp->sotype == SOCK_STREAM ? IPPROTO_TCP :
		    IPPROTO_UDP)) == 0) {
			if ((opflags & ISBGRND) == 0)
				clnt_pcreateerror("NFS Portmap");
		} else {
			saddr.sin_port = 0;
			pertry.tv_sec = 10;
			pertry.tv_usec = 0;
			if ((clp = (nfsargsp->sotype == SOCK_STREAM ?
			    clnttcp_create(&saddr, RPCPROG_MNT, RPCMNT_VER1,
					   &so, 0, 0) :
			    clntudp_create(&saddr, RPCPROG_MNT, RPCMNT_VER1,
					   pertry, &so))) == NULL) {
				if ((opflags & ISBGRND) == 0)
					clnt_pcreateerror("Cannot MNT RPC");
			} else {
				clp->cl_auth = authunix_create_default();
				try.tv_sec = 10;
				try.tv_usec = 0;
				clnt_stat = clnt_call(clp, RPCMNT_MOUNT,
				    xdr_dir, spec, xdr_fh, &nfhret, try);
				if (clnt_stat != RPC_SUCCESS) {
					if ((opflags & ISBGRND) == 0)
						warnx("%s", clnt_sperror(clp,
						    "bad MNT RPC"));
				} else {
					auth_destroy(clp->cl_auth);
					clnt_destroy(clp);
					retrycnt = 0;
				}
			}
		}
		if (--retrycnt > 0) {
			if (opflags & BGRND) {
				opflags &= ~BGRND;
				if (i = fork()) {
					if (i == -1)
						err(1, "nqnfs 2");
					exit(0);
				}
				(void) setsid();
				(void) close(STDIN_FILENO);
				(void) close(STDOUT_FILENO);
				(void) close(STDERR_FILENO);
				(void) chdir("/");
				opflags |= ISBGRND;
			}
			sleep(60);
		}
	}
	if (nfhret.stat) {
		if (opflags & ISBGRND)
			exit(1);
		errno = nfhret.stat;
		warn("can't access %s", spec);
		return (0);
	}
	saddr.sin_port = htons(tport);
#ifdef ISO
	if (isoflag) {
		nfsargsp->addr = (struct sockaddr *) &isoaddr;
		nfsargsp->addrlen = sizeof (isoaddr);
	} else
#endif /* ISO */
	{
		nfsargsp->addr = (struct sockaddr *) &saddr;
		nfsargsp->addrlen = sizeof (saddr);
	}
	nfsargsp->fh = &nfhret.nfh;
	nfsargsp->hostname = nam;
	return (1);
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
	if (!xdr_u_long(xdrsp, &(np->stat)))
		return (0);
	if (np->stat)
		return (1);
	return (xdr_opaque(xdrsp, (caddr_t)&(np->nfh), NFSX_FH));
}

__dead void
usage()
{
	(void)fprintf(stderr, "usage: mount_nfs %s\n%s\n%s\n%s\n",
"[-bcdiKklMPqsT] [-a maxreadahead] [-D deadthresh]",
"\t[-g maxgroups] [-L leaseterm] [-m realm] [-o options] [-R retrycnt]",
"\t[-r readsize] [-t timeout] [-w writesize] [-x retrans]",
"\trhost:path node");
	exit(1);
}
