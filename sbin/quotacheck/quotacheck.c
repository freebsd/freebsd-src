/*
 * Copyright (c) 1980, 1990 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
"@(#) Copyright (c) 1980, 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)quotacheck.c	5.16 (Berkeley) 2/28/91";
#endif /* not lint */

/*
 * Fix up / report on disk quotas & usage
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <ufs/dinode.h>
#include <ufs/fs.h>
#include <ufs/quota.h>
#include <fcntl.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;
char *quotagroup = QUOTAGROUP;

union {
	struct	fs	sblk;
	char	dummy[MAXBSIZE];
} un;
#define	sblock	un.sblk
long dev_bsize;
long maxino;

struct quotaname {
	long	flags;
	char	grpqfname[MAXPATHLEN + 1];
	char	usrqfname[MAXPATHLEN + 1];
};
#define	HASUSR	1
#define	HASGRP	2

struct fileusage {
	struct	fileusage *fu_next;
	u_long	fu_curinodes;
	u_long	fu_curblocks;
	u_long	fu_id;
	char	fu_name[1];
	/* actually bigger */
};
#define FUHASH 1024	/* must be power of two */
struct fileusage *fuhead[MAXQUOTAS][FUHASH];
struct fileusage *lookup();
struct fileusage *addid();
struct dinode *getnextinode();

int	aflag;			/* all file systems */
int	gflag;			/* check group quotas */
int	uflag;			/* check user quotas */
int	vflag;			/* verbose */
int	fi;			/* open disk file descriptor */
u_long	highid[MAXQUOTAS];	/* highest addid()'ed identifier per type */

