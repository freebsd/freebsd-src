/*
 * Copyright (c) 1980, 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *
 *	$Id: df.c,v 1.8.2.1 1996/12/13 17:15:11 joerg Exp $
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)df.c	8.7 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* XXX assumes MOUNT_MAXTYPE < 32 */
#define MT(m)		(1 << (m))

/* fixed values */
#define MT_NONE		(0)
#define MT_ALL		(MT(MOUNT_MAXTYPE+1)-1)

/* subject to change */
#define MT_LOCAL \
    (MT(MOUNT_UFS)|MT(MOUNT_MFS)|MT(MOUNT_LFS)|MT(MOUNT_MSDOS)|MT(MOUNT_CD9660))
#define MT_DEFAULT	MT_ALL

struct typetab {
	char *str;
	long types;
} typetab[] = {
	{"ufs",		MT(MOUNT_UFS)},
	{"local",	MT_LOCAL},
	{"all",		MT_ALL},
	{"nfs",		MT(MOUNT_NFS)},
	{"mfs",		MT(MOUNT_MFS)},
	{"lfs",		MT(MOUNT_LFS)},
	{"msdos",	MT(MOUNT_MSDOS)},
	{"fdesc",	MT(MOUNT_FDESC)},
	{"portal",	MT(MOUNT_PORTAL)},
#if 0
	/* return fsid of underlying FS */
	{"lofs",	MT(MOUNT_LOFS)},
	{"null",	MT(MOUNT_NULL)},
	{"umap",	MT(MOUNT_UMAP)},
#endif
	{"kernfs",	MT(MOUNT_KERNFS)},
	{"procfs",	MT(MOUNT_PROCFS)},
	{"afs",		MT(MOUNT_AFS)},
	{"iso9660fs",	MT(MOUNT_CD9660)},
	{"isofs",	MT(MOUNT_CD9660)},
	{"cd9660",	MT(MOUNT_CD9660)},
	{"cdfs",	MT(MOUNT_CD9660)},
	{"misc",	MT(MOUNT_LOFS)|MT(MOUNT_FDESC)|MT(MOUNT_PORTAL)|
			MT(MOUNT_KERNFS)|MT(MOUNT_PROCFS)},
	{NULL,		0}

};

long	addtype __P((long, char *));
long	regetmntinfo __P((struct statfs **, long, long));
int	 bread __P((off_t, void *, int));
char	*getmntpt __P((char *));
void	 prtstat __P((struct statfs *, int));
void	 ufs_df __P((char *, int));
void	 usage __P((void));

int	iflag, nflag, tflag;
struct	ufs_args mdev;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct stat stbuf;
	struct statfs statfsbuf, *mntbuf;
	long fsmask, mntsize;
	int ch, err, i, maxwidth, width;
	char *mntpt;

	iflag = nflag = tflag = 0;
	fsmask = MT_NONE;

	while ((ch = getopt(argc, argv, "iknt:")) != -1)
		switch (ch) {
		case 'i':
			iflag = 1;
			break;
		case 'k':
			putenv("BLOCKSIZE=1k");
			break;
		case 'n':
			nflag = 1;
			break;
		case 't':
			fsmask = addtype(fsmask, optarg);
			tflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	maxwidth = 0;
	for (i = 0; i < mntsize; i++) {
		width = strlen(mntbuf[i].f_mntfromname);
		if (width > maxwidth)
			maxwidth = width;
	}

	if (!*argv) {
		if (!tflag)
			fsmask = MT_DEFAULT;
		mntsize = regetmntinfo(&mntbuf, mntsize, fsmask);
		if (fsmask != MT_ALL) {
			maxwidth = 0;
			for (i = 0; i < mntsize; i++) {
				width = strlen(mntbuf[i].f_mntfromname);
				if (width > maxwidth)
					maxwidth = width;
			}
		}
		for (i = 0; i < mntsize; i++)
			prtstat(&mntbuf[i], maxwidth);
		exit(0);
	}

	for (; *argv; argv++) {
		if (stat(*argv, &stbuf) < 0) {
			err = errno;
			if ((mntpt = getmntpt(*argv)) == 0) {
				warn("%s", *argv);
				continue;
			}
		} else if ((stbuf.st_mode & S_IFMT) == S_IFCHR) {
			ufs_df(*argv, maxwidth);
			continue;
		} else if ((stbuf.st_mode & S_IFMT) == S_IFBLK) {
			if ((mntpt = getmntpt(*argv)) == 0) {
				mntpt = mktemp(strdup("/tmp/df.XXXXXX"));
				mdev.fspec = *argv;
				if (mkdir(mntpt, DEFFILEMODE) != 0) {
					warn("%s", mntpt);
					continue;
				}
				if (mount(MOUNT_UFS, mntpt, MNT_RDONLY,
				    &mdev) != 0) {
					ufs_df(*argv, maxwidth);
					(void)rmdir(mntpt);
					continue;
				} else if (statfs(mntpt, &statfsbuf)) {
					statfsbuf.f_mntonname[0] = '\0';
					prtstat(&statfsbuf, maxwidth);
				} else
					warn("%s", *argv);
				(void)unmount(mntpt, 0);
				(void)rmdir(mntpt);
				continue;
			}
		} else
			mntpt = *argv;
		/*
		 * Statfs does not take a `wait' flag, so we cannot
		 * implement nflag here.
		 */
		if (statfs(mntpt, &statfsbuf) < 0) {
			warn("%s", mntpt);
			continue;
		}
		if (argc == 1)
			maxwidth = strlen(statfsbuf.f_mntfromname) + 1;
		prtstat(&statfsbuf, maxwidth);
	}
	return (0);
}

