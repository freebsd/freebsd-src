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
 *	from:@(#)wd.c	7.2 (Berkeley) 5/9/91
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         6       00155
 * --------------------         -----   ----------------------
 *
 * 17 Sep 92	Frank Maclachlan	Fixed I/O error reporting on raw device
 * 31 Jul 92	Christoph Robitschko	Fixed second disk recognition,
 *					bzero of malloced memory for warm
 *					boot problem.
 * 19 Aug 92    Frank Maclachlan	Fixed bug when first sector of a
 *					multisector read is in bad144 table.
 * 17 Jan 93	B. Evans & A.Chernov	Fixed bugs from previous patches,
 *					driver initialization, and cylinder
 *					boundary conditions.
 * 28 Mar 93	Charles Hannum		Add missing splx calls.
 * 20 Apr 93	Terry Lee		Always report disk errors
 * 20 Apr 93	Brett Lymn		Change infinite while loops to
 *					timeouts
 * 17 May 93	Rodney W. Grimes	Fixed all 1000000 to use WDCTIMEOUT,
 *					and increased to 1000000*10 for new
 *					intr-0.1 code.
 */

/* TODO:peel out buffer at low ipl, speed improvement */


#include "wd.h"
#if	NWD > 0

#include "param.h"
#include "dkbad.h"
#include "systm.h"
#include "conf.h"
#include "file.h"
#include "stat.h"
#include "ioctl.h"
#include "disklabel.h"
#include "buf.h"
#include "uio.h"
#include "malloc.h"
#include "machine/cpu.h"
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/icu.h"
#include "i386/isa/wdreg.h"
#include "syslog.h"
#include "vm/vm.h"

#define _NWD  (NWD - 1)       /* One is for the controller XXX 31 Jul 92*/

#ifndef WDCTIMEOUT
#define WDCTIMEOUT	10000000  /* arbitrary timeout for drive ready waits */
#endif

#define	RETRIES		5	/* number of retries before giving up */
#define	MAXTRANSFER	32	/* max size of transfer in page clusters */

#define wdnoreloc(dev)	(minor(dev) & 0x80)	/* ignore partition table */
#define wddospart(dev)	(minor(dev) & 0x40)	/* use dos partitions */
#define wdunit(dev)	((minor(dev) & 0x38) >> 3)
#define wdpart(dev)	(minor(dev) & 0x7)
#define makewddev(maj, unit, part)	(makedev(maj,((unit<<3)+part)))
#define WDRAW	3		/* 'd' partition isn't a partition! */

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

/*
 * Drive states.  Used to initialize drive.
 */

#define	CLOSED		0		/* disk is closed. */
#define	WANTOPEN	1		/* open requested, not started */
#define	RECAL		2		/* doing restore */
#define	OPEN		3		/* done with open */

/*
 * The structure of a disk drive.
 */
struct	disk {
	long	dk_bc;		/* byte count left */
	short	dk_skip;	/* blocks already transferred */
	char	dk_unit;	/* physical unit number */
	char	dk_state;	/* control state */
	u_char	dk_status;	/* copy of status reg. */
	u_char	dk_error;	/* copy of error reg. */
	short	dk_port;	/* i/o port base */
	
        u_long  dk_copenpart;   /* character units open on this drive */
        u_long  dk_bopenpart;   /* block units open on this drive */
        u_long  dk_openpart;    /* all units open on this drive */
	short	dk_wlabel;	/* label writable? */
	short	dk_flags;	/* drive characteistics found */
#define	DKFL_DOSPART	0x00001	 /* has DOS partition table */
#define	DKFL_QUIET	0x00002	 /* report errors back, but don't complain */
#define	DKFL_SINGLE	0x00004	 /* sector at a time mode */
#define	DKFL_ERROR	0x00008	 /* processing a disk error */
#define	DKFL_BSDLABEL	0x00010	 /* has a BSD disk label */
#define	DKFL_BADSECT	0x00020	 /* has a bad144 badsector table */
#define	DKFL_WRITEPROT	0x00040	 /* manual unit write protect */
	struct wdparams dk_params; /* ESDI/IDE drive/controller parameters */
	struct disklabel dk_dd;	/* device configuration data */
	struct	dos_partition
		dk_dospartitions[NDOSPART];	/* DOS view of disk */
	struct	dkbad	dk_bad;	/* bad sector table */
};

struct	disk	*wddrives[_NWD];		/* table of units */
struct	buf	wdtab;
struct	buf	wdutab[_NWD];		/* head of queue per drive */
struct	buf	rwdbuf[_NWD];		/* buffers for raw IO */
long	wdxfer[_NWD];			/* count of transfers */
#ifdef	WDDEBUG
int	wddebug;
#endif

struct	isa_driver wddriver = {
	wdprobe, wdattach, "wd",
};

static void wdustart(struct disk *);
static void wdstart();
static int wdcommand(struct disk *, int);
static int wdcontrol(struct buf *);
static int wdsetctlr(dev_t, struct disk *);
static int wdgetctlr(int, struct disk *);

/*
 * Probe for controller.
 */
int
wdprobe(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	struct disk *du;
	int wdc;

	if (unit >= _NWD)				/* 31 Jul 92*/
		return(0);

	if ((du = wddrives[unit]) == 0) {
		du = wddrives[unit] = (struct disk *)
			malloc (sizeof(struct disk), M_TEMP, M_NOWAIT);
		bzero (du, sizeof(struct disk));	/* 31 Jul 92*/
		du->dk_unit = unit;
	}

	wdc = du->dk_port = dvp->id_iobase;
	
	/* check if we have registers that work */
	outb(wdc+wd_cyl_lo, 0xa5) ;	/* wd_cyl_lo is read/write */
	if(inb(wdc+wd_cyl_lo) != 0xa5)
		goto nodevice;

	/* reset the device */
	outb(wdc+wd_ctlr, (WDCTL_RST|WDCTL_IDS));
	DELAY(1000);
	outb(wdc+wd_ctlr, WDCTL_IDS);
	DELAY(1000);

	/* execute a controller only command */
	if (wdcommand(du, WDCC_DIAGNOSE) < 0)
		goto nodevice;

	(void) inb(wdc+wd_error);	/* XXX! */
	outb(wdc+wd_ctlr, WDCTL_4BIT);
	return (IO_WDCSIZE);

nodevice:
	free(du, M_TEMP);
	wddrives[unit] = 0;
	return (0);
}

/*
 * Attach each drive if possible.
 */
int
wdattach(struct isa_device *dvp)
{
	int unit;
/*	int unit = dvp->id_unit;*/

	for (unit=0; unit< _NWD; unit++) {
		struct disk *du;
		if ((du = wddrives[unit]) == 0) {
			du = wddrives[unit] = (struct disk *)
				malloc (sizeof(struct disk), M_TEMP, M_NOWAIT);
			bzero (du, sizeof(struct disk));
			du->dk_unit = unit;
			du->dk_port = dvp->id_iobase;
		}

		/* print out description of drive, suppressing multiple blanks*/
		if(wdgetctlr(unit, du) == 0)  {
			int i, blank;
			char c;
			printf("wd%d: unit %d type ", unit, unit);
			for (i = blank = 0 ; i < sizeof(du->dk_params.wdp_model); i++) {
				char c = du->dk_params.wdp_model[i];

				if (blank && c == ' ') continue;
				if (blank && c != ' ') {
					printf(" %c", c);
					blank = 0;
					continue;
				}
				if (c == ' ')
					blank = 1;
				else
					printf("%c", c);
			}
			printf("\n");
			du->dk_unit = unit;
		}
	}
	return(1);
}

/* Read/write routine for a buffer.  Finds the proper unit, range checks
 * arguments, and schedules the transfer.  Does not wait for the transfer
 * to complete.  Multi-page transfers are supported.  All I/O requests must
 * be a multiple of a sector in length.
 */
int
wdstrategy(register struct buf *bp)
{
	register struct buf *dp;
	struct disklabel *lp;
	register struct partition *p;
	struct disk *du;	/* Disk unit to do the IO.	*/
	long maxsz, sz;
	int	unit = wdunit(bp->b_dev);
	int	s;

	/* valid unit, controller, and request?  */
	if (unit >= _NWD || bp->b_blkno < 0 || (du = wddrives[unit]) == 0) {

		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* "soft" write protect check */
	if ((du->dk_flags & DKFL_WRITEPROT) && (bp->b_flags & B_READ) == 0) {
		bp->b_error = EROFS;
		bp->b_flags |= B_ERROR;
		goto done;
	}

	/* have partitions and want to use them? */
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW) {

		/*
		 * do bounds checking, adjust transfer. if error, process.
		 * if end of partition, just return
		 */
		if (bounds_check_with_label(bp, &du->dk_dd, du->dk_wlabel) <= 0)
			goto done;
		/* otherwise, process transfer request */
	}

q:
	/* queue transfer on drive, activate drive and controller if idle */
	dp = &wdutab[unit];
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0)
		wdustart(du);		/* start drive */
	if (wdtab.b_active == 0)
		wdstart(s);		/* start controller */
	splx(s);
	return;

done:
	/* toss transfer, we're done early */
	biodone(bp);
}

