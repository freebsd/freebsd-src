/*
 * Copyright (c) 2003 Poul-Henning Kamp.
 * Copyright (c) 1995 Jason R. Thorpe.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 * Copyright (c) 1988 University of Utah.
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
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The names of the authors may not be used to endorse or promote products
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
 *
 * Dynamic configuration and disklabel support by:
 *	Jason R. Thorpe <thorpej@nas.nasa.gov>
 *	Numerical Aerodynamic Simulation Facility
 *	Mail Stop 258-6
 *	NASA Ames Research Center
 *	Moffett Field, CA 94035
 *
 * from: Utah $Hdr: cd.c 1.6 90/11/28$
 *
 *	@(#)cd.c	8.2 (Berkeley) 11/16/93
 *
 *	$NetBSD: ccd.c,v 1.22 1995/12/08 19:13:26 thorpej Exp $ 
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>

#include <sys/ccdvar.h>

MALLOC_DEFINE(M_CCD, "CCD driver", "Concatenated Disk driver");

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
	struct bio	cb_buf;		/* new I/O buf */
	struct bio	*cb_obp;	/* ptr. to original I/O buf */
	struct ccdbuf	*cb_freenext;	/* free list link */
	struct ccd_s	*cb_softc;
	int		cb_comp;	/* target component */
	int		cb_pflags;	/* mirror/parity status flag */
	struct ccdbuf	*cb_mirror;	/* mirror counterpart */
};

/* bits in cb_pflags */
#define CCDPF_MIRROR_DONE 1	/* if set, mirror counterpart is done */

/* convinient macros for often-used statements */
#define IS_ALLOCATED(unit)	(ccdfind(unit) != NULL)
#define IS_INITED(cs)		(((cs)->sc_flags & CCDF_INITED) != 0)

static dev_t	ccdctldev;

static d_open_t ccdopen;
static d_close_t ccdclose;
static d_strategy_t ccdstrategy;
static d_ioctl_t ccdctlioctl;

#define NCCDFREEHIWAT	16

#define CDEV_MAJOR 74

static struct cdevsw ccdctl_cdevsw = {
	/* open */	nullopen,
	/* close */	nullclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	ccdctlioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ccdctl",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0
};

static struct cdevsw ccd_cdevsw = {
	/* open */	ccdopen,
	/* close */	ccdclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	noioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	ccdstrategy,
	/* name */	"ccd",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
};

static struct cdevsw ccddisk_cdevsw;

static LIST_HEAD(, ccd_s) ccd_softc_list =
	LIST_HEAD_INITIALIZER(&ccd_softc_list);

static struct ccd_s *ccdfind(int);
static struct ccd_s *ccdnew(int);
static int ccddestroy(struct ccd_s *);

/* called during module initialization */
static void ccdattach(void);
static int ccd_modevent(module_t, int, void *);

/* called by biodone() at interrupt time */
static void ccdiodone(struct bio *bp);

static void ccdstart(struct ccd_s *, struct bio *);
static void ccdinterleave(struct ccd_s *, int);
static int ccdinit(struct ccd_s *, char **, struct thread *);
static int ccdlookup(char *, struct thread *p, struct vnode **);
static int ccdbuffer(struct ccdbuf **ret, struct ccd_s *,
		      struct bio *, daddr_t, caddr_t, long);
static int ccdlock(struct ccd_s *);
static void ccdunlock(struct ccd_s *);


/*
 * Number of blocks to untouched in front of a component partition.
 * This is to avoid violating its disklabel area when it starts at the
 * beginning of the slice.
 */
#if !defined(CCD_OFFSET)
#define CCD_OFFSET 16
#endif

static struct ccd_s *
ccdfind(int unit)
{
	struct ccd_s *sc = NULL;

	/* XXX: LOCK(unique unit numbers) */
	LIST_FOREACH(sc, &ccd_softc_list, list) {
		if (sc->sc_unit == unit)
			break;
	}
	/* XXX: UNLOCK(unique unit numbers) */
	return ((sc == NULL) || (sc->sc_unit != unit) ? NULL : sc);
}

static struct ccd_s *
ccdnew(int unit)
{
	struct ccd_s *sc;

	/* XXX: LOCK(unique unit numbers) */
	if (IS_ALLOCATED(unit) || unit > 32)
		return (NULL);

	MALLOC(sc, struct ccd_s *, sizeof(*sc), M_CCD, M_ZERO);
	sc->sc_unit = unit;
	LIST_INSERT_HEAD(&ccd_softc_list, sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	return (sc);
}

static int
ccddestroy(struct ccd_s *sc)
{

	/* XXX: LOCK(unique unit numbers) */
	LIST_REMOVE(sc, list);
	/* XXX: UNLOCK(unique unit numbers) */
	FREE(sc, M_CCD);
	return (0);
}

/*
 * Called by main() during pseudo-device attachment.  All we need
 * to do is to add devsw entries.
 */
static void
ccdattach()
{

	ccdctldev = make_dev(&ccdctl_cdevsw, 0xffff00ff,
		UID_ROOT, GID_OPERATOR, 0640, "ccd.ctl");
	ccdctldev->si_drv1 = ccdctldev;
}

static int
ccd_modevent(module_t mod, int type, void *data)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		ccdattach();
		break;

	case MOD_UNLOAD:
		printf("ccd0: Unload not supported!\n");
		error = EOPNOTSUPP;
		break;

	case MOD_SHUTDOWN:
		break;

	default:
		error = EOPNOTSUPP;
	}
	return (error);
}

