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
 *	$Id: $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/malloc.h>
#include <sys/sliceio.h>
#include <dev/slice/slice.h>


struct private_data {
	u_int32_t	 flags;
	struct slice	*slice_down;
	int		 savedoflags;
	struct dos_partition dos_table[NDOSPART];
	struct subdev {
		int			 part;
		struct slice		*slice;
		struct slicelimits	 limit;
		struct private_data	*pd;
		u_int32_t		 offset; /* Fdisk only has 32 bits */
	}               subdevs[NDOSPART];
};
/*
 * Bits in the mbr private data flag word
 */
#define	MBRF_OPEN_RBIT	0x01
#define	MBRF_S1_OPEN_RD	0x01
#define	MBRF_S2_OPEN_RD	0x02
#define	MBRF_S3_OPEN_RD	0x04
#define	MBRF_S4_OPEN_RD	0x08
#define	MBRF_MSK_RD	0x0F
#define	MBRF_OPEN_WBIT	0x10
#define	MBRF_S1_OPEN_WR	0x10
#define	MBRF_S2_OPEN_WR	0x20
#define	MBRF_S3_OPEN_WR	0x40
#define	MBRF_S4_OPEN_WR	0x80
#define	MBRF_MSK_WR	0xF0
#define	MBRF_MSK_OPEN	0xFF

static struct dos_partition historical_bogus_partition_table[NDOSPART] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0,},
	{0x80, 0, 1, 0, DOSPTYP_386BSD, 255, 255, 255, 0, 50000,},
};
#define DOSPTYP_ONTRACK 84

static sl_h_constructor_t mbr_constructor;	/* constructor (from device) */
static sl_h_IO_req_t mbr_IOreq;	/* IO req downward (to device) */
static sl_h_ioctl_t mbr_ioctl;	/* ioctl req downward (to device) */
static sl_h_open_t mbr_open;	/* downwards travelling open */
static sl_h_close_t mbr_close;	/* downwards travelling close */
static sl_h_claim_t mbr_claim;	/* upwards travelling claim */
static sl_h_revoke_t mbr_revoke;/* upwards travelling revokation */
static sl_h_verify_t mbr_verify;/* things changed, are we stil valid? */
static sl_h_upconfig_t mbr_upconfig;/* config request from below */

static struct slice_handler slicetype = {
	"MBR",
	0,
	NULL,
	0,
	&mbr_constructor,	/* constructor */
	&mbr_IOreq,
	&mbr_ioctl,
	&mbr_open,
	&mbr_close,
	&mbr_revoke,		/* revoke */
	&mbr_claim,		/* claim */
	&mbr_verify,		/* verify */
	&mbr_upconfig		/* config from below */
};

static void
sd_drvinit(void *unused)
{
	sl_newtype(&slicetype);
}

SYSINIT(sddev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, sd_drvinit, NULL);

/*
 * Given a slice, extract out our table of information
 */
static int
mbr_find_table(sl_p slice, struct buf **bpp, int *blknum)
{
	int             ontrack_offset = 0;
	int		error;
	u_int8_t	*cp;
	struct dos_partition *dp0, *dp;
	int		part;
	int		redone = 0;
	struct buf	*bp;

RR;
	*bpp = NULL;
reread:
	if (error = slice_readblock(slice, ontrack_offset, &bp))
		return (error);
	cp = bp->b_data;
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		error = EINVAL;
		goto done;
	}
	dp0 = (struct dos_partition *) (cp + DOSPARTOFF);

	/*
	 * Check for "Ontrack Diskmanager". Note that if the geometry is
	 * still needed then we probably won't be able to read a DiskManager
	 * MBR because we will fail to read sector 63. The very act of
	 * finding a Disk Manager might however have given us the info we
	 * need if the disk manager set's its partition up correctly.
	 */
	if (!redone) {
		for (part = 0, dp = dp0;
		     part < NDOSPART; part++, dp++) {
			if (dp->dp_typ == DOSPTYP_ONTRACK) {
#ifdef MAYBE
				/*
				 * It's not known if this should always 63 or
				 * if this is just the start of the 2nd
				 * track.
				 */
				ontrack_offset = dp->dp_start;
#else
				ontrack_offset = 63;
#endif
				if (bootverbose)
					printf("Found \"Ontrack Disk Manager\"\n");
				bp->b_flags |= B_INVAL | B_AGE;
				brelse(bp);
				redone++;
				goto reread;
			}
		}
	}
