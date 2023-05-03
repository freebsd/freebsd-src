/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 * 3. Neither the name of the University nor the names of its contributors
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
"@(#) Copyright (c) 1980, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 5/14/95";
#endif /* not lint */
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _WANT_P_OSREL
#include <sys/param.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <inttypes.h>
#include <libufs.h>
#include <mntopts.h>
#include <paths.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "fsck.h"

static int	restarts;

static void usage(void) __dead2;
static intmax_t argtoimax(int flag, const char *req, const char *str, int base);
static int checkfilesys(char *filesys);
static int setup_bkgrdchk(struct statfs *mntp, int sbrdfailed, char **filesys);

int
main(int argc, char *argv[])
{
	int ch;
	struct rlimit rlimit;
	struct itimerval itimerval;
	int fsret;
	int ret = 0;

	sync();
	skipclean = 1;
	inoopt = 0;
	while ((ch = getopt(argc, argv, "b:Bc:CdEfFm:npRrSyZz")) != -1) {
		switch (ch) {
		case 'b':
			skipclean = 0;
			bflag = argtoimax('b', "number", optarg, 10);
			printf("Alternate super block location: %jd\n", bflag);
			break;

		case 'B':
			bkgrdflag = 1;
			break;

		case 'c':
			skipclean = 0;
			cvtlevel = argtoimax('c', "conversion level", optarg,
			    10);
			if (cvtlevel < 3)
				errx(EEXIT, "cannot do level %d conversion",
				    cvtlevel);
			break;

		case 'd':
			debug++;
			break;

		case 'E':
			Eflag++;
			break;

		case 'f':
			skipclean = 0;
			break;

		case 'F':
			bkgrdcheck = 1;
			break;

		case 'm':
			lfmode = argtoimax('m', "mode", optarg, 8);
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
			/*FALLTHROUGH*/

		case 'C':
			ckclean++;
			break;

		case 'R':
			wantrestart = 1;
			break;
		case 'r':
			inoopt++;
			break;

		case 'S':
			surrender = 1;
			break;

		case 'y':
			yflag++;
			nflag = 0;
			break;

		case 'Z':
			Zflag++;
			break;

		case 'z':
			zflag++;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc)
		usage();

	if (bkgrdflag && cvtlevel > 0) {
		pfatal("CANNOT CONVERT A SNAPSHOT\n");
		exit(EEXIT);
	}

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, catch);
	if (ckclean)
		(void)signal(SIGQUIT, catchquit);
	signal(SIGINFO, infohandler);
	if (bkgrdflag) {
		signal(SIGALRM, alarmhandler);
		itimerval.it_interval.tv_sec = 5;
		itimerval.it_interval.tv_usec = 0;
		itimerval.it_value.tv_sec = 5;
		itimerval.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &itimerval, NULL);
	}
	/*
	 * Push up our allowed memory limit so we can cope
	 * with huge file systems.
	 */
	if (getrlimit(RLIMIT_DATA, &rlimit) == 0) {
		rlimit.rlim_cur = rlimit.rlim_max;
		(void)setrlimit(RLIMIT_DATA, &rlimit);
	}
	while (argc > 0) {
		if ((fsret = checkfilesys(*argv)) == ERESTART)
			continue;
		ret |= fsret;
		argc--;
		argv++;
	}

	if (returntosingle)
		ret = 2;
	exit(ret);
}

static intmax_t
argtoimax(int flag, const char *req, const char *str, int base)
{
	char *cp;
	intmax_t ret;

	ret = strtoimax(str, &cp, base);
	if (cp == str || *cp)
		errx(EEXIT, "-%c flag requires a %s", flag, req);
	return (ret);
}

/*
 * Check the specified file system.
 */
