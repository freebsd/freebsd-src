/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (C) 1991, 1994 Wolfgang Solfrank.
 * Copyright (C) 1991, 1994 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/disklabel.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <libufs.h>
#include <mntopts.h>
#include <paths.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* some flags of what to do: */
static bool all;
static bool count;
static bool noname;
static bool unused;
static void (*func)(int, struct fs *);
static long blocksize;
static char *header;
static int headerlen;

static union dinode *get_inode(int, struct fs *, ino_t);
static int	isfree(struct fs *, union dinode *);
static void	inituser(void);
static void	usrrehash(void);
static struct user *user(uid_t);
static int	cmpusers(const void *, const void *);
static void	uses(uid_t, daddr_t, time_t);
static void	initfsizes(void);
static void	dofsizes(int, struct fs *);
static void	douser(int, struct fs *);
static void	donames(int, struct fs *);
static void	usage(void);
static void	quot(char *, char *);

/*
 * Original BSD quot doesn't round to number of frags/blocks,
 * doesn't account for indirection blocks and gets it totally
 * wrong if the	size is a multiple of the blocksize.
 * The new code always counts the number of 512 byte blocks
 * instead of the number of kilobytes and converts them	to
 * kByte when done (on request).
 *
 * Due to the size of modern disks, we must cast intermediate
 * values to 64 bits to prevent potential overflows.
 */
#define	SIZE(n) ((int)(((intmax_t)(n) * 512 + blocksize - 1) / blocksize))

#define	INOCNT(fs)	((fs)->fs_ipg)
#define	INOSZ(fs) \
	(((fs)->fs_magic == FS_UFS1_MAGIC ? sizeof(struct ufs1_dinode) : \
	sizeof(struct ufs2_dinode)) * INOCNT(fs))

#define	DIP(fs, dp, field) \
	(((fs)->fs_magic == FS_UFS1_MAGIC) ? \
	(dp)->dp1.field : (dp)->dp2.field)

static union dinode *
get_inode(int fd, struct fs *super, ino_t ino)
{
	static union dinode *ipbuf;
	static struct cg *cgp;
	static ino_t last;
	static unsigned long cg;
	struct ufs2_dinode *di2;
	off_t off;

	if (fd < 0) {		/* flush cache */
		free(ipbuf);
		ipbuf = NULL;
		free(cgp);
		cgp = NULL;
		return (NULL);
	}

	if (ipbuf == NULL || ino < last || ino >= last + INOCNT(super)) {
		if (super->fs_magic == FS_UFS2_MAGIC &&
		    (cgp == NULL || cg != ino_to_cg(super, ino))) {
			cg = ino_to_cg(super, ino);
			if (cgp == NULL && (cgp = malloc(super->fs_cgsize)) == NULL)
				errx(1, "allocate cg");
			if (lseek(fd, (off_t)cgtod(super, cg) << super->fs_fshift, 0) < 0)
				err(1, "lseek cg");
			if (read(fd, cgp, super->fs_cgsize) != super->fs_cgsize)
				err(1, "read cg");
			if (!cg_chkmagic(cgp))
				errx(1, "cg has bad magic");
		}
		if (ipbuf == NULL && (ipbuf = malloc(INOSZ(super))) == NULL)
			errx(1, "allocate inodes");
		last = rounddown(ino, INOCNT(super));
		off = (off_t)ino_to_fsba(super, last) << super->fs_fshift;
		if (lseek(fd, off, SEEK_SET) != off ||
		    read(fd, ipbuf, INOSZ(super)) != (ssize_t)INOSZ(super))
			err(1, "read inodes");
	}

	if (super->fs_magic == FS_UFS1_MAGIC)
		return ((union dinode *)
		    &((struct ufs1_dinode *)ipbuf)[ino % INOCNT(super)]);
	di2 = &((struct ufs2_dinode *)ipbuf)[ino % INOCNT(super)];
	/* If the inode is unused, it might be unallocated too, so zero it. */
	if (isclr(cg_inosused(cgp), ino % super->fs_ipg))
		memset(di2, 0, sizeof(*di2));
	return ((union dinode *)di2);
}

static int
isfree(struct fs *super, union dinode *dp)
{
	switch (DIP(super, dp, di_mode) & IFMT) {
	case IFIFO:
	case IFLNK:		/* should check FASTSYMLINK? */
	case IFDIR:
	case IFREG:
		return 0;
	case IFCHR:
	case IFBLK:
	case IFSOCK:
	case IFWHT:
	case 0:
		return 1;
	default:
		errx(1, "unknown IFMT 0%o", DIP(super, dp, di_mode) & IFMT);
	}
}

static struct user {
	uid_t uid;
	char *name;
	daddr_t space;
	long count;
	daddr_t spc30;
	daddr_t spc60;
	daddr_t spc90;
} *users;
static unsigned int nusers;

