/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	$Id: wd.c,v 1.94 1995/11/29 10:48:01 julian Exp $
 */

/* TODO:
 *	o Bump error count after timeout.
 *	o Satisfy ATA timing in all cases.
 *	o Finish merging berry/sos timeout code (bump error count...).
 *	o Merge/fix TIH/NetBSD bad144 code.
 *	o Don't use polling except for initialization.  Need to
 *	  reorganize the state machine.  Then "extra" interrupts
 *	  shouldn't happen (except maybe one for initialization).
 *	o Fix disklabel, boot and driver inconsistencies with
 *	  bad144 in standard versions.
 *	o Support extended DOS partitions.
 *	o Support swapping to DOS partitions.
 *	o Handle bad sectors, clustering, disklabelling, DOS
 *	  partitions and swapping driver-independently.  Use
 *	  i386/dkbad.c for bad sectors.  Swapping will need new
 *	  driver entries for polled reinit and polled write).
 */

#include "wd.h"
#ifdef  NWDC
#undef  NWDC
#endif

#include "wdc.h"
#if     NWDC > 0

#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/devconf.h>
#include <machine/bootinfo.h>
#include <machine/clock.h>
#include <machine/cons.h>
#include <machine/md_var.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/wdreg.h>
#include <sys/syslog.h>
#include <sys/dkstat.h>
#include <vm/vm.h>

#ifdef ATAPI
#include <i386/isa/atapi.h>
#endif

extern void wdstart(int ctrlr);

#define TIMEOUT		10000
#define	RETRIES		5	/* number of retries before giving up */
#define RECOVERYTIME	500000	/* usec for controller to recover after err */
#define	MAXTRANSFER	255	/* max size of transfer in sectors */
				/* correct max is 256 but some controllers */
				/* can't handle that in all cases */
#define WDOPT_32BIT	0x8000
#define WDOPT_SLEEPHACK	0x4000
#define WDOPT_MULTIMASK	0x00ff

#ifdef JREMOD
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#define CDEV_MAJOR 3
#define BDEV_MAJOR 0
#endif /*JREMOD */

static int wd_goaway(struct kern_devconf *, int);
static int wdc_goaway(struct kern_devconf *, int);
static int wd_externalize(struct kern_devconf *, struct sysctl_req *);

/*
 * Templates for the kern_devconf structures used when we attach.
 */
static struct kern_devconf kdc_wd[NWD] = { {
	0, 0, 0,		/* filled in by kern_devconf.c */
	"wd", 0, { MDDT_DISK, 0 },
	wd_externalize, 0, wd_goaway, DISK_EXTERNALLEN,
	0,			/* parent */
	0,			/* parentdata */
	DC_UNKNOWN,		/* state */
	"ST506/ESDI/IDE disk",	/* description */
	DC_CLS_DISK		/* class */
} };

static struct kern_devconf kdc_wdc[NWDC] = { {
	0, 0, 0,		/* filled in by kern_devconf.c */
	"wdc", 0, { MDDT_ISA, 0 },
	isa_generic_externalize, 0, wdc_goaway, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* state */
	"ST506/ESDI/IDE disk controller",
	DC_CLS_MISC		/* just an ordinary device */
} };

static inline void
wd_registerdev(int ctlr, int unit)
{
	if(unit != 0) {
		kdc_wd[unit] = kdc_wd[0];
		kdc_wd[unit].kdc_state = DC_IDLE;
	}

	kdc_wd[unit].kdc_unit = unit;
	kdc_wd[unit].kdc_parent = &kdc_wdc[ctlr];
	kdc_wdc[ctlr].kdc_state = DC_BUSY;
	dev_attach(&kdc_wd[unit]);
}

static inline void
wdc_registerdev(struct isa_device *dvp)
{
	int unit = dvp->id_unit;

	if(unit != 0) {
		kdc_wdc[unit] = kdc_wdc[0];
		kdc_wdc[unit].kdc_state = DC_IDLE;
	}

	kdc_wdc[unit].kdc_unit = unit;
	kdc_wdc[unit].kdc_parentdata = dvp;
	dev_attach(&kdc_wdc[unit]);
}

static int
wdc_goaway(struct kern_devconf *kdc, int force)
{
	if(force) {
		dev_detach(kdc);
		return 0;
	} else {
		return EBUSY;	/* XXX fix */
	}
}

static int
wd_goaway(struct kern_devconf *kdc, int force)
{
	dev_detach(kdc);
	return 0;
}

/*
 * This biotab field doubles as a field for the physical unit number on
 * the controller.
 */
#define	id_physid id_scsiid

/*
 * Drive states.  Used to initialize drive.
 */

#define	CLOSED		0	/* disk is closed. */
#define	WANTOPEN	1	/* open requested, not started */
#define	RECAL		2	/* doing restore */
#define	OPEN		3	/* done with open */

/*
 * Disk geometry.  A small part of struct disklabel.
 * XXX disklabel.5 contains an old clone of disklabel.h.
 */
struct diskgeom {
	u_long	d_secsize;		/* # of bytes per sector */
	u_long	d_nsectors;		/* # of data sectors per track */
	u_long	d_ntracks;		/* # of tracks per cylinder */
	u_long	d_ncylinders;		/* # of data cylinders per unit */
	u_long	d_secpercyl;		/* # of data sectors per cylinder */
	u_long	d_secperunit;		/* # of data sectors per unit */
	u_long	d_precompcyl;		/* XXX always 0 */
};

/*
 * The structure of a disk drive.
 */
struct disk {
	long	dk_bc;		/* byte count left */
	short	dk_skip;	/* blocks already transferred */
	char	dk_ctrlr;	/* physical controller number */
	char	dk_unit;	/* physical unit number */
	char	dk_lunit;	/* logical unit number */
	char	dk_state;	/* control state */
	u_char	dk_status;	/* copy of status reg. */
	u_char	dk_error;	/* copy of error reg. */
	u_char	dk_timeout;	/* countdown to next timeout */
	short	dk_port;	/* i/o port base */

	u_long	cfg_flags;	/* configured characteristics */
	short	dk_flags;	/* drive characteristics found */
#define	DKFL_SINGLE	0x00004	/* sector at a time mode */
#define	DKFL_ERROR	0x00008	/* processing a disk error */
#define	DKFL_LABELLING	0x00080	/* readdisklabel() in progress */
#define	DKFL_32BIT	0x00100	/* use 32-bit i/o mode */
#define	DKFL_MULTI	0x00200	/* use multi-i/o mode */
#define	DKFL_BADSCAN	0x00400	/* report all errors */
	struct wdparams dk_params; /* ESDI/IDE drive/controller parameters */
	int	dk_dkunit;	/* disk stats unit number */
	int	dk_multi;	/* multi transfers */
	int	dk_currentiosize;	/* current io size */
	struct diskgeom dk_dd;	/* device configuration data */
	struct diskslices *dk_slices;	/* virtual drives */
};

#define WD_COUNT_RETRIES
static int wdtest = 0;

static struct disk *wddrives[NWD];	/* table of units */
static struct buf_queue_head drive_queue[NWD];	/* head of queue per drive */
static struct {
	int	b_errcnt;
	int	b_active;
} wdutab[NWD];
/*
static struct buf wdtab[NWDC];
*/
static struct {
	struct	buf_queue_head controller_queue;
	int	b_errcnt;
	int	b_active;
} wdtab[NWDC];

#ifdef notyet
static struct buf rwdbuf[NWD];	/* buffers for raw IO */
#endif

static int wdprobe(struct isa_device *dvp);
static int wdattach(struct isa_device *dvp);
static void wdustart(struct disk *du);
static int wdcontrol(struct buf *bp);
static int wdcommand(struct disk *du, u_int cylinder, u_int head,
		     u_int sector, u_int count, u_int command);
static int wdsetctlr(struct disk *du);
static int wdwsetctlr(struct disk *du);
static int wdgetctlr(struct disk *du);
static void wderror(struct buf *bp, struct disk *du, char *mesg);
static void wdflushirq(struct disk *du, int old_ipl);
static int wdreset(struct disk *du);
static void wdsleep(int ctrlr, char *wmesg);
static void wdstrategy1(struct buf *bp);
static timeout_t wdtimeout;
static int wdunwedge(struct disk *du);
static int wdwait(struct disk *du, u_char bits_wanted, int timeout);

/*
 * Provide hw.devconf information.
 */
static int
wd_externalize(struct kern_devconf *kdc, struct sysctl_req *req)
{
	return disk_externalize(wddrives[kdc->kdc_unit]->dk_unit, req);
}

struct isa_driver wdcdriver = {
	wdprobe, wdattach, "wdc",
};

/*
 * Probe for controller.
 */
