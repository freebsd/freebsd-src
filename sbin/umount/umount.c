/*-
 * Copyright (c) 1980, 1989 The Regents of the University of California.
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
"@(#) Copyright (c) 1980, 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)umount.c	5.16 (Berkeley) 6/3/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>

#ifdef NFS
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>
#endif

#include <stdio.h>
#include <string.h>

#ifdef NFS
int xdr_dir();
char *nfshost;
#endif

int	vflag, all, errs, fake;
int	fflag = MNT_NOFORCE;
char	*getmntname();

#define	MNTON	1
#define	MNTFROM	2
#define	MNTTYPE 3

int *typelist, *maketypelist();

main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	int ch;

	sync();
	while ((ch = getopt(argc, argv, "afFh:t:v")) != EOF)
		switch((char)ch) {
		case 'v':
			vflag++;
			break;
		case 'f':
			fflag = MNT_FORCE;
			break;
		case 'F':
			fake++;
			break;
		case 'a':
			all++;
			break;
		case 't':
			typelist = maketypelist(optarg);
			break;
#ifdef	NFS
		case 'h':
			/* -h flag implies -a, and "-t nfs" if no -t flag */
			nfshost = optarg;
			all++;
			if (typelist == NULL)
				typelist = maketypelist("nfs");
			break;
#endif /* NFS */
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc == 0 && !all)
		usage();
	if (all) {
		if (argc > 0)
			usage();
		errs += umountall(typelist);
		exit(errs);
	}
	while (argc > 0) {
		if (umountfs(*argv++, 0) == 0)
			errs++;
		argc--;
	}
	exit(errs);
}

usage()
{
	fprintf(stderr,
		"%s\n%s\n",
		"Usage: umount [-fv] special | node",
#ifndef	NFS
		"    or umount -a[fv] [-t fstypelist]"
#else
		"    or umount -a[fv] [-h host] [-t fstypelist]"
#endif
	      );
	exit(1);
}

umountall(typelist)
	char **typelist;
{
	int		i, mntsize, errors;
	struct statfs	*mntbuf;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		fprintf(stderr, "umount: cannot get mount information.\n");
		exit(1);
	}
	for (errors = 0, i = mntsize - 1; i >= 0; i--) {
		if (badtype(mntbuf[i].f_type, typelist) ||
		    (strcmp(mntbuf[i].f_mntonname, "/") == 0))
			continue;
		if (!fake && unmount(mntbuf[i].f_mntonname, fflag) < 0) {
			perror(mntbuf[i].f_mntonname);
			errors ++;
		}
		if (vflag)
			fprintf(stderr, "%s: Unmounted from %s\n",
				mntbuf[i].f_mntfromname, mntbuf[i].f_mntonname);
	}
	return errors;
}

umountfs(name, typelist)
	char *name;
	int *typelist;
{
	char *mntpt;
	struct stat stbuf;
	int type;
#ifdef NFS
	register CLIENT *clp;
	struct hostent *hp = 0;
	struct sockaddr_in saddr;
	struct timeval pertry, try;
	enum clnt_stat clnt_stat;
	int so = RPC_ANYSOCK;
	char *hostp, *delimp;
#endif /* NFS */

	if (stat(name, &stbuf) < 0) {
		if (getmntname(name, MNTFROM, &type) != 0)
			mntpt = name;
		else if ((mntpt = getmntname(name, MNTON, &type)) == 0) {
			fprintf(stderr, "%s: not currently mounted\n", name);
			return (0);
		}
	} else if ((stbuf.st_mode & S_IFMT) == S_IFBLK) {
		if ((mntpt = getmntname(name, MNTON, &type)) == 0) {
			fprintf(stderr, "%s: not currently mounted\n", name);
			return (0);
		}
	} else if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		mntpt = name;
		if (getmntname(mntpt, MNTFROM, &type) == 0) {
			fprintf(stderr, "%s: not currently mounted\n", name);
			return (0);
		}
	} else {
		fprintf(stderr, "%s: not a directory or special device\n",
			name);
		return (0);
	}

	if (badtype(type, typelist))
		return(1);
#ifdef NFS
	if ((delimp = index(name, '@')) != NULL) {
		hostp = delimp + 1;
		*delimp = '\0';
		hp = gethostbyname(hostp);
		*delimp = '@';
	} else if ((delimp = index(name, ':')) != NULL) {
		*delimp = '\0';
		hostp = name;
		hp = gethostbyname(hostp);
		name = delimp+1;
		*delimp = ':';
	}

	if (!namematch(hp, nfshost))
		return(1);
