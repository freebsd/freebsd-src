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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/vnode.h>

#include <ufs/ffs/fs.h>

#define TRACE(str)	do { if (ds_debug) printf str; } while (0)

typedef	u_char	bool_t;

static volatile bool_t ds_debug;

static struct disklabel *clone_label(struct disklabel *lp);
static void dsiodone(struct bio *bp);
static char *fixlabel(char *sname, struct diskslice *sp,
			   struct disklabel *lp, int writeflag);
static void free_ds_label(struct diskslices *ssp, int slice);
static void partition_info(char *sname, int part, struct partition *pp);
static void slice_info(char *sname, struct diskslice *sp);
static void set_ds_label(struct diskslices *ssp, int slice,
			      struct disklabel *lp);
static void set_ds_labeldevs(dev_t dev, struct diskslices *ssp);
static void set_ds_wlabel(struct diskslices *ssp, int slice,
			       int wlabel);

/*
 * Duplicate a label for the whole disk, and initialize defaults in the
 * copy for fields that are not already initialized.  The caller only
 * needs to initialize d_secsize and d_secperunit, and zero the fields
 * that are to be defaulted.
 */
static struct disklabel *
clone_label(lp)
	struct disklabel *lp;
{
	struct disklabel *lp1;

	lp1 = malloc(sizeof *lp1, M_DEVBUF, M_WAITOK);
	*lp1 = *lp;
	lp = NULL;
	if (lp1->d_typename[0] == '\0')
		strncpy(lp1->d_typename, "amnesiac", sizeof(lp1->d_typename));
	if (lp1->d_packname[0] == '\0')
		strncpy(lp1->d_packname, "fictitious", sizeof(lp1->d_packname));
	if (lp1->d_nsectors == 0)
		lp1->d_nsectors = 32;
	if (lp1->d_ntracks == 0)
		lp1->d_ntracks = 64;
	lp1->d_secpercyl = lp1->d_nsectors * lp1->d_ntracks;
	lp1->d_ncylinders = lp1->d_secperunit / lp1->d_secpercyl;
	if (lp1->d_rpm == 0)
		lp1->d_rpm = 3600;
	if (lp1->d_interleave == 0)
		lp1->d_interleave = 1;
	if (lp1->d_npartitions < RAW_PART + 1)
		lp1->d_npartitions = MAXPARTITIONS;
	if (lp1->d_bbsize == 0)
		lp1->d_bbsize = BBSIZE;
	if (lp1->d_sbsize == 0)
		lp1->d_sbsize = SBSIZE;
	lp1->d_partitions[RAW_PART].p_size = lp1->d_secperunit;
	lp1->d_magic = DISKMAGIC;
	lp1->d_magic2 = DISKMAGIC;
	lp1->d_checksum = dkcksum(lp1);
	return (lp1);
}

dev_t
dkmodpart(dev_t dev, int part)
{
	return (makedev(major(dev), (minor(dev) & ~7) | part));
}

dev_t
dkmodslice(dev_t dev, int slice)
{
	return (makedev(major(dev), (minor(dev) & ~0x1f0000) | (slice << 16)));
}

u_int
dkunit(dev_t dev)
{
	return (((minor(dev) >> 16) & 0x1e0) | ((minor(dev) >> 3) & 0x1f));
}

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 *
 * XXX TODO:
 *	o Split buffers that are too big for the device.
 *	o Check for overflow.
 *	o Finish cleaning this up.
 */
int
dscheck(bp, ssp)
	struct bio *bp;
	struct diskslices *ssp;
{
	daddr_t	blkno;
	u_long	endsecno;
	daddr_t	labelsect;
	struct disklabel *lp;
	char *msg;
	long	nsec;
	struct partition *pp;
	daddr_t	secno;
	daddr_t	slicerel_secno;
	struct diskslice *sp;