done:
	if (blknum)
		*blknum = ontrack_offset;
	*bpp = bp;
	return (error);
}

/*
 * Given a slice, extract out our table of information
 */
static int
mbr_extract_table(sl_p slice, struct dos_partition *table)
{
	int		error;
	struct buf	*bp;

RR;
	/* start off with a known result */
	bzero(table, sizeof(*table) * NDOSPART);
	error = mbr_find_table(slice, &bp, NULL);
	if (!error)
		bcopy((bp->b_data + DOSPARTOFF), table,
				sizeof(*table) * NDOSPART);
done:
	if (bp) {
		bp->b_flags |= B_INVAL | B_AGE;
		brelse(bp);
	}
	return (error);
}

/*
 * read the block and replace the mbr table with that given.
 * If there isn't one, clear the rest of the block.
 */
static int
mbr_insert_table(sl_p slice, struct dos_partition *table)
{
	int             blknum = 0;
	int		error;
	struct buf	*bp;

RR;
	error = mbr_find_table(slice, &bp, &blknum);
	if ( error == EINVAL) {
		/*
		 * The block was read, but there was no table there.
		 * just clear out the cruft for now.
		 */
		bzero(bp->b_data, slice->limits.blksize);
	} else if (error == 0) {
		bcopy( table, (bp->b_data + DOSPARTOFF),
				sizeof(*table) * NDOSPART);
		bp->b_data[0x1FE] = 0x55;
		bp->b_data[0x1FF] = 0xAA;
		/* XXX Somehow we should get boot code in there too. */
		/* for now leave it to the tool */
		error = slice_writeblock(slice, blknum, bp);
	}
done:
	if (bp) {
		bp->b_flags |= B_INVAL | B_AGE;
		brelse(bp);
	}
	return (error);
}

/*
 * look at a slice and figure out if we should be interested in it. (Is it
 * ours?)
 */
static int
mbr_claim(struct slice * slice, struct slice * lower, void *ID)
{
	struct dos_partition table[NDOSPART];
	struct dos_partition *dp, *dp0;
	int             part;
	int             error;
	int             max_ncyls;
	int             max_nsectors;
	int             max_ntracks;
	u_int32_t       secpercyl;
	int		numactive = 0;
RR;

	/*
	 * Don't even BOTHER if it's not 512 byte sectors
	 */
	if (slice->limits.blksize != 512)
		return (EINVAL);
	/*
 	 * Try load a valid MBR table. This is 90% of what we need to check.
	 */
	if ((error = mbr_extract_table(slice, table)) != 0) {
		return (error);
	}
	dp0 = table;
	/*
	 * The first block of the dos code is marked like a valid MBR.
	 * Try to distinguish this case byt doing a sanity check on the table.
	 * Check:
	 * Flag byte can only be 0 or 0x80.
	 * At most one active partition.
	 * -Other tests to be added here-
	 */
	for (part = 0, dp = dp0; part < NDOSPART; part++, dp++) {
		if (dp->dp_flag & 0x7f) {
			printf ("rejected.. bad flag ");
			return(EINVAL);	/* must be either 0 or 0x80 */
		}
		if ((dp->dp_typ) && (dp->dp_size) && (dp->dp_start == 0)) {
			printf("rejected.. Slice includes MBR ");
			return (EINVAL);
		}
		if (dp->dp_flag == 0x80)
			numactive++;
	}
	if (numactive > 1) {
		printf ("rejected.. multiple active ");
		return (EINVAL);
	}
	/*
	 * If it's the MBR that comes with the disklabel then we should just
	 * give up and let the disklabel handler take control of this slice.
	 */
	if (bcmp(dp0, historical_bogus_partition_table,
		 sizeof historical_bogus_partition_table) == 0) {
		printf("rejecting disklabel table ");
		return (EINVAL);
	}
	/*
	 * well, it looks like one of ours.
	 */
		return (0);
}