/*
 * Routine to queue a command to the controller.  The unit's
 * request is linked into the active list for the controller.
 * If the controller is idle, the transfer is started.
 */
static void
wdustart(register struct disk *du)
{
	register struct buf *bp, *dp = &wdutab[du->dk_unit];

	/* unit already active? */
	if (dp->b_active)
		return;

	/* anything to start? */
	bp = dp->b_actf;
	if (bp == NULL)
		return;	

	/* link onto controller queue */
	dp->b_forw = NULL;
	if (wdtab.b_actf  == NULL)
		wdtab.b_actf = dp;
	else
		wdtab.b_actl->b_forw = dp;
	wdtab.b_actl = dp;

	/* mark the drive unit as busy */
	dp->b_active = 1;
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1.	The transfer length must be an exact multiple of the sector size.
 */

static void
wdstart()
{
	register struct disk *du;	/* disk unit for IO */
	register struct buf *bp;
	struct disklabel *lp;
	struct buf *dp;
	register struct bt_bad *bt_ptr;
	long	blknum, pagcnt, cylin, head, sector;
	long	secpertrk, secpercyl, addr, i, timeout;
	int	unit, s, wdc;

loop:
	/* is there a drive for the controller to do a transfer with? */
	dp = wdtab.b_actf;
	if (dp == NULL)
		return;

	/* is there a transfer to this drive ? if so, link it on
	   the controller's queue */
	bp = dp->b_actf;
	if (bp == NULL) {
		wdtab.b_actf = dp->b_forw;
		goto loop;
	}

	/* obtain controller and drive information */
	unit = wdunit(bp->b_dev);
	du = wddrives[unit];

	/* if not really a transfer, do control operations specially */
	if (du->dk_state < OPEN) {
		(void) wdcontrol(bp);
		return;
	}

	/* calculate transfer details */
	blknum = bp->b_blkno + du->dk_skip;
/*if(wddebug)printf("bn%d ", blknum);*/
#ifdef	WDDEBUG
	if (du->dk_skip == 0)
		printf("\nwdstart %d: %s %d@%d; map ", unit,
			(bp->b_flags & B_READ) ? "read" : "write",
			bp->b_bcount, blknum);
	else
		printf(" %d)%x", du->dk_skip, inb(wdc+wd_altsts));
#endif
	addr = (int) bp->b_un.b_addr;
	if (du->dk_skip == 0)
		du->dk_bc = bp->b_bcount;

	lp = &du->dk_dd;
	secpertrk = lp->d_nsectors;
	secpercyl = lp->d_secpercyl;
	if ((du->dk_flags & DKFL_BSDLABEL) != 0 && wdpart(bp->b_dev) != WDRAW)
		blknum += lp->d_partitions[wdpart(bp->b_dev)].p_offset;
	cylin = blknum / secpercyl;
	head = (blknum % secpercyl) / secpertrk;
	sector = blknum % secpertrk;

	/* 
	 * See if the current block is in the bad block list.
	 * (If we have one, and not formatting.)
	 */
	if ((du->dk_flags & (DKFL_SINGLE|DKFL_BADSECT))		/* 19 Aug 92*/
		== (DKFL_SINGLE|DKFL_BADSECT))
	    /* XXX
	     * BAD144END was done to clean up some old bad code that was
	     * attempting to compare a u_short to -1.  This makes the compilers
	     * happy and clearly shows what is going on.
	     * rgrimes 93/06/17
	     */
#define BAD144END (u_short)(-1)
	    for (bt_ptr = du->dk_bad.bt_bad; bt_ptr->bt_cyl != BAD144END; bt_ptr++) {
		if (bt_ptr->bt_cyl > cylin)
			/* Sorted list, and we passed our cylinder. quit. */
			break;
		if (bt_ptr->bt_cyl == cylin &&
				bt_ptr->bt_trksec == (head << 8) + sector) {
			/*
			 * Found bad block.  Calculate new block addr.
			 * This starts at the end of the disk (skip the
			 * last track which is used for the bad block list),
			 * and works backwards to the front of the disk.
			 */
#ifdef	WDDEBUG
			    printf("--- badblock code -> Old = %d; ",
				blknum);
#endif
			blknum = lp->d_secperunit - lp->d_nsectors
				- (bt_ptr - du->dk_bad.bt_bad) - 1;
			cylin = blknum / secpercyl;
			head = (blknum % secpercyl) / secpertrk;
			sector = blknum % secpertrk;
#ifdef	WDDEBUG
			    printf("new = %d\n", blknum);
#endif
			break;
		}
	}
/*if(wddebug)pg("c%d h%d s%d ", cylin, head, sector);*/
	sector += 1;	/* sectors begin with 1, not 0 */

	wdtab.b_active = 1;		/* mark controller active */
	wdc = du->dk_port;

RETRY:
	/* if starting a multisector transfer, or doing single transfers */
	if (du->dk_skip == 0 || (du->dk_flags & DKFL_SINGLE)) {
		if (wdtab.b_errcnt && (bp->b_flags & B_READ) == 0)
			du->dk_bc += DEV_BSIZE;

		/* controller idle? */
		timeout = 0;
		while (inb(wdc+wd_status) & WDCS_BUSY)
		{
			if (++timeout > WDCTIMEOUT)
			{
				printf("wd.c: Controller busy too long!\n");
				/* reset the device */
				outb(wdc+wd_ctlr, (WDCTL_RST|WDCTL_IDS));
				DELAY(1000);
				outb(wdc+wd_ctlr, WDCTL_IDS);
				DELAY(1000);
				(void) inb(wdc+wd_error);	/* XXX! */
				outb(wdc+wd_ctlr, WDCTL_4BIT);
				break;
			}
		}

		/* stuff the task file */
		outb(wdc+wd_precomp, lp->d_precompcyl / 4);
#ifdef	B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			outb(wdc+wd_sector, lp->d_gap3);
			outb(wdc+wd_seccnt, lp->d_nsectors);
		} else {
#endif
		if (du->dk_flags & DKFL_SINGLE)
			outb(wdc+wd_seccnt, 1);
		else
			outb(wdc+wd_seccnt, howmany(du->dk_bc, DEV_BSIZE));
		outb(wdc+wd_sector, sector);

#ifdef	B_FORMAT
		}
#endif

		outb(wdc+wd_cyl_lo, cylin);
		outb(wdc+wd_cyl_hi, cylin >> 8);

		/* set up the SDH register (select drive) */
		outb(wdc+wd_sdh, WDSD_IBM | (unit<<4) | (head & 0xf));

		/* wait for drive to become ready */
		timeout = 0;
		while ((inb(wdc+wd_status) & WDCS_READY) == 0)
		{
			if (++timeout > WDCTIMEOUT)
			{
				printf("wd.c: Drive busy too long!\n");
				/* reset the device */
				outb(wdc+wd_ctlr, (WDCTL_RST|WDCTL_IDS));
				DELAY(1000);
				outb(wdc+wd_ctlr, WDCTL_IDS);
				DELAY(1000);
				(void) inb(wdc+wd_error);	/* XXX! */
				outb(wdc+wd_ctlr, WDCTL_4BIT);
				goto RETRY;
			}
		}

		/* initiate command! */
#ifdef	B_FORMAT
		if (bp->b_flags & B_FORMAT)
			outb(wdc+wd_command, WDCC_FORMAT);
		else
#endif
		outb(wdc+wd_command,
			(bp->b_flags & B_READ)? WDCC_READ : WDCC_WRITE);
#ifdef	WDDEBUG
		printf("sector %d cylin %d head %d addr %x sts %x\n",
	    		sector, cylin, head, addr, inb(wdc+wd_altsts));
#endif
	}

	/* if this is a read operation, just go away until it's done.	*/
	if (bp->b_flags & B_READ) return;

	/* ready to send data?	*/
	timeout = 0;
	while ((inb(wdc+wd_status) & WDCS_DRQ) == 0)
	{
		if (++timeout > WDCTIMEOUT)
		{
			printf("wd.c: Drive not ready for too long!\n");
			/* reset the device */
			outb(wdc+wd_ctlr, (WDCTL_RST|WDCTL_IDS));
			DELAY(1000);
			outb(wdc+wd_ctlr, WDCTL_IDS);
			DELAY(1000);
			(void) inb(wdc+wd_error);	/* XXX! */
			outb(wdc+wd_ctlr, WDCTL_4BIT);
			goto RETRY;
		}
	}

	/* then send it! */
	outsw (wdc+wd_data, addr+du->dk_skip * DEV_BSIZE,
		DEV_BSIZE/sizeof(short));
	du->dk_bc -= DEV_BSIZE;
}