	blkno = bp->bio_blkno;
	if (blkno < 0) {
		printf("dscheck(%s): negative bio_blkno %ld\n", 
		    devtoname(bp->bio_dev), (long)blkno);
		bp->bio_error = EINVAL;
		goto bad;
	}
	sp = &ssp->dss_slices[dkslice(bp->bio_dev)];
	lp = sp->ds_label;
	if (ssp->dss_secmult == 1) {
		if (bp->bio_bcount % (u_long)DEV_BSIZE)
			goto bad_bcount;
		secno = blkno;
		nsec = bp->bio_bcount >> DEV_BSHIFT;
	} else if (ssp->dss_secshift != -1) {
		if (bp->bio_bcount & (ssp->dss_secsize - 1))
			goto bad_bcount;
		if (blkno & (ssp->dss_secmult - 1))
			goto bad_blkno;
		secno = blkno >> ssp->dss_secshift;
		nsec = bp->bio_bcount >> (DEV_BSHIFT + ssp->dss_secshift);
	} else {
		if (bp->bio_bcount % ssp->dss_secsize)
			goto bad_bcount;
		if (blkno % ssp->dss_secmult)
			goto bad_blkno;
		secno = blkno / ssp->dss_secmult;
		nsec = bp->bio_bcount / ssp->dss_secsize;
	}
	if (lp == NULL) {
		labelsect = -LABELSECTOR - 1;
		endsecno = sp->ds_size;
		slicerel_secno = secno;
	} else {
		labelsect = lp->d_partitions[LABEL_PART].p_offset;
if (labelsect != 0) Debugger("labelsect != 0 in dscheck()");
		pp = &lp->d_partitions[dkpart(bp->bio_dev)];
		endsecno = pp->p_size;
		slicerel_secno = pp->p_offset + secno;
	}

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
	if (slicerel_secno <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
	    slicerel_secno + nsec > LABELSECTOR + labelsect &&
#endif
	    (bp->bio_cmd == BIO_WRITE) && sp->ds_wlabel == 0) {
		bp->bio_error = EROFS;
		goto bad;
	}

#if defined(DOSBBSECTOR) && defined(notyet)
	/* overwriting master boot record? */
	if (slicerel_secno <= DOSBBSECTOR && (bp->bio_cmd == BIO_WRITE) &&
	    sp->ds_wlabel == 0) {
		bp->bio_error = EROFS;
		goto bad;
	}
#endif

	/* beyond partition? */
	if (secno + nsec > endsecno) {
		/* if exactly at end of disk, return an EOF */
		if (secno == endsecno) {
			bp->bio_resid = bp->bio_bcount;
			return (0);
		}
		/* or truncate if part of it fits */
		nsec = endsecno - secno;
		if (nsec <= 0) {
			bp->bio_error = EINVAL;
			goto bad;
		}
		bp->bio_bcount = nsec * ssp->dss_secsize;
	}

	bp->bio_pblkno = sp->ds_offset + slicerel_secno;

