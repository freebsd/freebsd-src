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
#include <sys/diskslice.h>
#include <sys/dkbad.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#define b_cylinder	b_resid

#define	FALSE	0
#define	TRUE	1
typedef	u_char	bool_t;

static char *fixlabel __P((char *dname, int unit, int slice,
			   struct diskslice *sp, struct disklabel *lp));
static void partition_info __P((char *dname, int unit, int slice,
				int part, struct partition *pp));
static void slice_info __P((char *dname, int unit, int slice,
			    struct diskslice *sp));

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
	struct partition *pp;
	struct diskslice *sp;
	long	sz;

	sp = &ssp->dss_slices[dkslice(bp->b_dev)];
	lp = sp->ds_label;
	sz = (bp->b_bcount + DEV_BSIZE - 1) >> DEV_BSHIFT;
	if (lp == NULL) {
		blkno = bp->b_blkno;
		labelsect = -LABELSECTOR - 1;
		maxsz = sp->ds_size;
	} else {
		labelsect = lp->d_partitions[LABEL_PART].p_offset;
if (labelsect != 0) Debugger("");
		pp = &lp->d_partitions[dkpart(bp->b_dev)];
		blkno = pp->p_offset + bp->b_blkno;
		maxsz = pp->p_size;
		if (sp->ds_bad != NULL) {
			daddr_t	newblkno;

			newblkno = transbad144(sp->ds_bad, blkno);
			if (newblkno != blkno)
				printf("bad block %lu -> %lu\n",
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
			sp->ds_bad = NULL;
		}
		if (sp->ds_label != NULL) {
			free(sp->ds_label, M_DEVBUF);
			sp->ds_label = NULL;
		}
	}
	*sspp = NULL;
}

/*
 * For the "write" commands (DIOCSBAD, DIOCSDINFO and DIOCWDINFO), this
 * is subject to the same restriction as dsopen().
 */
int
dsioctl(dev, cmd, data, flags, ssp, strat, setgeom)
	dev_t	dev;
	int	cmd;
	caddr_t	data;
	int	flags;
	struct diskslices *ssp;
	d_strategy_t *strat;
	ds_setgeom_t *setgeom;
{
	int	error;
	struct disklabel *lp;
	int	old_wlabel;
	struct diskslice *sp;

	sp = &ssp->dss_slices[dkslice(dev)];
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

	case DIOCSBAD:
		if (!(flags & FWRITE))
			return (EBADF);
		if (lp == NULL)
			return (EINVAL);
		if (sp->ds_bad != NULL)
			free(sp->ds_bad, M_DEVBUF);
		sp->ds_bad = internbad144((struct dkbad *)data, lp);
		return (0);

	case DIOCSDINFO:
		if (!(flags & FWRITE))
			return (EBADF);
		lp = malloc(sizeof *lp, M_DEVBUF, M_WAITOK);
		error = setdisklabel(lp, (struct disklabel *)data,
				     sp->ds_label != NULL
				     ? sp->ds_openmask : (u_long)0);
#if 0 /* XXX */
		if (error != 0 && setgeom != NULL)
			error = setgeom(lp);
#endif
		if (error != 0) {
			free(lp, M_DEVBUF);
			return (error);
		}
		if (sp->ds_label != NULL)
			free(sp->ds_label, M_DEVBUF);
		sp->ds_label = lp;
		return (0);

	case DIOCWDINFO:
		error = dsioctl(dev, DIOCSDINFO, data, flags, ssp, strat,
				setgeom);
		if (error != 0)
			return (error);
		/*
		 * XXX this used to hack on dk_openpart to fake opening
		 * partition 0 in case that is used instead of dkpart(dev).
		 */
		old_wlabel = sp->ds_wlabel;
		sp->ds_wlabel = TRUE;
		error = correct_writedisklabel(dev, strat, sp->ds_label);
		sp->ds_wlabel = old_wlabel;
		return (error);

	case DIOCWLABEL:
		if (!(flags & FWRITE))
			return (EBADF);
		sp->ds_wlabel = (*(int *)data != 0);
		return (0);

	default:
		return (-1);
	}
}

/*
 * This should only be called when the unit is inactive and the strategy
 * routine should not allow it to become active unless we call it.  Our
 * strategy routine must be special to allow activity.
 */
