#ifdef NO_GEOM
/*-
 * Copyright (c) 1994 Bruce D. Evans.
 * All rights reserved.
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
 *	from: @(#)ufs_disksubr.c	7.16 (Berkeley) 5/4/91
 *	from: ufs_disksubr.c,v 1.8 1994/06/07 01:21:39 phk Exp $
 * $FreeBSD$
 */

/*
 * PC9801 port by KATO Takenor <kato@eclogite.eps.nagoya-u.ac.jp>
 */

#include "compat_atdisk.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#if defined(PC98) && !defined(PC98_ATCOMPAT)
#include <sys/diskpc98.h>
#else
#include <sys/diskmbr.h>
#endif
#include <sys/diskslice.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#define TRACE(str)	do { if (dsi_debug) printf str; } while (0)

static volatile u_char dsi_debug;


static int check_part(char *sname, struct pc98_partition *dp,
			   u_long offset, int nsectors, int ntracks,
			   u_long mbr_offset);
static int mbr_setslice(char *sname, struct disklabel *lp,
			     struct diskslice *sp, struct pc98_partition *dp,
			     u_long br_offset);

#define DPBLKNO(cyl,hd,sect) ((cyl)*(lp->d_secpercyl))
#if	NCOMPAT_ATDISK > 0
int     atcompat_dsinit(dev_t dev,
		 struct disklabel *lp, struct diskslices **sspp);
#endif

static int
check_part(sname, dp, offset, nsectors, ntracks, mbr_offset )
	char	*sname;
	struct pc98_partition *dp;
	u_long	offset;
	int	nsectors;
	int	ntracks;
	u_long	mbr_offset;
{
	int	chs_ecyl;
	int	chs_esect;
	int	chs_scyl;
	int	chs_ssect;
	int	error;
	u_long	esector;
	u_long	esector1;
	u_long	secpercyl;
	u_long	ssector;
	u_long	ssector1;
	u_long	pc98_start;
	u_long	pc98_size;

	secpercyl = (u_long)nsectors * ntracks;
	chs_scyl = dp->dp_scyl;
	chs_ssect = dp->dp_ssect;
	ssector = chs_ssect + dp->dp_shd * nsectors + 
		chs_scyl * secpercyl + mbr_offset;
	pc98_start = dp->dp_scyl * secpercyl;
	pc98_size = dp->dp_ecyl ?
		(dp->dp_ecyl + 1) * secpercyl - pc98_start : 0;
	ssector1 = offset + pc98_start;

	/*
	 * If ssector1 is on a cylinder >= 1024, then ssector can't be right.
	 * Allow the C/H/S for it to be 1023/ntracks-1/nsectors, or correct
	 * apart from the cylinder being reduced modulo 1024.  Always allow
	 * 1023/255/63.
	 */
	if ((ssector < ssector1
	     && ((chs_ssect == nsectors && dp->dp_shd == ntracks - 1
		  && chs_scyl == 1023)
		 || (secpercyl != 0
		     && (ssector1 - ssector) % (1024 * secpercyl) == 0)))
	    || (dp->dp_scyl == 255 && dp->dp_shd == 255
		&& dp->dp_ssect == 255)) {
		TRACE(("%s: C/H/S start %d/%d/%d, start %lu: allow\n",
		       sname, chs_scyl, dp->dp_shd, chs_ssect, ssector1));
		ssector = ssector1;
	}

	chs_ecyl = dp->dp_ecyl;
	chs_esect = nsectors - 1;
	esector = chs_esect + (ntracks - 1) * nsectors +
		chs_ecyl * secpercyl + mbr_offset;
	esector1 = ssector1 + pc98_size - 1;

	/* Allow certain bogus C/H/S values for esector, as above. */
	if ((esector < esector1
	     && ((chs_esect == nsectors && dp->dp_ehd == ntracks - 1
		  && chs_ecyl == 1023)
		 || (secpercyl != 0
		     && (esector1 - esector) % (1024 * secpercyl) == 0)))
	    || (dp->dp_ecyl == 255 && dp->dp_ehd == 255
		&& dp->dp_esect == 255)) {
		TRACE(("%s: C/H/S end %d/%d/%d, end %lu: allow\n",
		       sname, chs_ecyl, dp->dp_ehd, chs_esect, esector1));
		esector = esector1;
	}

	error = (ssector == ssector1 && esector == esector1) ? 0 : EINVAL;
	if (bootverbose)
		printf("%s: mid 0x%x, start %lu, end = %lu, size %lu%s\n",
		       sname, dp->dp_mid, ssector1, esector1, pc98_size,
			error ? "" : ": OK");
	if (ssector != ssector1 && bootverbose)
		printf("%s: C/H/S start %d/%d/%d (%lu) != start %lu: invalid\n",
		       sname, chs_scyl, dp->dp_shd, chs_ssect,
		       ssector, ssector1);
	if (esector != esector1 && bootverbose)
		printf("%s: C/H/S end %d/%d/%d (%lu) != end %lu: invalid\n",
		       sname, chs_ecyl, dp->dp_ehd, chs_esect,
		       esector, esector1);
	return (error);
}

