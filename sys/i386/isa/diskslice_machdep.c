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
 *	$Id: diskslice_machdep.c,v 1.21 1996/09/20 17:39:11 bde Exp $
 */

#include <stddef.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#define	DOSPTYP_EXTENDED	5
#define	DOSPTYP_ONTRACK		84
#include <sys/diskslice.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#define TRACE(str)	do { if (dsi_debug) printf str; } while (0)

static volatile u_char dsi_debug;

static struct dos_partition historical_bogus_partition_table[NDOSPART] = {
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
	{ 0x80, 0, 1, 0, DOSPTYP_386BSD, 255, 255, 255, 0, 50000, },
};

static int check_part __P((char *sname, struct dos_partition *dp,
			   u_long offset, int nsectors, int ntracks,
			   u_long mbr_offset));
static void extended __P((char *dname, dev_t dev, d_strategy_t *strat,
			  struct disklabel *lp, struct diskslices *ssp,
			  u_long ext_offset, u_long ext_size,
			  u_long base_ext_offset, int nsectors, int ntracks,
			  u_long mbr_offset));

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

	secpercyl = (u_long)nsectors * ntracks;
	chs_scyl = DPCYL(dp->dp_scyl, dp->dp_ssect);
	chs_ssect = DPSECT(dp->dp_ssect);
	ssector = chs_ssect - 1 + dp->dp_shd * nsectors + chs_scyl * secpercyl
		  + mbr_offset;
	ssector1 = offset + dp->dp_start;

	/*
	 * If ssector1 is on a cylinder >= 1024, then ssector can't be right.
	 * Allow the C/H/S for it to be 1023/ntracks-1/nsectors, or correct
	 * apart from the cylinder being reduced modulo 1024.
	 */
	if (ssector < ssector1
	    && ((chs_ssect == nsectors && dp->dp_shd == ntracks - 1
		 && chs_scyl == 1023)
		|| (ssector1 - ssector) % (1024 * secpercyl) == 0)
		|| (dp->dp_scyl == 255 && dp->dp_shd == 255
		    && dp->dp_ssect == 255)) {
		TRACE(("%s: C/H/S start %d/%d/%d, start %lu: allow\n",
		       sname, chs_scyl, dp->dp_shd, chs_ssect, ssector1));
		ssector = ssector1;
	}

	chs_ecyl = DPCYL(dp->dp_ecyl, dp->dp_esect);
	chs_esect = DPSECT(dp->dp_esect);
	esector = chs_esect - 1 + dp->dp_ehd * nsectors + chs_ecyl * secpercyl
		  + mbr_offset;
	esector1 = ssector1 + dp->dp_size - 1;

	/* Allow certain bogus C/H/S values for esector, as above. */
	if (esector < esector1
	    && ((chs_esect == nsectors && dp->dp_ehd == ntracks - 1
		 && chs_ecyl == 1023)
		|| (esector1 - esector) % (1024 * secpercyl) == 0)
		|| (dp->dp_ecyl == 255 && dp->dp_ehd == 255
		    && dp->dp_esect == 255)) {
		TRACE(("%s: C/H/S end %d/%d/%d, end %lu: allow\n",
		       sname, chs_ecyl, dp->dp_ehd, chs_esect, esector1));
		esector = esector1;
	}

	error = (ssector == ssector1 && esector == esector1) ? 0 : EINVAL;
	if (bootverbose)
		printf("%s: type 0x%x, start %lu, end = %lu, size %lu %s\n",
		       sname, dp->dp_typ, ssector1, esector1, dp->dp_size,
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
dsinit(dname, dev, strat, lp, sspp)
	char	*dname;
	dev_t	dev;
	d_strategy_t *strat;
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

	/*
	 * Allocate a dummy slices "struct" and initialize it to contain
	 * only an empty compatibility slice (pointing to itself) and a
	 * whole disk slice (covering the disk as described by the label).
	 * If there is an error, then the dummy struct becomes final.
	 */
	ssp = malloc(offsetof(struct diskslices, dss_slices)
		     + BASE_SLICE * sizeof *sp, M_DEVBUF, M_WAITOK);
	*sspp = ssp;
	ssp->dss_first_bsd_slice = COMPATIBILITY_SLICE;
	ssp->dss_nslices = BASE_SLICE;
	sp = &ssp->dss_slices[0];
	bzero(sp, BASE_SLICE * sizeof *sp);
	sp[WHOLE_DISK_SLICE].ds_size = lp->d_secperunit;

	mbr_offset = DOSBBSECTOR;
reread_mbr:
	/* Read master boot record. */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	bp->b_blkno = mbr_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_BUSY | B_READ;
	(*strat)(bp);
	if (biowait(bp) != 0) {
		diskerr(bp, dname, "error reading primary partition table",
			LOG_PRINTF, 0, lp);
		printf("\n");
		error = EIO;
		goto done;
	}

	/* Weakly verify it. */
	cp = bp->b_un.b_addr;
	sname = dsname(dname, dkunit(dev), WHOLE_DISK_SLICE, RAW_PART,
		       partname);
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		printf("%s: invalid primary partition table: no magic\n",
		       sname);
		error = EINVAL;
		goto done;
	}
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
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0
		    && dp->dp_start == 0 && dp->dp_size == 0)
			continue;
		sname = dsname(dname, dkunit(dev), BASE_SLICE + dospart,
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
		u_long	secperunit;

		lp->d_nsectors = max_nsectors;
		lp->d_ntracks = max_ntracks;
		lp->d_secpercyl = secpercyl;
		secperunit = secpercyl * max_ncyls;
		if (lp->d_secperunit < secperunit)
			lp->d_secperunit = secperunit;
		lp->d_ncylinders = lp->d_secperunit / secpercyl;
	}

	/*
	 * Free the dummy slices "struct" and allocate a real new one.
	 * Initialize special slices as above.
	 */
	free(ssp, M_DEVBUF);
	ssp = malloc(offsetof(struct diskslices, dss_slices)
#define	MAX_SLICES_SUPPORTED	MAX_SLICES  /* was (BASE_SLICE + NDOSPART) */
		     + MAX_SLICES_SUPPORTED * sizeof *sp, M_DEVBUF, M_WAITOK);
	*sspp = ssp;
	ssp->dss_first_bsd_slice = COMPATIBILITY_SLICE;
	sp = &ssp->dss_slices[0];
	bzero(sp, MAX_SLICES_SUPPORTED * sizeof *sp);
	sp[WHOLE_DISK_SLICE].ds_size = lp->d_secperunit;

	/* Initialize normal slices. */
	sp += BASE_SLICE;
	for (dospart = 0, dp = dp0; dospart < NDOSPART; dospart++, dp++, sp++) {
		sp->ds_offset = mbr_offset + dp->dp_start;
		sp->ds_size = dp->dp_size;
		sp->ds_type = dp->dp_typ;
#if 0
		lp->d_subtype |= (lp->d_subtype & 3) | dospart
				 | DSTYPE_INDOSPART;
#endif
	}
	ssp->dss_nslices = BASE_SLICE + NDOSPART;

	/* Handle extended partitions. */
	sp -= NDOSPART;
	for (dospart = 0; dospart < NDOSPART; dospart++, sp++)
		if (sp->ds_type == DOSPTYP_EXTENDED)
			extended(dname, bp->b_dev, strat, lp, ssp,
				 sp->ds_offset, sp->ds_size, sp->ds_offset,
				 max_nsectors, max_ntracks, mbr_offset);

