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
static const char copyright[] =
"@(#) Copyright (c) 1980, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)umount.c	8.8 (Berkeley) 5/8/95";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>

#include <netdb.h>
#include <rpc/rpc.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mounttab.h"

#define ISDOT(x)	((x)[0] == '.' && (x)[1] == '\0')
#define ISDOTDOT(x)	((x)[0] == '.' && (x)[1] == '.' && (x)[2] == '\0')

typedef enum { MNTON, MNTFROM, NOTHING } mntwhat;
typedef enum { MARK, UNMARK, NAME, COUNT, FREE } dowhat;

struct  addrinfo *nfshost_ai = NULL;
int	fflag, vflag;
char   *nfshost;

void	 checkmntlist (char *, char **, char **, char **);
int	 checkvfsname (const char *, char **);
char	*getmntname (const char *, const char *,
	 mntwhat, char **, dowhat);
char 	*getrealname(char *, char *resolved_path);
char   **makevfslist (const char *);
size_t	 mntinfo (struct statfs **);
int	 namematch (struct addrinfo *);
int	 sacmp (struct sockaddr *, struct sockaddr *);
int	 umountall (char **);
int	 checkname (char *, char **);
int	 umountfs (char *, char *, char *);
void	 usage (void);
int	 xdr_dir (XDR *, char *);