/*
 * This routine tries to guess the geometry for
 * old disk drivers that need the MBR code to set it. Bits taken from
 * diskslice_machdep.c which itself evolved from earlier code.
 * This is not part of the SLICE code per-se, but just a convenient place to
 * put this HACK because everything is in scope. Only called by the IDE driver.
 */
int
mbr_geom_hack(struct slice * slice, struct ide_geom *geom)
{
	struct dos_partition table[NDOSPART];
	struct dos_partition *dp, *dp0;
	int             part;
	int             error;
	int             max_ncyls;
	int             max_nsectors;
	int             max_ntracks;
	u_int32_t       secpercyl;
RR;

	/*
	 * Don't even BOTHER if it's not claimable by us.
	 */
	if ((error = mbr_claim(slice,NULL,0)))
		return (error);
	/*
 	 * Load the mbr.
	 */
	if ((error = mbr_extract_table(slice, table)) != 0) {
		return (error);
	}
	dp0 = table;
	/*
	 * Guess the geometry. For some old drives (ESDI, st506) the
	 * driver below us may not yet know the geometry, but needs
	 * to before it can access blocks out of the first track.
	 * This hack is to use information in the MBR to "deduce"
	 * this information and pass it back.
	 */
	max_ncyls = 0;
	max_nsectors = 0;
	max_ntracks = 0;
	for (part = 0, dp = dp0; part < NDOSPART; part++, dp++) {
		int             ncyls;
		int             nsectors;
		int             ntracks;

		if (dp->dp_size == 0)
			continue;
		ncyls = DPCYL(dp->dp_ecyl, dp->dp_esect) + 1;
		if (max_ncyls < ncyls)
			max_ncyls = ncyls;
		nsectors = DPSECT(dp->dp_esect);
		if (max_nsectors < nsectors)
			max_nsectors = nsectors;
		ntracks = dp->dp_ehd + 1;
		if (max_ntracks < ntracks)
			max_ntracks = ntracks;
	}
	if ((max_ncyls == 0)
	    && (max_nsectors == 0)
	    && (max_ntracks == 0)) {
		/* we've gained nought, so just return */
		return (EINVAL);
	}
	secpercyl = (u_long) max_nsectors *max_ntracks;
	printf("s=%d, h=%d, c=%d\n", max_nsectors, max_ntracks, max_ncyls);
	/*
	 * Check that we have guessed the geometry right by checking
	 * the partition entries.
	 */
	error = 0;
	for (part = 0, dp = dp0; part < NDOSPART; part++, dp++) {
		int             cyl;
		int             sector;
		int             track;
		int             secpercyl;

		if (dp->dp_size == 0)
			continue;
		cyl = DPCYL(dp->dp_scyl, dp->dp_ssect);
		track = dp->dp_shd;
		sector = DPSECT(dp->dp_ssect) - 1;
		secpercyl = max_nsectors * max_ntracks;
		/*
		 * If the geometry doesn't work for any partition
		 * start then don't accept it.
		 */
		if (((((dp->dp_start / secpercyl) % 1024) != cyl)
		     && (cyl != 1023))
		    || (((dp->dp_start % secpercyl)
			 / max_nsectors) != track)
		    || (((dp->dp_start % secpercyl)
			 % max_nsectors) != sector)) {
			printf("Can't get disk geometry from MBR\n");
			return (EINVAL);
		}
		if ((dp->dp_start / secpercyl) > 1023) {
			printf("part %d above BIOS reach\n", part);
		}
	}

	/*
	 * Set our newely hypothesised numbers into the geometry
	 * slots in the supplied SLICE.
	 */
	geom->secpertrack = max_nsectors;
	geom->trackpercyl = max_ntracks;
	geom->cyls = max_ncyls;
	return (0);
}

/*
 * look at a slice we know to be ours and decide what the #$%^ to do with it.
 * We presume the driver already did the geometry hack if needed.
 */
