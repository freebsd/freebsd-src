#define	PRE_DISKSLICE_COMPAT
#ifndef PRE_DISKSLICE_COMPAT
#define	correct_readdisklabel	readdisklabel
#define	correct_writedisklabel	writedisklabel
#endif

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
 * $Id: ufs_disksubr.c,v 1.10 1995/02/22 22:46:48 bde Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/dkbad.h>
#include <sys/syslog.h>

/*
 * Seek sort for disks.  We depend on the driver which calls us using b_resid
 * as the current cylinder number.
 *
 * The argument ap structure holds a b_actf activity chain pointer on which we
 * keep two queues, sorted in ascending cylinder order.  The first queue holds
 * those requests which are positioned after the current cylinder (in the first
 * request); the second holds requests which came in after their cylinder number
 * was passed.  Thus we implement a one way scan, retracting after reaching the
 * end of the drive to the first request on the second queue, at which time it
 * becomes the first queue.
 *
 * A one-way scan is natural because of the way UNIX read-ahead blocks are
 * allocated.
 */

/*
 * For portability with historic industry practice, the
 * cylinder number has to be maintained in the `b_resid'
 * field.
 */
#define	b_cylinder	b_resid

void
disksort(ap, bp)
	register struct buf *ap, *bp;
{
	register struct buf *bq;

	/* If the queue is empty, then it's easy. */
	if (ap->b_actf == NULL) {
		bp->b_actf = NULL;
		ap->b_actf = bp;
		return;
	}

	/*
	 * If we lie after the first (currently active) request, then we
	 * must locate the second request list and add ourselves to it.
	 */
	bq = ap->b_actf;
	if (bp->b_cylinder < bq->b_cylinder) {
		while (bq->b_actf) {
			/*
			 * Check for an ``inversion'' in the normally ascending
			 * cylinder numbers, indicating the start of the second
			 * request list.
			 */
			if (bq->b_actf->b_cylinder < bq->b_cylinder) {
				/*
				 * Search the second request list for the first
				 * request at a larger cylinder number.  We go
				 * before that; if there is no such request, we
				 * go at end.
				 */
				do {
					if (bp->b_cylinder <
					    bq->b_actf->b_cylinder)
						goto insert;
					if (bp->b_cylinder ==
					    bq->b_actf->b_cylinder &&
					    bp->b_blkno < bq->b_actf->b_blkno)
						goto insert;
					bq = bq->b_actf;
				} while (bq->b_actf);
				goto insert;		/* after last */
			}
			bq = bq->b_actf;
		}
		/*
		 * No inversions... we will go after the last, and
		 * be the first request in the second request list.
		 */
		goto insert;
	}
	/*
	 * Request is at/after the current request...
	 * sort in the first request list.
	 */
	while (bq->b_actf) {
		/*
		 * We want to go after the current request if there is an
		 * inversion after it (i.e. it is the end of the first
		 * request list), or if the next request is a larger cylinder
		 * than our request.
		 */
		if (bq->b_actf->b_cylinder < bq->b_cylinder ||
		    bp->b_cylinder < bq->b_actf->b_cylinder ||
		    (bp->b_cylinder == bq->b_actf->b_cylinder &&
		    bp->b_blkno < bq->b_actf->b_blkno))
			goto insert;
		bq = bq->b_actf;
	}
	/*
	 * Neither a second list nor a larger request... we go at the end of
	 * the first list, which is the same as the end of the whole schebang.
	 */
insert:	bp->b_actf = bq->b_actf;
	bq->b_actf = bp;
}

/*
 * Attempt to read a disk label from a device using the indicated stategy
 * routine.  The label must be partly set up before this: secpercyl and
 * anything required in the strategy routine (e.g., sector size) must be
 * filled in before calling us.  Returns NULL on success and an error
 * string on failure.
 */
