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
 * Copyright (c) 2001 Joerg Wunsch,
 *  joerg_wunsch@uriah.heep.sax.de (Joerg Wunsch)
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_fdc.h"
#include "card.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/devicestat.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/fdcio.h>
#include <sys/filio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/syslog.h>

#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/clock.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include <isa/isavar.h>
#include <isa/isareg.h>
#include <isa/fdreg.h>
#include <isa/rtc.h>

enum fdc_type
{
	FDC_NE765, FDC_ENHANCED, FDC_UNKNOWN = -1
};

enum fdc_states {
	DEVIDLE,
	FINDWORK,
	DOSEEK,
	SEEKCOMPLETE ,
	IOCOMPLETE,
	RECALCOMPLETE,
	STARTRECAL,
	RESETCTLR,
	SEEKWAIT,
	RECALWAIT,
	MOTORWAIT,
	IOTIMEDOUT,
	RESETCOMPLETE,
	PIOREAD
};

#ifdef	FDC_DEBUG
static char const * const fdstates[] = {
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
	"PIOREAD"
};
#endif

/*
 * Per controller structure (softc).
 */
struct fdc_data
{
	int	fdcu;		/* our unit number */
	int	dmachan;
	int	flags;
#define FDC_ATTACHED	0x01
#define FDC_STAT_VALID	0x08
#define FDC_HAS_FIFO	0x10
#define FDC_NEEDS_RESET	0x20
#define FDC_NODMA	0x40
#define FDC_ISPNP	0x80
#define FDC_ISPCMCIA	0x100
	struct	fd_data *fd;
	int	fdu;		/* the active drive	*/
	enum	fdc_states state;
	int	retry;
	int	fdout;		/* mirror of the w/o digital output reg */
	u_int	status[7];	/* copy of the registers */
	enum	fdc_type fdct;	/* chip version of FDC */
	int	fdc_errs;	/* number of logged errors */
	int	dma_overruns;	/* number of DMA overruns */
	struct	bio_queue_head head;
	struct	bio *bp;	/* active buffer */
	struct	resource *res_ioport, *res_ctl, *res_irq, *res_drq;
	int	rid_ioport, rid_ctl, rid_irq, rid_drq;
	int	port_off;
	bus_space_tag_t portt;
	bus_space_handle_t porth;
	bus_space_tag_t ctlt;
	bus_space_handle_t ctlh;
	void	*fdc_intr;
	struct	device *fdc_dev;
	void	(*fdctl_wr)(struct fdc_data *fdc, u_int8_t v);
};

#define FDBIO_FORMAT	BIO_CMD2

typedef int	fdu_t;
typedef int	fdcu_t;
typedef int	fdsu_t;
typedef	struct fd_data *fd_p;
typedef struct fdc_data *fdc_p;
typedef enum fdc_type fdc_t;

#define FDUNIT(s)	(((s) >> 6) & 3)
#define FDNUMTOUNIT(n)	(((n) & 3) << 6)
#define FDTYPE(s)	((s) & 0x3f)

/*
 * fdc maintains a set (1!) of ivars per child of each controller.
 */
enum fdc_device_ivars {
	FDC_IVAR_FDUNIT,
};

/*
 * Simple access macros for the ivars.
 */
#define FDC_ACCESSOR(A, B, T)						\
static __inline T fdc_get_ ## A(device_t dev)				\
{									\
	uintptr_t v;							\
	BUS_READ_IVAR(device_get_parent(dev), dev, FDC_IVAR_ ## B, &v);	\
	return (T) v;							\
}
FDC_ACCESSOR(fdunit,	FDUNIT,	int)

/* configuration flags for fdc */
#define FDC_NO_FIFO	(1 << 2)	/* do not enable FIFO  */

/* error returns for fd_cmd() */
#define FD_FAILED -1
#define FD_NOT_VALID -2
#define FDC_ERRMAX	100	/* do not log more */
/*
 * Stop retrying after this many DMA overruns.  Since each retry takes
 * one revolution, with 300 rpm., 25 retries take approximately 5
 * seconds which the read attempt will block in case the DMA overrun
 * is persistent.
 */
#define FDC_DMAOV_MAX	25

/*
 * Timeout value for the PIO loops to wait until the FDC main status
 * register matches our expectations (request for master, direction
 * bit).  This is supposed to be a number of microseconds, although
 * timing might actually not be very accurate.
 *
 * Timeouts of 100 msec are believed to be required for some broken
 * (old) hardware.
 */
#define	FDSTS_TIMEOUT	100000

/*
 * Number of subdevices that can be used for different density types.
 * By now, the lower 6 bit of the minor number are reserved for this,
 * allowing for up to 64 subdevices, but we only use 16 out of this.
 * Density #0 is used for automatic format detection, the other
 * densities are available as programmable densities (for assignment
 * by fdcontrol(8)).
 * The upper 2 bits of the minor number are reserved for the subunit
 * (drive #) per controller.
 */
#define NUMDENS		16

#define FDBIO_RDSECTID	BIO_CMD1

/*
 * List of native drive densities.  Order must match enum fd_drivetype
 * in <sys/fdcio.h>.  Upon attaching the drive, each of the
 * programmable subdevices is initialized with the native density
 * definition.
 */
static struct fd_type fd_native_types[] =
{
{ 0 },				/* FDT_NONE */
{  9,2,0xFF,0x2A,40, 720,FDC_250KBPS,2,0x50,1,0,FL_MFM }, /* FDT_360K */
{ 15,2,0xFF,0x1B,80,2400,FDC_500KBPS,2,0x54,1,0,FL_MFM }, /* FDT_12M  */
{  9,2,0xFF,0x20,80,1440,FDC_250KBPS,2,0x50,1,0,FL_MFM }, /* FDT_720K */
{ 18,2,0xFF,0x1B,80,2880,FDC_500KBPS,2,0x6C,1,0,FL_MFM }, /* FDT_144M */
#if 0				/* we currently don't handle 2.88 MB */
{ 36,2,0xFF,0x1B,80,5760,FDC_1MBPS,  2,0x4C,1,1,FL_MFM|FL_PERPND } /*FDT_288M*/
#else
{ 18,2,0xFF,0x1B,80,2880,FDC_500KBPS,2,0x6C,1,0,FL_MFM }, /* FDT_144M */
#endif
};

/*
 * 360 KB 5.25" and 720 KB 3.5" drives don't have automatic density
 * selection, they just start out with their native density (or lose).
 * So 1.2 MB 5.25", 1.44 MB 3.5", and 2.88 MB 3.5" drives have their
 * respective lists of densities to search for.
 */
static struct fd_type fd_searchlist_12m[] = {
{ 15,2,0xFF,0x1B,80,2400,FDC_500KBPS,2,0x54,1,0,FL_MFM }, /* 1.2M */
{  9,2,0xFF,0x23,40, 720,FDC_300KBPS,2,0x50,1,0,FL_MFM|FL_2STEP }, /* 360K */
{  9,2,0xFF,0x20,80,1440,FDC_300KBPS,2,0x50,1,0,FL_MFM }, /* 720K */
};

static struct fd_type fd_searchlist_144m[] = {
{ 18,2,0xFF,0x1B,80,2880,FDC_500KBPS,2,0x6C,1,0,FL_MFM }, /* 1.44M */
{  9,2,0xFF,0x20,80,1440,FDC_250KBPS,2,0x50,1,0,FL_MFM }, /* 720K */
};

/* We search for 1.44M first since this is the most common case. */
static struct fd_type fd_searchlist_288m[] = {
{ 18,2,0xFF,0x1B,80,2880,FDC_500KBPS,2,0x6C,1,0,FL_MFM }, /* 1.44M */
#if 0
{ 36,2,0xFF,0x1B,80,5760,FDC_1MBPS,  2,0x4C,1,1,FL_MFM|FL_PERPND } /* 2.88M */
#endif
{  9,2,0xFF,0x20,80,1440,FDC_250KBPS,2,0x50,1,0,FL_MFM }, /* 720K */
};

#define MAX_SEC_SIZE	(128 << 3)
#define MAX_CYLINDER	85	/* some people really stress their drives
				 * up to cyl 82 */
#define MAX_HEAD	1

static devclass_t fdc_devclass;

/*
 * Per drive structure (softc).
 */
struct fd_data {
	struct	fdc_data *fdc;	/* pointer to controller structure */
	int	fdsu;		/* this units number on this controller */
	enum	fd_drivetype type; /* drive type */
	struct	fd_type *ft;	/* pointer to current type descriptor */
	struct	fd_type fts[NUMDENS]; /* type descriptors */
	int	flags;
#define	FD_OPEN		0x01	/* it's open		*/
#define	FD_NONBLOCK	0x02	/* O_NONBLOCK set	*/
#define	FD_ACTIVE	0x04	/* it's active		*/
#define	FD_MOTOR	0x08	/* motor should be on	*/
#define	FD_MOTOR_WAIT	0x10	/* motor coming up	*/
#define	FD_UA		0x20	/* force unit attention */
	int	skip;
	int	hddrv;
#define FD_NO_TRACK -2
	int	track;		/* where we think the head is */
	int	options;	/* user configurable options, see fdcio.h */
	struct	callout_handle toffhandle;
	struct	callout_handle tohandle;
	struct	devstat *device_stats;
	eventhandler_tag clonetag;
	dev_t	masterdev;
	dev_t	clonedevs[NUMDENS - 1];
	device_t dev;
	fdu_t	fdu;
};

struct fdc_ivars {
	int	fdunit;
};
static devclass_t fd_devclass;

/* configuration flags for fd */
#define FD_TYPEMASK	0x0f	/* drive type, matches enum
				 * fd_drivetype; on i386 machines, if
				 * given as 0, use RTC type for fd0
				 * and fd1 */
#define FD_DTYPE(flags)	((flags) & FD_TYPEMASK)
#define FD_NO_CHLINE	0x10	/* drive does not support changeline
				 * aka. unit attention */
#define FD_NO_PROBE	0x20	/* don't probe drive (seek test), just
				 * assume it is there */

/*
 * Throughout this file the following conventions will be used:
 *
 * fd is a pointer to the fd_data struct for the drive in question
 * fdc is a pointer to the fdc_data struct for the controller
 * fdu is the floppy drive unit number
 * fdcu is the floppy controller unit number
 * fdsu is the floppy drive unit number on that controller. (sub-unit)
 */

/*
 * Function declarations, same (chaotic) order as they appear in the
 * file.  Re-ordering is too late now, it would only obfuscate the
 * diffs against old and offspring versions (like the PC98 one).
 *
 * Anyone adding functions here, please keep this sequence the same
 * as below -- makes locating a particular function in the body much
 * easier.
 */
static void fdout_wr(fdc_p, u_int8_t);
static u_int8_t fdsts_rd(fdc_p);
static void fddata_wr(fdc_p, u_int8_t);
static u_int8_t fddata_rd(fdc_p);
static void fdctl_wr_isa(fdc_p, u_int8_t);
#if NCARD > 0
static void fdctl_wr_pcmcia(fdc_p, u_int8_t);
#endif
#if 0
static u_int8_t fdin_rd(fdc_p);
#endif
static int fdc_err(struct fdc_data *, const char *);
static int fd_cmd(struct fdc_data *, int, ...);
static int enable_fifo(fdc_p fdc);
static int fd_sense_drive_status(fdc_p, int *);
static int fd_sense_int(fdc_p, int *, int *);
static int fd_read_status(fdc_p);
static int fdc_alloc_resources(struct fdc_data *);
static void fdc_release_resources(struct fdc_data *);
static int fdc_read_ivar(device_t, device_t, int, uintptr_t *);
static int fdc_probe(device_t);
#if NCARD > 0
static int fdc_pccard_probe(device_t);
#endif
static int fdc_detach(device_t dev);
static void fdc_add_child(device_t, const char *, int);
static int fdc_attach(device_t);
static int fdc_print_child(device_t, device_t);
static void fd_clone (void *, char *, int, dev_t *);
static int fd_probe(device_t);
static int fd_attach(device_t);
static int fd_detach(device_t);
static void set_motor(struct fdc_data *, int, int);
#  define TURNON 1
#  define TURNOFF 0
static timeout_t fd_turnoff;
static timeout_t fd_motor_on;
static void fd_turnon(struct fd_data *);
static void fdc_reset(fdc_p);
static int fd_in(struct fdc_data *, int *);
static int out_fdc(struct fdc_data *, int);
/*
 * The open function is named Fdopen() to avoid confusion with fdopen()
 * in fd(4).  The difference is now only meaningful for debuggers.
 */
static	d_open_t	Fdopen;
static	d_close_t	fdclose;
static	d_strategy_t	fdstrategy;
static void fdstart(struct fdc_data *);
static timeout_t fd_iotimeout;
static timeout_t fd_pseudointr;
static driver_intr_t fdc_intr;
static int fdcpio(fdc_p, long, caddr_t, u_int);
static int fdautoselect(dev_t);
static int fdstate(struct fdc_data *);
static int retrier(struct fdc_data *);
static void fdbiodone(struct bio *);
static int fdmisccmd(dev_t, u_int, void *);
static	d_ioctl_t	fdioctl;

static int fifo_threshold = 8;	/* XXX: should be accessible via sysctl */

#ifdef	FDC_DEBUG
/* CAUTION: fd_debug causes huge amounts of logging output */
static int volatile fd_debug = 0;
#define TRACE0(arg) do { if (fd_debug) printf(arg); } while (0)
#define TRACE1(arg1, arg2) do { if (fd_debug) printf(arg1, arg2); } while (0)
#else /* FDC_DEBUG */
#define TRACE0(arg) do { } while (0)
#define TRACE1(arg1, arg2) do { } while (0)
#endif /* FDC_DEBUG */

/*
 * Bus space handling (access to low-level IO).
 */
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

static void
fdctl_wr_isa(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->ctlt, fdc->ctlh, 0, v);
}

