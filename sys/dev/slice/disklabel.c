/*-
 * Copyright (C) 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 *	$Id: disklabel.c,v 1.3 1998/04/22 19:27:51 julian Exp $
 */
#define BAD144

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/dkstat.h>
#ifdef BAD144
#include <sys/dkbad.h>
#endif
#include <sys/malloc.h>
#include <dev/slice/slice.h>

#include <sys/conf.h>
#include <sys/sliceio.h>
#include <sys/syslog.h>


struct private_data {
	u_int32_t	 flags;
	u_int8_t	 rflags;
	u_int8_t	 wflags;
	int		 savedoflags;
	struct slice	*slice_down;
	struct disklabel disklabel;
	struct subdev {
		int			 part;
		struct slice		*slice;
		struct slicelimits	 limit;
		struct private_data	*pd;
		u_int32_t       	 offset; /*  all disklabel supports */
	}               subdevs[MAXPARTITIONS];
#ifdef BAD144
	struct dkbad_intern *bad;
#endif
};

static sl_h_constructor_t dkl_constructor;	/* constructor (from device) */
static sl_h_IO_req_t dkl_IOreq;	/* IO req downward (to device) */
static sl_h_ioctl_t dkl_ioctl;	/* ioctl req downward (to device) */
static sl_h_open_t dkl_open;	/* downwards travelling open */
/*static sl_h_close_t dkl_close; */	/* downwards travelling close */
static sl_h_claim_t dkl_claim;	/* upwards travelling claim */
static sl_h_revoke_t dkl_revoke;/* upwards travelling revokation */
static sl_h_verify_t dkl_verify;/* things changed, are we stil valid? */
static sl_h_upconfig_t dkl_upconfig;/* config requests from below */
static sl_h_dump_t dkl_dump;	/* core dump req downward */

static struct slice_handler slicetype = {
	"disklabel",
	0,
	NULL,
	0,
	&dkl_constructor,	/* constructor */
	&dkl_IOreq,
	&dkl_ioctl,
	&dkl_open,
	/*&dkl_close*/NULL,
	&dkl_revoke,		/* revoke */
	&dkl_claim,		/* claim */
	&dkl_verify,		/* verify */
	&dkl_upconfig,		/* subslice manipulation */
	&dkl_dump
};

static void
sd_drvinit(void *unused)
{
	sl_newtype(&slicetype);
}

SYSINIT(sddev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, sd_drvinit, NULL);

/*-
 * Given a slice, extract out our table of information
 */
/*-
 * Attempt to read a disk label from a slice.
 * The label must be partly set up before this: secpercyl, secsize
 * and anything required in the strategy routine (e.g., dummy bounds for the
 * partition containing the label) must be filled in before calling us.
 * Returns NULL on success and an error string on failure.
 */
static int
dkl_extract_table(sl_p slice, struct disklabel * lp)
{
	int             error = EINVAL;
	struct buf     *bp;
	struct disklabel *dlp;
	struct partition *dp;
	int             part;
	int		slice_offset; /* XXX */

	RR;
	/* start off with a known result */
	bzero(lp, sizeof(*lp));
	if (error = slice_readblock(slice, LABELSECTOR, &bp))
		return (error);
	/*
	 * Step through the block looking for the label.
	 * It may not be at the front (Though I have never seen this).
	 * When found, copy it to the destination supplied.
	 */
	error = EINVAL;
	for (dlp = (struct disklabel *) bp->b_data;
	    dlp <= (struct disklabel *) ((char *) bp->b_data
					+ slice->limits.blksize
					- sizeof(*dlp));
	    dlp = (struct disklabel *) ((char *) dlp + sizeof(long))) {
		if ((dlp->d_magic != DISKMAGIC) ||
		    (dlp->d_magic2 != DISKMAGIC) ||
		    (dlp->d_npartitions > MAXPARTITIONS) ||
		    dkcksum(dlp))
			continue;
		error = 0;
		bcopy(dlp, lp, sizeof(*lp));
		/*
		 * disklabels are done relative to the base of the disk,
		 * rather than the local partition, (DUH!)
		 * so use partition 2 (c) to get the base,
		 * and subtract it from all non-0 offsets.
		 */
		dp = lp->d_partitions;
		slice_offset = dp[2].p_offset;
		for (part = 0; part < MAXPARTITIONS; part++, dp++) {
			/*
			 * We could be reloading, in which case skip
			 * entries already set up.
			 */
			if (dp->p_size == 0)
				continue;
			if( dp->p_offset < slice_offset ) {
				printf("slice before 'c'\n");
				dp->p_size = 0;
				continue;
			}
			dp->p_offset -= slice_offset;
		}
		break;
	}

done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (error);
}
/*
 * given a table, write it to disk.
 */
