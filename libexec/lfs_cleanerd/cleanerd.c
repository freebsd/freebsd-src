/*-
 * Copyright (c) 1992, 1993
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
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)cleanerd.c	8.5 (Berkeley) 6/10/95";
#endif /* not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "clean.h"
char *special = "cleanerd";
int do_small = 0;
int do_mmap = 0;
int stat_report = 0;
struct cleaner_stats {
	double	util_tot;
	double	util_sos;
	int	blocks_read;
	int	blocks_written;
	int	segs_cleaned;
	int	segs_empty;
	int	segs_error;
} cleaner_stats;

struct seglist { 
	int sl_id;	/* segment number */
	int sl_cost; 	/* cleaning cost */
	char sl_bytes;	/* bytes in segment */
};

struct tossstruct {
	struct lfs *lfs;
	int seg;
};

#define	CLEAN_BYTES	0x1

/* function prototypes for system calls; not sure where they should go */
int	 lfs_segwait __P((fsid_t *, struct timeval *));
int	 lfs_segclean __P((fsid_t *, u_long));
int	 lfs_bmapv __P((fsid_t *, BLOCK_INFO *, int));
int	 lfs_markv __P((fsid_t *, BLOCK_INFO *, int));

/* function prototypes */
int	 bi_tossold __P((const void *, const void *, const void *));
int	 choose_segments __P((FS_INFO *, struct seglist *, 
	     int (*)(FS_INFO *, SEGUSE *)));
void	 clean_fs __P((FS_INFO	*, int (*)(FS_INFO *, SEGUSE *), int, long));
int	 clean_loop __P((FS_INFO *, int, long));
int	 clean_segment __P((FS_INFO *, int));
int	 cost_benefit __P((FS_INFO *, SEGUSE *));
int	 cost_compare __P((const void *, const void *));
void	 sig_report __P((int));

/*
 * Cleaning Cost Functions:
 *
 * These return the cost of cleaning a segment.  The higher the cost value
 * the better it is to clean the segment, so empty segments have the highest
 * cost.  (It is probably better to think of this as a priority value
 * instead).
 *
 * This is the cost-benefit policy simulated and described in Rosenblum's
 * 1991 SOSP paper.
 */

int
cost_benefit(fsp, su)
	FS_INFO *fsp;		/* file system information */
	SEGUSE *su;
{
	struct lfs *lfsp;
	struct timeval t;
	int age;
	int live;

	gettimeofday(&t, NULL);

	live = su->su_nbytes;	
	age = t.tv_sec < su->su_lastmod ? 0 : t.tv_sec - su->su_lastmod;
	
	lfsp = &fsp->fi_lfs;
	if (live == 0)
		return (t.tv_sec * lblkno(lfsp, seg_size(lfsp)));
	else {
		/* 
		 * from lfsSegUsage.c (Mendel's code).
		 * priority calculation is done using INTEGER arithmetic.
		 * sizes are in BLOCKS (that is why we use lblkno below).
		 * age is in seconds.
		 *
		 * priority = ((seg_size - live) * age) / (seg_size + live)
		 */
#ifdef VERBOSE
		if (live < 0 || live > seg_size(lfsp)) {
			err(0, "Bad segusage count: %d", live);
			live = 0;
		}
#endif
		return (lblkno(lfsp, seg_size(lfsp) - live) * age)
			/ lblkno(lfsp, seg_size(lfsp) + live);
	}
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FS_INFO	*fsp;
	struct statfs *lstatfsp;	/* file system stats */
	struct timeval timeout;		/* sleep timeout */
	fsid_t fsid;
	long clean_opts;		/* cleaning options */
	int i, nodaemon, segs_per_clean;
	int opt, cmd_err;
	char *fs_name;			/* name of filesystem to clean */
	extern int optind;

	cmd_err = nodaemon = 0;
	clean_opts = 0;
	segs_per_clean = 1;
	while ((opt = getopt(argc, argv, "bdmn:r:s")) != EOF) {
		switch (opt) {
			case 'b':	/*
					 * Use live bytes to determine
					 * how many segs to clean.
					 */
				clean_opts |= CLEAN_BYTES;
				break;
			case 'd':	/* Debug mode. */
				nodaemon = 1;
				break;
			case 'm':	/* Use mmap instead of read/write */
				do_mmap = 1;
				break;
			case 'n':	/* How many segs to clean at once */
				segs_per_clean = atoi(optarg);
				break;
			case 'r':	/* Report every stat_report segments */
				stat_report = atoi(optarg);
				break;
			case 's':	/* small writes */
				do_small = 1;
				break;
			default:
				++cmd_err;
		}
	}
	argc -= optind;
	argv += optind;
	if (cmd_err || (argc != 1))
		err(1, "usage: lfs_cleanerd [-smd] fs_name");

	fs_name = argv[0];

	signal(SIGINT, sig_report);
	signal(SIGUSR1, sig_report);
	signal(SIGUSR2, sig_report);
	if (fs_getmntinfo(&lstatfsp, fs_name, "lfs") == 0) {
		/* didn't find the filesystem */
		err(1, "lfs_cleanerd: filesystem %s isn't an LFS!", fs_name);
	}

	if (!nodaemon)	/* should we become a daemon, chdir to / & close fd's */
		if (daemon(0, 0) == -1)
			err(1, "lfs_cleanerd: couldn't become a daemon!");

	timeout.tv_sec = 5*60; /* five minutes */
	timeout.tv_usec = 0;
	fsid.val[0] = 0;
	fsid.val[1] = 0;

	for (fsp = get_fs_info(lstatfsp, do_mmap); ;
	    reread_fs_info(fsp, do_mmap)) {
		/*
		 * clean the filesystem, and, if it needed cleaning
		 * (i.e. it returned nonzero) try it again
		 * to make sure that some nasty process hasn't just
		 * filled the disk system up.
		 */
		if (clean_loop(fsp, segs_per_clean, clean_opts))
			continue;

#ifdef VERBOSE
		(void)printf("Cleaner going to sleep.\n");
#endif
		if (lfs_segwait(&fsid, &timeout) < 0)
			err(0, "lfs_segwait: returned error\n");	
#ifdef VERBOSE
		(void)printf("Cleaner waking up.\n");
#endif
	}
}