	/*
	 * Snoop on label accesses if the slice offset is nonzero.  Fudge
	 * offsets in the label to keep the in-core label coherent with
	 * the on-disk one.
	 */
	if (slicerel_secno <= LABELSECTOR + labelsect
#if LABELSECTOR != 0
	    && slicerel_secno + nsec > LABELSECTOR + labelsect
#endif
	    && sp->ds_offset != 0) {
		struct iodone_chain *ic;

		ic = malloc(sizeof *ic , M_DEVBUF, M_WAITOK);
		ic->ic_prev_flags = bp->bio_flags;
		ic->ic_prev_iodone = bp->bio_done;
		ic->ic_prev_iodone_chain = bp->bio_done_chain;
		ic->ic_args[0].ia_long = (LABELSECTOR + labelsect -
		    slicerel_secno) * ssp->dss_secsize;
		ic->ic_args[1].ia_ptr = sp;
		bp->bio_done = dsiodone;
		bp->bio_done_chain = ic;
		if (!(bp->bio_cmd == BIO_READ)) {
			/*
			 * XXX even disklabel(8) writes directly so we need
			 * to adjust writes.  Perhaps we should drop support
			 * for DIOCWLABEL (always write protect labels) and
			 * require the use of DIOCWDINFO.
			 *
			 * XXX probably need to copy the data to avoid even
			 * temporarily corrupting the in-core copy.
			 */
			/* XXX need name here. */
			msg = fixlabel((char *)NULL, sp,
				       (struct disklabel *)
				       (bp->bio_data + ic->ic_args[0].ia_long),
				       TRUE);
			if (msg != NULL) {
				printf("dscheck(%s): %s\n", 
				    devtoname(bp->bio_dev), msg);
				bp->bio_error = EROFS;
				goto bad;
			}
		}
	}
	return (1);

bad_bcount:
	printf(
	"dscheck(%s): bio_bcount %ld is not on a sector boundary (ssize %d)\n",
	    devtoname(bp->bio_dev), bp->bio_bcount, ssp->dss_secsize);
	bp->bio_error = EINVAL;
	goto bad;

bad_blkno:
	printf(
	"dscheck(%s): bio_blkno %ld is not on a sector boundary (ssize %d)\n",
	    devtoname(bp->bio_dev), (long)blkno, ssp->dss_secsize);
	bp->bio_error = EINVAL;
	goto bad;

bad:
	bp->bio_resid = bp->bio_bcount;
	bp->bio_flags |= BIO_ERROR;
	return (-1);
}

void
dsclose(dev, mode, ssp)
	dev_t	dev;
	int	mode;
	struct diskslices *ssp;
{
	u_char	mask;
	struct diskslice *sp;

	sp = &ssp->dss_slices[dkslice(dev)];
	mask = 1 << dkpart(dev);
	sp->ds_openmask &= ~mask;
}

void
dsgone(sspp)
	struct diskslices **sspp;
{
	int	slice;
	struct diskslice *sp;
	struct diskslices *ssp;

	for (slice = 0, ssp = *sspp; slice < ssp->dss_nslices; slice++) {
		sp = &ssp->dss_slices[slice];
		free_ds_label(ssp, slice);
	}
	free(ssp, M_DEVBUF);
	*sspp = NULL;
}

/*
 * For the "write" commands (DIOCSDINFO and DIOCWDINFO), this
 * is subject to the same restriction as dsopen().
 */
int
dsioctl(dev, cmd, data, flags, sspp)
	dev_t	dev;
	u_long	cmd;
	caddr_t	data;
	int	flags;
	struct diskslices **sspp;
{
	int	error;
	struct disklabel *lp;
	int	old_wlabel;
	u_char	openmask;
	int	part;
	int	slice;
	struct diskslice *sp;
	struct diskslices *ssp;
	struct partition *pp;