int
main(int argc, char *argv[])
{
	int all, errs, ch, mntsize, error;
	char **typelist = NULL, *mntonname, *mntfromname;
	char *type, *mntfromnamerev, *mntonnamerev;
	struct statfs *mntbuf;
	struct addrinfo hints;

	/* Start disks transferring immediately. */
	sync();

	all = errs = 0;
	while ((ch = getopt(argc, argv, "Aafh:t:v")) != -1)
		switch (ch) {
		case 'A':
			all = 2;
			break;
		case 'a':
			all = 1;
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
				err(1, "only one -t option may be specified");
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

	if ((argc == 0 && !all) || (argc != 0 && all))
		usage();

	/* -h implies "-t nfs" if no -t flag. */
	if ((nfshost != NULL) && (typelist == NULL))
		typelist = makevfslist("nfs");

	if (nfshost != NULL) {
		memset(&hints, 0, sizeof hints);
		error = getaddrinfo(nfshost, NULL, &hints, &nfshost_ai);
		if (error)
			errx(1, "%s: %s", nfshost, gai_strerror(error));
	}

	switch (all) {
	case 2:
		if ((mntsize = mntinfo(&mntbuf)) <= 0)
			break;
		/*
		 * We unmount the nfs-mounts in the reverse order
		 * that they were mounted.
		 */
		for (errs = 0, mntsize--; mntsize > 0; mntsize--) {
			if (checkvfsname(mntbuf[mntsize].f_fstypename,
			    typelist))
				continue;
			/*
			 * Check if a mountpoint is laid over by another mount.
			 * A warning will be printed to stderr if this is
			 * the case. The laid over mount remains unmounted.
			 */
			mntonname = mntbuf[mntsize].f_mntonname;
			mntfromname = mntbuf[mntsize].f_mntfromname;
			mntonnamerev = getmntname(getmntname(mntonname,
			    NULL, MNTFROM, &type, NAME), NULL,
			    MNTON, &type, NAME);

			mntfromnamerev = getmntname(mntonnamerev,
			    NULL, MNTFROM, &type, NAME);

			if (strcmp(mntonnamerev, mntonname) == 0 &&
			    strcmp(mntfromnamerev, mntfromname ) != 0)
				warnx("cannot umount %s, %s\n        "
				    "is mounted there, umount it first",
				    mntonname, mntfromnamerev);

			if (checkname(mntbuf[mntsize].f_mntonname,
			    typelist) != 0)
				errs = 1;
		}
		free(mntbuf);
		break;
	case 1:
		if (setfsent() == 0)
			err(1, "%s", _PATH_FSTAB);
		errs = umountall(typelist);
		break;
	case 0:
		for (errs = 0; *argv != NULL; ++argv)
			if (checkname(*argv, typelist) != 0)
				errs = 1;
		break;
	}
	(void)getmntname(NULL, NULL, NOTHING, NULL, FREE);
	exit(errs);
}

int
umountall(char **typelist)
{
	struct xvfsconf vfc;
	struct fstab *fs;
	int rval;
	char *cp;
	static int firstcall = 1;

	if ((fs = getfsent()) != NULL)
		firstcall = 0;
	else if (firstcall)
		errx(1, "fstab reading failure");
	else
		return (0);
	do {
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
		/* Ignore unknown file system types. */
		if (getvfsbyname(fs->fs_vfstype, &vfc) == -1)
			continue;
		if (checkvfsname(fs->fs_vfstype, typelist))
			continue;

		/*
		 * We want to unmount the file systems in the reverse order
		 * that they were mounted.  So, we save off the file name
		 * in some allocated memory, and then call recursively.
		 */
		if ((cp = malloc((size_t)strlen(fs->fs_file) + 1)) == NULL)
			err(1, "malloc failed");
		(void)strcpy(cp, fs->fs_file);
		rval = umountall(typelist);
		rval = checkname(cp, typelist) || rval;
		free(cp);
		return (rval);
	} while ((fs = getfsent()) != NULL);
	return (0);
}

/*
 * Do magic checks on mountpoint and device or hand over
 * it to unmount(2) if everything fails.
 */
int
checkname(char *name, char **typelist)
{
	size_t len;
	int speclen;
	char *mntonname, *mntfromname;
	char *mntfromnamerev;
	char *resolved, realname[MAXPATHLEN];
	char *type, *hostp, *delimp, *origname;

	len = 0;
	mntfromname = mntonname = delimp = hostp = NULL;

	/*
	 * 1. Check if the name exists in the mounttable.
	 */
	(void)checkmntlist(name, &mntfromname, &mntonname, &type);
	/*
	 * 2. Remove trailing slashes if there are any. After that
	 * we look up the name in the mounttable again.
	 */
	if (mntfromname == NULL && mntonname == NULL) {
		speclen = strlen(name);
		for (speclen = strlen(name); 
		    speclen > 1 && name[speclen - 1] == '/';
		    speclen--)
			name[speclen - 1] = '\0';
		(void)checkmntlist(name, &mntfromname, &mntonname, &type);
		resolved = name;
		/* Save off original name in origname */
		if ((origname = strdup(name)) == NULL)
			err(1, "strdup");
		/*
		 * 3. Check if the deprecated nfs-syntax with an '@'
		 * has been used and translate it to the ':' syntax.
		 * Look up the name in the mounttable again.
		 */
		if (mntfromname == NULL && mntonname == NULL) {
			if ((delimp = strrchr(name, '@')) != NULL) {
				hostp = delimp + 1;
				if (*hostp != '\0') {
					/*
					 * Make both '@' and ':'
					 * notations equal 
					 */
					char *host = strdup(hostp);
					len = strlen(hostp);
					if (host == NULL)
						err(1, "strdup");
					memmove(name + len + 1, name,
					    (size_t)(delimp - name));
					name[len] = ':';
					memmove(name, host, len);
					free(host);
				}
				for (speclen = strlen(name); 
				    speclen > 1 && name[speclen - 1] == '/';
				    speclen--)
					name[speclen - 1] = '\0';
				name[len + speclen + 1] = '\0';
				(void)checkmntlist(name, &mntfromname,
				    &mntonname, &type);
				resolved = name;
			}
			/*
			 * 4. Check if a relative mountpoint has been
			 * specified. This should happen as last check,
			 * the order is important. To prevent possible
			 * nfs-hangs, we just call realpath(3) on the
			 * basedir of mountpoint and add the dirname again.
			 * Check the name in mounttable one last time.
			 */
			if (mntfromname == NULL && mntonname == NULL) {
				(void)strcpy(name, origname);
				if ((getrealname(name, realname)) != NULL) {
					(void)checkmntlist(realname,
					    &mntfromname, &mntonname, &type);
					resolved = realname;
				}
				/*
				 * 5. All tests failed, just hand over the
				 * mountpoint to the kernel, maybe the statfs
				 * structure has been truncated or is not
				 * useful anymore because of a chroot(2).
				 * Please note that nfs will not be able to
				 * notify the nfs-server about unmounting.
				 * These things can change in future when the 
				 * fstat structure get's more reliable,
				 * but at the moment we cannot thrust it.
				 */
				if (mntfromname == NULL && mntonname == NULL) {
					(void)strcpy(name, origname);
					if (umountfs(NULL, origname,
					    "none") == 0) {;
						warnx("%s not found in "
						    "mount table, "
						    "unmounted it anyway",
						    origname);
						free(origname);
						return (0);
					} else
						free(origname);
						return (1);
				}
			}
		}
		free(origname);
	} else
		resolved = name;

	if (checkvfsname(type, typelist))
		return (1);

	/*
	 * Check if the reverse entrys of the mounttable are really the
	 * same as the normal ones.
	 */
	if ((mntfromnamerev = strdup(getmntname(getmntname(mntfromname,
	    NULL, MNTON, &type, NAME), NULL, MNTFROM, &type, NAME))) == NULL)
		err(1, "strdup");
	/*
	 * Mark the uppermost mount as unmounted.
	 */
	(void)getmntname(mntfromname, mntonname, NOTHING, &type, MARK);
	/*
	 * If several equal mounts are in the mounttable, check the order
	 * and warn the user if necessary.
	 */
	if (strcmp(mntfromnamerev, mntfromname ) != 0 &&
	    strcmp(resolved, mntonname) != 0) {
		warnx("cannot umount %s, %s\n        "
		    "is mounted there, umount it first",
		    mntonname, mntfromnamerev);

		/* call getmntname again to set mntcheck[i] to 0 */
		(void)getmntname(mntfromname, mntonname,
		    NOTHING, &type, UNMARK);
		return (1);
	}
	free(mntfromnamerev);
	return (umountfs(mntfromname, mntonname, type));
}

/*
 * NFS stuff and unmount(2) call
 */
int
umountfs(char *mntfromname, char *mntonname, char *type)
{
	enum clnt_stat clnt_stat;
	struct timeval try;
	struct addrinfo *ai, hints;
	int do_rpc;
	CLIENT *clp;
	char *nfsdirname, *orignfsdirname;
	char *hostp, *delimp;

	ai = NULL;
	do_rpc = 0;
	hostp = NULL;
	nfsdirname = delimp = orignfsdirname = NULL;
	memset(&hints, 0, sizeof hints);

	if (strcmp(type, "nfs") == 0) {
		if ((nfsdirname = strdup(mntfromname)) == NULL)
			err(1, "strdup");
		orignfsdirname = nfsdirname;
		if ((delimp = strrchr(nfsdirname, ':')) != NULL) {
			*delimp = '\0';
			hostp = nfsdirname;
			getaddrinfo(hostp, NULL, &hints, &ai);
			if (ai == NULL) {
				warnx("can't get net id for host");
			}
			nfsdirname = delimp + 1;
		}

		/*
		 * Check if we have to start the rpc-call later.
		 * If there are still identical nfs-names mounted,
		 * we skip the rpc-call. Obviously this has to
		 * happen before unmount(2), but it should happen
		 * after the previous namecheck.
		 * A non-NULL return means that this is the last
		 * mount from mntfromname that is still mounted.
		 */
		if (getmntname(mntfromname, NULL, NOTHING, &type, COUNT)
		     != NULL)
			do_rpc = 1;
	}

	if (!namematch(ai))
		return (1);
	if (unmount(mntonname, fflag) != 0 ) {
		warn("unmount of %s failed", mntonname);
		return (1);
	}
	if (vflag)
		(void)printf("%s: unmount from %s\n", mntfromname, mntonname);
	/*
	 * Report to mountd-server which nfsname
	 * has been unmounted.
	 */
	if (ai != NULL && !(fflag & MNT_FORCE) && do_rpc) {
		clp = clnt_create(hostp, RPCPROG_MNT, RPCMNT_VER1, "udp");
		if (clp  == NULL) {
			warnx("%s: %s", hostp,
			    clnt_spcreateerror("RPCPROG_MNT"));
			return (1);
		}
		clp->cl_auth = authsys_create_default();
		try.tv_sec = 20;
		try.tv_usec = 0;
		clnt_stat = clnt_call(clp, RPCMNT_UMOUNT, xdr_dir,
		    nfsdirname, xdr_void, (caddr_t)0, try);
		if (clnt_stat != RPC_SUCCESS) {
			warnx("%s: %s", hostp,
			    clnt_sperror(clp, "RPCMNT_UMOUNT"));
			return (1);
		}
		/*
		 * Remove the unmounted entry from /var/db/mounttab.
		 */
		if (read_mtab()) {
			clean_mtab(hostp, nfsdirname, vflag);
			if(!write_mtab(vflag))
				warnx("cannot remove mounttab entry %s:%s",
				    hostp, nfsdirname);
			free_mtab();
		}
		free(orignfsdirname);
		auth_destroy(clp->cl_auth);
		clnt_destroy(clp);
	}
	return (0);
}

char *
getmntname(const char *fromname, const char *onname,
    mntwhat what, char **type, dowhat mark)
{
	static struct statfs *mntbuf;
	static size_t mntsize = 0;
	static char *mntcheck = NULL;
	static char *mntcount = NULL;
	int i, count;

	if (mntsize <= 0) {
		if ((mntsize = mntinfo(&mntbuf)) <= 0)
			return (NULL);
	}
	if (mntcheck == NULL) {
		if ((mntcheck = calloc(mntsize + 1, sizeof(int))) == NULL ||
		    (mntcount = calloc(mntsize + 1, sizeof(int))) == NULL)
			err(1, "calloc");
	}
	/*
	 * We want to get the file systems in the reverse order
	 * that they were mounted. Mounted and unmounted file systems
	 * are marked or unmarked in a table called 'mntcheck'.
	 * Unmount(const char *dir, int flags) does only take the
	 * mountpoint as argument, not the destination. If we don't pay
	 * attention to the order, it can happen that an overlaying
	 * file system gets unmounted instead of the one the user
	 * has choosen.
	 */
	switch (mark) {
	case NAME:
		/* Return only the specific name */
		for (i = mntsize - 1; i >= 0; i--) {
			if (fromname != NULL && what == MNTON &&
			    !strcmp(mntbuf[i].f_mntfromname, fromname) &&
			    mntcheck[i] != 1) {
				if (type)
					*type = mntbuf[i].f_fstypename;
				return (mntbuf[i].f_mntonname);
			}
			if (fromname != NULL && what == MNTFROM &&
			    !strcmp(mntbuf[i].f_mntonname, fromname) &&
			    mntcheck[i] != 1) {
				if (type)
					*type = mntbuf[i].f_fstypename;
				return (mntbuf[i].f_mntfromname);
			}
		}
		return (NULL);
	case MARK:
		/* Mark current mount with '1' and return name */
		for (i = mntsize - 1; i >= 0; i--) {
			if (mntcheck[i] == 0 &&
			    (strcmp(mntbuf[i].f_mntonname, onname) == 0) &&
			    (strcmp(mntbuf[i].f_mntfromname, fromname) == 0)) {
				mntcheck[i] = 1;
				return (mntbuf[i].f_mntonname);
			}
		}
		return (NULL);
	case UNMARK:
		/* Unmark current mount with '0' and return name */
		for (i = 0; i < mntsize; i++) {
			if (mntcheck[i] == 1 &&
			    (strcmp(mntbuf[i].f_mntonname, onname) == 0) &&
			    (strcmp(mntbuf[i].f_mntfromname, fromname) == 0)) {
				mntcheck[i] = 0;
				return (mntbuf[i].f_mntonname);
			}
		}
		return (NULL);
	case COUNT:
		/* Count the equal mntfromnames */
		count = 0;
		for (i = mntsize - 1; i >= 0; i--) {
			if (strcmp(mntbuf[i].f_mntfromname, fromname) == 0)
				count++;
		}
		/* Mark the already unmounted mounts and return
		 * mntfromname if count <= 1. Else return NULL.
		 */
		for (i = mntsize - 1; i >= 0; i--) {
			if (strcmp(mntbuf[i].f_mntfromname, fromname) == 0) {
				if (mntcount[i] == 1)
					count--;
				else {
					mntcount[i] = 1;
					break;
				}
			}
		}
		if (count <= 1)
			return (mntbuf[i].f_mntonname);
		else
			return (NULL);
	case FREE:
		free(mntbuf);
		free(mntcheck);
		free(mntcount);
		return (NULL);
	default:
		return (NULL);
	}
}

int
sacmp(struct sockaddr *sa1, struct sockaddr *sa2)
{
	void *p1, *p2;
	int len;

	if (sa1->sa_family != sa2->sa_family)
		return (1);

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
			return (1);
		break;
	default:
		return (1);
	}

	return memcmp(p1, p2, len);
}

