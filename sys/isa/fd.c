/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Copyright (c) 1993, 1994 by
 *  jc@irbs.UUCP (John Capo)
 *  vak@zebub.msk.su (Serge Vakulenko)
 *  ache@astral.msk.su (Andrew A. Chernov)
 *
 * Copyright (c) 1993, 1994, 1995 by
 *  joerg_wunsch@uriah.sax.de (Joerg Wunsch)
 *  dufault@hda.com (Peter Dufault)
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
 *	$Id: fd.c,v 1.105 1997/10/19 13:12:02 joerg Exp $
 *
 */

#include "ft.h"
#if NFT < 1
#undef NFDC
#endif
#include "fd.h"
#include "opt_fdc.h"

#if NFDC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <machine/clock.h>
#include <machine/ioctl_fd.h>
#include <sys/disklabel.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#ifdef notyet
#include <sys/dkstat.h>
#endif
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/fdreg.h>
#include <i386/isa/fdc.h>
#include <i386/isa/rtc.h>
#include <machine/stdarg.h>
#if NFT > 0
#include <sys/ftape.h>
#include <i386/isa/ftreg.h>
#endif
#ifdef DEVFS
#include <sys/devfsext.h>
#endif

/* misuse a flag to identify format operation */
#define B_FORMAT B_XXX

/* configuration flags */
#define FDC_PRETEND_D0	(1 << 0)	/* pretend drive 0 to be there */

/* internally used only, not really from CMOS: */
#define RTCFDT_144M_PRETENDED	0x1000

/*
 * this biotab field doubles as a field for the physical unit number
 * on the controller
 */
#define id_physid id_scsiid

/* error returns for fd_cmd() */
#define FD_FAILED -1
#define FD_NOT_VALID -2
#define FDC_ERRMAX	100	/* do not log more */

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