DEV_MODULE(ccd, ccd_modevent, NULL);

static int
ccdinit(struct ccd_s *cs, char **cpaths, struct thread *td)
{
	struct ccdcinfo *ci = NULL;	/* XXX */
	size_t size;
	int ix;
	struct vnode *vp;
	size_t minsize;
	int maxsecsize;
	struct ccdgeom *ccg = &cs->sc_geom;
	char *tmppath = NULL;
	int error = 0;
	off_t mediasize;
	u_int sectorsize;


	cs->sc_size = 0;

	/* Allocate space for the component info. */
	cs->sc_cinfo = malloc(cs->sc_nccdisks * sizeof(struct ccdcinfo),
	    M_CCD, 0);

	/*
	 * Verify that each component piece exists and record
	 * relevant information about it.
	 */
	maxsecsize = 0;
	minsize = 0;
	tmppath = malloc(MAXPATHLEN, M_CCD, 0);
	for (ix = 0; ix < cs->sc_nccdisks; ix++) {
		vp = cs->sc_vpp[ix];
		ci = &cs->sc_cinfo[ix];
		ci->ci_vp = vp;

		/*
		 * Copy in the pathname of the component.
		 */
		if ((error = copyinstr(cpaths[ix], tmppath,
		    MAXPATHLEN, &ci->ci_pathlen)) != 0) {
			goto fail;
		}
		ci->ci_path = malloc(ci->ci_pathlen, M_CCD, 0);
		bcopy(tmppath, ci->ci_path, ci->ci_pathlen);

		ci->ci_dev = vn_todev(vp);

		/*
		 * Get partition information for the component.
		 */
		error = VOP_IOCTL(vp, DIOCGMEDIASIZE, (caddr_t)&mediasize,
		    FREAD, td->td_ucred, td);
		if (error != 0) {
			goto fail;
		}
		/*
		 * Get partition information for the component.
		 */
		error = VOP_IOCTL(vp, DIOCGSECTORSIZE, (caddr_t)&sectorsize,
		    FREAD, td->td_ucred, td);
		if (error != 0) {
			goto fail;
		}
		if (sectorsize > maxsecsize)
			maxsecsize = sectorsize;
		size = mediasize / DEV_BSIZE - CCD_OFFSET;

		/*
		 * Calculate the size, truncating to an interleave
		 * boundary if necessary.
		 */

		if (cs->sc_ileave > 1)
			size -= size % cs->sc_ileave;

		if (size == 0) {
			error = ENODEV;
			goto fail;
		}

		if (minsize == 0 || size < minsize)
			minsize = size;
		ci->ci_size = size;
		cs->sc_size += size;
	}

	free(tmppath, M_CCD);
	tmppath = NULL;

	/*
	 * Don't allow the interleave to be smaller than
	 * the biggest component sector.
	 */
	if ((cs->sc_ileave > 0) &&
	    (cs->sc_ileave < (maxsecsize / DEV_BSIZE))) {
		error = EINVAL;
		goto fail;
	}

	/*
	 * If uniform interleave is desired set all sizes to that of
	 * the smallest component.  This will guarentee that a single
	 * interleave table is generated.
	 *
	 * Lost space must be taken into account when calculating the
	 * overall size.  Half the space is lost when CCDF_MIRROR is
	 * specified.
	 */
	if (cs->sc_flags & CCDF_UNIFORM) {
		for (ci = cs->sc_cinfo;
		     ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++) {
			ci->ci_size = minsize;
		}
		if (cs->sc_flags & CCDF_MIRROR) {
			/*
			 * Check to see if an even number of components
			 * have been specified.  The interleave must also
			 * be non-zero in order for us to be able to 
			 * guarentee the topology.
			 */
			if (cs->sc_nccdisks % 2) {
				printf("ccd%d: mirroring requires an even number of disks\n", cs->sc_unit );
				error = EINVAL;
				goto fail;
			}
			if (cs->sc_ileave == 0) {
				printf("ccd%d: an interleave must be specified when mirroring\n", cs->sc_unit);
				error = EINVAL;
				goto fail;
			}
			cs->sc_size = (cs->sc_nccdisks/2) * minsize;
		} else {
			if (cs->sc_ileave == 0) {
				printf("ccd%d: an interleave must be specified when using parity\n", cs->sc_unit);
				error = EINVAL;
				goto fail;
			}
			cs->sc_size = cs->sc_nccdisks * minsize;
		}
	}

	/*
	 * Construct the interleave table.
	 */
	ccdinterleave(cs, cs->sc_unit);

	/*
	 * Create pseudo-geometry based on 1MB cylinders.  It's
	 * pretty close.
	 */
	ccg->ccg_secsize = maxsecsize;
	ccg->ccg_ntracks = 1;
	ccg->ccg_nsectors = 1024 * 1024 / ccg->ccg_secsize;
	ccg->ccg_ncylinders = cs->sc_size / ccg->ccg_nsectors;

	/*
	 * Add a devstat entry for this device.
	 */
	devstat_add_entry(&cs->device_stats, "ccd", cs->sc_unit,
			  ccg->ccg_secsize, DEVSTAT_ALL_SUPPORTED,
			  DEVSTAT_TYPE_STORARRAY |DEVSTAT_TYPE_IF_OTHER,
			  DEVSTAT_PRIORITY_ARRAY);

	cs->sc_flags |= CCDF_INITED;
	cs->sc_cflags = cs->sc_flags;	/* So we can find out later... */
	return (0);
fail:
	while (ci > cs->sc_cinfo) {
		ci--;
		free(ci->ci_path, M_CCD);
	}
	if (tmppath != NULL)
		free(tmppath, M_CCD);
	free(cs->sc_cinfo, M_CCD);
	ccddestroy(cs);
	return (error);
}