static int
wdprobe(struct isa_device *dvp)
{
	int	unit = dvp->id_unit;
	struct disk *du;

	if (unit >= NWDC)
		return (0);

	du = malloc(sizeof *du, M_TEMP, M_NOWAIT);
	if (du == NULL)
		return (0);
	bzero(du, sizeof *du);
	du->dk_ctrlr = dvp->id_unit;
	du->dk_port = dvp->id_iobase;

	wdc_registerdev(dvp);

	/* check if we have registers that work */
	outb(du->dk_port + wd_sdh, WDSD_IBM);   /* set unit 0 */
	outb(du->dk_port + wd_cyl_lo, 0xa5);	/* wd_cyl_lo is read/write */
	if (inb(du->dk_port + wd_cyl_lo) == 0xff) {     /* XXX too weak */
#ifdef ATAPI
		/* There is no master, try the ATAPI slave. */
		outb(du->dk_port + wd_sdh, WDSD_IBM | 0x10);
		outb(du->dk_port + wd_cyl_lo, 0xa5);
		if (inb(du->dk_port + wd_cyl_lo) == 0xff)
#endif
			goto nodevice;
	}

	if (wdreset(du) == 0)
		goto reset_ok;
#ifdef ATAPI
	/* test for ATAPI signature */
	outb(du->dk_port + wd_sdh, WDSD_IBM);           /* master */
	if (inb(du->dk_port + wd_cyl_lo) == 0x14 &&
	    inb(du->dk_port + wd_cyl_hi) == 0xeb)
		goto reset_ok;
	du->dk_unit = 1;
	outb(du->dk_port + wd_sdh, WDSD_IBM | 0x10); /* slave */
	if (inb(du->dk_port + wd_cyl_lo) == 0x14 &&
	    inb(du->dk_port + wd_cyl_hi) == 0xeb)
		goto reset_ok;
#endif
	DELAY(RECOVERYTIME);
	if (wdreset(du) != 0)
		goto nodevice;
reset_ok:

	/* execute a controller only command */
	if (wdcommand(du, 0, 0, 0, 0, WDCC_DIAGNOSE) != 0
	    || wdwait(du, 0, TIMEOUT) < 0)
		goto nodevice;

	/*
	 * drive(s) did not time out during diagnostic :
	 * Get error status and check that both drives are OK.
	 * Table 9-2 of ATA specs suggests that we must check for
	 * a value of 0x01
	 *
	 * Strangely, some controllers will return a status of
	 * 0x81 (drive 0 OK, drive 1 failure), and then when
	 * the DRV bit is set, return status of 0x01 (OK) for
	 * drive 2.  (This seems to contradict the ATA spec.)
	 */
	du->dk_error = inb(du->dk_port + wd_error);
	/* printf("Error : %x\n", du->dk_error); */
	if(du->dk_error != 0x01) {
		if(du->dk_error & 0x80) { /* drive 1 failure */

			/* first set the DRV bit */
			u_int sdh;
			sdh = inb(du->dk_port+ wd_sdh);
			sdh = sdh | 0x10;
			outb(du->dk_port+ wd_sdh, sdh);

			/* Wait, to make sure drv 1 has completed diags */
			if ( wdwait(du, 0, TIMEOUT) < 0)
				goto nodevice;

			/* Get status for drive 1 */
			du->dk_error = inb(du->dk_port + wd_error);
			/* printf("Error (drv 1) : %x\n", du->dk_error); */

			if(du->dk_error != 0x01)
				goto nodevice;
		} else	/* drive 0 fail */
			goto nodevice;
	}


	free(du, M_TEMP);
	return (IO_WDCSIZE);

nodevice:
	free(du, M_TEMP);
	return (0);
}

/*
 * Attach each drive if possible.
 */
static int
wdattach(struct isa_device *dvp)
{
	int	unit, lunit;
	struct isa_device *wdup;
	struct disk *du;

	if (dvp->id_unit >= NWDC)
		return (0);

	kdc_wdc[dvp->id_unit].kdc_state = DC_UNKNOWN; /* XXX */
	TAILQ_INIT( &wdtab[dvp->id_unit].controller_queue);

	for (wdup = isa_biotab_wdc; wdup->id_driver != 0; wdup++) {
		if (wdup->id_iobase != dvp->id_iobase)
			continue;
		lunit = wdup->id_unit;
		if (lunit >= NWD)
			continue;

		unit = wdup->id_physid;

		du = malloc(sizeof *du, M_TEMP, M_NOWAIT);
		if (du == NULL)
			continue;
		if (wddrives[lunit] != NULL)
			panic("drive attached twice");
		wddrives[lunit] = du;
		TAILQ_INIT( &drive_queue[lunit]);
		bzero(du, sizeof *du);
		du->dk_ctrlr = dvp->id_unit;
		du->dk_unit = unit;
		du->dk_lunit = lunit;
		du->dk_port = dvp->id_iobase;

		/*
		 * Use the individual device flags or the controller
		 * flags.
		 */
		du->cfg_flags = wdup->id_flags |
			((dvp->id_flags) >> (16 * unit));

		if (wdgetctlr(du) == 0) {
			char buf[sizeof du->dk_params.wdp_model + 1];

			/*
			 * Print out description of drive.
			 * wdp_model may not be null terminated, and printf
			 * doesn't support "%.*s" :-(, so copy wdp_model
			 * and add a null before printing.
			 */
			bcopy(du->dk_params.wdp_model, buf, sizeof buf - 1);
			buf[sizeof buf - 1] = '\0';
			printf("wdc%d: unit %d (wd%d): <%s>",
			       dvp->id_unit, unit, lunit, buf);
			if (du->dk_flags & DKFL_32BIT)
				printf(", 32-bit");
			if (du->dk_multi > 1)
				printf(", multi-block-%d", du->dk_multi);
			if (du->cfg_flags & WDOPT_SLEEPHACK)
				printf(", sleep-hack");
			printf("\n");
			if (du->dk_params.wdp_heads == 0)
				printf("wd%d: size unknown, using %s values\n",
				       lunit, du->dk_dd.d_secperunit > 17
					      ? "BIOS" : "fake");
			printf(
"wd%d: %luMB (%lu sectors), %lu cyls, %lu heads, %lu S/T, %lu B/S\n",
			       lunit,
			       du->dk_dd.d_secperunit
			       * du->dk_dd.d_secsize / (1024 * 1024),
			       du->dk_dd.d_secperunit,
			       du->dk_dd.d_ncylinders,
			       du->dk_dd.d_ntracks,
			       du->dk_dd.d_nsectors,
			       du->dk_dd.d_secsize);

			/*
			 * Start timeout routine for this drive.
			 * XXX timeout should be per controller.
			 */
			wdtimeout(du);

			wd_registerdev(dvp->id_unit, lunit);
			if (dk_ndrive < DK_NDRIVE) {
				sprintf(dk_names[dk_ndrive], "wd%d", lunit);
				/*
				 * XXX we don't know the transfer rate of the
				 * drive.  Guess the maximum ISA rate of
				 * 4MB/sec.  `wpms' is words per _second_
				 * according to iostat.
				 */
				dk_wpms[dk_ndrive] = 4 * 1024 * 1024 / 2;
				du->dk_dkunit = dk_ndrive++;
			} else {
				du->dk_dkunit = -1;
			}
		} else {
			free(du, M_TEMP);
			wddrives[lunit] = NULL;
		}
	}
#ifdef ATAPI
	/*
	 * Probe all free IDE units, searching for ATAPI drives.
	 */
	for (unit=0; unit<2; ++unit) {
		for (lunit=0; lunit<NWD && wddrives[lunit]; ++lunit)
			if (wddrives[lunit]->dk_ctrlr == dvp->id_unit &&
			    wddrives[lunit]->dk_unit == unit)
				goto next;
		atapi_attach (dvp->id_unit, unit, dvp->id_iobase,
			&kdc_wdc[dvp->id_unit]);
next:   }
#endif
	/*
	 * Discard any interrupts generated by wdgetctlr().  wdflushirq()
	 * doesn't work now because the ambient ipl is too high.
	 */
	wdtab[dvp->id_unit].b_active = 2;

	return (1);
}