static int
dkl_insert_table(sl_p slice, struct disklabel * lp)
{
	int             error = EINVAL;
	struct buf     *bp;
	struct disklabel *dlp;
	struct partition *dp;
	int             part;
	int		slice_offset; /* XXX */

	RR;
	/* start off with a known result */
	if (error = slice_readblock(slice, LABELSECTOR, &bp))
		return (error);
	/*
	 * Step through the block looking for the label.
	 * It may not be at the front (Though I have never seen this).
	 * When found, replace it witht he new one. 
	 */
	error = EINVAL;
	for (dlp = (struct disklabel *) bp->b_data;
	    dlp <= (struct disklabel *) ((char *) bp->b_data
					+ slice->limits.blksize
					- sizeof(*dlp));
	    dlp = (struct disklabel *) ((char *) dlp + sizeof(long))) {
		if ((dlp->d_magic != DISKMAGIC) ||
		    (dlp->d_magic2 != DISKMAGIC) ||
		    (dlp->d_npartitions > MAXPARTITIONS) ||
		    dkcksum(dlp))
			continue;
		error = 0;
	}
	if (error) {
		/*
		 * We didn't find one..
		 * so clear the block and place the new disklabel
		 * at the start.
		 */
		bzero(bp->b_data, slice->limits.blksize);
		dlp = (struct disklabel *) bp->b_data;
	}
	/*
	 * old disklabels are done relative to the base of the disk,
	 * rather than the local partition, (DUH!)
	 * so use partition 2 (c) to get the base,
	 * and subtract it from all non-0 offsets.
	 */
	dp = dlp->d_partitions;
	slice_offset = dp[2].p_offset;
	bcopy(lp, dlp, sizeof(*lp));
	slice_offset -= dp[2].p_offset; /* size we adjust by? */
	for (part = 0; part < MAXPARTITIONS; part++, dp++) {
		if (dp->p_size == 0)
			continue;
		dp->p_offset += slice_offset;
	}
	error = slice_writeblock(slice, LABELSECTOR, bp);
quit:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	return (error);
}

#ifdef BAD144
static int
dkl_internbad144(struct private_data *pd, struct dkbad *btp, int flag)
{
	struct disklabel *lp = &pd->disklabel;
	struct dkbad_intern *bip = pd->bad;
	int i;

	if (bip == NULL) {
		bip = malloc(sizeof *bip, M_DEVBUF, flag);
		if (bip == NULL)
			return (ENOMEM);
		pd->bad = bip;
	}
	/*
	 * Spare sectors are allocated beginning with the last sector of
	 * the second last track of the disk (the last track is used for
	 * the bad sector list).
	 */
	bip->bi_maxspare = lp->d_secperunit - lp->d_nsectors - 1;
	bip->bi_nbad = DKBAD_MAXBAD;
	for (i=0; i < DKBAD_MAXBAD && btp->bt_bad[i].bt_cyl != DKBAD_NOCYL; i++)
		bip->bi_bad[i] = btp->bt_bad[i].bt_cyl * lp->d_secpercyl
				 + (btp->bt_bad[i].bt_trksec >> 8)
				   * lp->d_nsectors
				 + (btp->bt_bad[i].bt_trksec & 0x00ff);
	bip->bi_bad[i] = -1;
#if 1
	for (i = 0; i < DKBAD_MAXBAD && bip->bi_bad[i] != -1; i++)
		printf("  %8d => %8d\n", bip->bi_bad[i],
			bip->bi_maxspare - i);
#endif
	return (0);
}