int
namematch(struct addrinfo *ai)
{
	struct addrinfo *aip;

	if (nfshost == NULL || nfshost_ai == NULL)
		return (1);

	while (ai != NULL) {
		aip = nfshost_ai;
		while (aip != NULL) {
			if (sacmp(ai->ai_addr, aip->ai_addr) == 0)
				return (1);
			aip = aip->ai_next;
		}
		ai = ai->ai_next;
	}

	return (0);
}

void
checkmntlist(char *name, char **fromname, char **onname, char **type)
{

	*fromname = getmntname(name, NULL, MNTFROM, type, NAME);
	if (*fromname == NULL) {
		*onname = getmntname(name, NULL, MNTON, type, NAME);
		if (*onname != NULL)
			*fromname = name;
	} else
		*onname = name;
}

size_t
mntinfo(struct statfs **mntbuf)
{
	static struct statfs *origbuf;
	size_t bufsize;
	int mntsize;

	mntsize = getfsstat(NULL, 0, MNT_NOWAIT);
	if (mntsize <= 0)
		return (0);
	bufsize = (mntsize + 1) * sizeof(struct statfs);
	if ((origbuf = malloc(bufsize)) == NULL)
		err(1, "malloc");
	mntsize = getfsstat(origbuf, (long)bufsize, MNT_NOWAIT);
	*mntbuf = origbuf;
	return (mntsize);
}

