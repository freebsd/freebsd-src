/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *	@(#)ufs_disksubr.c	8.5 (Berkeley) 1/21/94
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/syslog.h>

/*
 * Seek sort for disks.
 *
 * The buf_queue keep two queues, sorted in ascending block order.  The first
 * queue holds those requests which are positioned after the current block
 * (in the first request); the second, which starts at queue->switch_point,
 * holds requests which came in after their block number was passed.  Thus
 * we implement a one way scan, retracting after reaching the end of the drive
 * to the first request on the second queue, at which time it becomes the
 * first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

void
bioqdisksort(bioq, bp)
	struct bio_queue_head *bioq;
	struct bio *bp;
{
	struct bio *bq;
	struct bio *bn;
	struct bio *be;
	
	be = TAILQ_LAST(&bioq->queue, bio_queue);
	/*
	 * If the queue is empty or we are an
	 * ordered transaction, then it's easy.
	 */
	if ((bq = bioq_first(bioq)) == NULL
	 || (bp->bio_flags & BIO_ORDERED) != 0) {
		bioq_insert_tail(bioq, bp);
		return;
	} else if (bioq->insert_point != NULL) {

		/*
		 * A certain portion of the list is
		 * "locked" to preserve ordering, so
		 * we can only insert after the insert
		 * point.
		 */
		bq = bioq->insert_point;
	} else {

		/*
		 * If we lie before the last removed (currently active)
		 * request, and are not inserting ourselves into the
		 * "locked" portion of the list, then we must add ourselves
		 * to the second request list.
		 */
		if (bp->bio_pblkno < bioq->last_pblkno) {

			bq = bioq->switch_point;
			/*
			 * If we are starting a new secondary list,
			 * then it's easy.
			 */
			if (bq == NULL) {
				bioq->switch_point = bp;
				bioq_insert_tail(bioq, bp);
				return;
			}
			/*
			 * If we lie ahead of the current switch point,
			 * insert us before the switch point and move
			 * the switch point.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				bioq->switch_point = bp;
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				return;
			}
		} else {
			if (bioq->switch_point != NULL)
				be = TAILQ_PREV(bioq->switch_point,
						bio_queue, bio_queue);
			/*
			 * If we lie between last_pblkno and bq,
			 * insert before bq.
			 */
			if (bp->bio_pblkno < bq->bio_pblkno) {
				TAILQ_INSERT_BEFORE(bq, bp, bio_queue);
				return;
			}
		}
	}

	/*
	 * Request is at/after our current position in the list.
	 * Optimize for sequential I/O by seeing if we go at the tail.
	 */
	if (bp->bio_pblkno > be->bio_pblkno) {
		TAILQ_INSERT_AFTER(&bioq->queue, be, bp, bio_queue);
		return;
	}

	/* Otherwise, insertion sort */
	while ((bn = TAILQ_NEXT(bq, bio_queue)) != NULL) {
		
		/*
		 * We want to go after the current request if it is the end
		 * of the first request list, or if the next request is a
		 * larger cylinder than our request.
		 */
		if (bn == bioq->switch_point
		 || bp->bio_pblkno < bn->bio_pblkno)
			break;
		bq = bn;
	}
	TAILQ_INSERT_AFTER(&bioq->queue, bq, bp, bio_queue);
}


/*
 * Attempt to read a disk label from a device using the indicated strategy
 * routine.  The label must be partly set up before this: secpercyl, secsize
 * and anything required in the strategy routine (e.g., dummy bounds for the
 * partition containing the label) must be filled in before calling us.
 * Returns NULL on success and an error string on failure.
 */
char *
readdisklabel(dev, lp)
	dev_t dev;
	register struct disklabel *lp;
{
	register struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR * ((int)lp->d_secsize/DEV_BSIZE);
	bp->b_bcount = lp->d_secsize;
	bp->b_flags &= ~B_INVAL;
	bp->b_iocmd = BIO_READ;
	DEV_STRATEGY(bp, 1);
	if (bufwait(bp))
		msg = "I/O error";
	else for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)((char *)bp->b_data +
	    lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic != DISKMAGIC || dlp->d_magic2 != DISKMAGIC) {
			if (msg == NULL)
				msg = "no disk label";
		} else if (dlp->d_npartitions > MAXPARTITIONS ||
			   dkcksum(dlp) != 0)
			msg = "disk label corrupted";
		else {
			*lp = *dlp;
			msg = NULL;
			break;
		}
	}
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask)
	register struct disklabel *olp, *nlp;
	u_long openmask;
{
	register int i;
	register struct partition *opp, *npp;

	/*
	 * Check it is actually a disklabel we are looking at.
	 */
	if (nlp->d_magic != DISKMAGIC || nlp->d_magic2 != DISKMAGIC ||
	    dkcksum(nlp) != 0)
		return (EINVAL);
	/*
	 * For each partition that we think is open,
	 */
	while ((i = ffs((long)openmask)) != 0) {
		i--;
		/*
	 	 * Check it is not changing....
	 	 */
		openmask &= ~(1 << i);
		if (nlp->d_npartitions <= i)
			return (EBUSY);
		opp = &olp->d_partitions[i];
		npp = &nlp->d_partitions[i];
		if (npp->p_offset != opp->p_offset || npp->p_size < opp->p_size)
			return (EBUSY);
		/*
		 * Copy internally-set partition information
		 * if new label doesn't include it.		XXX
		 * (If we are using it then we had better stay the same type)
		 * This is possibly dubious, as someone else noted (XXX)
		 */
		if (npp->p_fstype == FS_UNUSED && opp->p_fstype != FS_UNUSED) {
			npp->p_fstype = opp->p_fstype;
			npp->p_fsize = opp->p_fsize;
			npp->p_frag = opp->p_frag;
			npp->p_cpg = opp->p_cpg;
		}
	}
 	nlp->d_checksum = 0;
 	nlp->d_checksum = dkcksum(nlp);
	*olp = *nlp;
	return (0);
}