char *
correct_readdisklabel(dev, strat, lp)
	dev_t dev;
	d_strategy_t *strat;
	register struct disklabel *lp;
{
	register struct buf *bp;
	struct disklabel *dlp;
	char *msg = NULL;

#if 0
	/*
	 * This clobbers valid labels built by drivers.  It should fail,
	 * except on ancient systems, because it sets lp->d_npartitions
	 * to 1 but the label is supposed to be read from the raw partition,
	 * which is 0 only on ancient systems.  Apparently most drivers
	 * don't check lp->d_npartitions.
	 */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = 1;
	if (lp->d_partitions[0].p_size == 0)
		lp->d_partitions[0].p_size = 0x1fffffff;
	lp->d_partitions[0].p_offset = 0;
#endif

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = LABELSECTOR / lp->d_secpercyl;
	(*strat)(bp);
	if (biowait(bp))
		msg = "I/O error";
	else for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)((char *)bp->b_data +
	    DEV_BSIZE - sizeof(*dlp));
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
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}

#ifdef PRE_DISKSLICE_COMPAT
/*
 * Attempt to read a disk label from a device using the indicated stategy
 * routine.  The label must be partly set up before this: secpercyl and
 * anything required in the strategy routine (e.g., sector size) must be
 * filled in before calling us.  Returns NULL on success and an error
 * string on failure.
 * If Machine Specific Partitions (MSP) are not found, then it will proceed
 * as if the BSD partition starts at 0
 * The MBR on an IBM PC is an example of an MSP.
 */
char *
readdisklabel(dev, strat, lp, dp, bdp)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
	struct dos_partition *dp;
	struct dkbad *bdp;
{
	register struct buf *bp;
	struct disklabel *dlp;
	char *msgMSP = NULL;
	char *msg = NULL;
	int i;
	int cyl = 0;

	/*
	 * Set up the disklabel as in case there is no MSP.
	 * We set the BSD part, but don't need to set the
	 * RAW part, because readMSPtolabel() will reset that
	 * itself. On return however, if there was no MSP,
	 * then we will be looking into OUR part to find the label
	 * and we will want that to start at 0, and have at least SOME length.
	 */
	if (lp->d_secperunit == 0)
		lp->d_secperunit = 0x1fffffff;
	lp->d_npartitions = OURPART + 1;
	if (lp->d_partitions[OURPART].p_size == 0)
		lp->d_partitions[OURPART].p_size = 0x100; /*enough for a label*/
	lp->d_partitions[OURPART].p_offset = 0;

	/*
	 * Dig out the Dos MSP.. If we get it, all remaining transfers
	 * will be relative to the base of the BSD part.
	 */
	msgMSP = readMSPtolabel(dev, strat, lp, dp, &cyl );
	
	/*
	 * next, dig out disk label, relative to either the base of the
	 * BSD part, or block 0, depending on if an MSP was found.
	 */
	bp = geteblk((int)lp->d_secsize);
	bp->b_blkno = LABELSECTOR;
	bp->b_dev = makedev(major(dev), dkminor(dkunit(dev), OURPART));
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	bp->b_cylinder = cyl;
	(*strat)(bp);
	if (biowait(bp))
		msg = "I/O error";
	else for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)((char *)bp->b_data +
	    DEV_BSIZE - sizeof(*dlp));
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

	if (msg && msgMSP) {
		msg = msgMSP;
		goto done;
	}

	/*
	 * Since we had one of the two labels, either one made up from the
	 * MSP, one found in the FreeBSD-MSP-partitions sector 2, or even
	 * one in sector 2 absolute on the disk, there is not really an error.
	 */

	msg = NULL;

	/* obtain bad sector table if requested and present */
	if (bdp && (lp->d_flags & D_BADSECT)) {
		struct dkbad *db;

		printf("d_secsize: %ld\n", lp->d_secsize);
		i = 0;
		do {
			/* read a bad sector table */
			bp->b_flags = B_BUSY | B_READ;
			bp->b_blkno = lp->d_secperunit - lp->d_nsectors + i;
			if (lp->d_secsize > DEV_BSIZE)
				bp->b_blkno *= lp->d_secsize / DEV_BSIZE;
			else
				bp->b_blkno /= DEV_BSIZE / lp->d_secsize;
			bp->b_bcount = lp->d_secsize;
			bp->b_cylinder = lp->d_ncylinders - 1;
			(*strat)(bp);

			/* if successful, validate, otherwise try another */
			if (biowait(bp)) {
				msg = "bad sector table I/O error";
			} else {
				db = (struct dkbad *)(bp->b_un.b_addr);
#define DKBAD_MAGIC 0x4321
				if (db->bt_mbz == 0
					&& db->bt_flag == DKBAD_MAGIC) {
					msg = NULL;
					*bdp = *db;
					break;
				} else
					msg = "bad sector table corrupted";
			}
		} while ((bp->b_flags & B_ERROR) && (i += 2) < 10 &&
			i < lp->d_nsectors);
	}