#if NCARD > 0
static void
fdctl_wr_pcmcia(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->portt, fdc->porth, FDCTL+fdc->port_off, v);
}
#endif

static u_int8_t
fdin_rd(fdc_p fdc)
{
	return bus_space_read_1(fdc->portt, fdc->porth, FDIN);
}

#define CDEV_MAJOR 9
static struct cdevsw fd_cdevsw = {
	.d_open =	Fdopen,
	.d_close =	fdclose,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	fdioctl,
	.d_strategy =	fdstrategy,
	.d_name =	"fd",
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_DISK,
};

/*
 * Auxiliary functions.  Well, some only.  Others are scattered
 * throughout the entire file.
 */
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
		 * Cannot use fd_cmd the normal way here, since
		 * this might be an invalid command. Thus we send the
		 * first byte, and check for an early turn of data directon.
		 */
		
		if (out_fdc(fdc, I8207X_CONFIGURE) < 0)
			return fdc_err(fdc, "Enable FIFO failed\n");
		
		/* If command is invalid, return */
		j = FDSTS_TIMEOUT;
		while ((i = fdsts_rd(fdc) & (NE7_DIO | NE7_RQM))
		       != NE7_RQM && j-- > 0) {
			if (i == (NE7_DIO | NE7_RQM)) {
				fdc_reset(fdc);
				return FD_FAILED;
			}
			DELAY(1);
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

	return 0;
}


static int
fd_read_status(fdc_p fdc)
{
	int i, ret;

	for (i = ret = 0; i < 7; i++) {
		/*
		 * XXX types are poorly chosen.  Only bytes can be read
		 * from the hardware, but fdc->status[] wants u_ints and
		 * fd_in() gives ints.
		 */
		int status;

		ret = fd_in(fdc, &status);
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

static int
fdc_alloc_resources(struct fdc_data *fdc)
{
	device_t dev;
	int ispnp, ispcmcia, nports;

	dev = fdc->fdc_dev;
	ispnp = (fdc->flags & FDC_ISPNP) != 0;
	ispcmcia = (fdc->flags & FDC_ISPCMCIA) != 0;
	fdc->rid_ioport = fdc->rid_irq = fdc->rid_drq = 0;
	fdc->res_ioport = fdc->res_irq = fdc->res_drq = 0;

	/*
	 * On standard ISA, we don't just use an 8 port range
	 * (e.g. 0x3f0-0x3f7) since that covers an IDE control
	 * register at 0x3f6.
	 *
	 * Isn't PC hardware wonderful.
	 *
	 * The Y-E Data PCMCIA FDC doesn't have this problem, it
	 * uses the register with offset 6 for pseudo-DMA, and the
	 * one with offset 7 as control register.
	 */
	nports = ispcmcia ? 8 : (ispnp ? 1 : 6);
	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
					     &fdc->rid_ioport, 0ul, ~0ul, 
					     nports, RF_ACTIVE);
	if (fdc->res_ioport == 0) {
		device_printf(dev, "cannot reserve I/O port range (%d ports)\n",
			      nports);
		return ENXIO;
	}
	fdc->portt = rman_get_bustag(fdc->res_ioport);
	fdc->porth = rman_get_bushandle(fdc->res_ioport);

	if (!ispcmcia) {
		/*
		 * Some BIOSen report the device at 0x3f2-0x3f5,0x3f7
		 * and some at 0x3f0-0x3f5,0x3f7. We detect the former
		 * by checking the size and adjust the port address
		 * accordingly.
		 */
		if (bus_get_resource_count(dev, SYS_RES_IOPORT, 0) == 4)
			fdc->port_off = -2;

		/*
		 * Register the control port range as rid 1 if it
		 * isn't there already. Most PnP BIOSen will have
		 * already done this but non-PnP configurations don't.
		 *
		 * And some (!!) report 0x3f2-0x3f5 and completely
		 * leave out the control register!  It seems that some
		 * non-antique controller chips have a different
		 * method of programming the transfer speed which
		 * doesn't require the control register, but it's
		 * mighty bogus as the chip still responds to the
		 * address for the control register.
		 */
		if (bus_get_resource_count(dev, SYS_RES_IOPORT, 1) == 0) {
			u_long ctlstart;

			/* Find the control port, usually 0x3f7 */
			ctlstart = rman_get_start(fdc->res_ioport) +
				fdc->port_off + 7;

			bus_set_resource(dev, SYS_RES_IOPORT, 1, ctlstart, 1);
		}

		/*
		 * Now (finally!) allocate the control port.
		 */
		fdc->rid_ctl = 1;
		fdc->res_ctl = bus_alloc_resource(dev, SYS_RES_IOPORT,
						  &fdc->rid_ctl,
						  0ul, ~0ul, 1, RF_ACTIVE);
		if (fdc->res_ctl == 0) {
			device_printf(dev,
		"cannot reserve control I/O port range (control port)\n");
			return ENXIO;
		}
		fdc->ctlt = rman_get_bustag(fdc->res_ctl);
		fdc->ctlh = rman_get_bushandle(fdc->res_ctl);
	}

	fdc->res_irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					  &fdc->rid_irq, 0ul, ~0ul, 1, 
					  RF_ACTIVE);
	if (fdc->res_irq == 0) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return ENXIO;
	}

	if ((fdc->flags & FDC_NODMA) == 0) {
		fdc->res_drq = bus_alloc_resource(dev, SYS_RES_DRQ,
						  &fdc->rid_drq, 0ul, ~0ul, 1, 
						  RF_ACTIVE);
		if (fdc->res_drq == 0) {
			device_printf(dev, "cannot reserve DMA request line\n");
			return ENXIO;
		}
		fdc->dmachan = fdc->res_drq->r_start;
	}

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
	if (fdc->res_ctl != 0) {
		bus_deactivate_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
					fdc->res_ctl);
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ctl,
				     fdc->res_ctl);
	}
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