static int
dkl_readbad144(struct private_data *pd)
{
	sl_p slice = pd->slice_down;
	struct disklabel *lp = &pd->disklabel;
	struct dkbad *db;
	struct buf *bp;
	int blkno, i, error;

	for (i = 0; i < min(10, lp->d_nsectors); i += 2) {
		blkno = lp->d_secperunit - lp->d_nsectors + i;
		if (lp->d_secsize > slice->limits.blksize)
			blkno *= lp->d_secsize / slice->limits.blksize;
		else
			blkno /= slice->limits.blksize / lp->d_secsize;
		error = slice_readblock(slice, blkno, &bp);
		if (error)
			return (error);
		bp->b_flags |= B_INVAL | B_AGE;
		db = (struct dkbad *)bp->b_data;
		if (db->bt_mbz == 0 && db->bt_flag == DKBAD_MAGIC) {
			printf(" bad144 table found at block %d\n", blkno);
			error = dkl_internbad144(pd, db, M_NOWAIT);
			brelse(bp);
			return (error);
		}
		brelse(bp);
	}
	return (EINVAL);
}

static __inline daddr_t
dkl_transbad144(struct private_data *pd, daddr_t blkno)
{
	return transbad144(pd->bad, blkno);
}
#endif

/*-
 * look at a slice and figure out if we should be interested in it. (Is it
 * ours?)
 */
static int
dkl_claim(struct slice * slice, struct slice * lower, void *ID)
{
	struct disklabel disklabel;
	struct disklabel *dl, *dl0;
	int	error;
	RR;

	/*-
 	 * Try load a valid disklabel table.
	 * This is 90% of what we need to check.
	 */
	if ((error = dkl_extract_table(slice, &disklabel)) != 0) {
		return (error);
	}
	/*-
	 * If there is no geometry info, extract it from the label
	 * as some drivers need this.
	 */
	/* XXX */

	/*-
	 * well, it looks like one of ours.
	 */
	return (0);
}

/*-
 * This is a special HACK function for the IDE driver.
 * It is here because everything it need is in scope here,
 * but it is not really part of the SLICE code.
 * Because old ESDI drives could not tell their geometry, They need
 * to get it from the MBR or the disklabel. This is the disklabel bit.
 */
int
dkl_geom_hack(struct slice * slice, struct ide_geom *geom)
{
	struct disklabel disklabel;
	struct disklabel *dl, *dl0;
	int	error;
	RR;

	/* first check it's a disklabel*/
	if ((error = dkl_claim (slice, NULL, 0)))
		return (error);
	/*-
 	 * Try load a valid disklabel table.
	 * This is wasteful but never called on new (< 5 YO ) drives.
	 */
	if ((error = dkl_extract_table(slice, &disklabel)) != 0) {
		return (error);
	}
	geom->secpertrack = disklabel. d_nsectors;
	geom->trackpercyl = disklabel.d_ntracks;
	geom->cyls = disklabel.d_ncylinders;
	return (0);
}

/*-
 * look at a slice we know to be ours and decide what the #$%^ to do with it.
 */
