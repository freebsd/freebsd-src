/* $Id: ccd.c,v 1.31 1998/02/22 10:01:23 jkh Exp $ */

/*	$NetBSD: ccd.c,v 1.22 1995/12/08 19:13:26 thorpej Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.
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
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 */

/*
 * "Concatenated" disk driver.
 *
 * Dynamic configuration and disklabel support by:
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 *	Numerical Aerodynamic Simulation Facility
 *	Mail Stop 258-6
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 */

#include "ccd.h"
#if NCCD > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/disklabel.h>
#include <ufs/ffs/fs.h> 
#include <sys/device.h>
#undef KERNEL			/* XXX */
#include <sys/disk.h>
#define KERNEL
#include <sys/fcntl.h>
#include <sys/vnode.h>

#include <sys/ccdvar.h>

#if defined(CCDDEBUG) && !defined(DEBUG)
#define DEBUG
#endif

#ifdef DEBUG
#define CCDB_FOLLOW	0x01
#define CCDB_INIT	0x02
#define CCDB_IO		0x04
#define CCDB_LABEL	0x08
#define CCDB_VNODE	0x10
static int ccddebug = CCDB_FOLLOW | CCDB_INIT | CCDB_IO | CCDB_LABEL |
    CCDB_VNODE;
SYSCTL_INT(_debug, OID_AUTO, ccddebug, CTLFLAG_RW, &ccddebug, 0, "");
#undef DEBUG
#endif

#define	ccdunit(x)	dkunit(x)
#define ccdpart(x)	dkpart(x)

/*
   This is how mirroring works (only writes are special):

   When initiating a write, ccdbuffer() returns two "struct ccdbuf *"s
   linked together by the cb_mirror field.  "cb_pflags &
   CCDPF_MIRROR_DONE" is set to 0 on both of them.

   When a component returns to ccdiodone(), it checks if "cb_pflags &
   CCDPF_MIRROR_DONE" is set or not.  If not, it sets the partner's
   flag and returns.  If it is, it means its partner has already
   returned, so it will go to the regular cleanup.

 */

struct ccdbuf {
	struct buf	cb_buf;		/* new I/O buf */
	struct buf	*cb_obp;	/* ptr. to original I/O buf */
	int		cb_unit;	/* target unit */
	int		cb_comp;	/* target component */
	int		cb_pflags;	/* mirror/parity status flag */
	struct ccdbuf	*cb_mirror;	/* mirror counterpart */
};

/* bits in cb_pflags */
#define CCDPF_MIRROR_DONE 1	/* if set, mirror counterpart is done */

#define	getccdbuf()		\
	((struct ccdbuf *)malloc(sizeof(struct ccdbuf), M_DEVBUF, M_WAITOK))
#define putccdbuf(cbp)		\
	free((caddr_t)(cbp), M_DEVBUF)

#define CCDLABELDEV(dev)	\
	(makedev(major((dev)), dkmakeminor(ccdunit((dev)), 0, RAW_PART)))

static d_open_t ccdopen;
static d_close_t ccdclose;
static d_strategy_t ccdstrategy;
static d_ioctl_t ccdioctl;
static d_dump_t ccddump;
static d_psize_t ccdsize;

#define CDEV_MAJOR 74
#define BDEV_MAJOR 21

static struct cdevsw ccd_cdevsw;
static struct bdevsw ccd_bdevsw = {
  ccdopen, ccdclose, ccdstrategy, ccdioctl,
  ccddump, ccdsize, 0,
  "ccd", &ccd_cdevsw, -1
};

/* Called by main() during pseudo-device attachment */
static void	ccdattach __P((void *));
PSEUDO_SET(ccdattach, ccd);

/* called by biodone() at interrupt time */
static	void ccdiodone __P((struct ccdbuf *cbp));

static	void ccdstart __P((struct ccd_softc *, struct buf *));
static	void ccdinterleave __P((struct ccd_softc *, int));
static	void ccdintr __P((struct ccd_softc *, struct buf *));
static	int ccdinit __P((struct ccddevice *, char **, struct proc *));
static	int ccdlookup __P((char *, struct proc *p, struct vnode **));
static	void ccdbuffer __P((struct ccdbuf **ret, struct ccd_softc *,
		struct buf *, daddr_t, caddr_t, long));
static	void ccdgetdisklabel __P((dev_t));
static	void ccdmakedisklabel __P((struct ccd_softc *));
static	int ccdlock __P((struct ccd_softc *));
static	void ccdunlock __P((struct ccd_softc *));

#ifdef DEBUG
static	void printiinfo __P((struct ccdiinfo *));
#endif

/* Non-private for the benefit of libkvm. */
struct	ccd_softc *ccd_softc;
struct	ccddevice *ccddevs;
static	int numccd = 0;

static int ccd_devsw_installed = 0;

/*
 * Number of blocks to untouched in front of a component partition.
 * This is to avoid violating its disklabel area when it starts at the
 * beginning of the slice.
 */
#if !defined(CCD_OFFSET)
#define CCD_OFFSET 16
#endif

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is allocate enough space for devices to be configured later, and
 * add devsw entries.
 */