	slice = dkslice(dev);
	ssp = *sspp;
	sp = &ssp->dss_slices[slice];
	lp = sp->ds_label;
	switch (cmd) {

	case DIOCGDVIRGIN:
		lp = (struct disklabel *)data;
		if (ssp->dss_slices[WHOLE_DISK_SLICE].ds_label) {
			*lp = *ssp->dss_slices[WHOLE_DISK_SLICE].ds_label;
		} else {
			bzero(lp, sizeof(struct disklabel));
		}

		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		pp = &lp->d_partitions[RAW_PART];
		pp->p_offset = 0;
		pp->p_size = sp->ds_size;

		lp->d_npartitions = MAXPARTITIONS;
		if (lp->d_interleave == 0)
			lp->d_interleave = 1;
		if (lp->d_rpm == 0)
			lp->d_rpm = 3600;
		if (lp->d_nsectors == 0)
			lp->d_nsectors = 32;
		if (lp->d_ntracks == 0)
			lp->d_ntracks = 64;

		lp->d_bbsize = BBSIZE;
		lp->d_sbsize = SBSIZE;
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
		lp->d_ncylinders = sp->ds_size / lp->d_secpercyl;
		lp->d_secperunit = sp->ds_size;
		lp->d_checksum = 0;
		lp->d_checksum = dkcksum(lp);
		return (0);

	case DIOCGDINFO:
		if (lp == NULL)
			return (EINVAL);
		*(struct disklabel *)data = *lp;
		return (0);

	case DIOCGSECTORSIZE:
		if (lp == NULL)
			return (EINVAL);
		*(u_int *)data = lp->d_secsize;
		return (0);

	case DIOCGMEDIASIZE:
		if (lp == NULL)
			return (EINVAL);
		*(off_t *)data = (off_t)lp->d_partitions[dkpart(dev)].p_size *
		    lp->d_secsize;
		return (0);

	case DIOCGSLICEINFO:
		bcopy(ssp, data, (char *)&ssp->dss_slices[ssp->dss_nslices] -
				 (char *)ssp);
		return (0);

	case DIOCSDINFO:
		if (slice == WHOLE_DISK_SLICE)
			return (ENODEV);
		if (!(flags & FWRITE))
			return (EBADF);
		lp = malloc(sizeof *lp, M_DEVBUF, M_WAITOK);
		if (sp->ds_label == NULL)
			bzero(lp, sizeof *lp);
		else
			bcopy(sp->ds_label, lp, sizeof *lp);
		if (sp->ds_label == NULL)
			openmask = 0;
		else {
			openmask = sp->ds_openmask;
			if (slice == COMPATIBILITY_SLICE)
				openmask |= ssp->dss_slices[
				    ssp->dss_first_bsd_slice].ds_openmask;
			else if (slice == ssp->dss_first_bsd_slice)
				openmask |= ssp->dss_slices[
				    COMPATIBILITY_SLICE].ds_openmask;
		}
		error = setdisklabel(lp, (struct disklabel *)data,
				     (u_long)openmask);
		/* XXX why doesn't setdisklabel() check this? */
		if (error == 0 && lp->d_partitions[RAW_PART].p_offset != 0)
			error = EXDEV;
		if (error == 0) {
			if (lp->d_secperunit > sp->ds_size)
				error = ENOSPC;
			for (part = 0; part < lp->d_npartitions; part++)
				if (lp->d_partitions[part].p_size > sp->ds_size)
					error = ENOSPC;
		}
		if (error != 0) {
			free(lp, M_DEVBUF);
			return (error);
		}
		free_ds_label(ssp, slice);
		set_ds_label(ssp, slice, lp);
		set_ds_labeldevs(dev, ssp);
		return (0);

	case DIOCSYNCSLICEINFO:
		if (slice != WHOLE_DISK_SLICE || dkpart(dev) != RAW_PART)
			return (EINVAL);
		if (!*(int *)data)
			for (slice = 0; slice < ssp->dss_nslices; slice++) {
				openmask = ssp->dss_slices[slice].ds_openmask;
				if (openmask
				    && (slice != WHOLE_DISK_SLICE
					|| openmask & ~(1 << RAW_PART)))
					return (EBUSY);
			}

		/*
		 * Temporarily forget the current slices struct and read
		 * the current one.
		 * XXX should wait for current accesses on this disk to
		 * complete, then lock out future accesses and opens.
		 */
		*sspp = NULL;
		lp = malloc(sizeof *lp, M_DEVBUF, M_WAITOK);
		*lp = *ssp->dss_slices[WHOLE_DISK_SLICE].ds_label;
		error = dsopen(dev, S_IFCHR, ssp->dss_oflags, sspp, lp);
		if (error != 0) {
			free(lp, M_DEVBUF);
			*sspp = ssp;
			return (error);
		}

		/*
		 * Reopen everything.  This is a no-op except in the "force"
		 * case and when the raw bdev and cdev are both open.  Abort
		 * if anything fails.
		 */
		for (slice = 0; slice < ssp->dss_nslices; slice++) {
			for (openmask = ssp->dss_slices[slice].ds_openmask,
			     part = 0; openmask; openmask >>= 1, part++) {
				if (!(openmask & 1))
					continue;
				error = dsopen(dkmodslice(dkmodpart(dev, part),
							  slice),
					       S_IFCHR, ssp->dss_oflags, sspp,
					       lp);
				if (error != 0) {
					free(lp, M_DEVBUF);
					*sspp = ssp;
					return (EBUSY);
				}
			}
		}

		free(lp, M_DEVBUF);
		dsgone(&ssp);
		return (0);

	case DIOCWDINFO:
		error = dsioctl(dev, DIOCSDINFO, data, flags, &ssp);
		if (error != 0)
			return (error);
		/*
		 * XXX this used to hack on dk_openpart to fake opening
		 * partition 0 in case that is used instead of dkpart(dev).
		 */
		old_wlabel = sp->ds_wlabel;
		set_ds_wlabel(ssp, slice, TRUE);
		error = writedisklabel(dev, sp->ds_label);
		/* XXX should invalidate in-core label if write failed. */
		set_ds_wlabel(ssp, slice, old_wlabel);
		return (error);

	case DIOCWLABEL:
#ifndef __alpha__
		if (slice == WHOLE_DISK_SLICE)
			return (ENODEV);
#endif
		if (!(flags & FWRITE))
			return (EBADF);
		set_ds_wlabel(ssp, slice, *(int *)data != 0);
		return (0);

	default:
		return (ENOIOCTL);
	}
}

