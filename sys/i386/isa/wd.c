static int wdtest = 0;

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
 *	from: @(#)wx.c	7.2 (Berkeley) 5/9/91
 *	$Id: wx.c,v 1.5 1993/11/19 06:30:02 davidg Exp $
 */

/* TODO:peel out buffer at low ipl, speed improvement */


#include "wd.h"
#if	NWDC > 0

#include "param.h"
#include "dkbad.h"
#include "systm.h"
#include "kernel.h"
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

#ifndef WDCTIMEOUT
#define WDCTIMEOUT	10000000  /* arbitrary timeout for drive ready waits */
#endif

#define	RETRIES		5	/* number of retries before giving up */
#define RECOVERYTIME	500000	/* usec for controller to recover after err */
#define	MAXTRANSFER	32	/* max size of transfer in page clusters */

#define wdnoreloc(dev)	(minor(dev) & 0x80)	/* ignore partition table */
#define wddospart(dev)	(minor(dev) & 0x40)	/* use dos partitions */
#define wdunit(dev)	((minor(dev) & 0x38) >> 3)
#define wdpart(dev)	(minor(dev) & 0x7)
#define makewddev(maj, unit, part)	(makedev(maj,((unit<<3)+part)))
#define WDRAW	3		/* 'd' partition isn't a partition! */

#define b_cylin	b_resid		/* cylinder number for doing IO to */
				/* shares an entry in the buf struct */

#define	id_physid id_scsiid	/* this biotab field doubles as a field */
				/* for the physical unit number on the controller */

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
	char	dk_ctrlr;	/* physical controller number */
	char	dk_unit;	/* physical unit number */
	char	dk_lunit;	/* logical unit number */
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
#define	DKFL_SINGLE	0x00004	 /* sector at a time mode */
#define	DKFL_ERROR	0x00008	 /* processing a disk error */
#define	DKFL_BSDLABEL	0x00010	 /* has a BSD disk label */
#define	DKFL_BADSECT	0x00020	 /* has a bad144 badsector table */
#define	DKFL_WRITEPROT	0x00040	 /* manual unit write protect */
#define	DKFL_LABELLING	0x00080	 /* readdisklabel() in progress */
	struct wdparams dk_params; /* ESDI/IDE drive/controller parameters */
	struct disklabel dk_dd;	/* device configuration data */
	struct	dos_partition
		dk_dospartitions[NDOSPART];	/* DOS view of disk */
	struct	dkbad	dk_bad;	/* bad sector table */
};

/*static*/	struct	disk	*wddrives[NWD];	/* table of units */
static	struct	buf	wdtab[NWDC];
static	struct	buf	wdutab[NWD];	/* head of queue per drive */
#ifdef notyet
static	struct	buf	rwdbuf[NWD];	/* buffers for raw IO */
#endif
static	long	wdxfer[NWD];		/* count of transfers */

static	int	wdprobe(struct isa_device *dvp);
static	int	wdattach(struct isa_device *dvp);
static	void	wdustart(struct disk *du);
static	void	wdstart(int ctrlr);
static	int	wdcontrol(struct buf *bp);
static	int	wdcommand(struct disk *du, u_int cylinder, u_int head,
			  u_int sector, u_int count, u_int command);
static	int	wdsetctlr(struct disk *du);
static	int	wdwsetctlr(struct disk *du);
static	int	wdgetctlr(struct disk *du);
static	void	wderror(struct buf *bp, struct disk *du, char *mesg);
static	int	wdreset(struct disk *du);
static	void	wdsleep(int ctrlr, char *wmesg);
static	int	wdunwedge(struct disk *du);
static	int	wdwait(struct disk *du, u_char bits_wanted);

struct	isa_driver wdcdriver = {
	wdprobe, wdattach, "wdc",
};

/*
 * Probe for controller.
 */
