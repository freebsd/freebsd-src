/*-
 * Copyright (c) 1980, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
"@(#) Copyright (c) 1980, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)umount.c	8.3 (Berkeley) 2/20/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <netdb.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum { MNTON, MNTFROM } mntwhat;

int	fake, fflag, vflag, *typelist;
char	*nfshost;

int	 fsnametotype __P((char *));
char	*getmntname __P((char *, mntwhat, int *));
void	 maketypelist __P((char *));
int	 selected __P((int));
int	 namematch __P((struct hostent *));
int	 umountall __P((void));
int	 umountfs __P((char *));
void	 usage __P((void));
int	 xdr_dir __P((XDR *, char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int all, ch, errs;

	/* Start disks transferring immediately. */
	sync();

	all = 0;
	while ((ch = getopt(argc, argv, "aFfh:t:v")) != EOF)
		switch (ch) {
		case 'a':
			all = 1;
			break;
		case 'F':
			fake = 1;
			break;
		case 'f':
			fflag = MNT_FORCE;
			break;
		case 'h':	/* -h implies -a. */
			all = 1;
			nfshost = optarg;
			break;
		case 't':
			maketypelist(optarg);
			break;
		case 'v':
			vflag = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	if (argc == 0 && !all || argc != 0 && all)
		usage();

	/* -h implies "-t nfs" if no -t flag. */
	if ((nfshost != NULL) && (typelist == NULL))
		maketypelist("nfs");
		
	if (all) {
		if (setfsent() == 0)
			err(1, "%s", _PATH_FSTAB);
		errs = umountall();
	} else
		for (errs = 0; *argv != NULL; ++argv)
			if (umountfs(*argv) == 0)
				errs = 1;
	exit(errs);
}

int
umountall()
{
	struct fstab *fs;
	int rval, type;
	char *cp;

	while ((fs = getfsent()) != NULL) {
		/* Ignore the root. */
		if (strcmp(fs->fs_file, "/") == 0)
			continue;
		/*
		 * !!!
		 * Historic practice: ignore unknown FSTAB_* fields.
		 */
		if (strcmp(fs->fs_type, FSTAB_RW) &&
		    strcmp(fs->fs_type, FSTAB_RO) &&
		    strcmp(fs->fs_type, FSTAB_RQ))
			continue;
		/* If an unknown file system type, complain. */
		if ((type = fsnametotype(fs->fs_vfstype)) == MOUNT_NONE) {
			warnx("%s: unknown mount type", fs->fs_vfstype);
			continue;
		}
		if (!selected(type))
			continue;

		/* 
		 * We want to unmount the file systems in the reverse order
		 * that they were mounted.  So, we save off the file name
		 * in some allocated memory, and then call recursively.
		 */
		if ((cp = malloc((size_t)strlen(fs->fs_file) + 1)) == NULL)
			err(1, NULL);
		(void)strcpy(cp, fs->fs_file);
		rval = umountall();
		return (umountfs(cp) || rval);
	}
	return (0);
}

int
umountfs(name)
	char *name;
{
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct sockaddr_in saddr;
	struct stat sb;
	struct timeval pertry, try;
	CLIENT *clp;
	int so, type;
	char *delimp, *hostp, *mntpt, rname[MAXPATHLEN];

	if (realpath(name, rname) == NULL) {
		warn("%s", rname);
		return (0);
	}

	name = rname;

	if (stat(name, &sb) < 0) {
		if (((mntpt = getmntname(name, MNTFROM, &type)) == NULL) &&
		    ((mntpt = getmntname(name, MNTON, &type)) == NULL)) {
			warnx("%s: not currently mounted", name);
			return (1);
		}
	} else if (S_ISBLK(sb.st_mode)) {
		if ((mntpt = getmntname(name, MNTON, &type)) == NULL) {
			warnx("%s: not currently mounted", name);
			return (1);
		}
	} else if (S_ISDIR(sb.st_mode)) {
		mntpt = name;
		if ((name = getmntname(mntpt, MNTFROM, &type)) == NULL) {
			warnx("%s: not currently mounted", mntpt);
			return (1);
		}
	} else {
		warnx("%s: not a directory or special device", name);
		return (1);
	}

	if (!selected(type))
		return (0);

	if ((delimp = strchr(name, '@')) != NULL) {
		hostp = delimp + 1;
		*delimp = '\0';
		hp = gethostbyname(hostp);
		*delimp = '@';
	} else if ((delimp = strchr(name, ':')) != NULL) {
		*delimp = '\0';
		hostp = name;
		hp = gethostbyname(hostp);
		name = delimp + 1;
		*delimp = ':';
	} else
		hp = NULL;
	if (!namematch(hp))
		return (0);

	if (vflag)
		(void)printf("%s: unmount from %s\n", name, mntpt);
	if (fake)
		return (0);

	if (unmount(mntpt, fflag) < 0) {
		warn("%s", mntpt);
		return (1);
	}

	if ((hp != NULL) && !(fflag & MNT_FORCE)) {
		*delimp = '\0';
		memset(&saddr, 0, sizeof(saddr));
		saddr.sin_family = AF_INET;
		saddr.sin_port = 0;
		memmove(&saddr.sin_addr, hp->h_addr, hp->h_length);
		pertry.tv_sec = 3;
		pertry.tv_usec = 0;
		so = RPC_ANYSOCK;
		if ((clp = clntudp_create(&saddr,
		    RPCPROG_MNT, RPCMNT_VER1, pertry, &so)) == NULL) {
			clnt_pcreateerror("Cannot MNT PRC");
			return (1);
		}
		clp->cl_auth = authunix_create_default();
		try.tv_sec = 20;
		try.tv_usec = 0;
		clnt_stat = clnt_call(clp,
		    RPCMNT_UMOUNT, xdr_dir, name, xdr_void, (caddr_t)0, try);
		if (clnt_stat != RPC_SUCCESS) {
			clnt_perror(clp, "Bad MNT RPC");
			return (1);
		}
		auth_destroy(clp->cl_auth);
		clnt_destroy(clp);
	}
	return (0);
}