static void
dsiodone(bp)
	struct bio *bp;
{
	struct iodone_chain *ic;
	char *msg;

	ic = bp->bio_done_chain;
	bp->bio_done = ic->ic_prev_iodone;
	bp->bio_done_chain = ic->ic_prev_iodone_chain;
	if (!(bp->bio_cmd == BIO_READ)
	    || (!(bp->bio_flags & BIO_ERROR) && bp->bio_error == 0)) {
		msg = fixlabel((char *)NULL, ic->ic_args[1].ia_ptr,
			       (struct disklabel *)
			       (bp->bio_data + ic->ic_args[0].ia_long),
			       FALSE);
		if (msg != NULL)
			printf("%s\n", msg);
	}
	free(ic, M_DEVBUF);
	biodone(bp);
}

int
dsisopen(ssp)
	struct diskslices *ssp;
{
	int	slice;

	if (ssp == NULL)
		return (0);
	for (slice = 0; slice < ssp->dss_nslices; slice++)
		if (ssp->dss_slices[slice].ds_openmask)
			return (1);
	return (0);
}

/*
 * Allocate a slices "struct" and initialize it to contain only an empty
 * compatibility slice (pointing to itself), a whole disk slice (covering
 * the disk as described by the label), and (nslices - BASE_SLICES) empty
 * slices beginning at BASE_SLICE.
 */
struct diskslices *
dsmakeslicestruct(nslices, lp)
	int nslices;
	struct disklabel *lp;
{
	struct diskslice *sp;
	struct diskslices *ssp;

	ssp = malloc(offsetof(struct diskslices, dss_slices) +
		     nslices * sizeof *sp, M_DEVBUF, M_WAITOK);
	ssp->dss_first_bsd_slice = COMPATIBILITY_SLICE;
	ssp->dss_nslices = nslices;
	ssp->dss_oflags = 0;
	ssp->dss_secmult = lp->d_secsize / DEV_BSIZE;
	if (ssp->dss_secmult & (ssp->dss_secmult - 1))
		ssp->dss_secshift = -1;
	else
		ssp->dss_secshift = ffs(ssp->dss_secmult) - 1;
	ssp->dss_secsize = lp->d_secsize;
	sp = &ssp->dss_slices[0];
	bzero(sp, nslices * sizeof *sp);
	sp[WHOLE_DISK_SLICE].ds_size = lp->d_secperunit;
	return (ssp);
}

char *
dsname(dev, unit, slice, part, partname)
	dev_t	dev;
	int	unit;
	int	slice;
	int	part;
	char	*partname;
{
	static char name[32];
	const char *dname;

	dname = devsw(dev)->d_name;
	if (strlen(dname) > 16)
		dname = "nametoolong";
	snprintf(name, sizeof(name), "%s%d", dname, unit);
	partname[0] = '\0';
	if (slice != WHOLE_DISK_SLICE || part != RAW_PART) {
		partname[0] = 'a' + part;
		partname[1] = '\0';
		if (slice != COMPATIBILITY_SLICE)
			snprintf(name + strlen(name),
			    sizeof(name) - strlen(name), "s%d", slice - 1);
	}
	return (name);
}

