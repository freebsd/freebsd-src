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
 *	$Id$
 */

#include <stddef.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/malloc.h>
#include <sys/systm.h>

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
	struct dos_partition *dp;
	struct dos_partition *dp0;
	int	error;
	int	max_nsectors;
	int	max_ntracks;
	u_long	secpercyl;
	int	slice;
	struct diskslice *sp;
	struct diskslices *ssp;

	/*
	 * Free old slices "struct", if any, and allocate a dummy new one.
	 */
	if (*sspp != NULL)
		free(*sspp, M_DEVBUF);
	ssp = malloc(offsetof(struct diskslices, dss_slices) + sizeof(*sp),
		     M_DEVBUF, M_WAITOK);
	*sspp = ssp;

	/*
	 * Initialize dummy slice.  If there is an error, this becomes the
	 * only slice, and no more restrictive than the (dummy) label.
	 */
	sp = &ssp->dss_slices[0];
	bzero(sp, sizeof *sp);
	sp->ds_size = lp->d_secperunit;
	ssp->dss_nslices = 1;

	/* Read master boot record. */
	bp = geteblk((int)lp->d_secsize);
	bp->b_dev = dkmodpart(dkmodslice(dev, WHOLE_DISK_SLICE), RAW_PART);
	bp->b_blkno = DOSBBSECTOR;
	bp->b_bcount = lp->d_secsize;
	bp->b_flags = B_BUSY | B_READ;
	(*strat)(bp);
	if (biowait(bp) != 0) {
		error = EIO;
		goto done;
	}

	/* Weakly verify it. */
	cp = bp->b_un.b_addr;
	if (cp[0x1FE] != 0x55 || cp[0x1FF] != 0xAA) {
		error = EINVAL;
		goto done;
	}

	/* Guess the geometry. */
	/*
	 * TODO:
	 * Perhaps skip entries with 0 size.
	 * Perhaps only look at entries of type DOSPTYP_386BSD.
	 */
	max_nsectors = 0;
	max_ntracks = 0;
	dp0 = (struct dos_partition *)(cp + DOSPARTOFF);
	for (dp = dp0, slice = 1; slice <= NDOSPART; dp++, slice++) {
		int nsectors;
		int ntracks;

		nsectors = DPSECT(dp->dp_esect);
		if (max_nsectors < nsectors)
			max_nsectors = nsectors;
		ntracks = dp->dp_ehd + 1;
		if (max_ntracks < ntracks)
			max_ntracks = ntracks;
	}

	/* Check the geometry. */
	/*
	 * TODO:
	 * As above.
	 * Check for overlaps.
	 * Check or adjust against d_ncylinders and d_secperunit.
	 */
	error = 0;
	secpercyl = max_nsectors * max_ntracks;
	for (dp = dp0, slice = 1; slice <= NDOSPART; dp++, slice++) {
		u_long esector;
		u_long esector1;
		u_long ssector;

		if (dp->dp_scyl == 0 && dp->dp_shd == 0 && dp->dp_ssect == 0
		    && dp->dp_start == 0 && dp->dp_size == 0)
			continue;
		ssector = DPSECT(dp->dp_ssect) - 1 + dp->dp_shd * max_nsectors
			  + DPCYL(dp->dp_scyl, dp->dp_ssect) * secpercyl;
		esector = DPSECT(dp->dp_esect) - 1 + dp->dp_ehd * max_nsectors
			  + DPCYL(dp->dp_ecyl, dp->dp_esect) * secpercyl;
		esector1 = dp->dp_start + dp->dp_size - 1;
		if (ssector != dp->dp_start || esector != esector1)
			error = EINVAL;
#if 1
		else
			printf("%s%ds%d: start %lu, end = %lu, size %lu: OK\n",
			       dname, dkunit(dev), slice, ssector, esector,
			       dp->dp_size);
		if (ssector != dp->dp_start)
			printf(
			"%s%ds%d: C/H/S start %lu != start %lu: invalid\n",
			       dname, dkunit(dev), slice, ssector,
			       dp->dp_start);
		if (esector != esector1)
			printf(
			"%s%ds%d: C/H/S end %lu != end %lu: invalid\n",
			       dname, dkunit(dev), slice, esector, esector1);
#endif
	}
	if (error != 0)
		goto done;

	/*
	 * We're not handling extended partitions yet, so there are always
	 * 1 + NDOSPART slices.
	 */
	if (dkslice(dev) >= 1 + NDOSPART) {
		error = ENXIO;
		goto done;
	}

	/*
	 * Accept the DOS partition table.
	 * Free dummy slices "struct" and allocate a real new one.
	 */
	free(ssp, M_DEVBUF);
	ssp = malloc(offsetof(struct diskslices, dss_slices)
		     + (1 + NDOSPART) * sizeof(*sp), M_DEVBUF, M_WAITOK);
	*sspp = ssp;
	sp = &ssp->dss_slices[0];
	bzero(sp, (1 + NDOSPART) * sizeof *sp);
	sp->ds_size = lp->d_secperunit;
	sp++;
	for (dp = dp0, slice = 1; slice <= NDOSPART; dp++, slice++, sp++) {
		sp->ds_offset = dp->dp_start;
		sp->ds_size = dp->dp_size;
	}
	ssp->dss_nslices = 1 + NDOSPART;
	if (max_nsectors != 0) {
		lp->d_nsectors = max_nsectors;
		lp->d_ntracks = max_ntracks;
		lp->d_secpercyl = secpercyl;
	}
#if 0
	lp->d_secperunit = what?;
	lp->d_subtype |= (lp->d_subtype & 3) + (slice - 1) | DSTYPE_INDOSPART;
#endif

done:
	bp->b_flags = B_INVAL | B_AGE;
	brelse(bp);
	if (error == EINVAL)
		error = (dkslice(dev) == WHOLE_DISK_SLICE ? 0 : ENXIO);
	return (error);
}