/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
void
wdstrategy(register struct buf *bp)
{
	register struct buf *dp;
	struct disk *du;
	int	lunit = dkunit(bp->b_dev);
	int	s;

	/* valid unit, controller, and request?  */
	if (lunit >= NWD || bp->b_blkno < 0 || (du = wddrives[lunit]) == NULL
	    || bp->b_bcount % DEV_BSIZE != 0) {

		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/*
	 * Do bounds checking, adjust transfer, set b_cylin and b_pbklno.
	 */
	if (dscheck(bp, du->dk_slices) <= 0)
		goto done;

	/*
	 * Check for *any* block on this transfer being on the bad block list
	 * if it is, then flag the block as a transfer that requires
	 * bad block handling.  Also, used as a hint for low level disksort
	 * clustering code to keep from coalescing a bad transfer into
	 * a normal transfer.  Single block transfers for a large number of
	 * blocks associated with a cluster I/O are undesirable.
	 *
	 * XXX the old disksort() doesn't look at B_BAD.  Coalescing _is_
	 * desirable.  We should split the results at bad blocks just
	 * like we should split them at MAXTRANSFER boundaries.
	 */
	if (dsgetbad(bp->b_dev, du->dk_slices) != NULL) {
		long *badsect = dsgetbad(bp->b_dev, du->dk_slices)->bi_bad;
		int i;
		int nsecs = howmany(bp->b_bcount, DEV_BSIZE);
		/* XXX pblkno is too physical. */
		daddr_t nspblkno = bp->b_pblkno
		    - du->dk_slices->dss_slices[dkslice(bp->b_dev)].ds_offset;
		int blkend = nspblkno + nsecs;

		for (i = 0; badsect[i] != -1 && badsect[i] < blkend; i++) {
			if (badsect[i] >= nspblkno) {
				bp->b_flags |= B_BAD;
				break;
			}
		}
	}

	/* queue transfer on drive, activate drive and controller if idle */
	s = splbio();

	tqdisksort(&drive_queue[lunit], bp);

	if (wdutab[lunit].b_active == 0)
		wdustart(du);	/* start drive */

	/* Pick up changes made by readdisklabel(). */
	if (du->dk_flags & DKFL_LABELLING && du->dk_state > RECAL) {
		wdsleep(du->dk_ctrlr, "wdlab");
		du->dk_state = WANTOPEN;
	}

	if (wdtab[du->dk_ctrlr].b_active == 0)
		wdstart(du->dk_ctrlr);	/* start controller */

	if (du->dk_dkunit >= 0) {
		/*
		 * XXX perhaps we should only count successful transfers.
		 */
		dk_xfer[du->dk_dkunit]++;
		/*
		 * XXX we can't count seeks correctly but we can do better
		 * than this.  E.g., assume that the geometry is correct
		 * and count 1 seek if the starting cylinder of this i/o
		 * differs from the starting cylinder of the previous i/o,
		 * or count 1 seek if the starting bn of this i/o doesn't
		 * immediately follow the ending bn of the previos i/o.
		 */
		dk_seek[du->dk_dkunit]++;
	}

	splx(s);
	return;

done:
	s = splbio();
	/* toss transfer, we're done early */
	biodone(bp);
	splx(s);
}

static void
wdstrategy1(struct buf *bp)
{
	/*
	 * XXX - do something to make wdstrategy() but not this block while
	 * we're doing dsinit() and dsioctl().
	 */
	wdstrategy(bp);
}

/*
 * Routine to queue a command to the controller.  The unit's
 * request is linked into the active list for the controller.
 * If the controller is idle, the transfer is started.
 */
static void
wdustart(register struct disk *du)
{
	register struct buf *bp;
	int	ctrlr = du->dk_ctrlr;

	/* unit already active? */
	if (wdutab[du->dk_lunit].b_active)
		return;


	bp = drive_queue[du->dk_lunit].tqh_first;
	if (bp == NULL) {	/* yes, an assign */
		return;
	}
	TAILQ_REMOVE( &drive_queue[du->dk_lunit], bp, b_act);

	/* link onto controller queue */
	TAILQ_INSERT_TAIL( &wdtab[ctrlr].controller_queue, bp, b_act);

	/* mark the drive unit as busy */
	wdutab[du->dk_lunit].b_active = 1;
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1. The transfer length must be an exact multiple of the sector size.
 */

void
wdstart(int ctrlr)
{
	register struct disk *du;
	register struct buf *bp;
	struct diskgeom *lp;	/* XXX sic */
	struct buf *dp;
	long	blknum;
	long	secpertrk, secpercyl;
	int	lunit;
	int	count;

#ifdef ATAPI
	if (wdtab[ctrlr].b_active == 2)
		wdtab[ctrlr].b_active = 0;
	if (wdtab[ctrlr].b_active)
		return;
#endif
loop:
	/* is there a drive for the controller to do a transfer with? */
	bp = wdtab[ctrlr].controller_queue.tqh_first;
	if (bp == NULL) {
#ifdef ATAPI
		if (atapi_start && atapi_start (ctrlr))
			/* mark controller active in ATAPI mode */
			wdtab[ctrlr].b_active = 3;
#endif
		return;
	}

	/* obtain controller and drive information */
	lunit = dkunit(bp->b_dev);
	du = wddrives[lunit];

	/* if not really a transfer, do control operations specially */
	if (du->dk_state < OPEN) {
		if (du->dk_state != WANTOPEN)
			printf("wd%d: wdstart: weird dk_state %d\n",
			       du->dk_lunit, du->dk_state);
		if (wdcontrol(bp) != 0)
			printf("wd%d: wdstart: wdcontrol returned nonzero, state = %d\n",
			       du->dk_lunit, du->dk_state);
		return;
	}

	/* calculate transfer details */
	blknum = bp->b_pblkno + du->dk_skip;
#ifdef WDDEBUG
	if (du->dk_skip == 0)
		printf("wd%d: wdstart: %s %d@%d; map ", lunit,
		       (bp->b_flags & B_READ) ? "read" : "write",
		       bp->b_bcount, blknum);
	else
		printf(" %d)%x", du->dk_skip, inb(du->dk_port + wd_altsts));
#endif

	lp = &du->dk_dd;
	secpertrk = lp->d_nsectors;
	secpercyl = lp->d_secpercyl;

	if (du->dk_skip == 0) {
		du->dk_bc = bp->b_bcount;

		if (bp->b_flags & B_BAD
		    /*
		     * XXX handle large transfers inefficiently instead
		     * of crashing on them.
		     */
		    || howmany(du->dk_bc, DEV_BSIZE) > MAXTRANSFER)
			du->dk_flags |= DKFL_SINGLE;
	}

	if (du->dk_flags & DKFL_SINGLE
	    && dsgetbad(bp->b_dev, du->dk_slices) != NULL) {
		/* XXX */
		u_long ds_offset =
		    du->dk_slices->dss_slices[dkslice(bp->b_dev)].ds_offset;

		blknum = transbad144(dsgetbad(bp->b_dev, du->dk_slices),
				     blknum - ds_offset) + ds_offset;
	}

	wdtab[ctrlr].b_active = 1;	/* mark controller active */

	/* if starting a multisector transfer, or doing single transfers */
	if (du->dk_skip == 0 || (du->dk_flags & DKFL_SINGLE)) {
		u_int	command;
		u_int	count;
		long	cylin, head, sector;

		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;

		if (wdtab[ctrlr].b_errcnt && (bp->b_flags & B_READ) == 0)
			du->dk_bc += DEV_BSIZE;
		count = howmany( du->dk_bc, DEV_BSIZE);

		du->dk_flags &= ~DKFL_MULTI;

#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			command = WDCC_FORMAT;
			count = lp->d_nsectors;
			sector = lp->d_gap3 - 1;	/* + 1 later */
		} else
#endif

		{
			if (du->dk_flags & DKFL_SINGLE) {
				command = (bp->b_flags & B_READ)
					  ? WDCC_READ : WDCC_WRITE;
				count = 1;
				du->dk_currentiosize = 1;
			} else {
				if( (count > 1) && (du->dk_multi > 1)) {
					du->dk_flags |= DKFL_MULTI;
					if( bp->b_flags & B_READ) {
						command = WDCC_READ_MULTI;
					} else {
						command = WDCC_WRITE_MULTI;
					}
					du->dk_currentiosize = du->dk_multi;
					if( du->dk_currentiosize > count)
						du->dk_currentiosize = count;
				} else {
					if( bp->b_flags & B_READ) {
						command = WDCC_READ;
					} else {
						command = WDCC_WRITE;
					}
					du->dk_currentiosize = 1;
				}
			}
		}

		/*
		 * XXX this loop may never terminate.  The code to handle
		 * counting down of retries and eventually failing the i/o
		 * is in wdintr() and we can't get there from here.
		 */
		if (wdtest != 0) {
			if (--wdtest == 0) {
				wdtest = 100;
				printf("dummy wdunwedge\n");
				wdunwedge(du);
			}
		}
		if(du->dk_dkunit >= 0) {
			dk_busy |= 1 << du->dk_dkunit;
		}
		while (wdcommand(du, cylin, head, sector, count, command)
		       != 0) {
			wderror(bp, du,
				"wdstart: timeout waiting to give command");
			wdunwedge(du);
		}
#ifdef WDDEBUG
		printf("cylin %ld head %ld sector %ld addr %x sts %x\n",
		       cylin, head, sector,
		       (int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE,
		       inb(du->dk_port + wd_altsts));
#endif
	}

	/*
	 * Schedule wdtimeout() to wake up after a few seconds.  Retrying
	 * unmarked bad blocks can take 3 seconds!  Then it is not good that
	 * we retry 5 times.
	 *
	 * XXX wdtimeout() doesn't increment the error count so we may loop
	 * forever.  More seriously, the loop isn't forever but causes a
	 * crash.
	 *
	 * TODO fix b_resid bug elsewhere (fd.c....).  Fix short but positive
	 * counts being discarded after there is an error (in physio I
	 * think).  Discarding them would be OK if the (special) file offset
	 * was not advanced.
	 */
	du->dk_timeout = 1 + 3;

	/* If this is a read operation, just go away until it's done. */
	if (bp->b_flags & B_READ)
		return;

	/* Ready to send data? */
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT) < 0) {
		wderror(bp, du, "wdstart: timeout waiting for DRQ");
		/*
		 * XXX what do we do now?  If we've just issued the command,
		 * then we can treat this failure the same as a command
		 * failure.  But if we are continuing a multi-sector write,
		 * the command was issued ages ago, so we can't simply
		 * restart it.
		 *
		 * XXX we waste a lot of time unnecessarily translating block
		 * numbers to cylin/head/sector for continued i/o's.
		 */
	}

	count = 1;
	if( du->dk_flags & DKFL_MULTI) {
		count = howmany(du->dk_bc, DEV_BSIZE);
		if( count > du->dk_multi)
			count = du->dk_multi;
		if( du->dk_currentiosize > count)
			du->dk_currentiosize = count;
	}

	if (du->dk_flags & DKFL_32BIT)
		outsl(du->dk_port + wd_data,
		      (void *)((int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE),
		      (count * DEV_BSIZE) / sizeof(long));
	else
		outsw(du->dk_port + wd_data,
		      (void *)((int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE),
		      (count * DEV_BSIZE) / sizeof(short));
	du->dk_bc -= DEV_BSIZE * count;
	if (du->dk_dkunit >= 0) {
		/*
		 * `wd's are blocks of 32 16-bit `word's according to
		 * iostat.  dk_wds[] is the one disk i/o statistic that
		 * we can record correctly.
		 * XXX perhaps we shouldn't record words for failed
		 * transfers.
		 */
		dk_wds[du->dk_dkunit] += (count * DEV_BSIZE) >> 6;
	}
}

/* Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start
 * the next request.  Also check for a partially done transfer, and
 * continue with the next chunk if so.
 */
void
wdintr(int unit)
{
	register struct	disk *du;
	register struct buf *bp, *dp;

	if (wdtab[unit].b_active == 2)
		return;		/* intr in wdflushirq() */
	if (!wdtab[unit].b_active) {
#ifdef WDDEBUG
		/*
		 * These happen mostly because the power-mgt part of the
		 * bios shuts us down, and we just manage to see the
		 * interrupt from the "SLEEP" command.
 		 */
		printf("wdc%d: extra interrupt\n", unit);
#endif
		return;
	}
#ifdef ATAPI
	if (wdtab[unit].b_active == 3) {
		/* process an ATAPI interrupt */
		if (atapi_intr && atapi_intr (unit))
			/* ATAPI op continues */
			return;
		/* controller is free, start new op */
		wdtab[unit].b_active = 0;
		wdstart (unit);
		return;
	}
#endif
	bp = wdtab[unit].controller_queue.tqh_first;
	du = wddrives[dkunit(bp->b_dev)];
	du->dk_timeout = 0;

	if (wdwait(du, 0, TIMEOUT) < 0) {
		wderror(bp, du, "wdintr: timeout waiting for status");
		du->dk_status |= WDCS_ERR;	/* XXX */
	}

	/* is it not a transfer, but a control operation? */
	if (du->dk_state < OPEN) {
		wdtab[unit].b_active = 0;
		switch (wdcontrol(bp)) {
		case 0:
			return;
		case 1:
			wdstart(unit);
			return;
		case 2:
			goto done;
		}
	}

	/* have we an error? */
	if (du->dk_status & (WDCS_ERR | WDCS_ECCCOR)) {
oops:
		/*
		 * XXX bogus inb() here, register 0 is assumed and intr status
		 * is reset.
		 */
		if( (du->dk_status & DKFL_MULTI) && (inb(du->dk_port) & WDERR_ABORT)) {
			wderror(bp, du, "reverting to non-multi sector mode");
			du->dk_multi = 1;
		}
#ifdef WDDEBUG
		wderror(bp, du, "wdintr");
#endif
		if ((du->dk_flags & DKFL_SINGLE) == 0) {
			du->dk_flags |= DKFL_ERROR;
			goto outt;
		}
#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			goto done;
		}
#endif

		if (du->dk_flags & DKFL_BADSCAN) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
		} else if (du->dk_status & WDCS_ERR) {
			if (++wdtab[unit].b_errcnt < RETRIES) {
				wdtab[unit].b_active = 0;
			} else {
				wderror(bp, du, "hard error");
				bp->b_error = EIO;
				bp->b_flags |= B_ERROR;	/* flag the error */
			}
		} else
			wderror(bp, du, "soft ecc");
	}

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ)
	    && wdtab[unit].b_active) {
		int	chk, dummy, multisize;
		multisize = chk = du->dk_currentiosize * DEV_BSIZE;
		if( du->dk_bc < chk) {
			chk = du->dk_bc;
			if( ((chk + DEV_BSIZE - 1) / DEV_BSIZE) < du->dk_currentiosize) {
				du->dk_currentiosize = (chk + DEV_BSIZE - 1) / DEV_BSIZE;
				multisize = du->dk_currentiosize * DEV_BSIZE;
			}
		}

		/* ready to receive data? */
		if ((du->dk_status & (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
		    != (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
			wderror(bp, du, "wdintr: read intr arrived early");
		if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT) != 0) {
			wderror(bp, du, "wdintr: read error detected late");
			goto oops;
		}

		/* suck in data */
		if( du->dk_flags & DKFL_32BIT)
			insl(du->dk_port + wd_data,
			     (void *)((int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE),
					chk / sizeof(long));
		else
			insw(du->dk_port + wd_data,
			     (void *)((int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE),
					chk / sizeof(short));
		du->dk_bc -= chk;

		/* XXX for obsolete fractional sector reads. */
		while (chk < multisize) {
			insw(du->dk_port + wd_data, &dummy, 1);
			chk += sizeof(short);
		}

		if (du->dk_dkunit >= 0)
			dk_wds[du->dk_dkunit] += chk >> 6;
	}

outt:
	if (wdtab[unit].b_active) {
		if ((bp->b_flags & B_ERROR) == 0) {
			du->dk_skip += du->dk_currentiosize;/* add to successful sectors */
			if (wdtab[unit].b_errcnt)
				wderror(bp, du, "soft error");
			wdtab[unit].b_errcnt = 0;

			/* see if more to transfer */
			if (du->dk_bc > 0 && (du->dk_flags & DKFL_ERROR) == 0) {
				if( (du->dk_flags & DKFL_SINGLE) ||
					((bp->b_flags & B_READ) == 0)) {
					wdtab[unit].b_active = 0;
					wdstart(unit);
				} else {
					du->dk_timeout = 1 + 3;
				}
				return;	/* next chunk is started */
			} else if ((du->dk_flags & (DKFL_SINGLE | DKFL_ERROR))
				   == DKFL_ERROR) {
				du->dk_skip = 0;
				du->dk_flags &= ~DKFL_ERROR;
				du->dk_flags |= DKFL_SINGLE;
				wdtab[unit].b_active = 0;
				wdstart(unit);
				return;	/* redo xfer sector by sector */
			}
		}

done: ;
		/* done with this transfer, with or without error */
		du->dk_flags &= ~DKFL_SINGLE;
		TAILQ_REMOVE(&wdtab[unit].controller_queue, bp, b_act);
		wdtab[unit].b_errcnt = 0;
		bp->b_resid = bp->b_bcount - du->dk_skip * DEV_BSIZE;
		wdutab[du->dk_lunit].b_active = 0;
		wdutab[du->dk_lunit].b_errcnt = 0;
		du->dk_skip = 0;
		biodone(bp);
	}

	if(du->dk_dkunit >= 0) {
		dk_busy &= ~(1 << du->dk_dkunit);
	}

	/* controller idle */
	wdtab[unit].b_active = 0;

	/* anything more on drive queue? */
	wdustart(du);
	/* anything more for controller to do? */
#ifndef ATAPI
	/* This is not valid in ATAPI mode. */
	if (wdtab[unit].controller_queue.tqh_first)
#endif
		wdstart(unit);
}

/*
 * Initialize a drive.
 */
int
wdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	register unsigned int lunit;
	register struct disk *du;
	int	error;
	int	part = dkpart(dev), mask = 1 << part;
	struct partition *pp;
	char	*msg;

	lunit = dkunit(dev);
	if (lunit >= NWD || dktype(dev) != 0)
		return (ENXIO);
	du = wddrives[lunit];
	if (du == NULL)
		return (ENXIO);

	/* Finish flushing IRQs left over from wdattach(). */
	if (wdtab[du->dk_ctrlr].b_active == 2)
		wdtab[du->dk_ctrlr].b_active = 0;

	du->dk_flags &= ~DKFL_BADSCAN;

	while (du->dk_flags & DKFL_LABELLING)
		tsleep((caddr_t)&du->dk_flags, PZERO - 1, "wdopen", 1);
#if 1
	kdc_wd[lunit].kdc_state = DC_BUSY;
	wdsleep(du->dk_ctrlr, "wdopn1");
	du->dk_flags |= DKFL_LABELLING;
	du->dk_state = WANTOPEN;
	/* drive_queue[lunit].b_actf = NULL; */
	{
	struct disklabel label;

	bzero(&label, sizeof label);
	label.d_secsize = du->dk_dd.d_secsize;
	label.d_nsectors = du->dk_dd.d_nsectors;
	label.d_ntracks = du->dk_dd.d_ntracks;
	label.d_ncylinders = du->dk_dd.d_ncylinders;
	label.d_secpercyl = du->dk_dd.d_secpercyl;
	label.d_secperunit = du->dk_dd.d_secperunit;
	error = dsopen("wd", dev, fmt, &du->dk_slices, &label, wdstrategy1,
		       (ds_setgeom_t *)NULL);
	}
	du->dk_flags &= ~DKFL_LABELLING;
	wdsleep(du->dk_ctrlr, "wdopn2");
	return (error);
#else
	if ((du->dk_flags & DKFL_BSDLABEL) == 0) {
		/*
		 * wdtab[ctrlr].b_active != 0 implies
		 * drive_queue[lunit].b_actf == NULL (?)
		 * so the following guards most things (until the next i/o).
		 * It doesn't guard against a new i/o starting and being
		 * affected by the label being changed.  Sigh.
		 */
		wdsleep(du->dk_ctrlr, "wdopn1");

		du->dk_flags |= DKFL_LABELLING;
		du->dk_state = WANTOPEN;
		/* drive_queue[lunit].b_actf = NULL; */

		error = dsinit(dkmodpart(dev, RAW_PART), wdstrategy,
			       &du->dk_dd, &du->dk_slices);
		if (error != 0) {
			du->dk_flags &= ~DKFL_LABELLING;
			return (error);
		}
		/* XXX check value returned by wdwsetctlr(). */
		wdwsetctlr(du);
		if (dkslice(dev) == WHOLE_DISK_SLICE) {
			dsopen(dev, fmt, du->dk_slices);
			return (0);
		}

		/*
		 * Read label using RAW_PART partition.
		 *
		 * If the drive has an MBR, then the current geometry (from
		 * wdgetctlr()) is used to read it; then the BIOS/DOS
		 * geometry is inferred and used to read the label off the
		 * 'c' partition.  Otherwise the label is read using the
		 * current geometry.  The label gives the final geometry.
		 * If bad sector handling is enabled, then this geometry
		 * is used to read the bad sector table.  The geometry
		 * changes occur inside readdisklabel() and are propagated
		 * to the driver by resetting the state machine.
		 *
		 * XXX can now handle changes directly since dsinit() doesn't
		 * do too much.
		 */
		msg = correct_readdisklabel(dkmodpart(dev, RAW_PART), wdstrategy,
					    &du->dk_dd);
		/* XXX check value returned by wdwsetctlr(). */
		wdwsetctlr(du);
		if (msg == NULL && du->dk_dd.d_flags & D_BADSECT)
			msg = readbad144(dkmodpart(dev, RAW_PART), wdstrategy,
					 &du->dk_dd, &du->dk_bad);
		du->dk_flags &= ~DKFL_LABELLING;
		if (msg != NULL) {
			log(LOG_WARNING, "wd%d: cannot find label (%s)\n",
			    lunit, msg);
			if (part != RAW_PART)
				return (EINVAL);  /* XXX needs translation */
			/*
			 * Soon return.  This is how slices without labels
			 * are allowed.  They only work on the raw partition.
			 */
		} else {
			unsigned long newsize, offset, size;
#if 0
			/*
			 * Force RAW_PART partition to be the whole disk.
			 */
			offset = du->dk_dd.d_partitions[RAW_PART].p_offset;
			if (offset != 0) {
				printf(
		"wd%d: changing offset of '%c' partition from %lu to 0\n",
				       du->dk_lunit, 'a' + RAW_PART, offset);
				du->dk_dd.d_partitions[RAW_PART].p_offset = 0;
			}
			size = du->dk_dd.d_partitions[RAW_PART].p_size;
			newsize = du->dk_dd.d_secperunit;	/* XXX */
			if (size != newsize) {
				printf(
		"wd%d: changing size of '%c' partition from %lu to %lu\n",
				       du->dk_lunit, 'a' + RAW_PART, size,
				       newsize);
				du->dk_dd.d_partitions[RAW_PART].p_size
					= newsize;
			}
#endif
		}

		/* Pick up changes made by readdisklabel(). */
		wdsleep(du->dk_ctrlr, "wdopn2");
		du->dk_state = WANTOPEN;
	}

	/*
	 * Warn if a partion is opened that overlaps another partition which
	 * is open unless one is the "raw" partition (whole disk).
	 */
	if ((du->dk_openpart & mask) == 0 && part != RAW_PART) {
		int	start, end;

		pp = &du->dk_dd.d_partitions[part];
		start = pp->p_offset;
		end = pp->p_offset + pp->p_size;
		for (pp = du->dk_dd.d_partitions;
		     pp < &du->dk_dd.d_partitions[du->dk_dd.d_npartitions];
		     pp++) {
			if (pp->p_offset + pp->p_size <= start ||
			    pp->p_offset >= end)
				continue;
			if (pp - du->dk_dd.d_partitions == RAW_PART)
				continue;
			if (du->dk_openpart
			    & (1 << (pp - du->dk_dd.d_partitions)))
				log(LOG_WARNING,
				    "wd%d%c: overlaps open partition (%c)\n",
				    lunit, part + 'a',
				    pp - du->dk_dd.d_partitions + 'a');
		}
	}
	if (part >= du->dk_dd.d_npartitions && part != RAW_PART)
		return (ENXIO);

	dsopen(dev, fmt, du->dk_slices);

	return (0);
#endif
}

/*
 * Implement operations other than read/write.
 * Called from wdstart or wdintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed, 2 if error.
 */
static int
wdcontrol(register struct buf *bp)
{
	register struct disk *du;
	int	ctrlr;

	du = wddrives[dkunit(bp->b_dev)];
	ctrlr = du->dk_ctrlr;

	switch (du->dk_state) {
	case WANTOPEN:
tryagainrecal:
		wdtab[ctrlr].b_active = 1;
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0) {
			wderror(bp, du, "wdcontrol: wdcommand failed");
			goto maybe_retry;
		}
		du->dk_state = RECAL;
		return (0);
	case RECAL:
		if (du->dk_status & WDCS_ERR || wdsetctlr(du) != 0) {
			wderror(bp, du, "wdcontrol: recal failed");
maybe_retry:
			if (du->dk_status & WDCS_ERR)
				wdunwedge(du);
			du->dk_state = WANTOPEN;
			if (++wdtab[ctrlr].b_errcnt < RETRIES)
				goto tryagainrecal;
			bp->b_error = ENXIO;	/* XXX needs translation */
			bp->b_flags |= B_ERROR;
			return (2);
		}
		wdtab[ctrlr].b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done by normal
		 * means.
		 */
		return (1);
	}
	panic("wdcontrol");
	return (2);
}

/*
 * Wait uninterruptibly until controller is not busy, then send it a command.
 * The wait usually terminates immediately because we waited for the previous
 * command to terminate.
 */
static int
wdcommand(struct disk *du, u_int cylinder, u_int head, u_int sector,
	  u_int count, u_int command)
{
	u_int	wdc;

	wdc = du->dk_port;
	if (du->cfg_flags & WDOPT_SLEEPHACK)
		if(inb(wdc + wd_status) == WDCS_BUSY)
			wdunwedge(du);

	if (wdwait(du, 0, TIMEOUT) < 0)
		return (1);
	if( command == WDCC_FEATURES) {
		outb(wdc + wd_features, count);
	} else {
		outb(wdc + wd_precomp, du->dk_dd.d_precompcyl / 4);
		outb(wdc + wd_cyl_lo, cylinder);
		outb(wdc + wd_cyl_hi, cylinder >> 8);
		outb(wdc + wd_sdh, WDSD_IBM | (du->dk_unit << 4) | head);
		outb(wdc + wd_sector, sector + 1);
		outb(wdc + wd_seccnt, count);
	}
	if (wdwait(du, command == WDCC_DIAGNOSE || command == WDCC_IDC
		       ? 0 : WDCS_READY, TIMEOUT) < 0)
		return (1);
	outb(wdc + wd_command, command);
	return (0);
}

/*
 * issue IDC to drive to tell it just what geometry it is to be.
 */
static int
wdsetctlr(struct disk *du)
{
	int error = 0;
#ifdef WDDEBUG
	printf("wd(%d,%d): wdsetctlr: C %lu H %lu S %lu\n",
	       du->dk_ctrlr, du->dk_unit,
	       du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks,
	       du->dk_dd.d_nsectors);
#endif
	if (du->dk_dd.d_ntracks == 0 || du->dk_dd.d_ntracks > 16) {
		struct wdparams *wp;

		printf("wd%d: can't handle %lu heads from partition table ",
		       du->dk_lunit, du->dk_dd.d_ntracks);
		/* obtain parameters */
		wp = &du->dk_params;
		if (wp->wdp_heads > 0 && wp->wdp_heads <= 16) {
			printf("(controller value %u restored)\n",
				wp->wdp_heads);
			du->dk_dd.d_ntracks = wp->wdp_heads;
		}
		else {
			printf("(truncating to 16)\n");
			du->dk_dd.d_ntracks = 16;
		}
	}

	if (du->dk_dd.d_nsectors == 0 || du->dk_dd.d_nsectors > 255) {
		printf("wd%d: cannot handle %lu sectors (max 255)\n",
		       du->dk_lunit, du->dk_dd.d_nsectors);
		error = 1;
	}
	if (error) {
		wdtab[du->dk_ctrlr].b_errcnt += RETRIES;
		return (1);
	}
	if (wdcommand(du, du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks - 1, 0,
		      du->dk_dd.d_nsectors, WDCC_IDC) != 0
	    || wdwait(du, WDCS_READY, TIMEOUT) < 0) {
		wderror((struct buf *)NULL, du, "wdsetctlr failed");
		return (1);
	}
	return (0);
}

/*
 * Wait until driver is inactive, then set up controller.
 */
static int
wdwsetctlr(struct disk *du)
{
	int	stat;
	int	x;

	wdsleep(du->dk_ctrlr, "wdwset");
	x = splbio();
	stat = wdsetctlr(du);
	wdflushirq(du, x);
	splx(x);
	return (stat);
}

/*
 * issue READP to drive to ask it what it is.
 */
static int
wdgetctlr(struct disk *du)
{
	int	i;
	char    tb[DEV_BSIZE], tb2[DEV_BSIZE];
	struct wdparams *wp = NULL;
	u_long flags = du->cfg_flags;
again:
	if (wdcommand(du, 0, 0, 0, 0, WDCC_READP) != 0
	    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT) != 0) {

		/*
		 * if we failed on the second try, assume non-32bit
		 */
		if( du->dk_flags & DKFL_32BIT)
			goto failed;

		/* XXX need to check error status after final transfer. */
		/*
		 * Old drives don't support WDCC_READP.  Try a seek to 0.
		 * Some IDE controllers return trash if there is no drive
		 * attached, so first test that the drive can be selected.
		 * This also avoids long waits for nonexistent drives.
		 */
		if (wdwait(du, 0, TIMEOUT) < 0)
			return (1);
		outb(du->dk_port + wd_sdh, WDSD_IBM | (du->dk_unit << 4));
		DELAY(5000);	/* usually unnecessary; drive select is fast */
		if ((inb(du->dk_port + wd_status) & (WDCS_BUSY | WDCS_READY))
		    != WDCS_READY
		    || wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0
		    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0)
			return (1);

		if (du->dk_unit == bootinfo.bi_n_bios_used) {
			du->dk_dd.d_secsize = DEV_BSIZE;
			du->dk_dd.d_nsectors =
			    bootinfo.bi_bios_geom[du->dk_unit] & 0xff;
			du->dk_dd.d_ntracks =
			    ((bootinfo.bi_bios_geom[du->dk_unit] >> 8) & 0xff)
			    + 1;
			/* XXX Why 2 ? */
			du->dk_dd.d_ncylinders =
			    (bootinfo.bi_bios_geom[du->dk_unit] >> 16) + 2;
			du->dk_dd.d_secpercyl =
			    du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
			du->dk_dd.d_secperunit =
			    du->dk_dd.d_secpercyl * du->dk_dd.d_ncylinders;
#if 0
			du->dk_dd.d_partitions[WDRAW].p_size =
				du->dk_dd.d_secperunit;
			du->dk_dd.d_type = DTYPE_ST506;
			du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;
			strncpy(du->dk_dd.d_typename, "Bios geometry",
				sizeof du->dk_dd.d_typename);
			strncpy(du->dk_params.wdp_model, "ST506",
				sizeof du->dk_params.wdp_model);
#endif
			bootinfo.bi_n_bios_used ++;
			return 0;
		}
		/*
		 * Fake minimal drive geometry for reading the MBR.
		 * readdisklabel() may enlarge it to read the label and the
		 * bad sector table.
		 */
		du->dk_dd.d_secsize = DEV_BSIZE;
		du->dk_dd.d_nsectors = 17;
		du->dk_dd.d_ntracks = 1;
		du->dk_dd.d_ncylinders = 1;
		du->dk_dd.d_secpercyl = 17;
		du->dk_dd.d_secperunit = 17;

#if 0
		/*
		 * Fake maximal drive size for writing the label.
		 */
		du->dk_dd.d_partitions[RAW_PART].p_size = 64 * 16 * 1024;

		/*
		 * Fake some more of the label for printing by disklabel(1)
		 * in case there is no real label.
		 */
		du->dk_dd.d_type = DTYPE_ST506;
		du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;
		strncpy(du->dk_dd.d_typename, "Fake geometry",
			sizeof du->dk_dd.d_typename);
#endif

		/* Fake the model name for printing by wdattach(). */
		strncpy(du->dk_params.wdp_model, "unknown",
			sizeof du->dk_params.wdp_model);

		return (0);
	}

	/* obtain parameters */
	wp = &du->dk_params;
	if (du->dk_flags & DKFL_32BIT)
		insl(du->dk_port + wd_data, tb, sizeof(tb) / sizeof(long));
	else
		insw(du->dk_port + wd_data, tb, sizeof(tb) / sizeof(short));

	/* try 32-bit data path (VLB IDE controller) */
	if (flags & WDOPT_32BIT) {
		if (! (du->dk_flags & DKFL_32BIT)) {
			bcopy(tb, tb2, sizeof(struct wdparams));
			du->dk_flags |= DKFL_32BIT;
			goto again;
		}

		/* check that we really have 32-bit controller */
		if (bcmp (tb, tb2, sizeof(struct wdparams)) != 0) {
failed:
			/* test failed, use 16-bit i/o mode */
			bcopy(tb2, tb, sizeof(struct wdparams));
			du->dk_flags &= ~DKFL_32BIT;
		}
	}

	bcopy(tb, wp, sizeof(struct wdparams));

	/* shuffle string byte order */
	for (i = 0; i < sizeof(wp->wdp_model); i += 2) {
		u_short *p;

		p = (u_short *) (wp->wdp_model + i);
		*p = ntohs(*p);
	}
	/*
	 * Clean up the wdp_model by converting nulls to spaces, and
	 * then removing the trailing spaces.
	 */
	for (i=0; i < sizeof(wp->wdp_model); i++) {
		if (wp->wdp_model[i] == '\0') {
			wp->wdp_model[i] = ' ';
		}
	}
	for (i=sizeof(wp->wdp_model)-1; i>=0 && wp->wdp_model[i]==' '; i--) {
		wp->wdp_model[i] = '\0';
	}

#ifdef WDDEBUG
	printf(
"\nwd(%d,%d): wdgetctlr: gc %x cyl %d trk %d sec %d type %d sz %d model %s\n",
	       du->dk_ctrlr, du->dk_unit, wp->wdp_config,
	       wp->wdp_fixedcyl + wp->wdp_removcyl, wp->wdp_heads,
	       wp->wdp_sectors, wp->wdp_cntype, wp->wdp_cnsbsz,
	       wp->wdp_model);
#endif

	/* update disklabel given drive information */
	du->dk_dd.d_secsize = DEV_BSIZE;
	du->dk_dd.d_ncylinders = wp->wdp_fixedcyl + wp->wdp_removcyl /*+- 1*/ ;
	du->dk_dd.d_ntracks = wp->wdp_heads;
	du->dk_dd.d_nsectors = wp->wdp_sectors;
	du->dk_dd.d_secpercyl = du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
	du->dk_dd.d_secperunit = du->dk_dd.d_secpercyl * du->dk_dd.d_ncylinders;
#if 0
	du->dk_dd.d_partitions[RAW_PART].p_size = du->dk_dd.d_secperunit;
	/* dubious ... */
	bcopy("ESDI/IDE", du->dk_dd.d_typename, 9);
	bcopy(wp->wdp_model + 20, du->dk_dd.d_packname, 14 - 1);
	/* better ... */
	du->dk_dd.d_type = DTYPE_ESDI;
	du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;
#endif

	/*
	 * find out the drives maximum multi-block transfer capability
	 */
	du->dk_multi = wp->wdp_nsecperint & 0xff;

	/*
	 * The config option flags low 8 bits define the maximum multi-block
	 * transfer size.  If the user wants the maximum that the drive
	 * is capable of, just set the low bits of the config option to
	 * 0x00ff.
	 */
	if ((flags & WDOPT_MULTIMASK) != 0 && (du->dk_multi > 1)) {
		if (du->dk_multi > (flags & WDOPT_MULTIMASK))
			du->dk_multi = flags & WDOPT_MULTIMASK;
		if (wdcommand(du, 0, 0, 0, du->dk_multi, WDCC_SET_MULTI)) {
			du->dk_multi = 1;
		}
	} else {
		du->dk_multi = 1;
	}

#ifdef NOTYET
/* set read caching and write caching */
	wdcommand(du, 0, 0, 0, WDFEA_RCACHE, WDCC_FEATURES);
	wdcommand(du, 0, 0, 0, WDFEA_WCACHE, WDCC_FEATURES);
#endif

	return (0);
}