/*
 * Write disk label back to device after modification.
 */
int
writedisklabel(dev, lp)
	dev_t dev;
	register struct disklabel *lp;
{
	struct buf *bp;
	struct disklabel *dlp;
	int error = 0;

	if (lp->d_partitions[RAW_PART].p_offset != 0)
		return (EXDEV);			/* not quite right */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dkmodpart(dev, RAW_PART);
	bp->b_blkno = LABELSECTOR * ((int)lp->d_secsize/DEV_BSIZE);
	bp->b_bcount = lp->d_secsize;
#if 1
	/*
	 * We read the label first to see if it's there,
	 * in which case we will put ours at the same offset into the block..
	 * (I think this is stupid [Julian])
	 * Note that you can't write a label out over a corrupted label!
	 * (also stupid.. how do you write the first one? by raw writes?)
	 */
	bp->b_flags &= ~B_INVAL;
	bp->b_iocmd = BIO_READ;
	DEV_STRATEGY(bp, 1);
	error = bufwait(bp);
	if (error)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)
	      ((char *)bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags &= ~B_DONE;
			bp->b_iocmd = BIO_WRITE;
#ifdef __alpha__
			alpha_fix_srm_checksum(bp);
#endif
			DEV_STRATEGY(bp, 1);
			error = bufwait(bp);
			goto done;
		}
	}
	error = ESRCH;
done:
#else
	bzero(bp->b_data, lp->d_secsize);
	dlp = (struct disklabel *)bp->b_data;
	*dlp = *lp;
	bp->b_flags &= ~B_INVAL;
	bp->b_iocmd = BIO_WRITE;
	DEV_STRATEGY(bp, 1);
	error = bufwait(bp);
#endif
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (error);
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf.
 * The message should be completed with at least a newline. There is no
 * trailing space.
 */
void
diskerr(bp, what, blkdone, lp)
	struct bio *bp;
	char *what;
	int blkdone;
	register struct disklabel *lp;
{
	int unit = dkunit(bp->bio_dev);
	int slice = dkslice(bp->bio_dev);
	int part = dkpart(bp->bio_dev);
	char partname[2];
	char *sname;
	daddr_t sn;

	sname = dsname(bp->bio_dev, unit, slice, part, partname);
	printf("%s%s: %s %sing fsbn ", sname, partname, what,
	      bp->bio_cmd == BIO_READ ? "read" : "writ");
	sn = bp->bio_blkno;
	if (bp->bio_bcount <= DEV_BSIZE)
		printf("%ld", (long)sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			printf("%ld of ", (long)sn);
		}
		printf("%ld-%ld", (long)bp->bio_blkno,
		    (long)(bp->bio_blkno + (bp->bio_bcount - 1) / DEV_BSIZE));
	}
	if (lp && (blkdone >= 0 || bp->bio_bcount <= lp->d_secsize)) {
#ifdef tahoe
		sn *= DEV_BSIZE / lp->d_secsize;		/* XXX */
#endif
		sn += lp->d_partitions[part].p_offset;
		/*
		 * XXX should add slice offset and not print the slice,
		 * but we don't know the slice pointer.
		 * XXX should print bp->b_pblkno so that this will work
		 * independent of slices, labels and bad sector remapping,
		 * but some drivers don't set bp->b_pblkno.
		 */
		printf(" (%s bn %ld; cn %ld", sname, (long)sn,
		    (long)(sn / lp->d_secpercyl));
		sn %= (long)lp->d_secpercyl;
		printf(" tn %ld sn %ld)", (long)(sn / lp->d_nsectors),
		    (long)(sn % lp->d_nsectors));
	}
}
