/*
 * Copyright (c) 1980, 1986, 1993
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
"@(#) Copyright (c) 1980, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 5/14/95";
#endif
#endif /* not lint */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <paths.h>
#include <stdint.h>
#include <string.h>

#include "fsck.h"

static void usage(void) __dead2;
static int argtoi(int flag, const char *req, const char *str, int base);
static int checkfilesys(char *filesys);
static struct statfs *getmntpt(const char *);

int
main(int argc, char *argv[])
{
	int ch;
	struct rlimit rlimit;
	int ret = 0;

	sync();
	skipclean = 1;
	while ((ch = getopt(argc, argv, "b:Bc:dfFm:npy")) != -1) {
		switch (ch) {
		case 'b':
			skipclean = 0;
			bflag = argtoi('b', "number", optarg, 10);
			printf("Alternate super block location: %d\n", bflag);
			break;

		case 'B':
			bkgrdflag = 1;
			break;

		case 'c':
			skipclean = 0;
			cvtlevel = argtoi('c', "conversion level", optarg, 10);
			if (cvtlevel < 3)
				errx(EEXIT, "cannot do level %d conversion",
				    cvtlevel);
			break;

		case 'd':
			debug++;
			break;

		case 'f':
			skipclean = 0;
			break;

		case 'F':
			bkgrdcheck = 1;
			break;

		case 'm':
			lfmode = argtoi('m', "mode", optarg, 8);
			if (lfmode &~ 07777)
				errx(EEXIT, "bad mode to -m: %o", lfmode);
			printf("** lost+found creation mode %o\n", lfmode);
			break;

		case 'n':
			nflag++;
			yflag = 0;
			break;

		case 'p':
			preen++;
			break;

		case 'y':
			yflag++;
			nflag = 0;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, catch);
	if (preen)
		(void)signal(SIGQUIT, catchquit);
	signal(SIGINFO, infohandler);
	/*
	 * Push up our allowed memory limit so we can cope
	 * with huge filesystems.
	 */
	if (getrlimit(RLIMIT_DATA, &rlimit) == 0) {
		rlimit.rlim_cur = rlimit.rlim_max;
		(void)setrlimit(RLIMIT_DATA, &rlimit);
	}
	while (argc-- > 0)
		(void)checkfilesys(*argv++);

	if (returntosingle)
		ret = 2;
	exit(ret);
}

static int
argtoi(int flag, const char *req, const char *str, int base)
{
	char *cp;
	int ret;

	ret = (int)strtol(str, &cp, base);
	if (cp == str || *cp)
		errx(EEXIT, "-%c flag requires a %s", flag, req);
	return (ret);
}

/*
 * Check the specified filesystem.
 */
/* ARGSUSED */
static int
checkfilesys(char *filesys)
{
	ufs2_daddr_t n_ffree, n_bfree;
	struct ufs_args args;
	struct dups *dp;
	struct statfs *mntp;
	struct zlncnt *zlnp;
	ufs2_daddr_t blks;
	int cylno, ret;
	ino_t files;
	size_t size;

	cdevname = filesys;
	if (debug && preen)
		pwarn("starting\n");
	/*
	 * Make best effort to get the disk name. Check first to see
	 * if it is listed among the mounted filesystems. Failing that
	 * check to see if it is listed in /etc/fstab.
	 */
	mntp = getmntpt(filesys);
	if (mntp != NULL)
		filesys = mntp->f_mntfromname;
	else
		filesys = blockcheck(filesys);
	/*
	 * If -F flag specified, check to see whether a background check
	 * is possible and needed. If possible and needed, exit with
	 * status zero. Otherwise exit with status non-zero. A non-zero
	 * exit status will cause a foreground check to be run.
	 */
	sblock_init();
	if (bkgrdcheck) {
		if ((fsreadfd = open(filesys, O_RDONLY)) < 0 || readsb(0) == 0)
			exit(3);	/* Cannot read superblock */
		close(fsreadfd);
		if (sblock.fs_flags & FS_NEEDSFSCK)
			exit(4);	/* Earlier background failed */
		if ((sblock.fs_flags & FS_DOSOFTDEP) == 0)
			exit(5);	/* Not running soft updates */
		size = MIBSIZE;
		if (sysctlnametomib("vfs.ffs.adjrefcnt", adjrefcnt, &size) < 0)
			exit(6);	/* Lacks kernel support */
		if ((mntp == NULL && sblock.fs_clean == 1) ||
		    (mntp != NULL && (sblock.fs_flags & FS_UNCLEAN) == 0))
			exit(7);	/* Filesystem clean, report it now */
		exit(0);
	}
	/*
	 * If we are to do a background check:
	 *	Get the mount point information of the filesystem
	 *	create snapshot file
	 *	return created snapshot file
	 *	if not found, clear bkgrdflag and proceed with normal fsck
	 */
	if (bkgrdflag) {
		if (mntp == NULL) {
			bkgrdflag = 0;
			pfatal("NOT MOUNTED, CANNOT RUN IN BACKGROUND\n");
		} else if ((mntp->f_flags & MNT_SOFTDEP) == 0) {
			bkgrdflag = 0;
			pfatal("NOT USING SOFT UPDATES, %s\n",
			    "CANNOT RUN IN BACKGROUND");
		} else if ((mntp->f_flags & MNT_RDONLY) != 0) {
			bkgrdflag = 0;
			pfatal("MOUNTED READ-ONLY, CANNOT RUN IN BACKGROUND\n");
		} else if ((fsreadfd = open(filesys, O_RDONLY)) >= 0) {
			if (readsb(0) != 0) {
				if (sblock.fs_flags & FS_NEEDSFSCK) {
					bkgrdflag = 0;
					pfatal("UNEXPECTED INCONSISTENCY, %s\n",
					    "CANNOT RUN IN BACKGROUND\n");
				}
				if ((sblock.fs_flags & FS_UNCLEAN) == 0 &&
				    skipclean && preen) {
					/*
					 * filesystem is clean;
					 * skip snapshot and report it clean
					 */
					pwarn("FILESYSTEM CLEAN; %s\n",
					    "SKIPPING CHECKS");
					goto clean;
				}
			}
			close(fsreadfd);
		}
		if (bkgrdflag) {
			snprintf(snapname, sizeof snapname, "%s/.fsck_snapshot",
			    mntp->f_mntonname);
			args.fspec = snapname;
			while (mount("ffs", mntp->f_mntonname,
			    mntp->f_flags | MNT_UPDATE | MNT_SNAPSHOT,
			    &args) < 0) {
				if (errno == EEXIST && unlink(snapname) == 0)
					continue;
				bkgrdflag = 0;
				pfatal("CANNOT CREATE SNAPSHOT %s: %s\n",
				    snapname, strerror(errno));
				break;
			}
			if (bkgrdflag != 0)
				filesys = snapname;
		}
	}

	switch (setup(filesys)) {
	case 0:
		if (preen)
			pfatal("CAN'T CHECK FILE SYSTEM.");
		return (0);
	case -1:
	clean:
		pwarn("clean, %ld free ", (long)(sblock.fs_cstotal.cs_nffree +
		    sblock.fs_frag * sblock.fs_cstotal.cs_nbfree));
		printf("(%lld frags, %lld blocks, %.1f%% fragmentation)\n",
		    (long long)sblock.fs_cstotal.cs_nffree,
		    (long long)sblock.fs_cstotal.cs_nbfree,
		    sblock.fs_cstotal.cs_nffree * 100.0 / sblock.fs_dsize);
		return (0);
	}
	
	/*
	 * Cleared if any questions answered no. Used to decide if
	 * the superblock should be marked clean.
	 */
	resolved = 1;
	/*
	 * 1: scan inodes tallying blocks used
	 */
	if (preen == 0) {
		printf("** Last Mounted on %s\n", sblock.fs_fsmnt);
		if (mntp != NULL && mntp->f_flags & MNT_ROOTFS)
			printf("** Root filesystem\n");
		printf("** Phase 1 - Check Blocks and Sizes\n");
	}
	pass1();

	/*
	 * 1b: locate first references to duplicates, if any
	 */
	if (duplist) {
		if (preen || usedsoftdep)
			pfatal("INTERNAL ERROR: dups with -p");
		printf("** Phase 1b - Rescan For More DUPS\n");
		pass1b();
	}

	/*
	 * 2: traverse directories from root to mark all connected directories
	 */
	if (preen == 0)
		printf("** Phase 2 - Check Pathnames\n");
	pass2();

	/*
	 * 3: scan inodes looking for disconnected directories
	 */
	if (preen == 0)
		printf("** Phase 3 - Check Connectivity\n");
	pass3();

	/*
	 * 4: scan inodes looking for disconnected files; check reference counts
	 */
	if (preen == 0)
		printf("** Phase 4 - Check Reference Counts\n");
	pass4();

	/*
	 * 5: check and repair resource counts in cylinder groups
	 */
	if (preen == 0)
		printf("** Phase 5 - Check Cyl groups\n");
	pass5();

	/*
	 * print out summary statistics
	 */
	n_ffree = sblock.fs_cstotal.cs_nffree;
	n_bfree = sblock.fs_cstotal.cs_nbfree;
	files = maxino - ROOTINO - sblock.fs_cstotal.cs_nifree - n_files;
	blks = n_blks +
	    sblock.fs_ncg * (cgdmin(&sblock, 0) - cgsblock(&sblock, 0));
	blks += cgsblock(&sblock, 0) - cgbase(&sblock, 0);
	blks += howmany(sblock.fs_cssize, sblock.fs_fsize);
	blks = maxfsblock - (n_ffree + sblock.fs_frag * n_bfree) - blks;
	if (bkgrdflag && (files > 0 || blks > 0)) {
		countdirs = sblock.fs_cstotal.cs_ndir - countdirs;
		pwarn("Reclaimed: %ld directories, %ld files, %lld fragments\n",
		    countdirs, (long)files - countdirs, (long long)blks);
	}
	pwarn("%ld files, %jd used, %ju free ",
	    (long)n_files, (intmax_t)n_blks,
	    (uintmax_t)n_ffree + sblock.fs_frag * n_bfree);
	printf("(%ju frags, %ju blocks, %.1f%% fragmentation)\n",
	    (uintmax_t)n_ffree, (uintmax_t)n_bfree,
	    n_ffree * 100.0 / sblock.fs_dsize);
	if (debug) {
		if (files < 0)
			printf("%d inodes missing\n", -files);
		if (blks < 0)
			printf("%lld blocks missing\n", -(long long)blks);
		if (duplist != NULL) {
			printf("The following duplicate blocks remain:");
			for (dp = duplist; dp; dp = dp->next)
				printf(" %lld,", (long long)dp->dup);
			printf("\n");
		}
		if (zlnhead != NULL) {
			printf("The following zero link count inodes remain:");
			for (zlnp = zlnhead; zlnp; zlnp = zlnp->next)
				printf(" %u,", zlnp->zlncnt);
			printf("\n");
		}
	}
	zlnhead = (struct zlncnt *)0;
	duplist = (struct dups *)0;
	muldup = (struct dups *)0;
	inocleanup();
	if (fsmodified) {
		sblock.fs_time = time(NULL);
		sbdirty();
	}
	if (cvtlevel && sblk.b_dirty) {
		/*
		 * Write out the duplicate super blocks
		 */
		for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
			bwrite(fswritefd, (char *)&sblock,
			    fsbtodb(&sblock, cgsblock(&sblock, cylno)),
			    SBLOCKSIZE);
	}
	if (rerun)
		resolved = 0;

	/*
	 * Check to see if the filesystem is mounted read-write.
	 */
	if (bkgrdflag == 0 && mntp != NULL && (mntp->f_flags & MNT_RDONLY) == 0)
		resolved = 0;
	ckfini(resolved);

	for (cylno = 0; cylno < sblock.fs_ncg; cylno++)
		if (inostathead[cylno].il_stat != NULL)
			free((char *)inostathead[cylno].il_stat);
	free((char *)inostathead);
	inostathead = NULL;
	if (fsmodified && !preen)
		printf("\n***** FILE SYSTEM WAS MODIFIED *****\n");
	if (rerun)
		printf("\n***** PLEASE RERUN FSCK *****\n");
	if (mntp != NULL) {
		/*
		 * We modified a mounted filesystem.  Do a mount update on
		 * it unless it is read-write, so we can continue using it
		 * as safely as possible.
		 */
		if (mntp->f_flags & MNT_RDONLY) {
			args.fspec = 0;
			args.export.ex_flags = 0;
			args.export.ex_root = 0;
			ret = mount("ufs", mntp->f_mntonname,
			    mntp->f_flags | MNT_UPDATE | MNT_RELOAD, &args);
			if (ret == 0)
				return (0);
			pwarn("mount reload of '%s' failed: %s\n\n",
			    mntp->f_mntonname, strerror(errno));
		}
		if (!fsmodified)
			return (0);
		if (!preen)
			printf("\n***** REBOOT NOW *****\n");
		sync();
		return (4);
	}
	return (0);
}

/*
 * Get the mount point information for name.
 */
static struct statfs *
getmntpt(const char *name)
{
	struct stat devstat, mntdevstat;
	char device[sizeof(_PATH_DEV) - 1 + MNAMELEN];
	char *ddevname;
	struct statfs *mntbuf, *statfsp;
	int i, mntsize, isdev;

	if (stat(name, &devstat) != 0)
		return (NULL);
	if (S_ISCHR(devstat.st_mode) || S_ISBLK(devstat.st_mode))
		isdev = 1;
	else
		isdev = 0;
	mntsize = getmntinfo(&mntbuf, MNT_NOWAIT);
	for (i = 0; i < mntsize; i++) {
		statfsp = &mntbuf[i];
		ddevname = statfsp->f_mntfromname;
		if (*ddevname != '/') {
			strcpy(device, _PATH_DEV);
			strcat(device, ddevname);
			strcpy(statfsp->f_mntfromname, device);
		}
		if (isdev == 0) {
			if (strcmp(name, statfsp->f_mntonname))
				continue;
			return (statfsp);
		}
		if (stat(ddevname, &mntdevstat) == 0 &&
		    mntdevstat.st_rdev == devstat.st_rdev)
			return (statfsp);
	}
	statfsp = NULL;
	return (statfsp);
}

static void
usage(void)
{
        (void) fprintf(stderr,
            "usage: %s [-BFpfny] [-b block] [-c level] [-m mode] "
                        "filesystem ...\n",
            getprogname());
        exit(1);
}
