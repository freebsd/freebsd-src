#ifdef NO_GEOM
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
#include <sys/stdint.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/syslog.h>

dev_t
dkmodpart(dev_t dev, int part)
{
	return (makedev(major(dev), (minor(dev) & ~7) | part));
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
	DEV_STRATEGY(bp);
	if (bufwait(bp))
		msg = "I/O error";
	else if (bp->b_resid != 0)
		msg = "disk too small for a label";
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
	DEV_STRATEGY(bp);
	error = bufwait(bp);
	if (error)
		goto done;
	if (bp->b_resid != 0) {
		error = ENOSPC;
		goto done;
	}
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
			DEV_STRATEGY(bp);
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
	DEV_STRATEGY(bp);
	error = bufwait(bp);
#endif
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (error);
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 */
int
bounds_check_with_label(struct bio *bp, struct disklabel *lp, int wlabel)
{
        struct partition *p = lp->d_partitions + dkpart(bp->bio_dev);
        int labelsect = lp->d_partitions[0].p_offset;
        int maxsz = p->p_size,
                sz = (bp->bio_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;

        /* overwriting disk label ? */
        /* XXX should also protect bootstrap in first 8K */
        if (bp->bio_blkno + p->p_offset <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
            bp->bio_blkno + p->p_offset + sz > LABELSECTOR + labelsect &&
#endif
            (bp->bio_cmd == BIO_WRITE) && wlabel == 0) {
                bp->bio_error = EROFS;
                goto bad;
        }

#if     defined(DOSBBSECTOR) && defined(notyet)
        /* overwriting master boot record? */
        if (bp->bio_blkno + p->p_offset <= DOSBBSECTOR &&
            (bp->bio_cmd == BIO_WRITE) && wlabel == 0) {
                bp->bio_error = EROFS;
                goto bad;
        }
#endif

        /* beyond partition? */
        if (bp->bio_blkno < 0 || bp->bio_blkno + sz > maxsz) {
                /* if exactly at end of disk, return an EOF */
                if (bp->bio_blkno == maxsz) {
                        bp->bio_resid = bp->bio_bcount;
                        return(0);
                }
                /* or truncate if part of it fits */
                sz = maxsz - bp->bio_blkno;
                if (sz <= 0) {
                        bp->bio_error = EINVAL;
                        goto bad;
                }
                bp->bio_bcount = sz << DEV_BSHIFT;
        }

        bp->bio_pblkno = bp->bio_blkno + p->p_offset;
        return(1);

bad:
        bp->bio_flags |= BIO_ERROR;
        return(-1);
}

#ifdef __alpha__
void
alpha_fix_srm_checksum(bp)
	struct buf *bp;
{
	u_int64_t *p;
	u_int64_t sum;
	int i;

	p = (u_int64_t *) bp->b_data;
	sum = 0;
	for (i = 0; i < 63; i++)
		sum += p[i];
	p[63] = sum;
}
#endif
#endif