static void
ccdattach(dummy)
	void *dummy;
{
	int i;
	int num = NCCD;

	if (num > 1)
		printf("ccd0-%d: Concatenated disk drivers\n", num-1);
	else
		printf("ccd0: Concatenated disk driver\n");

	ccd_softc = (struct ccd_softc *)malloc(num * sizeof(struct ccd_softc),
	    M_DEVBUF, M_NOWAIT);
	ccddevs = (struct ccddevice *)malloc(num * sizeof(struct ccddevice),
	    M_DEVBUF, M_NOWAIT);
	if ((ccd_softc == NULL) || (ccddevs == NULL)) {
		printf("WARNING: no memory for concatenated disks\n");
		if (ccd_softc != NULL)
			free(ccd_softc, M_DEVBUF);
		if (ccddevs != NULL)
			free(ccddevs, M_DEVBUF);
		return;
	}
	numccd = num;
	bzero(ccd_softc, num * sizeof(struct ccd_softc));
	bzero(ccddevs, num * sizeof(struct ccddevice));

	/* XXX: is this necessary? */
	for (i = 0; i < numccd; ++i)
		ccddevs[i].ccd_dk = -1;

	if( ! ccd_devsw_installed ) {
		bdevsw_add_generic(BDEV_MAJOR,CDEV_MAJOR, &ccd_bdevsw);
		ccd_devsw_installed = 1;
    	}
	else {
		printf("huh?\n");
	}
}

static int
ccdinit(ccd, cpaths, p)
	struct ccddevice *ccd;
	char **cpaths;
	struct proc *p;
{
	register struct ccd_softc *cs = &ccd_softc[ccd->ccd_unit];
	register struct ccdcinfo *ci = NULL;	/* XXX */
	register size_t size;
	register int ix;
	struct vnode *vp;
	struct vattr va;
	size_t minsize;
	int maxsecsize;
	struct partinfo dpart;
	struct ccdgeom *ccg = &cs->sc_geom;
	char tmppath[MAXPATHLEN];
	int error;

#ifdef DEBUG
	if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
		printf("ccdinit: unit %d\n", ccd->ccd_unit);
#endif

#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
	cs->sc_dk = ccd->ccd_dk;
#endif
	cs->sc_size = 0;
	cs->sc_ileave = ccd->ccd_interleave;
	cs->sc_nccdisks = ccd->ccd_ndev;

	/* Allocate space for the component info. */
	cs->sc_cinfo = malloc(cs->sc_nccdisks * sizeof(struct ccdcinfo),
	    M_DEVBUF, M_WAITOK);

	/*
	 * Verify that each component piece exists and record
	 * relevant information about it.
	 */
	maxsecsize = 0;
	minsize = 0;
	for (ix = 0; ix < cs->sc_nccdisks; ix++) {
		vp = ccd->ccd_vpp[ix];
		ci = &cs->sc_cinfo[ix];
		ci->ci_vp = vp;

		/*
		 * Copy in the pathname of the component.
		 */
		bzero(tmppath, sizeof(tmppath));	/* sanity */
		if (error = copyinstr(cpaths[ix], tmppath,
		    MAXPATHLEN, &ci->ci_pathlen)) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("ccd%d: can't copy path, error = %d\n",
				    ccd->ccd_unit, error);
#endif
			while (ci > cs->sc_cinfo) {
				ci--;
				free(ci->ci_path, M_DEVBUF);
			}
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_path = malloc(ci->ci_pathlen, M_DEVBUF, M_WAITOK);
		bcopy(tmppath, ci->ci_path, ci->ci_pathlen);

		/*
		 * XXX: Cache the component's dev_t.
		 */
		if (error = VOP_GETATTR(vp, &va, p->p_ucred, p)) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("ccd%d: %s: getattr failed %s = %d\n",
				    ccd->ccd_unit, ci->ci_path,
				    "error", error);
#endif
			while (ci >= cs->sc_cinfo) {
				free(ci->ci_path, M_DEVBUF);
				ci--;
			}
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		ci->ci_dev = va.va_rdev;

		/*
		 * Get partition information for the component.
		 */
		if (error = VOP_IOCTL(vp, DIOCGPART, (caddr_t)&dpart,
		    FREAD, p->p_ucred, p)) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				 printf("ccd%d: %s: ioctl failed, error = %d\n",
				     ccd->ccd_unit, ci->ci_path, error);
#endif
			while (ci >= cs->sc_cinfo) {
				free(ci->ci_path, M_DEVBUF);
				ci--;
			}
			free(cs->sc_cinfo, M_DEVBUF);
			return (error);
		}
		if (dpart.part->p_fstype == FS_BSDFFS) {
			maxsecsize =
			    ((dpart.disklab->d_secsize > maxsecsize) ?
			    dpart.disklab->d_secsize : maxsecsize);
			size = dpart.part->p_size - CCD_OFFSET;
		} else {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("ccd%d: %s: incorrect partition type\n",
				    ccd->ccd_unit, ci->ci_path);
#endif
			while (ci >= cs->sc_cinfo) {
				free(ci->ci_path, M_DEVBUF);
				ci--;
			}
			free(cs->sc_cinfo, M_DEVBUF);
			return (EFTYPE);
		}

		/*
		 * Calculate the size, truncating to an interleave
		 * boundary if necessary.
		 */

		if (cs->sc_ileave > 1)
			size -= size % cs->sc_ileave;

		if (size == 0) {
#ifdef DEBUG
			if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
				printf("ccd%d: %s: size == 0\n",
				    ccd->ccd_unit, ci->ci_path);
#endif
			while (ci >= cs->sc_cinfo) {
				free(ci->ci_path, M_DEVBUF);
				ci--;
			}
			free(cs->sc_cinfo, M_DEVBUF);
			return (ENODEV);
		}