/*
 * This should only be called when the unit is inactive and the strategy
 * routine should not allow it to become active unless we call it.  Our
 * strategy routine must be special to allow activity.
 */
int
dsopen(dev, mode, flags, sspp, lp)
	dev_t	dev;
	int	mode;
	u_int	flags;
	struct diskslices **sspp;
	struct disklabel *lp;
{
	dev_t	dev1;
	int	error;
	struct disklabel *lp1;
	char	*msg;
	u_char	mask;
	int	part;
	char	partname[2];
	int	slice;
	char	*sname;
	struct diskslice *sp;
	struct diskslices *ssp;
	int	unit;

	dev->si_bsize_phys = lp->d_secsize;

	unit = dkunit(dev);
	if (lp->d_secsize % DEV_BSIZE) {
		printf("%s: invalid sector size %lu\n", devtoname(dev),
		    (u_long)lp->d_secsize);
		return (EINVAL);
	}

	/*
	 * XXX reinitialize the slice table unless there is an open device
	 * on the unit.  This should only be done if the media has changed.
	 */
	ssp = *sspp;
	if (!dsisopen(ssp)) {
		if (ssp != NULL)
			dsgone(sspp);
		/*
		 * Allocate a minimal slices "struct".  This will become
		 * the final slices "struct" if we don't want real slices
		 * or if we can't find any real slices.
		 */
		*sspp = dsmakeslicestruct(BASE_SLICE, lp);

		if (!(flags & DSO_ONESLICE)) {
			TRACE(("dsinit\n"));
			error = dsinit(dev, lp, sspp);
			if (error != 0) {
				dsgone(sspp);
				return (error);
			}
		}
		ssp = *sspp;
		ssp->dss_oflags = flags;

		/*
		 * If there are no real slices, then make the compatiblity
		 * slice cover the whole disk.
		 */
		if (ssp->dss_nslices == BASE_SLICE)
			ssp->dss_slices[COMPATIBILITY_SLICE].ds_size
				= lp->d_secperunit;

		/* Point the compatibility slice at the BSD slice, if any. */
		for (slice = BASE_SLICE; slice < ssp->dss_nslices; slice++) {
			sp = &ssp->dss_slices[slice];
			if (sp->ds_type == DOSPTYP_386BSD /* XXX */) {
				ssp->dss_first_bsd_slice = slice;
				ssp->dss_slices[COMPATIBILITY_SLICE].ds_offset
					= sp->ds_offset;
				ssp->dss_slices[COMPATIBILITY_SLICE].ds_size
					= sp->ds_size;
				ssp->dss_slices[COMPATIBILITY_SLICE].ds_type
					= sp->ds_type;
				break;
			}
		}

		ssp->dss_slices[WHOLE_DISK_SLICE].ds_label = clone_label(lp);
		ssp->dss_slices[WHOLE_DISK_SLICE].ds_wlabel = TRUE;
	}

