/*-
 * Copyright (c) 1994 Bruce D. Evans.
 * All rights reserved.
 *
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Copyright (c) 1982, 1986, 1988 Regents of the University of California.
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
 *	from: @(#)wd.c	7.2 (Berkeley) 5/9/91
 *	from: wd.c,v 1.55 1994/10/22 01:57:12 phk Exp $
 *	from: @(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 *	from: ufs_disksubr.c,v 1.8 1994/06/07 01:21:39 phk Exp $
 *	$Id$
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/dkbad.h>
#include <sys/malloc.h>

/*
 * Internalize the bad sector table.
 * TODO:
 *	o Fix types.
 *	  Type long should be daddr_t since we compare with blkno's.
 *	  Sentinel -1 should be ((daddr_t)-1).
 *	o Can remove explicit test for sentinel if it is a positive
 *	  (unsigned or not) value larger than all possible blkno's.
 *	o Check that the table is sorted.
 *	o Use faster searches.
 *	o Use the internal table in wddump().
 *	o Don't duplicate so much code.
 *	o Do all bad block handing in a driver-independent file.
 *	o Remove limit of 126 spare sectors.
 */
struct dkbad_intern *
internbad144(btp, lp)
	struct dkbad *btp;
	struct disklabel *lp;
{
	struct dkbad_intern *bip;
	int i;

	bip = malloc(sizeof *bip, M_DEVBUF, M_WAITOK);
	/*
	 * Spare sectors are allocated beginning with the last sector of
	 * the second last track of the disk (the last track is used for
	 * the bad sector list).
	 */
	bip->bi_maxspare = lp->d_secperunit - lp->d_nsectors - 1;
	bip->bi_nbad = DKBAD_MAXBAD;
	i = 0;
	for (; i < DKBAD_MAXBAD && btp->bt_bad[i].bt_cyl != DKBAD_NOCYL; i++)
		bip->bi_bad[i] = btp->bt_bad[i].bt_cyl * lp->d_secpercyl
				 + (btp->bt_bad[i].bt_trksec >> 8)
				   * lp->d_nsectors
				 + (btp->bt_bad[i].bt_trksec & 0x00ff);
	bip->bi_bad[i] = -1;
	return (bip);
}

char *
readbad144(dev, strat, lp, bdp)
	dev_t	dev;
	d_strategy_t *strat;
	struct disklabel *lp;
	struct dkbad *bdp;
{
	struct buf *bp;
	struct dkbad *db;
	int	i;
	char	*msg;

	bp = geteblk((int)lp->d_secsize);
	i = 0;
	do {
		/* Read a bad sector table. */
		bp->b_dev = dev;
		bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
		if (lp->d_secsize > DEV_BSIZE)
			bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
		else
			bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
		bp->b_bcount = lp->d_secsize;
		bp->b_flags = B_BUSY | B_READ;
		(*strat)(bp);

		/* If successful, validate, otherwise try another. */
		if (biowait(bp) == 0) {
			db = (struct dkbad *)(bp->b_un.b_addr);
			if (db->bt_mbz == 0 && db->bt_flag == DKBAD_MAGIC) {
				msg = NULL;
				*bdp = *db;
				break;
			}
			msg = "bad sector table corrupted";
		} else
			msg = "bad sector table I/O error";
	} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
		 i < lp->d_nsectors);
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}

daddr_t
transbad144(bip, blkno)
	struct dkbad_intern *bip;
	daddr_t	blkno;
{
	int	i;

	/*
	 * List is sorted, so the search can terminate when it is past our
	 * sector.
	 */
	for (i = 0; bip->bi_bad[i] != -1 && bip->bi_bad[i] <= blkno; i++)
		if (bip->bi_bad[i] == blkno)
			/*
			 * Spare sectors are allocated in decreasing order.
			 */
			return (bip->bi_maxspare - i);
	return (blkno);
}
