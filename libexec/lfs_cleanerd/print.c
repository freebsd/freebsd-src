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
 *
 *	$Id$
 */

#ifndef lint
static char sccsid[] = "@(#)print.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/lfs/lfs.h>

#include <stdlib.h>
#include <stdio.h>
#include "clean.h"

/*
 * Print out a summary block; return number of blocks in segment; 0
 * for empty segment or corrupt segment.
 * Returns a pointer to the array of inode addresses.
 */
int
dump_summary(lfsp, sp, flags, iaddrp)
	struct lfs *lfsp;
	SEGSUM *sp;
	u_long flags;
	daddr_t **iaddrp;
{
	int i, j, numblocks;
	daddr_t *dp;

	FINFO *fp;
	int ck;

	if (sp->ss_sumsum != (ck = cksum(&sp->ss_datasum,
	    LFS_SUMMARY_SIZE - sizeof(sp->ss_sumsum))))
		return(-1);

	if (flags & DUMP_SUM_HEADER) {
		(void)printf("    %s0x%X\t%s%d\t%s%d\n    %s0x%X\t%s0x%X",
			"next     ", sp->ss_next,
			"nfinfo   ", sp->ss_nfinfo,
			"ninos    ", sp->ss_ninos,
			"sumsum   ", sp->ss_sumsum,
			"datasum  ", sp->ss_datasum );
		(void)printf("\tcreate   %s", ctime((time_t *)&sp->ss_create));
	}

	numblocks = (sp->ss_ninos + INOPB(lfsp) - 1) / INOPB(lfsp);

	/* Dump out inode disk addresses */
	if (flags & DUMP_INODE_ADDRS)
		printf("    Inode addresses:");

	dp = (daddr_t *)((caddr_t)sp + LFS_SUMMARY_SIZE);
	for (--dp, i = 0; i < sp->ss_ninos; --dp)
		if (flags & DUMP_INODE_ADDRS) {
			(void)printf("\t0x%lx", *dp);
			if (++i % 7 == 0)
				(void)printf("\n");
		} else
			++i;
	if (iaddrp)
		*iaddrp = ++dp;
	if (flags & DUMP_INODE_ADDRS)
		printf("\n");

	for (fp = (FINFO *)(sp + 1), i = 0; i < sp->ss_nfinfo; ++i) {
		numblocks += fp->fi_nblocks;
		if (flags & DUMP_FINFOS) {
			(void)printf("    %s%d version %d nblocks %d\n",
			    "FINFO for inode: ", fp->fi_ino,
			    fp->fi_version, fp->fi_nblocks);
			dp = &(fp->fi_blocks[0]);
			for (j = 0; j < fp->fi_nblocks; j++, dp++) {
				(void)printf("\t%d", *dp);
				if ((j % 8) == 7)
					(void)printf("\n");
			}
			if ((j % 8) != 0)
				(void)printf("\n");
			fp = (FINFO *)dp;
		} else {
			fp = (FINFO *)(&fp->fi_blocks[fp->fi_nblocks]);
		}
	}
	return (numblocks);
}

#ifdef VERBOSE
void
dump_cleaner_info(ipage)
	void *ipage;
{
	CLEANERINFO *cip;

	cip = (CLEANERINFO *)ipage;
	(void)printf("segments clean\t%d\tsegments dirty\t%d\n\n",
	    cip->clean, cip->dirty);
}

void
dump_super(lfsp)
	struct lfs *lfsp;
{
	int i;

	(void)printf("%s0x%X\t%s0x%X\t%s%d\t%s%d\n",
		"magic    ", lfsp->lfs_magic,
		"version  ", lfsp->lfs_version,
		"size     ", lfsp->lfs_size,
		"ssize    ", lfsp->lfs_ssize);
	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"dsize    ", lfsp->lfs_dsize,
		"bsize    ", lfsp->lfs_bsize,
		"fsize    ", lfsp->lfs_fsize,
		"frag     ", lfsp->lfs_frag);

	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"minfree  ", lfsp->lfs_minfree,
		"inopb    ", lfsp->lfs_inopb,
		"ifpb     ", lfsp->lfs_ifpb,
		"nindir   ", lfsp->lfs_nindir);

	(void)printf("%s%d\t\t%s%d\t%s%d\t%s%d\n",
		"nseg     ", lfsp->lfs_nseg,
		"nspf     ", lfsp->lfs_nspf,
		"cleansz  ", lfsp->lfs_cleansz,
		"segtabsz ", lfsp->lfs_segtabsz);

	(void)printf("%s0x%X\t%s%d\t%s0x%X\t%s%d\n",
		"segmask  ", lfsp->lfs_segmask,
		"segshift ", lfsp->lfs_segshift,
		"bmask    ", lfsp->lfs_bmask,
		"bshift   ", lfsp->lfs_bshift);

	(void)printf("%s0x%X\t\t%s%d\t%s0x%X\t%s%d\n",
		"ffmask   ", lfsp->lfs_ffmask,
		"ffshift  ", lfsp->lfs_ffshift,
		"fbmask   ", lfsp->lfs_fbmask,
		"fbshift  ", lfsp->lfs_fbshift);

	(void)printf("%s%d\t\t%s0x%X\t%s0x%qx\n",
		"fsbtodb  ", lfsp->lfs_fsbtodb,
		"cksum    ", lfsp->lfs_cksum,
		"maxfilesize  ", lfsp->lfs_maxfilesize);

	(void)printf("Superblock disk addresses:\t");
	for (i = 0; i < LFS_MAXNUMSB; i++) {
		(void)printf(" 0x%X", lfsp->lfs_sboffs[i]);
		if ( i == (LFS_MAXNUMSB >> 1))
			(void)printf("\n\t\t\t\t");
	}
	(void)printf("\n");

	(void)printf("Checkpoint Info\n");
	(void)printf("%s%d\t%s0x%X\t%s%d\n",
		"free     ", lfsp->lfs_free,
		"idaddr   ", lfsp->lfs_idaddr,
		"ifile    ", lfsp->lfs_ifile);
	(void)printf("%s%d\t%s%d\t%s%d\n",
		"bfree    ", lfsp->lfs_bfree,
		"avail    ", lfsp->lfs_avail,
		"uinodes  ", lfsp->lfs_uinodes);
	(void)printf("%s%d\t%s0x%X\t%s0x%X\n%s0x%X\t%s0x%X\t",
		"nfiles   ", lfsp->lfs_nfiles,
		"lastseg  ", lfsp->lfs_lastseg,
		"nextseg  ", lfsp->lfs_nextseg,
		"curseg   ", lfsp->lfs_curseg,
		"offset   ", lfsp->lfs_offset);
	(void)printf("tstamp   %s", ctime((time_t *)&lfsp->lfs_tstamp));
	(void)printf("\nIn-Memory Information\n");
	(void)printf("%s%d\t%s0x%X\t%s%d\t%s%d\t%s%d\n",
		"seglock  ", lfsp->lfs_seglock,
		"iocount  ", lfsp->lfs_iocount,
		"writer   ", lfsp->lfs_writer,
		"dirops   ", lfsp->lfs_dirops,
		"doifile  ", lfsp->lfs_doifile );
	(void)printf("%s%d\t%s%d\t%s0x%X\t%s%d\n",
		"nactive  ", lfsp->lfs_nactive,
		"fmod     ", lfsp->lfs_fmod,
		"clean    ", lfsp->lfs_clean,
		"ronly    ", lfsp->lfs_ronly);
}
#endif /* VERBOSE */