	/* Initialize secondary info for all slices.  */
	for (slice = 0; slice < ssp->dss_nslices; slice++) {
		sp = &ssp->dss_slices[slice];
		if (sp->ds_label != NULL
#ifdef __alpha__
		    && slice != WHOLE_DISK_SLICE
#endif
		    )
			continue;
		dev1 = dkmodslice(dkmodpart(dev, RAW_PART), slice);
#if 0
		sname = dsname(dev, unit, slice, RAW_PART, partname);
#else
		*partname='\0';
		sname = dev1->si_name;
#endif
		/*
		 * XXX this should probably only be done for the need_init
		 * case, but there may be a problem with DIOCSYNCSLICEINFO.
		 */
		set_ds_wlabel(ssp, slice, TRUE);	/* XXX invert */
		lp1 = clone_label(lp);
		TRACE(("readdisklabel\n"));
		if (flags & DSO_NOLABELS)
			msg = NULL;
		else {
			msg = readdisklabel(dev1, lp1);

			/*
			 * readdisklabel() returns NULL for success, and an
			 * error string for failure.
			 *
			 * If there isn't a label on the disk, and if the
			 * DSO_COMPATLABEL is set, we want to use the
			 * faked-up label provided by the caller.
			 *
			 * So we set msg to NULL to indicate that there is
			 * no failure (since we have a faked-up label),
			 * free lp1, and then clone it again from lp.
			 * (In case readdisklabel() modified lp1.)
			 */
			if (msg != NULL && (flags & DSO_COMPATLABEL)) {
				msg = NULL;
				free(lp1, M_DEVBUF);
				lp1 = clone_label(lp);
			}
		}
		if (msg == NULL)
			msg = fixlabel(sname, sp, lp1, FALSE);
		if (msg == NULL && lp1->d_secsize != ssp->dss_secsize)
			msg = "inconsistent sector size";
		if (msg != NULL) {
			if (sp->ds_type == DOSPTYP_386BSD /* XXX */)
				log(LOG_WARNING, "%s: cannot find label (%s)\n",
				    sname, msg);
			free(lp1, M_DEVBUF);
			continue;
		}
		if (lp1->d_flags & D_BADSECT) {
			log(LOG_ERR, "%s: bad sector table not supported\n",
			    sname);
			free(lp1, M_DEVBUF);
			continue;
		}
		set_ds_label(ssp, slice, lp1);
		set_ds_labeldevs(dev1, ssp);
		set_ds_wlabel(ssp, slice, FALSE);
	}

	slice = dkslice(dev);
	if (slice >= ssp->dss_nslices)
		return (ENXIO);
	sp = &ssp->dss_slices[slice];
	part = dkpart(dev);
	if (part != RAW_PART
	    && (sp->ds_label == NULL || part >= sp->ds_label->d_npartitions))
		return (EINVAL);	/* XXX needs translation */
	mask = 1 << part;
	sp->ds_openmask |= mask;
	return (0);
}

int
dssize(dev, sspp)
	dev_t	dev;
	struct diskslices **sspp;
{
	struct disklabel *lp;
	int	part;
	int	slice;
	struct diskslices *ssp;

	slice = dkslice(dev);
	part = dkpart(dev);
	ssp = *sspp;
	if (ssp == NULL || slice >= ssp->dss_nslices
	    || !(ssp->dss_slices[slice].ds_openmask & (1 << part))) {
		if (devsw(dev)->d_open(dev, FREAD, S_IFCHR,
		    (struct thread *)NULL) != 0)
			return (-1);
		devsw(dev)->d_close(dev, FREAD, S_IFCHR, (struct thread *)NULL);
		ssp = *sspp;
	}
	lp = ssp->dss_slices[slice].ds_label;
	if (lp == NULL)
		return (-1);
	return ((int)lp->d_partitions[part].p_size);
}

static void
free_ds_label(ssp, slice)
	struct diskslices *ssp;
	int	slice;
{
	struct disklabel *lp;
	struct diskslice *sp;

	sp = &ssp->dss_slices[slice];
	lp = sp->ds_label;
	if (lp == NULL)
		return;
	free(lp, M_DEVBUF);
	set_ds_label(ssp, slice, (struct disklabel *)NULL);
}