/* Interrupt routine for the controller.  Acknowledge the interrupt, check for
 * errors on the current operation, mark it done if necessary, and start
 * the next request.  Also check for a partially done transfer, and
 * continue with the next chunk if so.
 */
void
wdintr(struct intrframe wdif)
{
	register struct	disk *du;
	register struct buf *bp, *dp;
	int status, wdc;
	char partch ;

	if (!wdtab.b_active) {
#ifdef nyet
		printf("wd: extra interrupt\n");
#endif
		return;
	}

	dp = wdtab.b_actf;
	bp = dp->b_actf;
	du = wddrives[wdunit(bp->b_dev)];
	wdc = du->dk_port;

#ifdef	WDDEBUG
	printf("I ");
#endif

	while ((status = inb(wdc+wd_status)) & WDCS_BUSY) ;

	/* is it not a transfer, but a control operation? */
	if (du->dk_state < OPEN) {
		if (wdcontrol(bp))
			wdstart();
		return;
	}

	/* have we an error? */
	if (status & (WDCS_ERR | WDCS_ECCCOR)) {

		du->dk_status = status;
		du->dk_error = inb(wdc + wd_error);
#ifdef	WDDEBUG
		printf("status %x error %x\n", status, du->dk_error);
#endif
		if((du->dk_flags & DKFL_SINGLE) == 0) {
			du->dk_flags |=  DKFL_ERROR;
			goto outt;
		}
#ifdef B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			bp->b_error = EIO;		/* 17 Sep 92*/
			bp->b_flags |= B_ERROR;
			goto done;
		}
#endif
		
		/* error or error correction? */
		if (status & WDCS_ERR) {
			if (++wdtab.b_errcnt < RETRIES) {
				wdtab.b_active = 0;
			} else {
				diskerr(bp, "wd", "hard error", LOG_PRINTF,
					du->dk_skip, &du->dk_dd);
#ifdef WDDEBUG
				printf( "status %b error %b\n",
					status, WDCS_BITS,
					inb(wdc+wd_error), WDERR_BITS);
#endif
				bp->b_error = EIO;	/* 17 Sep 92*/
				bp->b_flags |= B_ERROR;	/* flag the error */
			}
		} else {
			diskerr(bp, "wd", "soft ecc", 0,
				du->dk_skip, &du->dk_dd);
		}
	}