		if (minsize == 0 || size < minsize)
			minsize = size;
		ci->ci_size = size;
		cs->sc_size += size;
	}

	/*
	 * Don't allow the interleave to be smaller than
	 * the biggest component sector.
	 */
	if ((cs->sc_ileave > 0) &&
	    (cs->sc_ileave < (maxsecsize / DEV_BSIZE))) {
#ifdef DEBUG
		if (ccddebug & (CCDB_FOLLOW|CCDB_INIT))
			printf("ccd%d: interleave must be at least %d\n",
			    ccd->ccd_unit, (maxsecsize / DEV_BSIZE));
#endif
		while (ci >= cs->sc_cinfo) {
			free(ci->ci_path, M_DEVBUF);
			ci--;
		}
		free(cs->sc_cinfo, M_DEVBUF);
		return (EINVAL);
	}

	/*
	 * If uniform interleave is desired set all sizes to that of
	 * the smallest component.
	 */
	if (ccd->ccd_flags & CCDF_UNIFORM) {
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			ci->ci_size = minsize;
		if (ccd->ccd_flags & CCDF_MIRROR) {
			/*
			 * Check to see if an even number of components
			 * have been specified.
			 */
			if (cs->sc_nccdisks % 2) {
				printf("ccd%d: mirroring requires an even number of disks\n", ccd->ccd_unit );
				while (ci > cs->sc_cinfo) {
					ci--;
					free(ci->ci_path, M_DEVBUF);
				}
				free(cs->sc_cinfo, M_DEVBUF);
				return (EINVAL);
			}
			cs->sc_size = (cs->sc_nccdisks/2) * minsize;
		}
		else if (ccd->ccd_flags & CCDF_PARITY)
			cs->sc_size = (cs->sc_nccdisks-1) * minsize;
		else
			cs->sc_size = cs->sc_nccdisks * minsize;
	}

	/*
	 * Construct the interleave table.
	 */
	ccdinterleave(cs, ccd->ccd_unit);

	/*
	 * Create pseudo-geometry based on 1MB cylinders.  It's
	 * pretty close.
	 */
	ccg->ccg_secsize = maxsecsize;
	ccg->ccg_ntracks = 1;
	ccg->ccg_nsectors = 1024 * (1024 / ccg->ccg_secsize);
	ccg->ccg_ncylinders = cs->sc_size / ccg->ccg_nsectors;

#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
	if (ccd->ccd_dk >= 0)
		dk_wpms[ccd->ccd_dk] = 32 * (60 * DEV_BSIZE / 2);     /* XXX */
#endif

	cs->sc_flags |= CCDF_INITED;
	cs->sc_cflags = ccd->ccd_flags;	/* So we can find out later... */
	cs->sc_unit = ccd->ccd_unit;
	return (0);
}

static void
ccdinterleave(cs, unit)
	register struct ccd_softc *cs;
	int unit;
{
	register struct ccdcinfo *ci, *smallci;
	register struct ccdiinfo *ii;
	register daddr_t bn, lbn;
	register int ix;
	u_long size;

#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printf("ccdinterleave(%x): ileave %d\n", cs, cs->sc_ileave);
#endif
	/*
	 * Allocate an interleave table.
	 * Chances are this is too big, but we don't care.
	 */
	size = (cs->sc_nccdisks + 1) * sizeof(struct ccdiinfo);
	cs->sc_itable = (struct ccdiinfo *)malloc(size, M_DEVBUF, M_WAITOK);
	bzero((caddr_t)cs->sc_itable, size);

	/*
	 * Trivial case: no interleave (actually interleave of disk size).
	 * Each table entry represents a single component in its entirety.
	 */
	if (cs->sc_ileave == 0) {
		bn = 0;
		ii = cs->sc_itable;

		for (ix = 0; ix < cs->sc_nccdisks; ix++) {
			/* Allocate space for ii_index. */
			ii->ii_index = malloc(sizeof(int), M_DEVBUF, M_WAITOK);
			ii->ii_ndisk = 1;
			ii->ii_startblk = bn;
			ii->ii_startoff = 0;
			ii->ii_index[0] = ix;
			bn += cs->sc_cinfo[ix].ci_size;
			ii++;
		}
		ii->ii_ndisk = 0;
#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			printiinfo(cs->sc_itable);
#endif
		return;
	}

	/*
	 * The following isn't fast or pretty; it doesn't have to be.
	 */
	size = 0;
	bn = lbn = 0;
	for (ii = cs->sc_itable; ; ii++) {
		/* Allocate space for ii_index. */
		ii->ii_index = malloc((sizeof(int) * cs->sc_nccdisks),
		    M_DEVBUF, M_WAITOK);

		/*
		 * Locate the smallest of the remaining components
		 */
		smallci = NULL;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size > size &&
			    (smallci == NULL ||
			     ci->ci_size < smallci->ci_size))
				smallci = ci;

		/*
		 * Nobody left, all done
		 */
		if (smallci == NULL) {
			ii->ii_ndisk = 0;
			break;
		}

		/*
		 * Record starting logical block and component offset
		 */
		ii->ii_startblk = bn / cs->sc_ileave;
		ii->ii_startoff = lbn;

		/*
		 * Determine how many disks take part in this interleave
		 * and record their indices.
		 */
		ix = 0;
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++)
			if (ci->ci_size >= smallci->ci_size)
				ii->ii_index[ix++] = ci - cs->sc_cinfo;
		ii->ii_ndisk = ix;
		bn += ix * (smallci->ci_size - size);
		lbn = smallci->ci_size / cs->sc_ileave;
		size = smallci->ci_size;
	}
