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
static char sccsid[] = "@(#)umount.c	8.8 (Berkeley) 5/8/95";
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

int	fake, fflag, vflag;
char	*nfshost;

int	 checkvfsname __P((const char *, char **));
char	*getmntname __P((char *, mntwhat, char **));
char	**makevfslist __P((char *));
int	 selected __P((int));
int	 namematch __P((struct hostent *));
int	 umountall __P((char **));
int	 umountfs __P((char *, char **));
void	 usage __P((void));
int	 xdr_dir __P((XDR *, char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int all, ch, errs, mnts;
	char **typelist = NULL;
	struct statfs *mntbuf;

	/* Start disks transferring immediately. */
	sync();

	all = 0;
	while ((ch = getopt(argc, argv, "AaFfh:t:v")) != -1)
		switch (ch) {
		case 'A':
			all = 2;
			break;
		case 'a':
			all = 1;
			break;
		case 'F':
			fake = 1;
			break;
		case 'f':
			fflag = MNT_FORCE;
			break;
		case 'h':	/* -h implies -A. */
			all = 2;
			nfshost = optarg;
			break;
		case 't':
			if (typelist != NULL)
				errx(1, "only one -t option may be specified.");
			typelist = makevfslist(optarg);
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
		typelist = makevfslist("nfs");

	switch (all) {
	case 2:
		if ((mnts = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
			warn("getmntinfo");
			errs = 1;
			break;
		}
		for (errs = 0, mnts--; mnts > 0; mnts--) {
			if (checkvfsname(mntbuf[mnts].f_fstypename, typelist))
				continue;
			if (umountfs(mntbuf[mnts].f_mntonname, typelist) != 0)
				errs = 1;
		}
		break;
	case 1:
		if (setfsent() == 0)
			err(1, "%s", _PATH_FSTAB);
		errs = umountall(typelist);
		break;
	case 0:
		for (errs = 0; *argv != NULL; ++argv)
			if (umountfs(*argv, typelist) != 0)
				errs = 1;
		break;
	}
	exit(errs);
}

int
umountall(typelist)
	char **typelist;
{
	struct fstab *fs;
	int rval, type;
	char *cp;
	struct vfsconf vfc;

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
		if (getvfsbyname(fs->fs_vfstype, &vfc) < 0) {
			warnx("%s: unknown mount type", fs->fs_vfstype);
			continue;
		}
		if (checkvfsname(fs->fs_vfstype, typelist))
			continue;

		/*
		 * We want to unmount the file systems in the reverse order
		 * that they were mounted.  So, we save off the file name
		 * in some allocated memory, and then call recursively.
		 */
		if ((cp = malloc((size_t)strlen(fs->fs_file) + 1)) == NULL)
			err(1, NULL);
		(void)strcpy(cp, fs->fs_file);
		rval = umountall(typelist);
		return (umountfs(cp, typelist) || rval);
	}
	return (0);
}

int
umountfs(name, typelist)
	char *name;
	char **typelist;
{
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct sockaddr_in saddr;
	struct stat sb;
	struct timeval pertry, try;
	CLIENT *clp;
	int so;
	char *type, *delimp, *hostp, *mntpt, rname[MAXPATHLEN];

	if (realpath(name, rname) == NULL) {
		/* Continue and let the system call check it... */
		strcpy(rname, name);
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

	if (checkvfsname(type, typelist))
		return (1);

	hp = NULL;
	if (!strcmp(type, "nfs")) {
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
		}
	}

	if (!namematch(hp))
		return (1);

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
	char **type;
{
	static struct statfs *mntbuf;
	static int mntsize;
	int i;

	if (mntbuf == NULL &&
	    (mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
		warn("getmntinfo");
		return (NULL);
	}
	for (i = 0; i < mntsize; i++) {
		if ((what == MNTON) && !strcmp(mntbuf[i].f_mntfromname, name)) {
			if (type)
				*type = mntbuf[i].f_fstypename;
			return (mntbuf[i].f_mntonname);
		}
		if ((what == MNTFROM) && !strcmp(mntbuf[i].f_mntonname, name)) {
			if (type)
				*type = mntbuf[i].f_fstypename;
			return (mntbuf[i].f_mntfromname);
		}
	}
	return (NULL);
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