static int
dkl_constructor(sl_p slice)
{
	int             i;
	u_int64_t       disksize = slice->limits.slicesize;
	struct private_data *pd;
	struct partition *dp, *dp0;
	struct disklabel *dl;
	sh_p		tp;
	char            name[64];

	int             part;
	int             error = 0;
	u_long          dkl_offset;

	RR;
	/*-
	 * If we are being called to re-load a slice,
	 * then don't reallocate resources.
	 */
	if ((pd = slice->private_up) == NULL) {
		if (slice->name == NULL) {
			printf("name is NULL\n");
			return (EINVAL);
		}
		if (strlen(slice->name) > 58) {
			printf("slice: name %s too long\n", slice->name);
			return (ENAMETOOLONG);
		}
		pd = malloc(sizeof(*pd), M_DEVBUF, M_NOWAIT);
		if (pd == NULL) {
			printf("fdisk: failed malloc\n");
			return (ENOMEM);
		}
		bzero(pd, sizeof(*pd));
		pd->slice_down = slice;
		if ((error = dkl_extract_table(slice, &pd->disklabel)) != 0) {
			struct partinfo data;
			/* 
			 * If it's just that there is no disklabel there,
			 * Then we fake one up and write it. if this were
			 * not ok, then we would have not been called.
			 * (as probe will have failed). If it's 
			 * a physical error, then that's reason to fail.
			 */
			if (error != EINVAL) {
				free(pd, M_DEVBUF);
				return (error);
			}
			dkl_dummy_ioctl(slice, DIOCGPART,
					(caddr_t) &data, 0, NULL);
			bcopy(data.disklab, &pd->disklabel,
					sizeof(pd->disklabel));
			if ((error = dkl_insert_table(slice, &pd->disklabel))) {
				free(pd, M_DEVBUF);
				return (error);
			}
		}
#ifdef BAD144
		if (pd->disklabel.d_flags & D_BADSECT) {
			if ((error = dkl_readbad144(pd))) {
				free(pd, M_DEVBUF);
				return (error);
			}
		}
#endif
		slice->refs++;
		slice->handler_up = &slicetype;
		slice->private_up = pd;
		slicetype.refs++;
	}
	dl = &pd->disklabel;
	dp0 = dl->d_partitions;

	/*-
	 * Handle each of the partitions.
	 * We should check that each makes sence and is legal.
	 * 1/ it should not already have a slice.
	 * 2/ should not be 0 length.
	 * 3/ should not go past end of our slice.
	 * 4/ should not overlap other slices.
	 *  It can include sector 0 (unfortunatly)
	 */
	dp = dp0;
	for (part = 0; part < MAXPARTITIONS; part++, dp++) {
		int             i;
		if ( part == 2 )
			continue; /* XXX skip the 'c' partition */
		/*
		 * We could be reloading, in which case skip
		 * entries already set up.
		 */
		if (pd->subdevs[part].slice != NULL)
	breakout:	continue;
		/*
		 * also skip partitions not present
		 */
		if (dp->p_size == 0)
			continue;
printf(" part %d, start=%d, size=%d\n", part, dp->p_offset, dp->p_size);

		if ((dp->p_offset + dp->p_size) >
		    (slice->limits.slicesize / slice->limits.blksize)) {
			printf("dkl: slice %d too big ", part);
			printf("(%x > %x:%x )\n",
			    (dp->p_offset + dp->p_size),
			    (slice->limits.slicesize / slice->limits.blksize) );
			continue;
		}
		/* check for overlaps with existing slices */
		for (i = 0; i < MAXPARTITIONS; i++) {
			/*
			 * Don't bother if that slice was not made.
			 * This handles the (i == part) case.
			 */
			if (pd->subdevs[i].slice == NULL)
				continue;
			if ((dp0[i].p_offset < (dp->p_offset + dp->p_size))
			    && ((dp0[i].p_offset + dp0[i].p_size) > dp->p_offset)) {
				printf("dkl: slice %d overlaps slice %d\n",
				       part, i);
				goto breakout;
			}
		}
		/*-
		 * the slice seems to make sense. Use it.
		 */
		pd->subdevs[part].part = part;
		pd->subdevs[part].pd = pd;
		pd->subdevs[part].offset = dp->p_offset;
		pd->subdevs[part].limit.blksize
			= slice->limits.blksize;
		pd->subdevs[part].limit.slicesize
			= (slice->limits.blksize * (u_int64_t)dp->p_size);

		sprintf(name, "%s%c", slice->name, (char )('a' + part));
		sl_make_slice(&slicetype,
			      &pd->subdevs[part],
			      &pd->subdevs[part].limit,
			      &pd->subdevs[part].slice,
			      NULL,
			      name);
		pd->subdevs[part].slice->probeinfo.typespecific = &dp->p_fstype;
		switch (dp->p_fstype) {
		case	FS_UNUSED:
			/* allow unuseed to be further split */
			pd->subdevs[part].slice->probeinfo.type = NULL;
			break;
		case	FS_V6:
		case	FS_V7:
		case	FS_SYSV:
		case	FS_V71K:
		case	FS_V8:
		case	FS_MSDOS:
		case	FS_BSDLFS:
		case	FS_OTHER:
		case	FS_HPFS:
		case	FS_ISO9660:
		case	FS_BOOT	:
#if 0
			printf("%s: type %d. Leaving\n",
				pd->subdevs[part].slice->name,
				(u_int)dp->p_fstype);
#endif
		case	FS_SWAP:
		case	FS_BSDFFS:
			pd->subdevs[part].slice->probeinfo.type = NO_SUBPART;
			break;
		default:
			pd->subdevs[part].slice->probeinfo.type = NULL;
		}
		/* 
		 * Dont allow further breakup of slices that
		 * cover our disklabel
		 */
		if (dp->p_offset < 16) {
#if 0
			printf("%s: covers disklabel. Leaving\n",
				pd->subdevs[part].slice->name);
#endif
			pd->subdevs[part].slice->probeinfo.type = NO_SUBPART;
		}
		if ((tp = slice_probeall(pd->subdevs[part].slice)) != NULL) {
			(*tp->constructor)(pd->subdevs[part].slice);
		}
	}
	return (error);
}