/* return the number of segments cleaned */
int
clean_loop(fsp, nsegs, options)
	FS_INFO	*fsp;	/* file system information */
	int nsegs;
	long options;
{
	double loadavg[MAXLOADS];
	time_t	now;
	u_long max_free_segs;
	u_long db_per_seg;

        /*
	 * Compute the maximum possible number of free segments, given the
	 * number of free blocks.
	 */
	db_per_seg = fsbtodb(&fsp->fi_lfs, fsp->fi_lfs.lfs_ssize);
	max_free_segs = fsp->fi_lfs.lfs_bfree / db_per_seg;
	
	/* 
	 * We will clean if there are not enough free blocks or total clean
	 * space is less than BUSY_LIM % of possible clean space.
	 */
	now = time((time_t *)NULL);
#ifdef VERBOSE
	printf("db_er_seg = %d max_free_segs = %d, bfree = %d avail = %d ",
	    db_per_seg, max_free_segs, fsp->fi_lfs.lfs_bfree,
	    fsp->fi_lfs.lfs_avail);
	printf("clean = %d\n", fsp->fi_cip->clean);
#endif
	if ((fsp->fi_lfs.lfs_bfree - fsp->fi_lfs.lfs_avail > db_per_seg &&
	    fsp->fi_lfs.lfs_avail < db_per_seg) ||
	    (fsp->fi_cip->clean < max_free_segs &&
	    (fsp->fi_cip->clean <= MIN_SEGS(&fsp->fi_lfs) ||
	    fsp->fi_cip->clean < max_free_segs * BUSY_LIM))) {
		printf("Cleaner Running  at %s (%d of %d segments available)\n",
		    ctime(&now), fsp->fi_cip->clean, max_free_segs);
		clean_fs(fsp, cost_benefit, nsegs, options);
		return (1);
	} else {
	        /* 
		 * We will also clean if the system is reasonably idle and
		 * the total clean space is less then IDLE_LIM % of possible
		 * clean space.
		 */
		if (getloadavg(loadavg, MAXLOADS) == -1) {
			perror("getloadavg: failed\n");
			return (-1);
		}
		if (loadavg[ONE_MIN] == 0.0 && loadavg[FIVE_MIN] &&
		    fsp->fi_cip->clean < max_free_segs * IDLE_LIM) {
		        clean_fs(fsp, cost_benefit, nsegs, options);
			printf("Cleaner Running  at %s (system idle)\n",
			    ctime(&now));
			return (1);
		}
	} 
	printf("Cleaner Not Running at %s\n", ctime(&now));
	return (0);
}