#ifdef DEBUG
	if (ccddebug & CCDB_INIT)
		printiinfo(cs->sc_itable);
#endif
}

/* ARGSUSED */
static int
ccdopen(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	struct disklabel *lp;
	int error = 0, part, pmask;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdopen(%x, %x)\n", dev, flags);
#endif
	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if (error = ccdlock(cs))
		return (error);

	lp = &cs->sc_dkdev.dk_label;

	part = ccdpart(dev);
	pmask = (1 << part);

	/*
	 * If we're initialized, check to see if there are any other
	 * open partitions.  If not, then it's safe to update
	 * the in-core disklabel.
	 */
	if ((cs->sc_flags & CCDF_INITED) && (cs->sc_dkdev.dk_openmask == 0))
		ccdgetdisklabel(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART && ((part >= lp->d_npartitions) ||
	    (lp->d_partitions[part].p_fstype == FS_UNUSED))) {
		error = ENXIO;
		goto done;
	}

	/* Prevent our unit from being unconfigured while open. */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask |= pmask;
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask |= pmask;
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

 done:
	ccdunlock(cs);
	return (0);
}

/* ARGSUSED */
static int
ccdclose(dev, flags, fmt, p)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs;
	int error = 0, part;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdclose(%x, %x)\n", dev, flags);
#endif

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	if (error = ccdlock(cs))
		return (error);

	part = ccdpart(dev);

	/* ...that much closer to allowing unconfiguration... */
	switch (fmt) {
	case S_IFCHR:
		cs->sc_dkdev.dk_copenmask &= ~(1 << part);
		break;

	case S_IFBLK:
		cs->sc_dkdev.dk_bopenmask &= ~(1 << part);
		break;
	}
	cs->sc_dkdev.dk_openmask =
	    cs->sc_dkdev.dk_copenmask | cs->sc_dkdev.dk_bopenmask;

	ccdunlock(cs);
	return (0);
}

static void
ccdstrategy(bp)
	register struct buf *bp;
{
	register int unit = ccdunit(bp->b_dev);
	register struct ccd_softc *cs = &ccd_softc[unit];
	register int s;
	int wlabel;
	struct disklabel *lp;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdstrategy(%x): unit %d\n", bp, unit);
#endif
	if ((cs->sc_flags & CCDF_INITED) == 0) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* If it's a nil transfer, wake up the top half now. */
	if (bp->b_bcount == 0)
		goto done;

	lp = &cs->sc_dkdev.dk_label;

	/*
	 * Do bounds checking and adjust transfer.  If there's an
	 * error, the bounds check will flag that for us.
	 */
	wlabel = cs->sc_flags & (CCDF_WLABEL|CCDF_LABELLING);
	if (ccdpart(bp->b_dev) != RAW_PART)
		if (bounds_check_with_label(bp, lp, wlabel) <= 0)
			goto done;

	bp->b_resid = bp->b_bcount;

	/*
	 * "Start" the unit.
	 */
	s = splbio();
	ccdstart(cs, bp);
	splx(s);
	return;
done:
	biodone(bp);
}

static void
ccdstart(cs, bp)
	register struct ccd_softc *cs;
	register struct buf *bp;
{
	register long bcount, rcount;
	struct ccdbuf *cbp[4];
	/* XXX! : 2 reads and 2 writes for RAID 4/5 */
	caddr_t addr;
	daddr_t bn;
	struct partition *pp;

#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdstart(%x, %x)\n", cs, bp);
#endif

#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
	/*
	 * Instrumentation (not very meaningful)
	 */
	cs->sc_nactive++;
	if (cs->sc_dk >= 0) {
		dk_busy |= 1 << cs->sc_dk;
		dk_xfer[cs->sc_dk]++;
		dk_wds[cs->sc_dk] += bp->b_bcount >> 6;
	}
#endif

	/*
	 * Translate the partition-relative block number to an absolute.
	 */
	bn = bp->b_blkno;
	if (ccdpart(bp->b_dev) != RAW_PART) {
		pp = &cs->sc_dkdev.dk_label.d_partitions[ccdpart(bp->b_dev)];
		bn += pp->p_offset;
	}

	/*
	 * Allocate component buffers and fire off the requests
	 */
	addr = bp->b_data;
	for (bcount = bp->b_bcount; bcount > 0; bcount -= rcount) {
		ccdbuffer(cbp, cs, bp, bn, addr, bcount);
		rcount = cbp[0]->cb_buf.b_bcount;
		if ((cbp[0]->cb_buf.b_flags & B_READ) == 0)
			cbp[0]->cb_buf.b_vp->v_numoutput++;
		VOP_STRATEGY(&cbp[0]->cb_buf);
		if (cs->sc_cflags & CCDF_MIRROR &&
		    (cbp[0]->cb_buf.b_flags & B_READ) == 0) {
			/* mirror, start another write */
			cbp[1]->cb_buf.b_vp->v_numoutput++;
			VOP_STRATEGY(&cbp[1]->cb_buf);
		}
		bn += btodb(rcount);
		addr += rcount;
	}
}

/*
 * Build a component buffer header.
 */
