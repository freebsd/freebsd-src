/*#define DEBUG 1*/
/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Portions Copyright (c) 1993, 1994 by
 *  jc@irbs.UUCP (John Capo)
 *  vak@zebub.msk.su (Serge Vakulenko)
 *  ache@astral.msk.su (Andrew A. Chernov)
 *  joerg_wunsch@uriah.sax.de (Joerg Wunsch)
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
 *	from:	@(#)fd.c	7.4 (Berkeley) 5/25/91
 *	$Id: fd.c,v 1.26 1994/05/22 12:30:32 joerg Exp $
 *
 */

#include "ft.h"
#if NFT < 1
#undef NFDC
#endif
#include "fd.h"

#if NFDC > 0

#include <sys/param.h>
#include <sys/dkbad.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/ioctl_fd.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/fdreg.h"
#include "i386/isa/fdc.h"
#include "i386/isa/rtc.h"

#define b_cylin b_resid

/* misuse a flag to identify format operation */
#define B_FORMAT B_XXX

/*
 * this biotab field doubles as a field for the physical unit number
 * on the controller
 */
#define id_physid id_scsiid

#define NUMTYPES 14
#define NUMDENS  (NUMTYPES - 6)

/* These defines (-1) must match index for fd_types */
#define F_TAPE_TYPE	0x020	/* bit for fd_types to indicate tape */
#define NO_TYPE		0	/* must match NO_TYPE in ft.c */
#define FD_1720         1
#define FD_1480         2
#define FD_1440         3
#define FD_1200         4
#define FD_820          5
#define FD_800          6
#define FD_720          7
#define FD_360          8

#define FD_1480in5_25   9
#define FD_1440in5_25   10
#define FD_820in5_25    11
#define FD_800in5_25    12
#define FD_720in5_25    13
#define FD_360in5_25    14


struct fd_type fd_types[NUMTYPES] =
{
{ 21,2,0xFF,0x04,82,3444,1,FDC_500KBPS,2,0x0C,2 }, /* 1.72M in HD 3.5in */
{ 18,2,0xFF,0x1B,82,2952,1,FDC_500KBPS,2,0x6C,1 }, /* 1.48M in HD 3.5in */
{ 18,2,0xFF,0x1B,80,2880,1,FDC_500KBPS,2,0x6C,1 }, /* 1.44M in HD 3.5in */
{ 15,2,0xFF,0x1B,80,2400,1,FDC_500KBPS,2,0x54,1 }, /*  1.2M in HD 5.25/3.5 */
{ 10,2,0xFF,0x10,82,1640,1,FDC_250KBPS,2,0x2E,1 }, /*  820K in HD 3.5in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_250KBPS,2,0x2E,1 }, /*  800K in HD 3.5in */
{  9,2,0xFF,0x20,80,1440,1,FDC_250KBPS,2,0x50,1 }, /*  720K in HD 3.5in */
{  9,2,0xFF,0x2A,40, 720,1,FDC_250KBPS,2,0x50,1 }, /*  360K in DD 5.25in */

{ 18,2,0xFF,0x02,82,2952,1,FDC_500KBPS,2,0x02,2 }, /* 1.48M in HD 5.25in */
{ 18,2,0xFF,0x02,80,2880,1,FDC_500KBPS,2,0x02,2 }, /* 1.44M in HD 5.25in */
{ 10,2,0xFF,0x10,82,1640,1,FDC_300KBPS,2,0x2E,1 }, /*  820K in HD 5.25in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_300KBPS,2,0x2E,1 }, /*  800K in HD 5.25in */
{  9,2,0xFF,0x20,80,1440,1,FDC_300KBPS,2,0x50,1 }, /*  720K in HD 5.25in */
{  9,2,0xFF,0x23,40, 720,2,FDC_300KBPS,2,0x50,1 }, /*  360K in HD 5.25in */
};

#define DRVS_PER_CTLR 2		/* 2 floppies */
/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
struct fdc_data fdc_data[NFDC];

/***********************************************************************\
* Per drive structure.							*
* N per controller  (DRVS_PER_CTLR)					*
\***********************************************************************/
struct fd_data {
	struct	fdc_data *fdc;	/* pointer to controller structure */
	int	fdsu;		/* this units number on this controller */
	int	type;		/* Drive type (FD_1440...) */
	struct	fd_type *ft;	/* pointer to the type descriptor */
	int	flags;
#define	FD_OPEN		0x01	/* it's open		*/
#define	FD_ACTIVE	0x02	/* it's active		*/
#define	FD_MOTOR	0x04	/* motor should be on	*/
#define	FD_MOTOR_WAIT	0x08	/* motor coming up	*/
	int	skip;
	int	hddrv;
	int	track;		/* where we think the head is */
	int	options;	/* user configurable options, see ioctl_fd.h */
} fd_data[NFD];

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/

#if NFT > 0
int ftopen(dev_t, int);
int ftintr(/* ftu_t */ int ftu);
int ftclose(/* dev_t */ int, int);
void ftstrategy(struct buf *);
int ftioctl(/* dev_t */ int, int, caddr_t, int, struct proc *);
int ftdump(/* dev_t */ int);
int ftsize(/* dev_t */ int);
int ftattach(struct isa_device *, struct isa_device *);
#endif

/* autoconfig functions */
static int fdprobe(struct isa_device *);
static int fdattach(struct isa_device *);

/* exported functions */
int fdsize (/* dev_t */ int);
void fdintr(fdcu_t);
int Fdopen(/* dev_t */int, int);
int fdclose(/* dev_t */int, int);
void fdstrategy(struct buf *);
int fdioctl(/* dev_t */ int, int, caddr_t, int, struct proc *);

/* needed for ft driver, thus exported */
int in_fdc(fdcu_t);
int out_fdc(fdcu_t, int);

/* internal functions */
static void set_motor(fdcu_t, int, int);
#  define TURNON 1
#  define TURNOFF 0
static void fd_turnoff(caddr_t arg1, int arg2);
static void fd_motor_on(caddr_t arg1, int arg2);
static void fd_turnon(fdu_t);
static void fdc_reset(fdc_p);
static void fdstart(fdcu_t);
static void fd_timeout(caddr_t, int);
static void fd_pseudointr(caddr_t, int);
static int fdstate(fdcu_t, fdc_p);
static int retrier(fdcu_t);
static int fdformat(/* dev_t */ int, struct fd_formb *, struct proc *);


#define DEVIDLE		0
#define FINDWORK	1
#define	DOSEEK		2
#define SEEKCOMPLETE 	3
#define	IOCOMPLETE	4
#define RECALCOMPLETE	5
#define	STARTRECAL	6
#define	RESETCTLR	7
#define	SEEKWAIT	8
#define	RECALWAIT	9
#define	MOTORWAIT	10
#define	IOTIMEDOUT	11

#ifdef	DEBUG
char *fdstates[] =
{
"DEVIDLE",
"FINDWORK",
"DOSEEK",
"SEEKCOMPLETE",
"IOCOMPLETE",
"RECALCOMPLETE",
"STARTRECAL",
"RESETCTLR",
"SEEKWAIT",
"RECALWAIT",
"MOTORWAIT",
"IOTIMEDOUT"
};

/* CAUTION: fd_debug causes huge amounts of logging output */
int	fd_debug = 0;
#define TRACE0(arg) if(fd_debug) printf(arg)
#define TRACE1(arg1, arg2) if(fd_debug) printf(arg1, arg2)
#else /* DEBUG */
#define TRACE0(arg)
#define TRACE1(arg1, arg2)
#endif /* DEBUG */

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/
struct	isa_driver fdcdriver = {
	fdprobe, fdattach, "fdc",
};

/*
 * probe for existance of controller
 */
static int
fdprobe(dev)
	struct isa_device *dev;
{
	fdcu_t	fdcu = dev->id_unit;
	if(fdc_data[fdcu].flags & FDC_ATTACHED)
	{
		printf("fdc: same unit (%d) used multiple times\n", fdcu);
		return 0;
	}

	fdc_data[fdcu].baseport = dev->id_iobase;

	/* First - lets reset the floppy controller */
	outb(dev->id_iobase+FDOUT, 0);
	DELAY(100);
	outb(dev->id_iobase+FDOUT, FDO_FRST);

	/* see if it can handle a command */
	if (out_fdc(fdcu, NE7CMD_SPECIFY) < 0)
	{
		return(0);
	}
	out_fdc(fdcu, NE7_SPEC_1(3, 240));
	out_fdc(fdcu, NE7_SPEC_2(2, 0));
	return (IO_FDCSIZE);
}

/*
 * wire controller into system, look for floppy units
 */
static int
fdattach(dev)
	struct isa_device *dev;
{
	unsigned fdt;
	fdu_t	fdu;
	fdcu_t	fdcu = dev->id_unit;
	fdc_p	fdc = fdc_data + fdcu;
	fd_p	fd;
	int	fdsu, st0;
	struct isa_device *fdup;

	fdc->fdcu = fdcu;
	fdc->flags |= FDC_ATTACHED;
	fdc->dmachan = dev->id_drq;
	fdc->state = DEVIDLE;
	/* reset controller, turn motor off, clear fdout mirror reg */
	outb(fdc->baseport + FDOUT, ((fdc->fdout = 0)));
	printf("fdc%d:", fdcu);

	/* check for each floppy drive */
	for (fdup = isa_biotab_fdc; fdup->id_driver != 0; fdup++) {
		if (fdup->id_iobase != dev->id_iobase)
			continue;
		fdu = fdup->id_unit;
		fd = &fd_data[fdu];
		if (fdu >= (NFD+NFT))
			continue;
		fdsu = fdup->id_physid;
				/* look up what bios thinks we have */
		switch (fdu) {
			case 0: fdt = (rtcin(RTC_FDISKETTE) & 0xf0);
				break;
			case 1: fdt = ((rtcin(RTC_FDISKETTE) << 4) & 0xf0);
				break;
			default: fdt = RTCFDT_NONE;
				break;
		}
		/* is there a unit? */
		if ((fdt == RTCFDT_NONE)
#if NFT > 0
		|| (fdsu >= DRVS_PER_CTLR)) {
#else
		) {
			fd->type = NO_TYPE;
#endif
#if NFT > 0
				/* If BIOS says no floppy, or > 2nd device */
				/* Probe for and attach a floppy tape.     */
			if (ftattach(dev, fdup))
				continue;
			if (fdsu < DRVS_PER_CTLR) 
				fd->type = NO_TYPE;
#endif
			continue;
		}

		/* select it */
		set_motor(fdcu, fdsu, TURNON);
		spinwait(1000);	/* 1 sec */
		out_fdc(fdcu, NE7CMD_SEEK);	/* seek some steps... */
		out_fdc(fdcu, fdsu);
		out_fdc(fdcu, 10);
		spinwait(300);			/* ...wait a moment... */
		out_fdc(fdcu, NE7CMD_SENSEI);	/* make controller happy */
		(void)in_fdc(fdcu);
		(void)in_fdc(fdcu);
		out_fdc(fdcu, NE7CMD_RECAL);	/* ...and go back to 0 */
		out_fdc(fdcu, fdsu);
		spinwait(1000);	/* a second be enough for full stroke seek */

		/* anything responding */
		out_fdc(fdcu, NE7CMD_SENSEI);
		st0 = in_fdc(fdcu);
		(void)in_fdc(fdcu);
		set_motor(fdcu, fdsu, TURNOFF);
		
		if (st0 & NE7_ST0_EC) /* no track 0 -> no drive present */
			continue;

		fd->track = -2;
		fd->fdc = fdc;
		fd->fdsu = fdsu;
		fd->options = 0;
		printf(" [%d: fd%d: ", fdsu, fdu);
		
		switch (fdt) {
		case RTCFDT_12M:
			printf("1.2MB 5.25in]");
			fd->type = FD_1200;
			break;
		case RTCFDT_144M:
			printf("1.44MB 3.5in]");
			fd->type = FD_1440;
			break;
		case RTCFDT_360K:
			printf("360KB 5.25in]");
			fd->type = FD_360;
			break;
		case RTCFDT_720K:
			printf("720KB 3.5in]");
			fd->type = FD_720;
			break;
		default:
			printf("unknown]");
			fd->type = NO_TYPE;
			break;
		}
	}
	printf("\n");

	return (1);
}

int
fdsize(dev)
	dev_t	dev;
{
	return(0);
}

/****************************************************************************/
/*                            motor control stuff                           */
/*		remember to not deselect the drive we're working on         */
/****************************************************************************/
static void
set_motor(fdcu, fdsu, turnon)
	fdcu_t fdcu;
	int fdsu;
	int turnon;
{
	int fdout = fdc_data[fdcu].fdout;
	int needspecify = 0;
	
	if(turnon) {
		fdout &= ~FDO_FDSEL;		
		fdout |= (FDO_MOEN0 << fdsu) + fdsu;
	} else
		fdout &= ~(FDO_MOEN0 << fdsu);

	if(!turnon
	   && (fdout & (FDO_MOEN0+FDO_MOEN1+FDO_MOEN2+FDO_MOEN3)) == 0)
		/* gonna turn off the last drive, put FDC to bed */
		fdout &= ~ (FDO_FRST|FDO_FDMAEN);
	else {
		/* make sure controller is selected and specified */
		if((fdout & (FDO_FRST|FDO_FDMAEN)) == 0)
			needspecify = 1;
		fdout |= (FDO_FRST|FDO_FDMAEN);
	}

	outb(fdc_data[fdcu].baseport+FDOUT, fdout);
	fdc_data[fdcu].fdout = fdout;
	TRACE1("[0x%x->FDOUT]", fdout);

	if(needspecify) {
		out_fdc(fdcu, NE7CMD_SPECIFY);
		out_fdc(fdcu, NE7_SPEC_1(3, 240));
		out_fdc(fdcu, NE7_SPEC_2(2, 0));
	}
}

/* ARGSUSED */
static void
fd_turnoff(caddr_t arg1, int arg2)
{
	fdu_t fdu = (fdu_t)arg1;
	int	s;

	fd_p fd = fd_data + fdu;
	s = splbio();
	fd->flags &= ~FD_MOTOR;
	set_motor(fd->fdc->fdcu, fd->fdsu, TURNOFF);
	splx(s);
}

/* ARGSUSED */
static void
fd_motor_on(caddr_t arg1, int arg2)
{
	fdu_t fdu = (fdu_t)arg1;
	int	s;

	fd_p fd = fd_data + fdu;
	s = splbio();
	fd->flags &= ~FD_MOTOR_WAIT;
	if((fd->fdc->fd == fd) && (fd->fdc->state == MOTORWAIT))
	{
		fdintr(fd->fdc->fdcu);
	}
	splx(s);
}

static void
fd_turnon(fdu) 
	fdu_t fdu;
{
	fd_p fd = fd_data + fdu;
	if(!(fd->flags & FD_MOTOR))
	{
		fd->flags |= (FD_MOTOR + FD_MOTOR_WAIT);
		set_motor(fd->fdc->fdcu, fd->fdsu, TURNON);
		timeout(fd_motor_on, (caddr_t)fdu, hz); /* in 1 sec its ok */
	}
}

static void
fdc_reset(fdc)
	fdc_p fdc;
{
	fdcu_t fdcu = fdc->fdcu;
	
	/* Try a reset, keep motor on */
	outb(fdc->baseport + FDOUT, fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	DELAY(100);
	/* enable FDC, but defer interrupts a moment */
	outb(fdc->baseport + FDOUT, fdc->fdout & ~FDO_FDMAEN);
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~FDO_FDMAEN);
	DELAY(100);
	outb(fdc->baseport + FDOUT, fdc->fdout);
	TRACE1("[0x%x->FDOUT]", fdc->fdout);

	out_fdc(fdcu, NE7CMD_SPECIFY);
	out_fdc(fdcu, NE7_SPEC_1(3, 240));
	out_fdc(fdcu, NE7_SPEC_2(2, 0));
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc(fdcu)
	fdcu_t fdcu;
{
	int baseport = fdc_data[fdcu].baseport;
	int i, j = 100000;
	while ((i = inb(baseport+FDSTS) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM) return -1;
	if (j <= 0)
		return(-1);
#ifdef	DEBUG
	i = inb(baseport+FDDATA);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	return(i);
#else
	return inb(baseport+FDDATA);
#endif
}

int
out_fdc(fdcu, x)
	fdcu_t fdcu;
	int x;
{
	int baseport = fdc_data[fdcu].baseport;
	int i;

	/* Check that the direction bit is set */
	i = 100000;
	while ((inb(baseport+FDSTS) & NE7_DIO) && i-- > 0);
	if (i <= 0) return (-1);	/* Floppy timed out */

	/* Check that the floppy controller is ready for a command */
	i = 100000;
	while ((inb(baseport+FDSTS) & NE7_RQM) == 0 && i-- > 0);
	if (i <= 0) return (-1);	/* Floppy timed out */

	/* Send the command and return */
	outb(baseport+FDDATA, x);
	TRACE1("[0x%x->FDDATA]", x);
	return (0);
}

/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
int
Fdopen(dev, flags)
	dev_t	dev;
	int	flags;
{
 	fdu_t fdu = FDUNIT(minor(dev));
	int type = FDTYPE(minor(dev));
	fdc_p	fdc;

#if NFT > 0
	/* check for a tape open */
	if (type & F_TAPE_TYPE)
		return(ftopen(dev, flags));
#endif
	/* check bounds */
	if (fdu >= NFD) 
		return(ENXIO);
	fdc = fd_data[fdu].fdc;
	if ((fdc == NULL) || (fd_data[fdu].type == NO_TYPE))
		return(ENXIO);
	if (type > NUMDENS)
		return(ENXIO);
	if (type == 0)
		type = fd_data[fdu].type;
	else {
		if (type != fd_data[fdu].type) {
			switch (fd_data[fdu].type) {
			case FD_360:
				return(ENXIO);
			case FD_720:
				if (   type != FD_820
				    && type != FD_800
				   )
					return(ENXIO);
				break;
			case FD_1200:
				switch (type) {
				case FD_1480:
					type = FD_1480in5_25;
					break;
				case FD_1440:
					type = FD_1440in5_25;
					break;
				case FD_820:
					type = FD_820in5_25;
					break;
				case FD_800:
					type = FD_800in5_25;
					break;
				case FD_720:
					type = FD_720in5_25;
					break;
				case FD_360:
					type = FD_360in5_25;
					break;
				default:
					return(ENXIO);
				}
				break;
			case FD_1440:
				if (   type != FD_1720
				    && type != FD_1480
				    && type != FD_1200
				    && type != FD_820
				    && type != FD_800
				    && type != FD_720
				    )
					return(ENXIO);
				break;
			}
		}
	}
	fd_data[fdu].ft = fd_types + type - 1;
	fd_data[fdu].flags |= FD_OPEN;

	return 0;
}

int
fdclose(dev, flags)
	dev_t dev;
	int flags;
{
 	fdu_t fdu = FDUNIT(minor(dev));

#if NFT > 0
	int type = FDTYPE(minor(dev));

	if (type & F_TAPE_TYPE)
		return ftclose(dev, flags);
#endif
	fd_data[fdu].flags &= ~FD_OPEN;
	fd_data[fdu].options &= ~FDOPT_NORETRY;
	return(0);
}


/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
void
fdstrategy(struct buf *bp)
{
	register struct buf *dp;
	long nblocks, blknum;
 	int	s;
 	fdcu_t	fdcu;
 	fdu_t	fdu;
 	fdc_p	fdc;
 	fd_p	fd;
	size_t	fdblk;

 	fdu = FDUNIT(minor(bp->b_dev));
	fd = &fd_data[fdu];
	fdc = fd->fdc;
	fdcu = fdc->fdcu;
	fdblk = 128 << (fd->ft->secsize);

#if NFT > 0
	if (FDTYPE(minor(bp->b_dev)) & F_TAPE_TYPE) {
		/* ft tapes do not (yet) support strategy i/o */
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	/* check for controller already busy with tape */
	if (fdc->flags & FDC_TAPE_BUSY) {
		bp->b_error = EBUSY;
		bp->b_flags |= B_ERROR;
		goto bad; 
	}
#endif
	if (!(bp->b_flags & B_FORMAT)) {
		if ((fdu >= NFD) || (bp->b_blkno < 0)) {
			printf("fdstrat: fdu = %d, blkno = %d, bcount = %d\n",
			       fdu, bp->b_blkno, bp->b_bcount);
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
		if ((bp->b_bcount % fdblk) != 0) {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
	}
	
	/*
	 * Set up block calculations.
	 */
	blknum = (unsigned long) bp->b_blkno * DEV_BSIZE/fdblk;
 	nblocks = fd->ft->size;
	if (blknum + (bp->b_bcount / fdblk) > nblocks) {
		if (blknum == nblocks) {
			bp->b_resid = bp->b_bcount;
		} else {
			bp->b_error = ENOSPC;
			bp->b_flags |= B_ERROR;
		}
		goto bad;
	}
 	bp->b_cylin = blknum / (fd->ft->sectrac * fd->ft->heads);
	dp = &(fdc->head);
	s = splbio();
	disksort(dp, bp);
	untimeout(fd_turnoff, (caddr_t)fdu); /* a good idea */
	fdstart(fdcu);
	splx(s);
	return;

bad:
	biodone(bp);
	return;
}

/***************************************************************\
*				fdstart				*
* We have just queued something.. if the controller is not busy	*
* then simulate the case where it has just finished a command	*
* So that it (the interrupt routine) looks on the queue for more*
* work to do and picks up what we just added.			*
* If the controller is already busy, we need do nothing, as it	*
* will pick up our work when the present work completes		*
\***************************************************************/
static void
fdstart(fdcu)
	fdcu_t fdcu;
{
	int s;

	s = splbio();
	if(fdc_data[fdcu].state == DEVIDLE)
	{
		fdintr(fdcu);
	}
	splx(s);
}

/* ARGSUSED */
static void
fd_timeout(caddr_t arg1, int arg2)
{
	fdcu_t fdcu = (fdcu_t)arg1;
	fdu_t fdu = fdc_data[fdcu].fdu;
	int baseport = fdc_data[fdcu].baseport;
	struct buf *dp, *bp;
	int s;

	dp = &fdc_data[fdcu].head;
	bp = dp->b_actf;

	/*
	 * Due to IBM's brain-dead design, the FDC has a faked ready
	 * signal, hardwired to ready == true. Thus, any command
	 * issued if there's no diskette in the drive will _never_
	 * complete, and must be aborted by resetting the FDC.
	 * Many thanks, Big Blue!
	 */

	s = splbio();

	TRACE1("fd%d[fd_timeout()]", fdu);
	/* See if the controller is still busy (patiently awaiting data) */
	if(((inb(baseport + FDSTS)) & (NE7_CB|NE7_RQM)) == NE7_CB)
	{
		TRACE1("[FDSTS->0x%x]", inb(baseport + FDSTS));
		/* yup, it is; kill it now */
		fdc_reset(&fdc_data[fdcu]);
		printf("fd%d: Operation timeout\n", fdu);
	}

	if (bp)
	{
		retrier(fdcu);
		fdc_data[fdcu].status[0] = NE7_ST0_IC_RC;
		fdc_data[fdcu].state = IOTIMEDOUT;
		if( fdc_data[fdcu].retry < 6)
			fdc_data[fdcu].retry = 6;
	}
	else
	{
		fdc_data[fdcu].fd = (fd_p) 0;
		fdc_data[fdcu].fdu = -1;
		fdc_data[fdcu].state = DEVIDLE;
	}
	fdintr(fdcu);
	splx(s);
}

/* just ensure it has the right spl */
/* ARGSUSED */
static void
fd_pseudointr(caddr_t arg1, int arg2)
{
	fdcu_t fdcu = (fdcu_t)arg1;
	int	s;

	s = splbio();
	fdintr(fdcu);
	splx(s);
}

/***********************************************************************\
*                                 fdintr				*
* keep calling the state machine until it returns a 0			*
* ALWAYS called at SPLBIO 						*
\***********************************************************************/
void
fdintr(fdcu_t fdcu)
{
	fdc_p fdc = fdc_data + fdcu;
#if NFT > 0
	fdu_t fdu = fdc->fdu;

	if (fdc->flags & FDC_TAPE_BUSY)
		(ftintr(fdu));
	else
#endif
		while(fdstate(fdcu, fdc))
			;
}

/***********************************************************************\
* The controller state machine.						*
* if it returns a non zero value, it should be called again immediatly	*
\***********************************************************************/
static int
fdstate(fdcu, fdc)
	fdcu_t fdcu;
	fdc_p fdc;
{
	int read, format, head, sec = 0, i = 0, sectrac, st0, cyl, st3;
	unsigned long blknum;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	register struct buf *dp, *bp;
	struct fd_formb *finfo = NULL;
	size_t fdblk;

	dp = &(fdc->head);
	bp = dp->b_actf;
	if(!bp) 
	{
		/***********************************************\
		* nothing left for this controller to do	*
		* Force into the IDLE state,			*
		\***********************************************/
		fdc->state = DEVIDLE;
		if(fdc->fd)
		{
			printf("unexpected valid fd pointer (fdu = %d)\n",
			       fdc->fdu);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
		}
		TRACE1("[fdc%d IDLE]", fdcu);
 		return(0);
	}
	fdu = FDUNIT(minor(bp->b_dev));
	fd = fd_data + fdu;
	fdblk = 128 << fd->ft->secsize;
	if (fdc->fd && (fd != fdc->fd))
	{
		printf("confused fd pointers\n");
	}
	read = bp->b_flags & B_READ;
	format = bp->b_flags & B_FORMAT;
	if(format)
		finfo = (struct fd_formb *)bp->b_un.b_addr;
	TRACE1("fd%d", fdu);
	TRACE1("[%s]", fdstates[fdc->state]);
	TRACE1("(0x%x)", fd->flags);
	untimeout(fd_turnoff, (caddr_t)fdu);
	timeout(fd_turnoff, (caddr_t)fdu, 4 * hz);
	switch (fdc->state)
	{
	case DEVIDLE:
	case FINDWORK:	/* we have found new work */
		fdc->retry = 0;
		fd->skip = 0;
		fdc->fd = fd;
		fdc->fdu = fdu;
		outb(fdc->baseport+FDCTL, fd->ft->trans);
		TRACE1("[0x%x->FDCTL]", fd->ft->trans);
		/*******************************************************\
		* If the next drive has a motor startup pending, then	*
		* it will start up in it's own good time		*
		\*******************************************************/
		if(fd->flags & FD_MOTOR_WAIT)
		{
			fdc->state = MOTORWAIT;
			return(0); /* come back later */
		}
		/*******************************************************\
		* Maybe if it's not starting, it SHOULD be starting	*
		\*******************************************************/
		if (!(fd->flags & FD_MOTOR))
		{
			fdc->state = MOTORWAIT;
			fd_turnon(fdu);
			return(0);
		}
		else	/* at least make sure we are selected */
		{
			set_motor(fdcu, fd->fdsu, TURNON);
		}
		fdc->state = DOSEEK;
		break;
	case DOSEEK:
		if (bp->b_cylin == fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			break;
		}
		out_fdc(fdcu, NE7CMD_SEEK);	/* Seek function */
		out_fdc(fdcu, fd->fdsu);	/* Drive number */
		out_fdc(fdcu, bp->b_cylin * fd->ft->steptrac);
		fd->track = -2;
		fdc->state = SEEKWAIT;
		return(0);	/* will return later */
	case SEEKWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, (caddr_t)fdcu, hz / 32);
		fdc->state = SEEKCOMPLETE;
		return(0);	/* will return later */
	case SEEKCOMPLETE : /* SEEK DONE, START DMA */
		/* Make sure seek really happened*/
		if(fd->track == -2)
		{
			int descyl = bp->b_cylin * fd->ft->steptrac;
			do {
				out_fdc(fdcu, NE7CMD_SENSEI);
				st0 = in_fdc(fdcu);
				cyl = in_fdc(fdcu);
				/*
				 * if this was a "ready changed" interrupt,
				 * fetch status again (can happen after
				 * enabling controller from reset state)
				 */
			} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);
			if (0 == descyl)
			{
				/*
				 * seek to cyl 0 requested; make sure we are
				 * really there
				 */
				out_fdc(fdcu, NE7CMD_SENSED);
				out_fdc(fdcu, fdu);
				st3 = in_fdc(fdcu);
				if ((st3 & NE7_ST3_T0) == 0) {
					printf(
		"fd%d: Seek to cyl 0, but not really there (ST3 = %b)\n",
					       fdu, st3, NE7_ST3BITS);
					if(fdc->retry < 3)
						fdc->retry = 3;
					return(retrier(fdcu));
				}
			}
			if (cyl != descyl)
			{
				printf(
		"fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n",
				       fdu, descyl, cyl, st0, NE7_ST0BITS);
				return(retrier(fdcu));
			}
		}

		fd->track = bp->b_cylin;
		if(format)
			fd->skip = (char *)&(finfo->fd_formb_cylno(0))
				- (char *)finfo;
		isa_dmastart(bp->b_flags, bp->b_un.b_addr+fd->skip,
			format ? bp->b_bcount : fdblk, fdc->dmachan);
		blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/fdblk
			+ fd->skip/fdblk;
		sectrac = fd->ft->sectrac;
		sec = blknum %  (sectrac * fd->ft->heads);
		head = sec / sectrac;
		sec = sec % sectrac + 1;
		fd->hddrv = ((head&1)<<2)+fdu;

		if(format || !read)
		{
			/* make sure the drive is writable */
			out_fdc(fdcu, NE7CMD_SENSED);
			out_fdc(fdcu, fdu);
			st3 = in_fdc(fdcu);
			if(st3 & NE7_ST3_WP)
			{
				/*
				 * XXX YES! this is ugly.
				 * in order to force the current operation
				 * to fail, we will have to fake an FDC
				 * error - all error handling is done
				 * by the retrier()
				 */
				fdc->status[0] = NE7_ST0_IC_AT;
				fdc->status[1] = NE7_ST1_NW;
				fdc->status[2] = 0;
				fdc->status[3] = fd->track;
				fdc->status[4] = head;
				fdc->status[5] = sec;
				fdc->retry = 8;	/* break out immediately */
				fdc->state = IOTIMEDOUT; /* not really... */
				return (1);
			}
		}

		if(format)
		{
			/* formatting */
			out_fdc(fdcu, NE7CMD_FORMAT);
			out_fdc(fdcu, head << 2 | fdu);
			out_fdc(fdcu, finfo->fd_formb_secshift);
			out_fdc(fdcu, finfo->fd_formb_nsecs);
			out_fdc(fdcu, finfo->fd_formb_gaplen);
			out_fdc(fdcu, finfo->fd_formb_fillbyte);
		}			
		else
		{
			if (read)
			{
				out_fdc(fdcu, NE7CMD_READ);      /* READ */
			}
			else
			{
				out_fdc(fdcu, NE7CMD_WRITE);     /* WRITE */
			}
			out_fdc(fdcu, head << 2 | fdu);  /* head & unit */
			out_fdc(fdcu, fd->track);        /* track */
			out_fdc(fdcu, head);
			out_fdc(fdcu, sec);              /* sector XXX +1? */
			out_fdc(fdcu, fd->ft->secsize);  /* sector size */
			out_fdc(fdcu, sectrac);          /* sectors/track */
			out_fdc(fdcu, fd->ft->gap);      /* gap size */
			out_fdc(fdcu, fd->ft->datalen);  /* data length */
		}
		fdc->state = IOCOMPLETE;
		timeout(fd_timeout, (caddr_t)fdcu, hz);
		return(0);	/* will return later */
	case IOCOMPLETE: /* IO DONE, post-analyze */
		untimeout(fd_timeout, (caddr_t)fdcu);
		for(i=0;i<7;i++)
		{
			fdc->status[i] = in_fdc(fdcu);
		}
		fdc->state = IOTIMEDOUT;
		/* FALLTHROUGH */
	case IOTIMEDOUT:
		isa_dmadone(bp->b_flags, bp->b_un.b_addr+fd->skip,
			    format ? bp->b_bcount : fdblk, fdc->dmachan);
		if (fdc->status[0] & NE7_ST0_IC)
		{
                        if ((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
			    && fdc->status[1] & NE7_ST1_OR) {
                                /*
				 * DMA overrun. Someone hogged the bus
				 * and didn't release it in time for the
				 * next FDC transfer.
				 * Just restart it, don't increment retry
				 * count. (vak)
                                 */
                                fdc->state = SEEKCOMPLETE;
                                return (1);
                        }
			else if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_IV
				&& fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			else if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
				&& fdc->status[2] & NE7_ST2_WC
				&& fdc->retry < 3)
				fdc->retry = 3;	/* force recalibrate */
			return(retrier(fdcu));
		}
		/* All OK */
		fd->skip += fdblk;
		if (!format && fd->skip < bp->b_bcount)
		{
			/* set up next transfer */
			blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/fdblk
				+ fd->skip/fdblk;
			bp->b_cylin =
				(blknum / (fd->ft->sectrac * fd->ft->heads));
			fdc->state = DOSEEK;
		}
		else
		{
			/* ALL DONE */
			fd->skip = 0;
			bp->b_resid = 0;
			dp->b_actf = bp->av_forw;
			biodone(bp);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
			fdc->state = FINDWORK;
		}
		return(1);
	case RESETCTLR:
		fdc_reset(fdc);
		fdc->retry++;
		fdc->state = STARTRECAL;
		break;
	case STARTRECAL:
		out_fdc(fdcu, NE7CMD_RECAL);	/* Recalibrate Function */
		out_fdc(fdcu, fdu);
		fdc->state = RECALWAIT;
		return(0);	/* will return later */
	case RECALWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, (caddr_t)fdcu, hz / 32);
		fdc->state = RECALCOMPLETE;
		return(0);	/* will return later */
	case RECALCOMPLETE:
		do {
			out_fdc(fdcu, NE7CMD_SENSEI);
			st0 = in_fdc(fdcu);
			cyl = in_fdc(fdcu);
			/*
			 * if this was a "ready changed" interrupt,
			 * fetch status again (can happen after
			 * enabling controller from reset state)
			 */
		} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);
		if ((st0 & NE7_ST0_IC) != NE7_ST0_IC_NT || cyl != 0)
		{
			printf("fd%d: recal failed ST0 %b cyl %d\n", fdu,
				st0, NE7_ST0BITS, cyl);
			if(fdc->retry < 3) fdc->retry = 3;
			return(retrier(fdcu));
		}
		fd->track = 0;
		/* Seek (probably) necessary */
		fdc->state = DOSEEK;
		return(1);	/* will return immediatly */
	case MOTORWAIT:
		if(fd->flags & FD_MOTOR_WAIT)
		{
			return(0); /* time's not up yet */
		}
		/*
		 * since the controller was off, it has lost its
		 * idea about the current track it were; thus,
		 * recalibrate the bastard
		 */
		fdc->state = STARTRECAL;
		return(1);	/* will return immediatly */
	default:
		printf("Unexpected FD int->");
		out_fdc(fdcu, NE7CMD_SENSEI);
		st0 = in_fdc(fdcu);
		cyl = in_fdc(fdcu);
		printf("ST0 = %x, PCN = %x\n", st0, cyl);
		out_fdc(fdcu, NE7CMD_READID); 
		out_fdc(fdcu, fd->fdsu);
		for(i=0;i<7;i++) {
			fdc->status[i] = in_fdc(fdcu);
		}
		if(fdc->status[0] != -1)
			printf("intr status :%lx %lx %lx %lx %lx %lx %lx\n",
			       fdc->status[0],
			       fdc->status[1],
			       fdc->status[2],
			       fdc->status[3],
			       fdc->status[4],
			       fdc->status[5],
			       fdc->status[6] );
		else
			printf("FDC timed out\n");
		return(0);
	}
	return(1); /* Come back immediatly to new state */
}

static int
retrier(fdcu)
	fdcu_t fdcu;
{
	fdc_p fdc = fdc_data + fdcu;
	register struct buf *dp, *bp;

	dp = &(fdc->head);
	bp = dp->b_actf;

	if(fd_data[FDUNIT(minor(bp->b_dev))].options & FDOPT_NORETRY)
		goto fail;
	switch(fdc->retry)
	{
	case 0: case 1: case 2:
		fdc->state = SEEKCOMPLETE;
		break;
	case 3: case 4: case 5:
		fdc->state = STARTRECAL;
		break;
	case 6:
		fdc->state = RESETCTLR;
		break;
	case 7:
		break;
	default:
	fail:
		{
			dev_t sav_b_dev = bp->b_dev;
			/* Trick diskerr */
			bp->b_dev = makedev(major(bp->b_dev),
					    (FDUNIT(minor(bp->b_dev))<<3)|3);
			diskerr(bp, "fd", "hard error", LOG_PRINTF,
				fdc->fd->skip / DEV_BSIZE,
				(struct disklabel *)NULL);
			bp->b_dev = sav_b_dev;
			printf(" (ST0 %b ", fdc->status[0], NE7_ST0BITS);
			printf(" ST1 %b ", fdc->status[1], NE7_ST1BITS);
			printf(" ST2 %b ", fdc->status[2], NE7_ST2BITS);
			printf("cyl %d hd %d sec %d)\n",
			       fdc->status[3], fdc->status[4], fdc->status[5]);
		}
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid = bp->b_bcount - fdc->fd->skip;
		dp->b_actf = bp->av_forw;
		fdc->fd->skip = 0;
		biodone(bp);
		fdc->state = FINDWORK;
		fdc->fd = (fd_p) 0;
		fdc->fdu = -1;
		/* XXX abort current command, if any.  */
		return(1);
	}
	fdc->retry++;
	return(1);
}

static int
fdformat(dev, finfo, p)
	dev_t dev;
	struct fd_formb *finfo;
	struct proc *p;
{
 	fdu_t	fdu;
 	fd_p	fd;

	struct buf *bp;
	int rv = 0, s;
	size_t fdblk;

 	fdu = FDUNIT(minor(dev));
	fd = &fd_data[fdu];
	fdblk = 128 << fd->ft->secsize;

	/* set up a buffer header for fdstrategy() */
	bp = (struct buf *)malloc(sizeof(struct buf), M_TEMP, M_NOWAIT);
	if(bp == 0)
		return ENOBUFS;
	bzero((void *)bp, sizeof(struct buf));
	bp->b_flags = B_BUSY | B_PHYS | B_FORMAT;
	bp->b_proc = p;
	bp->b_dev = dev;

	/*
	 * calculate a fake blkno, so fdstrategy() would initiate a
	 * seek to the requested cylinder
	 */
	bp->b_blkno = (finfo->cyl * (fd->ft->sectrac * fd->ft->heads)
		+ finfo->head * fd->ft->sectrac) * fdblk / DEV_BSIZE;

	bp->b_bcount = sizeof(struct fd_idfield_data) * finfo->fd_formb_nsecs;
	bp->b_un.b_addr = (caddr_t)finfo;
	
	/* now do the format */
	fdstrategy(bp);

	/* ...and wait for it to complete */
	s = splbio();
	while(!(bp->b_flags & B_DONE))
	{
		rv = tsleep((caddr_t)bp, PRIBIO, "fdform", 20 * hz);
		if(rv == EWOULDBLOCK)
			break;
	}
	splx(s);
	
	if(rv == EWOULDBLOCK)
		/* timed out */
		rv = EIO;
	if(bp->b_flags & B_ERROR)
		rv = bp->b_error;
	biodone(bp);
	free(bp, M_TEMP);
	return rv;
}

/*
 *
 * TODO: Think about allocating buffer off stack.
 *       Don't pass uncast 0's and NULL's to read/write/setdisklabel().
 *       Watch out for NetBSD's different *disklabel() interface.
 *
 */

int
fdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	int cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
 	fdu_t	fdu = FDUNIT(minor(dev));
 	fd_p	fd = &fd_data[fdu];
	size_t fdblk;

	struct fd_type *fdt;
	struct disklabel *dl;
	char buffer[DEV_BSIZE];
	int error = 0;

#if NFT > 0
	int type = FDTYPE(minor(dev));

	/* check for a tape ioctl */
	if (type & F_TAPE_TYPE)
		return ftioctl(dev, cmd, addr, flag, p);
#endif

	fdblk = 128 << fd->ft->secsize;

	switch (cmd)
	{
	case DIOCGDINFO:
		bzero(buffer, sizeof (buffer));
		dl = (struct disklabel *)buffer;
		dl->d_secsize = fdblk;
		fdt = fd_data[FDUNIT(minor(dev))].ft;
		dl->d_secpercyl = fdt->size / fdt->tracks;
		dl->d_type = DTYPE_FLOPPY;

		if (readdisklabel(dev, fdstrategy, dl, NULL, 0, 0) == NULL)
			error = 0;
		else
			error = EINVAL;

		*(struct disklabel *)addr = *dl;
		break;

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			error = EBADF;
		break;

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
		{
			error = EBADF;
			break;
		}

		dl = (struct disklabel *)addr;

		if ((error =
		     setdisklabel ((struct disklabel *)buffer, dl, 0, NULL)))
			break;

		error = writedisklabel(dev, fdstrategy,
				       (struct disklabel *)buffer, NULL);
		break;

	case FD_FORM:
		if((flag & FWRITE) == 0)
			error = EBADF;	/* must be opened for writing */
		else if(((struct fd_formb *)addr)->format_version !=
			FD_FORMAT_VERSION)
			error = EINVAL;	/* wrong version of formatting prog */
		else
			error = fdformat(dev, (struct fd_formb *)addr, p);
		break;

	case FD_GTYPE:                  /* get drive type */
		*(struct fd_type *)addr = *fd_data[FDUNIT(minor(dev))].ft;
		break;

	case FD_STYPE:                  /* set drive type */
		/* this is considered harmful; only allow for superuser */
		if(suser(p->p_ucred, &p->p_acflag) != 0)
			return EPERM;
		*fd_data[FDUNIT(minor(dev))].ft = *(struct fd_type *)addr;
		break;

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd_data[FDUNIT(minor(dev))].options;
		break;
		
	case FD_SOPTS:			/* set drive options */
		fd_data[FDUNIT(minor(dev))].options = *(int *)addr;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

#endif
/*
 * Hello emacs, these are the
 * Local Variables:
 *  c-indent-level:               8
 *  c-continued-statement-offset: 8
 *  c-continued-brace-offset:     0
 *  c-brace-offset:              -8
 *  c-brace-imaginary-offset:     0
 *  c-argdecl-indent:             8
 *  c-label-offset:              -8
 *  c++-hanging-braces:           1
 *  c++-access-specifier-offset: -8
 *  c++-empty-arglist-indent:     8
 *  c++-friend-offset:            0
 * End:
 */