int
wdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	dsclose(dev, fmt, wddrives[dkunit(dev)]->dk_slices);
	kdc_wd[wddrives[dkunit(dev)]->dk_lunit].kdc_state = DC_IDLE;
	return (0);
}

int
wdioctl(dev_t dev, int cmd, caddr_t addr, int flags, struct proc *p)
{
	int	lunit = dkunit(dev);
	register struct disk *du;
	int	error;
#ifdef notyet
	struct uio auio;
	struct iovec aiov;
	struct format_op *fop;
#endif

	du = wddrives[lunit];
	wdsleep(du->dk_ctrlr, "wdioct");
	error = dsioctl("wd", dev, cmd, addr, flags, &du->dk_slices,
			wdstrategy1, (ds_setgeom_t *)NULL);
	if (error != -1)
		return (error);

	switch (cmd) {
	case DIOCSBADSCAN:
		if (*(int *)addr)
			du->dk_flags |= DKFL_BADSCAN;
		else
			du->dk_flags &= ~DKFL_BADSCAN;
		return (0);
#ifdef notyet
	case DIOCWFORMAT:
		if (!(flag & FWRITE))
			return (EBADF);
		fop = (struct format_op *)addr;
		aiov.iov_base = fop->df_buf;
		aiov.iov_len = fop->df_count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = fop->df_count;
		auio.uio_segflg = 0;
		auio.uio_offset = fop->df_startblk * du->dk_dd.d_secsize;
#error /* XXX the 386BSD interface is different */
		error = physio(wdformat, &rwdbuf[lunit], 0, dev, B_WRITE,
			       minphys, &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = du->dk_status;
		fop->df_reg[1] = du->dk_error;
		return (error);
#endif

	default:
		return (ENOTTY);
	}
}

#ifdef B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	wdstrategy(bp);
	/*
	 * phk put this here, better that return(wdstrategy(bp));
	 * XXX
	 */
	return -1;
}
#endif