int
dsopen(dname, dev, mode, sspp, lp, strat, setgeom)
	char	*dname;
	dev_t	dev;
	int	mode;
	struct diskslices **sspp;
	struct disklabel *lp;
	d_strategy_t *strat;
	ds_setgeom_t *setgeom;
{
	int	error;
	char	*msg;
	u_char	mask;
	bool_t	need_init;
	int	part;
	int	slice;
	struct diskslice *sp;
	struct diskslices *ssp;
	int	unit;

	need_init = TRUE;
	if (*sspp != NULL) {
		/*
		 * XXX reinitialize the slice table unless there is an open
		 * device on the unit.  This should only be done if the media
		 * has changed.
		 */
		for (slice = 0, ssp = *sspp; slice < ssp->dss_nslices; slice++)
			if (ssp->dss_slices[slice].ds_openmask) {
				need_init = FALSE;
				break;
			}
			if (need_init)
				dsgone(sspp);
	}
	if (need_init) {
		printf("dsinit\n");
		error = dsinit(dname, dev, strat, lp, sspp);
		if (error != 0)
			return (error);
		if (setgeom != NULL) {
			error = setgeom(lp);
			if (error != 0)
				/* XXX clean up? */
				return (error);
		}
	}
	ssp = *sspp;
	slice = dkslice(dev);
	if (slice >= ssp->dss_nslices)
		return (ENXIO);
	sp = &ssp->dss_slices[slice];
	part = dkpart(dev);
	unit = dkunit(dev);
	if (slice != WHOLE_DISK_SLICE && sp->ds_label == NULL) {
		struct disklabel *lp1;

		lp1 = malloc(sizeof *lp1, M_DEVBUF, M_WAITOK);
		*lp1 = *lp;
		lp = lp1;	
		printf("readdisklabel\n");
		msg = correct_readdisklabel(dkmodpart(dev, RAW_PART), strat, lp);
#if 0 /* XXX */
		if (msg == NULL && setgeom != NULL && setgeom(lp) != 0)
			msg = "setgeom failed";
#endif
		if (msg == NULL)
			msg = fixlabel(dname, unit, slice, sp, lp);
		if (msg != NULL) {
			free(lp, M_DEVBUF);
			log(LOG_WARNING, "%s%ds%d: cannot find label (%s)\n",
			    dname, unit, slice, msg);
			if (part == RAW_PART)
				goto out;
			return (EINVAL);	/* XXX needs translation */
		}
		if (lp->d_flags & D_BADSECT) {
			struct dkbad *btp;

			btp = malloc(sizeof *btp, M_DEVBUF, M_WAITOK);
			printf("readbad144\n");
			msg = readbad144(dev, strat, lp, btp);
			if (msg != NULL) {
				log(LOG_WARNING,
				"%s%ds%d: cannot find bad sector table (%s)\n",
				    dname, unit, slice, msg);
				free(btp, M_DEVBUF);
				free(lp, M_DEVBUF);
				if (part == RAW_PART)
					goto out;
				return (EINVAL);  /* XXX needs translation */
			}
			sp->ds_bad = internbad144(btp, lp);
			free(btp, M_DEVBUF);
			if (sp->ds_bad == NULL) {
				free(lp, M_DEVBUF);
				if (part == RAW_PART)
					goto out;
				return (EINVAL);  /* XXX needs translation */
			}
		}
		sp->ds_label = lp;
		sp->ds_wlabel = TRUE;
	}
	if (part != RAW_PART
	    && (sp->ds_label == NULL || part >= sp->ds_label->d_npartitions))
		return (EINVAL);	/* XXX needs translation */
out:
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

static char *
fixlabel(dname, unit, slice, sp, lp)
	char	*dname;
	int	unit;
	int	slice;
	struct diskslice *sp;
	struct disklabel *lp;
{
	bool_t	adjust;
	int	bsdpart;
	struct partition *pp;
	bool_t	warned;

	pp = &lp->d_partitions[LABEL_PART];
	if (pp->p_offset != 0 && pp->p_offset != sp->ds_offset) {
		printf(
"%s%ds%d: rejecting BSD label: label partition start != 0 and != slice start\n",
		       dname, unit, slice);
		slice_info(dname, unit, slice, sp);
		partition_info(dname, unit, slice, LABEL_PART, pp);
		return ("invalid partition table");
	}
	if (pp->p_size != sp->ds_size) {
		printf("%s%ds%d: label partition size != slice size\n",
		       dname, unit, slice);
		slice_info(dname, unit, slice, sp);
		partition_info(dname, unit, slice, LABEL_PART, pp);
		if (pp->p_size > sp->ds_size) {
			printf("%s%ds%d: truncating label partition\n",
			       dname, unit, slice);
			pp->p_size = sp->ds_size;
		}
	}
	adjust = FALSE;
	warned = FALSE;
	if (pp->p_offset != 0) {
		printf("%s%ds%d: adjusting offsets in BSD label\n",
		       dname, unit, slice);
		slice_info(dname, unit, slice, sp);
		partition_info(dname, unit, slice, LABEL_PART, pp);
		adjust = TRUE;
		warned = TRUE;
	}
	pp -= LABEL_PART;
	for (bsdpart = 0; bsdpart < lp->d_npartitions; bsdpart++, pp++) {
		if (adjust && (pp->p_offset != 0 || pp->p_size != 0))
			pp->p_offset -= sp->ds_offset;
		if (pp->p_offset + pp->p_size > sp->ds_size
		    || pp->p_offset + pp->p_size < pp->p_offset) {
			printf(
"%s%ds%d: rejecting partition in BSD label: it isn't entirely within the slice\n",
			       dname, unit, slice);
			if (!warned) {
				slice_info(dname, unit, slice, sp);
				warned = TRUE;
			}
			if (adjust)
				pp->p_offset += sp->ds_offset;
			partition_info(dname, unit, slice, bsdpart, pp);
			bzero(pp, sizeof *pp);
		}
	}

	/* XXX TODO: fix general params in *lp? */

	return (NULL);
}

static void
partition_info(dname, unit, slice, part, pp)
	char	*dname;
	int	unit;
	int	slice;
	int	part;
	struct partition *pp;
{
	printf("%s%ds%d%c: start %lu, end %lu, size %lu\n",
	       dname, unit, slice, 'a' + part,
	       pp->p_offset, pp->p_offset + pp->p_size - 1, pp->p_size);
}

static void
slice_info(dname, unit, slice, sp)
	char	*dname;
	int	unit;
	int	slice;
	struct diskslice *sp;
{
	printf("%s%ds%d: start %lu, end %lu, size %lu\n",
	       dname, unit, slice,
	       sp->ds_offset, sp->ds_offset + sp->ds_size - 1, sp->ds_size);
}