static struct fd_type fd_types[NUMTYPES] =
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
static struct fd_data {
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
#define FD_NO_TRACK -2
	int	track;		/* where we think the head is */
	int	options;	/* user configurable options, see ioctl_fd.h */
#ifdef notyet
	int	dkunit;		/* disk stats unit number */
#endif
	struct	callout_handle toffhandle;
	struct	callout_handle tohandle;
#ifdef DEVFS
	void	*bdevs[1 + NUMDENS + MAXPARTITIONS];
	void	*cdevs[1 + NUMDENS + MAXPARTITIONS];
#endif
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
int ftintr(ftu_t ftu);
int ftclose(dev_t, int);
void ftstrategy(struct buf *);
int ftioctl(dev_t, int, caddr_t, int, struct proc *);
int ftdump(dev_t);
int ftsize(dev_t);
int ftattach(struct isa_device *, struct isa_device *, int);
#endif

/* autoconfig functions */
static int fdprobe(struct isa_device *);
static int fdattach(struct isa_device *);

/* needed for ft driver, thus exported */
int in_fdc(fdcu_t);
int out_fdc(fdcu_t, int);

/* internal functions */
static void set_motor(fdcu_t, int, int);
#  define TURNON 1
#  define TURNOFF 0
static timeout_t fd_turnoff;
static timeout_t fd_motor_on;
static void fd_turnon(fdu_t);
static void fdc_reset(fdc_p);
static int fd_in(fdcu_t, int *);
static void fdstart(fdcu_t);
static timeout_t fd_timeout;
static timeout_t fd_pseudointr;
static int fdstate(fdcu_t, fdc_p);
static int retrier(fdcu_t);
static int fdformat(dev_t, struct fd_formb *, struct proc *);

static int enable_fifo(fdc_p fdc);

static int fifo_threshold = 8;	/* XXX: should be accessible via sysctl */


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

#ifdef	FDC_DEBUG
static char const * const fdstates[] =
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
static int volatile fd_debug = 0;
#define TRACE0(arg) if(fd_debug) printf(arg)
#define TRACE1(arg1, arg2) if(fd_debug) printf(arg1, arg2)
#else /* FDC_DEBUG */
#define TRACE0(arg)
#define TRACE1(arg1, arg2)
#endif /* FDC_DEBUG */

/* autoconfig structure */

struct	isa_driver fdcdriver = {
	fdprobe, fdattach, "fdc",
};

static	d_open_t	Fdopen;	/* NOTE, not fdopen */
static	d_close_t	fdclose;
static	d_ioctl_t	fdioctl;
static	d_strategy_t	fdstrategy;

#define CDEV_MAJOR 9
#define BDEV_MAJOR 2
static struct cdevsw fd_cdevsw;
static struct bdevsw fd_bdevsw = 
	{ Fdopen,	fdclose,	fdstrategy,	fdioctl,	/*2*/
	  nodump,	nopsize,	D_DISK,	"fd",	&fd_cdevsw,	-1 };


static struct isa_device *fdcdevs[NFDC];

static int
fdc_err(fdcu_t fdcu, const char *s)
{
	fdc_data[fdcu].fdc_errs++;
	if(s) {
		if(fdc_data[fdcu].fdc_errs < FDC_ERRMAX)
			printf("fdc%d: %s", fdcu, s);
		else if(fdc_data[fdcu].fdc_errs == FDC_ERRMAX)
			printf("fdc%d: too many errors, not logging any more\n",
			       fdcu);
	}

	return FD_FAILED;
}

/*
 * fd_cmd: Send a command to the chip.  Takes a varargs with this structure:
 * Unit number,
 * # of output bytes, output bytes as ints ...,
 * # of input bytes, input bytes as ints ...
 */

static int
fd_cmd(fdcu_t fdcu, int n_out, ...)
{
	u_char cmd;
	int n_in;
	int n;
	va_list ap;

	va_start(ap, n_out);
	cmd = (u_char)(va_arg(ap, int));
	va_end(ap);
	va_start(ap, n_out);
	for (n = 0; n < n_out; n++)
	{
		if (out_fdc(fdcu, va_arg(ap, int)) < 0)
		{
			char msg[50];
			sprintf(msg,
				"cmd %x failed at out byte %d of %d\n",
				cmd, n + 1, n_out);
			return fdc_err(fdcu, msg);
		}
	}
	n_in = va_arg(ap, int);
	for (n = 0; n < n_in; n++)
	{
		int *ptr = va_arg(ap, int *);
		if (fd_in(fdcu, ptr) < 0)
		{
			char msg[50];
			sprintf(msg,
				"cmd %02x failed at in byte %d of %d\n",
				cmd, n + 1, n_in);
			return fdc_err(fdcu, msg);
		}
	}

	return 0;
}

static int 
enable_fifo(fdc_p fdc)
{
	int i, j;

	if ((fdc->flags & FDC_HAS_FIFO) == 0) {
		
		/*
		 * XXX: 
		 * Cannot use fd_cmd the normal way here, since
		 * this might be an invalid command. Thus we send the
		 * first byte, and check for an early turn of data directon.
		 */
		
		if (out_fdc(fdc->fdcu, I8207X_CONFIGURE) < 0)
			return fdc_err(fdc->fdcu, "Enable FIFO failed\n");
		
		/* If command is invalid, return */
		j = 100000;
		while ((i = inb(fdc->baseport + FDSTS) & (NE7_DIO | NE7_RQM))
		       != NE7_RQM && j-- > 0)
			if (i == (NE7_DIO | NE7_RQM)) {
				fdc_reset(fdc);
				return FD_FAILED;
			}
		if (j<0 || 
		    fd_cmd(fdc->fdcu, 3,
			   0, (fifo_threshold - 1) & 0xf, 0, 0) < 0) {
			fdc_reset(fdc);
			return fdc_err(fdc->fdcu, "Enable FIFO failed\n");
		}
		fdc->flags |= FDC_HAS_FIFO;
		return 0;
	}
	if (fd_cmd(fdc->fdcu, 4,
		   I8207X_CONFIGURE, 0, (fifo_threshold - 1) & 0xf, 0, 0) < 0)
		return fdc_err(fdc->fdcu, "Re-enable FIFO failed\n");
	return 0;
}

static int
fd_sense_drive_status(fdc_p fdc, int *st3p)
{
	int st3;

	if (fd_cmd(fdc->fdcu, 2, NE7CMD_SENSED, fdc->fdu, 1, &st3))
	{
		return fdc_err(fdc->fdcu, "Sense Drive Status failed\n");
	}
	if (st3p)
		*st3p = st3;

	return 0;
}

static int
fd_sense_int(fdc_p fdc, int *st0p, int *cylp)
{
	int st0, cyl;

	int ret = fd_cmd(fdc->fdcu, 1, NE7CMD_SENSEI, 1, &st0);

	if (ret)
	{
		(void)fdc_err(fdc->fdcu,
			      "sense intr err reading stat reg 0\n");
		return ret;
	}

	if (st0p)
		*st0p = st0;

	if ((st0 & NE7_ST0_IC) == NE7_ST0_IC_IV)
	{
		/*
		 * There doesn't seem to have been an interrupt.
		 */
		return FD_NOT_VALID;
	}

	if (fd_in(fdc->fdcu, &cyl) < 0)
	{
		return fdc_err(fdc->fdcu, "can't get cyl num\n");
	}

	if (cylp)
		*cylp = cyl;

	return 0;
}


static int
fd_read_status(fdc_p fdc, int fdsu)
{
	int i, ret;

	for (i = 0; i < 7; i++)
	{
		/*
		 * XXX types are poorly chosen.  Only bytes can by read
		 * from the hardware, but fdc_status wants u_longs and
		 * fd_in() gives ints.
		 */
		int status;

		ret = fd_in(fdc->fdcu, &status);
		fdc->status[i] = status;
		if (ret != 0)
			break;
	}

	if (ret == 0)
		fdc->flags |= FDC_STAT_VALID;
	else
		fdc->flags &= ~FDC_STAT_VALID;

	return ret;
}

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/

/*
 * probe for existance of controller
 */
static int
fdprobe(struct isa_device *dev)
{
	fdcu_t	fdcu = dev->id_unit;
	if(fdc_data[fdcu].flags & FDC_ATTACHED)
	{
		printf("fdc%d: unit used multiple times\n", fdcu);
		return 0;
	}

	fdcdevs[fdcu] = dev;
	fdc_data[fdcu].baseport = dev->id_iobase;

	/* First - lets reset the floppy controller */
	outb(dev->id_iobase+FDOUT, 0);
	DELAY(100);
	outb(dev->id_iobase+FDOUT, FDO_FRST);

	/* see if it can handle a command */
	if (fd_cmd(fdcu,
		   3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
		   0))
	{
		return(0);
	}
	return (IO_FDCSIZE);
}

/*
 * wire controller into system, look for floppy units
 */
static int
fdattach(struct isa_device *dev)
{
	unsigned fdt;
	fdu_t	fdu;
	fdcu_t	fdcu = dev->id_unit;
	fdc_p	fdc = fdc_data + fdcu;
	fd_p	fd;
	int	fdsu, st0, st3, i;
#if NFT > 0
	int	unithasfd;
#endif
	struct isa_device *fdup;
	int ic_type = 0;
#ifdef DEVFS
	int	mynor;
	int	typemynor;
	int	typesize;
#endif

	fdc->fdcu = fdcu;
	fdc->flags |= FDC_ATTACHED;
	fdc->dmachan = dev->id_drq;
	/* Acquire the DMA channel forever, The driver will do the rest */
	isa_dma_acquire(fdc->dmachan);
	isa_dmainit(fdc->dmachan, 128 << 3 /* XXX max secsize */);
	fdc->state = DEVIDLE;
	/* reset controller, turn motor off, clear fdout mirror reg */
	outb(fdc->baseport + FDOUT, ((fdc->fdout = 0)));
	bufq_init(&fdc->head);

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
			case 0: if (dev->id_flags & FDC_PRETEND_D0)
					fdt = RTCFDT_144M | RTCFDT_144M_PRETENDED;
				else
					fdt = (rtcin(RTC_FDISKETTE) & 0xf0);
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
			/* Tell FT if there was already a disk     */
			/* with this unit number found.            */

			unithasfd = 0;
			if (fdu < NFD && fd->type != NO_TYPE)
				unithasfd = 1;
			if (ftattach(dev, fdup, unithasfd))
				continue;
			if (fdsu < DRVS_PER_CTLR)
				fd->type = NO_TYPE;
#endif
			continue;
		}

