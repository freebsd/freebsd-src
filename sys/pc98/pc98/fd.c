/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
 *
 * Libretto PCMCIA floppy support by David Horwitt (dhorwitt@ucsd.edu)
 * aided by the Linux floppy driver modifications from David Bateman
 * (dbateman@eng.uts.edu.au).
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
 * $FreeBSD$
 *
 */

#include "opt_fdc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disklabel.h>
#include <sys/devicestat.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/syslog.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/clock.h>
#include <machine/ioctl_fd.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <isa/isavar.h>
#ifdef PC98
#include <pc98/pc98/pc98.h>
#include <pc98/pc98/pc98_machdep.h>
#include <pc98/pc98/epsonio.h>
#include <pc98/pc98/fdreg.h>
#include <isa/fdc.h>
#else
#include <isa/isareg.h>
#include <isa/fdreg.h>
#include <isa/fdc.h>
#include <isa/rtc.h>
#endif

/* misuse a flag to identify format operation */
#define B_FORMAT B_XXX

/* configuration flags */
#define FDC_PRETEND_D0	(1 << 0)	/* pretend drive 0 to be there */
#define FDC_NO_FIFO	(1 << 2)	/* do not enable FIFO  */

/* internally used only, not really from CMOS: */
#define RTCFDT_144M_PRETENDED	0x1000

/* error returns for fd_cmd() */
#define FD_FAILED -1
#define FD_NOT_VALID -2
#define FDC_ERRMAX	100	/* do not log more */

#ifdef PC98
#define NUMTYPES 12
#define NUMDENS  NUMTYPES
#else
#define NUMTYPES 17
#define NUMDENS  (NUMTYPES - 7)
#endif

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
#define FD_640          9
#define FD_1232         10

#ifdef PC98
#define FD_1280         11
#define FD_1476         12

#define FDT_NONE	0 /* none present */
#define FDT_12M 	1 /* 1M/640K FDD */
#define FDT_144M	2 /* 1.44M/1M/640K FDD */
#else
#define FD_1480in5_25   11
#define FD_1440in5_25   12
#define FD_820in5_25    13
#define FD_800in5_25    14
#define FD_720in5_25    15
#define FD_360in5_25    16
#define FD_640in5_25    17
#endif


static struct fd_type fd_types[NUMTYPES] =
{
#ifdef PC98
{ 21,2,0xFF,0x04,82,3444,1,2,2,0x0C,2 }, /* 1.72M in 3mode */
{ 18,2,0xFF,0x1B,82,2952,1,2,2,0x54,1 }, /* 1.48M in 3mode */
{ 18,2,0xFF,0x1B,80,2880,1,2,2,0x54,1 }, /* 1.44M in 3mode */
{ 15,2,0xFF,0x1B,80,2400,1,0,2,0x54,1 }, /* 1.2M */
{ 10,2,0xFF,0x10,82,1640,1,1,2,0x30,1 }, /* 820K */
{ 10,2,0xFF,0x10,80,1600,1,1,2,0x30,1 }, /* 800K */
{  9,2,0xFF,0x20,80,1440,1,1,2,0x50,1 }, /* 720K */
{  9,2,0xFF,0x20,40, 720,2,1,2,0x50,1 }, /* 360K */
{  8,2,0xFF,0x2A,80,1280,1,1,2,0x50,1 }, /* 640K */
{  8,3,0xFF,0x35,77,1232,1,0,2,0x74,1 }, /* 1.23M 1024/sec */

{  8,3,0xFF,0x35,80,1280,1,0,2,0x74,1 }, /* 1.28M 1024/sec */
{  9,3,0xFF,0x35,82,1476,1,0,2,0x47,1 }, /* 1.48M 1024/sec 9sec */
#if 0
{ 10,3,0xFF,0x1B,82,1640,1,2,2,0x54,1 }, /* 1.64M in 3mode - Reserve */
#endif
#else
{ 21,2,0xFF,0x04,82,3444,1,FDC_500KBPS,2,0x0C,2 }, /* 1.72M in HD 3.5in */
{ 18,2,0xFF,0x1B,82,2952,1,FDC_500KBPS,2,0x6C,1 }, /* 1.48M in HD 3.5in */
{ 18,2,0xFF,0x1B,80,2880,1,FDC_500KBPS,2,0x6C,1 }, /* 1.44M in HD 3.5in */
{ 15,2,0xFF,0x1B,80,2400,1,FDC_500KBPS,2,0x54,1 }, /*  1.2M in HD 5.25/3.5 */
{ 10,2,0xFF,0x10,82,1640,1,FDC_250KBPS,2,0x2E,1 }, /*  820K in HD 3.5in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_250KBPS,2,0x2E,1 }, /*  800K in HD 3.5in */
{  9,2,0xFF,0x20,80,1440,1,FDC_250KBPS,2,0x50,1 }, /*  720K in HD 3.5in */
{  9,2,0xFF,0x2A,40, 720,1,FDC_250KBPS,2,0x50,1 }, /*  360K in DD 5.25in */
{  8,2,0xFF,0x2A,80,1280,1,FDC_250KBPS,2,0x50,1 }, /*  640K in DD 5.25in */
{  8,3,0xFF,0x35,77,1232,1,FDC_500KBPS,2,0x74,1 }, /* 1.23M in HD 5.25in */

{ 18,2,0xFF,0x02,82,2952,1,FDC_500KBPS,2,0x02,2 }, /* 1.48M in HD 5.25in */
{ 18,2,0xFF,0x02,80,2880,1,FDC_500KBPS,2,0x02,2 }, /* 1.44M in HD 5.25in */
{ 10,2,0xFF,0x10,82,1640,1,FDC_300KBPS,2,0x2E,1 }, /*  820K in HD 5.25in */
{ 10,2,0xFF,0x10,80,1600,1,FDC_300KBPS,2,0x2E,1 }, /*  800K in HD 5.25in */
{  9,2,0xFF,0x20,80,1440,1,FDC_300KBPS,2,0x50,1 }, /*  720K in HD 5.25in */
{  9,2,0xFF,0x23,40, 720,2,FDC_300KBPS,2,0x50,1 }, /*  360K in HD 5.25in */
{  8,2,0xFF,0x2A,80,1280,1,FDC_300KBPS,2,0x50,1 }, /*  640K in HD 5.25in */
#endif
};

#ifdef PC98
#define DRVS_PER_CTLR 4		/* 4 floppies */
#else
#define DRVS_PER_CTLR 2		/* 2 floppies */
#endif

/***********************************************************************\
* Per controller structure.						*
\***********************************************************************/
static devclass_t fdc_devclass;

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
#define FD_NO_TRACK -2
	int	track;		/* where we think the head is */
	int	options;	/* user configurable options, see ioctl_fd.h */
	struct	callout_handle toffhandle;
	struct	callout_handle tohandle;
	struct	devstat device_stats;
#ifdef PC98
	int	pc98_trans;
#endif
	device_t dev;
	fdu_t	fdu;
};

struct fdc_ivars {
	int	fdunit;
};
static devclass_t fd_devclass;

#ifdef EPSON_NRDISK
typedef unsigned int	nrd_t;

#define	P_NRD_ADDRH	0xc24
#define	P_NRD_ADDRM	0xc22
#define	P_NRD_ADDRL	0xc20
#define	P_NRD_CHECK	0xc20
#define	P_NRD_DATA	0xc26
#define	P_NRD_LED	0xc36
#define	B_NRD_CHK	0x80
#define	B_NRD_LED	0x40
#define	A_NRD_INFO	0x2
#define	A_NRD_BASE	0x400
#define NRD_STATUS	0x0
#define NRD_ST0_HD	0x04

static fdu_t nrdu=-1;
static int nrdsec=0;
static nrd_t nrdblkn=0;
static nrd_t nrdaddr=0x0;

#define	nrd_check_ready()	({		\
	(epson_inb(P_NRD_CHECK) & B_NRD_CHK) ? 0 : 1;	\
	})
#define	nrd_LED_on()	epson_outb(P_NRD_LED, B_NRD_LED)
#define	nrd_LED_off()	epson_outb(P_NRD_LED, ~B_NRD_LED)
#define	nrd_trac()	((int)(nrd_info(nrdaddr) & 0xff))
#define	nrd_head()	((int)((nrd_info(nrdaddr) >> 8) & 0xff))
#define	nrd_sec()	((int)(nrd_info(nrdaddr + 2) & 0xff))
#define	nrd_secsize()	((int)((nrd_info(A_NRD_INFO) >> 8) & 0xff))
#define	nrd_addrset(p)	nrd_addr((nrd_t)((nrd_t)p+A_NRD_BASE))

static inline void
nrd_addr(addr)
	nrd_t	addr;
{
	epson_outb(P_NRD_ADDRH, (u_char)((addr >> 16) & 0x1f));
	epson_outb(P_NRD_ADDRM, (u_char)((addr >> 8) & 0xff));
	epson_outb(P_NRD_ADDRL, (u_char)(addr & 0xff));
}