/*
 * Configuration/initialization stuff, per controller.
 */

static struct isa_pnp_id fdc_ids[] = {
	{0x0007d041, "PC standard floppy disk controller"}, /* PNP0700 */
	{0x0107d041, "Standard floppy controller supporting MS Device Bay Spec"}, /* PNP0701 */
	{0}
};

static int
fdc_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
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

static int
fdc_probe(device_t dev)
{
	int	error, ic_type;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	bzero(fdc, sizeof *fdc);
	fdc->fdc_dev = dev;
	fdc->fdctl_wr = fdctl_wr_isa;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == ENXIO)
		return ENXIO;
	if (error == 0)
		fdc->flags |= FDC_ISPNP;

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_alloc_resources(fdc);
	if (error)
		goto out;

	/* First - lets reset the floppy controller */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);

	/* see if it can handle a command */
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}

	if (fd_cmd(fdc, 1, NE7CMD_VERSION, 1, &ic_type) == 0) {
		ic_type = (u_char)ic_type;
		switch (ic_type) {
		case 0x80:
			device_set_desc(dev, "NEC 765 or clone");
			fdc->fdct = FDC_NE765;
			break;
		case 0x81:	/* not mentioned in any hardware doc */
		case 0x90:
			device_set_desc(dev,
		"Enhanced floppy controller (i82077, NE72065 or clone)");
			fdc->fdct = FDC_ENHANCED;
			break;
		default:
			device_set_desc(dev, "Generic floppy controller");
			fdc->fdct = FDC_UNKNOWN;
			break;
		}
	}

out:
	fdc_release_resources(fdc);
	return (error);
}

#if NCARD > 0

static int
fdc_pccard_probe(device_t dev)
{
	int	error;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	bzero(fdc, sizeof *fdc);
	fdc->fdc_dev = dev;
	fdc->fdctl_wr = fdctl_wr_pcmcia;

	fdc->flags |= FDC_ISPCMCIA | FDC_NODMA;

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_alloc_resources(fdc);
	if (error)
		goto out;

	/* First - lets reset the floppy controller */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);

	/* see if it can handle a command */
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}

	device_set_desc(dev, "Y-E Data PCMCIA floppy");
	fdc->fdct = FDC_NE765;

out:
	fdc_release_resources(fdc);
	return (error);
}

#endif /* NCARD > 0 */

static int
fdc_detach(device_t dev)
{
	struct	fdc_data *fdc;
	int	error;

	fdc = device_get_softc(dev);

	/* have our children detached first */
	if ((error = bus_generic_detach(dev)))
		return (error);

	/* reset controller, turn motor off */
	fdout_wr(fdc, 0);

	if ((fdc->flags & FDC_NODMA) == 0)
		isa_dma_release(fdc->dmachan);

	if ((fdc->flags & FDC_ATTACHED) == 0) {
		device_printf(dev, "already unloaded\n");
		return (0);
	}
	fdc->flags &= ~FDC_ATTACHED;

	BUS_TEARDOWN_INTR(device_get_parent(dev), dev, fdc->res_irq,
			  fdc->fdc_intr);
	fdc_release_resources(fdc);
	device_printf(dev, "unload\n");
	return (0);
}

/*
 * Add a child device to the fdc controller.  It will then be probed etc.
 */
static void
fdc_add_child(device_t dev, const char *name, int unit)
{
	int	disabled, flags;
	struct fdc_ivars *ivar;
	device_t child;

	ivar = malloc(sizeof *ivar, M_DEVBUF /* XXX */, M_NOWAIT | M_ZERO);
	if (ivar == NULL)
		return;
	if (resource_int_value(name, unit, "drive", &ivar->fdunit) != 0)
		ivar->fdunit = 0;
	child = device_add_child(dev, name, unit);
	if (child == NULL) {
		free(ivar, M_DEVBUF);
		return;
	}
	device_set_ivars(child, ivar);
	if (resource_int_value(name, unit, "flags", &flags) == 0)
		 device_set_flags(child, flags);
	if (resource_int_value(name, unit, "disabled", &disabled) == 0
	    && disabled != 0)
		device_disable(child);
}

static int
fdc_attach(device_t dev)
{
	struct	fdc_data *fdc;
	const char *name, *dname;
	int	i, error, dunit;

	fdc = device_get_softc(dev);
	error = fdc_alloc_resources(fdc);
	if (error) {
		device_printf(dev, "cannot re-acquire resources\n");
		return error;
	}
	error = BUS_SETUP_INTR(device_get_parent(dev), dev, fdc->res_irq,
			       INTR_TYPE_BIO | INTR_ENTROPY, fdc_intr, fdc,
			       &fdc->fdc_intr);
	if (error) {
		device_printf(dev, "cannot setup interrupt\n");
		return error;
	}
	fdc->fdcu = device_get_unit(dev);
	fdc->flags |= FDC_ATTACHED | FDC_NEEDS_RESET;

	if ((fdc->flags & FDC_NODMA) == 0) {
		/*
		 * Acquire the DMA channel forever, the driver will do
		 * the rest
		 * XXX should integrate with rman
		 */
		isa_dma_acquire(fdc->dmachan);
		isa_dmainit(fdc->dmachan, MAX_SEC_SIZE);
	}
	fdc->state = DEVIDLE;

	/* reset controller, turn motor off, clear fdout mirror reg */
	fdout_wr(fdc, fdc->fdout = 0);
	bioq_init(&fdc->head);

	/*
	 * Probe and attach any children.  We should probably detect
	 * devices from the BIOS unless overridden.
	 */
	name = device_get_nameunit(dev);
	i = 0;
	while ((resource_find_match(&i, &dname, &dunit, "at", name)) == 0)
		fdc_add_child(dev, dname, dunit);

	if ((error = bus_generic_attach(dev)) != 0)
		return (error);

	return (0);
}

static int
fdc_print_child(device_t me, device_t child)
{
	int retval = 0, flags;

	retval += bus_print_child_header(me, child);
	retval += printf(" on %s drive %d", device_get_nameunit(me),
	       fdc_get_fdunit(child));
	if ((flags = device_get_flags(me)) != 0)
		retval += printf(" flags %#x", flags);
	retval += printf("\n");
	
	return (retval);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_probe),
	DEVMETHOD(device_attach,	fdc_attach),
	DEVMETHOD(device_detach,	fdc_detach),
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
DRIVER_MODULE(fdc, acpi, fdc_driver, fdc_devclass, 0, 0);

#if NCARD > 0

static device_method_t fdc_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_pccard_probe),
	DEVMETHOD(device_attach,	fdc_attach),
	DEVMETHOD(device_detach,	fdc_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdc_print_child),
	DEVMETHOD(bus_read_ivar,	fdc_read_ivar),
	/* Our children never use any other bus interface methods. */

	{ 0, 0 }
};

static driver_t fdc_pccard_driver = {
	"fdc",
	fdc_pccard_methods,
	sizeof(struct fdc_data)
};

DRIVER_MODULE(fdc, pccard, fdc_pccard_driver, fdc_devclass, 0, 0);

#endif /* NCARD > 0 */

/*
 * Create a clone device upon request by devfs.
 */