/*-
 * look at a slice that USED to be ours.
 * decide if any sub-slices need to be revoked.
 * If not then at least ask them to verify themselves.
 */
static int
dkl_verify(sl_p slice)
{
	register struct private_data *pd;
	struct disklabel label;
	struct partition *dp, *dp2;
	struct disklabel *dl;
	int             part;
	int             error;
	/* register struct slice *slice; */

	RR;
	pd = slice->private_up;
	/* slice = pd->slice_down; */
	bzero(&label, sizeof(label));
	/*
	 * Try load a valid disklabel. This is 90% of what we need to check.
	 */
	if (((error = dkl_extract_table(slice, &label)) != 0)
	    || (slice->limits.blksize != 512)) {
		/*-
		 * Oh oh, we need to invalidate all the subslices.
		 * and relinquish this slice.
		 */
		return (dkl_revoke(pd));
	}
	dl = &(pd->disklabel);
	dp = dl->d_partitions;
	dp2 = label.d_partitions;
	for (part = 0; part < MAXPARTITIONS; part++, dp++, dp2++) {
		if (pd->subdevs[part].slice) {
			if ((dp2->p_offset != dp->p_offset)
			    || (dp2->p_size != dp->p_size)) {
				sl_rmslice(pd->subdevs[part].slice);
				pd->subdevs[part].slice = NULL;
			} else if (pd->subdevs[part].slice->handler_up) {
				(*pd->subdevs[part].slice->handler_up->verify)
					(pd->subdevs[part].slice);
			}
		}
	}
	/*- having got rid of changing slices, replace
	 * the old table with the new one, and
	 * handle any new slices by calling the constructor.
	 */
	bcopy(&label, dl, sizeof(label));
	error = dkl_constructor(slice);
done:
	return (error);
}

/*-
 * Invalidate all subslices, and free resources for this handler instance.
 */