static inline u_short
nrd_info(addr)
	nrd_t	addr;
{
	u_short	tmp;

	nrd_addr(addr);
	outb(0x43f, 0x42);
	tmp = (short)inw(P_NRD_DATA);
	outb(0x43f, 0x40);
	return ((u_short)tmp);
}
#endif /* EPSON_NRDISK */

/***********************************************************************\
* Throughout this file the following conventions will be used:		*
* fd is a pointer to the fd_data struct for the drive in question	*
* fdc is a pointer to the fdc_data struct for the controller		*
* fdu is the floppy drive unit number					*
* fdcu is the floppy controller unit number				*
* fdsu is the floppy drive unit number on that controller. (sub-unit)	*
\***********************************************************************/

/* needed for ft driver, thus exported */
int in_fdc(struct fdc_data *);
int out_fdc(struct fdc_data *, int);

/* internal functions */
static	void fdc_intr(void *);
static void set_motor(struct fdc_data *, int, int);
#  define TURNON 1
#  define TURNOFF 0
static timeout_t fd_turnoff;
static timeout_t fd_motor_on;
static void fd_turnon(struct fd_data *);
static void fdc_reset(fdc_p);
static int fd_in(struct fdc_data *, int *);
static void fdstart(struct fdc_data *);
static timeout_t fd_iotimeout;
static timeout_t fd_pseudointr;
static int fdstate(struct fdc_data *);
static int retrier(struct fdc_data *);
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
#define	RESETCOMPLETE	12
#define PIOREAD		13

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
"IOTIMEDOUT",
"RESETCOMPLETE",
"PIOREAD",
};

/* CAUTION: fd_debug causes huge amounts of logging output */
static int volatile fd_debug = 0;
#define TRACE0(arg) if(fd_debug) printf(arg)
#define TRACE1(arg1, arg2) if(fd_debug) printf(arg1, arg2)
#else /* FDC_DEBUG */
#define TRACE0(arg)
#define TRACE1(arg1, arg2)
#endif /* FDC_DEBUG */

static void
fdout_wr(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->portt, fdc->porth, FDOUT+fdc->port_off, v);
}

static u_int8_t
fdsts_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDSTS+fdc->port_off);
}

static void
fddata_wr(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->portt, fdc->porth, FDDATA+fdc->port_off, v);
}

static u_int8_t
fddata_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDDATA+fdc->port_off);
}

#ifndef PC98
static void
fdctl_wr(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->ctlt, fdc->ctlh, 0, v);
}
#endif

#if 0

static u_int8_t
fdin_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDIN);
}

#endif

#ifdef FDC_YE
#if NCARD > 0
#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/driver.h>
#include <pccard/slot.h>

/*
 *	PC-Card (PCMCIA) specific code.
 */
static int yeinit(struct pccard_devinfo *);		/* init device */
static void yeunload(struct pccard_devinfo *); 		/* Disable driver */
static int yeintr(struct pccard_devinfo *); 		/* Interrupt handler */

PCCARD_MODULE(fdc, yeinit, yeunload, yeintr, 0, bio_imask);

/*
 *	Initialize the device - called from Slot manager.
 */
static int yeinit(struct pccard_devinfo *devi)
{
	fdc_p fdc = &fdc_data[devi->isahd.id_unit];

	fdc->baseport = devi->isahd.id_iobase;
	/*
	 * reset controller
	 */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);

	/*
	 * wire into system
	 */
	if (yeattach(&devi->isahd) == 0)
		return(ENXIO);

	return(0);
}

/*
 *	yeunload - unload the driver and clear the table.
 *	XXX TODO:
 *	This is usually called when the card is ejected, but
 *	can be caused by a modunload of a controller driver.
 *	The idea is to reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
static void yeunload(struct pccard_devinfo *devi)
{
	if (fd_data[devi->isahd.id_unit].type == NO_TYPE)
		return;

	/*
	 * this prevents Fdopen() and fdstrategy() from attempting
	 * to access unloaded controller
	 */
	fd_data[devi->isahd.id_unit].type = NO_TYPE;

	printf("fdc%d: unload\n", devi->isahd.id_unit);
}

/*
 *	yeintr - Shared interrupt called from
 *	front end of PC-Card handler.
 */
static int yeintr(struct pccard_devinfo *devi)
{
	fdintr((fdcu_t)devi->isahd.id_unit);
	return(1);
}
#endif /* NCARD > 0 */
#endif /* FDC_YE */

static	d_open_t	Fdopen;	/* NOTE, not fdopen */
static	d_close_t	fdclose;
static	d_ioctl_t	fdioctl;
static	d_strategy_t	fdstrategy;

#define CDEV_MAJOR 9
#define BDEV_MAJOR 2

static struct cdevsw fd_cdevsw = {
	/* open */	Fdopen,
	/* close */	fdclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	fdioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	fdstrategy,
	/* name */	"fd",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* bmaj */	BDEV_MAJOR
};