static void
ccdbuffer(cb, cs, bp, bn, addr, bcount)
	register struct ccdbuf **cb;
	register struct ccd_softc *cs;
	struct buf *bp;
	daddr_t bn;
	caddr_t addr;
	long bcount;
{
	register struct ccdcinfo *ci, *ci2 = NULL;	/* XXX */
	register struct ccdbuf *cbp;
	register daddr_t cbn, cboff;

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf("ccdbuffer(%x, %x, %d, %x, %d)\n",
		       cs, bp, bn, addr, bcount);
#endif
	/*
	 * Determine which component bn falls in.
	 */
	cbn = bn;
	cboff = 0;

	/*
	 * Serially concatenated
	 */
	if (cs->sc_ileave == 0) {
		register daddr_t sblk;

		sblk = 0;
		for (ci = cs->sc_cinfo; cbn >= sblk + ci->ci_size; ci++)
			sblk += ci->ci_size;
		cbn -= sblk;
	}
	/*
	 * Interleaved
	 */
	else {
		register struct ccdiinfo *ii;
		int ccdisk, off;

		cboff = cbn % cs->sc_ileave;
		cbn /= cs->sc_ileave;
		for (ii = cs->sc_itable; ii->ii_ndisk; ii++)
			if (ii->ii_startblk > cbn)
				break;
		ii--;
		off = cbn - ii->ii_startblk;
		if (ii->ii_ndisk == 1) {
			ccdisk = ii->ii_index[0];
			cbn = ii->ii_startoff + off;
		} else {
			if (cs->sc_cflags & CCDF_MIRROR) {
				ccdisk = ii->ii_index[off % (ii->ii_ndisk/2)];
				cbn = ii->ii_startoff + off / (ii->ii_ndisk/2);
				/* mirrored data */
				ci2 = &cs->sc_cinfo[ccdisk + ii->ii_ndisk/2];
			}
			else if (cs->sc_cflags & CCDF_PARITY) {
				ccdisk = ii->ii_index[off % (ii->ii_ndisk-1)];
				cbn = ii->ii_startoff + off / (ii->ii_ndisk-1);
				if (cbn % ii->ii_ndisk <= ccdisk)
					ccdisk++;
			}
			else {
				ccdisk = ii->ii_index[off % ii->ii_ndisk];
				cbn = ii->ii_startoff + off / ii->ii_ndisk;
			}
		}
		cbn *= cs->sc_ileave;
		ci = &cs->sc_cinfo[ccdisk];
	}

	/*
	 * Fill in the component buf structure.
	 */
	cbp = getccdbuf();
	bzero(cbp, sizeof (struct ccdbuf));
	cbp->cb_buf.b_flags = bp->b_flags | B_CALL;
	cbp->cb_buf.b_iodone = (void (*)(struct buf *))ccdiodone;
	cbp->cb_buf.b_proc = bp->b_proc;
	cbp->cb_buf.b_dev = ci->ci_dev;		/* XXX */
	cbp->cb_buf.b_blkno = cbn + cboff + CCD_OFFSET;
	cbp->cb_buf.b_data = addr;
	cbp->cb_buf.b_vp = ci->ci_vp;
	LIST_INIT(&cbp->cb_buf.b_dep);
	if (cs->sc_ileave == 0)
		cbp->cb_buf.b_bcount = dbtob(ci->ci_size - cbn);
	else
		cbp->cb_buf.b_bcount = dbtob(cs->sc_ileave - cboff);
	if (cbp->cb_buf.b_bcount > bcount)
		cbp->cb_buf.b_bcount = bcount;

 	cbp->cb_buf.b_bufsize = cbp->cb_buf.b_bcount;

	/*
	 * context for ccdiodone
	 */
	cbp->cb_obp = bp;
	cbp->cb_unit = cs - ccd_softc;
	cbp->cb_comp = ci - cs->sc_cinfo;

#ifdef DEBUG
	if (ccddebug & CCDB_IO)
		printf(" dev %x(u%d): cbp %x bn %d addr %x bcnt %d\n",
		       ci->ci_dev, ci-cs->sc_cinfo, cbp, cbp->cb_buf.b_blkno,
		       cbp->cb_buf.b_data, cbp->cb_buf.b_bcount);
#endif
	cb[0] = cbp;
	if (cs->sc_cflags & CCDF_MIRROR &&
	    (cbp->cb_buf.b_flags & B_READ) == 0) {
		/* mirror, start one more write */
		cbp = getccdbuf();
		bzero(cbp, sizeof (struct ccdbuf));
		*cbp = *cb[0];
		cbp->cb_buf.b_dev = ci2->ci_dev;
		cbp->cb_buf.b_vp = ci2->ci_vp;
		LIST_INIT(&cbp->cb_buf.b_dep);
		cbp->cb_comp = ci2 - cs->sc_cinfo;
		cb[1] = cbp;
		/* link together the ccdbuf's and clear "mirror done" flag */
		cb[0]->cb_mirror = cb[1];
		cb[1]->cb_mirror = cb[0];
		cb[0]->cb_pflags &= ~CCDPF_MIRROR_DONE;
		cb[1]->cb_pflags &= ~CCDPF_MIRROR_DONE;
	}
}