char *
getmntpt(name)
	char *name;
{
	long mntsize, i;
	struct statfs *mntbuf;

	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		if (!strcmp(mntbuf[i].f_mntfromname, name))
			return (mntbuf[i].f_mntonname);
	}
	return (0);
}

long
addtype(omask, str)
	long omask;
	char *str;
{
	struct typetab *tp;

	/*
	 * If it is one of our known types, add it to the current mask
	 */
	for (tp = typetab; tp->str; tp++)
		if (strcmp(str, tp->str) == 0)
			return (tp->types | (tflag ? omask : MT_NONE));
	/*
	 * See if it is the negation of one of the known values
	 */
	if (strlen(str) > 2 && str[0] == 'n' && str[1] == 'o')
		for (tp = typetab; tp->str; tp++)
			if (strcmp(str+2, tp->str) == 0)
				return (~tp->types & (tflag ? omask : MT_ALL));
	errx(1, "unknown type `%s'", str);
}

/*
 * Make a pass over the filesystem info in ``mntbuf'' filtering out
 * filesystem types not in ``fsmask'' and possibly re-stating to get
 * current (not cached) info.  Returns the new count of valid statfs bufs.
 */
long
regetmntinfo(mntbufp, mntsize, fsmask)
	struct statfs **mntbufp;
	long mntsize, fsmask;
{
	int i, j;
	struct statfs *mntbuf;

	if (fsmask == MT_ALL)
		return (nflag ? mntsize : getmntinfo(mntbufp, MNT_WAIT));

	mntbuf = *mntbufp;
	j = 0;
	for (i = 0; i < mntsize; i++) {
		if (fsmask & MT(mntbuf[i].f_type)) {
			if (!nflag)
				(void)statfs(mntbuf[i].f_mntonname,&mntbuf[j]);
			else if (i != j)
				mntbuf[j] = mntbuf[i];
			j++;
		}
	}
	return (j);
}

/*
 * Convert statfs returned filesystem size into BLOCKSIZE units.
 * Attempts to avoid overflow for large filesystems.
 */
#define fsbtoblk(num, fsbs, bs) \
	(((fsbs) != 0 && (fsbs) < (bs)) ? \
		(num) / ((bs) / (fsbs)) : (num) * ((fsbs) / (bs)))

/*
 * Print out status about a filesystem.
 */