static int
fdc_err(struct fdc_data *fdc, const char *s)
{
	fdc->fdc_errs++;
	if (s) {
		if (fdc->fdc_errs < FDC_ERRMAX)
			device_printf(fdc->fdc_dev, "%s", s);
		else if (fdc->fdc_errs == FDC_ERRMAX)
			device_printf(fdc->fdc_dev, "too many errors, not "
						    "logging any more\n");
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
fd_cmd(struct fdc_data *fdc, int n_out, ...)
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
		if (out_fdc(fdc, va_arg(ap, int)) < 0)
		{
			char msg[50];
			snprintf(msg, sizeof(msg),
				"cmd %x failed at out byte %d of %d\n",
				cmd, n + 1, n_out);
			return fdc_err(fdc, msg);
		}
	}
	n_in = va_arg(ap, int);
	for (n = 0; n < n_in; n++)
	{
		int *ptr = va_arg(ap, int *);
		if (fd_in(fdc, ptr) < 0)
		{
			char msg[50];
			snprintf(msg, sizeof(msg),
				"cmd %02x failed at in byte %d of %d\n",
				cmd, n + 1, n_in);
			return fdc_err(fdc, msg);
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
		
		if (out_fdc(fdc, I8207X_CONFIGURE) < 0)
			return fdc_err(fdc, "Enable FIFO failed\n");
		
		/* If command is invalid, return */
		j = 100000;
		while ((i = fdsts_rd(fdc) & (NE7_DIO | NE7_RQM))
		       != NE7_RQM && j-- > 0)
			if (i == (NE7_DIO | NE7_RQM)) {
				fdc_reset(fdc);
				return FD_FAILED;
			}
		if (j<0 || 
		    fd_cmd(fdc, 3,
			   0, (fifo_threshold - 1) & 0xf, 0, 0) < 0) {
			fdc_reset(fdc);
			return fdc_err(fdc, "Enable FIFO failed\n");
		}
		fdc->flags |= FDC_HAS_FIFO;
		return 0;
	}
	if (fd_cmd(fdc, 4,
		   I8207X_CONFIGURE, 0, (fifo_threshold - 1) & 0xf, 0, 0) < 0)
		return fdc_err(fdc, "Re-enable FIFO failed\n");
	return 0;
}

static int
fd_sense_drive_status(fdc_p fdc, int *st3p)
{
	int st3;

	if (fd_cmd(fdc, 2, NE7CMD_SENSED, fdc->fdu, 1, &st3))
	{
		return fdc_err(fdc, "Sense Drive Status failed\n");
	}
	if (st3p)
		*st3p = st3;

	return 0;
}

static int
fd_sense_int(fdc_p fdc, int *st0p, int *cylp)
{
	int cyl, st0, ret;

#ifdef EPSON_NRDISK
	if (fdc->fdu == nrdu) {
		if (fdc->fd->track >= 0) nrdaddr = (fdc->fd->track + 1) * 8;
		else nrdaddr = 0x0;
		*st0p = nrd_head() ? NRD_ST0_HD : NRD_STATUS;
		*cylp = nrd_trac();
	}
	else {
#endif /* EPSON_NRDISK */
	ret = fd_cmd(fdc, 1, NE7CMD_SENSEI, 1, &st0);
	if (ret) {
		(void)fdc_err(fdc,
			      "sense intr err reading stat reg 0\n");
		return ret;
	}

	if (st0p)
		*st0p = st0;

	if ((st0 & NE7_ST0_IC) == NE7_ST0_IC_IV) {
		/*
		 * There doesn't seem to have been an interrupt.
		 */
		return FD_NOT_VALID;
	}

	if (fd_in(fdc, &cyl) < 0) {
		return fdc_err(fdc, "can't get cyl num\n");
	}

	if (cylp)
		*cylp = cyl;

#ifdef EPSON_NRDISK
	}
#endif /* EPSON_NRDISK */
	return 0;
}


static int
fd_read_status(fdc_p fdc, int fdsu)
{
	int i, ret;

	for (i = 0; i < 7; i++) {
		/*
		 * XXX types are poorly chosen.  Only bytes can by read
		 * from the hardware, but fdc->status[] wants u_ints and
		 * fd_in() gives ints.
		 */
		int status;

#ifdef EPSON_NRDISK
		if (fdc->fdu == nrdu) {
			switch (i) {
				case 0:	fdc->status[i] = nrd_head()
					? NRD_ST0_HD : NRD_STATUS; break;
				case 1:	fdc->status[i] = NRD_STATUS; break;
				case 2:	fdc->status[i] = NRD_STATUS; break;
				case 3:	fdc->status[i] = nrd_trac(); break;
				case 4:	fdc->status[i] = nrd_head(); break;
				case 5:	fdc->status[i] = nrdsec; break;
				case 6:	fdc->status[i] = nrd_secsize(); break;
			}
			ret = 0;
		}
		else {
#endif /* EPSON_NRDISK */
		ret = fd_in(fdc, &status);
		fdc->status[i] = status;
		if (ret != 0)
			break;
#ifdef EPSON_NRDISK
		}
#endif /* EPSON_NRDISK */
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
#ifdef PC98
static int pc98_trans = 0; /* 0 : HD , 1 : DD , 2 : 1.44 */
static int pc98_trans_prev = 0;

static void set_density(fdc_p fdc)
{
	/* always motor on */
	outb(IO_FDPORT, (pc98_trans != 1 ? FDP_FDDEXC : 0) | FDP_PORTEXC);
	DELAY(100);
	fdout_wr(fdc, FDO_RST | FDO_DMAE);
	/* in the case of note W, always inhibit 100ms timer */
}

static int pc98_fd_check_ready(fdu_t fdu)
{
	fd_p fd = devclass_get_softc(fd_devclass, fdu);
	struct fdc_data *fdc = fd->fdc;
	int retry = 0;

#ifdef EPSON_NRDISK
	if (fdu == nrdu) {
		if (nrd_check_ready()) return 0;
		else return -1;
	}
#endif
	while (retry++ < 30000) {
		set_motor(fdc, fd->fdsu, TURNON);
		out_fdc(fdc, NE7CMD_SENSED); /* Sense Drive Status */
		DELAY(100);
		out_fdc(fdc, fdu); /* Drive number */
		DELAY(100);
		if ((in_fdc(fdc) & NE7_ST3_RD)){
			fdout_wr(fdc, FDO_DMAE | FDO_MTON);
			DELAY(10);
			return 0;
		}
	}
	return -1;
}
#endif

static int
fdc_alloc_resources(struct fdc_data *fdc)
{
	device_t dev;
	int ispnp;

	dev = fdc->fdc_dev;
	ispnp = fdc->fdc_ispnp;
	fdc->rid_ioport = fdc->rid_irq = fdc->rid_drq = 0;
	fdc->res_ioport = fdc->res_irq = fdc->res_drq = 0;

#ifdef PC98
	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
					     &fdc->rid_ioport, 0ul, ~0ul, 
					     ispnp ? 1 : IO_FDCSIZE,
					     RF_ACTIVE);
#else
	/*
	 * We don't just use an 8 port range (e.g. 0x3f0-0x3f7) since that
	 * covers an IDE control register at 0x3f6.
	 * Isn't PC hardware wonderful.
	 */
	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
					     &fdc->rid_ioport, 0ul, ~0ul, 
					     ispnp ? 1 : 6, RF_ACTIVE);
#endif
	if (fdc->res_ioport == 0) {
		device_printf(dev, "cannot reserve I/O port range\n");
		return ENXIO;
	}
	fdc->portt = rman_get_bustag(fdc->res_ioport);
	fdc->porth = rman_get_bushandle(fdc->res_ioport);

#ifndef PC98
	/*
	 * Some BIOSen report the device at 0x3f2-0x3f5,0x3f7 and some at
	 * 0x3f0-0x3f5,0x3f7. We detect the former by checking the size
	 * and adjust the port address accordingly.
	 */
	if (bus_get_resource_count(dev, SYS_RES_IOPORT, 0) == 4)
		fdc->port_off = -2;

	/*
	 * Register the control port range as rid 1 if it isn't there
	 * already. Most PnP BIOSen will have already done this but
	 * non-PnP configurations don't.
	 *
	 * And some (!!) report 0x3f2-0x3f5 and completely leave out the
	 * control register!  It seems that some non-antique controller chips
	 * have a different method of programming the transfer speed which
	 * doesn't require the control register, but it's mighty bogus as the
	 * chip still responds to the address for the control register.
	 */
	if (bus_get_resource_count(dev, SYS_RES_IOPORT, 1) == 0) {
		u_long ctlstart;

		/* Find the control port, usually 0x3f7 */
		ctlstart = rman_get_start(fdc->res_ioport) + fdc->port_off + 7;

		bus_set_resource(dev, SYS_RES_IOPORT, 1, ctlstart, 1);
	}

	/*
	 * Now (finally!) allocate the control port.
	 */
	fdc->rid_ctl = 1;
	fdc->res_ctl = bus_alloc_resource(dev, SYS_RES_IOPORT, &fdc->rid_ctl,
					  0ul, ~0ul, 1, RF_ACTIVE);
	if (fdc->res_ctl == 0) {
		device_printf(dev, "cannot reserve control I/O port range\n");
		return ENXIO;
	}
	fdc->ctlt = rman_get_bustag(fdc->res_ctl);
	fdc->ctlh = rman_get_bushandle(fdc->res_ctl);
#endif

	fdc->res_irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					  &fdc->rid_irq, 0ul, ~0ul, 1, 
					  RF_ACTIVE);
	if (fdc->res_irq == 0) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return ENXIO;
	}
	fdc->res_drq = bus_alloc_resource(dev, SYS_RES_DRQ,
					  &fdc->rid_drq, 0ul, ~0ul, 1, 
					  RF_ACTIVE);
	if (fdc->res_drq == 0) {
		device_printf(dev, "cannot reserve DMA request line\n");
		return ENXIO;
	}
	fdc->dmachan = fdc->res_drq->r_start;

	return 0;
}

static void
fdc_release_resources(struct fdc_data *fdc)
{
	device_t dev;

	dev = fdc->fdc_dev;
	if (fdc->res_irq != 0) {
		bus_deactivate_resource(dev, SYS_RES_IRQ, fdc->rid_irq,
					fdc->res_irq);
		bus_release_resource(dev, SYS_RES_IRQ, fdc->rid_irq,
				     fdc->res_irq);
	}
#ifndef PC98
	if (fdc->res_ctl != 0) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
					fdc->res_ctl);
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
				     fdc->res_ctl);
	}
#endif
	if (fdc->res_ioport != 0) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT, fdc->rid_ioport,
					fdc->res_ioport);
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ioport,
				     fdc->res_ioport);
	}
	if (fdc->res_drq != 0) {
		bus_deactivate_resource(dev, SYS_RES_DRQ, fdc->rid_drq,
					fdc->res_drq);
		bus_release_resource(dev, SYS_RES_DRQ, fdc->rid_drq,
				     fdc->res_drq);
	}
}

/****************************************************************************/
/*                      autoconfiguration stuff                             */
/****************************************************************************/

static struct isa_pnp_id fdc_ids[] = {
	{0x0007d041, "PC standard floppy disk controller"}, /* PNP0700 */
	{0x0107d041, "Standard floppy controller supporting MS Device Bay Spec"}, /* PNP0701 */
	{0}
};

static int
fdc_read_ivar(device_t dev, device_t child, int which, u_long *result)
{
	struct fdc_ivars *ivars = device_get_ivars(child);

	switch (which) {
	case FDC_IVAR_FDUNIT:
		*result = ivars->fdunit;
		break;
	default:
		return ENOENT;
	}
	return 0;
}

/*
 * fdc controller section.
 */
static int
fdc_probe(device_t dev)
{
#ifdef PC98
	int	error;
#else
	int	error, ic_type;
#endif
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	bzero(fdc, sizeof *fdc);
	fdc->fdc_dev = dev;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == ENXIO)
		return ENXIO;
	fdc->fdc_ispnp = (error == 0);

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_alloc_resources(fdc);
	if (error)
		goto out;

#ifndef PC98
	/* First - lets reset the floppy controller */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);
#endif

	/* see if it can handle a command */
#ifdef PC98
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(4, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}
#else
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}
#endif

#ifndef PC98
	if (fd_cmd(fdc, 1, NE7CMD_VERSION, 1, &ic_type) == 0) {
		ic_type = (u_char)ic_type;
		switch (ic_type) {
		case 0x80:
			device_set_desc(dev, "NEC 765 or clone");
			fdc->fdct = FDC_NE765;
			break;
		case 0x81:
			device_set_desc(dev, "Intel 82077 or clone");
			fdc->fdct = FDC_I82077;
			break;
		case 0x90:
			device_set_desc(dev, "NEC 72065B or clone");
			fdc->fdct = FDC_NE72065;
			break;
		default:
			device_set_desc(dev, "generic floppy controller");
			fdc->fdct = FDC_UNKNOWN;
			break;
		}
	}