static char *
fixlabel(sname, sp, lp, writeflag)
	char	*sname;
	struct diskslice *sp;
	struct disklabel *lp;
	int	writeflag;
{
	u_long	end;
	u_long	offset;
	int	part;
	struct partition *pp;
	u_long	start;
	bool_t	warned;

	/* These errors "can't happen" so don't bother reporting details. */
	if (lp->d_magic != DISKMAGIC || lp->d_magic2 != DISKMAGIC)
		return ("fixlabel: invalid magic");
	if (dkcksum(lp) != 0)
		return ("fixlabel: invalid checksum");

	pp = &lp->d_partitions[RAW_PART];
	if (writeflag) {
		start = 0;
		offset = sp->ds_offset;
	} else {
		start = sp->ds_offset;
		offset = -sp->ds_offset;
	}
	if (pp->p_offset != start) {
		if (sname != NULL) {
			printf(
"%s: rejecting BSD label: raw partition offset != slice offset\n",
			       sname);
			slice_info(sname, sp);
			partition_info(sname, RAW_PART, pp);
		}
		return ("fixlabel: raw partition offset != slice offset");
	}
	if (pp->p_size != sp->ds_size) {
		if (sname != NULL) {
			printf("%s: raw partition size != slice size\n", sname);
			slice_info(sname, sp);
			partition_info(sname, RAW_PART, pp);
		}
		if (pp->p_size > sp->ds_size) {
			if (sname == NULL)
				return ("fixlabel: raw partition size > slice size");
			printf("%s: truncating raw partition\n", sname);
			pp->p_size = sp->ds_size;
		}
	}
	end = start + sp->ds_size;
	if (start > end)
		return ("fixlabel: slice wraps");
	if (lp->d_secpercyl <= 0)
		return ("fixlabel: d_secpercyl <= 0");
	pp -= RAW_PART;
	warned = FALSE;
	for (part = 0; part < lp->d_npartitions; part++, pp++) {
		if (pp->p_offset != 0 || pp->p_size != 0) {
			if (pp->p_offset < start
			    || pp->p_offset + pp->p_size > end
			    || pp->p_offset + pp->p_size < pp->p_offset) {
				if (sname != NULL) {
					printf(
"%s: rejecting partition in BSD label: it isn't entirely within the slice\n",
					       sname);
					if (!warned) {
						slice_info(sname, sp);
						warned = TRUE;
					}
					partition_info(sname, part, pp);
				}
				/* XXX else silently discard junk. */
				bzero(pp, sizeof *pp);
			} else
				pp->p_offset += offset;
		}
	}
	lp->d_ncylinders = sp->ds_size / lp->d_secpercyl;
	lp->d_secperunit = sp->ds_size;
 	lp->d_checksum = 0;
 	lp->d_checksum = dkcksum(lp);
	return (NULL);
}

static void
partition_info(sname, part, pp)
	char	*sname;
	int	part;
	struct partition *pp;
{
	printf("%s%c: start %lu, end %lu, size %lu\n", sname, 'a' + part,
	       (u_long)pp->p_offset, (u_long)(pp->p_offset + pp->p_size - 1),
	       (u_long)pp->p_size);
}

static void
slice_info(sname, sp)
	char	*sname;
	struct diskslice *sp;
{
	printf("%s: start %lu, end %lu, size %lu\n", sname,
	       sp->ds_offset, sp->ds_offset + sp->ds_size - 1, sp->ds_size);
}

static void
set_ds_label(ssp, slice, lp)
	struct diskslices *ssp;
	int	slice;
	struct disklabel *lp;
{
	ssp->dss_slices[slice].ds_label = lp;
	if (slice == COMPATIBILITY_SLICE)
		ssp->dss_slices[ssp->dss_first_bsd_slice].ds_label = lp;
	else if (slice == ssp->dss_first_bsd_slice)
		ssp->dss_slices[COMPATIBILITY_SLICE].ds_label = lp;
}

static void
set_ds_labeldevs(dev, ssp)
	dev_t	dev;
	struct diskslices *ssp;
{
}


static void
set_ds_wlabel(ssp, slice, wlabel)
	struct diskslices *ssp;
	int	slice;
	int	wlabel;
{
	ssp->dss_slices[slice].ds_wlabel = wlabel;
	if (slice == COMPATIBILITY_SLICE)
		ssp->dss_slices[ssp->dss_first_bsd_slice].ds_wlabel = wlabel;
	else if (slice == ssp->dss_first_bsd_slice)
		ssp->dss_slices[COMPATIBILITY_SLICE].ds_wlabel = wlabel;
}