static void
ccdinterleave(struct ccd_s *cs, int unit)
{
	struct ccdcinfo *ci, *smallci;
	struct ccdiinfo *ii;
	daddr_t bn, lbn;
	int ix;
	u_long size;


	/*
	 * Allocate an interleave table.  The worst case occurs when each
	 * of N disks is of a different size, resulting in N interleave
	 * tables.
	 *
	 * Chances are this is too big, but we don't care.
	 */
	size = (cs->sc_nccdisks + 1) * sizeof(struct ccdiinfo);
	cs->sc_itable = (struct ccdiinfo *)malloc(size, M_CCD,
	    M_ZERO);

	/*
	 * Trivial case: no interleave (actually interleave of disk size).
	 * Each table entry represents a single component in its entirety.
	 *
	 * An interleave of 0 may not be used with a mirror setup.
	 */
	if (cs->sc_ileave == 0) {
		bn = 0;
		ii = cs->sc_itable;

		for (ix = 0; ix < cs->sc_nccdisks; ix++) {
			/* Allocate space for ii_index. */
			ii->ii_index = malloc(sizeof(int), M_CCD, 0);
			ii->ii_ndisk = 1;
			ii->ii_startblk = bn;
			ii->ii_startoff = 0;
			ii->ii_index[0] = ix;
			bn += cs->sc_cinfo[ix].ci_size;
			ii++;
		}
		ii->ii_ndisk = 0;
		return;
	}

	/*
	 * The following isn't fast or pretty; it doesn't have to be.
	 */
	size = 0;
	bn = lbn = 0;
	for (ii = cs->sc_itable; ; ii++) {
		/*
		 * Allocate space for ii_index.  We might allocate more then
		 * we use.
		 */
		ii->ii_index = malloc((sizeof(int) * cs->sc_nccdisks),
		    M_CCD, 0);

		/*
		 * Locate the smallest of the remaining components
		 */
		smallci = NULL;
		for (ci = cs->sc_cinfo; ci < &cs->sc_cinfo[cs->sc_nccdisks]; 
		    ci++) {
			if (ci->ci_size > size &&
			    (smallci == NULL ||
			     ci->ci_size < smallci->ci_size)) {
				smallci = ci;
			}
		}

		/*
		 * Nobody left, all done
		 */
		if (smallci == NULL) {
			ii->ii_ndisk = 0;
			free(ii->ii_index, M_CCD);
			break;
		}

		/*
		 * Record starting logical block using an sc_ileave blocksize.
		 */
		ii->ii_startblk = bn / cs->sc_ileave;

		/*
		 * Record starting comopnent block using an sc_ileave 
		 * blocksize.  This value is relative to the beginning of
		 * a component disk.
		 */
		ii->ii_startoff = lbn;

		/*
		 * Determine how many disks take part in this interleave
		 * and record their indices.
		 */
		ix = 0;
		for (ci = cs->sc_cinfo; 
		    ci < &cs->sc_cinfo[cs->sc_nccdisks]; ci++) {
			if (ci->ci_size >= smallci->ci_size) {
				ii->ii_index[ix++] = ci - cs->sc_cinfo;
			}
		}
		ii->ii_ndisk = ix;
		bn += ix * (smallci->ci_size - size);
		lbn = smallci->ci_size / cs->sc_ileave;
		size = smallci->ci_size;
	}
}