outt:

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ) && wdtab.b_active) {
		int chk, dummy;

		chk = min(DEV_BSIZE / sizeof(short), du->dk_bc / sizeof(short));

		/* ready to receive data? */
		while ((inb(wdc+wd_status) & WDCS_DRQ) == 0)
			;

		/* suck in data */
		insw (wdc+wd_data,
			(int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE, chk);
		du->dk_bc -= chk * sizeof(short);

		/* for obselete fractional sector reads */
		while (chk++ < 256) insw (wdc+wd_data, &dummy, 1);
	}

	wdxfer[du->dk_unit]++;
	if (wdtab.b_active) {
		if ((bp->b_flags & B_ERROR) == 0) {
			du->dk_skip++;		/* Add to successful sectors. */
			if (wdtab.b_errcnt)
				diskerr(bp, "wd", "soft error", 0,
					du->dk_skip, &du->dk_dd);
			wdtab.b_errcnt = 0;

			/* see if more to transfer */
			if (du->dk_bc > 0 && (du->dk_flags & DKFL_ERROR) == 0) {
				wdstart();
				return;		/* next chunk is started */
			} else if ((du->dk_flags & (DKFL_SINGLE|DKFL_ERROR))
					== DKFL_ERROR) {
				du->dk_skip = 0;
				du->dk_flags &= ~DKFL_ERROR;
				du->dk_flags |=  DKFL_SINGLE;
				wdstart();
				return;		/* redo xfer sector by sector */
			}
		}

done:
		/* done with this transfer, with or without error */
		du->dk_flags &= ~DKFL_SINGLE;
		wdtab.b_actf = dp->b_forw;
		wdtab.b_errcnt = 0;
		du->dk_skip = 0;
		dp->b_active = 0;
		dp->b_actf = bp->av_forw;
		dp->b_errcnt = 0;
		bp->b_resid = 0;
		biodone(bp);
	}

	/* controller idle */
	wdtab.b_active = 0;

	/* anything more on drive queue? */
	if (dp->b_actf)
		wdustart(du);
	/* anything more for controller to do? */
	if (wdtab.b_actf)
		wdstart();
}