static void
ccdintr(cs, bp)
	register struct ccd_softc *cs;
	register struct buf *bp;
{
#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdintr(%x, %x)\n", cs, bp);
#endif
	/*
	 * Request is done for better or worse, wakeup the top half.
	 */
#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
	--cs->sc_nactive;
#ifdef DIAGNOSTIC
	if (cs->sc_nactive < 0)
		panic("ccdintr: ccd%d: sc_nactive < 0", cs->sc_unit);
#endif

	if (cs->sc_nactive == 0 && cs->sc_dk >= 0)
		dk_busy &= ~(1 << cs->sc_dk);
#endif
	if (bp->b_flags & B_ERROR)
		bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Called at interrupt time.
 * Mark the component as done and if all components are done,
 * take a ccd interrupt.
 */
static void
ccdiodone(cbp)
	struct ccdbuf *cbp;
{
	register struct buf *bp = cbp->cb_obp;
	register int unit = cbp->cb_unit;
	int count, s;

	s = splbio();
#ifdef DEBUG
	if (ccddebug & CCDB_FOLLOW)
		printf("ccdiodone(%x)\n", cbp);
	if (ccddebug & CCDB_IO) {
		printf("ccdiodone: bp %x bcount %d resid %d\n",
		       bp, bp->b_bcount, bp->b_resid);
		printf(" dev %x(u%d), cbp %x bn %d addr %x bcnt %d\n",
		       cbp->cb_buf.b_dev, cbp->cb_comp, cbp,
		       cbp->cb_buf.b_blkno, cbp->cb_buf.b_data,
		       cbp->cb_buf.b_bcount);
	}
#endif

	if (cbp->cb_buf.b_flags & B_ERROR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = cbp->cb_buf.b_error ? cbp->cb_buf.b_error : EIO;
#ifdef DEBUG
		printf("ccd%d: error %d on component %d\n",
		       unit, bp->b_error, cbp->cb_comp);
#endif
	}

	if (ccd_softc[unit].sc_cflags & CCDF_MIRROR &&
	    (cbp->cb_buf.b_flags & B_READ) == 0)
		if ((cbp->cb_pflags & CCDPF_MIRROR_DONE) == 0) {
			/* I'm done before my counterpart, so just set
			   partner's flag and return */
			cbp->cb_mirror->cb_pflags |= CCDPF_MIRROR_DONE;
			putccdbuf(cbp);
			splx(s);
			return;
		}
		
	count = cbp->cb_buf.b_bcount;
	putccdbuf(cbp);

	/*
	 * If all done, "interrupt".
	 */
	bp->b_resid -= count;
	if (bp->b_resid < 0)
		panic("ccdiodone: count");
	if (bp->b_resid == 0)
		ccdintr(&ccd_softc[unit], bp);
	splx(s);
}

static int
ccdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = ccdunit(dev);
	int i, j, lookedup = 0, error = 0;
	int part, pmask, s;
	struct ccd_softc *cs;
	struct ccd_ioctl *ccio = (struct ccd_ioctl *)data;
	struct ccddevice ccd;
	char **cpp;
	struct vnode **vpp;
#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
	extern int dkn;
#endif

	if (unit >= numccd)
		return (ENXIO);
	cs = &ccd_softc[unit];

	bzero(&ccd, sizeof(ccd));

	switch (cmd) {
	case CCDIOCSET:
		if (cs->sc_flags & CCDF_INITED)
			return (EBUSY);

		if ((flag & FWRITE) == 0)
			return (EBADF);

		if (error = ccdlock(cs))
			return (error);

		/* Fill in some important bits. */
		ccd.ccd_unit = unit;
		ccd.ccd_interleave = ccio->ccio_ileave;
		if (ccd.ccd_interleave == 0 &&
		    ((ccio->ccio_flags & CCDF_MIRROR) ||
		     (ccio->ccio_flags & CCDF_PARITY))) {
			printf("ccd%d: disabling mirror/parity, interleave is 0\n", unit);
			ccio->ccio_flags &= ~(CCDF_MIRROR | CCDF_PARITY);
		}
		if ((ccio->ccio_flags & CCDF_MIRROR) &&
		    (ccio->ccio_flags & CCDF_PARITY)) {
			printf("ccd%d: can't specify both mirror and parity, using mirror\n", unit);
			ccio->ccio_flags &= ~CCDF_PARITY;
		}
		if ((ccio->ccio_flags & (CCDF_MIRROR | CCDF_PARITY)) &&
		    !(ccio->ccio_flags & CCDF_UNIFORM)) {
			printf("ccd%d: mirror/parity forces uniform flag\n",
			       unit);
			ccio->ccio_flags |= CCDF_UNIFORM;
		}
		ccd.ccd_flags = ccio->ccio_flags & CCDF_USERMASK;

		/*
		 * Allocate space for and copy in the array of
		 * componet pathnames and device numbers.
		 */
		cpp = malloc(ccio->ccio_ndisks * sizeof(char *),
		    M_DEVBUF, M_WAITOK);
		vpp = malloc(ccio->ccio_ndisks * sizeof(struct vnode *),
		    M_DEVBUF, M_WAITOK);

		error = copyin((caddr_t)ccio->ccio_disks, (caddr_t)cpp,
		    ccio->ccio_ndisks * sizeof(char **));
		if (error) {
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

#ifdef DEBUG
		if (ccddebug & CCDB_INIT)
			for (i = 0; i < ccio->ccio_ndisks; ++i)
				printf("ccdioctl: component %d: 0x%x\n",
				    i, cpp[i]);
#endif

		for (i = 0; i < ccio->ccio_ndisks; ++i) {
#ifdef DEBUG
			if (ccddebug & CCDB_INIT)
				printf("ccdioctl: lookedup = %d\n", lookedup);
#endif
			if (error = ccdlookup(cpp[i], p, &vpp[i])) {
				for (j = 0; j < lookedup; ++j)
					(void)vn_close(vpp[j], FREAD|FWRITE,
					    p->p_ucred, p);
				free(vpp, M_DEVBUF);
				free(cpp, M_DEVBUF);
				ccdunlock(cs);
				return (error);
			}
			++lookedup;
		}
		ccd.ccd_cpp = cpp;
		ccd.ccd_vpp = vpp;
		ccd.ccd_ndev = ccio->ccio_ndisks;

#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
		/*
		 * Assign disk index first so that init routine
		 * can use it (saves having the driver drag around
		 * the ccddevice pointer just to set up the dk_*
		 * info in the open routine).
		 */
		if (dkn < DK_NDRIVE)
			ccd.ccd_dk = dkn++;
		else
			ccd.ccd_dk = -1;
#endif

		/*
		 * Initialize the ccd.  Fills in the softc for us.
		 */
		if (error = ccdinit(&ccd, cpp, p)) {
#ifdef WORKING_DISK_STATISTICS		/* XXX !! */
			if (ccd.ccd_dk >= 0)
				--dkn;
#endif
			for (j = 0; j < lookedup; ++j)
				(void)vn_close(vpp[j], FREAD|FWRITE,
				    p->p_ucred, p);
			bzero(&ccd_softc[unit], sizeof(struct ccd_softc));
			free(vpp, M_DEVBUF);
			free(cpp, M_DEVBUF);
			ccdunlock(cs);
			return (error);
		}

		/*
		 * The ccd has been successfully initialized, so
		 * we can place it into the array and read the disklabel.
		 */
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));
		ccio->ccio_unit = unit;
		ccio->ccio_size = cs->sc_size;
		ccdgetdisklabel(dev);

		ccdunlock(cs);

		break;

	case CCDIOCCLR:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);

		if ((flag & FWRITE) == 0)
			return (EBADF);

		if (error = ccdlock(cs))
			return (error);

		/*
		 * Don't unconfigure if any other partitions are open
		 * or if both the character and block flavors of this
		 * partition are open.
		 */
		part = ccdpart(dev);
		pmask = (1 << part);
		if ((cs->sc_dkdev.dk_openmask & ~pmask) ||
		    ((cs->sc_dkdev.dk_bopenmask & pmask) &&
		    (cs->sc_dkdev.dk_copenmask & pmask))) {
			ccdunlock(cs);
			return (EBUSY);
		}

		/*
		 * Free ccd_softc information and clear entry.
		 */

		/* Close the components and free their pathnames. */
		for (i = 0; i < cs->sc_nccdisks; ++i) {
			/*
			 * XXX: this close could potentially fail and
			 * cause Bad Things.  Maybe we need to force
			 * the close to happen?
			 */
#ifdef DEBUG
			if (ccddebug & CCDB_VNODE)
				vprint("CCDIOCCLR: vnode info",
				    cs->sc_cinfo[i].ci_vp);
#endif
			(void)vn_close(cs->sc_cinfo[i].ci_vp, FREAD|FWRITE,
			    p->p_ucred, p);
			free(cs->sc_cinfo[i].ci_path, M_DEVBUF);
		}

		/* Free interleave index. */
		for (i = 0; cs->sc_itable[i].ii_ndisk; ++i)
			free(cs->sc_itable[i].ii_index, M_DEVBUF);

		/* Free component info and interleave table. */
		free(cs->sc_cinfo, M_DEVBUF);
		free(cs->sc_itable, M_DEVBUF);
		cs->sc_flags &= ~CCDF_INITED;

		/*
		 * Free ccddevice information and clear entry.
		 */
		free(ccddevs[unit].ccd_cpp, M_DEVBUF);
		free(ccddevs[unit].ccd_vpp, M_DEVBUF);
		ccd.ccd_dk = -1;
		bcopy(&ccd, &ccddevs[unit], sizeof(ccd));

		/* This must be atomic. */
		s = splhigh();
		ccdunlock(cs);
		bzero(cs, sizeof(struct ccd_softc));
		splx(s);

		break;

	case DIOCGDINFO:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);

		*(struct disklabel *)data = cs->sc_dkdev.dk_label;
		break;

	case DIOCGPART:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);

		((struct partinfo *)data)->disklab = &cs->sc_dkdev.dk_label;
		((struct partinfo *)data)->part =
		    &cs->sc_dkdev.dk_label.d_partitions[ccdpart(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);

		if ((flag & FWRITE) == 0)
			return (EBADF);

		if (error = ccdlock(cs))
			return (error);

		cs->sc_flags |= CCDF_LABELLING;

		error = setdisklabel(&cs->sc_dkdev.dk_label,
		    (struct disklabel *)data, 0);
				/*, &cs->sc_dkdev.dk_cpulabel); */
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(CCDLABELDEV(dev),
				    ccdstrategy, &cs->sc_dkdev.dk_label);
				/*
				    &cs->sc_dkdev.dk_cpulabel); */
		}

		cs->sc_flags &= ~CCDF_LABELLING;

		ccdunlock(cs);

		if (error)
			return (error);
		break;

	case DIOCWLABEL:
		if ((cs->sc_flags & CCDF_INITED) == 0)
			return (ENXIO);

		if ((flag & FWRITE) == 0)
			return (EBADF);
		if (*(int *)data != 0)
			cs->sc_flags |= CCDF_WLABEL;
		else
			cs->sc_flags &= ~CCDF_WLABEL;
		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