static void
inituser(void)
{
	struct user *usr;
	unsigned int i;

	if (nusers == 0) {
		nusers = 8;
		if ((users = calloc(nusers, sizeof(*users))) == NULL)
			errx(1, "allocate users");
	} else {
		for (usr = users, i = nusers; i-- > 0; usr++) {
			usr->space = usr->spc30 = usr->spc60 = usr->spc90 = 0;
			usr->count = 0;
		}
	}
}

static void
usrrehash(void)
{
	struct user *usr, *usrn;
	struct user *svusr;
	unsigned int i;

	svusr = users;
	nusers *= 2;
	if ((users = calloc(nusers, sizeof(*users))) == NULL)
		errx(1, "allocate users");
	for (usr = svusr, i = nusers / 2; i-- > 0; usr++) {
		for (usrn = users + usr->uid % nusers; usrn->name; usrn--) {
			if (usrn <= users)
				usrn += nusers;
		}
		*usrn = *usr;
	}
}

static struct user *
user(uid_t uid)
{
	struct user *usr;
	struct passwd *pwd;
	unsigned int i;

	while (1) {
		for (usr = users + uid % nusers, i = nusers; i-- > 0; usr--) {
			if (usr->name == NULL) {
				usr->uid = uid;
				if (noname || (pwd = getpwuid(uid)) == NULL)
					asprintf(&usr->name, "#%u", uid);
				else
					usr->name = strdup(pwd->pw_name);
				if (usr->name == NULL)
					errx(1, "allocate users");
			}
			if (usr->uid == uid)
				return (usr);
			if (usr <= users)
				usr += nusers;
		}
		usrrehash();
	}
}

static int
cmpusers(const void *v1, const void *v2)
{
	const struct user *u1 = v1, *u2 = v2;

	return (u2->space > u1->space ? 1 :
	    u2->space < u1->space ? -1 :
	    u1->uid > u2->uid ? 1 :
	    u1->uid < u2->uid ? -1 : 0);
}

#define	sortusers(users)						\
	qsort((users), nusers, sizeof(struct user), cmpusers)

static void
uses(uid_t uid, daddr_t blks, time_t act)
{
	static time_t today;
	struct user *usr;

	if (!today)
		time(&today);

	usr = user(uid);
	usr->count++;
	usr->space += blks;

	if (today - act > 90L * 24L * 60L * 60L)
		usr->spc90 += blks;
	if (today - act > 60L * 24L * 60L * 60L)
		usr->spc60 += blks;
	if (today - act > 30L * 24L * 60L * 60L)
		usr->spc30 += blks;
}

#define	FSZCNT	512U
static struct fsizes {
	struct fsizes *fsz_next;
	daddr_t fsz_first, fsz_last;
	ino_t fsz_count[FSZCNT];
	daddr_t fsz_sz[FSZCNT];
} *fsizes;

static void
initfsizes(void)
{
	struct fsizes *fp;
	unsigned int i;

	for (fp = fsizes; fp; fp = fp->fsz_next) {
		for (i = FSZCNT; i-- > 0;) {
			fp->fsz_count[i] = 0;
			fp->fsz_sz[i] = 0;
		}
	}
}

static void
dofsizes(int fd, struct fs *super)
{
	ino_t inode, maxino;
	union dinode *dp;
	daddr_t sz, ksz;
	struct fsizes *fp, **fsp;
	unsigned int i;

	maxino = super->fs_ncg * super->fs_ipg - 1;
	for (inode = 0; inode < maxino; inode++) {
		if ((dp = get_inode(fd, super, inode)) != NULL &&
		    !isfree(super, dp)) {
			sz = DIP(super, dp, di_blocks);
			ksz = SIZE(sz);
			for (fsp = &fsizes; (fp = *fsp); fsp = &fp->fsz_next) {
				if (ksz < fp->fsz_last)
					break;
			}
			if (fp == NULL || ksz < fp->fsz_first) {
				if ((fp = malloc(sizeof(*fp))) == NULL)
					errx(1, "allocate fsize structure");
				fp->fsz_next = *fsp;
				*fsp = fp;
				fp->fsz_first = rounddown(ksz, FSZCNT);
				fp->fsz_last = fp->fsz_first + FSZCNT;
				for (i = FSZCNT; i-- > 0;) {
					fp->fsz_count[i] = 0;
					fp->fsz_sz[i] = 0;
				}
			}
			fp->fsz_count[ksz % FSZCNT]++;
			fp->fsz_sz[ksz % FSZCNT] += sz;
		}
	}
	sz = 0;
	for (fp = fsizes; fp != NULL; fp = fp->fsz_next) {
		for (i = 0; i < FSZCNT; i++) {
			if (fp->fsz_count[i] != 0) {
				printf("%jd\t%jd\t%d\n",
				    (intmax_t)(fp->fsz_first + i),
				    (intmax_t)fp->fsz_count[i],
				    SIZE(sz += fp->fsz_sz[i]));
			}
		}
	}
}