static int
ccdopen(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct ccd_s *cs;

	cs = dev->si_drv1;
	cs->sc_openmask = 1;
	return (0);
}

/* ARGSUSED */
static int
ccdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
	struct ccd_s *cs;

	cs = dev->si_drv1;
	cs->sc_openmask = 0;
	return (0);
}

static void
ccdstrategy(struct bio *bp)
{
	struct ccd_s *cs;
	int pbn;        /* in sc_secsize chunks */
	long sz;        /* in sc_secsize chunks */

	cs = bp->bio_dev->si_drv1;

	pbn = bp->bio_blkno / (cs->sc_geom.ccg_secsize / DEV_BSIZE);
	sz = howmany(bp->bio_bcount, cs->sc_geom.ccg_secsize);

	/*
	 * If out of bounds return an error. If at the EOF point,
	 * simply read or write less.
	 */

	if (pbn < 0 || pbn >= cs->sc_size) {
		bp->bio_resid = bp->bio_bcount;
		if (pbn != cs->sc_size)
			biofinish(bp, NULL, EINVAL);
		else
			biodone(bp);
		return;
	}

	/*
	 * If the request crosses EOF, truncate the request.
	 */
	if (pbn + sz > cs->sc_size) {
		bp->bio_bcount = (cs->sc_size - pbn) * 
		    cs->sc_geom.ccg_secsize;
	}

	bp->bio_resid = bp->bio_bcount;

	/*
	 * "Start" the unit.
	 */
	ccdstart(cs, bp);
	return;
}

static void
ccdstart(struct ccd_s *cs, struct bio *bp)
{
	long bcount, rcount;
	struct ccdbuf *cbp[2];
	caddr_t addr;
	daddr_t bn;
	int err;


	/* Record the transaction start  */
	devstat_start_transaction(&cs->device_stats);

	/*
	 * Translate the partition-relative block number to an absolute.
	 */
	bn = bp->bio_blkno;

	/*
	 * Allocate component buffers and fire off the requests
	 */
	addr = bp->bio_data;
	for (bcount = bp->bio_bcount; bcount > 0; bcount -= rcount) {
		err = ccdbuffer(cbp, cs, bp, bn, addr, bcount);
		if (err) {
			printf("ccdbuffer error %d\n", err);
			/* We're screwed */
			bp->bio_resid -= bcount;
			bp->bio_error = ENOMEM;
			bp->bio_flags |= BIO_ERROR;
			return;
		}
		rcount = cbp[0]->cb_buf.bio_bcount;

		if (cs->sc_cflags & CCDF_MIRROR) {
			/*
			 * Mirroring.  Writes go to both disks, reads are
			 * taken from whichever disk seems most appropriate.
			 *
			 * We attempt to localize reads to the disk whos arm
			 * is nearest the read request.  We ignore seeks due
			 * to writes when making this determination and we
			 * also try to avoid hogging.
			 */
			if (cbp[0]->cb_buf.bio_cmd == BIO_WRITE) {
				BIO_STRATEGY(&cbp[0]->cb_buf);
				BIO_STRATEGY(&cbp[1]->cb_buf);
			} else {
				int pick = cs->sc_pick;
				daddr_t range = cs->sc_size / 16;

				if (bn < cs->sc_blk[pick] - range ||
				    bn > cs->sc_blk[pick] + range
				) {
					cs->sc_pick = pick = 1 - pick;
				}
				cs->sc_blk[pick] = bn + btodb(rcount);
				BIO_STRATEGY(&cbp[pick]->cb_buf);
			}
		} else {
			/*
			 * Not mirroring
			 */
			BIO_STRATEGY(&cbp[0]->cb_buf);
		}
		bn += btodb(rcount);
		addr += rcount;
	}
}

/*
 * Build a component buffer header.
 */