static void
fd_clone(void *arg, char *name, int namelen, dev_t *dev)
{
	struct	fd_data *fd;
	int i, u;
	char *n;
	size_t l;

	fd = (struct fd_data *)arg;
	if (*dev != NODEV)
		return;
	if (dev_stdclone(name, &n, "fd", &u) != 2)
		return;
	if (u != fd->fdu)
		/* unit # mismatch */
		return;
	l = strlen(n);
	if (l == 1 && *n >= 'a' && *n <= 'h') {
		/*
		 * Trailing letters a through h denote
		 * pseudo-partitions.  We don't support true
		 * (UFS-style) partitions, so we just implement them
		 * as symlinks if someone asks us nicely.
		 */
		*dev = make_dev_alias(fd->masterdev, name);
		return;
	}
	if (l >= 2 && l <= 5 && *n == '.') {
		/*
		 * Trailing numbers, preceded by a dot, denote
		 * subdevices for different densities.  Historically,
		 * they have been named by density (like fd0.1440),
		 * but we allow arbitrary numbers between 1 and 4
		 * digits, so fd0.1 through fd0.15 are possible as
		 * well.
		 */
		for (i = 1; i < l; i++)
			if (n[i] < '0' || n[i] > '9')
				return;
		for (i = 0; i < NUMDENS - 1; i++)
			if (fd->clonedevs[i] == NODEV) {
				*dev = make_dev(&fd_cdevsw,
						FDNUMTOUNIT(u) + i + 1,
						UID_ROOT, GID_OPERATOR, 0640,
						name);
				fd->clonedevs[i] = *dev;
				return;
			}
	}
}

/*
 * Configuration/initialization, per drive.
 */
static int
fd_probe(device_t dev)
{
	int	i;
	u_int	st0, st3;
	struct	fd_data *fd;
	struct	fdc_data *fdc;
	fdsu_t	fdsu;
	int	flags;

	fdsu = *(int *)device_get_ivars(dev); /* xxx cheat a bit... */
	fd = device_get_softc(dev);
	fdc = device_get_softc(device_get_parent(dev));
	flags = device_get_flags(dev);

	bzero(fd, sizeof *fd);
	fd->dev = dev;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->fdu = device_get_unit(dev);
	fd->flags = FD_UA;	/* make sure fdautoselect() will be called */

	fd->type = FD_DTYPE(flags);
/*
 * XXX I think using __i386__ is wrong here since we actually want to probe
 * for the machine type, not the CPU type (so non-PC arch's like the PC98 will
 * fail the probe).  However, for whatever reason, testing for _MACHINE_ARCH
 * == i386 breaks the test on FreeBSD/Alpha.
 */
#ifdef __i386__
	if (fd->type == FDT_NONE && (fd->fdu == 0 || fd->fdu == 1)) {
		/* Look up what the BIOS thinks we have. */
		if (fd->fdu == 0) {
			if ((fdc->flags & FDC_ISPCMCIA))
				/*
				 * Somewhat special.  No need to force the
				 * user to set device flags, since the Y-E
				 * Data PCMCIA floppy is always a 1.44 MB
				 * device.
				 */
				fd->type = FDT_144M;
			else
				fd->type = (rtcin(RTC_FDISKETTE) & 0xf0) >> 4;
		} else {
			fd->type = rtcin(RTC_FDISKETTE) & 0x0f;
		}
		if (fd->type == FDT_288M_1)
			fd->type = FDT_288M;
	}
#endif /* __i386__ */
	/* is there a unit? */
	if (fd->type == FDT_NONE)
		return (ENXIO);

	/* select it */
	set_motor(fdc, fdsu, TURNON);
	fdc_reset(fdc);		/* XXX reset, then unreset, etc. */
	DELAY(1000000);	/* 1 sec */

	/* XXX This doesn't work before the first set_motor() */
	if ((fdc->flags & FDC_HAS_FIFO) == 0  &&
	    fdc->fdct == FDC_ENHANCED &&
	    (device_get_flags(fdc->fdc_dev) & FDC_NO_FIFO) == 0 &&
	    enable_fifo(fdc) == 0) {
		device_printf(device_get_parent(dev),
		    "FIFO enabled, %d bytes threshold\n", fifo_threshold);
	}

	if ((flags & FD_NO_PROBE) == 0) {
		/* If we're at track 0 first seek inwards. */
		if ((fd_sense_drive_status(fdc, &st3) == 0) &&
		    (st3 & NE7_ST3_T0)) {
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
			 * heads have been beyond cylinder 76, since
			 * most FDCs still barf when attempting to
			 * recalibrate more than 77 steps
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
	}

	set_motor(fdc, fdsu, TURNOFF);

	if ((flags & FD_NO_PROBE) == 0 &&
	    (st0 & NE7_ST0_EC) != 0) /* no track 0 -> no drive present */
		return (ENXIO);

	switch (fd->type) {
	case FDT_12M:
		device_set_desc(dev, "1200-KB 5.25\" drive");
		fd->type = FDT_12M;
		break;
	case FDT_144M:
		device_set_desc(dev, "1440-KB 3.5\" drive");
		fd->type = FDT_144M;
		break;
	case FDT_288M:
		device_set_desc(dev, "2880-KB 3.5\" drive (in 1440-KB mode)");
		fd->type = FDT_288M;
		break;
	case FDT_360K:
		device_set_desc(dev, "360-KB 5.25\" drive");
		fd->type = FDT_360K;
		break;
	case FDT_720K:
		device_set_desc(dev, "720-KB 3.5\" drive");
		fd->type = FDT_720K;
		break;
	default:
		return (ENXIO);
	}
	fd->track = FD_NO_TRACK;
	fd->fdc = fdc;
	fd->fdsu = fdsu;
	fd->options = 0;
	callout_handle_init(&fd->toffhandle);
	callout_handle_init(&fd->tohandle);

	/* initialize densities for subdevices */
	for (i = 0; i < NUMDENS; i++)
		memcpy(fd->fts + i, fd_native_types + fd->type,
		       sizeof(struct fd_type));
	return (0);
}

static int
fd_attach(device_t dev)
{
	struct	fd_data *fd;
	int i;

	fd = device_get_softc(dev);
	fd->clonetag = EVENTHANDLER_REGISTER(dev_clone, fd_clone, fd, 1000);
	fd->masterdev = make_dev(&fd_cdevsw, fd->fdu << 6,
				 UID_ROOT, GID_OPERATOR, 0640, "fd%d", fd->fdu);
	for (i = 0; i < NUMDENS - 1; i++)
		fd->clonedevs[i] = NODEV;
	fd->device_stats = devstat_new_entry(device_get_name(dev), 
			  device_get_unit(dev), 0, DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_FLOPPY | DEVSTAT_TYPE_IF_OTHER,
			  DEVSTAT_PRIORITY_FD);
	return (0);
}

static int
fd_detach(device_t dev)
{
	struct	fd_data *fd;
	int i;

	fd = device_get_softc(dev);
	untimeout(fd_turnoff, fd, fd->toffhandle);
	devstat_remove_entry(fd->device_stats);
	destroy_dev(fd->masterdev);
	for (i = 0; i < NUMDENS - 1; i++)
		if (fd->clonedevs[i] != NODEV)
			destroy_dev(fd->clonedevs[i]);
	EVENTHANDLER_DEREGISTER(dev_clone, fd->clonetag);

	return (0);
}

static device_method_t fd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fd_probe),
	DEVMETHOD(device_attach,	fd_attach),
	DEVMETHOD(device_detach,	fd_detach),
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

/*
 * More auxiliary functions.
 */
/*
 * Motor control stuff.
 * Remember to not deselect the drive we're working on.
 */
static void
set_motor(struct fdc_data *fdc, int fdsu, int turnon)
{
	int fdout;

	fdout = fdc->fdout;
	if (turnon) {
		fdout &= ~FDO_FDSEL;
		fdout |= (FDO_MOEN0 << fdsu) | FDO_FDMAEN | FDO_FRST | fdsu;
	} else
		fdout &= ~(FDO_MOEN0 << fdsu);
	fdc->fdout = fdout;
	fdout_wr(fdc, fdout);
	TRACE1("[0x%x->FDOUT]", fdout);
}

static void
fd_turnoff(void *xfd)
{
	int	s;
	fd_p fd = xfd;

	TRACE1("[fd%d: turnoff]", fd->fdu);

	s = splbio();
	/*
	 * Don't turn off the motor yet if the drive is active.
	 *
	 * If we got here, this could only mean we missed an interrupt.
	 * This can e. g. happen on the Y-E Date PCMCIA floppy controller
	 * after a controller reset.  Just schedule a pseudo-interrupt
	 * so the state machine gets re-entered.
	 */
	if (fd->fdc->state != DEVIDLE && fd->fdc->fdu == fd->fdu) {
		fdc_intr(fd->fdc);
		splx(s);
		return;
	}

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
	fdout_wr(fdc, fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~(FDO_FRST|FDO_FDMAEN));
	DELAY(100);
	/* enable FDC, but defer interrupts a moment */
	fdout_wr(fdc, fdc->fdout & ~FDO_FDMAEN);
	TRACE1("[0x%x->FDOUT]", fdc->fdout & ~FDO_FDMAEN);
	DELAY(100);
	fdout_wr(fdc, fdc->fdout);
	TRACE1("[0x%x->FDOUT]", fdc->fdout);

	/* XXX after a reset, silently believe the FDC will accept commands */
	(void)fd_cmd(fdc, 3, NE7CMD_SPECIFY,
		     NE7_SPEC_1(3, 240), NE7_SPEC_2(2, 0),
		     0);
	if (fdc->flags & FDC_HAS_FIFO)
		(void) enable_fifo(fdc);
}