char *
getmntname(name, what, type)
	char *name;
	mntwhat what;
	int *type;
{
	struct statfs *mntbuf;
	int i, mntsize;

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return (NULL);
	}
	for (i = 0; i < mntsize; i++) {
		if ((what == MNTON) && !strcmp(mntbuf[i].f_mntfromname, name)) {
			if (type)
				*type = mntbuf[i].f_type;
			return (mntbuf[i].f_mntonname);
		}
		if ((what == MNTFROM) && !strcmp(mntbuf[i].f_mntonname, name)) {
			if (type)
				*type = mntbuf[i].f_type;
			return (mntbuf[i].f_mntfromname);
		}
	}
	return (NULL);
}

static enum { IN_LIST, NOT_IN_LIST } which;

int
selected(type)
	int type;
{
	/* If no type specified, it's always selected. */
	if (typelist == NULL)
		return (1);
	for (; *typelist != MOUNT_NONE; ++typelist)
		if (type == *typelist)
			return (which == IN_LIST ? 1 : 0);
	return (which == IN_LIST ? 0 : 1);
}

void
maketypelist(fslist)
	char *fslist;
{
	int *av, i;
	char *nextcp;

	if ((fslist == NULL) || (fslist[0] == '\0'))
		errx(1, "empty type list");

	/*
	 * XXX
	 * Note: the syntax is "noxxx,yyy" for no xxx's and
	 * no yyy's, not the more intuitive "noyyy,noyyy".
	 */
	if (fslist[0] == 'n' && fslist[1] == 'o') {
		fslist += 2;
		which = NOT_IN_LIST;
	} else
		which = IN_LIST;

	/* Count the number of types. */
	for (i = 0, nextcp = fslist; *nextcp != NULL; ++nextcp)
		if (*nextcp == ',')
			i++;

	/* Build an array of that many types. */
	if ((av = typelist = malloc((i + 2) * sizeof(int))) == NULL)
		err(1, NULL);
	for (i = 0; fslist != NULL; fslist = nextcp, ++i) {
		if ((nextcp = strchr(fslist, ',')) != NULL)
			*nextcp++ = '\0';
		av[i] = fsnametotype(fslist);
		if (av[i] == MOUNT_NONE)
			errx(1, "%s: unknown mount type", fslist);
	}
	/* Terminate the array. */
	av[i++] = MOUNT_NONE;
}

int
fsnametotype(name)
	char *name;
{
	static char const *namelist[] = INITMOUNTNAMES;
	char const **cp;

	for (cp = namelist; *cp; ++cp)
		if (strcmp(name, *cp) == 0)
			return (cp - namelist);
	return (MOUNT_NONE);
}

int
namematch(hp)
	struct hostent *hp;
{
	char *cp, **np;

	if ((hp == NULL) || (nfshost == NULL))
		return (1);

	if (strcasecmp(nfshost, hp->h_name) == 0)
		return (1);

	if ((cp = strchr(hp->h_name, '.')) != NULL) {
		*cp = '\0';
		if (strcasecmp(nfshost, hp->h_name) == 0)
			return (1);
	}
	for (np = hp->h_aliases; *np; np++) {
		if (strcasecmp(nfshost, *np) == 0)
			return (1);
		if ((cp = strchr(*np, '.')) != NULL) {
			*cp = '\0';
			if (strcasecmp(nfshost, *np) == 0)
				return (1);
		}
	}
	return (0);
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

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s\n       %s\n",
	    "umount [-fv] [-t fstypelist] special | node",
	    "umount -a[fv] [-h host] [-t fstypelist]");
	exit(1);
}
