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
 *	$Id: subr_diskslice.c,v 1.30.2.2 1998/03/20 16:35:48 jkh Exp $
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/dkbad.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <ufs/ffs/fs.h>

#define b_cylinder	b_resid

#define TRACE(str)	do { if (ds_debug) printf str; } while (0)

typedef	u_char	bool_t;

static volatile bool_t ds_debug;

static void dsiodone __P((struct buf *bp));
static char *fixlabel __P((char *sname, struct diskslice *sp,
			   struct disklabel *lp, int writeflag));
static void free_ds_label __P((struct diskslices *ssp, int slice));
#ifdef DEVFS
static void free_ds_labeldevs __P((struct diskslices *ssp, int slice));
#endif
static void partition_info __P((char *sname, int part, struct partition *pp));
static void slice_info __P((char *sname, struct diskslice *sp));
static void set_ds_bad __P((struct diskslices *ssp, int slice,
			    struct dkbad_intern *btp));
static void set_ds_label __P((struct diskslices *ssp, int slice,
			      struct disklabel *lp));
#ifdef DEVFS
static void set_ds_labeldevs __P((char *dname, dev_t dev,
				  struct diskslices *ssp));
static void set_ds_labeldevs_unaliased __P((char *dname, dev_t dev,
					    struct diskslices *ssp));
#endif
static void set_ds_wlabel __P((struct diskslices *ssp, int slice,
			       int wlabel));

/*
 * Determine the size of the transfer, and make sure it is
 * within the boundaries of the partition. Adjust transfer
 * if needed, and signal errors or early completion.
 *
 * XXX TODO:
 *	o Do bad sector remapping.  May need to split buffer.
 *	o Split buffers that are too big for the device.
 *	o Check for overflow.
 *	o Finish cleaning this up.
 */
int
dscheck(bp, ssp)
	struct buf *bp;
	struct diskslices *ssp;
{
	daddr_t	blkno;
	daddr_t	labelsect;
	struct disklabel *lp;
	u_long	maxsz;
	char *msg;
	struct partition *pp;
	struct diskslice *sp;
	long	sz;

	if (bp->b_blkno < 0) {
		Debugger("Slice code got negative blocknumber");
		bp->b_error = EINVAL;
		goto bad;
	}

	sp = &ssp->dss_slices[dkslice(bp->b_dev)];
	lp = sp->ds_label;
	sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;
	if (lp == NULL) {
		blkno = bp->b_blkno;
		labelsect = -LABELSECTOR - 1;
		maxsz = sp->ds_size;
	} else {
		labelsect = lp->d_partitions[LABEL_PART].p_offset;
if (labelsect != 0) Debugger("labelsect != 0 in dscheck()");
		pp = &lp->d_partitions[dkpart(bp->b_dev)];
		blkno = pp->p_offset + bp->b_blkno;
		maxsz = pp->p_size;
		if (sp->ds_bad != NULL && ds_debug) {
			daddr_t	newblkno;

			newblkno = transbad144(sp->ds_bad, blkno);
			if (newblkno != blkno)
				printf("should map bad block %lu -> %lu\n",
				       blkno, newblkno);
		}
	}

	/* overwriting disk label ? */
	/* XXX should also protect bootstrap in first 8K */
	if (blkno <= LABELSECTOR + labelsect &&
#if LABELSECTOR != 0
	    bp->b_blkno + sz > LABELSECTOR + labelsect &&
#endif
	    (bp->b_flags & B_READ) == 0 && sp->ds_wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}

#if defined(DOSBBSECTOR) && defined(notyet)
	/* overwriting master boot record? */
	if (blkno <= DOSBBSECTOR && (bp->b_flags & B_READ) == 0 &&
	    sp->ds_wlabel == 0) {
		bp->b_error = EROFS;
		goto bad;
	}
#endif

	/* beyond partition? */
	if (bp->b_blkno < 0 || bp->b_blkno + sz > maxsz) {
		/* if exactly at end of disk, return an EOF */
		if (bp->b_blkno == maxsz) {
			bp->b_resid = bp->b_bcount;
			return (0);
		}
		/* or truncate if part of it fits */
		sz = maxsz - bp->b_blkno;
		if (sz <= 0) {
			bp->b_error = EINVAL;
			goto bad;
		}
		bp->b_bcount = sz << DEV_BSHIFT;
	}