main(argc, argv)
	int argc;
	char **argv;
{
	register struct fstab *fs;
	register struct passwd *pw;
	register struct group *gr;
	int i, argnum, maxrun = 0, errs = 0;
	long auxdata, done = 0;
	char ch, *name, *blockcheck();
	int needchk(), chkquota();
	extern char *optarg;
	extern int optind;

	while ((ch = getopt(argc, argv, "aguvl:")) != EOF) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'l':
			maxrun = atoi(optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ((argc == 0 && !aflag) || (argc > 0 && aflag))
		usage();
	if (!gflag && !uflag) {
		gflag++;
		uflag++;
	}
	if (gflag) {
		setgrent();
		while ((gr = getgrent()) != 0)
			(void) addid((u_long)gr->gr_gid, GRPQUOTA, gr->gr_name);
		endgrent();
	}
	if (uflag) {
		setpwent();
		while ((pw = getpwent()) != 0)
			(void) addid((u_long)pw->pw_uid, USRQUOTA, pw->pw_name);
		endpwent();
	}
	if (aflag)
		exit(checkfstab(1, maxrun, needchk, chkquota));
	if (setfsent() == 0) {
		fprintf(stderr, "Can't open ");
		perror(FSTAB);
		exit(8);
	}
	while ((fs = getfsent()) != NULL) {
		if (((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) &&
		    (auxdata = needchk(fs)) &&
		    (name = blockcheck(fs->fs_spec))) {
			done |= 1 << argnum;
			errs += chkquota(name, fs->fs_file, auxdata);
		}
	}
	endfsent();
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			fprintf(stderr, "%s not found in %s\n",
				argv[i], FSTAB);
	exit(errs);
}

usage()
{
	(void) fprintf(stderr, "usage:\t%s\n\t%s\n",
		"quotacheck -a [-guv]",
		"quotacheck [-guv] filesys ...");
	exit(1);
}

needchk(fs)
	register struct fstab *fs;
{
	register struct quotaname *qnp;
	char *qfnp;

	if (strcmp(fs->fs_vfstype, "ufs") ||
	    strcmp(fs->fs_type, FSTAB_RW))
		return (0);
	if ((qnp = (struct quotaname *)malloc(sizeof *qnp)) == 0) {
		fprintf(stderr, "out of memory for quota structures\n");
		exit(1);
	}
	qnp->flags = 0;
	if (gflag && hasquota(fs, GRPQUOTA, &qfnp)) {
		strcpy(qnp->grpqfname, qfnp);
		qnp->flags |= HASGRP;
	}
	if (uflag && hasquota(fs, USRQUOTA, &qfnp)) {
		strcpy(qnp->usrqfname, qfnp);
		qnp->flags |= HASUSR;
	}
	if (qnp->flags)
		return ((int)qnp);
	free((char *)qnp);
	return (0);
}

/*
 * Scan the specified filesystem to check quota(s) present on it.
 */
chkquota(fsname, mntpt, qnp)
	char *fsname, *mntpt;
	register struct quotaname *qnp;
{
	register struct fileusage *fup;
	register struct dinode *dp;
	int cg, i, mode, errs = 0;
	ino_t ino;

	if ((fi = open(fsname, 0)) < 0) {
		perror(fsname);
		return (1);
	}
	if (vflag) {
		fprintf(stdout, "*** Checking ");
		if (qnp->flags & HASUSR)
			fprintf(stdout, "%s%s", qfextension[USRQUOTA],
			    (qnp->flags & HASGRP) ? " and " : "");
		if (qnp->flags & HASGRP)
			fprintf(stdout, "%s", qfextension[GRPQUOTA]);
		fprintf(stdout, " quotas for %s (%s)\n", fsname, mntpt);
	}
	sync();
	dev_bsize = 1;
	bread(SBOFF, (char *)&sblock, (long)SBSIZE);
	dev_bsize = sblock.fs_fsize / fsbtodb(&sblock, 1);
	maxino = sblock.fs_ncg * sblock.fs_ipg;
	resetinodebuf();
	for (ino = 0, cg = 0; cg < sblock.fs_ncg; cg++) {
		for (i = 0; i < sblock.fs_ipg; i++, ino++) {
			if (ino < ROOTINO)
				continue;
			if ((dp = getnextinode(ino)) == NULL)
				continue;
			if ((mode = dp->di_mode & IFMT) == 0)
				continue;
			if (qnp->flags & HASGRP) {
				fup = addid((u_long)dp->di_gid, GRPQUOTA,
				    (char *)0);
				fup->fu_curinodes++;
				if (mode == IFREG || mode == IFDIR ||
				    mode == IFLNK)
					fup->fu_curblocks += dp->di_blocks;
			}
			if (qnp->flags & HASUSR) {
				fup = addid((u_long)dp->di_uid, USRQUOTA,
				    (char *)0);
				fup->fu_curinodes++;
				if (mode == IFREG || mode == IFDIR ||
				    mode == IFLNK)
					fup->fu_curblocks += dp->di_blocks;
			}
		}
	}
	freeinodebuf();
	if (qnp->flags & HASUSR)
		errs += update(mntpt, qnp->usrqfname, USRQUOTA);
	if (qnp->flags & HASGRP)
		errs += update(mntpt, qnp->grpqfname, GRPQUOTA);
	close(fi);
	return (errs);
}

/*
 * Update a specified quota file.
 */
update(fsname, quotafile, type)
	char *fsname, *quotafile;
	register int type;
{
	register struct fileusage *fup;
	register FILE *qfi, *qfo;
	register u_long id, lastid;
	struct dqblk dqbuf;
	static int warned = 0;
	static struct dqblk zerodqbuf;
	static struct fileusage zerofileusage;

	if ((qfo = fopen(quotafile, "r+")) == NULL) {
		if (errno == ENOENT)
			qfo = fopen(quotafile, "w+");
		if (qfo) {
			(void) fprintf(stderr,
			    "quotacheck: creating quota file %s\n", quotafile);
#define	MODE	(S_IRUSR|S_IWUSR|S_IRGRP)
			(void) fchown(fileno(qfo), getuid(), getquotagid());
			(void) fchmod(fileno(qfo), MODE);
		} else {
			(void) fprintf(stderr,
			    "quotacheck: %s: %s\n", quotafile, strerror(errno));
			return (1);
		}
	}
	if ((qfi = fopen(quotafile, "r")) == NULL) {
		(void) fprintf(stderr,
		    "quotacheck: %s: %s\n", quotafile, strerror(errno));
		(void) fclose(qfo);
		return (1);
	}
	if (quotactl(fsname, QCMD(Q_SYNC, type), (u_long)0, (caddr_t)0) < 0 &&
	    errno == EOPNOTSUPP && !warned && vflag) {
		warned++;
		fprintf(stdout, "*** Warning: %s\n",
		    "Quotas are not compiled into this kernel");
	}
	for (lastid = highid[type], id = 0; id <= lastid; id++) {
		if (fread((char *)&dqbuf, sizeof(struct dqblk), 1, qfi) == 0)
			dqbuf = zerodqbuf;
		if ((fup = lookup(id, type)) == 0)
			fup = &zerofileusage;
		if (dqbuf.dqb_curinodes == fup->fu_curinodes &&
		    dqbuf.dqb_curblocks == fup->fu_curblocks) {
			fup->fu_curinodes = 0;
			fup->fu_curblocks = 0;
			fseek(qfo, (long)sizeof(struct dqblk), 1);
			continue;
		}
		if (vflag) {
			if (aflag)
				printf("%s: ", fsname);
			printf("%-8s fixed:", fup->fu_name);
			if (dqbuf.dqb_curinodes != fup->fu_curinodes)
				fprintf(stdout, "\tinodes %d -> %d",
					dqbuf.dqb_curinodes, fup->fu_curinodes);
			if (dqbuf.dqb_curblocks != fup->fu_curblocks)
				fprintf(stdout, "\tblocks %d -> %d",
					dqbuf.dqb_curblocks, fup->fu_curblocks);
			fprintf(stdout, "\n");
		}
		/*
		 * Reset time limit if have a soft limit and were
		 * previously under it, but are now over it.
		 */
		if (dqbuf.dqb_bsoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_bsoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_bsoftlimit)
			dqbuf.dqb_btime = 0;
		if (dqbuf.dqb_isoftlimit &&
		    dqbuf.dqb_curblocks < dqbuf.dqb_isoftlimit &&
		    fup->fu_curblocks >= dqbuf.dqb_isoftlimit)
			dqbuf.dqb_itime = 0;
		dqbuf.dqb_curinodes = fup->fu_curinodes;
		dqbuf.dqb_curblocks = fup->fu_curblocks;
		fwrite((char *)&dqbuf, sizeof(struct dqblk), 1, qfo);
		(void) quotactl(fsname, QCMD(Q_SETUSE, type), id,
		    (caddr_t)&dqbuf);
		fup->fu_curinodes = 0;
		fup->fu_curblocks = 0;
	}
	fclose(qfi);
	fflush(qfo);
	ftruncate(fileno(qfo),
	    (off_t)((highid[type] + 1) * sizeof(struct dqblk)));
	fclose(qfo);
	return (0);
}

/*
 * Check to see if target appears in list of size cnt.
 */
oneof(target, list, cnt)
	register char *target, *list[];
	int cnt;
{
	register int i;

	for (i = 0; i < cnt; i++)
		if (strcmp(target, list[i]) == 0)
			return (i);
	return (-1);
}

/*
 * Determine the group identifier for quota files.
 */
getquotagid()
{
	struct group *gr;

	if (gr = getgrnam(quotagroup))
		return (gr->gr_gid);
	return (-1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp, *index(), *strtok();
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if (cp = index(opt, '='))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	(void) sprintf(buf, "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}

/*
 * Routines to manage the file usage table.
 *
 * Lookup an id of a specific type.
 */
struct fileusage *
lookup(id, type)
	u_long id;
	int type;
{
	register struct fileusage *fup;

	for (fup = fuhead[type][id & (FUHASH-1)]; fup != 0; fup = fup->fu_next)
		if (fup->fu_id == id)
			return (fup);
	return ((struct fileusage *)0);
}

/*
 * Add a new file usage id if it does not already exist.
 */
struct fileusage *
addid(id, type, name)
	u_long id;
	int type;
	char *name;
{
	struct fileusage *fup, **fhp;
	int len;

	if (fup = lookup(id, type))
		return (fup);
	if (name)
		len = strlen(name);
	else
		len = 10;
	if ((fup = (struct fileusage *)calloc(1, sizeof(*fup) + len)) == NULL) {
		fprintf(stderr, "out of memory for fileusage structures\n");
		exit(1);
	}
	fhp = &fuhead[type][id & (FUHASH - 1)];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_id = id;
	if (id > highid[type])
		highid[type] = id;
	if (name) {
		bcopy(name, fup->fu_name, len + 1);
	} else {
		sprintf(fup->fu_name, "%u", id);
	}
	return (fup);
}

/*
 * Special purpose version of ginode used to optimize pass
 * over all the inodes in numerical order.
 */
ino_t nextino, lastinum;
long readcnt, readpercg, fullcnt, inobufsize, partialcnt, partialsize;
struct dinode *inodebuf;
#define	INOBUFSIZE	56*1024	/* size of buffer to read inodes */

struct dinode *
getnextinode(inumber)
	ino_t inumber;
{
	long size;
	daddr_t dblk;
	static struct dinode *dp;

	if (inumber != nextino++ || inumber > maxino) {
		fprintf(stderr, "bad inode number %d to nextinode\n", inumber);
		exit(1);
	}
	if (inumber >= lastinum) {
		readcnt++;
		dblk = fsbtodb(&sblock, itod(&sblock, lastinum));
		if (readcnt % readpercg == 0) {
			size = partialsize;
			lastinum += partialcnt;
		} else {
			size = inobufsize;
			lastinum += fullcnt;
		}
		bread(dblk, (char *)inodebuf, size);
		dp = inodebuf;
	}
	return (dp++);
}

/*
 * Prepare to scan a set of inodes.
 */
resetinodebuf()
{

	nextino = 0;
	lastinum = 0;
	readcnt = 0;
	inobufsize = blkroundup(&sblock, INOBUFSIZE);
	fullcnt = inobufsize / sizeof(struct dinode);
	readpercg = sblock.fs_ipg / fullcnt;
	partialcnt = sblock.fs_ipg % fullcnt;
	partialsize = partialcnt * sizeof(struct dinode);
	if (partialcnt != 0) {
		readpercg++;
	} else {
		partialcnt = fullcnt;
		partialsize = inobufsize;
	}
	if (inodebuf == NULL &&
	   (inodebuf = (struct dinode *)malloc((unsigned)inobufsize)) == NULL) {
		fprintf(stderr, "Cannot allocate space for inode buffer\n");
		exit(1);
	}
	while (nextino < ROOTINO)
		getnextinode(nextino);
}

/*
 * Free up data structures used to scan inodes.
 */
freeinodebuf()
{

	if (inodebuf != NULL)
		free((char *)inodebuf);
	inodebuf = NULL;
}

/*
 * Read specified disk blocks.
 */
bread(bno, buf, cnt)
	daddr_t bno;
	char *buf;
	long cnt;
{

	if (lseek(fi, bno * dev_bsize, 0) < 0) {
		perror("lseek");
		exit(1);
	}

	if (read(fi, buf, cnt) != cnt) {
		perror("read");
		exit(1);
	}
}