static int
ccdsize(dev)
	dev_t dev;
{
	struct ccd_softc *cs;
	int part, size;

	if (ccdopen(dev, 0, S_IFBLK, curproc))
		return (-1);

	cs = &ccd_softc[ccdunit(dev)];
	part = ccdpart(dev);

	if ((cs->sc_flags & CCDF_INITED) == 0)
		return (-1);

	if (cs->sc_dkdev.dk_label.d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = cs->sc_dkdev.dk_label.d_partitions[part].p_size;

	if (ccdclose(dev, 0, S_IFBLK, curproc))
		return (-1);

	return (size);
}

static int
ccddump(dev)
	dev_t dev;
{

	/* Not implemented. */
	return ENXIO;
}

/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 */
static int
ccdlookup(path, p, vpp)
	char *path;
	struct proc *p;
	struct vnode **vpp;	/* result */
{
	struct nameidata nd;
	struct vnode *vp;
	struct vattr va;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, path, p);
	if (error = vn_open(&nd, FREAD|FWRITE, 0)) {
#ifdef DEBUG
		if (ccddebug & CCDB_FOLLOW|CCDB_INIT)
			printf("ccdlookup: vn_open error = %d\n", error);
#endif
		return (error);
	}
	vp = nd.ni_vp;

	if (vp->v_usecount > 1) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (EBUSY);
	}

	if (error = VOP_GETATTR(vp, &va, p->p_ucred, p)) {
#ifdef DEBUG
		if (ccddebug & CCDB_FOLLOW|CCDB_INIT)
			printf("ccdlookup: getattr error = %d\n", error);
#endif
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (error);
	}

	/* XXX: eventually we should handle VREG, too. */
	if (va.va_type != VBLK) {
		VOP_UNLOCK(vp, 0, p);
		(void)vn_close(vp, FREAD|FWRITE, p->p_ucred, p);
		return (ENOTBLK);
	}