static int
ccdbuffer(struct ccdbuf **cb, struct ccd_s *cs, struct bio *bp, daddr_t bn, caddr_t addr, long bcount)
{
	struct ccdcinfo *ci, *ci2 = NULL;	/* XXX */
	struct ccdbuf *cbp;
	daddr_t cbn, cboff;
	off_t cbc;

	/*
	 * Determine which component bn falls in.
	 */
	cbn = bn;
	cboff = 0;

	if (cs->sc_ileave == 0) {
		/*
		 * Serially concatenated and neither a mirror nor a parity
		 * config.  This is a special case.
		 */
		daddr_t sblk;

		sblk = 0;
		for (ci = cs->sc_cinfo; cbn >= sblk + ci->ci_size; ci++)
			sblk += ci->ci_size;
		cbn -= sblk;
	} else {
		struct ccdiinfo *ii;
		int ccdisk, off;

		/*
		 * Calculate cbn, the logical superblock (sc_ileave chunks),
		 * and cboff, a normal block offset (DEV_BSIZE chunks) relative
		 * to cbn.
		 */
		cboff = cbn % cs->sc_ileave;	/* DEV_BSIZE gran */
		cbn = cbn / cs->sc_ileave;	/* DEV_BSIZE * ileave gran */

		/*
		 * Figure out which interleave table to use.
		 */
		for (ii = cs->sc_itable; ii->ii_ndisk; ii++) {
			if (ii->ii_startblk > cbn)
				break;
		}
		ii--;

		/*
		 * off is the logical superblock relative to the beginning 
		 * of this interleave block.  
		 */
		off = cbn - ii->ii_startblk;

		/*
		 * We must calculate which disk component to use (ccdisk),
		 * and recalculate cbn to be the superblock relative to
		 * the beginning of the component.  This is typically done by
		 * adding 'off' and ii->ii_startoff together.  However, 'off'
		 * must typically be divided by the number of components in
		 * this interleave array to be properly convert it from a
		 * CCD-relative logical superblock number to a 
		 * component-relative superblock number.
		 */
		if (ii->ii_ndisk == 1) {
			/*
			 * When we have just one disk, it can't be a mirror
			 * or a parity config.
			 */
			ccdisk = ii->ii_index[0];
			cbn = ii->ii_startoff + off;
		} else {
			if (cs->sc_cflags & CCDF_MIRROR) {
				/*
				 * We have forced a uniform mapping, resulting
				 * in a single interleave array.  We double
				 * up on the first half of the available
				 * components and our mirror is in the second
				 * half.  This only works with a single 
				 * interleave array because doubling up
				 * doubles the number of sectors, so there
				 * cannot be another interleave array because
				 * the next interleave array's calculations
				 * would be off.
				 */
				int ndisk2 = ii->ii_ndisk / 2;
				ccdisk = ii->ii_index[off % ndisk2];
				cbn = ii->ii_startoff + off / ndisk2;
				ci2 = &cs->sc_cinfo[ccdisk + ndisk2];
			} else {
				ccdisk = ii->ii_index[off % ii->ii_ndisk];
				cbn = ii->ii_startoff + off / ii->ii_ndisk;
			}
		}

		ci = &cs->sc_cinfo[ccdisk];

		/*
		 * Convert cbn from a superblock to a normal block so it
		 * can be used to calculate (along with cboff) the normal
		 * block index into this particular disk.
		 */
		cbn *= cs->sc_ileave;
	}

	/*
	 * Fill in the component buf structure.
	 */
	cbp = malloc(sizeof(struct ccdbuf), M_CCD, M_NOWAIT | M_ZERO);
	if (cbp == NULL)
		return (ENOMEM);
	cbp->cb_buf.bio_cmd = bp->bio_cmd;
	cbp->cb_buf.bio_done = ccdiodone;
	cbp->cb_buf.bio_dev = ci->ci_dev;		/* XXX */
	cbp->cb_buf.bio_blkno = cbn + cboff + CCD_OFFSET;
	cbp->cb_buf.bio_offset = dbtob(cbn + cboff + CCD_OFFSET);
	cbp->cb_buf.bio_data = addr;
	cbp->cb_buf.bio_caller2 = cbp;
	if (cs->sc_ileave == 0)
              cbc = dbtob((off_t)(ci->ci_size - cbn));
	else
              cbc = dbtob((off_t)(cs->sc_ileave - cboff));
	cbp->cb_buf.bio_bcount = (cbc < bcount) ? cbc : bcount;
 	cbp->cb_buf.bio_caller1 = (void*)cbp->cb_buf.bio_bcount;

	/*
	 * context for ccdiodone
	 */
	cbp->cb_obp = bp;
	cbp->cb_softc = cs;
	cbp->cb_comp = ci - cs->sc_cinfo;

	cb[0] = cbp;

	/*
	 * Note: both I/O's setup when reading from mirror, but only one
	 * will be executed.
	 */
	if (cs->sc_cflags & CCDF_MIRROR) {
		/* mirror, setup second I/O */
		cbp = malloc(sizeof(struct ccdbuf), M_CCD, M_NOWAIT);
		if (cbp == NULL) {
			free(cb[0], M_CCD);
			cb[0] = NULL;
			return (ENOMEM);
		}
		bcopy(cb[0], cbp, sizeof(struct ccdbuf));
		cbp->cb_buf.bio_dev = ci2->ci_dev;
		cbp->cb_comp = ci2 - cs->sc_cinfo;
		cb[1] = cbp;
		/* link together the ccdbuf's and clear "mirror done" flag */
		cb[0]->cb_mirror = cb[1];
		cb[1]->cb_mirror = cb[0];
		cb[0]->cb_pflags &= ~CCDPF_MIRROR_DONE;
		cb[1]->cb_pflags &= ~CCDPF_MIRROR_DONE;
	}
	return (0);
}