		/* select it */
		set_motor(fdcu, fdsu, TURNON);
		DELAY(1000000);	/* 1 sec */

		if (ic_type == 0 &&
		    fd_cmd(fdcu, 1, NE7CMD_VERSION, 1, &ic_type) == 0)
		{
#ifdef FDC_PRINT_BOGUS_CHIPTYPE
			printf("fdc%d: ", fdcu);
#endif
			ic_type = (u_char)ic_type;
			switch( ic_type ) {
			case 0x80:
#ifdef FDC_PRINT_BOGUS_CHIPTYPE
				printf("NEC 765\n");
#endif
				fdc->fdct = FDC_NE765;
				break;
			case 0x81:
#ifdef FDC_PRINT_BOGUS_CHIPTYPE
				printf("Intel 82077\n");
#endif
				fdc->fdct = FDC_I82077;
				break;
			case 0x90:
#ifdef FDC_PRINT_BOGUS_CHIPTYPE
				printf("NEC 72065B\n");
#endif
				fdc->fdct = FDC_NE72065;
				break;
			default:
#ifdef FDC_PRINT_BOGUS_CHIPTYPE
				printf("unknown IC type %02x\n", ic_type);
#endif
				fdc->fdct = FDC_UNKNOWN;
				break;
			}
			if (fdc->fdct != FDC_NE765 &&
			    fdc->fdct != FDC_UNKNOWN && 
			    enable_fifo(fdc) == 0) {
				printf("fdc%d: FIFO enabled", fdcu);
				printf(", %d bytes threshold\n", 
				       fifo_threshold);
			}
		}
		if ((fd_cmd(fdcu, 2, NE7CMD_SENSED, fdsu, 1, &st3) == 0) &&
		    (st3 & NE7_ST3_T0)) {
			/* if at track 0, first seek inwards */
			/* seek some steps: */
			(void)fd_cmd(fdcu, 3, NE7CMD_SEEK, fdsu, 10, 0);
			DELAY(300000); /* ...wait a moment... */
			(void)fd_sense_int(fdc, 0, 0); /* make ctrlr happy */
		}