/*
 * FDC IO functions, take care of the main status register, timeout
 * in case the desired status bits are never set.
 *
 * These PIO loops initially start out with short delays between
 * each iteration in the expectation that the required condition
 * is usually met quickly, so it can be handled immediately.  After
 * about 1 ms, stepping is increased to achieve a better timing
 * accuracy in the calls to DELAY().
 */
static int
fd_in(struct fdc_data *fdc, int *ptr)
{
	int i, j, step;

	for (j = 0, step = 1;
	    (i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM)) != (NE7_DIO|NE7_RQM) &&
	    j < FDSTS_TIMEOUT;
	    j += step) {
		if (i == NE7_RQM)
			return (fdc_err(fdc, "ready for output in input\n"));
		if (j == 1000)
			step = 1000;
		DELAY(step);
	}
	if (j >= FDSTS_TIMEOUT)
		return (fdc_err(fdc, bootverbose? "input ready timeout\n": 0));
#ifdef	FDC_DEBUG
	i = fddata_rd(fdc);
	TRACE1("[FDDATA->0x%x]", (unsigned char)i);
	*ptr = i;
	return (0);
#else	/* !FDC_DEBUG */
	i = fddata_rd(fdc);
	if (ptr)
		*ptr = i;
	return (0);
#endif	/* FDC_DEBUG */
}

static int
out_fdc(struct fdc_data *fdc, int x)
{
	int i, j, step;

	for (j = 0, step = 1;
	    (i = fdsts_rd(fdc) & (NE7_DIO|NE7_RQM)) != NE7_RQM &&
	    j < FDSTS_TIMEOUT;
	    j += step) {
		if (i == (NE7_DIO|NE7_RQM))
			return (fdc_err(fdc, "ready for input in output\n"));
		if (j == 1000)
			step = 1000;
		DELAY(step);
	}
	if (j >= FDSTS_TIMEOUT)
		return (fdc_err(fdc, bootverbose? "output ready timeout\n": 0));

	/* Send the command and return */
	fddata_wr(fdc, x);
	TRACE1("[0x%x->FDDATA]", x);
	return (0);
}

/*
 * Block device driver interface functions (interspersed with even more
 * auxiliary functions).
 */
static int
Fdopen(dev_t dev, int flags, int mode, struct thread *td)
{
 	fdu_t fdu = FDUNIT(minor(dev));
	int type = FDTYPE(minor(dev));
	fd_p	fd;
	fdc_p	fdc;
 	int rv, unitattn, dflags;

	if ((fd = devclass_get_softc(fd_devclass, fdu)) == 0)
		return (ENXIO);
	fdc = fd->fdc;
	if ((fdc == NULL) || (fd->type == FDT_NONE))
		return (ENXIO);
	if (type > NUMDENS)
		return (ENXIO);
	dflags = device_get_flags(fd->dev);
	/*
	 * This is a bit bogus.  It's still possible that e. g. a
	 * descriptor gets inherited to a child, but then it's at
	 * least for the same subdevice.  By checking FD_OPEN here, we
	 * can ensure that a device isn't attempted to be opened with
	 * different densities at the same time where the second open
	 * could clobber the settings from the first one.
	 */
	if (fd->flags & FD_OPEN)
		return (EBUSY);

	if (type == 0) {
		if (flags & FNONBLOCK) {
			/*
			 * Unfortunately, physio(9) discards its ioflag
			 * argument, thus preventing us from seeing the
			 * IO_NDELAY bit.  So we need to keep track
			 * ourselves.
			 */
			fd->flags |= FD_NONBLOCK;
			fd->ft = 0;
		} else {
			/*
			 * Figure out a unit attention condition.
			 *
			 * If UA has been forced, proceed.
			 *
			 * If motor is off, turn it on for a moment
			 * and select our drive, in order to read the
			 * UA hardware signal.
			 *
			 * If motor is on, and our drive is currently
			 * selected, just read the hardware bit.
			 *
			 * If motor is on, but active for another
			 * drive on that controller, we are lost.  We
			 * cannot risk to deselect the other drive, so
			 * we just assume a forced UA condition to be
			 * on the safe side.
			 */
			unitattn = 0;
			if ((dflags & FD_NO_CHLINE) != 0 ||
			    (fd->flags & FD_UA) != 0) {
				unitattn = 1;
				fd->flags &= ~FD_UA;
			} else if (fdc->fdout & (FDO_MOEN0 | FDO_MOEN1 |
						 FDO_MOEN2 | FDO_MOEN3)) {
				if ((fdc->fdout & FDO_FDSEL) == fd->fdsu)
					unitattn = fdin_rd(fdc) & FDI_DCHG;
				else
					unitattn = 1;
			} else {
				set_motor(fdc, fd->fdsu, TURNON);
				unitattn = fdin_rd(fdc) & FDI_DCHG;
				set_motor(fdc, fd->fdsu, TURNOFF);
			}
			if (unitattn && (rv = fdautoselect(dev)) != 0)
				return (rv);
		}
	} else {
		fd->ft = fd->fts + type;
	}
	fd->flags |= FD_OPEN;
	/*
	 * Clearing the DMA overrun counter at open time is a bit messy.
	 * Since we're only managing one counter per controller, opening
	 * the second drive could mess it up.  Anyway, if the DMA overrun
	 * condition is really persistent, it will eventually time out
	 * still.  OTOH, clearing it here will ensure we'll at least start
	 * trying again after a previous (maybe even long ago) failure.
	 * Also, this is merely a stop-gap measure only that should not
	 * happen during normal operation, so we can tolerate it to be a
	 * bit sloppy about this.
	 */
	fdc->dma_overruns = 0;

	return 0;
}

static int
fdclose(dev_t dev, int flags, int mode, struct thread *td)
{
 	fdu_t fdu = FDUNIT(minor(dev));
	struct fd_data *fd;

	fd = devclass_get_softc(fd_devclass, fdu);
	fd->flags &= ~(FD_OPEN | FD_NONBLOCK);
	fd->options &= ~(FDOPT_NORETRY | FDOPT_NOERRLOG | FDOPT_NOERROR);

	return (0);
}

