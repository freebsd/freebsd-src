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
#include <sys/disklabel.h>
#ifndef PC98
#define	DOSPTYP_EXTENDED	5
#define	DOSPTYP_EXTENDEDX	15
#define	DOSPTYP_ONTRACK		84
#endif
#include <sys/diskslice.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#define TRACE(str)	do { if (dsi_debug) printf str; } while (0)

static volatile u_char dsi_debug;

#ifndef PC98
static struct dos_partition historical_bogus_partition_table[NDOSPART] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0x80, 0, 1, 0, DOSPTYP_386BSD, 255, 255, 255, 0, 50000, },
};
#endif

static int check_part __P((char *sname, struct dos_partition *dp,
			   u_long offset, int nsectors, int ntracks,
			   u_long mbr_offset));
#ifndef PC98
static void mbr_extended __P((dev_t dev, struct disklabel *lp,
			      struct diskslices *ssp, u_long ext_offset,
			      u_long ext_size, u_long base_ext_offset,
			      int nsectors, int ntracks, u_long mbr_offset,
			      int level));
#endif
static int mbr_setslice __P((char *sname, struct disklabel *lp,
			     struct diskslice *sp, struct dos_partition *dp,
			     u_long br_offset));

#ifdef PC98
#define DPBLKNO(cyl,hd,sect) ((cyl)*(lp->d_secpercyl))
#if	NCOMPAT_ATDISK > 0
int     atcompat_dsinit __P((dev_t dev,
		 struct disklabel *lp, struct diskslices **sspp));
#endif
#endif

static int
check_part(sname, dp, offset, nsectors, ntracks, mbr_offset )
	char	*sname;
	struct dos_partition *dp;
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
#ifdef PC98
	u_long	pc98_start;
	u_long	pc98_size;
#endif

	secpercyl = (u_long)nsectors * ntracks;
#ifdef PC98
	chs_scyl = dp->dp_scyl;
	chs_ssect = dp->dp_ssect;
	ssector = chs_ssect + dp->dp_shd * nsectors + 
		chs_scyl * secpercyl + mbr_offset;
#else
	chs_scyl = DPCYL(dp->dp_scyl, dp->dp_ssect);
	chs_ssect = DPSECT(dp->dp_ssect);
	ssector = chs_ssect - 1 + dp->dp_shd * nsectors + chs_scyl * secpercyl
		  + mbr_offset;
#endif
#ifdef PC98
	pc98_start = dp->dp_scyl * secpercyl;
	pc98_size = dp->dp_ecyl ?
		(dp->dp_ecyl + 1) * secpercyl - pc98_start : 0;
	ssector1 = offset + pc98_start;
#else
	ssector1 = offset + dp->dp_start;
#endif

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

#ifdef PC98
	chs_ecyl = dp->dp_ecyl;
	chs_esect = nsectors - 1;
	esector = chs_esect + (ntracks - 1) * nsectors +
		chs_ecyl * secpercyl + mbr_offset;
	esector1 = ssector1 + pc98_size - 1;
#else
	chs_ecyl = DPCYL(dp->dp_ecyl, dp->dp_esect);
	chs_esect = DPSECT(dp->dp_esect);
	esector = chs_esect - 1 + dp->dp_ehd * nsectors + chs_ecyl * secpercyl
		  + mbr_offset;
	esector1 = ssector1 + dp->dp_size - 1;
#endif

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
#ifdef PC98
		printf("%s: mid 0x%x, start %lu, end = %lu, size %lu%s\n",
		       sname, dp->dp_mid, ssector1, esector1, pc98_size,
			error ? "" : ": OK");
#else
		printf("%s: type 0x%x, start %lu, end = %lu, size %lu %s\n",
		       sname, dp->dp_typ, ssector1, esector1,
		       (u_long)dp->dp_size, error ? "" : ": OK");
#endif
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
	struct dos_partition *dp;
	struct dos_partition *dp0;
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
#ifdef PC98
	/* Read master boot record. */
	if ((int)lp->d_secsize < 1024)
		bp = geteblk((int)1024);
	else
		bp = geteblk((int)lp->d_secsize);
#else
reread_mbr:
	/* Read master boot record. */
	bp = geteblk((int)lp->d_secsize);
#endif
	bp->b_dev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	bp->b_blkno = mbr_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_iocmd = BIO_READ;
#ifdef PC98
	if (bp->b_bcount < 1024)
		bp->b_bcount = 1024;