int
wdsize(dev_t dev)
{
	struct disk *du;
	int	lunit;

	lunit = dkunit(dev);
	if (lunit >= NWD || dktype(dev) != 0)
		return (-1);
	du = wddrives[lunit];
	if (du == NULL)
		return (-1);
	return (dssize(dev, &du->dk_slices, wdopen, wdclose));
}

/*
 * Dump core after a system crash.
 */
int
wddump(dev_t dev)
{
	register struct disk *du;
	struct disklabel *lp;
	long	num;		/* number of sectors to write */
	int	lunit, part;
	long	blkoff, blknum;
	long	blkchk, blkcnt, blknext;
	long	cylin, head, sector;
	long	secpertrk, secpercyl, nblocks;
	u_long	ds_offset;
	char   *addr;
	static int wddoingadump = 0;

	/* Toss any characters present prior to dump. */
	while (cncheckc())
		;

	/* Check for acceptable device. */
	/* XXX should reset to maybe allow du->dk_state < OPEN. */
	lunit = dkunit(dev);	/* eventually support floppies? */
	part = dkpart(dev);
	if (lunit >= NWD || (du = wddrives[lunit]) == NULL
	    || du->dk_state < OPEN
	    || (lp = dsgetlabel(dev, du->dk_slices)) == NULL)
		return (ENXIO);

	/* Size of memory to dump, in disk sectors. */
	num = (u_long)Maxmem * NBPG / du->dk_dd.d_secsize;

	secpertrk = du->dk_dd.d_nsectors;
	secpercyl = du->dk_dd.d_secpercyl;
	nblocks = lp->d_partitions[part].p_size;
	blkoff = lp->d_partitions[part].p_offset;
	/* XXX */
	ds_offset = du->dk_slices->dss_slices[dkslice(dev)].ds_offset;
	blkoff += ds_offset;

#if 0
	pg("part %x, nblocks %d, dumplo %d num %d\n",
	   part, nblocks, dumplo, num);
#endif

	/* Check transfer bounds against partition size. */
	if (dumplo < 0 || dumplo + num > nblocks)
		return (EINVAL);

	/* Check if we are being called recursively. */
	if (wddoingadump)
		return (EFAULT);

#if 0
	/* Mark controller active for if we panic during the dump. */
	wdtab[du->dk_ctrlr].b_active = 1;
#endif
	wddoingadump = 1;

	/* Recalibrate the drive. */
	DELAY(5);		/* ATA spec XXX NOT */
	if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0
	    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0
	    || wdsetctlr(du) != 0) {
		wderror((struct buf *)NULL, du, "wddump: recalibrate failed");
		return (EIO);
	}

	du->dk_flags |= DKFL_SINGLE;
	addr = (char *) 0;
	blknum = dumplo + blkoff;
	while (num > 0) {
		blkcnt = num;
		if (blkcnt > MAXTRANSFER)
			blkcnt = MAXTRANSFER;
		/* Keep transfer within current cylinder. */
		if ((blknum + blkcnt - 1) / secpercyl != blknum / secpercyl)
			blkcnt = secpercyl - (blknum % secpercyl);
		blknext = blknum + blkcnt;

		/*
		 * See if one of the sectors is in the bad sector list
		 * (if we have one).  If the first sector is bad, then
		 * reduce the transfer to this one bad sector; if another
		 * sector is bad, then reduce reduce the transfer to
		 * avoid any bad sectors.
		 */
		if (du->dk_flags & DKFL_SINGLE
		    && dsgetbad(dev, du->dk_slices) != NULL) {
		  for (blkchk = blknum; blkchk < blknum + blkcnt; blkchk++) {
			daddr_t blknew;
			blknew = transbad144(dsgetbad(dev, du->dk_slices),
					     blkchk - ds_offset) + ds_offset;
			if (blknew != blkchk) {
				/* Found bad block. */
				blkcnt = blkchk - blknum;
				if (blkcnt > 0) {
					blknext = blknum + blkcnt;
					goto out;
				}
				blkcnt = 1;
				blknext = blknum + blkcnt;
#if 1 || defined(WDDEBUG)
				printf("bad block %lu -> %lu\n",
				       blknum, blknew);
#endif
				break;
			}
		  }
		}
out:

		/* Compute disk address. */
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;

#if 0
		/* Let's just talk about this first... */
		pg("cylin l%d head %ld sector %ld addr 0x%x count %ld",
		   cylin, head, sector, addr, blkcnt);
#endif

		/* Do the write. */
		if (wdcommand(du, cylin, head, sector, blkcnt, WDCC_WRITE)
		    != 0) {
			wderror((struct buf *)NULL, du,
				"wddump: timeout waiting to to give command");
			return (EIO);
		}
		while (blkcnt != 0) {
			pmap_enter(kernel_pmap, (vm_offset_t)CADDR1, trunc_page(addr),
				   VM_PROT_READ, TRUE);

			/* Ready to send data? */
			DELAY(5);	/* ATA spec */
			if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ, TIMEOUT)
			    < 0) {
				wderror((struct buf *)NULL, du,
					"wddump: timeout waiting for DRQ");
				return (EIO);
			}
			if (du->dk_flags & DKFL_32BIT)
				outsl(du->dk_port + wd_data,
				      CADDR1 + ((int)addr & (NBPG - 1)),
				      DEV_BSIZE / sizeof(long));
			else
				outsw(du->dk_port + wd_data,
				      CADDR1 + ((int)addr & (NBPG - 1)),
				      DEV_BSIZE / sizeof(short));
			addr += DEV_BSIZE;
			if ((unsigned)addr % (1024 * 1024) == 0)
				printf("%ld ", num / (1024 * 1024 / DEV_BSIZE));
			num--;
			blkcnt--;
		}

		/* Wait for completion. */
		DELAY(5);	/* ATA spec XXX NOT */
		if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) < 0) {
			wderror((struct buf *)NULL, du,
				"wddump: timeout waiting for status");
			return (EIO);
		}

		/* Check final status. */
		if (du->dk_status
		    & (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ | WDCS_ERR)
		    != (WDCS_READY | WDCS_SEEKCMPLT)) {
			wderror((struct buf *)NULL, du,
				"wddump: extra DRQ, or error");
			return (EIO);
		}

		/* Update block count. */
		blknum = blknext;

		/* Operator aborting dump? */
		if (cncheckc())
			return (EINTR);
	}
	return (0);
}