#ifdef DEBUG
	if (ccddebug & CCDB_VNODE)
		vprint("ccdlookup: vnode info", vp);
#endif

	VOP_UNLOCK(vp, 0, p);
	*vpp = vp;
	return (0);
}

/*
 * Read the disklabel from the ccd.  If one is not present, fake one
 * up.
 */
static void
ccdgetdisklabel(dev)
	dev_t dev;
{
	int unit = ccdunit(dev);
	struct ccd_softc *cs = &ccd_softc[unit];
	char *errstring;
	struct disklabel *lp = &cs->sc_dkdev.dk_label;
	struct ccdgeom *ccg = &cs->sc_geom;

	bzero(lp, sizeof(*lp));

	lp->d_secperunit = cs->sc_size;
	lp->d_secsize = ccg->ccg_secsize;
	lp->d_nsectors = ccg->ccg_nsectors;
	lp->d_ntracks = ccg->ccg_ntracks;
	lp->d_ncylinders = ccg->ccg_ncylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "ccd", sizeof(lp->d_typename));
	lp->d_type = DTYPE_CCD;
	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = cs->sc_size;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_bbsize = BBSIZE;				/* XXX */
	lp->d_sbsize = SBSIZE;				/* XXX */

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(&cs->sc_dkdev.dk_label);

	/*
	 * Call the generic disklabel extraction routine.
	 */
	if (errstring = readdisklabel(CCDLABELDEV(dev), ccdstrategy,
	    &cs->sc_dkdev.dk_label))
		ccdmakedisklabel(cs);

#ifdef DEBUG
	/* It's actually extremely common to have unlabeled ccds. */
	if (ccddebug & CCDB_LABEL)
		if (errstring != NULL)
			printf("ccd%d: %s\n", unit, errstring);
#endif
}

/*
 * Take care of things one might want to take care of in the event
 * that a disklabel isn't present.
 */
static void
ccdmakedisklabel(cs)
	struct ccd_softc *cs;
{
	struct disklabel *lp = &cs->sc_dkdev.dk_label;

	/*
	 * For historical reasons, if there's no disklabel present
	 * the raw partition must be marked FS_BSDFFS.
	 */
	lp->d_partitions[RAW_PART].p_fstype = FS_BSDFFS;

	strncpy(lp->d_packname, "default label", sizeof(lp->d_packname));
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
static int
ccdlock(cs)
	struct ccd_softc *cs;
{
	int error;

	while ((cs->sc_flags & CCDF_LOCKED) != 0) {
		cs->sc_flags |= CCDF_WANTED;
		if ((error = tsleep(cs, PRIBIO | PCATCH, "ccdlck", 0)) != 0)
			return (error);
	}
	cs->sc_flags |= CCDF_LOCKED;
	return (0);
}

/*
 * Unlock and wake up any waiters.
 */
static void
ccdunlock(cs)
	struct ccd_softc *cs;
{

	cs->sc_flags &= ~CCDF_LOCKED;
	if ((cs->sc_flags & CCDF_WANTED) != 0) {
		cs->sc_flags &= ~CCDF_WANTED;
		wakeup(cs);
	}
}

#ifdef DEBUG
static void
printiinfo(ii)
	struct ccdiinfo *ii;
{
	register int ix, i;

	for (ix = 0; ii->ii_ndisk; ix++, ii++) {
		printf(" itab[%d]: #dk %d sblk %d soff %d",
		       ix, ii->ii_ndisk, ii->ii_startblk, ii->ii_startoff);
		for (i = 0; i < ii->ii_ndisk; i++)
			printf(" %d", ii->ii_index[i]);
		printf("\n");
	}
}
#endif

#endif /* NCCD > 0 */

/* Local Variables: */
/* c-argdecl-indent: 8 */
/* c-continued-statement-offset: 8 */
/* c-indent-level: 8 */
/* End: */