done:
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	return (msg);
}
#endif /* PRE_DISKSLICE_COMPAT */

/*
 * Check new disk label for sensibility before setting it.
 */
int
setdisklabel(olp, nlp, openmask)
	register struct disklabel *olp, *nlp;
	u_long openmask;
{
	register i;
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
correct_writedisklabel(dev, strat, lp)
	dev_t dev;
	d_strategy_t *strat;
	register struct disklabel *lp;
{
	struct buf *bp;
	struct disklabel *dlp;
	int labelpart;
	int error = 0;

	labelpart = dkpart(dev);
	if (lp->d_partitions[labelpart].p_offset != 0) {
		if (lp->d_partitions[0].p_offset != 0)
			return (EXDEV);			/* not quite right */
		labelpart = 0;
	}
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dkmodpart(dev, labelpart);
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_READ;
	(*strat)(bp);
	error = biowait(bp);
	if (error)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)
	      ((char *)bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			*dlp = *lp;
			bp->b_flags = B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;
done:
	brelse(bp);
	return (error);
}

#ifdef PRE_DISKSLICE_COMPAT
/*
 * Write disk label back to device after modification.
 * For FreeBSD 2.0(x86) this routine will refuse to install a label if
 * there is no DOS MSP. (this can be changed)
 *
 * Assumptions for THIS VERSION:
 * The given disklabel pointer is actually that which is controlling this
 * Device, so that by fiddling it, readMSPtolabel() can ensure that
 * it can read from the MSP if it exists,
 * This assumption will cease as soon as ther is a better way of ensuring
 * that a read is done to the whole raw device.
 * MSP defines a BSD part, label is in block 1 (2nd block) of this
 */
int
writedisklabel(dev, strat, lp)
	dev_t dev;
	void (*strat)();
	register struct disklabel *lp;
{
	struct buf *bp = NULL;
	struct disklabel *dlp;
	int error = 0;
	struct disklabel label;
	char *msg;
	int BSDstart,BSDlen;
	int cyl; /* dummy arg for readMSPtolabel() */

	/*
	 * Save the label (better be the real one)
	 * because we are going to play funny games with the disklabel
	 * controlling this device..
	 */
	bcopy(lp,&label,sizeof(label));
	/*
	 * Unlike the read, we will trust the parameters given to us
	 * about the disk, in the new disklabel but will simply
	 * force OURPART to start at block 0 as a default in case there is NO
	 * MSP.
	 * readMSPtolabel() will reset it to start at the start of the BSD
	 * part if it exists 
	 * At this time this is an error contition but I've left support for it
	 */
	lp->d_npartitions = OURPART + 1;
	if (lp->d_partitions[OURPART].p_size == 0)
		lp->d_partitions[OURPART].p_size = 0x1fffffff;
	lp->d_partitions[OURPART].p_offset = 0;

	msg = readMSPtolabel(dev, strat, lp, 0, &cyl );
	/*
	 * If we want to be able to install without an Machine Specific 
	 * Partitioning , then
	 * the failure of readMSPtolabel() should be made non fatal.
	 */
	if(msg) {
		printf("writedisklabel:%s\n",msg);
		error = ENXIO;
		goto done;
	}
	/*
	 * If we had MSP (no message) but there
	 * was no BSD part in it
	 * then balk.. they should use fdisk to make one first or smash it..
	 * This may just be me being paranoid, but it's my choice for now..
	 * note we test for !msg, because the test above might be changed
	 * as a valid option..
	 */
	if((!msg) && (!(lp->d_subtype & DSTYPE_INDOSPART))) {
		printf("writedisklabel: MSP with no BSD part\n");
	}

	/*
	 * get all the other bits back from the good new disklabel
	 * (the user wouldn't try confuse us would he?)
	 * With the exception of the OURPART which now points to the 
	 * BSD partition.
	 */
	BSDstart = lp->d_partitions[OURPART].p_offset;
	BSDlen = lp->d_partitions[OURPART].p_size;
	bcopy(&label,lp,sizeof(label));
	lp->d_partitions[OURPART].p_offset = BSDstart;
	lp->d_partitions[OURPART].p_size = BSDlen;

	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = makedev(major(dev), dkminor(dkunit(dev), OURPART));
	bp->b_blkno = LABELSECTOR;
	bp->b_bcount = lp->d_secsize;
#ifdef STUPID
	/*
	 * We read the label first to see if it's there,
	 * in which case we will put ours at the same offset into the block..
	 * (I think this is stupid [Julian])
	 * Note that you can't write a label out over a corrupted label!
	 * (also stupid.. how do you write the first one? by raw writes?)
	 */
	bp->b_flags = B_READ;
	(*strat)(bp);
	error = biowait(bp);
	if (error)
		goto done;
	for (dlp = (struct disklabel *)bp->b_data;
	    dlp <= (struct disklabel *)
	      ((char *)bp->b_data + lp->d_secsize - sizeof(*dlp));
	    dlp = (struct disklabel *)((char *)dlp + sizeof(long))) {
		if (dlp->d_magic == DISKMAGIC && dlp->d_magic2 == DISKMAGIC &&
		    dkcksum(dlp) == 0) {
			bcopy(&label,dlp,sizeof(label));
			bp->b_flags = B_WRITE;
			(*strat)(bp);
			error = biowait(bp);
			goto done;
		}
	}
	error = ESRCH;
#else	/* Stupid */
	dlp = (struct disklabel *)bp->b_data;
	bcopy(&label,dlp,sizeof(label));
	bp->b_flags = B_WRITE;
	(*strat)(bp);
	error = biowait(bp);
#endif 	/* Stupid */
done:
	bcopy(&label,lp,sizeof(label)); /* start using the new label again */
	if(bp)
		brelse(bp);
	return (error);
}
#endif /* PRE_DISKSLICE_COMPAT */

/*
 * Compute checksum for disk label.
 */
u_int
dkcksum(lp)
	register struct disklabel *lp;
{
	register u_short *start, *end;
	register u_short sum = 0;

	start = (u_short *)lp;
	end = (u_short *)&lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}

/*
 * Disk error is the preface to plaintive error messages
 * about failing disk transfers.  It prints messages of the form

hp0g: hard error reading fsbn 12345 of 12344-12347 (hp0 bn %d cn %d tn %d sn %d)

 * if the offset of the error in the transfer and a disk label
 * are both available.  blkdone should be -1 if the position of the error
 * is unknown; the disklabel pointer may be null from drivers that have not
 * been converted to use them.  The message is printed with printf
 * if pri is LOG_PRINTF, otherwise it uses log at the specified priority.
 * The message should be completed (with at least a newline) with printf
 * or addlog, respectively.  There is no trailing space.
 */
void
diskerr(bp, dname, what, pri, blkdone, lp)
	register struct buf *bp;
	char *dname, *what;
	int pri, blkdone;
	register struct disklabel *lp;
{
	int unit = dkunit(bp->b_dev);
	int slice = dkslice(bp->b_dev);
	int part = dkpart(bp->b_dev);
	register void (*pr) __P((const char *, ...));
	char partname[2];
	char *sname;
	int sn;

	if (pri != LOG_PRINTF) {
		log(pri, "");
		pr = addlog;
	} else
		pr = printf;
	sname = dsname(dname, unit, slice, part, partname);
	(*pr)("%s%s: %s %sing fsbn ", sname, partname, what,
	      bp->b_flags & B_READ ? "read" : "writ");
	sn = bp->b_blkno;
	if (bp->b_bcount <= DEV_BSIZE)
		(*pr)("%d", sn);
	else {
		if (blkdone >= 0) {
			sn += blkdone;
			(*pr)("%d of ", sn);
		}
		(*pr)("%d-%d", bp->b_blkno,
		    bp->b_blkno + (bp->b_bcount - 1) / DEV_BSIZE);
	}
	if (lp && (blkdone >= 0 || bp->b_bcount <= lp->d_secsize)) {
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
		(*pr)(" (%s bn %d; cn %d", sname, sn, sn / lp->d_secpercyl);
		sn %= lp->d_secpercyl;
		(*pr)(" tn %d sn %d)", sn / lp->d_nsectors, sn % lp->d_nsectors);
	}
}