#endif
	DEV_STRATEGY(bp, 1);
	if (bufwait(bp) != 0) {
		diskerr(&bp->b_io, "reading primary partition table: error",
		    0, (struct disklabel *)NULL);
		printf("\n");
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
#ifdef PC98
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
		((strncmp(devtoname(bp->b_dev), "rda", 3) == 0) ||
	    (strncmp(devtoname(bp->b_dev), "rwd", 3) == 0))) {
		/* IBM-PC HDD */
		bp->b_flags |= B_INVAL | B_AGE;
		brelse(bp);
		return atcompat_dsinit(dev, lp, sspp);
	}
#endif
	dp0 = (struct dos_partition *)(cp + 512);
#else
	dp0 = (struct dos_partition *)(cp + DOSPARTOFF);

	/* Check for "Ontrack Diskmanager". */
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++) {
		if (dp->dp_typ == DOSPTYP_ONTRACK) {
			if (bootverbose)
				printf(
	    "%s: Found \"Ontrack Disk Manager\" on this disk.\n", sname);
			bp->b_flags |= B_INVAL | B_AGE;
			brelse(bp);
			mbr_offset = 63;
			goto reread_mbr;
		}
	}

	if (bcmp(dp0, historical_bogus_partition_table,
		 sizeof historical_bogus_partition_table) == 0) {
		TRACE(("%s: invalid primary partition table: historical\n",
		       sname));
		error = EINVAL;
		goto done;
	}
#endif

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


#ifdef PC98
		ncyls = lp->d_ncylinders;
#else
		ncyls = DPCYL(dp->dp_ecyl, dp->dp_esect) + 1;
#endif
		if (max_ncyls < ncyls)
			max_ncyls = ncyls;
#ifdef PC98
		nsectors = lp->d_nsectors;
#else
		nsectors = DPSECT(dp->dp_esect);
#endif
		if (max_nsectors < nsectors)
			max_nsectors = nsectors;
#ifdef PC98
		ntracks = lp->d_ntracks;
#else
		ntracks = dp->dp_ehd + 1;
#endif
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
#ifdef PC98
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++) {
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0)
			continue;
		sname = dsname(dev, dkunit(dev), BASE_SLICE + dospart,
				RAW_PART, partname);
#else
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++) {
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0
		    && dp->dp_start == 0 && dp->dp_size == 0)
			continue;
		sname = dsname(dev, dkunit(dev), BASE_SLICE + dospart,
			       RAW_PART, partname);
#endif
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

#ifndef PC98
	/* Handle extended partitions. */
	sp -= NDOSPART;
	for (dospart = 0; dospart < NDOSPART; dospart++, sp++)
		if (sp->ds_type == DOSPTYP_EXTENDED ||
		    sp->ds_type == DOSPTYP_EXTENDEDX)
			mbr_extended(bp->b_dev, lp, ssp,
				     sp->ds_offset, sp->ds_size, sp->ds_offset,
				     max_nsectors, max_ntracks, mbr_offset, 1);

	/*
	 * mbr_extended() abuses ssp->dss_nslices for the number of slices
	 * that would be found if there were no limit on the number of slices
	 * in *ssp.  Cut it back now.
	 */
	if (ssp->dss_nslices > MAX_SLICES)
		ssp->dss_nslices = MAX_SLICES;
#endif

done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	if (error == EINVAL)
		error = 0;
	return (error);
}