#endif	/* NFS */
	if (!fake && unmount(mntpt, fflag) < 0) {
		perror(mntpt);
		return (0);
	}
	if (vflag)
		fprintf(stderr, "%s: Unmounted from %s\n", name, mntpt);

#ifdef	NFS
	if (!fake && hp != NULL && (fflag & MNT_FORCE) == 0) {
		*delimp = '\0';
		bcopy(hp->h_addr,(caddr_t)&saddr.sin_addr,hp->h_length);
		saddr.sin_family = AF_INET;
		saddr.sin_port = 0;
		pertry.tv_sec = 3;
		pertry.tv_usec = 0;
		if ((clp = clntudp_create(&saddr, RPCPROG_MNT, RPCMNT_VER1,
		    pertry, &so)) == NULL) {
			clnt_pcreateerror("Cannot MNT PRC");
			return (1);
		}
		clp->cl_auth = authunix_create_default();
		try.tv_sec = 20;
		try.tv_usec = 0;
		clnt_stat = clnt_call(clp, RPCMNT_UMOUNT, xdr_dir, name,
			xdr_void, (caddr_t)0, try);
		if (clnt_stat != RPC_SUCCESS) {
			clnt_perror(clp, "Bad MNT RPC");
			return (1);
		}
		auth_destroy(clp->cl_auth);
		clnt_destroy(clp);
	}
#endif /* NFS */
	return (1);
}

char *
getmntname(name, what, type)
	char *name;
	int what;
	int *type;
{
	int mntsize, i;
	struct statfs *mntbuf;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		perror("umount");
		return (0);
	}
	for (i = 0; i < mntsize; i++) {
		if (what == MNTON && !strcmp(mntbuf[i].f_mntfromname, name)) {
			if (type)
				*type = mntbuf[i].f_type;
			return (mntbuf[i].f_mntonname);
		}
		if (what == MNTFROM && !strcmp(mntbuf[i].f_mntonname, name)) {
			if (type)
				*type = mntbuf[i].f_type;
			return (mntbuf[i].f_mntfromname);
		}
	}
	return (0);
}

static int skipvfs;

badtype(type, typelist)
	int type;
	int *typelist;
{
	if (typelist == 0)
		return(0);
	while (*typelist) {
		if (type == *typelist)
			return(skipvfs);
		typelist++;
	}
	return(!skipvfs);
}

int *
maketypelist(fslist)
	char *fslist;
{
	register char *nextcp;
	register int *av, i;

	if (fslist == NULL)
		return(NULL);
	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		skipvfs = 1;
	} else
		skipvfs = 0;
	for (i = 0, nextcp = fslist; *nextcp; nextcp++)
		if (*nextcp == ',')
			i++;
	av = (int *)malloc((i+2) * sizeof(int));
	if (av == NULL)
		return(NULL);
	for (i = 0; fslist; fslist = nextcp) {
		if (nextcp = index(fslist, ','))
			*nextcp++ = '\0';
		if (strcmp(fslist, "ufs") == 0)
			av[i++] = MOUNT_UFS;
		else if (strcmp(fslist, "nfs") == 0)
			av[i++] = MOUNT_NFS;
		else if (strcmp(fslist, "mfs") == 0)
			av[i++] = MOUNT_MFS;
		else if (strcmp(fslist, "msdos") == 0)
			av[i++] = MOUNT_MSDOS;
		else if (strcmp(fslist, "isofs") == 0)
			av[i++] = MOUNT_ISOFS;
	}
	av[i++] = 0;
	return(av);
}

#ifdef	NFS
namematch(hp, nfshost)
	struct hostent *hp;
	char *nfshost;
{
	register char *cp;
	register char **np;

	if (hp == NULL || nfshost == NULL)
		return(1);
	if (strcasecmp(nfshost, hp->h_name) == 0)
		return(1);
	if (cp = index(hp->h_name, '.')) {
		*cp = '\0';
		if (strcasecmp(nfshost, hp->h_name) == 0)
			return(1);
	}
	for (np = hp->h_aliases; *np; np++) {
		if (strcasecmp(nfshost, *np) == 0)
			return(1);
		if (cp = index(*np, '.')) {
			*cp = '\0';
			if (strcasecmp(nfshost, *np) == 0)
				return(1);
		}
	}
	return(0);
}

/*
 * xdr routines for mount rpc's
 */
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}
#endif /* NFS */