	/* calculate cylinder for disksort to order transfers with */
	bp->b_pblkno = blkno + sp->ds_offset;
	if (lp == NULL)
		bp->b_cylinder = 0;	/* XXX always 0 would be better */
	else
		bp->b_cylinder = bp->b_pblkno / lp->d_secpercyl;

	/*
	 * Snoop on label accesses if the slice offset is nonzero.  Fudge
	 * offsets in the label to keep the in-core label coherent with
	 * the on-disk one.
	 */
	if (blkno <= LABELSECTOR + labelsect
#if LABELSECTOR != 0
	    && bp->b_blkno + sz > LABELSECTOR + labelsect
#endif
	    && sp->ds_offset != 0) {
		struct iodone_chain *ic;

		ic = malloc(sizeof *ic , M_DEVBUF, M_WAITOK);
		ic->ic_prev_flags = bp->b_flags;
		ic->ic_prev_iodone = bp->b_iodone;
		ic->ic_prev_iodone_chain = bp->b_iodone_chain;
		ic->ic_args[0].ia_long = (LABELSECTOR + labelsect - blkno)
					 << DEV_BSHIFT;
		ic->ic_args[1].ia_ptr = sp;
		bp->b_flags |= B_CALL;
		bp->b_iodone = dsiodone;
		bp->b_iodone_chain = ic;
		if (!(bp->b_flags & B_READ)) {
			/*
			 * XXX even disklabel(8) writes directly so we need
			 * to adjust writes.  Perhaps we should drop support
			 * for DIOCWLABEL (always write protect labels) and
			 * require the use of DIOCWDINFO.
			 *
			 * XXX probably need to copy the data to avoid even
			 * temporarily corrupting the in-core copy.
			 */
			if (bp->b_vp != NULL)
				bp->b_vp->v_numoutput++;
			msg = fixlabel((char *)NULL, sp,
				       (struct disklabel *)
				       (bp->b_data + ic->ic_args[0].ia_long),
				       TRUE);
			if (msg != NULL) {
				printf("%s\n", msg);
				bp->b_error = EROFS;
				goto bad;
			}
		}
	}
	return (1);

bad:
	bp->b_flags |= B_ERROR;
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
	switch (mode) {
	case S_IFBLK:
		sp->ds_bopenmask &= ~mask;
		break;
	case S_IFCHR:
		sp->ds_copenmask &= ~mask;
		break;
	}
	sp->ds_openmask = sp->ds_bopenmask | sp->ds_copenmask;
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
		if (sp->ds_bad != NULL) {
			free(sp->ds_bad, M_DEVBUF);
			set_ds_bad(ssp, slice, (struct dkbad_intern *)NULL);
		}
#ifdef DEVFS
		if (sp->ds_bdev != NULL)
			devfs_remove_dev(sp->ds_bdev);
		if (sp->ds_cdev != NULL)
			devfs_remove_dev(sp->ds_cdev);
#endif
		free_ds_label(ssp, slice);
	}
	free(ssp, M_DEVBUF);
	*sspp = NULL;
}

/*
 * For the "write" commands (DIOCSBAD, DIOCSDINFO and DIOCWDINFO), this
 * is subject to the same restriction as dsopen().
 */