/*
 * Initialize a drive.
 */
int
wdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	register unsigned int unit;
	register struct disk *du;
        int part = wdpart(dev), mask = 1 << part;
        struct partition *pp;
	struct dkbad *db;
	int i, error = 0;
	char *msg;

	unit = wdunit(dev);
	if (unit >= _NWD) return (ENXIO) ;

	du = wddrives[unit];
	if (du == 0) return (ENXIO) ;

	if ((du->dk_flags & DKFL_BSDLABEL) == 0) {
		du->dk_flags |= DKFL_WRITEPROT;
		wdutab[unit].b_actf = NULL;

		/*
		 * Use the default sizes until we've read the label,
		 * or longer if there isn't one there.
		 */
		bzero(&du->dk_dd, sizeof(du->dk_dd));
#undef d_type /* fix goddamn segments.h! XXX */
		du->dk_dd.d_type = DTYPE_ST506;
		du->dk_dd.d_ncylinders = 1024;
		du->dk_dd.d_secsize = DEV_BSIZE;
		du->dk_dd.d_ntracks = 8;
		du->dk_dd.d_nsectors = 17;
		du->dk_dd.d_secpercyl = 17*8;
		du->dk_state = WANTOPEN;
		du->dk_unit = unit;

		/* read label using "c" partition */
		if (msg = readdisklabel(makewddev(major(dev), wdunit(dev), WDRAW),
				wdstrategy, &du->dk_dd, du->dk_dospartitions,
				&du->dk_bad, 0)) {
			log(LOG_WARNING, "wd%d: cannot find label (%s)\n",
				unit, msg);
			if (part != WDRAW)
				error = EINVAL;		/* XXX needs translation */
			goto done;
		} else {

			wdsetctlr(dev, du);
			du->dk_flags |= DKFL_BSDLABEL;
			du->dk_flags &= ~DKFL_WRITEPROT;
			if (du->dk_dd.d_flags & D_BADSECT)
				du->dk_flags |= DKFL_BADSECT;
		}

done:
		if (error)
			return(error);

	}
        /*
         * Warn if a partion is opened
         * that overlaps another partition which is open
         * unless one is the "raw" partition (whole disk).
         */
        if ((du->dk_openpart & mask) == 0 /*&& part != RAWPART*/ && part != WDRAW) {
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
                        /*if (pp - du->dk_dd.d_partitions == RAWPART)
                                continue; */
                        if (pp - du->dk_dd.d_partitions == WDRAW)
                                continue;
                        if (du->dk_openpart & (1 << (pp -
					du->dk_dd.d_partitions)))
                                log(LOG_WARNING,
                                    "wd%d%c: overlaps open partition (%c)\n",
                                    unit, part + 'a',
                                    pp - du->dk_dd.d_partitions + 'a');
                }
        }
        if (part >= du->dk_dd.d_npartitions && part != WDRAW)
                return (ENXIO);

	/* insure only one open at a time */
        du->dk_openpart |= mask;
        switch (fmt) {
        case S_IFCHR:
                du->dk_copenpart |= mask;
                break;
        case S_IFBLK:
                du->dk_bopenpart |= mask;
                break;
        }
	return (0);
}

/*
 * Implement operations other than read/write.
 * Called from wdstart or wdintr during opens and formats.
 * Uses finite-state-machine to track progress of operation in progress.
 * Returns 0 if operation still in progress, 1 if completed.
 */
static int
wdcontrol(register struct buf *bp)
{
	register struct disk *du;
	register unit;
	unsigned char  stat;
	int s, cnt;
	extern int bootdev;
	int cyl, trk, sec, i, wdc;
	struct wdparams foo;

	du = wddrives[wdunit(bp->b_dev)];
	unit = du->dk_unit;
	wdc = du->dk_port;
	
	switch (du->dk_state) {

	tryagainrecal:
	case WANTOPEN:			/* set SDH, step rate, do restore */
#ifdef	WDDEBUG
		printf("wd%d: recal ", unit);
#endif
		s = splbio();		/* not called from intr level ... */
		wdgetctlr(unit, du);

		outb(wdc+wd_sdh, WDSD_IBM | (unit << 4));
		wdtab.b_active = 1;

		/* wait for drive and controller to become ready */
		for (i = WDCTIMEOUT; (inb(wdc+wd_status) & (WDCS_READY|WDCS_BUSY))
				  != WDCS_READY && i-- != 0; )
			;
		outb(wdc+wd_command, WDCC_RESTORE | WD_STEP);
		du->dk_state++;
		splx(s);
		return(0);

	case RECAL:
		if ((stat = inb(wdc+wd_status)) & WDCS_ERR) {
			printf("wd%d: recal", du->dk_unit);
			printf(": status %b error %b\n", stat, WDCS_BITS,
				inb(wdc+wd_error), WDERR_BITS);
			if (++wdtab.b_errcnt < RETRIES) {
				du->dk_state = WANTOPEN;
				goto tryagainrecal;
			}
			bp->b_error = ENXIO;	/* XXX needs translation */
			goto badopen;
		}

		/* some controllers require this ... */
		wdsetctlr(bp->b_dev, du);

		wdtab.b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done
		 * by normal means.
		 */
		return(1);

	default:
		panic("wdcontrol");
	}
	/* NOTREACHED */

badopen:
	printf(": status %b error %b\n", stat, WDCS_BITS,
		inb(wdc + wd_error), WDERR_BITS);
	bp->b_flags |= B_ERROR;
	return(1);
}