static void
fdstrategy(struct bio *bp)
{
	long blknum, nblocks;
 	int	s;
 	fdu_t	fdu;
 	fdc_p	fdc;
 	fd_p	fd;
	size_t	fdblk;

 	fdu = FDUNIT(minor(bp->bio_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	if (fd == 0)
		panic("fdstrategy: buf for nonexistent device (%#lx, %#lx)",
		      (u_long)major(bp->bio_dev), (u_long)minor(bp->bio_dev));
	fdc = fd->fdc;
	if (fd->type == FDT_NONE || fd->ft == 0) {
		bp->bio_error = ENXIO;
		bp->bio_flags |= BIO_ERROR;
		goto bad;
	}
	fdblk = 128 << (fd->ft->secsize);
	if (bp->bio_cmd != FDBIO_FORMAT && bp->bio_cmd != FDBIO_RDSECTID) {
		if (fd->flags & FD_NONBLOCK) {
			bp->bio_error = EAGAIN;
			bp->bio_flags |= BIO_ERROR;
			goto bad;
		}
		if (bp->bio_blkno < 0) {
			printf(
		"fd%d: fdstrat: bad request blkno = %lu, bcount = %ld\n",
			       fdu, (u_long)bp->bio_blkno, bp->bio_bcount);
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
			goto bad;
		}
		if ((bp->bio_bcount % fdblk) != 0) {
			bp->bio_error = EINVAL;
			bp->bio_flags |= BIO_ERROR;
			goto bad;
		}
	}

	/*
	 * Set up block calculations.
	 */
	if (bp->bio_blkno > 20000000) {
		/*
		 * Reject unreasonably high block number, prevent the
		 * multiplication below from overflowing.
		 */
		bp->bio_error = EINVAL;
		bp->bio_flags |= BIO_ERROR;
		goto bad;
	}
	blknum = bp->bio_blkno * DEV_BSIZE / fdblk;
 	nblocks = fd->ft->size;
	if (blknum + bp->bio_bcount / fdblk > nblocks) {
		if (blknum >= nblocks) {
			if (bp->bio_cmd == BIO_READ)
				bp->bio_resid = bp->bio_bcount;
			else {
				bp->bio_error = ENOSPC;
				bp->bio_flags |= BIO_ERROR;
			}
			goto bad;	/* not always bad, but EOF */
		}
		bp->bio_bcount = (nblocks - blknum) * fdblk;
	}
 	bp->bio_pblkno = blknum;
	s = splbio();
	bioq_disksort(&fdc->head, bp);
	untimeout(fd_turnoff, fd, fd->toffhandle); /* a good idea */
	devstat_start_transaction_bio(fd->device_stats, bp);
	device_busy(fd->dev);
	fdstart(fdc);
	splx(s);
	return;

bad:
	biodone(bp);
}

/*
 * fdstart
 *
 * We have just queued something.  If the controller is not busy
 * then simulate the case where it has just finished a command
 * So that it (the interrupt routine) looks on the queue for more
 * work to do and picks up what we just added.
 *
 * If the controller is already busy, we need do nothing, as it
 * will pick up our work when the present work completes.
 */
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

/* Just ensure it has the right spl. */
static void
fd_pseudointr(void *xfdc)
{
	int	s;

	s = splbio();
	fdc_intr(xfdc);
	splx(s);
}

/*
 * fdc_intr
 *
 * Keep calling the state machine until it returns a 0.
 * Always called at splbio.
 */
static void
fdc_intr(void *xfdc)
{
	fdc_p fdc = xfdc;
	while(fdstate(fdc))
		;
}

/*
 * Magic pseudo-DMA initialization for YE FDC. Sets count and
 * direction.
 */
#define SET_BCDR(fdc,wr,cnt,port) \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port,	 \
	    ((cnt)-1) & 0xff);						 \
	bus_space_write_1(fdc->portt, fdc->porth, fdc->port_off + port + 1, \
	    ((wr ? 0x80 : 0) | ((((cnt)-1) >> 8) & 0x7f)));

/*
 * fdcpio(): perform programmed IO read/write for YE PCMCIA floppy.
 */
static int
fdcpio(fdc_p fdc, long flags, caddr_t addr, u_int count)
{
	u_char *cptr = (u_char *)addr;

	if (flags == BIO_READ) {
		if (fdc->state != PIOREAD) {
			fdc->state = PIOREAD;
			return(0);
		}
		SET_BCDR(fdc, 0, count, 0);
		bus_space_read_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
	} else {
		bus_space_write_multi_1(fdc->portt, fdc->porth, fdc->port_off +
		    FDC_YE_DATAPORT, cptr, count);
		SET_BCDR(fdc, 0, count, 0);
	}
	return(1);
}

/*
 * Try figuring out the density of the media present in our device.
 */
static int
fdautoselect(dev_t dev)
{
	fdu_t fdu;
 	fd_p fd;
	struct fd_type *fdtp;
	struct fdc_readid id;
	int i, n, oopts, rv;

 	fdu = FDUNIT(minor(dev));
	fd = devclass_get_softc(fd_devclass, fdu);

	switch (fd->type) {
	default:
		return (ENXIO);

	case FDT_360K:
	case FDT_720K:
		/* no autoselection on those drives */
		fd->ft = fd_native_types + fd->type;
		return (0);

	case FDT_12M:
		fdtp = fd_searchlist_12m;
		n = sizeof fd_searchlist_12m / sizeof(struct fd_type);
		break;

	case FDT_144M:
		fdtp = fd_searchlist_144m;
		n = sizeof fd_searchlist_144m / sizeof(struct fd_type);
		break;

	case FDT_288M:
		fdtp = fd_searchlist_288m;
		n = sizeof fd_searchlist_288m / sizeof(struct fd_type);
		break;
	}

	/*
	 * Try reading sector ID fields, first at cylinder 0, head 0,
	 * then at cylinder 2, head N.  We don't probe cylinder 1,
	 * since for 5.25in DD media in a HD drive, there are no data
	 * to read (2 step pulses per media cylinder required).  For
	 * two-sided media, the second probe always goes to head 1, so
	 * we can tell them apart from single-sided media.  As a
	 * side-effect this means that single-sided media should be
	 * mentioned in the search list after two-sided media of an
	 * otherwise identical density.  Media with a different number
	 * of sectors per track but otherwise identical parameters
	 * cannot be distinguished at all.
	 *
	 * If we successfully read an ID field on both cylinders where
	 * the recorded values match our expectation, we are done.
	 * Otherwise, we try the next density entry from the table.
	 *
	 * Stepping to cylinder 2 has the side-effect of clearing the
	 * unit attention bit.
	 */
	oopts = fd->options;
	fd->options |= FDOPT_NOERRLOG | FDOPT_NORETRY;
	for (i = 0; i < n; i++, fdtp++) {
		fd->ft = fdtp;

		id.cyl = id.head = 0;
		rv = fdmisccmd(dev, FDBIO_RDSECTID, &id);
		if (rv != 0)
			continue;
		if (id.cyl != 0 || id.head != 0 ||
		    id.secshift != fdtp->secsize)
			continue;
		id.cyl = 2;
		id.head = fd->ft->heads - 1;
		rv = fdmisccmd(dev, FDBIO_RDSECTID, &id);
		if (id.cyl != 2 || id.head != fdtp->heads - 1 ||
		    id.secshift != fdtp->secsize)
			continue;
		if (rv == 0)
			break;
	}

	fd->options = oopts;
	if (i == n) {
		if (bootverbose)
			device_printf(fd->dev, "autoselection failed\n");
		fd->ft = 0;
		return (EIO);
	} else {
		if (bootverbose)
			device_printf(fd->dev, "autoselected %d KB medium\n",
				      fd->ft->size / 2);
		return (0);
	}
}


/*
 * The controller state machine.
 *
 * If it returns a non zero value, it should be called again immediately.
 */
static int
fdstate(fdc_p fdc)
{
	struct fdc_readid *idp;
	int read, format, rdsectid, cylinder, head, i, sec = 0, sectrac;
	int st0, cyl, st3, idf, ne7cmd, mfm, steptrac;
	unsigned long blknum;
	fdu_t fdu = fdc->fdu;
	fd_p fd;
	register struct bio *bp;
	struct fd_formb *finfo = NULL;
	size_t fdblk;

	bp = fdc->bp;
	if (bp == NULL) {
		bp = bioq_first(&fdc->head);
		if (bp != NULL) {
			bioq_remove(&fdc->head, bp);
			fdc->bp = bp;
		}
	}
	if (bp == NULL) {
		/*
		 * Nothing left for this controller to do,
		 * force into the IDLE state.
		 */
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
	fdu = FDUNIT(minor(bp->bio_dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;
	if (fdc->fd && (fd != fdc->fd))
		device_printf(fd->dev, "confused fd pointers\n");
	read = bp->bio_cmd == BIO_READ;
	mfm = (fd->ft->flags & FL_MFM)? NE7CMD_MFM: 0;
	steptrac = (fd->ft->flags & FL_2STEP)? 2: 1;
	if (read)
		idf = ISADMA_READ;
	else
		idf = ISADMA_WRITE;
	format = bp->bio_cmd == FDBIO_FORMAT;
	rdsectid = bp->bio_cmd == FDBIO_RDSECTID;
	if (format)
		finfo = (struct fd_formb *)bp->bio_data;
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
		fdc->fdctl_wr(fdc, fd->ft->trans);
		TRACE1("[0x%x->FDCTL]", fd->ft->trans);
		/*
		 * If the next drive has a motor startup pending, then
		 * it will start up in its own good time.
		 */
		if(fd->flags & FD_MOTOR_WAIT) {
			fdc->state = MOTORWAIT;
			return (0); /* will return later */
		}
		/*
		 * Maybe if it's not starting, it SHOULD be starting.
		 */
		if (!(fd->flags & FD_MOTOR))
		{
			fdc->state = MOTORWAIT;
			fd_turnon(fd);
			return (0); /* will return later */
		}
		else	/* at least make sure we are selected */
		{
			set_motor(fdc, fd->fdsu, TURNON);
		}
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else
			fdc->state = DOSEEK;
		return (1);	/* will return immediately */

	case DOSEEK:
		blknum = bp->bio_pblkno + fd->skip / fdblk;
		cylinder = blknum / (fd->ft->sectrac * fd->ft->heads);
		if (cylinder == fd->track)
		{
			fdc->state = SEEKCOMPLETE;
			return (1); /* will return immediately */
		}
		if (fd_cmd(fdc, 3, NE7CMD_SEEK,
			   fd->fdsu, cylinder * steptrac, 0))
		{
			/*
			 * Seek command not accepted, looks like
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

	case SEEKCOMPLETE : /* seek done, start DMA */
		blknum = bp->bio_pblkno + fd->skip / fdblk;
		cylinder = blknum / (fd->ft->sectrac * fd->ft->heads);

		/* Make sure seek really happened. */
		if(fd->track == FD_NO_TRACK) {
			int descyl = cylinder * steptrac;
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
					return (0); /* will return later */
				if(fdc->fdct == FDC_NE765
				   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
					return (0); /* hope for a real intr */
			} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);

			if (0 == descyl) {
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

				if (failed) {
					if(fdc->retry < 3)
						fdc->retry = 3;
					return (retrier(fdc));
				}
			}

			if (cyl != descyl) {
				printf(
		"fd%d: Seek to cyl %d failed; am at cyl %d (ST0 = 0x%x)\n",
				       fdu, descyl, cyl, st0);
				if (fdc->retry < 3)
					fdc->retry = 3;
				return (retrier(fdc));
			}
		}

		fd->track = cylinder;
		if (format)
			fd->skip = (char *)&(finfo->fd_formb_cylno(0))
			    - (char *)finfo;
		if (!rdsectid && !(fdc->flags & FDC_NODMA))
			isa_dmastart(idf, bp->bio_data+fd->skip,
				format ? bp->bio_bcount : fdblk, fdc->dmachan);
		blknum = bp->bio_pblkno + fd->skip / fdblk;
		sectrac = fd->ft->sectrac;
		sec = blknum %  (sectrac * fd->ft->heads);
		head = sec / sectrac;
		sec = sec % sectrac + 1;
		if (head != 0 && fd->ft->offset_side2 != 0)
			sec += fd->ft->offset_side2;
		fd->hddrv = ((head&1)<<2)+fdu;

		if(format || !(read || rdsectid))
		{
			/* make sure the drive is writable */
			if(fd_sense_drive_status(fdc, &st3) != 0)
			{
				/* stuck controller? */
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(idf,
						    bp->bio_data + fd->skip,
						    format ? bp->bio_bcount : fdblk,
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
				return (1); /* will return immediately */
			}
		}

		if (format) {
			ne7cmd = NE7CMD_FORMAT | mfm;
			if (fdc->flags & FDC_NODMA) {
				/*
				 * This seems to be necessary for
				 * whatever obscure reason; if we omit
				 * it, we end up filling the sector ID
				 * fields of the newly formatted track
				 * entirely with garbage, causing
				 * `wrong cylinder' errors all over
				 * the place when trying to read them
				 * back.
				 *
				 * Umpf.
				 */
				SET_BCDR(fdc, 1, bp->bio_bcount, 0);

				(void)fdcpio(fdc,bp->bio_cmd,
					bp->bio_data+fd->skip,
					bp->bio_bcount);

			}
			/* formatting */
			if(fd_cmd(fdc, 6,  ne7cmd, head << 2 | fdu,
				  finfo->fd_formb_secshift,
				  finfo->fd_formb_nsecs,
				  finfo->fd_formb_gaplen,
				  finfo->fd_formb_fillbyte, 0)) {
				/* controller fell over */
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(idf,
						    bp->bio_data + fd->skip,
						    format ? bp->bio_bcount : fdblk,
						    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		} else if (rdsectid) {
			ne7cmd = NE7CMD_READID | mfm;
			if (fd_cmd(fdc, 2, ne7cmd, head << 2 | fdu, 0)) {
				/* controller jamming */
				fdc->retry = 6;
				return (retrier(fdc));
			}
		} else {
			/* read or write operation */
			ne7cmd = (read ? NE7CMD_READ | NE7CMD_SK : NE7CMD_WRITE) | mfm;
			if (fdc->flags & FDC_NODMA) {
				/*
				 * This seems to be necessary even when
				 * reading data.
				 */
				SET_BCDR(fdc, 1, fdblk, 0);

				/*
				 * Perform the write pseudo-DMA before
				 * the WRITE command is sent.
				 */
				if (!read)
					(void)fdcpio(fdc,bp->bio_cmd,
					    bp->bio_data+fd->skip,
					    fdblk);
			}
			if (fd_cmd(fdc, 9,
				   ne7cmd,
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
				if (!(fdc->flags & FDC_NODMA))
					isa_dmadone(idf,
						    bp->bio_data + fd->skip,
						    format ? bp->bio_bcount : fdblk,
						    fdc->dmachan);
				fdc->retry = 6;
				return (retrier(fdc));
			}
		}
		if (!rdsectid && (fdc->flags & FDC_NODMA))
			/*
			 * If this is a read, then simply await interrupt
			 * before performing PIO.
			 */
			if (read && !fdcpio(fdc,bp->bio_cmd,
			    bp->bio_data+fd->skip,fdblk)) {
				fd->tohandle = timeout(fd_iotimeout, fdc, hz);
				return(0);      /* will return later */
			}

		/*
		 * Write (or format) operation will fall through and
		 * await completion interrupt.
		 */
		fdc->state = IOCOMPLETE;
		fd->tohandle = timeout(fd_iotimeout, fdc, hz);
		return (0);	/* will return later */

	case PIOREAD:
		/* 
		 * Actually perform the PIO read.  The IOCOMPLETE case
		 * removes the timeout for us.
		 */
		(void)fdcpio(fdc,bp->bio_cmd,bp->bio_data+fd->skip,fdblk);
		fdc->state = IOCOMPLETE;
		/* FALLTHROUGH */
	case IOCOMPLETE: /* IO done, post-analyze */
		untimeout(fd_iotimeout, fdc, fd->tohandle);

		if (fd_read_status(fdc)) {
			if (!rdsectid && !(fdc->flags & FDC_NODMA))
				isa_dmadone(idf, bp->bio_data + fd->skip,
					    format ? bp->bio_bcount : fdblk,
					    fdc->dmachan);
			if (fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			return (retrier(fdc));
  		}

		fdc->state = IOTIMEDOUT;

		/* FALLTHROUGH */
	case IOTIMEDOUT:
		if (!rdsectid && !(fdc->flags & FDC_NODMA))
			isa_dmadone(idf, bp->bio_data + fd->skip,
				format ? bp->bio_bcount : fdblk, fdc->dmachan);
		if (fdc->status[0] & NE7_ST0_IC) {
                        if ((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
			    && fdc->status[1] & NE7_ST1_OR) {
                                /*
				 * DMA overrun. Someone hogged the bus and
				 * didn't release it in time for the next
				 * FDC transfer.
				 *
				 * We normally restart this without bumping
				 * the retry counter.  However, in case
				 * something is seriously messed up (like
				 * broken hardware), we rather limit the
				 * number of retries so the IO operation
				 * doesn't block indefinately.
				 */
				if (fdc->dma_overruns++ < FDC_DMAOV_MAX) {
					fdc->state = SEEKCOMPLETE;
					return (1);/* will return immediately */
				} /* else fall through */
                        }
			if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_IV
				&& fdc->retry < 6)
				fdc->retry = 6;	/* force a reset */
			else if((fdc->status[0] & NE7_ST0_IC) == NE7_ST0_IC_AT
				&& fdc->status[2] & NE7_ST2_WC
				&& fdc->retry < 3)
				fdc->retry = 3;	/* force recalibrate */
			return (retrier(fdc));
		}
		/* All OK */
		if (rdsectid) {
			/* copy out ID field contents */
			idp = (struct fdc_readid *)bp->bio_data;
			idp->cyl = fdc->status[3];
			idp->head = fdc->status[4];
			idp->sec = fdc->status[5];
			idp->secshift = fdc->status[6];
		}
		/* Operation successful, retry DMA overruns again next time. */
		fdc->dma_overruns = 0;
		fd->skip += fdblk;
		if (!rdsectid && !format && fd->skip < bp->bio_bcount) {
			/* set up next transfer */
			fdc->state = DOSEEK;
		} else {
			/* ALL DONE */
			fd->skip = 0;
			bp->bio_resid = 0;
			fdc->bp = NULL;
			device_unbusy(fd->dev);
			biofinish(bp, fd->device_stats, 0);
			fdc->fd = (fd_p) 0;
			fdc->fdu = -1;
			fdc->state = FINDWORK;
		}
		return (1);	/* will return immediately */

	case RESETCTLR:
		fdc_reset(fdc);
		fdc->retry++;
		fdc->state = RESETCOMPLETE;
		return (0);	/* will return later */

	case RESETCOMPLETE:
		/*
		 * Discard all the results from the reset so that they
		 * can't cause an unexpected interrupt later.
		 */
		for (i = 0; i < 4; i++)
			(void)fd_sense_int(fdc, &st0, &cyl);
		fdc->state = STARTRECAL;
		/* FALLTHROUGH */
	case STARTRECAL:
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
				return (0); /* will return later */
			if(fdc->fdct == FDC_NE765
			   && (st0 & NE7_ST0_IC) == NE7_ST0_IC_RC)
				return (0); /* hope for a real intr */
		} while ((st0 & NE7_ST0_IC) == NE7_ST0_IC_RC);
		if ((st0 & NE7_ST0_IC) != NE7_ST0_IC_NT || cyl != 0)
		{
			if(fdc->retry > 3)
				/*
				 * A recalibrate from beyond cylinder 77
				 * will "fail" due to the FDC limitations;
				 * since people used to complain much about
				 * the failure message, try not logging
				 * this one if it seems to be the first
				 * time in a line.
				 */
				printf("fd%d: recal failed ST0 %b cyl %d\n",
				       fdu, st0, NE7_ST0BITS, cyl);
			if(fdc->retry < 3) fdc->retry = 3;
			return (retrier(fdc));
		}
		fd->track = 0;
		/* Seek (probably) necessary */
		fdc->state = DOSEEK;
		return (1);	/* will return immediately */

	case MOTORWAIT:
		if(fd->flags & FD_MOTOR_WAIT)
		{
			return (0); /* time's not up yet */
		}
		if (fdc->flags & FDC_NEEDS_RESET) {
			fdc->state = RESETCTLR;
			fdc->flags &= ~FDC_NEEDS_RESET;
		} else
			fdc->state = DOSEEK;
		return (1);	/* will return immediately */

	default:
		device_printf(fdc->fdc_dev, "unexpected FD int->");
		if (fd_read_status(fdc) == 0)
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
			return (0); /* will return later */
		}
		printf("ST0 = %x, PCN = %x\n", st0, cyl);
		return (0);	/* will return later */
	}
	/* noone should ever get here */
}

static int
retrier(struct fdc_data *fdc)
{
	struct bio *bp;
	struct fd_data *fd;
	int fdu;

	bp = fdc->bp;

	/* XXX shouldn't this be cached somewhere?  */
	fdu = FDUNIT(minor(bp->bio_dev));
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
		if ((fd->options & FDOPT_NOERRLOG) == 0) {
			disk_err(bp, "hard error",
			    fdc->fd->skip / DEV_BSIZE, 0);
			if (fdc->flags & FDC_STAT_VALID) {
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
		if ((fd->options & FDOPT_NOERROR) == 0) {
			bp->bio_flags |= BIO_ERROR;
			bp->bio_error = EIO;
			bp->bio_resid = bp->bio_bcount - fdc->fd->skip;
		} else
			bp->bio_resid = 0;
		fdc->bp = NULL;
		fdc->fd->skip = 0;
		device_unbusy(fd->dev);
		biofinish(bp, fdc->fd->device_stats, 0);
		fdc->state = FINDWORK;
		fdc->flags |= FDC_NEEDS_RESET;
		fdc->fd = (fd_p) 0;
		fdc->fdu = -1;
		return (1);
	}
	fdc->retry++;
	return (1);
}

static void
fdbiodone(struct bio *bp)
{
	wakeup(bp);
}

static int
fdmisccmd(dev_t dev, u_int cmd, void *data)
{
 	fdu_t fdu;
 	fd_p fd;
	struct bio *bp;
	struct fd_formb *finfo;
	struct fdc_readid *idfield;
	size_t fdblk;
	int error;

 	fdu = FDUNIT(minor(dev));
	fd = devclass_get_softc(fd_devclass, fdu);
	fdblk = 128 << fd->ft->secsize;
	finfo = (struct fd_formb *)data;
	idfield = (struct fdc_readid *)data;

	bp = malloc(sizeof(struct bio), M_TEMP, M_WAITOK | M_ZERO);

	/*
	 * Set up a bio request for fdstrategy().  bio_blkno is faked
	 * so that fdstrategy() will seek to the the requested
	 * cylinder, and use the desired head.
	 */
	bp->bio_cmd = cmd;
	if (cmd == FDBIO_FORMAT) {
		bp->bio_blkno =
		    (finfo->cyl * (fd->ft->sectrac * fd->ft->heads) +
		     finfo->head * fd->ft->sectrac) *
		    fdblk / DEV_BSIZE;
		bp->bio_bcount = sizeof(struct fd_idfield_data) *
		    finfo->fd_formb_nsecs;
	} else if (cmd == FDBIO_RDSECTID) {
		bp->bio_blkno =
		    (idfield->cyl * (fd->ft->sectrac * fd->ft->heads) +
		     idfield->head * fd->ft->sectrac) *
		    fdblk / DEV_BSIZE;
		bp->bio_bcount = sizeof(struct fdc_readid);
	} else
		panic("wrong cmd in fdmisccmd()");
	bp->bio_data = data;
	bp->bio_dev = dev;
	bp->bio_done = fdbiodone;
	bp->bio_flags = 0;

	/* Now run the command. */
	fdstrategy(bp);
	error = biowait(bp, "fdcmd");

	free(bp, M_TEMP);
	return (error);
}

static int
fdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
 	fdu_t fdu;
 	fd_p fd;
	struct fdc_status *fsp;
	struct fdc_readid *rid;
	int error, type;

 	fdu = FDUNIT(minor(dev));
	type = FDTYPE(minor(dev));
 	fd = devclass_get_softc(fd_devclass, fdu);

	/*
	 * First, handle everything that could be done with
	 * FD_NONBLOCK still being set.
	 */
	switch (cmd) {

	case DIOCGMEDIASIZE:
		if (fd->ft == 0)
			return ((fd->flags & FD_NONBLOCK) ? EAGAIN : ENXIO);
		*(off_t *)addr = (128 << (fd->ft->secsize)) * fd->ft->size;
		return (0);

	case DIOCGSECTORSIZE:
		if (fd->ft == 0)
			return ((fd->flags & FD_NONBLOCK) ? EAGAIN : ENXIO);
		*(u_int *)addr = 128 << (fd->ft->secsize);
		return (0);

	case FIONBIO:
		if (*(int *)addr != 0)
			fd->flags |= FD_NONBLOCK;
		else {
			if (fd->ft == 0) {
				/*
				 * No drive type has been selected yet,
				 * cannot turn FNONBLOCK off.
				 */
				return (EINVAL);
			}
			fd->flags &= ~FD_NONBLOCK;
		}
		return (0);

	case FIOASYNC:
		/* keep the generic fcntl() code happy */
		return (0);

	case FD_GTYPE:                  /* get drive type */
		if (fd->ft == 0)
			/* no type known yet, return the native type */
			*(struct fd_type *)addr = fd_native_types[fd->type];
		else
			*(struct fd_type *)addr = *fd->ft;
		return (0);

	case FD_STYPE:                  /* set drive type */
		if (type == 0) {
			/*
			 * Allow setting drive type temporarily iff
			 * currently unset.  Used for fdformat so any
			 * user can set it, and then start formatting.
			 */
			if (fd->ft)
				return (EINVAL); /* already set */
			fd->ft = fd->fts;
			*fd->ft = *(struct fd_type *)addr;
			fd->flags |= FD_UA;
		} else {
			/*
			 * Set density definition permanently.  Only
			 * allow for superuser.
			 */
			if (suser(td) != 0)
				return (EPERM);
			fd->fts[type] = *(struct fd_type *)addr;
		}
		return (0);

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd->options + (type == 0? FDOPT_AUTOSEL: 0);
		return (0);

	case FD_SOPTS:			/* set drive options */
		fd->options = *(int *)addr & ~FDOPT_AUTOSEL;
		return (0);

#ifdef FDC_DEBUG
	case FD_DEBUG:
		if ((fd_debug != 0) != (*(int *)addr != 0)) {
			fd_debug = (*(int *)addr != 0);
			printf("fd%d: debugging turned %s\n",
			    fd->fdu, fd_debug ? "on" : "off");
		}
		return (0);
#endif

	case FD_CLRERR:
		if (suser(td) != 0)
			return (EPERM);
		fd->fdc->fdc_errs = 0;
		return (0);

	case FD_GSTAT:
		fsp = (struct fdc_status *)addr;
		if ((fd->fdc->flags & FDC_STAT_VALID) == 0)
			return (EINVAL);
		memcpy(fsp->status, fd->fdc->status, 7 * sizeof(u_int));
		return (0);

	case FD_GDTYPE:
		*(enum fd_drivetype *)addr = fd->type;
		return (0);
	}

	/*
	 * Now handle everything else.  Make sure we have a valid
	 * drive type.
	 */
	if (fd->flags & FD_NONBLOCK)
		return (EAGAIN);
	if (fd->ft == 0)
		return (ENXIO);
	error = 0;

	switch (cmd) {

	case FD_FORM:
		if ((flag & FWRITE) == 0)
			return (EBADF);	/* must be opened for writing */
		if (((struct fd_formb *)addr)->format_version !=
		    FD_FORMAT_VERSION)
			return (EINVAL); /* wrong version of formatting prog */
		error = fdmisccmd(dev, FDBIO_FORMAT, addr);
		break;

	case FD_GTYPE:                  /* get drive type */
		*(struct fd_type *)addr = *fd->ft;
		break;

	case FD_STYPE:                  /* set drive type */
		/* this is considered harmful; only allow for superuser */
		if (suser(td) != 0)
			return (EPERM);
		*fd->ft = *(struct fd_type *)addr;
		break;

	case FD_GOPTS:			/* get drive options */
		*(int *)addr = fd->options;
		break;

	case FD_SOPTS:			/* set drive options */
		fd->options = *(int *)addr;
		break;

#ifdef FDC_DEBUG
	case FD_DEBUG:
		if ((fd_debug != 0) != (*(int *)addr != 0)) {
			fd_debug = (*(int *)addr != 0);
			printf("fd%d: debugging turned %s\n",
			    fd->fdu, fd_debug ? "on" : "off");
		}
		break;
#endif

	case FD_CLRERR:
		if (suser(td) != 0)
			return (EPERM);
		fd->fdc->fdc_errs = 0;
		break;

	case FD_GSTAT:
		fsp = (struct fdc_status *)addr;
		if ((fd->fdc->flags & FDC_STAT_VALID) == 0)
			return (EINVAL);
		memcpy(fsp->status, fd->fdc->status, 7 * sizeof(u_int));
		break;

	case FD_READID:
		rid = (struct fdc_readid *)addr;
		if (rid->cyl > MAX_CYLINDER || rid->head > MAX_HEAD)
			return (EINVAL);
		error = fdmisccmd(dev, FDBIO_RDSECTID, addr);
		break;

	default:
		error = ENOTTY;
		break;
	}
	return (error);
}