/*
 * Called at interrupt time.
 * Mark the component as done and if all components are done,
 * take a ccd interrupt.
 */
static void
ccdiodone(struct bio *ibp)
{
	struct ccdbuf *cbp;
	struct bio *bp;
	struct ccd_s *cs;
	int count;

	cbp = ibp->bio_caller2;
	cs = cbp->cb_softc;
	bp = cbp->cb_obp;
	/*
	 * If an error occured, report it.  If this is a mirrored 
	 * configuration and the first of two possible reads, do not
	 * set the error in the bp yet because the second read may
	 * succeed.
	 */

	if (cbp->cb_buf.bio_flags & BIO_ERROR) {
		const char *msg = "";

		if ((cs->sc_cflags & CCDF_MIRROR) &&
		    (cbp->cb_buf.bio_cmd == BIO_READ) &&
		    (cbp->cb_pflags & CCDPF_MIRROR_DONE) == 0) {
			/*
			 * We will try our read on the other disk down
			 * below, also reverse the default pick so if we 
			 * are doing a scan we do not keep hitting the
			 * bad disk first.
			 */

			msg = ", trying other disk";
			cs->sc_pick = 1 - cs->sc_pick;
			cs->sc_blk[cs->sc_pick] = bp->bio_blkno;
		} else {
			bp->bio_flags |= BIO_ERROR;
			bp->bio_error = cbp->cb_buf.bio_error ? 
			    cbp->cb_buf.bio_error : EIO;
		}
		printf("ccd%d: error %d on component %d block %jd "
		    "(ccd block %jd)%s\n", cs->sc_unit, bp->bio_error,
		    cbp->cb_comp, 
		    (intmax_t)cbp->cb_buf.bio_blkno, (intmax_t)bp->bio_blkno,
		    msg);
	}

	/*
	 * Process mirror.  If we are writing, I/O has been initiated on both
	 * buffers and we fall through only after both are finished.
	 *
	 * If we are reading only one I/O is initiated at a time.  If an
	 * error occurs we initiate the second I/O and return, otherwise 
	 * we free the second I/O without initiating it.
	 */

	if (cs->sc_cflags & CCDF_MIRROR) {
		if (cbp->cb_buf.bio_cmd == BIO_WRITE) {
			/*
			 * When writing, handshake with the second buffer
			 * to determine when both are done.  If both are not
			 * done, return here.
			 */
			if ((cbp->cb_pflags & CCDPF_MIRROR_DONE) == 0) {
				cbp->cb_mirror->cb_pflags |= CCDPF_MIRROR_DONE;
				free(cbp, M_CCD);
				return;
			}
		} else {
			/*
			 * When reading, either dispose of the second buffer
			 * or initiate I/O on the second buffer if an error 
			 * occured with this one.
			 */
			if ((cbp->cb_pflags & CCDPF_MIRROR_DONE) == 0) {
				if (cbp->cb_buf.bio_flags & BIO_ERROR) {
					cbp->cb_mirror->cb_pflags |= 
					    CCDPF_MIRROR_DONE;
					BIO_STRATEGY(&cbp->cb_mirror->cb_buf);
					free(cbp, M_CCD);
					return;
				} else {
					free(cbp->cb_mirror, M_CCD);
				}
			}
		}
	}

	/*
	 * use bio_caller1 to determine how big the original request was rather
	 * then bio_bcount, because bio_bcount may have been truncated for EOF.
	 *
	 * XXX We check for an error, but we do not test the resid for an
	 * aligned EOF condition.  This may result in character & block
	 * device access not recognizing EOF properly when read or written 
	 * sequentially, but will not effect filesystems.
	 */
	count = (long)cbp->cb_buf.bio_caller1;
	free(cbp, M_CCD);

	/*
	 * If all done, "interrupt".
	 */
	bp->bio_resid -= count;
	if (bp->bio_resid < 0)
		panic("ccdiodone: count");
	if (bp->bio_resid == 0) {
		if (bp->bio_flags & BIO_ERROR)
			bp->bio_resid = bp->bio_bcount;
		biofinish(bp, &cs->device_stats, 0);
	}
}

static int ccdioctltoo(int unit, u_long cmd, caddr_t data, int flag, struct thread *td);