int
dsinit(dev, lp, sspp)
	dev_t	dev;
	struct disklabel *lp;
	struct diskslices **sspp;
{
	struct buf *bp;
	u_char	*cp;
	int	dospart;
	struct pc98_partition *dp;
	struct pc98_partition *dp0;
	int	error;
	int	max_ncyls;
	int	max_nsectors;
	int	max_ntracks;
	u_long	mbr_offset;
	char	partname[2];
	u_long	secpercyl;
	char	*sname;
	struct diskslice *sp;
	struct diskslices *ssp;

	mbr_offset = DOSBBSECTOR;
	/* Read master boot record. */
	if ((int)lp->d_secsize < 1024)
		bp = geteblk((int)1024);
	else
		bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	bp->b_blkno = mbr_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_iocmd = BIO_READ;
	if (bp->b_bcount < 1024)
		bp->b_bcount = 1024;
	DEV_STRATEGY(bp);
	if (bufwait(bp) != 0) {
		disk_err(&bp->b_io, "reading primary partition table: error",
		    0, 1);
		error = EIO;
		goto done;
	}

	/* Weakly verify it. */
	cp = bp->b_data;
	sname = dsname(dev, dkunit(dev), WHOLE_DISK_SLICE, RAW_PART, partname);
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		if (bootverbose)
			printf("%s: invalid primary partition table: no magic\n",
			       sname);
		error = EINVAL;
		goto done;
	}
	/*
	 * entire disk for FreeBSD
	 */
	if ((*(cp + 512) == 0x57) && (*(cp + 513) == 0x45) &&
		(*(cp + 514) == 0x56) && (*(cp + 515) == 0x82)) {
		sname = dsname(dev, dkunit(dev), BASE_SLICE,
			RAW_PART, partname);
		free(*sspp, M_DEVBUF);
		ssp = dsmakeslicestruct(MAX_SLICES, lp);
		*sspp = ssp;

		/* Initialize normal slices. */
		sp = &ssp->dss_slices[BASE_SLICE];
		sp->ds_offset = 0;
		sp->ds_size = lp->d_secperunit;
		sp->ds_type = DOSPTYP_386BSD;
		sp->ds_subtype = 0xc4;
		error = 0;
		ssp->dss_nslices = BASE_SLICE + 1;
		goto done;
	}

	/*
	 * XXX --- MS-DOG MO
	 */
	if ((*(cp + 0x0e) == 1) && (*(cp + 0x15) == 0xf0) &&
	    (*(cp + 0x1c) == 0x0) && 
		((*(cp + 512) == 0xf0) || (*(cp + 512) == 0xf8)) &&
	    (*(cp + 513) == 0xff) && (*(cp + 514) == 0xff)) {
		sname = dsname(dev, dkunit(dev), BASE_SLICE,
			RAW_PART, partname);
		free(*sspp, M_DEVBUF);
		ssp = dsmakeslicestruct(MAX_SLICES, lp);
		*sspp = ssp;

		/* Initialize normal slices. */
		sp = &ssp->dss_slices[BASE_SLICE];
		sp->ds_offset = 0;
		sp->ds_size = lp->d_secperunit;
		sp->ds_type = 0xa0;  /* XXX */
		sp->ds_subtype = 0x81;  /* XXX */
		error = 0;
		ssp->dss_nslices = BASE_SLICE + 1;
		goto done;
	}