static int
mbr_constructor(sl_p slice)
{
	int             i;
	u_int64_t       disksize = slice->limits.slicesize;
	struct private_data *pd;
	struct dos_partition *dp, *dp0;
	int             redone = 0;
	int             ontrack_offset = 0;
	char            name[64];
	sh_p	tp;

	int             part;
	int             error = 0;

RR;
	/*
	 * If we are being called to re-load a slice,
	 * then don't reallocate resources.
	 */
	if ( (pd = slice->private_up) == NULL) {
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
		if ((error = mbr_extract_table(slice, pd->dos_table)) != 0) {
			/* 
			 * If it's just that there is no table there,
			 * Then we fake an empty one up and write it. if
			 * this were not ok, then we would have not been
			 * called. (as probe will have failed). If it's 
			 * a physical error, then that's reason to fail.
			 */
			if (error != EINVAL) {
				free(pd, M_DEVBUF);
				return (error);
			}
			bzero(pd->dos_table, sizeof(pd->dos_table));
			if ((error = mbr_insert_table(slice, pd->dos_table))) {
				free(pd, M_DEVBUF);
				return (error);
			}
			
		}
		slice->refs++;
		slice->handler_up = &slicetype;
		slice->private_up = pd;
		slicetype.refs++;
	}

	dp0  = pd->dos_table;

	/*
	 * Handle each of the partitions. 
	 * We should check that each makes sence and is legal.
	 * 1/ it should not already have a slice.
	 * 2/ should not be 0 length.
	 * 3/ should not go past end of our slice.
	 * 4/ should not include sector 0.
	 * 5/ should not overlap other slices.
	 */
	dp = dp0;
	for (part = 0; part < NDOSPART; part++, dp++) {
		int i;
		if (pd->subdevs[part].slice != NULL)
breakout:		continue;
		if (dp->dp_size == 0)
			continue;
		if (dp->dp_start < 1)
			continue;
		if ((dp->dp_start + dp->dp_size) >
		    (slice->limits.slicesize/slice->limits.blksize))
			continue;
		/* check for overlaps with existing slices */
		for (i = 0; i < NDOSPART; i++) {
 			/* skip empty slots (including this one) */
			if(pd->subdevs[i].slice == NULL )
				continue;
			if ((dp0[i].dp_start < (dp->dp_start + dp->dp_size))
			&& ((dp0[i].dp_start + dp0[i].dp_size) > dp->dp_start))
			{
				printf("mbr: new slice %d overlaps slice %d\n",
					part, i);
				goto breakout;
			}
		}
		/* 
		 * the slice seems to make sense. Use it.
		 */
		pd->subdevs[part].part = part;
		pd->subdevs[part].pd = pd;
		pd->subdevs[part].offset = dp->dp_start;
		pd->subdevs[part].limit.blksize
			= slice->limits.blksize;
		pd->subdevs[part].limit.slicesize
			= (slice->limits.blksize * (u_int64_t)dp->dp_size);

		sprintf(name, "%ss%d", slice->name, part + 1);
		sl_make_slice(&slicetype,
			      &pd->subdevs[part],
			      &pd->subdevs[part].limit,
			      &pd->subdevs[part].slice,
			      NULL,
			      name);
		pd->subdevs[part].slice->probeinfo.typespecific = &dp->dp_typ;
		switch (dp->dp_typ) { /* list stolen from fdisk */
		case	0x00:	/* "unused" */
		case	0x01:	/* "Primary DOS with 12 bit FAT" */
		case	0x02:	/* "XENIX / filesystem" */
		case	0x03:	/* "XENIX /usr filesystem" */
		case	0x04:	/* "Primary DOS with 16 bit FAT" */
		case	0x05:	/* "Extended DOS" */
		case	0x06:	/* "Primary 'big' DOS (> 32MB)" */
		case	0x07:	/* "OS/2 HPFS, QNX or Advanced UNIX" */
		case	0x08:	/* "AIX filesystem" */
		case	0x09:	/* "AIX boot partition or Coherent" */
		case	0x0A:	/* "OS/2 Boot Manager or OPUS" */
		case	0x10:	/* "OPUS" */
		case	0x40:	/* "VENIX 286" */
		case	0x50:	/* "DM" */
		case	0x51:	/* "DM" */
		case	0x52:	/* "CP/M or Microport SysV/AT" */
		case	0x56:	/* "GB" */
		case	0x61:	/* "Speed" */
		case	0x63:	/* "ISC UNIX, System V/386, GNU HURD or Mach" */
		case	0x64:	/* "Novell Netware 2.xx" */
		case	0x65:	/* "Novell Netware 3.xx" */
		case	0x75:	/* "PCIX" */
		case	0x80:	/* "Minix 1.1 ... 1.4a" */
		case	0x81:	/* "Minix 1.4b ... 1.5.10" */
		case	0x82:	/* "Linux swap" */
		case	0x83:	/* "Linux filesystem" */
		case	0x93:	/* "Amoeba filesystem" */
		case	0x94:	/* "Amoeba bad block table" */
		case	0xA6:	/* "OpenBSD" */
		case	0xA7:	/* "NEXTSTEP" */
		case	0xB7:	/* "BSDI BSD/386 filesystem" */
		case	0xB8:	/* "BSDI BSD/386 swap" */
		case	0xDB:	/* "Concurrent CPM or C.DOS or CTOS" */
		case	0xE1:	/* "Speed" */
		case	0xE3:	/* "Speed" */
		case	0xE4:	/* "Speed" */
		case	0xF1:	/* "Speed" */
		case	0xF2:	/* "DOS 3.3+ Secondary" */
		case	0xF4:	/* "Speed" */
		case	0xFF:	/* "BBT (Bad Blocks Table)" */
			printf("%s: type %d. Leaving\n",
				pd->subdevs[part].slice->name,
				(u_int)dp->dp_typ);
			pd->subdevs[part].slice->probeinfo.type = NO_SUBPART;
			break;
		case DOSPTYP_386BSD:	/* 0xA5 "FreeBSD/NetBSD/386BSD" */
			pd->subdevs[part].slice->probeinfo.type = "disklabel";
			break;
		default:
			pd->subdevs[part].slice->probeinfo.type = NULL;
		}
		if ((tp = slice_probeall(pd->subdevs[part].slice)) != NULL) {
			(*tp->constructor)(pd->subdevs[part].slice);
		}
	}
	return (error);
}