char *
getrealname(char *name, char *realname)
{
	char *dirname;
	int havedir;
	size_t baselen;
	size_t dirlen;
	
	dirname = '\0';
	havedir = 0;
	if (*name == '/') {
		if (ISDOT(name + 1) || ISDOTDOT(name + 1))
			strcpy(realname, "/");
		else {
			if ((dirname = strrchr(name + 1, '/')) == NULL)
				snprintf(realname, MAXPATHLEN, "%s", name);
			else
				havedir = 1;
		}
	} else {
		if (ISDOT(name) || ISDOTDOT(name))
			(void)realpath(name, realname);
		else {
			if ((dirname = strrchr(name, '/')) == NULL) {
				if ((realpath(name, realname)) == NULL)
					return (NULL);
			} else 
				havedir = 1;
		}
	}
	if (havedir) {
		*dirname++ = '\0';
		if (ISDOT(dirname)) {
			*dirname = '\0';
			if ((realpath(name, realname)) == NULL)
				return (NULL);
		} else if (ISDOTDOT(dirname)) {
			*--dirname = '/';
			if ((realpath(name, realname)) == NULL)
				return (NULL);
		} else {
			if ((realpath(name, realname)) == NULL)
				return (NULL);
			baselen = strlen(realname);
			dirlen = strlen(dirname);
			if (baselen + dirlen + 1 > MAXPATHLEN)
				return (NULL);
			if (realname[1] == '\0') {
				memmove(realname + 1, dirname, dirlen);
				realname[dirlen + 1] = '\0';
			} else {
				realname[baselen] = '/';
				memmove(realname + baselen + 1,
				    dirname, dirlen);
				realname[baselen + dirlen + 1] = '\0';
			}
		}
	}
	return (realname);
}

/*
 * xdr routines for mount rpc's
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{

	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

void
usage()
{

	(void)fprintf(stderr, "%s\n%s\n",
	    "usage: umount [-fv] special | node",
	    "       umount -a | -A [-fv] [-h host] [-t type]");
	exit(1);
}