#if NCOMPAT_ATDISK > 0
	/* 
	 * Check magic number of 'extended format' for PC-9801.
	 * If no magic, it may be formatted on IBM-PC.
	 */
	if (((cp[4] != 'I') || (cp[5] != 'P') || (cp[6] != 'L') ||
		 (cp[7] != '1')) &&
		((strncmp(devtoname(bp->b_dev), "da", 2) == 0) ||
		 (strncmp(devtoname(bp->b_dev), "wd", 2) == 0) ||
	    (strncmp(devtoname(bp->b_dev), "ad", 2) == 0))) {
		/* IBM-PC HDD */
		bp->b_flags |= B_INVAL | B_AGE;
		brelse(bp);
		return atcompat_dsinit(dev, lp, sspp);
	}
#endif
	dp0 = (struct pc98_partition *)(cp + 512);

	/* Guess the geometry. */
	/*
	 * TODO:
	 * Perhaps skip entries with 0 size.
	 * Perhaps only look at entries of type DOSPTYP_386BSD.
	 */
	max_ncyls = 0;
	max_nsectors = 0;
	max_ntracks = 0;
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++) {
		int	ncyls;
		int	nsectors;
		int	ntracks;


		ncyls = lp->d_ncylinders;
		if (max_ncyls < ncyls)
			max_ncyls = ncyls;
		nsectors = lp->d_nsectors;
		if (max_nsectors < nsectors)
			max_nsectors = nsectors;
		ntracks = lp->d_ntracks;
		if (max_ntracks < ntracks)
			max_ntracks = ntracks;
	}

	/*
	 * Check that we have guessed the geometry right by checking the
	 * partition entries.
	 */
	/*
	 * TODO:
	 * As above.
	 * Check for overlaps.
	 * Check against d_secperunit if the latter is reliable.
	 */
	error = 0;
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++) {
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0)
			continue;
		sname = dsname(dev, dkunit(dev), BASE_SLICE + dospart,
				RAW_PART, partname);
		/*
		 * Temporarily ignore errors from this check.  We could
		 * simplify things by accepting the table eariler if we
		 * always ignore errors here.  Perhaps we should always
		 * accept the table if the magic is right but not let
		 * bad entries affect the geometry.
		 */
		check_part(sname, dp, mbr_offset, max_nsectors, max_ntracks,
			   mbr_offset);
	}
	if (error != 0)
		goto done;

	/*
	 * Accept the DOS partition table.
	 * First adjust the label (we have been careful not to change it
	 * before we can guarantee success).
	 */
	secpercyl = (u_long)max_nsectors * max_ntracks;
	if (secpercyl != 0) {
		lp->d_nsectors = max_nsectors;
		lp->d_ntracks = max_ntracks;
		lp->d_secpercyl = secpercyl;
		lp->d_ncylinders = lp->d_secperunit / secpercyl;
	}

	/*
	 * We are passed a pointer to a suitably initialized minimal
	 * slices "struct" with no dangling pointers in it.  Replace it
	 * by a maximal one.  This usually oversizes the "struct", but
	 * enlarging it while searching for logical drives would be
	 * inconvenient.
	 */
	free(*sspp, M_DEVBUF);
	ssp = dsmakeslicestruct(MAX_SLICES, lp);
	*sspp = ssp;

	/* Initialize normal slices. */
	sp = &ssp->dss_slices[BASE_SLICE];
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++, sp++) {
		sname = dsname(dev, dkunit(dev), BASE_SLICE + dospart,
			       RAW_PART, partname);
		(void)mbr_setslice(sname, lp, sp, dp, mbr_offset);
	}
	ssp->dss_nslices = BASE_SLICE + NDOSPART;


done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	if (error == EINVAL)
		error = 0;
	return (error);
}


static int
mbr_setslice(sname, lp, sp, dp, br_offset)
	char	*sname;
	struct disklabel *lp;
	struct diskslice *sp;
	struct pc98_partition *dp;
	u_long	br_offset;
{
	u_long	offset;
	u_long	size;

	offset = DPBLKNO(dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	size = dp->dp_ecyl ?
	    DPBLKNO(dp->dp_ecyl + 1, dp->dp_ehd, dp->dp_esect) - offset : 0;
	sp->ds_offset = offset;
	sp->ds_size = size;
	sp->ds_type = dp->dp_mid;
	sp->ds_subtype = dp->dp_sid;
	strncpy(sp->ds_name, dp->dp_name, sizeof(sp->ds_name));
#if 0
	lp->d_subtype |= (lp->d_subtype & 3) | dospart | DSTYPE_INDOSPART;
#endif
	return (0);
}
#endif