/*
 * look at a slice that USED to be ours.
 * decide if any sub-slices need to be revoked.
 * If not then at least ask them to verify themselves.
 * Note, arg 'slice' is not strictly needed
 */
static int
mbr_verify(sl_p slice)
{
	register struct private_data *pd;
	struct dos_partition table[NDOSPART];
	struct dos_partition *dp, *dp0;
	int             part;
	int             error;
	/* register struct slice *slice; */

RR;
	pd = slice->private_up;
	/* slice = pd->slice_down; */
	bzero(table, sizeof(table));
	/*
 	 * Try load a valid MBR table. This is 90% of what we need to check.
	 */
	if ((slice->limits.blksize != 512)
	|| ((error = mbr_extract_table(slice, table)) != 0)) {
		/*
		 * Oh oh, we need to invalidate all the subslices.
		 * and relinquish this slice.
		 */
		return(mbr_revoke(pd));
	}
	/*
	 * For each existing subslice, check that the basic size
	 * and position has not changed. Also check the TYPE.
	 * It is possible we should allow a slice to grow.
	 */
	dp = dp0 = pd->dos_table;
	for (part = 0, dp = dp0; part < NDOSPART; part++, dp++) {
		if (pd->subdevs[part].slice) {
			if ((table[part].dp_start != dp->dp_start)
			|| (table[part].dp_size != dp->dp_size)
			|| (table[part].dp_typ != dp->dp_typ) ) {
				sl_rmslice(pd->subdevs[part].slice);
				pd->subdevs[part].slice = NULL;
			} else if ( pd->subdevs[part].slice->handler_up) {
				(*pd->subdevs[part].slice->handler_up->verify)
					(pd->subdevs[part].slice);
			}
		}
	}
	/*
	 * Having got rid of changing slices, replace
	 * the old table with the new one, and 
	 * Handle any new slices by calling the constructor.
	 * This way, if we are in 'promiscuous' mode,
	 * (e.g. repartitionning a disk we are running on from 
	 * Single user mode, the unchanged slices can remain open and active
	 * through the process. If you change an open slice,
	 * the vnodes will be changed to deadfs so a crash is probably
	 * nearby. XXX too late. It's written to disk.. (we COULD reverse it,
	 * but....)
	 */
	bcopy( table, dp0, sizeof(table));
	error = mbr_constructor(slice);
	return (error);
}

/*
 * Invalidate all subslices, and free resources for this handler instance.
 */