static int
dkl_revoke(void *private)
{
	register struct private_data *pd;
	register struct slice *slice;
	int             part;

	RR;
	pd = private;
	slice = pd->slice_down;
	for (part = 0; part < MAXPARTITIONS; part++) {
		if (pd->subdevs[part].slice) {
			sl_rmslice(pd->subdevs[part].slice);
		}
	}
	/*-
	 * remove ourself as a handler
	 */
	slice->handler_up = NULL;
	slice->private_up = NULL;
	slicetype.refs--;
#ifdef BAD144
	if (pd->bad)
		free(pd->bad, M_DEVBUF);
#endif
	free(pd, M_DEVBUF);
	sl_unref(slice);
	return (0);
}

/*-
 * shift the appropriate IO by the offset for that slice.
 */
static void
dkl_IOreq(void *private, struct buf * bp)
{
	register struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;

RR;
	sdp = private;
	pd = sdp->pd;
	slice = pd->slice_down;
	bp->b_pblkno += sdp->offset; /* add the offset for that slice */
	sliceio(slice, bp, SLW_ABOVE);
}

static int
dkl_open(void *private, int flags, int mode, struct proc * p)
{
	register struct private_data *pd;
	struct subdev		*sdp;
	register struct slice	*slice;
	int			 error;
	u_int8_t		 newrflags = 0;
	u_int8_t		 newwflags = 0;
	int			 newoflags;
	int			 part;
	u_int8_t		 partbit;

RR;
	sdp = private;
	part = sdp->part;
	partbit = (1 << part);
	pd = sdp->pd;
	slice = pd->slice_down;

	/*
	 * Calculate the change to to over-all picture here.
	 * Notice that this might result in LESS open bits
	 * if that was what was passed from above.
	 * (Prelude to 'mode-change' instead of open/close.)
	 */
	/* work out what our stored flags will be if this succeeds */
	newwflags &= ~ (partbit);
	newrflags &= ~ (partbit);
	newwflags |= (flags & FWRITE) ? (partbit) : 0;
	newrflags |= (flags & FREAD) ? (partbit) : 0;

	/* work out what we want to pass down this time */
	newoflags = newwflags ? FWRITE : 0;
	newoflags |= newrflags ? FREAD : 0;

	/*
	 * If the agregate flags we used last time are the same as
	 * the agregate flags we would use this time, then don't
	 * bother re-doing the command.
	 */
	if (newoflags != pd->savedoflags) {
		if (error = sliceopen(slice, newoflags, mode, p, SLW_ABOVE)) {
				return (error);
		}
	}

	/*
	 * Now that we know it succeeded, commit, by replacing the old
	 * flags with the new ones.
	 */
	pd->rflags = newrflags;
	pd->wflags = newwflags;
	pd->savedoflags = newoflags;
	return (0);
}

#if 0
static  void
dkl_close(void *private, int flags, int mode, struct proc * p)
{
	register struct private_data *pd;
	struct subdev		*sdp;
	register struct slice	*slice;
	u_int8_t		 newrflags = 0;
	u_int8_t		 newwflags = 0;
	int			 newoflags;
	int			 part;
	u_int8_t		 partbit;

RR;
	sdp = private;
	part = sdp->part;
	partbit = (1 << part);
	pd = sdp->pd;
	slice = pd->slice_down;

	if ((pd->rflags == 0) && (pd->wflags == 0))
		return;

	/* work out what our stored flags will be if this succeeds */
	newwflags &= ~ (partbit);
	newrflags &= ~ (partbit);
	newwflags |= (flags & FWRITE) ? (partbit) : 0;
	newrflags |= (flags & FREAD) ? (partbit) : 0;

	/* work out what we want to pass down this time */
	newoflags = newwflags ? FWRITE : 0;
	newoflags |= newrflags ? FREAD : 0;

	/*                                                                      
	 * If this was the last open slice above, then release our own open     
	 */                   
	if ((pd->rflags == 0) && (pd->wflags == 0)) {
		sliceclose(slice, newoflags, mode, p, SLW_ABOVE);
	}
	pd->rflags = newrflags;
	pd->wflags = newwflags;
	pd->savedoflags = newoflags;
	return ;
}
#endif	/* 0 */