		/* If we're at track 0 first seek inwards. */
		if ((fd_sense_drive_status(fdc, &st3) == 0) &&
		    (st3 & NE7_ST3_T0)) {
			/* Seek some steps... */
			if (fd_cmd(fdcu, 3, NE7CMD_SEEK, fdsu, 10, 0) == 0) {
				/* ...wait a moment... */
				DELAY(300000);
				/* make ctrlr happy: */
				(void)fd_sense_int(fdc, 0, 0);
			}
		}

		for(i = 0; i < 2; i++) {
			/*
			 * we must recalibrate twice, just in case the
			 * heads have been beyond cylinder 76, since most
			 * FDCs still barf when attempting to recalibrate
			 * more than 77 steps
			 */
			/* go back to 0: */
			if (fd_cmd(fdcu, 2, NE7CMD_RECAL, fdsu, 0) == 0) {
				/* a second being enough for full stroke seek*/
				DELAY(i == 0? 1000000: 300000);

				/* anything responding? */
				if (fd_sense_int(fdc, &st0, 0) == 0 &&
				(st0 & NE7_ST0_EC) == 0)
					break; /* already probed succesfully */
			}
		}

		set_motor(fdcu, fdsu, TURNOFF);

		if (st0 & NE7_ST0_EC) /* no track 0 -> no drive present */
			continue;

		fd->track = FD_NO_TRACK;
		fd->fdc = fdc;
		fd->fdsu = fdsu;
		fd->options = 0;
		callout_handle_init(&fd->toffhandle);
		callout_handle_init(&fd->tohandle);
		printf("fd%d: ", fdu);

		switch (fdt) {
		case RTCFDT_12M:
			printf("1.2MB 5.25in\n");
			fd->type = FD_1200;
			break;
		case RTCFDT_144M | RTCFDT_144M_PRETENDED:
			printf("config-pretended ");
			fdt = RTCFDT_144M;
			/* fallthrough */
		case RTCFDT_144M:
			printf("1.44MB 3.5in\n");
			fd->type = FD_1440;
			break;
		case RTCFDT_288M:
		case RTCFDT_288M_1:
			printf("2.88MB 3.5in - 1.44MB mode\n");
			fd->type = FD_1440;
			break;
		case RTCFDT_360K:
			printf("360KB 5.25in\n");
			fd->type = FD_360;
			break;
		case RTCFDT_720K:
			printf("720KB 3.5in\n");
			fd->type = FD_720;
			break;
		default:
			printf("unknown\n");
			fd->type = NO_TYPE;
			continue;
		}
#ifdef DEVFS
		mynor = fdu << 6;
		fd->bdevs[0] = devfs_add_devswf(&fd_bdevsw, mynor, DV_BLK,
						UID_ROOT, GID_OPERATOR, 0640,
						"fd%d", fdu);
		fd->cdevs[0] = devfs_add_devswf(&fd_cdevsw, mynor, DV_CHR,
						UID_ROOT, GID_OPERATOR, 0640,
						"rfd%d", fdu);
		for (i = 1; i < 1 + NUMDENS; i++) {
			/*
			 * XXX this and the lookup in Fdopen() should be
			 * data driven.
			 */
			switch (fd->type) {
			case FD_360:
				if (i != FD_360)
					continue;
				break;
			case FD_720:
				if (i != FD_720 && i != FD_800 && i != FD_820)
					continue;
				break;
			case FD_1200:
				if (i != FD_360 && i != FD_720 && i != FD_800
				    && i != FD_820 && i != FD_1200
				    && i != FD_1440 && i != FD_1480)
					continue;
				break;
			case FD_1440:
				if (i != FD_720 && i != FD_800 && i != FD_820
				    && i != FD_1200 && i != FD_1440
				    && i != FD_1480 && i != FD_1720)
					continue;
				break;
			}
			typemynor = mynor | i;
			typesize = fd_types[i - 1].size / 2;
			/*
			 * XXX all these conversions give bloated code and
			 * confusing names.
			 */
			if (typesize == 1476)
				typesize = 1480;
			if (typesize == 1722)
				typesize = 1720;
			fd->bdevs[i] =
				devfs_add_devswf(&fd_bdevsw, typemynor, DV_BLK,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "fd%d.%d", fdu, typesize);
			fd->cdevs[i] =
				devfs_add_devswf(&fd_cdevsw, typemynor, DV_CHR,
						 UID_ROOT, GID_OPERATOR, 0640,
						 "rfd%d.%d", fdu, typesize);
		}
		for (i = 0; i < MAXPARTITIONS; i++) {
			fd->bdevs[1 + NUMDENS + i] =
				devfs_link(fd->bdevs[0],
					   "fd%d%c", fdu, 'a' + i);
			fd->cdevs[1 + NUMDENS + i] =
				devfs_link(fd->cdevs[0],
					   "rfd%d%c", fdu, 'a' + i);
		}
#endif /* DEVFS */
#ifdef notyet
		if (dk_ndrive < DK_NDRIVE) {
			sprintf(dk_names[dk_ndrive], "fd%d", fdu);
			fd->dkunit = dk_ndrive++;
			/*
			 * XXX assume rate is FDC_500KBPS.
			 */
			dk_wpms[dk_ndrive] = 500000 / 8 / 2;
		} else {
			fd->dkunit = -1;
		}
#endif
	}

	return (1);
}