static void
wderror(struct buf *bp, struct disk *du, char *mesg)
{
	if (bp == NULL)
		printf("wd%d: %s:\n", du->dk_lunit, mesg);
	else
		diskerr(bp, "wd", mesg, LOG_PRINTF, du->dk_skip,
			dsgetlabel(bp->b_dev, du->dk_slices));
	printf("wd%d: status %b error %b\n", du->dk_lunit,
	       du->dk_status, WDCS_BITS, du->dk_error, WDERR_BITS);
}

/*
 * Discard any interrupts that were latched by the interrupt system while
 * we were doing polled i/o.
 */
static void
wdflushirq(struct disk *du, int old_ipl)
{
	wdtab[du->dk_ctrlr].b_active = 2;
	splx(old_ipl);
	(void)splbio();
	wdtab[du->dk_ctrlr].b_active = 0;
}

/*
 * Reset the controller.
 */
static int
wdreset(struct disk *du)
{
	int     wdc, err = 0;

	wdc = du->dk_port;
	(void)wdwait(du, 0, TIMEOUT);
	outb(wdc + wd_ctlr, WDCTL_IDS | WDCTL_RST);
	DELAY(10 * 1000);
	outb(wdc + wd_ctlr, WDCTL_IDS);
#ifdef ATAPI
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0)
		err = 1;                /* no IDE drive found */
	du->dk_error = inb(wdc + wd_error);
	if (du->dk_error != 0x01)
		err = 1;                /* the drive is incompatible */