static int
dkl_ioctl(void *private, int cmd, caddr_t addr, int flag, struct proc * p)
{
	register struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;
	struct disklabel *lp;
	int	error;

	RR;
	sdp = private;
	pd = sdp->pd;
	slice = pd->slice_down;
	lp = &pd->disklabel;
	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *lp;
		return (0);

	case DIOCGPART:
		if (lp == NULL)
			return (EINVAL);
		((struct partinfo *)addr)->disklab = lp;
		((struct partinfo *)addr)->part = lp->d_partitions + sdp->part;
		return (0);

#ifdef BAD144
	case DIOCSBAD:
		if (!(flag & FWRITE))
			return (EBADF);
		return (dkl_internbad144(pd, (struct dkbad *)addr, M_WAITOK));
#endif

/* These don't really make sense. keep the headers for a reminder */
	case DIOCSDINFO:
	case DIOCSYNCSLICEINFO:
	case DIOCWDINFO:
	case DIOCWLABEL:
		return (ENOIOCTL);
	}

	return ((*slice->handler_down->ioctl) (slice->private_down,
					       cmd, addr, flag, p));
}

static int
dkl_upconfig(struct slice *slice, int cmd, caddr_t addr, int flag, struct proc * p)
{
	RR;
	switch (cmd) {
	case SLCIOCRESET:
		return (0);

#ifdef BAD144
	case SLCIOCTRANSBAD:
	{
		struct private_data *pd;
		daddr_t blkno;

		pd = slice->private_up;
		if (pd->bad)
			*(daddr_t*)addr = dkl_transbad144(pd, *(daddr_t*)addr);
		return (0);
	}
#endif

/* These don't really make sense. keep the headers for a reminder */
	default:
		return (ENOIOCTL);
	}
	return (0);
}

static struct disklabel static_label;
/*
 * This is a hack routine called from the slice generic code to produce a dummy
 * disklabel when given a slice descriptor. It's in here because this code
 * knows about disklabels.
 */
int
dkl_dummy_ioctl(struct slice *slice, int cmd, caddr_t addr,
					int flag, struct proc * p)
{
	struct disklabel *lp = &static_label;

	switch (cmd) {
	case DIOCGDINFO:
	case DIOCGPART:
		bzero(lp, sizeof(static_label));
		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		lp->d_secsize = slice->limits.blksize;
		lp->d_nsectors = 1;
		lp->d_ntracks = 1;
		lp->d_secpercyl = 1;
		lp->d_ncylinders = 
		lp->d_secperunit = slice->limits.slicesize
				 / slice->limits.blksize;
		lp->d_npartitions = RAW_PART + 1;
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		lp->d_partitions[RAW_PART].p_offset = 0;
		break;
	default:
		return (ENOIOCTL);
	}
	lp->d_checksum = dkcksum(lp);

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *lp;
		break;
	case DIOCGPART:
		/*  XXX hack alert.
		 * This is a hack as this information is consumed immediatly
		 * otherwise the use of a static buffer would be dangerous.
		 */
		((struct partinfo *)addr)->disklab = lp;
		((struct partinfo *)addr)->part = lp->d_partitions + RAW_PART;
	}
	
	return (0);

}

#if 0 /* use the existing one for now */
/*-
 * Compute checksum for disk label.
 */
u_int
dkcksum(lp)
	register struct disklabel *lp;
{
	register u_short *start, *end;
	register u_short sum = 0;

	start = (u_short *) lp;
	end = (u_short *) & lp->d_partitions[lp->d_npartitions];
	while (start < end)
		sum ^= *start++;
	return (sum);
}
#endif /* 0 */

static int
dkl_dump(void *private, int32_t blkoff, int32_t blkcnt)
{
	struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;

RR;
	sdp = private;
	pd = sdp->pd;
	slice = pd->slice_down;
	blkoff += sdp->offset;
	return (*slice->handler_down->dump)(slice->private_down, blkoff, blkcnt);
}