done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
	if (error == EINVAL)
		error = 0;
	return (error);
}

void
extended(dname, dev, strat, lp, ssp, ext_offset, ext_size, base_ext_offset,
	 nsectors, ntracks, mbr_offset)
	char	*dname;
	dev_t	dev;
	struct disklabel *lp;
	d_strategy_t *strat;
	struct diskslices *ssp;
	u_long	ext_offset;
	u_long	ext_size;
	u_long	base_ext_offset;
	int	nsectors;
	int	ntracks;
	u_long	mbr_offset;
{
	struct buf *bp;
	u_char	*cp;
	int	dospart;
	struct dos_partition *dp;
	u_long	ext_offsets[NDOSPART];
	u_long	ext_sizes[NDOSPART];
	char	partname[2];
	int	slice;
	char	*sname;
	struct diskslice *sp;

	/* Read extended boot record. */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dev;
	bp->b_blkno = ext_offset;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags |= B_BUSY | B_READ;
	(*strat)(bp);
	if (biowait(bp) != 0) {
		diskerr(bp, dname, "error reading extended partition table",
			LOG_PRINTF, 0, lp);
		printf("\n");
		goto done;
	}

	/* Weakly verify it. */
	cp = bp->b_un.b_addr;
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		sname = dsname(dname, dkunit(dev), WHOLE_DISK_SLICE, RAW_PART,
			       partname);
		printf("%s: invalid extended partition table: no magic\n",
		       sname);
		goto done;
	}

	for (dospart = 0,
	     dp = (struct dos_partition *)(bp->b_un.b_addr + DOSPARTOFF),
	     slice = ssp->dss_nslices, sp = &ssp->dss_slices[slice];
	     dospart < NDOSPART; dospart++, dp++) {
		ext_sizes[dospart] = 0;
		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0
		    && dp->dp_start == 0 && dp->dp_size == 0)
			continue;
		if (dp->dp_typ == DOSPTYP_EXTENDED) {
			char buf[32];

			sname = dsname(dname, dkunit(dev), WHOLE_DISK_SLICE,
				       RAW_PART, partname);
			strcpy(buf, sname);
			if (strlen(buf) < sizeof buf - 11)
				strcat(buf, "<extended>");
			check_part(buf, dp, base_ext_offset, nsectors,
				   ntracks, mbr_offset);
			ext_offsets[dospart] = base_ext_offset + dp->dp_start;
			ext_sizes[dospart] = dp->dp_size;
		} else {
			sname = dsname(dname, dkunit(dev), slice, RAW_PART,
				       partname);
			check_part(sname, dp, ext_offset, nsectors, ntracks,
				   mbr_offset);
			if (slice >= MAX_SLICES) {
				printf("%s: too many slices\n", sname);
				slice++;
				continue;
			}
			sp->ds_offset = ext_offset + dp->dp_start;
			sp->ds_size = dp->dp_size;
			sp->ds_type = dp->dp_typ;
			ssp->dss_nslices++;
			slice++;
			sp++;
		}
	}

	/* If we found any more slices, recursively find all the subslices. */
	for (dospart = 0; dospart < NDOSPART; dospart++)
		if (ext_sizes[dospart] != 0)
			extended(dname, dev, strat, lp, ssp,
				 ext_offsets[dospart], ext_sizes[dospart],
				 base_ext_offset, nsectors, ntracks,
				 mbr_offset);

done:
	bp->b_flags |= B_INVAL | B_AGE;
	brelse(bp);
}