/****************************************************************************/
/*                            motor control stuff                           */
/*		remember to not deselect the drive we're working on         */
/****************************************************************************/
static void
set_motor(fdcu_t fdcu, int fdsu, int turnon)
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
		/*
		 * XXX
		 * special case: since we have just woken up the FDC
		 * from its sleep, we silently assume the command will
		 * be accepted, and do not test for a timeout
		 */
		(void)fd_cmd(fdcu, 3, NE7CMD_SPECIFY,
			     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
			     0);
		if (fdc_data[fdcu].flags & FDC_HAS_FIFO)
			(void) enable_fifo(&fdc_data[fdcu]);
	}
}

static void
fd_turnoff(void *arg1)
{
	fdu_t fdu = (fdu_t)arg1;
	int	s;
	fd_p fd = fd_data + fdu;

	TRACE1("[fd%d: turnoff]", fdu);

	/*
	 * Don't turn off the motor yet if the drive is active.
	 * XXX shouldn't even schedule turnoff until drive is inactive
	 * and nothing is queued on it.
	 */
	if (fd->fdc->state != DEVIDLE && fd->fdc->fdu == fdu) {
		fd->toffhandle = timeout(fd_turnoff, arg1, 4 * hz);
		return;
	}

	s = splbio();
	fd->flags &= ~FD_MOTOR;
	set_motor(fd->fdc->fdcu, fd->fdsu, TURNOFF);
	splx(s);
}