#ifndef PC98
void
mbr_extended(dev, lp, ssp, ext_offset, ext_size, base_ext_offset, nsectors,
	     ntracks, mbr_offset, level)
	dev_t	dev;
	struct disklabel *lp;
	struct diskslices *ssp;
	u_long	ext_offset;
	u_long	ext_size;
	u_long	base_ext_offset;
	int	nsectors;
	int	ntracks;
	u_long	mbr_offset;
	int	level;
{
	struct buf *bp;
	u_char	*cp;
	int	dospart;
	struct dos_partition *dp;
	struct dos_partition dpcopy[NDOSPART];
	u_long	ext_offsets[NDOSPART];
	u_long	ext_sizes[NDOSPART];
	char	partname[2];
	int	slice;
	char	*sname;
	struct diskslice *sp;

	if (level >= 16) {
		printf(
	"%s: excessive recursion in search for slices; aborting search\n",
		       devtoname(dev));
		return;
	}

	/* Read extended boot record. */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = ext_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_iocmd = BIO_READ;
	DEV_STRATEGY(bp, 1);
	if (bufwait(bp) != 0) {
		diskerr(&bp->b_io, "reading extended partition table: error",
		    0, (struct disklabel *)NULL);
		printf("\n");
		goto done;
	}

	/* Weakly verify it. */
	cp = bp->b_data;
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		sname = dsname(dev, dkunit(dev), WHOLE_DISK_SLICE, RAW_PART,
			       partname);
		if (bootverbose)
			printf("%s: invalid extended partition table: no magic\n",
			       sname);
		goto done;
	}

	/* Make a copy of the partition table to avoid alignment problems. */
	memcpy(&dpcopy[0], cp + DOSPARTOFF, sizeof(dpcopy));

	slice = ssp->dss_nslices;
	for (dospart = 0, dp = &dpcopy[0]; dospart < NDOSPART;
	    dospart++, dp++) {
		ext_sizes[dospart] = 0;
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0
		    && dp->dp_start == 0 && dp->dp_size == 0)
			continue;
		if (dp->dp_typ == DOSPTYP_EXTENDED ||
		    dp->dp_typ == DOSPTYP_EXTENDEDX) {
			static char buf[32];

			sname = dsname(dev, dkunit(dev), WHOLE_DISK_SLICE,
				       RAW_PART, partname);
			snprintf(buf, sizeof(buf), "%s", sname);
			if (strlen(buf) < sizeof buf - 11)
				strcat(buf, "<extended>");
			check_part(buf, dp, base_ext_offset, nsectors,
				   ntracks, mbr_offset);
			ext_offsets[dospart] = base_ext_offset + dp->dp_start;
			ext_sizes[dospart] = dp->dp_size;
		} else {
			sname = dsname(dev, dkunit(dev), slice, RAW_PART,
				       partname);
			check_part(sname, dp, ext_offset, nsectors, ntracks,
				   mbr_offset);
			if (slice >= MAX_SLICES) {
				printf("%s: too many slices\n", sname);
				slice++;
				continue;
			}
			sp = &ssp->dss_slices[slice];
			if (mbr_setslice(sname, lp, sp, dp, ext_offset) != 0)
				continue;
			slice++;
		}
	}
	ssp->dss_nslices = slice;

	/* If we found any more slices, recursively find all the subslices. */
	for (dospart = 0; dospart < NDOSPART; dospart++)
		if (ext_sizes[dospart] != 0)
			mbr_extended(dev, lp, ssp, ext_offsets[dospart],
				     ext_sizes[dospart], base_ext_offset,
				     nsectors, ntracks, mbr_offset, ++level);

done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
}
#endif

static int
mbr_setslice(sname, lp, sp, dp, br_offset)
	char	*sname;
	struct disklabel *lp;
	struct diskslice *sp;
	struct dos_partition *dp;
	u_long	br_offset;
{
	u_long	offset;
	u_long	size;

#ifdef PC98
	offset = DPBLKNO(dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	size = dp->dp_ecyl ?
	    DPBLKNO(dp->dp_ecyl + 1, dp->dp_ehd, dp->dp_esect) - offset : 0;
#else
	offset = br_offset + dp->dp_start;
	if (offset > lp->d_secperunit || offset < br_offset) {
		printf(
		"%s: slice starts beyond end of the disk: rejecting it\n",
		       sname);
		return (1);
	}
	size = lp->d_secperunit - offset;
	if (size >= dp->dp_size)
		size = dp->dp_size;
	else
		printf(
"%s: slice extends beyond end of disk: truncating from %lu to %lu sectors\n",
		       sname, (u_long)dp->dp_size, size);
#endif
	sp->ds_offset = offset;
	sp->ds_size = size;
#ifdef PC98
	sp->ds_type = dp->dp_mid;
	sp->ds_subtype = dp->dp_sid;
	strncpy(sp->ds_name, dp->dp_name, sizeof(sp->ds_name));
#else
	sp->ds_type = dp->dp_typ;
#ifdef PC98_ATCOMPAT
	/* Fake FreeBSD(98). */
	if (sp->ds_type == DOSPTYP_386BSD)
		sp->ds_type = 0x94;
#endif
#endif /* PC98 */
#if 0
	lp->d_subtype |= (lp->d_subtype & 3) | dospart | DSTYPE_INDOSPART;
#endif
	return (0);
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