static int
wdprobe(struct isa_device *dvp)
{
	int unit = dvp->id_unit;
	int u;
	struct disk *du;
	int wdc;

	if (unit >= NWDC)				/* 31 Jul 92*/
		return(0);

	du = malloc (sizeof(struct disk), M_TEMP, M_NOWAIT);
	bzero (du, sizeof(struct disk));	/* 31 Jul 92*/
	du->dk_ctrlr = dvp->id_unit;
	du->dk_unit = 0;
	du->dk_lunit = 0;

	wdc = du->dk_port = dvp->id_iobase;

	/* check if we have registers that work */
	outb(wdc+wd_cyl_lo, 0xa5) ;	/* wd_cyl_lo is read/write */
	if(inb(wdc+wd_cyl_lo) != 0xa5)
		goto nodevice;

	if (wdreset(du) != 0 && (DELAY(RECOVERYTIME), wdreset(du)) != 0)
		goto nodevice;

	/* execute a controller only command */
	if (wdcommand(du, 0, 0, 0, 0, WDCC_DIAGNOSE) != 0
	    || wdwait(du, 0) != 0)
		goto nodevice;

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
	int unit, lunit;
	struct isa_device *wdup;
	struct disk *du;

	if (dvp->id_unit >= NWDC)
		return(0);

	printf("wdc%d:", dvp->id_unit);

	for (wdup = isa_biotab_wdc; wdup->id_driver != 0; wdup++) {
		if (wdup->id_iobase != dvp->id_iobase)
			continue;
		lunit = wdup->id_unit;
		if (lunit >= NWD)
			continue;
		unit = wdup->id_physid;

		du = wddrives[lunit] = (struct disk *)
			malloc (sizeof(struct disk), M_TEMP, M_NOWAIT);
		bzero (du, sizeof(struct disk));
		bzero (&wdutab[lunit], sizeof(struct buf));
#ifdef notyet
		bzero (&rwdbuf[lunit], sizeof(struct buf));
#endif
		wdxfer[lunit] = 0;

		du->dk_ctrlr = dvp->id_unit;
		du->dk_unit = unit;
		du->dk_lunit = lunit;
		du->dk_port = dvp->id_iobase;

		/* print out description of drive, suppressing multiple blanks*/
		if (wdgetctlr(du) == 0)  {
			int i, blank;

			printf(" [%d: wd%d: ", unit, lunit);
			for (i = blank = 0 ; i < sizeof(du->dk_params.wdp_model); i++) {
				char c = du->dk_params.wdp_model[i];

				if (blank && c == ' ')
					continue;
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
			printf("]");
		}
		else {
			free(du, M_TEMP);
			wddrives[unit] = 0;
			printf(" [%d: wd%d]", unit, lunit);
		}
	}
	printf("\n");
	return(1);
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
	struct disk *du;			/* Disk unit to do the IO. */
	int	lunit = wdunit(bp->b_dev);
	int	s;

	/* valid unit, controller, and request?  */
	if (lunit >= NWD || bp->b_blkno < 0 || (du = wddrives[lunit]) == 0) {

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

	/* queue transfer on drive, activate drive and controller if idle */
	dp = &wdutab[lunit];
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0)
		wdustart(du);			/* start drive */

	/* Pick up changes made by readdisklabel(). */
	if (du->dk_flags & DKFL_LABELLING && du->dk_state > RECAL) {
		wdsleep(du->dk_ctrlr, "wdlab");
		du->dk_state = WANTOPEN;
	}

	if (wdtab[du->dk_ctrlr].b_active == 0)
		wdstart(du->dk_ctrlr);		/* start controller */
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
	register struct buf *bp, *dp = &wdutab[du->dk_lunit];
	int ctrlr = du->dk_ctrlr;

	/* unit already active? */
	if (dp->b_active)
		return;

	/* anything to start? */
	bp = dp->b_actf;
	if (bp == NULL)
		return;

	/* link onto controller queue */
	dp->b_forw = NULL;
	if (wdtab[ctrlr].b_actf  == NULL)
		wdtab[ctrlr].b_actf = dp;
	else
		wdtab[ctrlr].b_actl->b_forw = dp;
	wdtab[ctrlr].b_actl = dp;

	/* mark the drive unit as busy */
	dp->b_active = 1;
}

/*
 * Controller startup routine.  This does the calculation, and starts
 * a single-sector read or write operation.  Called to start a transfer,
 * or from the interrupt routine to continue a multi-sector transfer.
 * RESTRICTIONS:
 * 1. The transfer length must be an exact multiple of the sector size.
 */

static void
wdstart(int ctrlr)
{
	register struct disk *du;	/* disk unit for IO */
	register struct buf *bp;
	struct disklabel *lp;
	struct buf *dp;
	register struct bt_bad *bt_ptr;
	long	blknum, cylin, head, sector;
	long	secpertrk, secpercyl, addr;
	int	lunit, wdc;

loop:
	/* is there a drive for the controller to do a transfer with? */
	dp = wdtab[ctrlr].b_actf;
	if (dp == NULL)
		return;

	/* is there a transfer to this drive? */
	/* If so, link it on the controller's queue */
	bp = dp->b_actf;
	if (bp == NULL) {
		wdtab[ctrlr].b_actf = dp->b_forw;
		goto loop;
	}

	/* obtain controller and drive information */
	lunit = wdunit(bp->b_dev);
	du = wddrives[lunit];
	wdc = du->dk_port;

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
	blknum = bp->b_blkno + du->dk_skip;
#ifdef WDDEBUG
	if (du->dk_skip == 0)
		printf("wd%d: wdstart: %s %d@%d; map ", lunit,
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
	    for (bt_ptr = du->dk_bad.bt_bad; bt_ptr->bt_cyl != 0xffff;
		 bt_ptr++) {
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
			printf("--- badblock code -> Old = %d; ", blknum);
#endif

			/*
			 * XXX the offset of the bad sector table ought
			 * to be stored in the in-core copy of the table.
			 */
#define BAD144_PART	2	/* XXX scattered magic numbers */
#define BSD_PART	0	/* XXX should be 2 but bad144.c uses 0 */
			if (lp->d_partitions[BSD_PART].p_offset != 0)
				blknum = lp->d_partitions[BAD144_PART].p_offset
					 + lp->d_partitions[BAD144_PART].p_size;
			else
				blknum = lp->d_secperunit;
			blknum -= lp->d_nsectors + (bt_ptr - du->dk_bad.bt_bad)
				  + 1;

			cylin = blknum / secpercyl;
			head = (blknum % secpercyl) / secpertrk;
			sector = blknum % secpertrk;
#ifdef	WDDEBUG
			printf("new = %d\n", blknum);
#endif
			break;
		}
	}

	wdtab[ctrlr].b_active = 1;		/* mark controller active */

	/* if starting a multisector transfer, or doing single transfers */
	if (du->dk_skip == 0 || (du->dk_flags & DKFL_SINGLE)) {
		int command;
		u_int count;

		if (wdtab[ctrlr].b_errcnt && (bp->b_flags & B_READ) == 0)
			du->dk_bc += DEV_BSIZE;

#ifdef	B_FORMAT
		if (bp->b_flags & B_FORMAT) {
			command = WDCC_FORMAT;
			count = lp->d_nsectors;
			sector = lp->d_gap3 - 1;	/* + 1 later */
		} else
#endif
		{
		if (du->dk_flags & DKFL_SINGLE)
			count = 1;
		else
			count = howmany(du->dk_bc, DEV_BSIZE);
		command = (bp->b_flags & B_READ) ? WDCC_READ : WDCC_WRITE;
		}

		/*
		 * XXX this loop may never terminate.  The code to handle
		 * counting down of retries and eventually failing the i/o is
		 * in wdintr() and we can't get there from here.
		 */
		if (wdtest != 0) {
			if (--wdtest == 0) {
				wdtest = 100;
				printf("dummy wdunwedge\n");
				wdunwedge(du);
			}
		}
		while (wdcommand(du, cylin, head, sector, count, command) != 0)
		{
			wderror(bp, du,
				"wdstart: timeout waiting to send command");
			wdunwedge(du);
		}
#ifdef	WDDEBUG
		printf("sector %d cylin %d head %d addr %x sts %x\n",
	    		sector, cylin, head, addr, inb(wdc+wd_altsts));
#endif
	}

	/* if this is a read operation, just go away until it's done.	*/
	if (bp->b_flags & B_READ) return;

	/* ready to send data?	*/
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ) != 0) {
		wderror(bp, du, "wdstart: timeout waiting for DRQ");
		/*
		 * XXX what do we do now?  If we've just issued the command,
		 * then we can treat this failure the same as a command
		 * failure.  But if we are continuing a multi-sector write,
		 * the command was issued ages ago, so we can't simply
		 * restart it.
		 *
		 * XXX we waste a lot of time unnecessarily translating
		 * block numbers to cylin/head/sector for continued i/o's.
		 */
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
wdintr(int unit)
{
	register struct	disk *du;
	register struct buf *bp, *dp;
	int wdc;

	if (!wdtab[unit].b_active) {
		printf("wdc%d: extra interrupt\n", unit);
		return;
	}

	dp = wdtab[unit].b_actf;
	bp = dp->b_actf;
	du = wddrives[wdunit(bp->b_dev)];
	wdc = du->dk_port;

	if (wdwait(du, 0) < 0) {
		wderror(bp, du, "wdintr: timeout waiting for status");
		du->dk_status |= WDCS_ERR;	/* XXX */
	}

	/* is it not a transfer, but a control operation? */
	if (du->dk_state < OPEN) {
		wdtab[unit].b_active = 0;
		if (wdcontrol(bp))
			wdstart(unit);
		return;
	}

	/* have we an error? */
	if (du->dk_status & (WDCS_ERR | WDCS_ECCCOR)) {
oops:
#ifdef	WDDEBUG
		wderror(bp, du, "wdintr");
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
		if (du->dk_status & WDCS_ERR) {
			if (++wdtab[unit].b_errcnt < RETRIES) {
				wdtab[unit].b_active = 0;
			} else {
				wderror(bp, du, "hard error");
				bp->b_error = EIO;	/* 17 Sep 92*/
				bp->b_flags |= B_ERROR;	/* flag the error */
			}
		} else
			wderror(bp, du, "soft ecc");
	}

	/*
	 * If this was a successful read operation, fetch the data.
	 */
	if (((bp->b_flags & (B_READ | B_ERROR)) == B_READ) && wdtab[unit].b_active) {
		int chk, dummy;

		chk = min(DEV_BSIZE / sizeof(short), du->dk_bc / sizeof(short));

		/* ready to receive data? */
		if ((du->dk_status & (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
		    != (WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ))
			wderror(bp, du, "wdintr: read intr arrived early");
		if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ) != 0) {
			wderror(bp, du, "wdintr: read error detected late");
			goto oops;
		}

		/* suck in data */
		insw (wdc+wd_data,
			(int)bp->b_un.b_addr + du->dk_skip * DEV_BSIZE, chk);
		du->dk_bc -= chk * sizeof(short);

		/* for obselete fractional sector reads */
		while (chk++ < 256) insw (wdc+wd_data, &dummy, 1);
	}

	wdxfer[du->dk_lunit]++;
outt:
	if (wdtab[unit].b_active) {
		if ((bp->b_flags & B_ERROR) == 0) {
			du->dk_skip++;		/* Add to successful sectors. */
			if (wdtab[unit].b_errcnt)
				wderror(bp, du, "soft error");
			wdtab[unit].b_errcnt = 0;

			/* see if more to transfer */
			if (du->dk_bc > 0 && (du->dk_flags & DKFL_ERROR) == 0) {
				wdtab[unit].b_active = 0;
				wdstart(unit);
				return;		/* next chunk is started */
			} else if ((du->dk_flags & (DKFL_SINGLE|DKFL_ERROR))
					== DKFL_ERROR) {
				du->dk_skip = 0;
				du->dk_flags &= ~DKFL_ERROR;
				du->dk_flags |=  DKFL_SINGLE;
				wdtab[unit].b_active = 0;
				wdstart(unit);
				return;		/* redo xfer sector by sector */
			}
		}

#ifdef B_FORMAT
done: ;
#endif
		/* done with this transfer, with or without error */
		du->dk_flags &= ~DKFL_SINGLE;
		wdtab[unit].b_actf = dp->b_forw;
		wdtab[unit].b_errcnt = 0;
		du->dk_skip = 0;
		dp->b_active = 0;
		dp->b_actf = bp->av_forw;
		dp->b_errcnt = 0;
		bp->b_resid = 0;
		biodone(bp);
	}

	/* controller idle */
	wdtab[unit].b_active = 0;

	/* anything more on drive queue? */
	if (dp->b_actf)
		wdustart(du);
	/* anything more for controller to do? */
	if (wdtab[unit].b_actf)
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
	int part = wdpart(dev), mask = 1 << part;
	struct partition *pp;
	char *msg;
	struct disklabel save_label;

	lunit = wdunit(dev);
	if (lunit >= NWD) return (ENXIO) ;

	du = wddrives[lunit];
	if (du == 0) return (ENXIO) ;

	while (du->dk_flags & DKFL_LABELLING)
		tsleep((caddr_t)&du->dk_flags, PZERO - 1, "wdopen", 1);
	if ((du->dk_flags & DKFL_BSDLABEL) == 0) {
		/*
		 * wdtab[ctrlr].b_active != 0 implies wdutab[lunit].b_actf == NULL (?)
		 * so the following guards most things (until the next i/o).
		 * It doesn't guard against a new i/o starting and being
		 * affected by the label being changed.  Sigh.
		 */
		wdsleep(du->dk_ctrlr, "wdopn1");

		du->dk_flags |= DKFL_LABELLING | DKFL_WRITEPROT;
		du->dk_state = WANTOPEN;
		wdutab[lunit].b_actf = NULL;

		/*
		 * Read label using WDRAW partition.
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
		 */
		save_label = du->dk_dd;
#define WDSTRATEGY	((void (*)(struct buf *)) wdstrategy)	/* XXX */
		msg = readdisklabel(makewddev(major(dev), lunit, WDRAW),
				    WDSTRATEGY, &du->dk_dd,
				    du->dk_dospartitions, &du->dk_bad,
				    (struct buf **)NULL);
		du->dk_flags &= ~DKFL_LABELLING;
		if (msg != NULL) {
			du->dk_dd = save_label;
			log(LOG_WARNING, "wd%d: cannot find label (%s)\n",
			    lunit, msg);
			if (part != WDRAW)
				return (EINVAL);  /* XXX needs translation */
		} else {
			du->dk_flags |= DKFL_BSDLABEL;
			du->dk_flags &= ~DKFL_WRITEPROT;
			if (du->dk_dd.d_flags & D_BADSECT)
				du->dk_flags |= DKFL_BADSECT;
		}

		/* Pick up changes made by readdisklabel(). */
		wdsleep(du->dk_ctrlr, "wdopn2");
		du->dk_state = WANTOPEN;
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
			if (pp - du->dk_dd.d_partitions == WDRAW)
				continue;
			if (du->dk_openpart & (1 << (pp -
			    du->dk_dd.d_partitions)))
				log(LOG_WARNING,
				    "wd%d%c: overlaps open partition (%c)\n",
				    lunit, part + 'a',
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
	int ctrlr;

	du = wddrives[wdunit(bp->b_dev)];
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
		return(0);
	case RECAL:
		if (du->dk_status & WDCS_ERR || wdsetctlr(du) != 0) {
			wderror(bp, du, "wdcontrol: recal failed");
maybe_retry:
			if (du->dk_status & WDCS_ERR)
				wdunwedge(du);
			if (++wdtab[ctrlr].b_errcnt < RETRIES) {
				du->dk_state = WANTOPEN;
				goto tryagainrecal;
			}
			bp->b_error = ENXIO;	/* XXX needs translation */
			bp->b_flags |= B_ERROR;
			return(1);
		}
		wdtab[ctrlr].b_errcnt = 0;
		du->dk_state = OPEN;
		/*
		 * The rest of the initialization can be done
		 * by normal means.
		 */
		return(1);
	}
	panic("wdcontrol");
	return (1);
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
	u_int wdc;

	if (wdwait(du, 0) < 0)
		return (1);
	wdc = du->dk_port;
	outb(wdc + wd_precomp, du->dk_dd.d_precompcyl / 4);
	outb(wdc + wd_cyl_lo, cylinder);
	outb(wdc + wd_cyl_hi, cylinder >> 8);
	outb(wdc + wd_sdh, WDSD_IBM | (du->dk_unit << 4) | (head & 0x0f));
	DELAY(10);			/* XXX give drive time to see change */
	if((command != WDCC_DIAGNOSE) && (command != WDCC_IDC)) {
		if (wdwait(du, WDCS_READY) < 0) {
			return(1);
		}
	}
	else {
		if (wdwait(du, 0) < 0) {
			return(1);
		}
	}
	outb(wdc + wd_sector, sector + 1);
	outb(wdc + wd_seccnt, count);
	outb(du->dk_port + wd_command, command);
	return (0);
}

/*
 * issue IDC to drive to tell it just what geometry it is to be.
 */
static int
wdsetctlr(struct disk *du)
{
#ifdef	WDDEBUG
	printf("wd(%d,%d): wdsetctlr C %lu H %lu S %lu\n", du->dk_ctrlr, du->dk_unit,
		du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks,
		du->dk_dd.d_nsectors);
#endif
	if (wdcommand(du, du->dk_dd.d_ncylinders, du->dk_dd.d_ntracks - 1, 0,
		      du->dk_dd.d_nsectors, WDCC_IDC) != 0
	    || wdwait(du, WDCS_READY) != 0) {
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
	int stat;
	int x;

	wdsleep(du->dk_ctrlr, "wdwset");
	x = splbio();
	stat = wdsetctlr(du);
	splx(x);
	return (stat);
}

/*
 * issue READP to drive to ask it what it is.
 */
static int
wdgetctlr(struct disk *du) {
	int i;
	char tb[DEV_BSIZE];
	struct wdparams *wp;

	if (wdcommand(du, 0, 0, 0, 0, WDCC_READP) != 0
	    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT | WDCS_DRQ) != 0) {

#ifndef	MFM
		/* Old drives don't support WDCC_READP.  Try a seek to 0. */
		if (wdcommand(du, 0, 0, 0, 0, WDCC_RESTORE | WD_STEP) != 0
		    || wdwait(du, WDCS_READY | WDCS_SEEKCMPLT) != 0)
			return (1);
#else	/* IDE */
		/*
		 * Some IDE drives return trash if there is not a unit 1
		 * out there, need to make sure that it is READY and not BUSY
		 * before you assume it is there !
		 */
		outb(du->dk_port+wd_sdh, WDSD_IBM | (du->dk_unit<<4));
		DELAY(5000);
		if ((inb(du->dk_port+wd_status) & (WDCS_READY|WDCS_BUSY)) !=
			WDCS_READY) {
			return (1);
		}
#endif	/* MFM */

		/* Fake minimal drive geometry for reading the MBR or label. */
		du->dk_dd.d_secsize = DEV_BSIZE;
		du->dk_dd.d_nsectors = 17;
		du->dk_dd.d_ntracks = 1;
		du->dk_dd.d_ncylinders = 1;
		du->dk_dd.d_secpercyl = 17;

		/*
		 * Fake some more of the label for printing by disklabel(1)
		 * in case there is no real label.
		 */
		du->dk_dd.d_type = DTYPE_ST506;
		du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;
		strncpy(du->dk_dd.d_typename, "Fake geometry",
			sizeof du->dk_dd.d_typename);

		/* Fake the model name for printing by wdattach(). */
		strncpy(du->dk_params.wdp_model, "Unknown Type",
			sizeof du->dk_params.wdp_model);

		return (0);
	}

	/* obtain parameters */
	wp = &du->dk_params;
	insw(du->dk_port + wd_data, tb, sizeof(tb)/sizeof(short));
	bcopy(tb, wp, sizeof(struct wdparams));

	/* shuffle string byte order */
	for (i=0; i < sizeof(wp->wdp_model) ;i+=2) {
		u_short *p;
		p = (u_short *) (wp->wdp_model + i);
		*p = ntohs(*p);
	}
#ifdef	WDDEBUG
	printf(
    "\nwdgetctlr wd(%d,%d): gc %x cyl %d trk %d sec %d type %d sz %d model %s\n",
		du->dk_ctrlr, du->dk_unit, wp->wdp_config,
		wp->wdp_fixedcyl + wp->wdp_removcyl, wp->wdp_heads,
		wp->wdp_sectors, wp->wdp_cntype, wp->wdp_cnsbsz,
		wp->wdp_model);
#endif

	/* update disklabel given drive information */
	du->dk_dd.d_secsize = DEV_BSIZE;
	du->dk_dd.d_ncylinders = wp->wdp_fixedcyl + wp->wdp_removcyl /*+- 1*/;
	du->dk_dd.d_ntracks = wp->wdp_heads;
	du->dk_dd.d_nsectors = wp->wdp_sectors;
	du->dk_dd.d_secpercyl = du->dk_dd.d_ntracks * du->dk_dd.d_nsectors;
	/* dubious ... */
	bcopy("ESDI/IDE", du->dk_dd.d_typename, 9);
	bcopy(wp->wdp_model+20, du->dk_dd.d_packname, 14-1);
	/* better ... */
	du->dk_dd.d_type = DTYPE_ESDI;
	du->dk_dd.d_subtype |= DSTYPE_GEOMETRY;
	
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
	int lunit = wdunit(dev);
	register struct disk *du;
	int error = 0;
#ifdef notyet
	struct uio auio;
	struct iovec aiov;
#endif

	du = wddrives[lunit];

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
			/* du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart : */0,
				du->dk_dospartitions);
		if (error == 0) {
			du->dk_flags |= DKFL_BSDLABEL;
			wdwsetctlr(du);	/* XXX - check */
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
			        /*du->dk_flags & DKFL_BSDLABEL) ? du->dk_openpart :*/ 0,
				du->dk_dospartitions)) == 0) {
			int wlab;

			du->dk_flags |= DKFL_BSDLABEL;
			wdwsetctlr(du);			/* XXX - check */

			/* simulate opening partition 0 so write succeeds */
			du->dk_openpart |= (1 << 0);	/* XXX */
			wlab = du->dk_wlabel;
			du->dk_wlabel = 1;
			error = writedisklabel(dev, WDSTRATEGY,
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
			error = physio(wdformat, &rwdbuf[lunit], 0, dev, B_WRITE,
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
	int lunit = wdunit(dev), part = wdpart(dev), val;
	struct disk *du;

	if (lunit >= NWD)	/* 31 Jul 92*/
		return(-1);

	du = wddrives[lunit];
	val = 0;
	if (du == 0 || du->dk_state == CLOSED)
		val = wdopen (makewddev(major(dev), lunit, WDRAW), FREAD, S_IFBLK, 0);
	if (du == 0 || val != 0 || du->dk_flags & DKFL_WRITEPROT)
		return (-1);

	return((int)du->dk_dd.d_partitions[part].p_size);
}

extern char	*vmmap;			/* poor name! */

int
wddump(dev_t dev)			/* dump core after a system crash */
{
	register struct disk *du;	/* disk unit to do the IO */
#ifdef notyet
	register struct bt_bad *bt_ptr;
#endif
	long	num;			/* number of sectors to write */
	int	ctrlr, lunit, part, wdc;
	long	blkoff, blknum;
#ifdef notdef
	long	blkcnt;
#endif
	long	cylin, head, sector;
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
	lunit = wdunit(dev);		/* eventually support floppies? */
	part = wdpart(dev);		/* file system */
	/* check for acceptable drive number */
	if (lunit >= NWD) return(ENXIO);		/* 31 Jul 92*/

	du = wddrives[lunit];
	if (du == 0) return(ENXIO);
	/* was it ever initialized ? */
	if (du->dk_state < OPEN) return (ENXIO) ;
	if (du->dk_flags & DKFL_WRITEPROT) return(ENXIO);
	wdc = du->dk_port;
	ctrlr = du->dk_ctrlr;

	/* Convert to disk sectors */
	num = (u_long) num * NBPG / du->dk_dd.d_secsize;

	/* check if controller active */
	/*if (wdtab[ctrlr].b_active) return(EFAULT); */
	if (wddoingadump) return(EFAULT);

	secpertrk = du->dk_dd.d_nsectors;
	secpercyl = du->dk_dd.d_secpercyl;
	nblocks = du->dk_dd.d_partitions[part].p_size;
	blkoff = du->dk_dd.d_partitions[part].p_offset;

/*pg("xunit %x, nblocks %d, dumplo %d num %d\n", part,nblocks,dumplo,num);*/
	/* check transfer bounds against partition size */
	if ((dumplo < 0) || ((dumplo + num) > nblocks))
		return(EINVAL);

#if 0
	wdtab[ctrlr].b_active = 1;	/* mark controller active for if we */
					/* panic during the dump */
#endif
	wddoingadump = 1  ;
	i = 100000;			/* WHY NOT TIMEOUT */
	/* must delay 5us to conform to ATA spec */
	DELAY(5);
	while ((inb(wdc+wd_status) & WDCS_BUSY) && (i-- > 0)) ;
	outb(wdc+wd_sdh, WDSD_IBM | (du->dk_unit << 4));
	outb(wdc+wd_command, WDCC_RESTORE | WD_STEP);
	/* must delay 5us to conform to ATA spec */
	DELAY(5);
	while (inb(wdc+wd_status) & WDCS_BUSY) ;

	/* some compaq controllers require this ... */
	wdsetctlr(du);

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
				/* XXX as usual */
				if (lp->d_partitions[BSD_PART].p_offset != 0)
					blknum = lp->d_partitions[BAD144_PART]
						     .p_offset
						 + lp->d_partitions[BAD144_PART]
						       .p_size;
				else
					blknum = lp->d_secperunit;
				blknum -= du->dk_dd.d_nsectors
					  + (bt_ptr - du->dk_bad.bt_bad) + 1;
				cylin = blknum / secpercyl;
				head = (blknum % secpercyl) / secpertrk;
				sector = blknum % secpertrk;
				break;
			}
		}
#endif
		sector++;		/* origin 1 */

		/* select drive. */
		outb(wdc+wd_sdh, WDSD_IBM | (du->dk_unit<<4) | (head & 0xf));
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
		/* Check data request (should be done) */
		if (inb(wdc+wd_status) & WDCS_DRQ) return(EIO) ;

		/* wait for completion */
		/* must delay 5us to conform to ATA spec */
		DELAY(5);
		for ( i = WDCTIMEOUT ; inb(wdc+wd_status) & WDCS_BUSY ; i--) {
				if (i < 0) return (EIO) ;
		}
		/* error check the xfer */
		if (inb(wdc+wd_status) & WDCS_ERR) return(EIO) ;

		if ((unsigned)addr % (1024*1024) == 0) printf("%ld ", num/2048);
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

static void
wderror(struct buf *bp, struct disk *du, char *mesg)
{
	if (bp == NULL)
		printf("wd%d: %s:", du->dk_lunit, mesg);
	else
		diskerr(bp, "wd", mesg, LOG_PRINTF, du->dk_skip, &du->dk_dd);
	printf(" status %b error %b\n",
		du->dk_status, WDCS_BITS, du->dk_error, WDERR_BITS);
}

/*
 * Reset the controller.
 */
static int
wdreset(struct disk *du)
{
	int wdc;

	wdc = du->dk_port;
	(void)wdwait(du, 0);
	outb(wdc + wd_ctlr, WDCTL_IDS | WDCTL_RST);
	DELAY(10 * 1000);
	outb(wdc + wd_ctlr, WDCTL_IDS);
	if (wdwait(du, WDCS_READY | WDCS_SEEKCMPLT) != 0
	    || (du->dk_error = inb(wdc + wd_error)) != 0x01)
		return (1);
	outb(wdc + wd_ctlr, WDCTL_4BIT);
	return (0);
}

/*
 * Sleep until driver is inactive.
 * This is used only for avoiding rare race conditions, so it is unimportant
 * that the sleep may be far too short or too long.
 */
static void
wdsleep(int ctrlr, char *wmesg)
{
	while (wdtab[ctrlr].b_active)
		tsleep((caddr_t)&wdtab[ctrlr].b_active, PZERO - 1, wmesg, 1);
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
	int lunit;

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
		    && wdwait(du, WDCS_READY | WDCS_SEEKCMPLT) == 0
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
static int min_retries[NWDC];
static int
wdwait(struct disk *du, u_char bits_wanted)
{
	int retries;
	int wdc;
	int ctrlr = du->dk_ctrlr;
	u_char status;
#define	POLLING		1000
#define	TIMEOUT		2000	/* WDCC_DIAGNOSE can take > 300 msec */

	wdc = du->dk_port;
	retries = POLLING + TIMEOUT;
	do {
		if (min_retries[ctrlr] > retries || min_retries[ctrlr] == 0)
			min_retries[ctrlr] = retries;
		/* must delay 5us to conform to ATA spec */
		DELAY(5);
		du->dk_status = status = inb(wdc + wd_status);
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
		if (retries < TIMEOUT)
			/*
			 * Switch to a polling rate of about 1 KHz so that
			 * the timeout is almost machine-independent.  The
			 * controller is taking a long time to respond, so
			 * an extra msec won't matter.
			 */
			DELAY(1000);
	} while (--retries != 0);
	return (-1);
}

#endif	/* NWDC > 0 */