void
clean_fs(fsp, cost_func, nsegs, options)
	FS_INFO	*fsp;	/* file system information */
	int (*cost_func) __P((FS_INFO *, SEGUSE *));
	int nsegs;
	long options;
{
	struct seglist *segs, *sp;
	int to_clean, cleaned_bytes;
	int i;

	if ((segs =
	    malloc(fsp->fi_lfs.lfs_nseg * sizeof(struct seglist))) == NULL) {
		err(0, "malloc failed");
		return;
	}
	i = choose_segments(fsp, segs, cost_func);
#ifdef VERBOSE
	printf("clean_fs: found %d segments to clean in file system %s\n",
		i, fsp->fi_statfsp->f_mntonname);
	fflush(stdout);
#endif
	if (i) {
		/* Check which cleaning algorithm to use. */
		if (options & CLEAN_BYTES) {
			cleaned_bytes = 0;
			to_clean = nsegs <<
			    (fsp->fi_lfs.lfs_segshift + fsp->fi_lfs.lfs_bshift);
			for (sp = segs; i && cleaned_bytes < to_clean;
			    i--, ++sp) {
				if (clean_segment(fsp, sp->sl_id) < 0)
					perror("clean_segment failed");
				else if (lfs_segclean(&fsp->fi_statfsp->f_fsid,
				    sp->sl_id) < 0)
					perror("lfs_segclean failed");
				printf("Cleaned segment %d (%d bytes)\n",
				    sp->sl_id, sp->sl_bytes);
				cleaned_bytes += sp->sl_bytes;
			}
		} else
			for (i = MIN(i, nsegs), sp = segs; i-- ; ++sp) {
				if (clean_segment(fsp, sp->sl_id) < 0)
					perror("clean_segment failed");
				else if (lfs_segclean(&fsp->fi_statfsp->f_fsid,
				    sp->sl_id) < 0)
					perror("lfs_segclean failed");
				printf("Completed cleaning segment %d\n", sp->sl_id);
			}
	}
	free(segs);
}

/*
 * Segment with the highest priority get sorted to the beginning of the
 * list.  This sort assumes that empty segments always have a higher
 * cost/benefit than any utilized segment.
 */
int
cost_compare(a, b)
	const void *a;
	const void *b;
{
	return (((struct seglist *)b)->sl_cost -
	    ((struct seglist *)a)->sl_cost);
}


/*
 * Returns the number of segments to be cleaned with the elements of seglist
 * filled in.
 */
int
choose_segments(fsp, seglist, cost_func)
	FS_INFO *fsp;
	struct seglist *seglist;
	int (*cost_func) __P((FS_INFO *, SEGUSE *));
{
	struct lfs *lfsp;
	struct seglist *sp;
	SEGUSE *sup;
	int i, nsegs;

	lfsp = &fsp->fi_lfs;

#ifdef VERBOSE
	(void)printf("Entering choose_segments\n");
#endif
	dump_super(lfsp);
	dump_cleaner_info(fsp->fi_cip);

	for (sp = seglist, i = 0; i < lfsp->lfs_nseg; ++i) {
		sup = SEGUSE_ENTRY(lfsp, fsp->fi_segusep, i);
		 PRINT_SEGUSE(sup, i);
		if (!(sup->su_flags & SEGUSE_DIRTY) ||
		    sup->su_flags & SEGUSE_ACTIVE)
			continue;
#ifdef VERBOSE
		(void)printf("\tchoosing segment %d\n", i);
#endif
		sp->sl_cost = (*cost_func)(fsp, sup);
		sp->sl_id = i;
		sp->sl_bytes = sup->su_nbytes;
		++sp;
	}
	nsegs = sp - seglist;
	qsort(seglist, nsegs, sizeof(struct seglist), cost_compare);
#ifdef VERBOSE
	(void)printf("Returning %d segments\n", nsegs);
#endif
	return (nsegs);
}