#endif

out:
	fdc_release_resources(fdc);
	return (error);
}

/*
 * Add a child device to the fdc controller.  It will then be probed etc.
 */
static void
fdc_add_child(device_t dev, const char *name, int unit)
{
	int	disabled;
	struct fdc_ivars *ivar;
	device_t child;

	ivar = malloc(sizeof *ivar, M_DEVBUF /* XXX */, M_NOWAIT);
	if (ivar == NULL)
		return;
	bzero(ivar, sizeof *ivar);
	if (resource_int_value(name, unit, "drive", &ivar->fdunit) != 0)
		ivar->fdunit = 0;
	child = device_add_child(dev, name, unit);
	if (child == NULL)
		return;
	device_set_ivars(child, ivar);
	if (resource_int_value(name, unit, "disabled", &disabled) == 0
	    && disabled != 0)
		device_disable(child);
}

static int
fdc_attach(device_t dev)
{
	struct	fdc_data *fdc;
	int	i, error;

	fdc = device_get_softc(dev);
	error = fdc_alloc_resources(fdc);
	if (error) {
		device_printf(dev, "cannot re-aquire resources\n");
		return error;
	}
	error = BUS_SETUP_INTR(device_get_parent(dev), dev, fdc->res_irq,
			       INTR_TYPE_BIO, fdc_intr, fdc, &fdc->fdc_intr);
	if (error) {
		device_printf(dev, "cannot setup interrupt\n");
		return error;
	}
	fdc->fdcu = device_get_unit(dev);
	fdc->flags |= FDC_ATTACHED;

	/* Acquire the DMA channel forever, The driver will do the rest */
				/* XXX should integrate with rman */
	isa_dma_acquire(fdc->dmachan);
	isa_dmainit(fdc->dmachan, 128 << 3 /* XXX max secsize */);
	fdc->state = DEVIDLE;

#ifdef PC98
	/* reset controller, turn motor off, clear fdout mirror reg */
	fdc_reset(fdc);
#else
	/* reset controller, turn motor off, clear fdout mirror reg */
	fdout_wr(fdc, ((fdc->fdout = 0)));
#endif
	bufq_init(&fdc->head);

	/*
	 * Probe and attach any children.  We should probably detect
	 * devices from the BIOS unless overridden.
	 */
	for (i = resource_query_string(-1, "at", device_get_nameunit(dev));
	     i != -1;
	     i = resource_query_string(i, "at", device_get_nameunit(dev)))
		fdc_add_child(dev, resource_query_name(i),
			       resource_query_unit(i));

	return (bus_generic_attach(dev));
}

static int
fdc_print_child(device_t me, device_t child)
{
	int retval = 0;

	retval += bus_print_child_header(me, child);
	retval += printf(" on %s drive %d\n", device_get_nameunit(me),
	       fdc_get_fdunit(child));
	
	return (retval);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_probe),
	DEVMETHOD(device_attach,	fdc_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdc_print_child),
	DEVMETHOD(bus_read_ivar,	fdc_read_ivar),
	/* Our children never use any other bus interface methods. */

	{ 0, 0 }
};

static driver_t fdc_driver = {
	"fdc",
	fdc_methods,
	sizeof(struct fdc_data)
};

DRIVER_MODULE(fdc, isa, fdc_driver, fdc_devclass, 0, 0);

/******************************************************************/
/*
 * devices attached to the controller section.  
 */
static int
fd_probe(device_t dev)
{
#ifdef PC98
	u_int	fdt;
#else
	int	i;
	u_int	fdt, st0, st3;
#endif
	struct	fd_data *fd;
	struct	fdc_data *fdc;
	fdsu_t	fdsu;
#ifndef PC98
	static int fd_fifo = 0;
#endif

	fdsu = *(int *)device_get_ivars(dev); /* xxx cheat a bit... */
	fd = device_get_softc(dev);
	fdc = device_get_softc(device_get_parent(dev));

	bzero(fd, sizeof *fd);
	fd->dev = dev;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->fdu = device_get_unit(dev);

#ifdef PC98
	/* look up what bios thinks we have */
	switch (fd->fdu) {
	case 0: case 1: case 2: case 3:
		if ((PC98_SYSTEM_PARAMETER(0x5ae) >> fd->fdu) & 0x01)
			fdt = FDT_144M;
#ifdef EPSON_NRDISK
		else if ((PC98_SYSTEM_PARAMETER(0x55c) >> fd->fdu) & 0x01) {
			fdt = FDT_12M;
			switch (epson_machine_id) {
			case 0x20: case 0x27:
			    if ((PC98_SYSTEM_PARAMETER(0x488) >> fd->fdu) & 0x01) {
				if (nrd_check_ready()) {
				    nrd_LED_on();
				    nrdu = fd->fdu;
				} else {
				    fdt = FDT_NONE;
				}
			    }
			}
		}
#else /* !EPSON_NRDISK */
		else if ((PC98_SYSTEM_PARAMETER(0x55c) >> fd->fdu) & 0x01) {
			fdt = FDT_12M;
			switch (epson_machine_id) {
			case 0x20: case 0x27:
			    if ((PC98_SYSTEM_PARAMETER(0x488) >> fd->fdu) & 0x01)
				fdt = FDT_NONE;
			}
		}
#endif /* EPSON_NRDISK */
		else
			fdt = FDT_NONE;
		break;
	default:
		fdt = FDT_NONE;
		break;
	}
#else
#ifdef __i386__
	/* look up what bios thinks we have */
	switch (fd->fdu) {
	case 0:
		if (device_get_flags(fdc->fdc_dev) & FDC_PRETEND_D0)
			fdt = RTCFDT_144M | RTCFDT_144M_PRETENDED;
		else
			fdt = (rtcin(RTC_FDISKETTE) & 0xf0);
		break;
	case 1:
		fdt = ((rtcin(RTC_FDISKETTE) << 4) & 0xf0);
		break;
	default:
		fdt = RTCFDT_NONE;
		break;
	}
#else
	fdt = RTCFDT_144M;	/* XXX probably */
#endif
#endif

	/* is there a unit? */
#ifdef PC98
	if (fdt == FDT_NONE)
		return (ENXIO);
#else
	if (fdt == RTCFDT_NONE)
		return (ENXIO);
#endif

#ifndef PC98
	/* select it */
	set_motor(fdc, fdsu, TURNON);
	DELAY(1000000);	/* 1 sec */

	/* XXX This doesn't work before the first set_motor() */
	if (fd_fifo == 0 && fdc->fdct != FDC_NE765 && fdc->fdct != FDC_UNKNOWN
	    && (device_get_flags(fdc->fdc_dev) & FDC_NO_FIFO) == 0
	    && enable_fifo(fdc) == 0) {
		device_printf(device_get_parent(dev),
		    "FIFO enabled, %d bytes threshold\n", fifo_threshold);
	}
	fd_fifo = 1;

	if ((fd_cmd(fdc, 2, NE7CMD_SENSED, fdsu, 1, &st3) == 0)
	    && (st3 & NE7_ST3_T0)) {
		/* if at track 0, first seek inwards */
		/* seek some steps: */
		fd_cmd(fdc, 3, NE7CMD_SEEK, fdsu, 10, 0);
		DELAY(300000); /* ...wait a moment... */
		fd_sense_int(fdc, 0, 0); /* make ctrlr happy */
	}

	/* If we're at track 0 first seek inwards. */
	if ((fd_sense_drive_status(fdc, &st3) == 0) && (st3 & NE7_ST3_T0)) {
		/* Seek some steps... */
		if (fd_cmd(fdc, 3, NE7CMD_SEEK, fdsu, 10, 0) == 0) {
			/* ...wait a moment... */
			DELAY(300000);
			/* make ctrlr happy: */
			fd_sense_int(fdc, 0, 0);
		}
	}

	for (i = 0; i < 2; i++) {
		/*
		 * we must recalibrate twice, just in case the
		 * heads have been beyond cylinder 76, since most
		 * FDCs still barf when attempting to recalibrate
		 * more than 77 steps
		 */
		/* go back to 0: */
		if (fd_cmd(fdc, 2, NE7CMD_RECAL, fdsu, 0) == 0) {
			/* a second being enough for full stroke seek*/
			DELAY(i == 0 ? 1000000 : 300000);

			/* anything responding? */
			if (fd_sense_int(fdc, &st0, 0) == 0 &&
			    (st0 & NE7_ST0_EC) == 0)
				break; /* already probed succesfully */
		}
	}

	set_motor(fdc, fdsu, TURNOFF);

	if (st0 & NE7_ST0_EC) /* no track 0 -> no drive present */
		return (ENXIO);
#endif /* PC98 */

	fd->track = FD_NO_TRACK;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->options = 0;
	callout_handle_init(&fd->toffhandle);
	callout_handle_init(&fd->tohandle);

#ifdef PC98
	switch (fdt) {
	case FDT_144M:
		/* Check 3mode I/F */
		fd->pc98_trans = 0;
		outb(0x4be, (fd->fdu << 5) | 0x10);
		if (!(inb(0x4be) & 0x01)) {
			device_set_desc(dev, "1.44M FDD");
			fd->type = FD_1440;
			break;
		}

		printf("Warning: can't control 3mode I/F, "
			"fallback to 2mode.\n"
			"fd%d: ", fd->fdu);
		/* FALLTHROUGH */
	case FDT_12M:
#ifdef EPSON_NRDISK
		if (fd->fdu == nrdu) {
			device_set_desc(dev, "EPSON RAM DRIVE");
			nrd_LED_off();
		} else
#endif
			device_set_desc(dev, "1M/640K FDD");
		fd->type = FD_1200;
		fd->pc98_trans = 0;
		break;
	default:
		return (ENXIO);
	}
#else
	switch (fdt) {
	case RTCFDT_12M:
		device_set_desc(dev, "1200-KB 5.25\" drive");
		fd->type = FD_1200;
		break;
	case RTCFDT_144M | RTCFDT_144M_PRETENDED:
		device_set_desc(dev, "config-pretended 1440-MB 3.5\" drive");
		fdt = RTCFDT_144M;
		fd->type = FD_1440;
	case RTCFDT_144M:
		device_set_desc(dev, "1440-KB 3.5\" drive");
		fd->type = FD_1440;
		break;
	case RTCFDT_288M:
	case RTCFDT_288M_1:
		device_set_desc(dev, "2880-KB 3.5\" drive (in 1440-KB mode)");
		fd->type = FD_1440;
		break;
	case RTCFDT_360K:
		device_set_desc(dev, "360-KB 5.25\" drive");
		fd->type = FD_360;
		break;
	case RTCFDT_720K:
		printf("720-KB 3.5\" drive");
		fd->type = FD_720;
		break;
	default:
		return (ENXIO);
	}
#endif
	return (0);
}