static int
ccdctlioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	struct ccd_ioctl *ccio;
	u_int unit;
	dev_t dev2;
	int error;

	switch (cmd) {
	case CCDIOCSET:
	case CCDIOCCLR:
		ccio = (struct ccd_ioctl *)data;
		unit = ccio->ccio_size;
		return (ccdioctltoo(unit, cmd, data, flag, td));
	case CCDCONFINFO:
		{
		int ninit = 0;
		struct ccdconf *conf = (struct ccdconf *)data;
		struct ccd_s *tmpcs;
		struct ccd_s *ubuf = conf->buffer;

		/* XXX: LOCK(unique unit numbers) */
		LIST_FOREACH(tmpcs, &ccd_softc_list, list)
			if (IS_INITED(tmpcs))
				ninit++;

		if (conf->size == 0) {
			conf->size = sizeof(struct ccd_s) * ninit;
			return (0);
		} else if ((conf->size / sizeof(struct ccd_s) != ninit) ||
		    (conf->size % sizeof(struct ccd_s) != 0)) {
			/* XXX: UNLOCK(unique unit numbers) */
			return (EINVAL);
		}

		ubuf += ninit;
		LIST_FOREACH(tmpcs, &ccd_softc_list, list) {
			if (!IS_INITED(tmpcs))
				continue;
			error = copyout(tmpcs, --ubuf,
			    sizeof(struct ccd_s));
			if (error != 0)
				/* XXX: UNLOCK(unique unit numbers) */
				return (error);
		}
		/* XXX: UNLOCK(unique unit numbers) */
		return (0);
		}

	case CCDCPPINFO:
		{
		struct ccdcpps *cpps = (struct ccdcpps *)data;
		char *ubuf = cpps->buffer;
		struct ccd_s *cs;

	
		error = copyin(ubuf, &unit, sizeof (unit));
		if (error)
			return (error);

		if (!IS_ALLOCATED(unit))
			return (ENXIO);
		dev2 = makedev(CDEV_MAJOR, unit * 8 + 2);
		cs = ccdfind(unit);
		if (!IS_INITED(cs))
			return (ENXIO);

		{
			int len = 0, i;
			struct ccdcpps *cpps = (struct ccdcpps *)data;
			char *ubuf = cpps->buffer;


			for (i = 0; i < cs->sc_nccdisks; ++i)
				len += cs->sc_cinfo[i].ci_pathlen;

			if (cpps->size < len)
				return (ENOMEM);

			for (i = 0; i < cs->sc_nccdisks; ++i) {
				len = cs->sc_cinfo[i].ci_pathlen;
				error = copyout(cs->sc_cinfo[i].ci_path, ubuf,
				    len);
				if (error != 0)
					return (error);
				ubuf += len;
			}
			return(copyout("", ubuf, 1));
		}
		break;
		}

	default:
		return (ENXIO);
	}
}