void
prtstat(sfsp, maxwidth)
	struct statfs *sfsp;
	int maxwidth;
{
	static long blocksize;
	static int headerlen, timesthrough;
	static char *header;
	long used, availblks, inodes;

	if (maxwidth < 11)
		maxwidth = 11;
	if (++timesthrough == 1) {
		header = getbsize(&headerlen, &blocksize);
		(void)printf("%-*.*s %s     Used    Avail Capacity",
		    maxwidth, maxwidth, "Filesystem", header);
		if (iflag)
			(void)printf(" iused   ifree  %%iused");
		(void)printf("  Mounted on\n");
	}
	(void)printf("%-*.*s", maxwidth, maxwidth, sfsp->f_mntfromname);
	used = sfsp->f_blocks - sfsp->f_bfree;
	availblks = sfsp->f_bavail + used;
	(void)printf(" %*ld %8ld %8ld", headerlen,
	    fsbtoblk(sfsp->f_blocks, sfsp->f_bsize, blocksize),
	    fsbtoblk(used, sfsp->f_bsize, blocksize),
	    fsbtoblk(sfsp->f_bavail, sfsp->f_bsize, blocksize));
	(void)printf(" %5.0f%%",
	    availblks == 0 ? 100.0 : (double)used / (double)availblks * 100.0);
	if (iflag) {
		inodes = sfsp->f_files;
		used = inodes - sfsp->f_ffree;
		(void)printf(" %7ld %7ld %5.0f%% ", used, sfsp->f_ffree,
		   inodes == 0 ? 100.0 : (double)used / (double)inodes * 100.0);
	} else
		(void)printf("  ");
	(void)printf("  %s\n", sfsp->f_mntonname);
}

/*
 * This code constitutes the pre-system call Berkeley df code for extracting
 * information from filesystem superblocks.
 */
#include <ufs/ffs/fs.h>
#include <errno.h>
#include <fstab.h>

union {
	struct fs iu_fs;
	char dummy[SBSIZE];
} sb;
#define sblock sb.iu_fs

int	rfd;

void
ufs_df(file, maxwidth)
	char *file;
	int maxwidth;
{
	struct statfs statfsbuf;
	struct statfs *sfsp;
	char *mntpt;
	static int synced;

	if (synced++ == 0)
		sync();

	if ((rfd = open(file, O_RDONLY)) < 0) {
		warn("%s", file);
		return;
	}
	if (bread((off_t)SBOFF, &sblock, SBSIZE) == 0) {
		(void)close(rfd);
		return;
	}
	sfsp = &statfsbuf;
	sfsp->f_type = MOUNT_UFS;
	sfsp->f_flags = 0;
	sfsp->f_bsize = sblock.fs_fsize;
	sfsp->f_iosize = sblock.fs_bsize;
	sfsp->f_blocks = sblock.fs_dsize;
	sfsp->f_bfree = sblock.fs_cstotal.cs_nbfree * sblock.fs_frag +
		sblock.fs_cstotal.cs_nffree;
	sfsp->f_bavail = freespace(&sblock, sblock.fs_minfree);
	sfsp->f_files =  sblock.fs_ncg * sblock.fs_ipg;
	sfsp->f_ffree = sblock.fs_cstotal.cs_nifree;
	sfsp->f_fsid.val[0] = 0;
	sfsp->f_fsid.val[1] = 0;
	if ((mntpt = getmntpt(file)) == 0)
		mntpt = "";
	memmove(&sfsp->f_mntonname[0], mntpt, MNAMELEN);
	memmove(&sfsp->f_mntfromname[0], file, MNAMELEN);
	prtstat(sfsp, maxwidth);
	(void)close(rfd);
}

int
bread(off, buf, cnt)
	off_t off;
	void *buf;
	int cnt;
{
	int nr;

	(void)lseek(rfd, off, SEEK_SET);
	if ((nr = read(rfd, buf, cnt)) != cnt) {
		/* Probably a dismounted disk if errno == EIO. */
		if (errno != EIO)
			(void)fprintf(stderr, "\ndf: %qd: %s\n",
			    off, strerror(nr > 0 ? EIO : errno));
		return (0);
	}
	return (1);
}

void
usage()
{
	fprintf(stderr,
		"usage: df [-ikn] [-t fstype] [file | file_system ...]\n");
	exit(1);
}