static int
fd_attach(device_t dev)
{
	struct	fd_data *fd;
#if 0
	int	i;
	int	mynor;
	int	typemynor;
	int	typesize;
#endif
	static int cdevsw_add_done = 0;

	fd = device_get_softc(dev);

	if (!cdevsw_add_done) {
		cdevsw_add(&fd_cdevsw);	/* XXX */
		cdevsw_add_done++;
	}
	make_dev(&fd_cdevsw, (fd->fdu << 6),
		UID_ROOT, GID_OPERATOR, 0640, "rfd%d", fd->fdu);

#if 0
	/* Other make_dev() go here. */
#endif

	/*
	 * Export the drive to the devstat interface.
	 */
	devstat_add_entry(&fd->device_stats, device_get_name(dev), 
			  device_get_unit(dev), 512, DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_FLOPPY | DEVSTAT_TYPE_IF_OTHER,
			  DEVSTAT_PRIORITY_FD);
	return (0);
}

static device_method_t fd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fd_probe),
	DEVMETHOD(device_attach,	fd_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend), /* XXX */
	DEVMETHOD(device_resume,	bus_generic_resume), /* XXX */

	{ 0, 0 }
};

static driver_t fd_driver = {
	"fd",
	fd_methods,
	sizeof(struct fd_data)
};

DRIVER_MODULE(fd, fdc, fd_driver, fd_devclass, 0, 0);

/******************************************************************/

#ifdef FDC_YE
/*
 * this is a subset of fdattach() optimized for the Y-E Data
 * PCMCIA floppy drive.
 */