static int
ccdioctltoo(int unit, u_long cmd, caddr_t data, int flag, struct thread *td)
{
	int i, j, lookedup = 0, error = 0;
	struct ccd_s *cs;
	struct ccd_ioctl *ccio = (struct ccd_ioctl *)data;
	struct ccdgeom *ccg;
	char **cpp;
	struct vnode **vpp;

	cs = ccdfind(unit);
	switch (cmd) {
	case CCDIOCSET:
		if (cs == NULL)
			cs = ccdnew(unit);
		if (IS_INITED(cs))
			return (EBUSY);

		if ((flag & FWRITE) == 0)
			return (EBADF);

		if ((error = ccdlock(cs)) != 0)
			return (error);

		if (ccio->ccio_ndisks > CCD_MAXNDISKS)
			return (EINVAL);
 
		/* Fill in some important bits. */
		cs->sc_ileave = ccio->ccio_ileave;
		if (cs->sc_ileave == 0 && (ccio->ccio_flags & CCDF_MIRROR)) {
			printf("ccd%d: disabling mirror, interleave is 0\n",
			    unit);
			ccio->ccio_flags &= ~(CCDF_MIRROR);
		}
		if ((ccio->ccio_flags & CCDF_MIRROR) &&
		    !(ccio->ccio_flags & CCDF_UNIFORM)) {
			printf("ccd%d: mirror/parity forces uniform flag\n",
			       unit);
			ccio->ccio_flags |= CCDF_UNIFORM;
		}
		cs->sc_flags = ccio->ccio_flags & CCDF_USERMASK;

		/*
		 * Allocate space for and copy in the array of
		 * componet pathnames and device numbers.
		 */
		cpp = malloc(ccio->ccio_ndisks * sizeof(char *),
		    M_CCD, 0);
		vpp = malloc(ccio->ccio_ndisks * sizeof(struct vnode *),
		    M_CCD, 0);

		error = copyin((caddr_t)ccio->ccio_disks, (caddr_t)cpp,
		    ccio->ccio_ndisks * sizeof(char **));
		if (error) {
			free(vpp, M_CCD);
			free(cpp, M_CCD);
			ccdunlock(cs);
			return (error);
		}


		for (i = 0; i < ccio->ccio_ndisks; ++i) {
			if ((error = ccdlookup(cpp[i], td, &vpp[i])) != 0) {
				for (j = 0; j < lookedup; ++j)
					(void)vn_close(vpp[j], FREAD|FWRITE,
					    td->td_ucred, td);
				free(vpp, M_CCD);
				free(cpp, M_CCD);
				ccdunlock(cs);
				return (error);
			}
			++lookedup;
		}
		cs->sc_vpp = vpp;
		cs->sc_nccdisks = ccio->ccio_ndisks;

		/*
		 * Initialize the ccd.  Fills in the softc for us.
		 */
		if ((error = ccdinit(cs, cpp, td)) != 0) {
			for (j = 0; j < lookedup; ++j)
				(void)vn_close(vpp[j], FREAD|FWRITE,
				    td->td_ucred, td);
			/*
			 * We can't ccddestroy() cs just yet, because nothing
			 * prevents user-level app to do another ioctl()
			 * without closing the device first, therefore
			 * declare unit null and void and let ccdclose()
			 * destroy it when it is safe to do so.
			 */
			cs->sc_flags &= (CCDF_WANTED | CCDF_LOCKED);
			free(vpp, M_CCD);
			free(cpp, M_CCD);
			ccdunlock(cs);
			return (error);
		}
		free(cpp, M_CCD);

		/*
		 * The ccd has been successfully initialized, so
		 * we can place it into the array and read the disklabel.
		 */
		ccio->ccio_unit = unit;
		ccio->ccio_size = cs->sc_size;
		cs->sc_disk = malloc(sizeof(struct disk), M_CCD, 0);
		cs->sc_dev = disk_create(unit, cs->sc_disk, 0,
		    &ccd_cdevsw, &ccddisk_cdevsw);
		cs->sc_dev->si_drv1 = cs;
		ccg = &cs->sc_geom;
		cs->sc_disk->d_sectorsize = ccg->ccg_secsize;
		cs->sc_disk->d_mediasize =
		    cs->sc_size * (off_t)ccg->ccg_secsize;
		cs->sc_disk->d_fwsectors = ccg->ccg_nsectors;
		cs->sc_disk->d_fwheads = ccg->ccg_ntracks;

		ccdunlock(cs);

		break;

	case CCDIOCCLR:
		if (cs == NULL)
			return (ENXIO);

		if (!IS_INITED(cs))
			return (ENXIO);

		if ((flag & FWRITE) == 0)
			return (EBADF);

		if ((error = ccdlock(cs)) != 0)
			return (error);

		/* Don't unconfigure if any other partitions are open */
		if (cs->sc_openmask) {
			ccdunlock(cs);
			return (EBUSY);
		}

		disk_destroy(cs->sc_dev);
		free(cs->sc_disk, M_CCD);
		cs->sc_disk = NULL;
		/* Declare unit null and void (reset all flags) */
		cs->sc_flags &= (CCDF_WANTED | CCDF_LOCKED);

		/* Close the components and free their pathnames. */
		for (i = 0; i < cs->sc_nccdisks; ++i) {
			/*
			 * XXX: this close could potentially fail and
			 * cause Bad Things.  Maybe we need to force
			 * the close to happen?
			 */
			(void)vn_close(cs->sc_cinfo[i].ci_vp, FREAD|FWRITE,
			    td->td_ucred, td);
			free(cs->sc_cinfo[i].ci_path, M_CCD);
		}

		/* Free interleave index. */
		for (i = 0; cs->sc_itable[i].ii_ndisk; ++i)
			free(cs->sc_itable[i].ii_index, M_CCD);

		/* Free component info and interleave table. */
		free(cs->sc_cinfo, M_CCD);
		free(cs->sc_itable, M_CCD);
		free(cs->sc_vpp, M_CCD);

		/* And remove the devstat entry. */
		devstat_remove_entry(&cs->device_stats);

		/* This must be atomic. */
		ccdunlock(cs);
		ccddestroy(cs);

		break;
	}

	return (0);
}


/*
 * Lookup the provided name in the filesystem.  If the file exists,
 * is a valid block device, and isn't being used by anyone else,
 * set *vpp to the file's vnode.
 */
static int
ccdlookup(char *path, struct thread *td, struct vnode **vpp)
{
	struct nameidata nd;
	struct vnode *vp;
	int error, flags;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, path, td);
	flags = FREAD | FWRITE;
	if ((error = vn_open(&nd, &flags, 0)) != 0) {
		return (error);
	}
	vp = nd.ni_vp;

	if (vrefcnt(vp) > 1) {
		error = EBUSY;
		goto bad;
	}

	if (!vn_isdisk(vp, &error)) 
		goto bad;


	VOP_UNLOCK(vp, 0, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	*vpp = vp;
	return (0);
bad:
	VOP_UNLOCK(vp, 0, td);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	/* vn_close does vrele() for vp */
	(void)vn_close(vp, FREAD|FWRITE, td->td_ucred, td);
	return (error);
}

/*

 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
static int
ccdlock(struct ccd_s *cs)
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
ccdunlock(struct ccd_s *cs)
{

	cs->sc_flags &= ~CCDF_LOCKED;
	if ((cs->sc_flags & CCDF_WANTED) != 0) {
		cs->sc_flags &= ~CCDF_WANTED;
		wakeup(cs);
	}
}