int
clean_segment(fsp, id)
	FS_INFO *fsp;	/* file system information */
	int id;		/* segment number */
{
	BLOCK_INFO *block_array, *bp;
	SEGUSE *sp;
	struct lfs *lfsp;
	struct tossstruct t;
	caddr_t seg_buf;
	double util;
	int num_blocks, maxblocks, clean_blocks;

	lfsp = &fsp->fi_lfs;
	sp = SEGUSE_ENTRY(lfsp, fsp->fi_segusep, id);

#ifdef VERBOSE
	(void)printf("cleaning segment %d: contains %lu bytes\n", id,
	    sp->su_nbytes);
	fflush(stdout);
#endif
	/* XXX could add debugging to verify that segment is really empty */
	if (sp->su_nbytes == sp->su_nsums * LFS_SUMMARY_SIZE) {
		++cleaner_stats.segs_empty;
		return (0);
	}

	/* map the segment into a buffer */
	if (mmap_segment(fsp, id, &seg_buf, do_mmap) < 0) {
		err(0, "mmap_segment failed");
		++cleaner_stats.segs_error;
		return (-1);
	}
	/* get a list of blocks that are contained by the segment */
	if (lfs_segmapv(fsp, id, seg_buf, &block_array, &num_blocks) < 0) {
		err(0, "clean_segment: lfs_segmapv failed");
		++cleaner_stats.segs_error;
		return (-1);
	}
	cleaner_stats.blocks_read += fsp->fi_lfs.lfs_ssize;

#ifdef VERBOSE
	(void)printf("lfs_segmapv returned %d blocks\n", num_blocks);
	fflush(stdout);
#endif

	/* get the current disk address of blocks contained by the segment */
	if (lfs_bmapv(&fsp->fi_statfsp->f_fsid, block_array, num_blocks) < 0) {
		perror("clean_segment: lfs_bmapv failed\n");
		++cleaner_stats.segs_error;
		return -1;
	}

	/* Now toss any blocks not in the current segment */
	t.lfs = lfsp;
	t.seg = id;
	toss(block_array, &num_blocks, sizeof(BLOCK_INFO), bi_tossold, &t);

	/* Check if last element should be tossed */
	if (num_blocks && bi_tossold(&t, block_array + num_blocks - 1, NULL))
		--num_blocks;

#ifdef VERBOSE
	{
		BLOCK_INFO *_bip;
		u_long *lp;
		int i;

		(void)printf("after bmapv still have %d blocks\n", num_blocks);
		fflush(stdout);
		if (num_blocks)
			printf("BLOCK INFOS\n");
		for (_bip = block_array, i=0; i < num_blocks; ++_bip, ++i) {
			PRINT_BINFO(_bip);
			lp = (u_long *)_bip->bi_bp;
		}
	}

#endif
	++cleaner_stats.segs_cleaned;
	cleaner_stats.blocks_written += num_blocks;
	util = ((double)num_blocks / fsp->fi_lfs.lfs_ssize);
	cleaner_stats.util_tot += util;
	cleaner_stats.util_sos += util * util;

	if (do_small)
		maxblocks = MAXPHYS / fsp->fi_lfs.lfs_bsize - 1;
	else
		maxblocks = num_blocks;

	for (bp = block_array; num_blocks > 0; bp += clean_blocks) {
		clean_blocks = maxblocks < num_blocks ? maxblocks : num_blocks;
		if (lfs_markv(&fsp->fi_statfsp->f_fsid,
		    bp, clean_blocks) < 0) {
			err(0, "clean_segment: lfs_markv failed");
			++cleaner_stats.segs_error;
			return (-1);
		}
		num_blocks -= clean_blocks;
	}
		
	free(block_array);
	munmap_segment(fsp, seg_buf, do_mmap);
	if (stat_report && cleaner_stats.segs_cleaned % stat_report == 0)
		sig_report(SIGUSR1);
	return (0);
}


int
bi_tossold(client, a, b)
	const void *client;
	const void *a;
	const void *b;
{
	const struct tossstruct *t;

	t = (struct tossstruct *)client;

	return (((BLOCK_INFO *)a)->bi_daddr == LFS_UNUSED_DADDR ||
	    datosn(t->lfs, ((BLOCK_INFO *)a)->bi_daddr) != t->seg);
}

void
sig_report(sig)
	int sig;
{
	double avg;

	printf("lfs_cleanerd:\t%s%d\n\t\t%s%d\n\t\t%s%d\n\t\t%s%d\n\t\t%s%d\n",
		"blocks_read    ", cleaner_stats.blocks_read,
		"blocks_written ", cleaner_stats.blocks_written,
		"segs_cleaned   ", cleaner_stats.segs_cleaned,
		"segs_empty     ", cleaner_stats.segs_empty,
		"seg_error      ", cleaner_stats.segs_error);
	printf("\t\t%s%5.2f\n\t\t%s%5.2f\n",
		"util_tot       ", cleaner_stats.util_tot,
		"util_sos       ", cleaner_stats.util_sos);
	printf("\t\tavg util: %4.2f std dev: %9.6f\n",
		avg = cleaner_stats.util_tot / cleaner_stats.segs_cleaned,
		cleaner_stats.util_sos / cleaner_stats.segs_cleaned - avg * avg);
		
		
	if (sig == SIGUSR2) {
		cleaner_stats.blocks_read = 0;
		cleaner_stats.blocks_written = 0;
		cleaner_stats.segs_cleaned = 0;
		cleaner_stats.segs_empty = 0;
		cleaner_stats.segs_error = 0;
		cleaner_stats.util_tot = 0.0;
		cleaner_stats.util_sos = 0.0;
	}
	if (sig == SIGINT)
		exit(0);
}