int
dsioctl(dname, dev, cmd, data, flags, sspp, strat, setgeom)
	char	*dname;
	dev_t	dev;
	int	cmd;
	caddr_t	data;
	int	flags;
	struct diskslices **sspp;
	d_strategy_t *strat;
	ds_setgeom_t *setgeom;
{
	int	error;
	struct disklabel *lp;
	int	old_wlabel;
	u_char	openmask;
	int	part;
	int	slice;
	struct diskslice *sp;
	struct diskslices *ssp;

	slice = dkslice(dev);
	ssp = *sspp;
	sp = &ssp->dss_slices[slice];
	lp = sp->ds_label;
	switch (cmd) {

	case DIOCGDINFO:
		if (lp == NULL)
			return (EINVAL);
		*(struct disklabel *)data = *lp;
		return (0);

#ifdef notyet
	case DIOCGDINFOP:
		if (lp == NULL)
			return (EINVAL);
		*(struct disklabel **)data = lp;
		return (0);
#endif

	case DIOCGPART:
		if (lp == NULL)
			return (EINVAL);
		((struct partinfo *)data)->disklab = lp;
		((struct partinfo *)data)->part
			= &lp->d_partitions[dkpart(dev)];
		return (0);

	case DIOCGSLICEINFO:
		*(struct diskslices *)data = *ssp;
		return (0);

	case DIOCSBAD:
		if (slice == WHOLE_DISK_SLICE)
			return (ENODEV);
		if (!(flags & FWRITE))
			return (EBADF);
		if (lp == NULL)
			return (EINVAL);
		if (sp->ds_bad != NULL)
			free(sp->ds_bad, M_DEVBUF);
		set_ds_bad(ssp, slice, internbad144((struct dkbad *)data, lp));
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
			error = EINVAL;
		if (error == 0) {
			if (lp->d_secperunit > sp->ds_size)
				error = ENOSPC;
			for (part = 0; part < lp->d_npartitions; part++)
				if (lp->d_partitions[part].p_size > sp->ds_size)
					error = ENOSPC;
		}
#if 0 /* XXX */
		if (error != 0 && setgeom != NULL)
			error = setgeom(lp);
#endif
		if (error != 0) {
			free(lp, M_DEVBUF);
			return (error);
		}
		free_ds_label(ssp, slice);
		set_ds_label(ssp, slice, lp);
#ifdef DEVFS
		set_ds_labeldevs(dname, dev, ssp);
#endif
		return (0);

	case DIOCSYNCSLICEINFO:
		if (slice != WHOLE_DISK_SLICE || dkpart(dev) != RAW_PART)
			return (EINVAL);
		if (!*(int *)data)
			for (slice = 0; slice < ssp->dss_nslices; slice++) {
				u_char	openmask;

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
		error = dsopen(dname, dev,
			       ssp->dss_slices[WHOLE_DISK_SLICE].ds_copenmask
			       & (1 << RAW_PART) ? S_IFCHR : S_IFBLK,
			       sspp, lp, strat, setgeom, ssp->dss_bdevsw,
			       ssp->dss_cdevsw);
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
			for (openmask = ssp->dss_slices[slice].ds_bopenmask,
			     part = 0; openmask; openmask >>= 1, part++) {
				if (!(openmask & 1))
					continue;
				error = dsopen(dname,
					       dkmodslice(dkmodpart(dev, part),
							  slice),
					       S_IFBLK, sspp, lp, strat,
					       setgeom, ssp->dss_bdevsw,
					       ssp->dss_cdevsw);
				if (error != 0) {
					/* XXX should free devfs toks. */
					free(lp, M_DEVBUF);
					/* XXX should restore devfs toks. */
					*sspp = ssp;
					return (EBUSY);
				}
			}
			for (openmask = ssp->dss_slices[slice].ds_copenmask,
			     part = 0; openmask; openmask >>= 1, part++) {
				if (!(openmask & 1))
					continue;
				error = dsopen(dname,
					       dkmodslice(dkmodpart(dev, part),
							  slice),
					       S_IFCHR, sspp, lp, strat,
					       setgeom, ssp->dss_bdevsw,
					       ssp->dss_cdevsw);
				if (error != 0) {
					/* XXX should free devfs toks. */
					free(lp, M_DEVBUF);
					/* XXX should restore devfs toks. */
					*sspp = ssp;
					return (EBUSY);
				}
			}
		}

		/* XXX devfs tokens? */
		free(lp, M_DEVBUF);
		dsgone(&ssp);
		return (0);

	case DIOCWDINFO:
		error = dsioctl(dname, dev, DIOCSDINFO, data, flags, &ssp,
				strat, setgeom);
		if (error != 0)
			return (error);
		/*
		 * XXX this used to hack on dk_openpart to fake opening
		 * partition 0 in case that is used instead of dkpart(dev).
		 */
		old_wlabel = sp->ds_wlabel;
		set_ds_wlabel(ssp, slice, TRUE);
		error = writedisklabel(dev, strat, sp->ds_label);
		/* XXX should invalidate in-core label if write failed. */
		set_ds_wlabel(ssp, slice, old_wlabel);
		return (error);

	case DIOCWLABEL:
		if (slice == WHOLE_DISK_SLICE)
			return (ENODEV);
		if (!(flags & FWRITE))
			return (EBADF);
		set_ds_wlabel(ssp, slice, *(int *)data != 0);
		return (0);

	default:
		return (-1);
	}
}