#else
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) != 0
	    || (du->dk_error = inb(wdc + wd_error)) != 0x01)
		return (1);
#endif
	outb(wdc + wd_ctlr, WDCTL_4BIT);
	return (err);
}

/*
 * Sleep until driver is inactive.
 * This is used only for avoiding rare race conditions, so it is unimportant
 * that the sleep may be far too short or too long.
 */
static void
wdsleep(int ctrlr, char *wmesg)
{
	int s = splbio();
	while (wdtab[ctrlr].b_active)
		tsleep((caddr_t)&wdtab[ctrlr].b_active, PZERO - 1, wmesg, 1);
	splx(s);
}

static void
wdtimeout(void *cdu)
{
	struct disk *du;
	int	x;
	static	int	timeouts;

	du = (struct disk *)cdu;
	x = splbio();
	if (du->dk_timeout != 0 && --du->dk_timeout == 0) {
		if(timeouts++ == 5)
			wderror((struct buf *)NULL, du,
   "Last time I say: interrupt timeout.  Probably a portable PC.");
		else if(timeouts++ < 5)
			wderror((struct buf *)NULL, du, "interrupt timeout");
		wdunwedge(du);
		wdflushirq(du, x);
		du->dk_skip = 0;
		du->dk_flags |= DKFL_SINGLE;
		wdstart(du->dk_ctrlr);
	}
	timeout(wdtimeout, cdu, hz);
	splx(x);
}