/*
 * send a command and wait uninterruptibly until controller is finished.
 * return -1 if controller busy for too long, otherwise
 * return status. intended for brief controller commands at critical points.
 * assumes interrupts are blocked.
 */
static int
wdcommand(struct disk *du, int cmd) {
	int timeout = WDCTIMEOUT, stat, wdc;

	/* controller ready for command? */
	wdc = du->dk_port;
	while (((stat = inb(wdc + wd_status)) & WDCS_BUSY) && timeout > 0)
		timeout--;
	if (timeout <= 0)
		return(-1);

	/* send command, await results */
	outb(wdc+wd_command, cmd);
	while (((stat = inb(wdc+wd_status)) & WDCS_BUSY) && timeout > 0)
		timeout--;
	if (timeout <= 0)
		return(-1);
	if (cmd != WDCC_READP)
		return (stat);

	/* is controller ready to return data? */
	while (((stat = inb(wdc+wd_status)) & (WDCS_ERR|WDCS_DRQ)) == 0 &&
		timeout > 0)
		timeout--;
	if (timeout <= 0)
		return(-1);

	return (stat);
}

/*
 * issue IDC to drive to tell it just what geometry it is to be.
 */
static int
wdsetctlr(dev_t dev, struct disk *du) {
	int stat, x, wdc;

/*printf("C%dH%dS%d ", du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks,
	du->dk_dd.d_nsectors);*/

	x = splbio();
	wdc = du->dk_port;
	outb(wdc+wd_cyl_lo, du->dk_dd.d_ncylinders+1);
	outb(wdc+wd_cyl_hi, (du->dk_dd.d_ncylinders+1)>>8);
	outb(wdc+wd_sdh, WDSD_IBM | (wdunit(dev) << 4) + du->dk_dd.d_ntracks-1);
	outb(wdc+wd_seccnt, du->dk_dd.d_nsectors);
	stat = wdcommand(du, WDCC_IDC);

	if (stat < 0) {
	  	splx(x);
		return(stat);
      	}
	if (stat & WDCS_ERR)
		printf("wdsetctlr: status %b error %b\n",
			stat, WDCS_BITS, inb(wdc+wd_error), WDERR_BITS);
	splx(x);
	return(stat);
}

/*
 * issue READP to drive to ask it what it is.
 */
static int
wdgetctlr(int u, struct disk *du) {
	int stat, x, i, wdc;
	char tb[DEV_BSIZE];
	struct wdparams *wp;

	x = splbio();		/* not called from intr level ... */
	wdc = du->dk_port;
	outb(wdc+wd_sdh, WDSD_IBM | (u << 4));
	stat = wdcommand(du, WDCC_READP);

	if (stat < 0) {
		splx(x);
		return(stat);
	}
	/*
	 * If WDCC_READP fails then we might have an old ST506 type drive
	 * so we try a seek to 0; if that passes then the
	 * drive is there but it's OLD AND KRUSTY
	 */
	if (stat & WDCS_ERR) {
		stat = wdcommand(du, WDCC_RESTORE | WD_STEP);
		if (stat & WDCS_ERR) {
	  		stat = inb(wdc+wd_error);
			splx(x);
			return(stat);
		}

		strncpy(du->dk_dd.d_typename, "ST506",
			sizeof du->dk_dd.d_typename);
		strncpy(du->dk_params.wdp_model, "Unknown Type",
			sizeof du->dk_params.wdp_model);
		du->dk_dd.d_type = DTYPE_ST506;
		splx(x);
		return(0);
	}

	/* obtain parameters */
	wp = &du->dk_params;
	insw(wdc+wd_data, tb, sizeof(tb)/sizeof(short));
	bcopy(tb, wp, sizeof(struct wdparams));

	/* shuffle string byte order */
	for (i=0; i < sizeof(wp->wdp_model) ;i+=2) {
		u_short *p;
		p = (u_short *) (wp->wdp_model + i);
		*p = ntohs(*p);
	}
/*printf("gc %x cyl %d trk %d sec %d type %d sz %d model %s\n", wp->wdp_config,
wp->wdp_fixedcyl+wp->wdp_removcyl, wp->wdp_heads, wp->wdp_sectors,
wp->wdp_cntype, wp->wdp_cnsbsz, wp->wdp_model);*/

	/* update disklabel given drive information */
	du->dk_dd.d_ncylinders = wp->wdp_fixedcyl + wp->wdp_removcyl /*+- 1*/;
	du->dk_dd.d_ntracks = wp->wdp_heads;
	du->dk_dd.d_nsectors = wp->wdp_sectors;
	du->dk_dd.d_secpercyl = du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
	du->dk_dd.d_partitions[1].p_size = du->dk_dd.d_secpercyl *
			wp->wdp_sectors;
	du->dk_dd.d_partitions[1].p_offset = 0;
	/* dubious ... */
	bcopy("ESDI/IDE", du->dk_dd.d_typename, 9);
	bcopy(wp->wdp_model+20, du->dk_dd.d_packname, 14-1);
	/* better ... */
	du->dk_dd.d_type = DTYPE_ESDI;
	du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;

	/* XXX sometimes possibly needed */
	(void) inb(wdc+wd_status);
	return (0);
}