static void
dsiodone(bp)
	struct buf *bp;
{
	struct iodone_chain *ic;
	char *msg;

	ic = bp->b_iodone_chain;
	bp->b_flags = (ic->ic_prev_flags & B_CALL)
		      | (bp->b_flags & ~(B_CALL | B_DONE));
	bp->b_iodone = ic->ic_prev_iodone;
	bp->b_iodone_chain = ic->ic_prev_iodone_chain;
	if (!(bp->b_flags & B_READ)
	    || (!(bp->b_flags & B_ERROR) && bp->b_error == 0)) {
		msg = fixlabel((char *)NULL, ic->ic_args[1].ia_ptr,
			       (struct disklabel *)
			       (bp->b_data + ic->ic_args[0].ia_long),
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

char *
dsname(dname, unit, slice, part, partname)
	char	*dname;
	int	unit;
	int	slice;
	int	part;
	char	*partname;
{
	static char name[32];

	if (strlen(dname) > 16)
		dname = "nametoolong";
	sprintf(name, "%s%d", dname, unit);
	partname[0] = '\0';
	if (slice != WHOLE_DISK_SLICE || part != RAW_PART) {
		partname[0] = 'a' + part;
		partname[1] = '\0';
		if (slice != COMPATIBILITY_SLICE)
			sprintf(name + strlen(name), "s%d", slice - 1);
	}
	return (name);
}

/*
 * This should only be called when the unit is inactive and the strategy
 * routine should not allow it to become active unless we call it.  Our
 * strategy routine must be special to allow activity.
 */
int
dsopen(dname, dev, mode, sspp, lp, strat, setgeom, bdevsw, cdevsw)
	char	*dname;
	dev_t	dev;
	int	mode;
	struct diskslices **sspp;
	struct disklabel *lp;
	d_strategy_t *strat;
	ds_setgeom_t *setgeom;
	struct bdevsw *bdevsw;
	struct cdevsw *cdevsw;
{
	struct dkbad *btp;
	dev_t	dev1;
	int	error;
	struct disklabel *lp1;
	char	*msg;
	u_char	mask;
#ifdef DEVFS
	int	mynor;
#endif
	bool_t	need_init;
	int	part;
	char	partname[2];
	int	slice;
	char	*sname;
	struct diskslice *sp;
	struct diskslices *ssp;
	int	unit;

	/*
	 * XXX reinitialize the slice table unless there is an open device
	 * on the unit.  This should only be done if the media has changed.
	 */
	ssp = *sspp;
	need_init = !dsisopen(ssp);
	if (ssp != NULL && need_init)
		dsgone(sspp);
	if (need_init) {
		TRACE(("dsinit\n"));
		error = dsinit(dname, dev, strat, lp, sspp);
		if (error != 0) {
			dsgone(sspp);
			return (error);
		}
		lp->d_npartitions = MAXPARTITIONS;
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		ssp = *sspp;
#ifdef DEVFS
		ssp->dss_bdevsw = bdevsw;
		ssp->dss_cdevsw = cdevsw;
#endif

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

		lp1 = malloc(sizeof *lp1, M_DEVBUF, M_WAITOK);
		*lp1 = *lp;

		/*
		 * Initialize defaults for the label for the whole disk so
		 * that it can be used as a template for disklabel(8).
		 * d_rpm = 3600 is unlikely to be correct for a modern
		 * disk, but d_rpm is normally irrelevant.
		 */
		if (lp1->d_rpm == 0)
			lp1->d_rpm = 3600;
		if (lp1->d_interleave == 0)
			lp1->d_interleave = 1;
		if (lp1->d_bbsize == 0)
			lp1->d_bbsize = BBSIZE;
		if (lp1->d_sbsize == 0)
			lp1->d_sbsize = SBSIZE;

		ssp->dss_slices[WHOLE_DISK_SLICE].ds_label = lp1;
		ssp->dss_slices[WHOLE_DISK_SLICE].ds_wlabel = TRUE;
		if (setgeom != NULL) {
			error = setgeom(lp);
			if (error != 0) {
				dsgone(sspp);
				return (error);
			}
		}
	}

	unit = dkunit(dev);

	/*
	 * Initialize secondary info for all slices.  It is needed for more
	 * than the current slice in the DEVFS case.
	 */
	for (slice = 0; slice < ssp->dss_nslices; slice++) {
		sp = &ssp->dss_slices[slice];
		if (sp->ds_label != NULL)
			continue;
		dev1 = dkmodslice(dkmodpart(dev, RAW_PART), slice);
		sname = dsname(dname, unit, slice, RAW_PART, partname);
#ifdef DEVFS
		if (slice != COMPATIBILITY_SLICE && sp->ds_bdev == NULL
		    && sp->ds_size != 0) {
			mynor = minor(dev1);
			sp->ds_bdev =
				devfs_add_devswf(bdevsw, mynor, DV_BLK,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "%s", sname);
			sp->ds_cdev =
				devfs_add_devswf(cdevsw, mynor, DV_CHR,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "r%s", sname);
		}
#endif
		/*
		 * XXX this should probably only be done for the need_init
		 * case, but there may be a problem with DIOCSYNCSLICEINFO.
		 */
		set_ds_wlabel(ssp, slice, TRUE);	/* XXX invert */
		lp1 = malloc(sizeof *lp1, M_DEVBUF, M_WAITOK);
		*lp1 = *lp;
		TRACE(("readdisklabel\n"));
		msg = readdisklabel(dev1, strat, lp1);
#if 0 /* XXX */
		if (msg == NULL && setgeom != NULL && setgeom(lp1) != 0)
			msg = "setgeom failed";
#endif
		if (msg == NULL)
			msg = fixlabel(sname, sp, lp1, FALSE);
		if (msg != NULL) {
			free(lp1, M_DEVBUF);
			if (sp->ds_type == DOSPTYP_386BSD /* XXX */)
				log(LOG_WARNING, "%s: cannot find label (%s)\n",
				    sname, msg);
			continue;
		}
		if (lp1->d_flags & D_BADSECT) {
			btp = malloc(sizeof *btp, M_DEVBUF, M_WAITOK);
			TRACE(("readbad144\n"));
			msg = readbad144(dev1, strat, lp1, btp);
			if (msg != NULL) {
				log(LOG_WARNING,
				    "%s: cannot find bad sector table (%s)\n",
				    sname, msg);
				free(btp, M_DEVBUF);
				free(lp1, M_DEVBUF);
				continue;
			}
			set_ds_bad(ssp, slice, internbad144(btp, lp1));
			free(btp, M_DEVBUF);
			if (sp->ds_bad == NULL) {
				free(lp1, M_DEVBUF);
				continue;
			}
		}
		set_ds_label(ssp, slice, lp1);
#ifdef DEVFS
		set_ds_labeldevs(dname, dev1, ssp);
#endif
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
	switch (mode) {
	case S_IFBLK:
		sp->ds_bopenmask |= mask;
		break;
	case S_IFCHR:
		sp->ds_copenmask |= mask;
		break;
	}
	sp->ds_openmask = sp->ds_bopenmask | sp->ds_copenmask;
	return (0);
}

int
dssize(dev, sspp, dopen, dclose)
	dev_t	dev;
	struct diskslices **sspp;
	d_open_t dopen;
	d_close_t dclose;
{
	struct disklabel *lp;
	int	part;
	int	slice;
	struct diskslices *ssp;

	slice = dkslice(dev);
	part = dkpart(dev);
	ssp = *sspp;
	if (ssp == NULL || slice >= ssp->dss_nslices
	    || !(ssp->dss_slices[slice].ds_bopenmask & (1 << part))) {
		if (dopen(dev, FREAD, S_IFBLK, (struct proc *)NULL) != 0)
			return (-1);
		dclose(dev, FREAD, S_IFBLK, (struct proc *)NULL);
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
#ifdef DEVFS
	free_ds_labeldevs(ssp, slice);
	if (slice == COMPATIBILITY_SLICE)
		free_ds_labeldevs(ssp, ssp->dss_first_bsd_slice);
	else if (slice == ssp->dss_first_bsd_slice)
		free_ds_labeldevs(ssp, COMPATIBILITY_SLICE);
#endif
	free(lp, M_DEVBUF);
	set_ds_label(ssp, slice, (struct disklabel *)NULL);
}

#ifdef DEVFS
static void
free_ds_labeldevs(ssp, slice)
	struct diskslices *ssp;
	int	slice;
{
	struct disklabel *lp;
	int	part;
	struct diskslice *sp;

	sp = &ssp->dss_slices[slice];
	lp = sp->ds_label;
	if (lp == NULL)
		return;
	for (part = 0; part < lp->d_npartitions; part++) {
		if (sp->ds_bdevs[part] != NULL) {
			devfs_remove_dev(sp->ds_bdevs[part]);
			sp->ds_bdevs[part] = NULL;
		}
		if (sp->ds_cdevs[part] != NULL) {
			devfs_remove_dev(sp->ds_cdevs[part]);
			sp->ds_cdevs[part] = NULL;
		}
	}
}
#endif

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
	       pp->p_offset, pp->p_offset + pp->p_size - 1, pp->p_size);
}

static void
slice_info(sname, sp)
	char	*sname;
	struct diskslice *sp;
{
	printf("%s: start %lu, end %lu, size %lu\n", sname,
	       sp->ds_offset, sp->ds_offset + sp->ds_size - 1, sp->ds_size);
}

/*
 * Most changes to ds_bad, ds_label and ds_wlabel are made using the
 * following functions to ensure coherency of the compatibility slice
 * with the first BSD slice.  The openmask fields are _not_ shared and
 * the other fields (ds_offset and ds_size) aren't changed after they
 * are initialized.
 */
static void
set_ds_bad(ssp, slice, btp)
	struct diskslices *ssp;
	int	slice;
	struct dkbad_intern *btp;
{
	ssp->dss_slices[slice].ds_bad = btp;
	if (slice == COMPATIBILITY_SLICE)
		ssp->dss_slices[ssp->dss_first_bsd_slice].ds_bad = btp;
	else if (slice == ssp->dss_first_bsd_slice)
		ssp->dss_slices[COMPATIBILITY_SLICE].ds_bad = btp;
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

#ifdef DEVFS
static void
set_ds_labeldevs(dname, dev, ssp)
	char	*dname;
	dev_t	dev;
	struct diskslices *ssp;
{
	int	slice;

	set_ds_labeldevs_unaliased(dname, dev, ssp);
	if (ssp->dss_first_bsd_slice == COMPATIBILITY_SLICE)
		return;
	slice = dkslice(dev);
	if (slice == COMPATIBILITY_SLICE)
		set_ds_labeldevs_unaliased(dname,
			dkmodslice(dev, ssp->dss_first_bsd_slice), ssp);
	else if (slice == ssp->dss_first_bsd_slice)
		set_ds_labeldevs_unaliased(dname,
			dkmodslice(dev, COMPATIBILITY_SLICE), ssp);
}

static void
set_ds_labeldevs_unaliased(dname, dev, ssp)
	char	*dname;
	dev_t	dev;
	struct diskslices *ssp;
{
	struct disklabel *lp;
	int	mynor;
	int	part;
	char	partname[2];
	struct partition *pp;
	int	slice;
	char	*sname;
	struct diskslice *sp;
 
	slice = dkslice(dev);
	sp = &ssp->dss_slices[slice];
	if (sp->ds_size == 0)
		return;
	lp = sp->ds_label;
	for (part = 0; part < lp->d_npartitions; part++) {
		pp = &lp->d_partitions[part];
		if (pp->p_size == 0)
			continue;
		sname = dsname(dname, dkunit(dev), slice, part, partname);
		if (part == RAW_PART && sp->ds_bdev != NULL) {
			sp->ds_bdevs[part] =
				devfs_link(sp->ds_bdev,
					   "%s%s", sname, partname);
			sp->ds_cdevs[part] =
				devfs_link(sp->ds_cdev,
					   "r%s%s", sname, partname);
		} else {
			mynor = minor(dkmodpart(dev, part));
			sp->ds_bdevs[part] =
				devfs_add_devswf(ssp->dss_bdevsw, mynor, DV_BLK,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "%s%s", sname, partname);
			sp->ds_cdevs[part] =
				devfs_add_devswf(ssp->dss_cdevsw, mynor, DV_CHR,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "r%s%s", sname, partname);
		}
	}
}
#endif /* DEVFS */

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