static void
fd_motor_on(void *arg1)
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
fd_turnon(fdu_t fdu)
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
fdc_reset(fdc_p fdc)
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

	/* XXX after a reset, silently believe the FDC will accept commands */
	(void)fd_cmd(fdcu, 3, NE7CMD_SPECIFY,
		     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
		     0);
	if (fdc->flags & FDC_HAS_FIFO)
		(void) enable_fifo(fdc);
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc(fdcu_t fdcu)
{
	int baseport = fdc_data[fdcu].baseport;
	int i, j = 100000;
	while ((i = inb(baseport+FDSTS) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM)
			return fdc_err(fdcu, "ready for output in input\n");
	if (j <= 0)
		return fdc_err(fdcu, bootverbose? "input ready timeout\n": 0);
#ifdef	FDC_DEBUG
	i = inb(baseport+FDDATA);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	return(i);
#else	/* !FDC_DEBUG */
	return inb(baseport+FDDATA);
#endif	/* FDC_DEBUG */
}

/*
 * fd_in: Like in_fdc, but allows you to see if it worked.
 */
static int
fd_in(fdcu_t fdcu, int *ptr)
{
	int baseport = fdc_data[fdcu].baseport;
	int i, j = 100000;
	while ((i = inb(baseport+FDSTS) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM)
			return fdc_err(fdcu, "ready for output in input\n");
	if (j <= 0)
		return fdc_err(fdcu, bootverbose? "input ready timeout\n": 0);
#ifdef	FDC_DEBUG
	i = inb(baseport+FDDATA);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	*ptr = i;
	return 0;
#else	/* !FDC_DEBUG */
	i = inb(baseport+FDDATA);
	if (ptr)
		*ptr = i;
	return 0;
#endif	/* FDC_DEBUG */
}

int
out_fdc(fdcu_t fdcu, int x)
{
	int baseport = fdc_data[fdcu].baseport;
	int i;

	/* Check that the direction bit is set */
	i = 100000;
	while ((inb(baseport+FDSTS) & NE7_DIO) && i-- > 0);
	if (i <= 0) return fdc_err(fdcu, "direction bit not set\n");

	/* Check that the floppy controller is ready for a command */
	i = 100000;
	while ((inb(baseport+FDSTS) & NE7_RQM) == 0 && i-- > 0);
	if (i <= 0)
		return fdc_err(fdcu, bootverbose? "output ready timeout\n": 0);

	/* Send the command and return */
	outb(baseport+FDDATA, x);
	TRACE1("[0x%x->FDDATA]", x);
	return (0);
}

/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
int
Fdopen(dev_t dev, int flags, int mode, struct proc *p)
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
fdclose(dev_t dev, int flags, int mode, struct proc *p)
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
	unsigned nblocks, blknum, cando;
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

#if NFT > 0
	if (FDTYPE(minor(bp->b_dev)) & F_TAPE_TYPE) {
		/* ft tapes do not (yet) support strategy i/o */
		bp->b_error = ENODEV;
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
	fdblk = 128 << (fd->ft->secsize);
	if (!(bp->b_flags & B_FORMAT)) {
		if ((fdu >= NFD) || (bp->b_blkno < 0)) {
			printf(
		"fd%d: fdstrat: bad request blkno = %lu, bcount = %ld\n",
			       fdu, (u_long)bp->b_blkno, bp->b_bcount);
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
	if (bp->b_blkno > 20000000) {
		/*
		 * Reject unreasonably high block number, prevent the
		 * multiplication below from overflowing.
		 */
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}
	blknum = (unsigned) bp->b_blkno * DEV_BSIZE/fdblk;
 	nblocks = fd->ft->size;
	bp->b_resid = 0;
	if (blknum + (bp->b_bcount / fdblk) > nblocks) {
		if (blknum <= nblocks) {
			cando = (nblocks - blknum) * fdblk;
			bp->b_resid = bp->b_bcount - cando;
			if (cando == 0)
				goto bad;	/* not actually bad but EOF */
		} else {
			bp->b_error = EINVAL;
			bp->b_flags |= B_ERROR;
			goto bad;
		}
	}
 	bp->b_pblkno = bp->b_blkno;
	s = splbio();
	bufqdisksort(&fdc->head, bp);
	untimeout(fd_turnoff, (caddr_t)fdu, fd->toffhandle); /* a good idea */
	fdstart(fdcu);
	splx(s);
	return;

bad:
	biodone(bp);
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
fdstart(fdcu_t fdcu)
{
	int s;

	s = splbio();
	if(fdc_data[fdcu].state == DEVIDLE)
	{
		fdintr(fdcu);
	}
	splx(s);
}

static void
fd_timeout(void *arg1)
{
	fdcu_t fdcu = (fdcu_t)arg1;
	fdu_t fdu = fdc_data[fdcu].fdu;
	int baseport = fdc_data[fdcu].baseport;
	struct buf *bp;
	int s;

	bp = bufq_first(&fdc_data[fdcu].head);

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
static void
fd_pseudointr(void *arg1)
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
fdstate(fdcu_t fdcu, fdc_p fdc)
{
	int read, format, head, sec = 0, sectrac, st0, cyl, st3;
	unsigned blknum = 0, b_cylinder = 0;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	register struct buf *bp;
	struct fd_formb *finfo = NULL;
	size_t fdblk;

	bp = bufq_first(&fdc->head);
	if(!bp) {
		/***********************************************\
		* nothing left for this controller to do	*
		* Force into the IDLE state,			*
		\***********************************************/
		fdc->state = DEVIDLE;
		if(fdc->fd)
		{
			printf("fd%d: unexpected valid fd pointer\n",
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
		printf("fd%d: confused fd pointers\n", fdu);
	}
	read = bp->b_flags & B_READ;
	format = bp->b_flags & B_FORMAT;
	if(format) {
		finfo = (struct fd_formb *)bp->b_data;
		fd->skip = (char *)&(finfo->fd_formb_cylno(0))
			- (char *)finfo;
	}
	if (fdc->state == DOSEEK || fdc->state == SEEKCOMPLETE) {
		blknum = (unsigned) bp->b_blkno * DEV_BSIZE/fdblk +
			fd->skip/fdblk;
		b_cylinder = blknum / (fd->ft->sectrac * fd->ft->heads);
	}
	TRACE1("fd%d", fdu);
	TRACE1("[%s]", fdstates[fdc->state]);
	TRACE1("(0x%x)", fd->flags);
	untimeout(fd_turnoff, (caddr_t)fdu, fd->toffhandle);
	fd->toffhandle = timeout(fd_turnoff, (caddr_t)fdu, 4 * hz);
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
		if (b_cylinder == (unsigned)fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			break;
		}
		if (fd_cmd(fdcu, 3, NE7CMD_SEEK,
			   fd->fdsu, b_cylinder * fd->ft->steptrac,
			   0))
		{
			/*
			 * seek command not accepted, looks like
			 * the FDC went off to the Saints...
			 */
			fdc->retry = 6;	/* try a reset */
			return(retrier(fdcu));
		}
		fd->track = FD_NO_TRACK;
		fdc->state = SEEKWAIT;
		return(0);	/* will return later */
	case SEEKWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, (caddr_t)fdcu, hz / 16);
		fdc->state = SEEKCOMPLETE;
		return(0);	/* will return later */
	case SEEKCOMPLETE : /* SEEK DONE, START DMA */
		/* Make sure seek really happened*/
		if(fd->track == FD_NO_TRACK)
		{
			int descyl = b_cylinder * fd->ft->steptrac;
			do {
				/*
				 * This might be a "ready changed" interrupt,
				 * which cannot really happen since the
				 * RDY pin is hardwired to + 5 volts.  This
				 * generally indicates a "bouncing" intr
				 * line, so do one of the following:
				 *
				 * When running on an enhanced FDC that is
				 * known to not go stuck after responding
				 * with INVALID, fetch all interrupt states
				 * until seeing either an INVALID or a
				 * real interrupt condition.
				 *
				 * When running on a dumb old NE765, give
				 * up immediately.  The controller will
				 * provide up to four dummy RC interrupt
				 * conditions right after reset (for the
				 * corresponding four drives), so this is
				 * our only chance to get notice that it
				 * was not the FDC that caused the interrupt.
				 */
				if (fd_sense_int(fdc, &st0, &cyl)
				    == FD_NOT_VALID)
					return 0;
				if(fdc->fdct == FDC_NE765
				   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
					return 0; /* hope for a real intr */
			} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);

			if (0 == descyl)
			{
				int failed = 0;
				/*
				 * seek to cyl 0 requested; make sure we are
				 * really there
				 */
				if (fd_sense_drive_status(fdc, &st3))
					failed = 1;
				if ((st3 & NE7_ST3_T0) == 0) {
					printf(
		"fd%d: Seek to cyl 0, but not really there (ST3 = %b)\n",
					       fdu, st3, NE7_ST3BITS);
					failed = 1;
				}

				if (failed)
				{
					if(fdc->retry < 3)
						fdc->retry = 3;
					return(retrier(fdcu));
				}
			}

			if (cyl != descyl)
			{
				printf(
		"fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n",
				       fdu, descyl, cyl, st0);
				return(retrier(fdcu));
			}
		}

		fd->track = b_cylinder;
		isa_dmastart(bp->b_flags, bp->b_data+fd->skip,
			format ? bp->b_bcount : fdblk, fdc->dmachan);
		sectrac = fd->ft->sectrac;
		sec = blknum %  (sectrac * fd->ft->heads);
		head = sec / sectrac;
		sec = sec % sectrac + 1;
		fd->hddrv = ((head&1)<<2)+fdu;

		if(format || !read)
		{
			/* make sure the drive is writable */
			if(fd_sense_drive_status(fdc, &st3) != 0)
			{
				/* stuck controller? */
				fdc->retry = 6;	/* reset the beast */
				return(retrier(fdcu));
			}
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
			if(fd_cmd(fdcu, 6,
				  NE7CMD_FORMAT,
				  head << 2 | fdu,
				  finfo->fd_formb_secshift,
				  finfo->fd_formb_nsecs,
				  finfo->fd_formb_gaplen,
				  finfo->fd_formb_fillbyte,
				  0))
			{
				/* controller fell over */
				fdc->retry = 6;
				return(retrier(fdcu));
			}
		}
		else
		{
			if (fd_cmd(fdcu, 9,
				   (read ? NE7CMD_READ : NE7CMD_WRITE),
				   head << 2 | fdu,  /* head & unit */
				   fd->track,        /* track */
				   head,
				   sec,              /* sector + 1 */
				   fd->ft->secsize,  /* sector size */
				   sectrac,          /* sectors/track */
				   fd->ft->gap,      /* gap size */
				   fd->ft->datalen,  /* data length */
				   0))
			{
				/* the beast is sleeping again */
				fdc->retry = 6;
				return(retrier(fdcu));
			}
		}
		fdc->state = IOCOMPLETE;
		fd->tohandle = timeout(fd_timeout, (caddr_t)fdcu, hz);
		return(0);	/* will return later */
	case IOCOMPLETE: /* IO DONE, post-analyze */
		untimeout(fd_timeout, (caddr_t)fdcu, fd->tohandle);

		if (fd_read_status(fdc, fd->fdsu))
		{
			if (fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			return retrier(fdcu);
  		}

		fdc->state = IOTIMEDOUT;

		/* FALLTHROUGH */

	case IOTIMEDOUT:
		isa_dmadone(bp->b_flags, bp->b_data+fd->skip,
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
		if (!format && fd->skip < bp->b_bcount - bp->b_resid)
		{
			/* set up next transfer */
			fdc->state = DOSEEK;
		}
		else
		{
			/* ALL DONE */
			fd->skip = 0;
			bufq_remove(&fdc->head, bp);
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
		/* XXX clear the fdc results from the last reset, if any. */
		{
			int i;
			for (i = 0; i < 4; i++)
				(void)fd_sense_int(fdc, &st0, &cyl);
		}

		if(fd_cmd(fdcu,
			  2, NE7CMD_RECAL, fdu,
			  0)) /* Recalibrate Function */
		{
			/* arrgl */
			fdc->retry = 6;
			return(retrier(fdcu));
		}
		fdc->state = RECALWAIT;
		return(0);	/* will return later */
	case RECALWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, (caddr_t)fdcu, hz / 8);
		fdc->state = RECALCOMPLETE;
		return(0);	/* will return later */
	case RECALCOMPLETE:
		do {
			/*
			 * See SEEKCOMPLETE for a comment on this:
			 */
			if (fd_sense_int(fdc, &st0, &cyl) == FD_NOT_VALID)
				return 0;
			if(fdc->fdct == FDC_NE765
			   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
				return 0; /* hope for a real intr */
		} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);
		if ((st0 & NE7_ST0_IC) != NE7_ST0_IC_NT || cyl != 0)
		{
			if(fdc->retry > 3)
				/*
				 * a recalibrate from beyond cylinder 77
				 * will "fail" due to the FDC limitations;
				 * since people used to complain much about
				 * the failure message, try not logging
				 * this one if it seems to be the first
				 * time in a line
				 */
				printf("fd%d: recal failed ST0 %b cyl %d\n",
				       fdu, st0, NE7_ST0BITS, cyl);
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
		printf("fdc%d: Unexpected FD int->", fdcu);
		if (fd_read_status(fdc, fd->fdsu) == 0)
			printf("FDC status :%lx %lx %lx %lx %lx %lx %lx   ",
			       fdc->status[0],
			       fdc->status[1],
			       fdc->status[2],
			       fdc->status[3],
			       fdc->status[4],
			       fdc->status[5],
			       fdc->status[6] );
		else
			printf("No status available   ");
		if (fd_sense_int(fdc, &st0, &cyl) != 0)
		{
			printf("[controller is dead now]\n");
			return(0);
		}
		printf("ST0 = %x, PCN = %x\n", st0, cyl);
		return(0);
	}
	/*XXX confusing: some branches return immediately, others end up here*/
	return(1); /* Come back immediatly to new state */
}

static int
retrier(fdcu)
	fdcu_t fdcu;
{
	fdc_p fdc = fdc_data + fdcu;
	register struct buf *bp;

	bp = bufq_first(&fdc->head);

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
					    (FDUNIT(minor(bp->b_dev))<<3)|RAW_PART);
			diskerr(bp, "fd", "hard error", LOG_PRINTF,
				fdc->fd->skip / DEV_BSIZE,
				(struct disklabel *)NULL);
			bp->b_dev = sav_b_dev;
			if (fdc->flags & FDC_STAT_VALID)
			{
				printf(
			" (ST0 %b ST1 %b ST2 %b cyl %ld hd %ld sec %ld)\n",
				       fdc->status[0], NE7_ST0BITS,
				       fdc->status[1], NE7_ST1BITS,
				       fdc->status[2], NE7_ST2BITS,
				       fdc->status[3], fdc->status[4],
				       fdc->status[5]);
			}
			else
				printf(" (No status)\n");
		}
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		bp->b_resid += bp->b_bcount - fdc->fd->skip;
		bufq_remove(&fdc->head, bp);
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
	/*
	 * keep the process from being swapped
	 */
	p->p_flag |= P_PHYSIO;
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
	bp->b_data = (caddr_t)finfo;

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

	if(rv == EWOULDBLOCK) {
		/* timed out */
		rv = EIO;
		biodone(bp);
	}
	if(bp->b_flags & B_ERROR)
		rv = bp->b_error;
	/*
	 * allow the process to be swapped
	 */
	p->p_flag &= ~P_PHYSIO;
	free(bp, M_TEMP);
	return rv;
}

/*
 * TODO: don't allocate buffer on stack.
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

		if (readdisklabel(dkmodpart(dev, RAW_PART), fdstrategy, dl)
		    == NULL)
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

		if ((error = setdisklabel((struct disklabel *)buffer, dl,
					  (u_long)0)) != 0)
			break;

		error = writedisklabel(dev, fdstrategy,
				       (struct disklabel *)buffer);
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


static fd_devsw_installed = 0;

static void 	fd_drvinit(void *notused )
{

	if( ! fd_devsw_installed ) {
		bdevsw_add_generic(BDEV_MAJOR,CDEV_MAJOR, &fd_bdevsw);
		fd_devsw_installed = 1;
	}
}

SYSINIT(fddev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,fd_drvinit,NULL)

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