/* ARGSUSED */
int
wdclose(dev_t dev, int flags, int fmt)
{
	register struct disk *du;
        int part = wdpart(dev), mask = 1 << part;

	du = wddrives[wdunit(dev)];

	/* insure only one open at a time */
        du->dk_openpart &= ~mask;
        switch (fmt) {
        case S_IFCHR:
                du->dk_copenpart &= ~mask;
                break;
        case S_IFBLK:
                du->dk_bopenpart &= ~mask;
                break;
        }
	return(0);
}

int
wdioctl(dev_t dev, int cmd, caddr_t addr, int flag)
{
	int unit = wdunit(dev);
	register struct disk *du;
	int error = 0;
	struct uio auio;
	struct iovec aiov;

	du = wddrives[unit];

	switch (cmd) {

	case DIOCSBAD:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
		else
			du->dk_bad = *(struct dkbad *)addr;
		break;

	case DIOCGDINFO:
		*(struct disklabel *)addr = du->dk_dd;
		break;

        case DIOCGPART:
                ((struct partinfo *)addr)->disklab = &du->dk_dd;
                ((struct partinfo *)addr)->part =
                    &du->dk_dd.d_partitions[wdpart(dev)];
                break;

        case DIOCSDINFO:
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        error = setdisklabel(&du->dk_dd,
					(struct disklabel *)addr,
                         /*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart : */0,
				du->dk_dospartitions);
                if (error == 0) {
			du->dk_flags |= DKFL_BSDLABEL;
			wdsetctlr(dev, du);
		}
                break;

        case DIOCWLABEL:
		du->dk_flags &= ~DKFL_WRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else
                        du->dk_wlabel = *(int *)addr;
                break;

        case DIOCWDINFO:
		du->dk_flags &= ~DKFL_WRITEPROT;
                if ((flag & FWRITE) == 0)
                        error = EBADF;
                else if ((error = setdisklabel(&du->dk_dd, (struct disklabel *)addr,
                         /*(du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart :*/ 0,
				du->dk_dospartitions)) == 0) {
                        int wlab;

			du->dk_flags |= DKFL_BSDLABEL;
			wdsetctlr(dev, du);

                        /* simulate opening partition 0 so write succeeds */
                        du->dk_openpart |= (1 << 0);            /* XXX */
                        wlab = du->dk_wlabel;
                        du->dk_wlabel = 1;
                        error = writedisklabel(dev, wdstrategy,
				&du->dk_dd, du->dk_dospartitions);
                        du->dk_openpart = du->dk_copenpart | du->dk_bopenpart;
                        du->dk_wlabel = wlab;
                }
                break;

#ifdef notyet
	case DIOCGDINFOP:
		*(struct disklabel **)addr = &(du->dk_dd);
		break;

	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		else {
			register struct format_op *fop;

			fop = (struct format_op *)addr;
			aiov.iov_base = fop->df_buf;
			aiov.iov_len = fop->df_count;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_resid = fop->df_count;
			auio.uio_segflg = 0;
			auio.uio_offset =
				fop->df_startblk * du->dk_dd.d_secsize;
			error = physio(wdformat, &rwdbuf[unit], dev, B_WRITE,
				minphys, &auio);
			fop->df_count -= auio.uio_resid;
			fop->df_reg[0] = du->dk_status;
			fop->df_reg[1] = du->dk_error;
		}
		break;
#endif

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

#ifdef	B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return (wdstrategy(bp));
}
#endif

int
wdsize(dev_t dev)
{
	int unit = wdunit(dev), part = wdpart(dev), val = 0;
	struct disk *du;

	if (unit >= _NWD)	/* 31 Jul 92*/
		return(-1);

	du = wddrives[unit];
	if (du == 0 || du->dk_state == 0)
		val = wdopen (makewddev(major(dev), unit, WDRAW), FREAD, S_IFBLK, 0);
	if (du == 0 || val != 0 || du->dk_flags & DKFL_WRITEPROT)
		return (-1);

	return((int)du->dk_dd.d_partitions[part].p_size);
}

extern        char *vmmap;            /* poor name! */

int
wddump(dev_t dev)			/* dump core after a system crash */
{
	register struct disk *du;	/* disk unit to do the IO */
	register struct bt_bad *bt_ptr;
	long	num;			/* number of sectors to write */
	int	unit, part, wdc;
	long	blkoff, blknum, blkcnt;
	long	cylin, head, sector, stat;
	long	secpertrk, secpercyl, nblocks, i;
	char *addr;
	extern	int Maxmem;
	static  wddoingadump = 0 ;
	extern caddr_t CADDR1;

	addr = (char *) 0;		/* starting address */

	/* toss any characters present prior to dump */
	while (sgetc(1))
		;

	/* size of memory to dump */
	num = Maxmem;
	unit = wdunit(dev);		/* eventually support floppies? */
	part = wdpart(dev);		/* file system */
	/* check for acceptable drive number */
	if (unit >= _NWD) return(ENXIO);		/* 31 Jul 92*/

	du = wddrives[unit];
	if (du == 0) return(ENXIO);
	/* was it ever initialized ? */
	if (du->dk_state < OPEN) return (ENXIO) ;
	if (du->dk_flags & DKFL_WRITEPROT) return(ENXIO);
	wdc = du->dk_port;

	/* Convert to disk sectors */
	num = (u_long) num * NBPG / du->dk_dd.d_secsize;

	/* check if controller active */
	/*if (wdtab.b_active) return(EFAULT); */
	if (wddoingadump) return(EFAULT);

	secpertrk = du->dk_dd.d_nsectors;
	secpercyl = du->dk_dd.d_secpercyl;
	nblocks = du->dk_dd.d_partitions[part].p_size;
	blkoff = du->dk_dd.d_partitions[part].p_offset;

/*pg("xunit %x, nblocks %d, dumplo %d num %d\n", part,nblocks,dumplo,num);*/
	/* check transfer bounds against partition size */
	if ((dumplo < 0) || ((dumplo + num) > nblocks))
		return(EINVAL);

	/*wdtab.b_active = 1;		/* mark controller active for if we
					   panic during the dump */
	wddoingadump = 1  ;  i = 100000 ;
	while ((inb(wdc+wd_status) & WDCS_BUSY) && (i-- > 0)) ;
	outb(wdc+wd_sdh, WDSD_IBM | (unit << 4));
	outb(wdc+wd_command, WDCC_RESTORE | WD_STEP);
	while (inb(wdc+wd_status) & WDCS_BUSY) ;

	/* some compaq controllers require this ... */
	wdsetctlr(dev, du);
	
	blknum = dumplo + blkoff;
	while (num > 0) {
#ifdef notdef
		if (blkcnt > MAXTRANSFER) blkcnt = MAXTRANSFER;
		if ((blknum + blkcnt - 1) / secpercyl != blknum / secpercyl)
			blkcnt = secpercyl - (blknum % secpercyl);
			    /* keep transfer within current cylinder */
#endif
		pmap_enter(kernel_pmap, CADDR1, trunc_page(addr), VM_PROT_READ, TRUE);

		/* compute disk address */
		cylin = blknum / secpercyl;
		head = (blknum % secpercyl) / secpertrk;
		sector = blknum % secpertrk;

#ifdef notyet
		/* 
		 * See if the current block is in the bad block list.
		 * (If we have one.)
		 */
	    		for (bt_ptr = du->dk_bad.bt_bad;
				bt_ptr->bt_cyl != -1; bt_ptr++) {
			if (bt_ptr->bt_cyl > cylin)
				/* Sorted list, and we passed our cylinder.
					quit. */
				break;
			if (bt_ptr->bt_cyl == cylin &&
				bt_ptr->bt_trksec == (head << 8) + sector) {
			/*
			 * Found bad block.  Calculate new block addr.
			 * This starts at the end of the disk (skip the
			 * last track which is used for the bad block list),
			 * and works backwards to the front of the disk.
			 */
				blknum = (du->dk_dd.d_secperunit)
					- du->dk_dd.d_nsectors
					- (bt_ptr - du->dk_bad.bt_bad) - 1;
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
				break;
			}

#endif
		sector++;		/* origin 1 */

		/* select drive.     */
		outb(wdc+wd_sdh, WDSD_IBM | (unit<<4) | (head & 0xf));
		while ((inb(wdc+wd_status) & WDCS_READY) == 0) ;

		/* transfer some blocks */
		outb(wdc+wd_sector, sector);
		outb(wdc+wd_seccnt,1);
		outb(wdc+wd_cyl_lo, cylin);
		outb(wdc+wd_cyl_hi, cylin >> 8);
#ifdef notdef
		/* lets just talk about this first...*/
		pg ("sdh 0%o sector %d cyl %d addr 0x%x",
			inb(wdc+wd_sdh), inb(wdc+wd_sector),
			inb(wdc+wd_cyl_hi)*256+inb(wdc+wd_cyl_lo), addr) ;
#endif
		outb(wdc+wd_command, WDCC_WRITE);
		
		/* Ready to send data?	*/
		while ((inb(wdc+wd_status) & WDCS_DRQ) == 0) ;
		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;

		outsw (wdc+wd_data, CADDR1+((int)addr&(NBPG-1)), 256);

		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;
		/* Check data request (should be done).         */
		if (inb(wdc+wd_status) & WDCS_DRQ) return(EIO) ;

		/* wait for completion */
		for ( i = WDCTIMEOUT ; inb(wdc+wd_status) & WDCS_BUSY ; i--) {
				if (i < 0) return (EIO) ;
		}
		/* error check the xfer */
		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;

		if ((unsigned)addr % (1024*1024) == 0) printf("%d ", num/2048) ;
		/* update block count */
		num--;
		blknum++ ;
		(int) addr += 512;

		/* operator aborting dump? */
		if (sgetc(1))
			return(EINTR);
	}
	return(0);
}
#endif