/*
 * Reset the controller after it has become wedged.  This is different from
 * wdreset() so that wdreset() can be used in the probe and so that this
 * can restore the geometry .
 */
static int
wdunwedge(struct disk *du)
{
	struct disk *du1;
	int	lunit;

	/* Schedule other drives for recalibration. */
	for (lunit = 0; lunit < NWD; lunit++)
		if ((du1 = wddrives[lunit]) != NULL && du1 != du
		    && du1->dk_ctrlr == du->dk_ctrlr
		    && du1->dk_state > WANTOPEN)
			du1->dk_state = WANTOPEN;

	DELAY(RECOVERYTIME);
	if (wdreset(du) == 0) {
		/*
		 * XXX - recalibrate current drive now because some callers
		 * aren't prepared to have its state change.
		 */
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) == 0
		    && wdwait(du, WDCS_READY | WDCS_SEEKCMPLT, TIMEOUT) == 0
		    && wdsetctlr(du) == 0)
			return (0);
	}
	wderror((struct buf *)NULL, du, "wdunwedge failed");
	return (1);
}

/*
 * Wait uninterruptibly until controller is not busy and either certain
 * status bits are set or an error has occurred.
 * The wait is usually short unless it is for the controller to process
 * an entire critical command.
 * Return 1 for (possibly stale) controller errors, -1 for timeout errors,
 * or 0 for no errors.
 * Return controller status in du->dk_status and, if there was a controller
 * error, return the error code in du->dk_error.
 */
#ifdef WD_COUNT_RETRIES
static int min_retries[NWDC];
#endif

static int
wdwait(struct disk *du, u_char bits_wanted, int timeout)
{
	int	wdc;
	u_char	status;

#define	POLLING		1000

	wdc = du->dk_port;
	timeout += POLLING;

/*
 * This delay is really too long, but does not impact the performance
 * as much when using the multi-sector option.  Shorter delays have
 * caused I/O errors on some drives and system configs.  This should
 * probably be fixed if we develop a better short term delay mechanism.
 */
	DELAY(1);

	do {
#ifdef WD_COUNT_RETRIES
		if (min_retries[du->dk_ctrlr] > timeout
		    || min_retries[du->dk_ctrlr] == 0)
			min_retries[du->dk_ctrlr] = timeout;
#endif
		du->dk_status = status = inb(wdc + wd_status);
#ifdef ATAPI
		/*
		 * Atapi drives have a very interesting feature, when attached
		 * as a slave on the IDE bus, and there is no master.
		 * They release the bus after getting the command.
		 * We should reselect the drive here to get the status.
		 */
		if (status == 0xff) {
			outb(wdc + wd_sdh, WDSD_IBM | du->dk_unit << 4);
			du->dk_status = status = inb(wdc + wd_status);
		}
#endif
		if (!(status & WDCS_BUSY)) {
			if (status & WDCS_ERR) {
				du->dk_error = inb(wdc + wd_error);
				/*
				 * We once returned here.  This is wrong
				 * because the error bit is apparently only
				 * valid after the controller has interrupted
				 * (e.g., the error bit is stale when we wait
				 * for DRQ for writes).  So we can't depend
				 * on the error bit at all when polling for
				 * command completion.
				 */
			}
			if ((status & bits_wanted) == bits_wanted)
				return (status & WDCS_ERR);
		}
		if (timeout < TIMEOUT)
			/*
			 * Switch to a polling rate of about 1 KHz so that
			 * the timeout is almost machine-independent.  The
			 * controller is taking a long time to respond, so
			 * an extra msec won't matter.
			 */
			DELAY(1000);
		else
			DELAY(1);
	} while (--timeout != 0);
	return (-1);
}

#ifdef JREMOD
struct bdevsw wd_bdevsw = 
	{ wdopen,	wdclose,	wdstrategy,	wdioctl,	/*0*/
	  wddump,	wdsize,		0 };

struct cdevsw wd_cdevsw = 
	{ wdopen,	wdclose,	rawread,	rawwrite,	/*3*/
	  wdioctl,	nostop,		nullreset,	nodevtotty,/* wd */
	  seltrue,	nommap,		wdstrategy };

static wd_devsw_installed = 0;

static void 	wd_drvinit(void *unused)
{
	dev_t dev;
	dev_t dev_chr;

	if( ! wd_devsw_installed ) {
		dev = makedev(CDEV_MAJOR,0);
		cdevsw_add(&dev,&wd_cdevsw,NULL);
		dev_chr = dev;
		dev = makedev(BDEV_MAJOR,0);
		bdevsw_add(&dev,&wd_bdevsw,NULL);
		wd_devsw_installed = 1;
#ifdef DEVFS
		{
			int x;
/* default for a simple device with no probe routine (usually delete this) */
			x=devfs_add_devsw(
/*	path	name	devsw		minor	type   uid gid perm*/
	"/",	"rwd",	major(dev_chr),	0,	DV_CHR,	0,  0, 0600);
			x=devfs_add_devsw(
	"/",	"wd",	major(dev),	0,	DV_BLK,	0,  0, 0600);
		}
#endif
    	}
}

SYSINIT(wddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,wd_drvinit,NULL)

#endif /* JREMOD */

#endif /* NWDC > 0 */