/* ARGSUSED */
static int
checkfilesys(char *filesys)
{
	ufs2_daddr_t n_ffree, n_bfree;
	struct dups *dp;
	struct statfs *mntp;
	intmax_t blks, files;
	size_t size;
	int sbreadfailed, ofsmodified;

	fsutilinit();
	fsckinit();

	cdevname = filesys;
	if (debug && ckclean)
		pwarn("starting\n");
	/*
	 * Make best effort to get the disk name. Check first to see
	 * if it is listed among the mounted file systems. Failing that
	 * check to see if it is listed in /etc/fstab.
	 */
	mntp = getmntpoint(filesys);
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
	sbreadfailed = 0;
	if (openfilesys(filesys) == 0 || readsb() == 0)
		sbreadfailed = 1;
	if (bkgrdcheck) {
		if (sbreadfailed)
			exit(3);	/* Cannot read superblock */
		/* Earlier background failed or journaled */
		if (sblock.fs_flags & (FS_NEEDSFSCK | FS_SUJ))
			exit(4);
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
	if (ckclean && skipclean) {
		/*
		 * If file system is gjournaled, check it here.
		 */
		if (sbreadfailed)
			exit(3);	/* Cannot read superblock */
		if (bkgrdflag == 0 &&
		    (nflag || (fswritefd = open(filesys, O_WRONLY)) < 0)) {
			fswritefd = -1;
			if (preen)
				pfatal("NO WRITE ACCESS");
			printf(" (NO WRITE)");
		}
		if ((sblock.fs_flags & FS_GJOURNAL) != 0) {
			if (sblock.fs_clean == 1) {
				pwarn("FILE SYSTEM CLEAN; SKIPPING CHECKS\n");
				exit(0);
			}
			if ((sblock.fs_flags &
			    (FS_UNCLEAN | FS_NEEDSFSCK)) == 0) {
				bufinit();
				gjournal_check(filesys);
				if (chkdoreload(mntp, pwarn) == 0)
					exit(0);
				exit(4);
			} else {
				pfatal("FULL FSCK NEEDED, CANNOT RUN FAST "
				    "FSCK\n");
			}
		}
		close(fswritefd);
		fswritefd = -1;
	}
	if (bkgrdflag) {
		switch (setup_bkgrdchk(mntp, sbreadfailed, &filesys)) {
		case -1: /* filesystem clean */
			goto clean;
		case 0: /* cannot do background, give up */
			exit(EEXIT);
		case 1: /* doing background check, preen rules apply */
			preen = 1;
			break;
		}
	}

	switch (setup(filesys)) {
	case 0:
		if (preen)
			pfatal("CAN'T CHECK FILE SYSTEM.");
		return (EEXIT);
	case -1:
	clean:
		pwarn("clean, %ld free ", (long)(sblock.fs_cstotal.cs_nffree +
		    sblock.fs_frag * sblock.fs_cstotal.cs_nbfree));
		printf("(%jd frags, %jd blocks, %.1f%% fragmentation)\n",
		    (intmax_t)sblock.fs_cstotal.cs_nffree,
		    (intmax_t)sblock.fs_cstotal.cs_nbfree,
		    sblock.fs_cstotal.cs_nffree * 100.0 / sblock.fs_dsize);
		return (0);
	}
	/*
	 * Determine if we can and should do journal recovery.
	 */
	if ((sblock.fs_flags & FS_SUJ) == FS_SUJ) {
		if ((sblock.fs_flags & FS_NEEDSFSCK) != FS_NEEDSFSCK && skipclean) {
			sujrecovery = 1;
			if (suj_check(filesys) == 0) {
				printf("\n***** FILE SYSTEM MARKED CLEAN *****\n");
				if (chkdoreload(mntp, pwarn) == 0)
					exit(0);
				exit(4);
			}
			sujrecovery = 0;
			printf("** Skipping journal, falling through to full fsck\n\n");
		}
		if (fswritefd != -1) {
			/*
			 * Write the superblock so we don't try to recover the
			 * journal on another pass. If this is the only change
			 * to the filesystem, we do not want it to be called
			 * out as modified.
			 */
			sblock.fs_mtime = time(NULL);
			sbdirty();
			ofsmodified = fsmodified;
			flush(fswritefd, &sblk);
			fsmodified = ofsmodified;
		}
	}
	/*
	 * If the filesystem was run on an old kernel that did not
	 * support check hashes, clear the check-hash flags so that
	 * we do not try to verify them.
	 */
	if ((sblock.fs_flags & FS_METACKHASH) == 0)
		sblock.fs_metackhash = 0;
	/*
	 * If we are running on a kernel that can provide check hashes
	 * that are not yet enabled for the filesystem and we are
	 * running manually without the -y flag, offer to add any
	 * supported check hashes that are not already enabled.
	 */
	ckhashadd = 0;
	if (preen == 0 && yflag == 0 && sblock.fs_magic != FS_UFS1_MAGIC &&
	    fswritefd != -1 && getosreldate() >= P_OSREL_CK_CYLGRP) {
		if ((sblock.fs_metackhash & CK_CYLGRP) == 0 &&
		    reply("ADD CYLINDER GROUP CHECK-HASH PROTECTION") != 0) {
			ckhashadd |= CK_CYLGRP;
			sblock.fs_metackhash |= CK_CYLGRP;
		}
		if ((sblock.fs_metackhash & CK_SUPERBLOCK) == 0 &&
		    getosreldate() >= P_OSREL_CK_SUPERBLOCK &&
		    reply("ADD SUPERBLOCK CHECK-HASH PROTECTION") != 0) {
			ckhashadd |= CK_SUPERBLOCK;
			sblock.fs_metackhash |= CK_SUPERBLOCK;
		}
		if ((sblock.fs_metackhash & CK_INODE) == 0 &&
		    getosreldate() >= P_OSREL_CK_INODE &&
		    reply("ADD INODE CHECK-HASH PROTECTION") != 0) {
			ckhashadd |= CK_INODE;
			sblock.fs_metackhash |= CK_INODE;
		}
#ifdef notyet
		if ((sblock.fs_metackhash & CK_INDIR) == 0 &&
		    getosreldate() >= P_OSREL_CK_INDIR &&
		    reply("ADD INDIRECT BLOCK CHECK-HASH PROTECTION") != 0) {
			ckhashadd |= CK_INDIR;
			sblock.fs_metackhash |= CK_INDIR;
		}
		if ((sblock.fs_metackhash & CK_DIR) == 0 &&
		    getosreldate() >= P_OSREL_CK_DIR &&
		    reply("ADD DIRECTORY CHECK-HASH PROTECTION") != 0) {
			ckhashadd |= CK_DIR;
			sblock.fs_metackhash |= CK_DIR;
		}
#endif /* notyet */
		if (ckhashadd != 0) {
			sblock.fs_flags |= FS_METACKHASH;
			sbdirty();
		}
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
			printf("** Root file system\n");
		printf("** Phase 1 - Check Blocks and Sizes\n");
	}
	clock_gettime(CLOCK_REALTIME_PRECISE, &startprog);
	pass1();
	IOstats("Pass1");

	/*
	 * 1b: locate first references to duplicates, if any
	 */
	if (duplist) {
		if (preen || usedsoftdep)
			pfatal("INTERNAL ERROR: DUPS WITH %s%s%s",
			    preen ? "-p" : "",
			    (preen && usedsoftdep) ? " AND " : "",
			    usedsoftdep ? "SOFTUPDATES" : "");
		printf("** Phase 1b - Rescan For More DUPS\n");
		pass1b();
		IOstats("Pass1b");
	}

	/*
	 * 2: traverse directories from root to mark all connected directories
	 */
	if (preen == 0)
		printf("** Phase 2 - Check Pathnames\n");
	pass2();
	IOstats("Pass2");

	/*
	 * 3: scan inodes looking for disconnected directories
	 */
	if (preen == 0)
		printf("** Phase 3 - Check Connectivity\n");
	pass3();
	IOstats("Pass3");

	/*
	 * 4: scan inodes looking for disconnected files; check reference counts
	 */
	if (preen == 0)
		printf("** Phase 4 - Check Reference Counts\n");
	pass4();
	IOstats("Pass4");

	/*
	 * 5: check and repair resource counts in cylinder groups
	 */
	if (preen == 0)
		printf("** Phase 5 - Check Cyl groups\n");
	snapflush(std_checkblkavail);
	if (cgheader_corrupt) {
		printf("PHASE 5 SKIPPED DUE TO CORRUPT CYLINDER GROUP "
		    "HEADER(S)\n\n");
	} else {
		pass5();
		IOstats("Pass5");
	}

	/*
	 * print out summary statistics
	 */
	n_ffree = sblock.fs_cstotal.cs_nffree;
	n_bfree = sblock.fs_cstotal.cs_nbfree;
	files = maxino - UFS_ROOTINO - sblock.fs_cstotal.cs_nifree - n_files;
	blks = n_blks +
	    sblock.fs_ncg * (cgdmin(&sblock, 0) - cgsblock(&sblock, 0));
	blks += cgsblock(&sblock, 0) - cgbase(&sblock, 0);
	blks += howmany(sblock.fs_cssize, sblock.fs_fsize);
	blks = maxfsblock - (n_ffree + sblock.fs_frag * n_bfree) - blks;
	if (bkgrdflag && (files > 0 || blks > 0)) {
		countdirs = sblock.fs_cstotal.cs_ndir - countdirs;
		pwarn("Reclaimed: %ld directories, %jd files, %jd fragments\n",
		    countdirs, files - countdirs, blks);
	}
	pwarn("%ld files, %jd used, %ju free ",
	    (long)n_files, (intmax_t)n_blks,
	    (uintmax_t)n_ffree + sblock.fs_frag * n_bfree);
	printf("(%ju frags, %ju blocks, %.1f%% fragmentation)\n",
	    (uintmax_t)n_ffree, (uintmax_t)n_bfree,
	    n_ffree * 100.0 / sblock.fs_dsize);
	if (debug) {
		if (files < 0)
			printf("%jd inodes missing\n", -files);
		if (blks < 0)
			printf("%jd blocks missing\n", -blks);
		if (duplist != NULL) {
			printf("The following duplicate blocks remain:");
			for (dp = duplist; dp; dp = dp->next)
				printf(" %jd,", (intmax_t)dp->dup);
			printf("\n");
		}
	}
	duplist = (struct dups *)0;
	muldup = (struct dups *)0;
	inocleanup();
	if (fsmodified) {
		sblock.fs_time = time(NULL);
		sbdirty();
	}
	if (cvtlevel && (sblk.b_flags & B_DIRTY) != 0) {
		/*
		 * Write out the duplicate super blocks
		 */
		if (sbput(fswritefd, &sblock, sblock.fs_ncg) == 0)
			fsmodified = 1;
	}
	if (rerun)
		resolved = 0;

	/*
	 * Check to see if the file system is mounted read-write.
	 */
	if (bkgrdflag == 0 && mntp != NULL && (mntp->f_flags & MNT_RDONLY) == 0)
		resolved = 0;
	ckfini(resolved);

	if (fsmodified && !preen)
		printf("\n***** FILE SYSTEM WAS MODIFIED *****\n");
	if (rerun) {
		if (wantrestart && (restarts++ < 10) &&
		    (preen || reply("RESTART")))
			return (ERESTART);
		printf("\n***** PLEASE RERUN FSCK *****\n");
	}
	if (chkdoreload(mntp, pwarn) != 0) {
		if (!fsmodified)
			return (0);
		if (!preen)
			printf("\n***** REBOOT NOW *****\n");
		sync();
		return (4);
	}
	return (rerun ? ERERUN : 0);
}

/*
 * If we are to do a background check:
 *	Get the mount point information of the file system
 *	If already clean, return -1
 *	Check that kernel supports background fsck
 *	Find or create the snapshot directory
 *	Create the snapshot file
 *	Open snapshot
 *	If anything fails print reason and return 0 which exits
 */
static int
setup_bkgrdchk(struct statfs *mntp, int sbreadfailed, char **filesys)
{
	struct stat snapdir;
	struct group *grp;
	struct iovec *iov;
	char errmsg[255];
	int iovlen;
	size_t size;

	/* Get the mount point information of the file system */
	if (mntp == NULL) {
		pwarn("NOT MOUNTED, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if ((mntp->f_flags & MNT_RDONLY) != 0) {
		pwarn("MOUNTED READ-ONLY, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if ((mntp->f_flags & MNT_SOFTDEP) == 0) {
		pwarn("NOT USING SOFT UPDATES, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if (sbreadfailed) {
		pwarn("SUPERBLOCK READ FAILED, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if ((sblock.fs_flags & FS_NEEDSFSCK) != 0) {
		pwarn("FULL FSCK NEEDED, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if ((sblock.fs_flags & FS_SUJ) != 0) {
		pwarn("JOURNALED FILESYSTEM, CANNOT RUN IN BACKGROUND\n");
		return (0);
	}
	if (skipclean && ckclean &&
	   (sblock.fs_flags & (FS_UNCLEAN|FS_NEEDSFSCK)) == 0) {
		/*
		 * file system is clean;
		 * skip snapshot and report it clean
		 */
		pwarn("FILE SYSTEM CLEAN; SKIPPING CHECKS\n");
		return (-1);
	}
	/* Check that kernel supports background fsck */
	size = MIBSIZE;
	if (sysctlnametomib("vfs.ffs.adjrefcnt", adjrefcnt, &size) < 0||
	    sysctlnametomib("vfs.ffs.adjblkcnt", adjblkcnt, &size) < 0||
	    sysctlnametomib("vfs.ffs.setsize", setsize, &size) < 0 ||
	    sysctlnametomib("vfs.ffs.freefiles", freefiles, &size) < 0||
	    sysctlnametomib("vfs.ffs.freedirs", freedirs, &size) < 0 ||
	    sysctlnametomib("vfs.ffs.freeblks", freeblks, &size) < 0) {
		pwarn("KERNEL LACKS BACKGROUND FSCK SUPPORT\n");
		return (0);
	}
	/*
	 * When kernel lacks runtime bgfsck superblock summary
	 * adjustment functionality, it does not mean we can not
	 * continue, as old kernels will recompute the summary at
	 * mount time. However, it will be an unexpected softupdates
	 * inconsistency if it turns out that the summary is still
	 * incorrect. Set a flag so subsequent operation can know this.
	 */
	bkgrdsumadj = 1;
	if (sysctlnametomib("vfs.ffs.adjndir", adjndir, &size) < 0 ||
	   sysctlnametomib("vfs.ffs.adjnbfree", adjnbfree, &size) < 0 ||
	   sysctlnametomib("vfs.ffs.adjnifree", adjnifree, &size) < 0 ||
	   sysctlnametomib("vfs.ffs.adjnffree", adjnffree, &size) < 0 ||
	   sysctlnametomib("vfs.ffs.adjnumclusters", adjnumclusters,
	   &size) < 0) {
		bkgrdsumadj = 0;
		pwarn("KERNEL LACKS RUNTIME SUPERBLOCK SUMMARY ADJUSTMENT "
		    "SUPPORT\n");
	}
	/* Find or create the snapshot directory */
	snprintf(snapname, sizeof snapname, "%s/.snap",
	    mntp->f_mntonname);
	if (stat(snapname, &snapdir) < 0) {
		if (errno != ENOENT) {
			pwarn("CANNOT FIND SNAPSHOT DIRECTORY %s: %s, CANNOT "
			    "RUN IN BACKGROUND\n", snapname, strerror(errno));
			return (0);
		}
		if ((grp = getgrnam("operator")) == NULL ||
			   mkdir(snapname, 0770) < 0 ||
			   chown(snapname, -1, grp->gr_gid) < 0 ||
			   chmod(snapname, 0770) < 0) {
			pwarn("CANNOT CREATE SNAPSHOT DIRECTORY %s: %s, "
			    "CANNOT RUN IN BACKGROUND\n", snapname,
			    strerror(errno));
			return (0);
		}
	} else if (!S_ISDIR(snapdir.st_mode)) {
		pwarn("%s IS NOT A DIRECTORY, CANNOT RUN IN BACKGROUND\n",
		    snapname);
		return (0);
	}
	/* Create the snapshot file */
	iov = NULL;
	iovlen = 0;
	errmsg[0] = '\0';
	snprintf(snapname, sizeof snapname, "%s/.snap/fsck_snapshot",
	    mntp->f_mntonname);
	build_iovec(&iov, &iovlen, "fstype", "ffs", 4);
	build_iovec(&iov, &iovlen, "from", snapname, (size_t)-1);
	build_iovec(&iov, &iovlen, "fspath", mntp->f_mntonname, (size_t)-1);
	build_iovec(&iov, &iovlen, "errmsg", errmsg, sizeof(errmsg));
	build_iovec(&iov, &iovlen, "update", NULL, 0);
	build_iovec(&iov, &iovlen, "snapshot", NULL, 0);
	/* Create snapshot, removing old snapshot if it exists */
	while (nmount(iov, iovlen, mntp->f_flags) < 0) {
		if (errno == EEXIST && unlink(snapname) == 0)
			continue;
		pwarn("CANNOT CREATE SNAPSHOT %s: %s %s\n", snapname,
		    strerror(errno), errmsg);
		return (0);
	}
	/* Open snapshot */
	if (openfilesys(snapname) == 0) {
		unlink(snapname);
		pwarn("CANNOT OPEN SNAPSHOT %s: %s, CANNOT RUN IN "
		    "BACKGROUND\n", snapname, strerror(errno));
		return (0);
	}
	free(sblock.fs_csp);
	free(sblock.fs_si);
	havesb = 0;
	*filesys = snapname;
	cmd.version = FFS_CMD_VERSION;
	cmd.handle = fsreadfd;
	return (1);
}

static void
usage(void)
{
	(void) fprintf(stderr,
"usage: %s [-BCdEFfnpRrSyZ] [-b block] [-c level] [-m mode] filesystem ...\n",
	    getprogname());
	exit(1);
}

void
infohandler(int sig __unused)
{
	got_siginfo = 1;
}

void
alarmhandler(int sig __unused)
{
	got_sigalarm = 1;
}