static int
mbr_revoke(void *private)
{
	register struct private_data *pd;
	register struct slice *slice;
	int             part;

RR;
	pd = private;
	slice = pd->slice_down;
	for (part = 0; part < NDOSPART; part++) {
		if (pd->subdevs[part].slice) {
			sl_rmslice(pd->subdevs[part].slice);
		}
	}
	/*
	 * remove ourself as a handler
	 */
	slice->handler_up = NULL;
	slice->private_up = NULL;
	slicetype.refs--;
	free(pd,M_DEVBUF);
	sl_unref(slice);
	return (0);
}

/*
 * shift the appropriate IO by the offset for that slice.
 */
static void
mbr_IOreq(void *private, struct buf * bp)
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
mbr_open(void *private, int flags, int mode, struct proc * p)
{
	register struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;
	int	part;
	int	error;
	int	newflags = 0;
	int	oldoflags = 0;
	int	newoflags = 0;

RR;
	sdp = private;
	part = sdp->part;
	pd = sdp->pd;
	slice = pd->slice_down;

	/*
	 * Calculate the change to to over-all picture here.
	 * Notice that this might result in LESS open bits
	 * if that was what was passed from above.
	 * (Prelude to 'mode-change' instead of open/close.)
	 */
	/* work out what our stored flags will be if this succeeds */
	newflags = pd->flags & ~((MBRF_OPEN_WBIT|MBRF_OPEN_RBIT) << part);
	newflags |= (flags & FWRITE) ? (MBRF_OPEN_WBIT << part) : 0;
	newflags |= (flags & FREAD) ? (MBRF_OPEN_RBIT << part) : 0;

	/* work out what we want to pass down this time */
	newoflags = (newflags & MBRF_MSK_WR) ? FWRITE : 0;
	newoflags |= (newflags & MBRF_MSK_RD) ? FREAD : 0;

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
	pd->flags &= ~MBRF_MSK_OPEN;
	pd->flags |= newflags;
	pd->savedoflags = newoflags;
	return (0);
}

static  void
mbr_close(void *private, int flags, int mode, struct proc * p)
{
	register struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;
	int	newflags;
	int	newoflags;
	int	part;

RR;
	sdp = private;
	part = sdp->part;
	pd = sdp->pd;
	slice = pd->slice_down;

	if ((pd->flags & MBRF_MSK_OPEN) == 0)
		return;

	/* work out what our stored flags will be if this succeeds */
	newflags = pd->flags & ~((MBRF_OPEN_WBIT|MBRF_OPEN_RBIT) << part);
	newflags |= (flags & FWRITE) ? (MBRF_OPEN_WBIT << part) : 0;
	newflags |= (flags & FREAD) ? (MBRF_OPEN_RBIT << part) : 0;

	/* work out what we want to pass down this time */
	newoflags = (newflags & MBRF_MSK_WR) ? FWRITE : 0;
	newoflags |= (newflags & MBRF_MSK_RD) ? FREAD : 0;

	/*                                                                      
	 * If this was the last open slice above, then release our own open     
	 */                   
	pd->flags &= ~((MBRF_OPEN_RBIT|MBRF_OPEN_WBIT) << part);
	if (pd->flags & MBRF_MSK_OPEN) {
		sliceclose(slice, newoflags, mode, p, SLW_ABOVE);
	}
	pd->flags &= ~MBRF_MSK_OPEN;
	pd->flags |= newflags;
	pd->savedoflags = newoflags;
	return ;
}

static int
mbr_ioctl(void *private, int cmd, caddr_t addr, int flag, struct proc * p)
{
	register struct private_data *pd;
	struct subdev *sdp;
	register struct slice *slice;
	int             error;

RR;
	sdp = private;
	pd = sdp->pd;
	slice = pd->slice_down;

	return ((*slice->handler_down->ioctl) (slice->private_down,
					       cmd, addr, flag, p));
}

static int
mbr_upconfig(struct slice *slice, int cmd, caddr_t addr,
			int flag, struct proc * p)
{
	RR;
	switch (cmd) {
	case SLCIOCRESET:
		return (0);

/* These don't really make sense. keep the headers for a reminder */
	default:
		return (ENOIOCTL);
	}
	return (0);
}