static int yeattach(struct isa_device *dev)
{
	fdcu_t  fdcu = dev->id_unit;
	fdc_p   fdc = fdc_data + fdcu;
	fdsu_t  fdsu = 0;               /* assume 1 drive per YE controller */
	fdu_t   fdu;
	fd_p    fd;
	int     st0, st3, i;
	fdc->fdcu = fdcu;
	/*
	 * the FDC_NODMA flag is used to to indicate special PIO is used
	 * instead of DMA
	 */
	fdc->flags = FDC_ATTACHED|FDC_NODMA;
	fdc->state = DEVIDLE;
	/* reset controller, turn motor off, clear fdout mirror reg */
	fdout_wr(fdc, ((fdc->fdout = 0)));
	bufq_init(&fdc->head);
	/*
	 * assume 2 drives/ "normal" controller
	 */
	fdu = fdcu * 2;
	if (fdu >= NFD) {
		printf("fdu %d >= NFD\n",fdu);
		return(0);
	};
	fd = &fd_data[fdu];

	set_motor(fdcu, fdsu, TURNON);
	DELAY(1000000); /* 1 sec */
	fdc->fdct = FDC_NE765;

	if ((fd_cmd(fdcu, 2, NE7CMD_SENSED, fdsu, 1, &st3) == 0) &&
		(st3 & NE7_ST3_T0)) {
		/* if at track 0, first seek inwards */
		/* seek some steps: */
		(void)fd_cmd(fdcu, 3, NE7CMD_SEEK, fdsu, 10, 0);
		DELAY(300000); /* ...wait a moment... */
		(void)fd_sense_int(fdc, 0, 0); /* make ctrlr happy */
	}

	/* If we're at track 0 first seek inwards. */
	if ((fd_sense_drive_status(fdc, &st3) == 0) && (st3 & NE7_ST3_T0)) {
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
		return(0);

	fd->track = FD_NO_TRACK;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->options = 0;
	printf("fdc%d: 1.44MB 3.5in PCMCIA\n", fdcu);
	fd->type = FD_1440;

	return (1);
}
#endif

/****************************************************************************/
/*                            motor control stuff                           */
/*		remember to not deselect the drive we're working on         */
/****************************************************************************/
static void
set_motor(struct fdc_data *fdc, int fdsu, int turnon)
{
	int fdout = fdc->fdout;
	int needspecify = 0;

#ifdef PC98
	outb(IO_FDPORT, (pc98_trans != 1 ? FDP_FDDEXC : 0)|FDP_PORTEXC);
	DELAY(10);
	fdout = FDO_DMAE|FDO_MTON;
#else
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
#endif

	fdout_wr(fdc, fdout);
	fdc->fdout = fdout;
	TRACE1("[0x%x->FDOUT]", fdout);

	if (needspecify) {
		/*
		 * XXX
		 * special case: since we have just woken up the FDC
		 * from its sleep, we silently assume the command will
		 * be accepted, and do not test for a timeout
		 */
#ifdef PC98
		(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
			     NE7_SPEC_1(4, 240), NE7_SPEC_2(2, 0),
			     0);
#else
		(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
			     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
			     0);
#endif
		if (fdc->flags & FDC_HAS_FIFO)
			(void) enable_fifo(fdc);
	}
}

static void
fd_turnoff(void *xfd)
{
	int	s;
	fd_p fd = xfd;

	TRACE1("[fd%d: turnoff]", fd->fdu);

	/*
	 * Don't turn off the motor yet if the drive is active.
	 * XXX shouldn't even schedule turnoff until drive is inactive
	 * and nothing is queued on it.
	 */
	if (fd->fdc->state != DEVIDLE && fd->fdc->fdu == fd->fdu) {
		fd->toffhandle = timeout(fd_turnoff, fd, 4 * hz);
		return;
	}

	s = splbio();
	fd->flags &= ~FD_MOTOR;
	set_motor(fd->fdc, fd->fdsu, TURNOFF);
	splx(s);
}

static void
fd_motor_on(void *xfd)
{
	int	s;
	fd_p fd = xfd;

	s = splbio();
	fd->flags &= ~FD_MOTOR_WAIT;
	if((fd->fdc->fd == fd) && (fd->fdc->state == MOTORWAIT))
	{
		fdc_intr(fd->fdc);
	}
	splx(s);
}

static void
fd_turnon(fd_p fd)
{
	if(!(fd->flags & FD_MOTOR))
	{
		fd->flags |= (FD_MOTOR + FD_MOTOR_WAIT);
		set_motor(fd->fdc, fd->fdsu, TURNON);
		timeout(fd_motor_on, fd, hz); /* in 1 sec its ok */
	}
}

static void
fdc_reset(fdc_p fdc)
{
	/* Try a reset, keep motor on */
#ifdef PC98
	set_density(fdc);
	if (pc98_machine_type & M_EPSON_PC98)
		fdout_wr(fdc, 0xe8);
	else
		fdout_wr(fdc, 0xd8);
	DELAY(200);
	fdout_wr(fdc, 0x18);
	DELAY(10);
#else
	fdout_wr(fdc, fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	DELAY(100);
	/* enable FDC, but defer interrupts a moment */
	fdout_wr(fdc, fdc->fdout & ~FDO_FDMAEN);
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~FDO_FDMAEN);
	DELAY(100);
	fdout_wr(fdc, fdc->fdout);
	TRACE1("[0x%x->FDOUT]", fdc->fdout);
#endif

	/* XXX after a reset, silently believe the FDC will accept commands */
#ifdef PC98
	(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
		     NE7_SPEC_1(4, 240), NE7_SPEC_2(2, 0),
		     0);
#else
	(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
		     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
		     0);
#endif
	if (fdc->flags & FDC_HAS_FIFO)
		(void) enable_fifo(fdc);
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc(struct fdc_data *fdc)
{
	int i, j = 100000;
	while ((i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM)
			return fdc_err(fdc, "ready for output in input\n");
	if (j <= 0)
		return fdc_err(fdc, bootverbose? "input ready timeout\n": 0);
#ifdef	FDC_DEBUG
	i = fddata_rd(fdc);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	return(i);
#else	/* !FDC_DEBUG */
	return fddata_rd(fdc);
#endif	/* FDC_DEBUG */
}

/*
 * fd_in: Like in_fdc, but allows you to see if it worked.
 */
static int
fd_in(struct fdc_data *fdc, int *ptr)
{
	int i, j = 100000;
	while ((i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM))
		!= (NE7_DIO|NE7_RQM) && j-- > 0)
		if (i == NE7_RQM)
			return fdc_err(fdc, "ready for output in input\n");
	if (j <= 0)
		return fdc_err(fdc, bootverbose? "input ready timeout\n": 0);
#ifdef	FDC_DEBUG
	i = fddata_rd(fdc);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	*ptr = i;
	return 0;
#else	/* !FDC_DEBUG */
	i = fddata_rd(fdc);
	if (ptr)
		*ptr = i;
	return 0;
#endif	/* FDC_DEBUG */
}

int
out_fdc(struct fdc_data *fdc, int x)
{
	int i;

	/* Check that the direction bit is set */
	i = 100000;
	while ((fdsts_rd(fdc) & NE7_DIO) && i-- > 0);
	if (i <= 0) return fdc_err(fdc, "direction bit not set\n");

	/* Check that the floppy controller is ready for a command */
	i = 100000;
	while ((fdsts_rd(fdc) & NE7_RQM) == 0 && i-- > 0);
	if (i <= 0)
		return fdc_err(fdc, bootverbose? "output ready timeout\n": 0);

	/* Send the command and return */
	fddata_wr(fdc, x);
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
	fd_p	fd;
	fdc_p	fdc;

	/* check bounds */
	if ((fd = devclass_get_softc(fd_devclass, fdu)) == 0)
		return (ENXIO);
	fdc = fd->fdc;
	if ((fdc == NULL) || (fd->type == NO_TYPE))
		return (ENXIO);
	if (type > NUMDENS)
		return (ENXIO);
#ifdef PC98
	if (type == 0)
		type = FD_1200;		/* XXX backward compatibility */
	else
		;			/* XXX any types are OK for PC-98 */

	if (pc98_fd_check_ready(fdu) == -1)
		return(EIO);
#else
	if (type == 0)
		type = fd->type;
	else {
		/*
		 * For each type of basic drive, make sure we are trying
		 * to open a type it can do,
		 */
		if (type != fd->type) {
			switch (fd->type) {
			case FD_360:
				return (ENXIO);
			case FD_720:
				if (   type != FD_820
				    && type != FD_800
				    && type != FD_640
				   )
					return (ENXIO);
				break;
			case FD_1200:
				switch (type) {
				case FD_1480:
					type = FD_1480in5_25;
					break;
				case FD_1440:
					type = FD_1440in5_25;
					break;
				case FD_1232:
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
				case FD_640:
					type = FD_640in5_25;
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
				    && type != FD_640
				    )
					return(ENXIO);
				break;
			}
		}
	}
#endif
	fd->ft = fd_types + type - 1;
	fd->flags |= FD_OPEN;
	device_busy(fd->dev);
	device_busy(fd->fdc->fdc_dev);
	return 0;
}

int
fdclose(dev_t dev, int flags, int mode, struct proc *p)
{
 	fdu_t fdu = FDUNIT(minor(dev));
	struct fd_data *fd;

	fd = devclass_get_softc(fd_devclass, fdu);
	fd->flags &= ~FD_OPEN;
	fd->options &= ~FDOPT_NORETRY;

	return (0);
}

/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
void
fdstrategy(struct buf *bp)
{
	unsigned nblocks, blknum, cando;
 	int	s;
 	fdu_t	fdu;
 	fdc_p	fdc;
 	fd_p	fd;
	size_t	fdblk;

 	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	if (fd == 0)
		panic("fdstrategy: buf for nonexistent device (%#lx, %#lx)",
		      (u_long)major(bp->b_dev), (u_long)minor(bp->b_dev));
	fdc = fd->fdc;
	if (fd->type == NO_TYPE) {
		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		goto bad;
	};

	fdblk = 128 << (fd->ft->secsize);
	if (!(bp->b_flags & B_FORMAT)) {
		if (bp->b_blkno < 0) {
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
	untimeout(fd_turnoff, fd, fd->toffhandle); /* a good idea */

	/* Tell devstat we are starting on the transaction */
	devstat_start_transaction(&fd->device_stats);

	fdstart(fdc);
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
fdstart(struct fdc_data *fdc)
{
	int s;

	s = splbio();
	if(fdc->state == DEVIDLE)
	{
		fdc_intr(fdc);
	}
	splx(s);
}

static void
fd_iotimeout(void *xfdc)
{
 	fdc_p fdc;
	int s;

	fdc = xfdc;
	TRACE1("fd%d[fd_iotimeout()]", fdc->fdu);

	/*
	 * Due to IBM's brain-dead design, the FDC has a faked ready
	 * signal, hardwired to ready == true. Thus, any command
	 * issued if there's no diskette in the drive will _never_
	 * complete, and must be aborted by resetting the FDC.
	 * Many thanks, Big Blue!
	 * The FDC must not be reset directly, since that would
	 * interfere with the state machine.  Instead, pretend that
	 * the command completed but was invalid.  The state machine
	 * will reset the FDC and retry once.
	 */
	s = splbio();
	fdc->status[0] = NE7_ST0_IC_IV;
	fdc->flags &= ~FDC_STAT_VALID;
	fdc->state = IOTIMEDOUT;
	fdc_intr(fdc);
	splx(s);
}

/* just ensure it has the right spl */
static void
fd_pseudointr(void *xfdc)
{
	int	s;

	s = splbio();
	fdc_intr(xfdc);
	splx(s);
}

/***********************************************************************\
*                                 fdintr				*
* keep calling the state machine until it returns a 0			*
* ALWAYS called at SPLBIO 						*
\***********************************************************************/
static void
fdc_intr(void *xfdc)
{
	fdc_p fdc = xfdc;
	while(fdstate(fdc))
		;
}

/*
 * magic pseudo-DMA initialization for YE FDC. Sets count and
 * direction
 */
#define SET_BCDR(fdc,wr,cnt,port) \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port,	 \
	    ((cnt)-1) & 0xff);						 \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port + 1, \
	    ((wr ? 0x80 : 0) | ((((cnt)-1) >> 8) & 0x7f)));

/*
 * fdcpio(): perform programmed IO read/write for YE PCMCIA floppy
 */
static int fdcpio(fdc_p fdc, long flags, caddr_t addr, u_int count)
{
	u_char *cptr = (u_char *)addr;

	if (flags & B_READ) {
		if (fdc->state != PIOREAD) {
			fdc->state = PIOREAD;
			return(0);
		};
		SET_BCDR(fdc, 0, count, 0);
		bus_space_read_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
	} else {
		bus_space_write_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
		SET_BCDR(fdc, 0, count, 0);
	};
	return(1);
}

/***********************************************************************\
* The controller state machine.						*
* if it returns a non zero value, it should be called again immediatly	*
\***********************************************************************/
static int
fdstate(fdc_p fdc)
{
	int read, format, head, i, sec = 0, sectrac, st0, cyl, st3;
	unsigned blknum = 0, b_cylinder = 0;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	register struct buf *bp;
	struct fd_formb *finfo = NULL;
	size_t fdblk;

	bp = fdc->bp;
	if (bp == NULL) {
		bp = bufq_first(&fdc->head);
		if (bp != NULL) {
			bufq_remove(&fdc->head, bp);
			fdc->bp = bp;
		}
	}
	if (bp == NULL) {
		/***********************************************\
		* nothing left for this controller to do	*
		* Force into the IDLE state,			*
		\***********************************************/
		fdc->state = DEVIDLE;
		if (fdc->fd) {
			device_printf(fdc->fdc_dev,
			    "unexpected valid fd pointer\n");
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
		}
		TRACE1("[fdc%d IDLE]", fdc->fdcu);
 		return (0);
	}
	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;
	if (fdc->fd && (fd != fdc->fd))
		device_printf(fd->dev, "confused fd pointers\n");
	read = bp->b_flags & B_READ;
	format = bp->b_flags & B_FORMAT;
	if (format) {
		finfo = (struct fd_formb *)bp->b_data;
		fd->skip = (char *)&(finfo->fd_formb_cylno(0))
			- (char *)finfo;
	}
	if (fdc->state == DOSEEK || fdc->state == SEEKCOMPLETE) {
		blknum = (unsigned) bp->b_pblkno * DEV_BSIZE/fdblk +
			fd->skip/fdblk;
		b_cylinder = blknum / (fd->ft->sectrac * fd->ft->heads);
	}
	TRACE1("fd%d", fdu);
	TRACE1("[%s]", fdstates[fdc->state]);
	TRACE1("(0x%x)", fd->flags);
	untimeout(fd_turnoff, fd, fd->toffhandle);
	fd->toffhandle = timeout(fd_turnoff, fd, 4 * hz);
	switch (fdc->state)
	{
	case DEVIDLE:
	case FINDWORK:	/* we have found new work */
		fdc->retry = 0;
		fd->skip = 0;
		fdc->fd = fd;
		fdc->fdu = fdu;
#ifdef PC98
		pc98_trans = fd->ft->trans;
		if (pc98_trans_prev != pc98_trans) {
			int i;
			set_density(fdc);
			for (i = 0; i < 10; i++) {
				outb(0x5f, 0);
				outb(0x5f, 0);
			}
			pc98_trans_prev = pc98_trans;
		}
		if (pc98_trans != fd->pc98_trans) {
			if (fd->type == FD_1440) {
				outb(0x4be, (fdu << 5) | 0x10 | (pc98_trans >> 1));
				outb(0x5f, 0);
				outb(0x5f, 0);
			}
			fd->pc98_trans = pc98_trans;
		}
#else
		fdctl_wr(fdc, fd->ft->trans);
#endif
		TRACE1("[0x%x->FDCTL]", fd->ft->trans);
		/*******************************************************\
		* If the next drive has a motor startup pending, then	*
		* it will start up in its own good time		*
		\*******************************************************/
		if(fd->flags & FD_MOTOR_WAIT) {
			fdc->state = MOTORWAIT;
			return (0); /* come back later */
		}
		/*******************************************************\
		* Maybe if it's not starting, it SHOULD be starting	*
		\*******************************************************/
#ifdef EPSON_NRDISK
		if (fdu != nrdu) {
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
		}
#else /* !EPSON_NRDISK */
		if (!(fd->flags & FD_MOTOR))
		{
			fdc->state = MOTORWAIT;
			fd_turnon(fd);
			return (0);
		}
		else	/* at least make sure we are selected */
		{
			set_motor(fdc, fd->fdsu, TURNON);
		}
#endif
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else
			fdc->state = DOSEEK;
		break;
	case DOSEEK:
		if (b_cylinder == (unsigned)fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			break;
		}
#ifdef PC98
		pc98_fd_check_ready(fdu);
#endif
		if (fd_cmd(fdc, 3, NE7CMD_SEEK,
			   fd->fdsu, b_cylinder * fd->ft->steptrac,
			   0))
		{
			/*
			 * seek command not accepted, looks like
			 * the FDC went off to the Saints...
			 */
			fdc->retry = 6;	/* try a reset */
			return(retrier(fdc));
		}
		fd->track = FD_NO_TRACK;
		fdc->state = SEEKWAIT;
		return(0);	/* will return later */
	case SEEKWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, fdc, hz / 16);
		fdc->state = SEEKCOMPLETE;
		return(0);	/* will return later */
	case SEEKCOMPLETE : /* SEEK DONE, START DMA */
		/* Make sure seek really happened*/
		if(fd->track == FD_NO_TRACK) {
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

			if (0 == descyl) {
				int failed = 0;
				/*
				 * seek to cyl 0 requested; make sure we are
				 * really there
				 */
				if (fd_sense_drive_status(fdc, &st3))
					failed = 1;
#ifdef EPSON_NRDISK
				if (fdu == nrdu) st3 = NE7_ST3_T0;
#endif /* EPSON_NRDISK */
				if ((st3 & NE7_ST3_T0) == 0) {
					printf(
		"fd%d: Seek to cyl 0, but not really there (ST3 = %b)\n",
					       fdu, st3, NE7_ST3BITS);
					failed = 1;
				}

				if (failed) {
					if(fdc->retry < 3)
						fdc->retry = 3;
					return (retrier(fdc));
				}
			}
#ifdef EPSON_NRDISK
			if (fdu == nrdu) cyl = descyl;
#endif

			if (cyl != descyl) {
				printf(
		"fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n",
				       fdu, descyl, cyl, st0);
				if (fdc->retry < 3)
					fdc->retry = 3;
				return (retrier(fdc));
			}
		}

		fd->track = b_cylinder;
#ifdef EPSON_NRDISK
		if (fdu != nrdu) {
#endif /* EPSON_NRDISK */
		if (!(fdc->flags & FDC_NODMA))
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
				isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
					    format ? bp->b_bcount : fdblk,
					    fdc->dmachan);
				fdc->retry = 6;	/* reset the beast */
				return (retrier(fdc));
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

		if (format) {
			if (fdc->flags & FDC_NODMA)
				(void)fdcpio(fdc,bp->b_flags,
					bp->b_data+fd->skip,
					bp->b_bcount);
			/* formatting */
			if(fd_cmd(fdc, 6,  NE7CMD_FORMAT, head << 2 | fdu,
				  finfo->fd_formb_secshift,
				  finfo->fd_formb_nsecs,
				  finfo->fd_formb_gaplen,
				  finfo->fd_formb_fillbyte, 0)) {
				/* controller fell over */
				isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
					    format ? bp->b_bcount : fdblk,
					    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		} else {
			if (fdc->flags & FDC_NODMA) {
				/*
				 * this seems to be necessary even when
				 * reading data
				 */
				SET_BCDR(fdc, 1, fdblk, 0);

				/*
				 * perform the write pseudo-DMA before
				 * the WRITE command is sent
				 */
				if (!read)
					(void)fdcpio(fdc,bp->b_flags,
					    bp->b_data+fd->skip,
					    fdblk);
			}
			if (fd_cmd(fdc, 9,
				   (read ? NE7CMD_READ : NE7CMD_WRITE),
				   head << 2 | fdu,  /* head & unit */
				   fd->track,        /* track */
				   head,
				   sec,              /* sector + 1 */
				   fd->ft->secsize,  /* sector size */
				   sectrac,          /* sectors/track */
				   fd->ft->gap,      /* gap size */
				   fd->ft->datalen,  /* data length */
				   0)) {
				/* the beast is sleeping again */
				isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
					    format ? bp->b_bcount : fdblk,
					    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		}
		if (fdc->flags & FDC_NODMA)
			/*
			 * if this is a read, then simply await interrupt
			 * before performing PIO
			 */
			if (read && !fdcpio(fdc,bp->b_flags,
			    bp->b_data+fd->skip,fdblk)) {
				fd->tohandle = timeout(fd_iotimeout, fdc, hz);
				return(0);      /* will return later */
			};

		/*
		 * write (or format) operation will fall through and
		 * await completion interrupt
		 */
		fdc->state = IOCOMPLETE;
		fd->tohandle = timeout(fd_iotimeout, fdc, hz);
		return (0);	/* will return later */
#ifdef EPSON_NRDISK
		}
		else {
			nrdblkn = (nrd_t)((unsigned long)bp->b_blkno*DEV_BSIZE/fdblk
				+ fd->skip/fdblk);
			nrd_LED_on();
			nrd_addrset(fdblk * nrdblkn);
			while (!nrd_check_ready()) DELAY(1);
			if (read) epson_insw(P_NRD_DATA,
					bp->b_data + fd->skip,
					fdblk / sizeof(short));
			else epson_outsw(P_NRD_DATA,
				bp->b_data + fd->skip,
				(format ? bp->b_bcount : fdblk)
					/ sizeof(short));

			blknum = (unsigned long)bp->b_blkno*DEV_BSIZE/fdblk
				+ fd->skip/fdblk;
			sectrac = fd->ft->sectrac;
			sec = blknum %  (sectrac * fd->ft->heads);
			head = sec / sectrac;
			sec = sec % sectrac + 1;
			fd->hddrv = ((head&1)<<2)+fdu;

			if (nrdsec++ >= nrd_sec())
				nrdaddr = (nrd_t)(fd->track * 8	+ head * 4);
			nrdsec = sec;
			fdc->state = IOCOMPLETE;
		}
#endif
	case PIOREAD:
		/* 
		 * actually perform the PIO read.  The IOCOMPLETE case
		 * removes the timeout for us.  
		 */
		(void)fdcpio(fdc,bp->b_flags,bp->b_data+fd->skip,fdblk);
		fdc->state = IOCOMPLETE;
		/* FALLTHROUGH */
	case IOCOMPLETE: /* IO DONE, post-analyze */
#ifdef EPSON_NRDISK
		if (fdu != nrdu) 
			untimeout(fd_iotimeout, fdc, fd->tohandle);
#else
		untimeout(fd_iotimeout, fdc, fd->tohandle);
#endif

		if (fd_read_status(fdc, fd->fdsu)) {
			isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
				    format ? bp->b_bcount : fdblk,
				    fdc->dmachan);
			if (fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			return (retrier(fdc));
  		}

		fdc->state = IOTIMEDOUT;

		/* FALLTHROUGH */

	case IOTIMEDOUT:
#ifdef EPSON_NRDISK
		if (fdu != nrdu) {
#endif /* EPSON_NRDISK */
		if (!(fdc->flags & FDC_NODMA))
			isa_dmadone(bp->b_flags, bp->b_data + fd->skip,
				format ? bp->b_bcount : fdblk, fdc->dmachan);
#ifdef EPSON_NRDISK
		}
		else nrd_LED_off();
#endif /* EPSON_NRDISK */
		if (fdc->status[0] & NE7_ST0_IC) {
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
			return (retrier(fdc));
		}
		/* All OK */
		fd->skip += fdblk;
		if (!format && fd->skip < bp->b_bcount - bp->b_resid) {
			/* set up next transfer */
			fdc->state = DOSEEK;
		} else {
			/* ALL DONE */
			fd->skip = 0;
			fdc->bp = NULL;
			devstat_end_transaction_buf(&fd->device_stats, bp);
			biodone(bp);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
			fdc->state = FINDWORK;
		}
		return (1);
	case RESETCTLR:
		fdc_reset(fdc);
		fdc->retry++;
		fdc->state = RESETCOMPLETE;
		return (0);
	case RESETCOMPLETE:
		/*
		 * Discard all the results from the reset so that they
		 * can't cause an unexpected interrupt later.
		 */
		for (i = 0; i < 4; i++)
			(void)fd_sense_int(fdc, &st0, &cyl);
		fdc->state = STARTRECAL;
		/* Fall through. */
	case STARTRECAL:
#ifdef PC98
		pc98_fd_check_ready(fdu);
#endif
		if(fd_cmd(fdc, 2, NE7CMD_RECAL, fdu, 0)) {
			/* arrgl */
			fdc->retry = 6;
			return (retrier(fdc));
		}
		fdc->state = RECALWAIT;
		return (0);	/* will return later */
	case RECALWAIT:
		/* allow heads to settle */
		timeout(fd_pseudointr, fdc, hz / 8);
		fdc->state = RECALCOMPLETE;
		return (0);	/* will return later */
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
#ifdef EPSON_NRDISK
		if (fdu == nrdu) {
			st0 = NE7_ST0_IC_NT;
			cyl = 0;
		}
#endif
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
			return (retrier(fdc));
		}
		fd->track = 0;
		/* Seek (probably) necessary */
		fdc->state = DOSEEK;
		return (1);	/* will return immediatly */
	case MOTORWAIT:
		if(fd->flags & FD_MOTOR_WAIT)
		{
			return (0); /* time's not up yet */
		}
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else {
			/*
			 * If all motors were off, then the controller was
			 * reset, so it has lost track of the current
			 * cylinder.  Recalibrate to handle this case.
			 * But first, discard the results of the reset.
			 */
			fdc->state = RESETCOMPLETE;
		}
		return (1);	/* will return immediatly */
	default:
		device_printf(fdc->fdc_dev, "unexpected FD int->");
		if (fd_read_status(fdc, fd->fdsu) == 0)
			printf("FDC status :%x %x %x %x %x %x %x   ",
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
			return (0);
		}
		printf("ST0 = %x, PCN = %x\n", st0, cyl);
		return (0);
	}
	/*XXX confusing: some branches return immediately, others end up here*/
	return (1); /* Come back immediatly to new state */
}

static int
retrier(struct fdc_data *fdc)
{
	register struct buf *bp;
	struct fd_data *fd;
	int fdu;

	bp = fdc->bp;

	/* XXX shouldn't this be cached somewhere?  */
	fdu = FDUNIT(minor(bp->b_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	if (fd->options & FDOPT_NORETRY)
		goto fail;

	switch (fdc->retry) {
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
			diskerr(bp, "hard error", LOG_PRINTF,
				fdc->fd->skip / DEV_BSIZE,
				(struct disklabel *)NULL);
			bp->b_dev = sav_b_dev;
			if (fdc->flags & FDC_STAT_VALID)
			{
				printf(
			" (ST0 %b ST1 %b ST2 %b cyl %u hd %u sec %u)\n",
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
		fdc->bp = NULL;
		fdc->fd->skip = 0;
		devstat_end_transaction_buf(&fdc->fd->device_stats, bp);
		biodone(bp);
		fdc->state = FINDWORK;
		fdc->flags |= FDC_NEEDS_RESET;
		fdc->fd = (fd_p) 0;
		fdc->fdu = -1;
		return (1);
	}
	fdc->retry++;
	return (1);
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

 	fdu	= FDUNIT(minor(dev));
	fd	= devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;

	/* set up a buffer header for fdstrategy() */
	bp = (struct buf *)malloc(sizeof(struct buf), M_TEMP, M_NOWAIT);
	if(bp == 0)
		return ENOBUFS;
	/*
	 * keep the process from being swapped
	 */
	PHOLD(p);
	bzero((void *)bp, sizeof(struct buf));
	BUF_LOCKINIT(bp);
	BUF_LOCK(bp, LK_EXCLUSIVE);
	bp->b_flags = B_PHYS | B_FORMAT;

	/*
	 * calculate a fake blkno, so fdstrategy() would initiate a
	 * seek to the requested cylinder
	 */
	bp->b_blkno = (finfo->cyl * (fd->ft->sectrac * fd->ft->heads)
		+ finfo->head * fd->ft->sectrac) * fdblk / DEV_BSIZE;

	bp->b_bcount = sizeof(struct fd_idfield_data) * finfo->fd_formb_nsecs;
	bp->b_data = (caddr_t)finfo;

	/* now do the format */
	bp->b_dev = dev;
	BUF_STRATEGY(bp, 0);

	/* ...and wait for it to complete */
	s = splbio();
	while(!(bp->b_flags & B_DONE)) {
		rv = tsleep((caddr_t)bp, PRIBIO, "fdform", 20 * hz);
		if (rv == EWOULDBLOCK)
			break;
	}
	splx(s);

	if (rv == EWOULDBLOCK) {
		/* timed out */
		rv = EIO;
		biodone(bp);
	}
	if (bp->b_flags & B_ERROR)
		rv = bp->b_error;
	/*
	 * allow the process to be swapped
	 */
	PRELE(p);
	BUF_UNLOCK(bp);
	BUF_LOCKFREE(bp);
	free(bp, M_TEMP);
	return rv;
}

/*
 * TODO: don't allocate buffer on stack.
 */

static int
fdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
 	fdu_t	fdu = FDUNIT(minor(dev));
 	fd_p	fd = devclass_get_softc(fd_devclass, fdu);
	size_t fdblk;

	struct fd_type *fdt;
	struct disklabel *dl;
	char buffer[DEV_BSIZE];
	int error = 0;

	fdblk = 128 << fd->ft->secsize;

#ifdef PC98
	pc98_fd_check_ready(fdu);
#endif	
	switch (cmd) {
	case DIOCGDINFO:
		bzero(buffer, sizeof (buffer));
		dl = (struct disklabel *)buffer;
		dl->d_secsize = fdblk;
		fdt = fd->ft;
		dl->d_secpercyl = fdt->size / fdt->tracks;
		dl->d_type = DTYPE_FLOPPY;

		if (readdisklabel(dkmodpart(dev, RAW_PART), dl)
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
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}

		dl = (struct disklabel *)addr;

		if ((error = setdisklabel((struct disklabel *)buffer, dl,
					  (u_long)0)) != 0)
			break;

		error = writedisklabel(dev, (struct disklabel *)buffer);
		break;
	case FD_FORM:
		if ((flag & FWRITE) == 0)
			error = EBADF;	/* must be opened for writing */
		else if (((struct fd_formb *)addr)->format_version !=
			FD_FORMAT_VERSION)
			error = EINVAL;	/* wrong version of formatting prog */
		else
			error = fdformat(dev, (struct fd_formb *)addr, p);
		break;

	case FD_GTYPE:                  /* get drive type */
		*(struct fd_type *)addr = *fd->ft;
		break;

	case FD_STYPE:                  /* set drive type */
		/* this is considered harmful; only allow for superuser */
		if (suser(p) != 0)
			return EPERM;
		*fd->ft = *(struct fd_type *)addr;
		break;

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd->options;
		break;

	case FD_SOPTS:			/* set drive options */
		fd->options = *(int *)addr;
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

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