static void
douser(int fd, struct fs *super)
{
	ino_t inode, maxino;
	struct user *usr, *usrs;
	union dinode *dp;
	int n;

	maxino = super->fs_ncg * super->fs_ipg - 1;
	for (inode = 0; inode < maxino; inode++) {
		if ((dp = get_inode(fd, super, inode)) != NULL &&
		    !isfree(super, dp)) {
			uses(DIP(super, dp, di_uid),
			    DIP(super, dp, di_blocks),
			    DIP(super, dp, di_atime));
		}
	}
	if ((usrs = malloc(nusers * sizeof(*usrs))) == NULL)
		errx(1, "allocate users");
	memcpy(usrs, users, nusers * sizeof(*usrs));
	sortusers(usrs);
	for (usr = usrs, n = nusers; --n >= 0 && usr->count; usr++) {
		printf("%5d", SIZE(usr->space));
		if (count)
			printf("\t%5ld", usr->count);
		printf("\t%-8s", usr->name);
		if (unused) {
			printf("\t%5d\t%5d\t%5d",
			    SIZE(usr->spc30),
			    SIZE(usr->spc60),
			    SIZE(usr->spc90));
		}
		printf("\n");
	}
	free(usrs);
}

static void
donames(int fd, struct fs *super)
{
	int c;
	ino_t maxino;
	uintmax_t inode;
	union dinode *dp;

	maxino = super->fs_ncg * super->fs_ipg - 1;
	/* first skip the name of the filesystem */
	while ((c = getchar()) != EOF && (c < '0' || c > '9'))
		while ((c = getchar()) != EOF && c != '\n');
	ungetc(c, stdin);
	while (scanf("%ju", &inode) == 1) {
		if (inode > maxino) {
			warnx("illegal inode %ju", inode);
			return;
		}
		if ((dp = get_inode(fd, super, inode)) != NULL &&
		    !isfree(super, dp)) {
			printf("%s\t", user(DIP(super, dp, di_uid))->name);
			/* now skip whitespace */
			while ((c = getchar()) == ' ' || c == '\t')
				/* nothing */;
			/* and print out the remainder of the input line */
			while (c != EOF && c != '\n') {
				putchar(c);
				c = getchar();
			}
			putchar('\n');
		} else {
			/* skip this line */
			while ((c = getchar()) != EOF && c != '\n')
				/* nothing */;
		}
		if (c == EOF)
			break;
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: quot [-cfknv] [-a | filesystem ...]\n");
	exit(1);
}

void
quot(char *name, char *mp)
{
	int fd;
	struct fs *fs;

	get_inode(-1, NULL, 0);		/* flush cache */
	inituser();
	initfsizes();
	if ((fd = open(name, 0)) < 0) {
		warn("%s", name);
		close(fd);
		return;
	}
	switch (errno = sbget(fd, &fs, UFS_STDSB, UFS_NOCSUM)) {
	case 0:
		break;
	case ENOENT:
		warn("Cannot find file system superblock");
		close(fd);
		return;
	default:
		warn("Unable to read file system superblock");
		close(fd);
		return;
	}
	printf("%s:", name);
	if (mp)
		printf(" (%s)", mp);
	putchar('\n');
	(*func)(fd, fs);
	free(fs);
	close(fd);
}

int
main(int argc, char *argv[])
{
	struct statfs *mp;
	int ch, cnt;

	func = douser;
	header = getbsize(&headerlen, &blocksize);
	while ((ch = getopt(argc, argv, "acfhkNnv")) != -1) {
		switch (ch) {
		case 'a':
			all = true;
			break;
		case 'c':
			func = dofsizes;
			break;
		case 'f':
			count = true;
			break;
		case 'h':
			/* ignored for backward compatibility */
			break;
		case 'k':
			blocksize = 1024;
			break;
		case 'N':
			noname = true;
			break;
		case 'n':
			func = donames;
			break;
		case 'v':
			unused = true;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((argc == 0 && !all) || (all && argc))
		usage();

	if (all) {
		for (cnt = getmntinfo(&mp, MNT_NOWAIT); --cnt >= 0; mp++)
			if (strncmp(mp->f_fstypename, "ufs", MFSNAMELEN) == 0)
				quot(mp->f_mntfromname, mp->f_mntonname);
	}
	while (argc-- > 0) {
		if ((mp = getmntpoint(*argv)) != NULL)
			quot(mp->f_mntfromname, mp->f_mntonname);
		else
			quot(*argv, 0);
		argv++;
	}
	return (0);
}
