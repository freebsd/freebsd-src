/*-
 * cyclades cyclom-y serial driver
 *	Andrew Herbert <andrew@werple.apana.org.au>, 17 August 1993
 *
 * Copyright (c) 1993 Andrew Herbert.
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
 * 3. The name Andrew Herbert may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL I BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: cy.c,v 1.23 1995/12/06 23:42:34 bde Exp $
 */

#include "cy.h"
#if NCY > 0
/*
 * TODO:
 * Check that cy16's work.
 * Implement BREAK.
 * Fix overflows when closing line.
 * Atomic COR change.
 * Don't report individual ports in devconf; busy flag for board should be
 * union of the current individual busy flags.
 * Consoles.
 */

/*
 * Temporary compile-time configuration options.
 */
#define	RxFifoThreshold	(CD1400_RX_FIFO_SIZE / 2)
			/* Number of chars in the receiver FIFO before an
			 * an interrupt is generated.  Should depend on
			 * line speed.  Needs to be about 6 on a 486DX33
			 * for 4 active ports at 115200 bps.  Why doesn't
			 * 10 work?
			 */
#define	PollMode	/* Use polling-based irq service routine, not the
			 * hardware svcack lines.  Must be defined for
			 * Cyclom-16Y boards.  Less efficient for Cyclom-8Ys,
			 * and stops 4 * 115200 bps from working.
			 */
#undef	Smarts		/* Enable slightly more CD1400 intelligence.  Mainly
			 * the output CR/LF processing, plus we can avoid a
			 * few checks usually done in ttyinput().
			 *
			 * XXX not fully implemented, and not particularly
			 * worthwhile.
			 */
#undef	CyDebug		/* Include debugging code (not very expensive). */

/* These will go away. */
#undef	SOFT_CTS_OFLOW
#define	SOFT_HOTCHAR

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/devconf.h>

#include <machine/clock.h>

#include <i386/isa/icu.h>	/* XXX just to get at `imen' */
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/cyreg.h>
#include <i386/isa/ic/cd1400.h>

/*
 * Dictionary so that I can name everything *sio* or *com* to compare with
 * sio.c.  There is also lots of ugly formatting and unnecessary ifdefs to
 * simplify the comparision.  These will go away.
 */
#define	LSR_BI		CD1400_RDSR_BREAK
#define	LSR_FE		CD1400_RDSR_FE
#define	LSR_OE		CD1400_RDSR_OE
#define	LSR_PE		CD1400_RDSR_PE
#define	MCR_DTR		CD1400_MSVR2_DTR
#define	MCR_RTS		CD1400_MSVR1_RTS
#define	MSR_CTS		CD1400_MSVR2_CTS
#define	MSR_DCD		CD1400_MSVR2_CD
#define	MSR_DSR		CD1400_MSVR2_DSR
#define	MSR_RI		CD1400_MSVR2_RI
#define	NSIO		(NCY * CY_MAX_PORTS)
#define	comconsole	cyconsole
#define	comdefaultrate	cydefaultrate
#define	com_events	cy_events
#define	comhardclose	cyhardclose
#define	commajor	cymajor
#define	commctl		cymctl
#define	comparam	cyparam
#define	comspeed	cyspeed
#define	comstart	cystart
#define	comwakeup	cywakeup
#define	kdc_sio		kdc_cy
#define	nsio_tty	ncy_tty
#define	p_com_addr	p_cy_addr
#define	sioattach	cyattach
#define	sioclose	cyclose
#define	siodevtotty	cydevtotty
#define	siodriver	cydriver
#define	siodtrwakeup	cydtrwakeup
#define	sioioctl	cyioctl
#define	siointr		cyintr
#define	siointr1	cyintr1
#define	siointrts	cyintrts
#define	sioopen		cyopen
#define	siopoll		cypoll
#define	sioprobe	cyprobe
#define	sioread		cyread
#define	sioregisterdev	cyregisterdev
#define	siosettimeout	cysettimeout
#define	siostop		cystop
#define	siowrite	cywrite
#define	sio_timeout	cy_timeout
#define	sio_timeouts_until_log	cy_timeouts_until_log
#define	sio_tty		cy_tty

#define	CY_MAX_PORTS		(CD1400_NO_OF_CHANNELS * CY_MAX_CD1400s)

/* We encode the cyclom unit number (cyu) in spare bits in the IVR's. */
#define	CD1400_xIVR_CHAN_SHIFT	3
#define	CD1400_xIVR_CHAN	0x0F	/* XXX reduce to pack Cyclom-8Ys */

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */
#define	RB_I_HIGH_WATER	(TTYHOG - 2 * RS_IBUFSIZE)
#define	RS_IBUFSIZE	256

#define	CALLOUT_MASK		0x80
#define	CONTROL_MASK		0x60
#define	CONTROL_INIT_STATE	0x20
#define	CONTROL_LOCK_STATE	0x40
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	((mynor) & ~MINOR_MAGIC_MASK)

/*
 * Input buffer watermarks.
 * The external device is asked to stop sending when the buffer exactly reaches
 * high water, or when the high level requests it.
 * The high level is notified immediately (rather than at a later clock tick)
 * when this watermark is reached.
 * The buffer size is chosen so the watermark should almost never be reached.
 * The low watermark is invisibly 0 since the buffer is always emptied all at
 * once.
 */
#define	RS_IHIGHWATER (3 * RS_IBUFSIZE / 4)

/*
 * com state bits.
 * (CS_BUSY | CS_TTGO) and (CS_BUSY | CS_TTGO | CS_ODEVREADY) must be higher
 * than the other bits so that they can be tested as a group without masking
 * off the low bits.
 *
 * The following com and tty flags correspond closely:
 *	CS_BUSY		= TS_BUSY (maintained by comstart(), siopoll() and
 *				   siostop())
 *	CS_TTGO		= ~TS_TTSTOP (maintained by comparam() and comstart())
 *	CS_CTS_OFLOW	= CCTS_OFLOW (maintained by comparam())
 *	CS_RTS_IFLOW	= CRTS_IFLOW (maintained by comparam())
 * TS_FLUSH is not used.
 * XXX I think TIOCSETA doesn't clear TS_TTSTOP when it clears IXON.
 * XXX CS_*FLOW should be CF_*FLOW in com->flags (control flags not state).
 */
#define	CS_BUSY		0x80	/* output in progress */
#define	CS_TTGO		0x40	/* output not stopped by XOFF */
#define	CS_ODEVREADY	0x20	/* external device h/w ready (CTS) */
#define	CS_CHECKMSR	1	/* check of MSR scheduled */
#define	CS_CTS_OFLOW	2	/* use CTS output flow control */
#define	CS_DTR_OFF	0x10	/* DTR held off */
#define	CS_ODONE	4	/* output completed */
#define	CS_RTS_IFLOW	8	/* use RTS input flow control */

static	char const * const	error_desc[] = {
#define	CE_OVERRUN			0
	"silo overflow",
#define	CE_INTERRUPT_BUF_OVERFLOW	1
	"interrupt-level buffer overflow",
#define	CE_TTY_BUF_OVERFLOW		2
	"tty-level buffer overflow",
};

#define	CE_NTYPES			3
#define	CE_RECORD(com, errnum)		(++(com)->delta_error_counts[errnum])

/* types.  XXX - should be elsewhere */
typedef u_char	bool_t;		/* boolean */
typedef u_char volatile *cy_addr;

/* queue of linear buffers */
struct lbq {
	u_char	*l_head;	/* next char to process */
	u_char	*l_tail;	/* one past the last char to process */
	struct lbq *l_next;	/* next in queue */
	bool_t	l_queued;	/* nonzero if queued */
};

/* com device structure */
struct com_s {
	u_char	state;		/* miscellaneous flag bits */
	bool_t  active_out;	/* nonzero if the callout device is open */
#if 0
	u_char	cfcr_image;	/* copy of value written to CFCR */
	u_char	ftl;		/* current rx fifo trigger level */
	u_char	ftl_init;	/* ftl_max for next open() */
	u_char	ftl_max;	/* maximum ftl for curent open() */
	bool_t	hasfifo;	/* nonzero for 16550 UARTs */
	bool_t	loses_outints;	/* nonzero if device loses output interrupts */
#endif
	u_char	mcr_image;	/* copy of value written to MCR */
#if 0
#ifdef COM_MULTIPORT
	bool_t	multiport;	/* is this unit part of a multiport device? */
#endif /* COM_MULTIPORT */
	bool_t	no_irq;		/* nonzero if irq is not attached */
	bool_t	poll;		/* nonzero if polling is required */
	bool_t	poll_output;	/* nonzero if polling for output is required */
#endif
	int	unit;		/* unit	number */
	int	dtr_wait;	/* time to hold DTR down on close (* 1/hz) */
#if 0
	u_int	tx_fifo_size;
#endif
	u_int	wopeners;	/* # processes waiting for DCD in open() */

	/*
	 * The high level of the driver never reads status registers directly
	 * because there would be too many side effects to handle conveniently.
	 * Instead, it reads copies of the registers stored here by the
	 * interrupt handler.
	 */
	u_char	last_modem_status;	/* last MSR read by intr handler */
	u_char	prev_modem_status;	/* last MSR handled by high level */

	u_char	hotchar;	/* ldisc-specific char to be handled ASAP */
	u_char	*ibuf;		/* start of input buffer */
	u_char	*ibufend;	/* end of input buffer */
	u_char	*ihighwater;	/* threshold in input buffer */
	u_char	*iptr;		/* next free spot in input buffer */

	struct lbq	obufq;	/* head of queue of output buffers */
	struct lbq	obufs[2];	/* output buffers */

	cy_addr	cy_iobase;	/* base address of this port's cyclom */
	cy_addr	iobase;		/* base address of this port's cd1400 */

	struct tty	*tp;	/* cross reference */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

	bool_t	do_timestamp;
	struct timeval	timestamp;

	u_long	bytes_in;	/* statistics */
	u_long	bytes_out;
	u_int	delta_error_counts[CE_NTYPES];
	u_long	error_counts[CE_NTYPES];

	u_int	recv_exception;	/* exception chars received */
	u_int	mdm;		/* modem signal changes */
#ifdef CyDebug
	u_int	start_count;	/* no. of calls to comstart() */
	u_int	start_real;	/* no. of calls that did something */
#endif
	u_char	channel_control;/* CD1400 CCR control command shadow */
	u_char	cor[3];		/* CD1400 COR1-3 shadows */
	u_char	intr_enable;	/* CD1400 SRER shadow */

	/*
	 * Ping-pong input buffers.  The extra factor of 2 in the sizes is
	 * to allow for an error byte for each input byte.
	 */
#define	CE_INPUT_OFFSET		RS_IBUFSIZE
	u_char	ibuf1[2 * RS_IBUFSIZE];
	u_char	ibuf2[2 * RS_IBUFSIZE];

	/*
	 * Data area for output buffers.  Someday we should build the output
	 * buffer queue without copying data.
	 */
	u_char	obuf1[256];
	u_char	obuf2[256];

#ifdef DEVFS
	void *devfs_token; /* one for now */
#endif

	struct kern_devconf kdc;
};

/*
 * XXX public functions in drivers should be declared in headers produced
 * by `config', not here.
 */

/* Interrupt handling entry points. */
inthand2_t	siointrts;
void	siopoll		__P((void));

/* Device switch entry points. */
#define	sioreset	noreset
#define	siommap		nommap
#define	siostrategy	nostrategy

static	int	sioattach	__P((struct isa_device *dev));
static	void	cd1400_channel_cmd __P((cy_addr iobase, int cmd));
static	timeout_t siodtrwakeup;
static	void	comhardclose	__P((struct com_s *com));
static	void	siointr1	__P((struct com_s *com));
static	int	commctl		__P((struct com_s *com, int bits, int how));
static	int	comparam	__P((struct tty *tp, struct termios *t));
static	int	sioprobe	__P((struct isa_device *dev));
static	void	sioregisterdev	__P((struct isa_device *id));
static	void	siosettimeout	__P((void));
static	int	comspeed	__P((speed_t speed, int *prescaler_io));
static	void	comstart	__P((struct tty *tp));
static	timeout_t comwakeup;
static	void	disc_optim	__P((struct tty	*tp, struct termios *t,
				     struct com_s *com));

#ifdef CyDebug
void	cystatus	__P((int unit));
#endif

/* table and macro for fast conversion from a unit number to its com struct */
static	struct com_s	*p_com_addr[NSIO];
#define	com_addr(unit)	(p_com_addr[unit])

static  struct timeval	intr_timestamp;

static	d_open_t	cyopen;
static	d_close_t	cyclose;
static	d_read_t	cyread;
static	d_write_t	cywrite;
static	d_ioctl_t	cyioctl;
static	d_stop_t	cystop;
static	d_ttycv_t	cydevtotty;

#define CDEV_MAJOR 48
struct cdevsw cy_cdevsw = 
	{ cyopen,	cyclose,	cyread,		cywrite,	/*48*/
	  cyioctl,	cystop,		nxreset,	cydevtotty,/*cyclades*/
	  ttselect,	nxmmap,		NULL, "cy",	NULL,	-1 };


struct isa_driver	siodriver = {
	sioprobe, sioattach, "cy"
};

static	int	comconsole = -1;
static	speed_t	comdefaultrate = TTYDEF_SPEED;
static	u_int	com_events;	/* input chars + weighted output completions */
static	int	commajor;
static	int	sio_timeout;
static	int	sio_timeouts_until_log;
#if 0 /* XXX */
static struct tty	*sio_tty[NSIO];
#else
static struct tty	sio_tty[NSIO];
static	int	nsio_tty = NSIO;
#endif

#ifdef KGDB
#include <machine/remote-sl.h>

extern	int	kgdb_dev;
extern	int	kgdb_rate;
extern	int	kgdb_debug_init;
#endif

#ifdef CyDebug
static	u_int	cd_inbs;
static	u_int	cy_inbs;
static	u_int	cd_outbs;
static	u_int	cy_outbs;
static	u_int	cy_svrr_probes;
static	u_int	cy_timeouts;
#endif

static	int	cy_nr_cd1400s[NCY];
#undef	RxFifoThreshold
static	int	volatile RxFifoThreshold = (CD1400_RX_FIFO_SIZE / 2);

#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

static struct kern_devconf kdc_sio[NCY] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"cyc", 0, { MDDT_ISA, 0, "tty" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* state */
	"Cyclades multiport board",
	DC_CLS_MISC		/* just an ordinary device */
} };

static void
sioregisterdev(id)
	struct isa_device *id;
{
	int	unit;

	unit = id->id_unit;
	if (unit != 0)
		kdc_sio[unit] = kdc_sio[0];
	kdc_sio[unit].kdc_unit = unit;
	kdc_sio[unit].kdc_isa = id;
	dev_attach(&kdc_sio[unit]);
}

static int
sioprobe(dev)
	struct isa_device	*dev;
{
	int	cyu;
	u_char	firmware_version;
	cy_addr	iobase;
	int	unit;

	iobase = (cy_addr)dev->id_maddr;
	unit = dev->id_unit;
	if ((u_int)unit >= NCY)
		return (0);
	cy_nr_cd1400s[unit] = 0;
	sioregisterdev(dev);


	/* Cyclom-16Y hardware reset (Cyclom-8Ys don't care) */
	cy_inb(iobase, CY16_RESET);	/* XXX? */
	DELAY(500);	/* wait for the board to get its act together */

	/* this is needed to get the board out of reset */
	cy_outb(iobase, CY_CLEAR_INTR, 0);
	DELAY(500);

	for (cyu = 0; cyu < CY_MAX_CD1400s;
	    ++cyu, iobase += CY_CD1400_MEMSIZE) {
		int	i;

		/* wait for chip to become ready for new command */
		for (i = 0; i < 10; i++) {
			DELAY(50);
			if (!cd_inb(iobase, CD1400_CCR))
				break;
		}

		/* clear the GFRCR register */
		cd_outb(iobase, CD1400_GFRCR, 0);

		/* issue a reset command */
		cd_outb(iobase, CD1400_CCR,
			CD1400_CCR_CMDRESET | CD1400_CCR_FULLRESET);

		/* wait for the CD1400 to initialize itself */
		for (i = 0; i < 200; i++) {
			DELAY(50);

			/* retrieve firmware version */
			firmware_version = cd_inb(iobase, CD1400_GFRCR);
			if ((firmware_version & 0xf0) == 0x40)
				break;
		}

		/*
		 * Anything in the 0x40-0x4F range is fine.
		 * If one CD1400 is bad then we don't support higher
		 * numbered good ones on this board.
		 */
		if ((firmware_version & 0xf0) != 0x40)
			break;
		++cy_nr_cd1400s[unit];
	}
	return (cy_nr_cd1400s[unit] == 0 ? 0 : -1);
}

static int
sioattach(isdp)
	struct isa_device	*isdp;
{
	int	cyu;
	cy_addr	cy_iobase;
	cy_addr	iobase;
	int	ncyu;
	int	unit;
	char name [32];

	unit = isdp->id_unit;
	if ((u_int)unit >= NCY)
		return (0);
	ncyu = cy_nr_cd1400s[unit];
	if (ncyu == 0)
		return (0);
	isdp->id_ri_flags |= RI_FAST;

	cy_iobase = (cy_addr)isdp->id_maddr;
	unit *= CY_MAX_PORTS;
	for (cyu = 0, iobase = cy_iobase; cyu < ncyu;
	     ++cyu, iobase += CY_CD1400_MEMSIZE) {
		int	cdu;

		/* Set up a receive timeout period of than 1+ ms. */
		cd_outb(iobase, CD1400_PPR,
			howmany(CY_CLOCK / CD1400_PPR_PRESCALER, 1000));

		for (cdu = 0; cdu < CD1400_NO_OF_CHANNELS; ++cdu, ++unit) {
			struct com_s	*com;
			int		s;

	com = malloc(sizeof *com, M_DEVBUF, M_NOWAIT);
	if (com == NULL)
		break;
	bzero(com, sizeof *com);
	com->unit = unit;
	com->dtr_wait = 3 * hz;
	com->iptr = com->ibuf = com->ibuf1;
	com->ibufend = com->ibuf1 + RS_IBUFSIZE;
	com->ihighwater = com->ibuf1 + RS_IHIGHWATER;
	com->obufs[0].l_head = com->obuf1;
	com->obufs[1].l_head = com->obuf2;

			com->cy_iobase = cy_iobase;
	com->iobase = iobase;

	/*
	 * We don't use all the flags from <sys/ttydefaults.h> since they
	 * are only relevant for logins.  It's important to have echo off
	 * initially so that the line doesn't start blathering before the
	 * echo flag can be turned off.
	 */
	com->it_in.c_iflag = 0;
	com->it_in.c_oflag = 0;
	com->it_in.c_cflag = TTYDEF_CFLAG;
	com->it_in.c_lflag = 0;
	if (unit == comconsole) {
		com->it_in.c_iflag = TTYDEF_IFLAG;
		com->it_in.c_oflag = TTYDEF_OFLAG;
		com->it_in.c_cflag = TTYDEF_CFLAG | CLOCAL;
		com->it_in.c_lflag = TTYDEF_LFLAG;
		com->lt_out.c_cflag = com->lt_in.c_cflag = CLOCAL;
	}
	termioschars(&com->it_in);
	com->it_in.c_ispeed = com->it_in.c_ospeed = comdefaultrate;
	com->it_out = com->it_in;

			com->kdc = kdc_sio[0];
			com->kdc.kdc_name = "cy";
			com->kdc.kdc_unit = unit;
			com->kdc.kdc_isa = isdp;
			com->kdc.kdc_parent = &kdc_sio[isdp->id_unit];
			com->kdc.kdc_state = DC_IDLE;
			com->kdc.kdc_description =
			  "Serial port: Cirrus Logic CD1400";
			com->kdc.kdc_class = DC_CLS_SERIAL;
			dev_attach(&com->kdc);

	s = spltty();
	com_addr(unit) = com;
	splx(s);
#ifdef DEVFS
/* XXX */ /* Fix this when you work out what the f*ck it looks like */
			sprintf(name, "cy%d", unit);
			com->devfs_token =
				devfs_add_devsw( "/", name, &cy_cdevsw, unit,
						DV_CHR, 0,  0, 0600);
#endif
		}
	}
	kdc_sio[isdp->id_unit].kdc_state = DC_BUSY;	/* XXX */

	/* ensure an edge for the next interrupt */
	cy_outb(cy_iobase, CY_CLEAR_INTR, 0);

	return (1);
}

static int
sioopen(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	cy_addr		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIO || (com = com_addr(unit)) == NULL)
		return (ENXIO);
	if (mynor & CONTROL_MASK)
		return (0);
#if 0 /* XXX */
	tp = com->tp = sio_tty[unit] = ttymalloc(sio_tty[unit]);
#else
	tp = com->tp = &sio_tty[unit];
#endif
	s = spltty();
	/*
	 * We jump to this label after all non-interrupted sleeps to pick
	 * up any changes of the device state.
	 */
open_top:
	while (com->state & CS_DTR_OFF) {
		error = tsleep(&com->dtr_wait, TTIPRI | PCATCH, "cydtr", 0);
		if (error != 0)
			goto out;
	}
	com->kdc.kdc_state = DC_BUSY;
	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialized.
		 * Handle conflicts.
		 */
		if (mynor & CALLOUT_MASK) {
			if (!com->active_out) {
				error = EBUSY;
				goto out;
			}
		} else {
			if (com->active_out) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto out;
				}
				error =	tsleep(&com->active_out,
					       TTIPRI | PCATCH, "cybi", 0);
				if (error != 0)
					goto out;
				goto open_top;
			}
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it.  Initialization is done twice in many
		 * cases: to preempt sleeping callin opens if we are
		 * callout, and to complete a callin open after DCD rises.
		 */
		tp->t_oproc = comstart;
		tp->t_param = comparam;
		tp->t_dev = dev;
		tp->t_termios = mynor & CALLOUT_MASK
				? com->it_out : com->it_in;
#if 0
		(void)commctl(com, TIOCM_DTR | TIOCM_RTS, DMSET);
		com->ftl_max = com->ftl_init;
		com->poll = com->no_irq;
		com->poll_output = com->loses_outints;
#endif
		++com->wopeners;
		iobase = com->iobase;

		/* reset this channel */
		cd_outb(iobase, CD1400_CAR, unit & CD1400_CAR_CHAN);
		cd1400_channel_cmd(iobase, CD1400_CCR_CMDRESET);

		/*
		 * Resetting disables the transmitter and receiver as well as
		 * flushing the fifos so some of our cached state becomes
		 * invalid.  The documentation suggests that all registers
		 * for the current channel are reset to defaults, but
		 * apparently none are.  We wouldn't want DTR cleared.
		 */
		com->channel_control = 0;

		/* Encode per-board unit in LIVR for access in intr routines. */
		cd_outb(iobase, CD1400_LIVR,
			(unit & CD1400_xIVR_CHAN) << CD1400_xIVR_CHAN_SHIFT);

		/*
		 * raise dtr and generally set things up correctly.  this
		 * has the side-effect of selecting the appropriate cd1400
		 * channel, to help us with subsequent channel control stuff
		 */
		error = comparam(tp, &tp->t_termios);
		--com->wopeners;
		if (error != 0)
			goto out;
		/*
		 * XXX we should goto open_top if comparam() slept.
		 */
		ttsetwater(tp);
#if 0
		if (com->hasfifo) {
			/*
			 * (Re)enable and drain fifos.
			 *
			 * Certain SMC chips cause problems if the fifos
			 * are enabled while input is ready.  Turn off the
			 * fifo if necessary to clear the input.  We test
			 * the input ready bit after enabling the fifos
			 * since we've already enabled them in comparam()
			 * and to handle races between enabling and fresh
			 * input.
			 */
			while (TRUE) {
				outb(iobase + com_fifo,
				     FIFO_RCV_RST | FIFO_XMT_RST
				     | FIFO_ENABLE | com->ftl);
				DELAY(100);
				if (!(inb(com->line_status_port) & LSR_RXRDY))
					break;
				outb(iobase + com_fifo, 0);
				DELAY(100);
				(void) inb(com->data_port);
			}
		}

		disable_intr();
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
		com->prev_modem_status = com->last_modem_status
		    = inb(com->modem_status_port);
		outb(iobase + com_ier, IER_ERXRDY | IER_ETXRDY | IER_ERLS
				       | IER_EMSC);
		enable_intr();
#else /* !0 */
		/* XXX raise RTS too */
		(void)commctl(com, TIOCM_DTR | TIOCM_RTS, DMSET);
		disable_intr();
		com->prev_modem_status = com->last_modem_status
		    = cd_inb(iobase, CD1400_MSVR2);
		cd_outb(iobase, CD1400_SRER,
			com->intr_enable
			    = CD1400_SRER_MDMCH | CD1400_SRER_RXDATA);
		enable_intr();
#endif /* 0 */
		/*
		 * Handle initial DCD.  Callout devices get a fake initial
		 * DCD (trapdoor DCD).  If we are callout, then any sleeping
		 * callin opens get woken up and resume sleeping on "cybi"
		 * instead of "cydcd".
		 */
		/*
		 * XXX `mynor & CALLOUT_MASK' should be
		 * `tp->t_cflag & (SOFT_CARRIER | TRAPDOOR_CARRIER) where
		 * TRAPDOOR_CARRIER is the default initial state for callout
		 * devices and SOFT_CARRIER is like CLOCAL except it hides
		 * the true carrier.
		 */
		if (com->prev_modem_status & MSR_DCD || mynor & CALLOUT_MASK)
			(*linesw[tp->t_line].l_modem)(tp, 1);
	}
	/*
	 * Wait for DCD if necessary.
	 */
	if (!(tp->t_state & TS_CARR_ON) && !(mynor & CALLOUT_MASK)
	    && !(tp->t_cflag & CLOCAL) && !(flag & O_NONBLOCK)) {
		++com->wopeners;
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "cydcd", 0);
		--com->wopeners;
		if (error != 0)
			goto out;
		goto open_top;
	}
	error =	(*linesw[tp->t_line].l_open)(dev, tp);
	disc_optim(tp, &tp->t_termios, com);
	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		com->active_out = TRUE;
	siosettimeout();
out:
	splx(s);
	if (!(tp->t_state & TS_ISOPEN) && com->wopeners == 0)
		comhardclose(com);
	return (error);
}

static int
sioclose(dev, flag, mode, p)
	dev_t		dev;
	int		flag;
	int		mode;
	struct proc	*p;
{
	struct com_s	*com;
	int		mynor;
	int		s;
	struct tty	*tp;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (0);
	com = com_addr(MINOR_TO_UNIT(mynor));
	tp = com->tp;
	s = spltty();
	(*linesw[tp->t_line].l_close)(tp, flag);
	disc_optim(tp, &tp->t_termios, com);
	siostop(tp, FREAD | FWRITE);
	comhardclose(com);
	ttyclose(tp);
	siosettimeout();
	splx(s);
#ifdef broken /* session holds a ref to the tty; can't deallocate */
	ttyfree(tp);
	com->tp = sio_tty[unit] = NULL;
#endif
	return (0);
}

static void
comhardclose(com)
	struct com_s	*com;
{
	cy_addr		iobase;
	int		s;
	struct tty	*tp;
	int		unit;

	unit = com->unit;
	iobase = com->iobase;
	s = spltty();
#if 0
	com->poll = FALSE;
	com->poll_output = FALSE;
#endif
	com->do_timestamp = 0;
	cd_outb(iobase, CD1400_CAR, unit & CD1400_CAR_CHAN);
#if 0
	outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
#endif

#ifdef KGDB
	/* do not disable interrupts or hang up if debugging */
	if (kgdb_dev != makedev(commajor, unit))
#endif
	{
#if 0
		outb(iobase + com_ier, 0);
#else
		disable_intr();
		cd_outb(iobase, CD1400_SRER, com->intr_enable = 0);
		enable_intr();
#endif
		tp = com->tp;
		if (tp->t_cflag & HUPCL
		    /*
		     * XXX we will miss any carrier drop between here and the
		     * next open.  Perhaps we should watch DCD even when the
		     * port is closed; it is not sufficient to check it at
		     * the next open because it might go up and down while
		     * we're not watching.
		     */
		    || !com->active_out
		       && !(com->prev_modem_status & MSR_DCD)
		       && !(com->it_in.c_cflag & CLOCAL)
		    || !(tp->t_state & TS_ISOPEN)) {
			(void)commctl(com, TIOCM_DTR, DMBIC);

			/* Disable receiver (leave transmitter enabled). */
			com->channel_control = CD1400_CCR_CMDCHANCTL
					       | CD1400_CCR_XMTEN
					       | CD1400_CCR_RCVDIS;
			cd1400_channel_cmd(iobase, com->channel_control);

			if (com->dtr_wait != 0) {
				timeout(siodtrwakeup, com, com->dtr_wait);
				com->state |= CS_DTR_OFF;
			}
		}
	}
	com->active_out = FALSE;
	wakeup(&com->active_out);
	wakeup(TSA_CARR_ON(tp));	/* restart any wopeners */
	if (!(com->state & CS_DTR_OFF) && unit != comconsole)
		com->kdc.kdc_state = DC_IDLE;
	splx(s);
}

static int
sioread(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct tty	*tp;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);
	tp = com_addr(MINOR_TO_UNIT(mynor))->tp;
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}

static int
siowrite(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);

	unit = MINOR_TO_UNIT(mynor);
	tp = com_addr(unit)->tp;
	/*
	 * (XXX) We disallow virtual consoles if the physical console is
	 * a serial port.  This is in case there is a display attached that
	 * is not the console.  In that situation we don't need/want the X
	 * server taking over the console.
	 */
	if (constty != NULL && unit == comconsole)
		constty = NULL;
#ifdef Smarts
	/* XXX duplicate ttwrite(), but without so much output processing on
	 * CR & LF chars.  Hardly worth the effort, given that high-throughput
	 * sessions are raw anyhow.
	 */
#else
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
#endif
}

static void
siodtrwakeup(chan)
	void	*chan;
{
	struct com_s	*com;

	com = (struct com_s *)chan;
	com->state &= ~CS_DTR_OFF;
	if (com->unit != comconsole)
		com->kdc.kdc_state = DC_IDLE;
	wakeup(&com->dtr_wait);
}

/* Interrupt routine for timekeeping purposes */
void
siointrts(unit)
	int	unit;
{
	/*
	 * XXX microtime() reenables CPU interrupts.  We can't afford to
	 * be interrupted and don't want to slow down microtime(), so lock
	 * out interrupts in another way.
	 */
	outb(IO_ICU1 + 1, 0xff);
	microtime(&intr_timestamp);
	disable_intr();
	outb(IO_ICU1 + 1, imen);

	siointr(unit);
}

void
siointr(unit)
	int	unit;
{
	int	baseu;
	cy_addr	cy_iobase;
	int	cyu;
	cy_addr	iobase;
	u_char	status;

	baseu = unit * CY_MAX_PORTS;
	cy_iobase = com_addr(baseu)->cy_iobase;

	/* check each CD1400 in turn */
	for (cyu = 0, iobase = cy_iobase; cyu < cy_nr_cd1400s[unit];
	     ++cyu, iobase += CY_CD1400_MEMSIZE) {
		/* poll to see if it has any work */
		status = cd_inb(iobase, CD1400_SVRR);
		if (status == 0)
			continue;
#ifdef CyDebug
		++cy_svrr_probes;
#endif
		/* service requests as appropriate, giving priority to RX */
		if (status & CD1400_SVRR_RXRDY) {
			struct com_s	*com;
			u_int		count;
			u_char		*ioptr;
			u_char		line_status;
			u_char		recv_data;
			u_char		serv_type;
#ifdef PollMode
			u_char		save_car;
			u_char		save_rir;
#endif

#ifdef PollMode
			save_rir = cd_inb(iobase, CD1400_RIR);
			save_car = cd_inb(iobase, CD1400_CAR);

			/* enter rx service */
			cd_outb(iobase, CD1400_CAR, save_rir);

			serv_type = cd_inb(iobase, CD1400_RIVR);
			com = com_addr(baseu
				       + ((serv_type >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#else
			/* ack receive service */
			serv_type = cy_inb(iobase, CY8_SVCACKR);

			com = com_addr(baseu +
				       + ((serv_type >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif

	if (com->do_timestamp)
		/* XXX a little bloat here... */
		com->timestamp = intr_timestamp;

		if (serv_type & CD1400_RIVR_EXCEPTION) {
			++com->recv_exception;
			line_status = cd_inb(iobase, CD1400_RDSR);
			/* break/unnattached error bits or real input? */
			recv_data = cd_inb(iobase, CD1400_RDSR);
#ifndef SOFT_HOTCHAR
			if (line_status & CD1400_RDSR_SPECIAL
			    && com->hotchar != 0)
				setsofttty();
#endif
#if 1 /* XXX "intelligent" PFO error handling would break O error handling */
			if (line_status & (LSR_PE|LSR_FE|LSR_BI)) {
				/*
				  Don't store PE if IGNPAR and BI if IGNBRK,
				  this hack allows "raw" tty optimization
				  works even if IGN* is set.
				*/
				if (   com->tp == NULL
				    || !(com->tp->t_state & TS_ISOPEN)
				    || (line_status & (LSR_PE|LSR_FE))
				    &&  (com->tp->t_iflag & IGNPAR)
				    || (line_status & LSR_BI)
				    &&  (com->tp->t_iflag & IGNBRK))
					goto cont;
				if (   (line_status & (LSR_PE|LSR_FE))
				    && (com->tp->t_state & TS_CAN_BYPASS_L_RINT)
				    && ((line_status & LSR_FE)
				    ||  (line_status & LSR_PE)
				    &&  (com->tp->t_iflag & INPCK)))
					recv_data = 0;
			}
#endif /* 1 */
			++com->bytes_in;
#ifdef SOFT_HOTCHAR
			if (com->hotchar != 0 && recv_data == com->hotchar)
				setsofttty();
#endif
			ioptr = com->iptr;
			if (ioptr >= com->ibufend)
				CE_RECORD(com, CE_INTERRUPT_BUF_OVERFLOW);
			else {
				++com_events;
				ioptr[0] = recv_data;
				ioptr[CE_INPUT_OFFSET] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#if 0
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
#else
					cd_outb(iobase, CD1400_MSVR1,
						com->mcr_image &= ~MCR_RTS);
#endif
				if (line_status & LSR_OE)
					CE_RECORD(com, CE_OVERRUN);
			}
			goto cont;
		} else {
			int	ifree;

			count = cd_inb(iobase, CD1400_RDCR);
			com->bytes_in += count;
			ioptr = com->iptr;
			ifree = com->ibufend - ioptr;
			if (count > ifree) {
				count -= ifree;
				com_events += ifree;
				while (ifree-- != 0) {
					recv_data = cd_inb(iobase, CD1400_RDSR);
#ifdef SOFT_HOTCHAR
					if (com->hotchar != 0
					    && recv_data == com->hotchar)
						setsofttty();
#endif
					ioptr[0] = recv_data;
					ioptr[CE_INPUT_OFFSET] = 0;
					++ioptr;
				}
				com->delta_error_counts
				    [CE_INTERRUPT_BUF_OVERFLOW] += count;
				do {
					recv_data = cd_inb(iobase, CD1400_RDSR);
#ifdef SOFT_HOTCHAR
					if (com->hotchar != 0
					    && recv_data == com->hotchar)
						setsofttty();
#endif
				} while (--count != 0);
			} else {
				if (ioptr <= com->ihighwater
				    && ioptr + count > com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#if 0
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
#else
					cd_outb(iobase, CD1400_MSVR1,
						com->mcr_image &= ~MCR_RTS);
#endif
				com_events += count;
				do {
					recv_data = cd_inb(iobase, CD1400_RDSR);
#ifdef SOFT_HOTCHAR
					if (com->hotchar != 0
					    && recv_data == com->hotchar)
						setsofttty();
#endif
					ioptr[0] = recv_data;
					ioptr[CE_INPUT_OFFSET] = 0;
					++ioptr;
				} while (--count != 0);
			}
			com->iptr = ioptr;
		}
cont:

			/* terminate service context */
#ifdef PollMode
			cd_outb(iobase, CD1400_RIR,
				save_rir
				& ~(CD1400_RIR_RDIREQ | CD1400_RIR_RBUSY));
			cd_outb(iobase, CD1400_CAR, save_car);
#else
			cd_outb(iobase, CD1400_EOSRR, 0);
#endif
		}
		if (status & CD1400_SVRR_MDMCH) {
			struct com_s	*com;
			u_char	modem_status;
#ifdef PollMode
			u_char	save_car;
			u_char	save_mir;
#else
			u_char	vector;
#endif

#ifdef PollMode
			save_mir = cd_inb(iobase, CD1400_MIR);
			save_car = cd_inb(iobase, CD1400_CAR);

			/* enter modem service */
			cd_outb(iobase, CD1400_CAR, save_mir);

			com = com_addr(baseu + cyu * CD1400_NO_OF_CHANNELS
				       + (save_mir & CD1400_MIR_CHAN));
#else
			/* ack modem service */
			vector = cy_inb(iobase, CY8_SVCACKM);

			com = com_addr(baseu
				       + ((vector >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif
			++com->mdm;
			modem_status = cd_inb(iobase, CD1400_MSVR2);
		if (modem_status != com->last_modem_status) {
			/*
			 * Schedule high level to handle DCD changes.  Note
			 * that we don't use the delta bits anywhere.  Some
			 * UARTs mess them up, and it's easy to remember the
			 * previous bits and calculate the delta.
			 */
			com->last_modem_status = modem_status;
			if (!(com->state & CS_CHECKMSR)) {
				com_events += LOTS_OF_EVENTS;
				com->state |= CS_CHECKMSR;
				setsofttty();
			}

#ifdef SOFT_CTS_OFLOW
			/* handle CTS change immediately for crisp flow ctl */
			if (com->state & CS_CTS_OFLOW) {
				if (modem_status & MSR_CTS) {
					com->state |= CS_ODEVREADY;
					if (com->state >= (CS_BUSY | CS_TTGO
							   | CS_ODEVREADY)
					    && !(com->intr_enable
						 & CD1400_SRER_TXRDY))
						cd_outb(iobase, CD1400_SRER,
							com->intr_enable
							|= CD1400_SRER_TXRDY);
				} else {
					com->state &= ~CS_ODEVREADY;
					if (com->intr_enable & CD1400_SRER_TXRDY)
						cd_outb(iobase, CD1400_SRER,
							com->intr_enable
							&= ~CD1400_SRER_TXRDY);
				}
			}
#endif
		}

			/* terminate service context */
#ifdef PollMode
			cd_outb(iobase, CD1400_MIR,
				save_mir
				& ~(CD1400_MIR_RDIREQ | CD1400_MIR_RBUSY));
			cd_outb(iobase, CD1400_CAR, save_car);
#else
			cd_outb(iobase, CD1400_EOSRR, 0);
#endif
		}
		if (status & CD1400_SVRR_TXRDY) {
			struct com_s	*com;
#ifdef PollMode
			u_char	save_car;
			u_char	save_tir;
#else
			u_char	vector;
#endif

#ifdef PollMode
			save_tir = cd_inb(iobase, CD1400_TIR);
			save_car = cd_inb(iobase, CD1400_CAR);

			/* enter tx service */
			cd_outb(iobase, CD1400_CAR, save_tir);
			com = com_addr(baseu
				       + cyu * CD1400_NO_OF_CHANNELS
				       + (save_tir & CD1400_TIR_CHAN));
#else
			/* ack transmit service */
			vector = cy_inb(iobase, CY8_SVCACKT);

			com = com_addr(baseu
				       + ((vector >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif

		if (com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
			u_char	*ioptr;
			u_int	ocount;

			ioptr = com->obufq.l_head;
				ocount = com->obufq.l_tail - ioptr;
				if (ocount > CD1400_TX_FIFO_SIZE)
					ocount = CD1400_TX_FIFO_SIZE;
				com->bytes_out += ocount;
				do
					cd_outb(iobase, CD1400_TDR, *ioptr++);
				while (--ocount != 0);
			com->obufq.l_head = ioptr;
			if (ioptr >= com->obufq.l_tail) {
				struct lbq	*qp;

				qp = com->obufq.l_next;
				qp->l_queued = FALSE;
				qp = qp->l_next;
				if (qp != NULL) {
					com->obufq.l_head = qp->l_head;
					com->obufq.l_tail = qp->l_tail;
					com->obufq.l_next = qp;
				} else {
					/* output just completed */
					com->state &= ~CS_BUSY;
					cd_outb(iobase, CD1400_SRER,
						com->intr_enable
						&= ~CD1400_SRER_TXRDY);
				}
				if (!(com->state & CS_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->state |= CS_ODONE;
					setsofttty();	/* handle at high level ASAP */
				}
			}
		}

			/* terminate service context */
#ifdef PollMode
			cd_outb(iobase, CD1400_TIR,
				save_tir
				& ~(CD1400_TIR_RDIREQ | CD1400_TIR_RBUSY));
			cd_outb(iobase, CD1400_CAR, save_car);
#else
			cd_outb(iobase, CD1400_EOSRR, 0);
#endif
		}
	}

	/* ensure an edge for the next interrupt */
	cy_outb(cy_iobase, CY_CLEAR_INTR, 0);

	schedsofttty();
}

static void
siointr1(com)
	struct com_s	*com;
{
}

static int
sioioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	int		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	cy_addr		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	int		oldcmd;
	struct termios	term;
#endif

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
	iobase = com->iobase;
	if (mynor & CONTROL_MASK) {
		struct termios	*ct;

		switch (mynor & CONTROL_MASK) {
		case CONTROL_INIT_STATE:
			ct = mynor & CALLOUT_MASK ? &com->it_out : &com->it_in;
			break;
		case CONTROL_LOCK_STATE:
			ct = mynor & CALLOUT_MASK ? &com->lt_out : &com->lt_in;
			break;
		default:
			return (ENODEV);	/* /dev/nodev */
		}
		switch (cmd) {
		case TIOCSETA:
			error = suser(p->p_ucred, &p->p_acflag);
			if (error != 0)
				return (error);
			*ct = *(struct termios *)data;
			return (0);
		case TIOCGETA:
			*(struct termios *)data = *ct;
			return (0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return (0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return (0);
		default:
			return (ENOTTY);
		}
	}
	tp = com->tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t)&term;
#endif
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int	cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt = mynor & CALLOUT_MASK
				     ? &com->lt_out : &com->lt_in;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
			      | (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
			      | (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
			      | (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
			      | (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		return (error);
	s = spltty();
	error = ttioctl(tp, cmd, data, flag);
	disc_optim(tp, &tp->t_termios, com);
	if (error >= 0) {
		splx(s);
		return (error);
	}
	cd_outb(iobase, CD1400_CAR, MINOR_TO_UNIT(mynor) & CD1400_CAR_CHAN);
	switch (cmd) {
#if 0
	case TIOCSBRK:
		outb(iobase + com_cfcr, com->cfcr_image |= CFCR_SBREAK);
		break;
	case TIOCCBRK:
		outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
		break;
#endif /* 0 */
	case TIOCSDTR:
		(void)commctl(com, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		(void)commctl(com, TIOCM_DTR, DMBIC);
		break;
	case TIOCMSET:
		(void)commctl(com, *(int *)data, DMSET);
		break;
	case TIOCMBIS:
		(void)commctl(com, *(int *)data, DMBIS);
		break;
	case TIOCMBIC:
		(void)commctl(com, *(int *)data, DMBIC);
		break;
	case TIOCMGET:
		*(int *)data = commctl(com, 0, DMGET);
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0) {
			splx(s);
			return (error);
		}
		com->dtr_wait = *(int *)data * hz / 100;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = com->dtr_wait * 100 / hz;
		break;
	case TIOCTIMESTAMP:
		com->do_timestamp = TRUE;
		*(struct timeval *)data = com->timestamp;
		break;
	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}

void
siopoll()
{
	int		unit;

#ifdef CyDebug
	++cy_timeouts;
#endif
	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < NSIO; ++unit) {
		u_char		*buf;
		struct com_s	*com;
		u_char		*ibuf;
		cy_addr		iobase;
		int		incc;
		struct tty	*tp;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		tp = com->tp;
		if (tp == NULL) {
			/*
			 * XXX forget any events related to closed devices
			 * (actually never opened devices) so that we don't
			 * loop.
			 */
			disable_intr();
			incc = com->iptr - com->ibuf;
			com->iptr = com->ibuf;
			if (com->state & CS_CHECKMSR) {
				incc += LOTS_OF_EVENTS;
				com->state &= ~CS_CHECKMSR;
			}
			com_events -= incc;
			enable_intr();
			if (incc != 0)
				log(LOG_DEBUG,
				    "sio%d: %d events for device with no tp\n",
				    unit, incc);
			continue;
		}

		/* switch the role of the low-level input buffers */
		if (com->iptr == (ibuf = com->ibuf)) {
			buf = NULL;     /* not used, but compiler can't tell */
			incc = 0;
		} else {
			buf = ibuf;
			disable_intr();
			incc = com->iptr - buf;
			com_events -= incc;
			if (ibuf == com->ibuf1)
				ibuf = com->ibuf2;
			else
				ibuf = com->ibuf1;
			com->ibufend = ibuf + RS_IBUFSIZE;
			com->ihighwater = ibuf + RS_IHIGHWATER;
			com->iptr = ibuf;

			/*
			 * There is now room for another low-level buffer full
			 * of input, so enable RTS if it is now disabled and
			 * there is room in the high-level buffer.
			 */
			/*
			 * XXX this used not to look at CS_RTS_IFLOW.  The
			 * change is to allow full control of MCR_RTS via
			 * ioctls after turning CS_RTS_IFLOW off.  Check
			 * for races.  We shouldn't allow the ioctls while
			 * CS_RTS_IFLOW is on.
			 */
			if ((com->state & CS_RTS_IFLOW)
			    && !(com->mcr_image & MCR_RTS)
			    && !(tp->t_state & TS_TBLOCK))
#if 0
				outb(com->modem_ctl_port,
				     com->mcr_image |= MCR_RTS);
#else
				iobase = com->iobase,
				cd_outb(iobase, CD1400_CAR,
					unit & CD1400_CAR_CHAN),
				cd_outb(iobase, CD1400_MSVR1,
					com->mcr_image |= MCR_RTS);
#endif
			enable_intr();
			com->ibuf = ibuf;
		}

		if (com->state & CS_CHECKMSR) {
			u_char	delta_modem_status;

			disable_intr();
			delta_modem_status = com->last_modem_status
					     ^ com->prev_modem_status;
			com->prev_modem_status = com->last_modem_status;
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_CHECKMSR;
			enable_intr();
			if (delta_modem_status & MSR_DCD)
				(*linesw[tp->t_line].l_modem)
					(tp, com->prev_modem_status & MSR_DCD);
		}
		if (com->state & CS_ODONE) {
			disable_intr();
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_ODONE;
			if (!(com->state & CS_BUSY))
				com->tp->t_state &= ~TS_BUSY;
			enable_intr();
			(*linesw[tp->t_line].l_start)(tp);
		}
		if (incc <= 0 || !(tp->t_state & TS_ISOPEN))
			continue;
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode.  It usually takes about 450
		 * instructions (that's without canonical processing or echo!).
		 * slinput is reasonably fast (usually 40 instructions plus
		 * call overhead).
		 */
		if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
			if (tp->t_rawq.c_cc + incc >= RB_I_HIGH_WATER
			    && (com->state & CS_RTS_IFLOW
				|| tp->t_iflag & IXOFF)
			    && !(tp->t_state & TS_TBLOCK))
				ttyblock(tp);
			tk_nin += incc;
			tk_rawcc += incc;
			tp->t_rawcc += incc;
			com->delta_error_counts[CE_TTY_BUF_OVERFLOW]
				+= b_to_q((char *)buf, incc, &tp->t_rawq);
			ttwakeup(tp);
			if (tp->t_state & TS_TTSTOP
			    && (tp->t_iflag & IXANY
				|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
				tp->t_state &= ~TS_TTSTOP;
				tp->t_lflag &= ~FLUSHO;
				comstart(tp);
			}
		} else {
			do {
				u_char	line_status;
				int	recv_data;

				line_status = (u_char) buf[CE_INPUT_OFFSET];
				recv_data = (u_char) *buf++;
				if (line_status
				    & (LSR_BI | LSR_FE | LSR_OE | LSR_PE)) {
					if (line_status & LSR_BI)
						recv_data |= TTY_BI;
					if (line_status & LSR_FE)
						recv_data |= TTY_FE;
					if (line_status & LSR_OE)
						recv_data |= TTY_OE;
					if (line_status & LSR_PE)
						recv_data |= TTY_PE;
				}
				(*linesw[tp->t_line].l_rint)(recv_data, tp);
			} while (--incc > 0);
		}
		if (com_events == 0)
			break;
	}
	if (com_events >= LOTS_OF_EVENTS)
		goto repeat;
}

static int
comparam(tp, t)
	struct tty	*tp;
	struct termios	*t;
{
	int		bits;
	int		cflag;
	struct com_s	*com;
	u_char		cor_change;
	int		idivisor;
	int		iflag;
	cy_addr		iobase;
	int		iprescaler;
	int		itimeout;
	int		odivisor;
	int		oprescaler;
	u_char		opt;
	int		s;
	int		unit;

	/* do historical conversions */
	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	/* check requested parameters */
	idivisor = comspeed(t->c_ispeed, &iprescaler);
	if (idivisor < 0)
		return (EINVAL);
	odivisor = comspeed(t->c_ospeed, &oprescaler);
	if (odivisor < 0)
		return (EINVAL);

	/* parameters are OK, convert them to the com struct and the device */
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
	s = spltty();
	cd_outb(iobase, CD1400_CAR, unit & CD1400_CAR_CHAN);
	if (odivisor == 0)
		(void)commctl(com, TIOCM_DTR, DMBIC);	/* hang up line */
	else
		(void)commctl(com, TIOCM_DTR, DMBIS);

	if (idivisor != 0) {
		cd_outb(iobase, CD1400_RBPR, idivisor);
		cd_outb(iobase, CD1400_RCOR, iprescaler);
	}
	if (odivisor != 0) {
		cd_outb(iobase, CD1400_TBPR, odivisor);
		cd_outb(iobase, CD1400_TCOR, oprescaler);
	}

	/*
	 * channel control
	 *	receiver enable
	 *	transmitter enable (always set)
	 */
	cflag = t->c_cflag;
	opt = CD1400_CCR_CMDCHANCTL | CD1400_CCR_XMTEN
	      | (cflag & CREAD ? CD1400_CCR_RCVEN : CD1400_CCR_RCVDIS);
	if (opt != com->channel_control) {
		com->channel_control = opt;
		cd1400_channel_cmd(iobase, opt);
	}

#ifdef Smarts
	/* set special chars */
	/* XXX if one is _POSIX_VDISABLE, can't use some others */
	if (t->c_cc[VSTOP] != _POSIX_VDISABLE)
		cd_outb(iobase, CD1400_SCHR1, t->c_cc[VSTOP]);
	if (t->c_cc[VSTART] != _POSIX_VDISABLE)
		cd_outb(iobase, CD1400_SCHR2, t->c_cc[VSTART]);
	if (t->c_cc[VINTR] != _POSIX_VDISABLE)
		cd_outb(iobase, CD1400_SCHR3, t->c_cc[VINTR]);
	if (t->c_cc[VSUSP] != _POSIX_VDISABLE)
		cd_outb(iobase, CD1400_SCHR4, t->c_cc[VSUSP]);
#endif

	/*
	 * set channel option register 1 -
	 *	parity mode
	 *	stop bits
	 *	char length
	 */
	opt = 0;
	/* parity */
	if (cflag & PARENB) {
		if (cflag & PARODD)
			opt |= CD1400_COR1_PARODD;
		opt |= CD1400_COR1_PARNORMAL;
	}
	iflag = t->c_iflag;
	if (!(iflag & INPCK))
		opt |= CD1400_COR1_NOINPCK;
	bits = 1 + 1;
	/* stop bits */
	if (cflag & CSTOPB) {
		++bits;
		opt |= CD1400_COR1_STOP2;
	}
	/* char length */
	switch (cflag & CSIZE) {
	case CS5:
		bits += 5;
		opt |= CD1400_COR1_CS5;
		break;
	case CS6:
		bits += 6;
		opt |= CD1400_COR1_CS6;
		break;
	case CS7:
		bits += 7;
		opt |= CD1400_COR1_CS7;
		break;
	default:
		bits += 8;
		opt |= CD1400_COR1_CS8;
		break;
	}
	cor_change = 0;
	if (opt != com->cor[0]) {
		cor_change |= CD1400_CCR_COR1;
		cd_outb(iobase, CD1400_COR1, com->cor[0] = opt);
	}

	/*
	 * Set receive time-out period, normally to max(one char time, 5 ms).
	 */
	if (t->c_ispeed == 0)
		itimeout = cd_inb(iobase, CD1400_RTPR);
	else {
		itimeout = (1000 * bits + t->c_ispeed - 1) / t->c_ispeed;
#ifdef SOFT_HOTCHAR
#define	MIN_RTP		1
#else
#define	MIN_RTP		5
#endif
		if (itimeout < MIN_RTP)
			itimeout = MIN_RTP;
	}
	if (!(t->c_lflag & ICANON) && t->c_cc[VMIN] != 0 && t->c_cc[VTIME] != 0
	    && t->c_cc[VTIME] * 10 > itimeout)
		itimeout = t->c_cc[VTIME] * 10;
	if (itimeout > 255)
		itimeout = 255;
	cd_outb(iobase, CD1400_RTPR, itimeout);

	/*
	 * set channel option register 2 -
	 *	flow control
	 */
	opt = 0;
#ifdef Smarts
	if (iflag & IXANY)
		opt |= CD1400_COR2_IXANY;
	if (iflag & IXOFF)
		opt |= CD1400_COR2_IXOFF;
#endif
#ifndef SOFT_CTS_OFLOW
	if (cflag & CCTS_OFLOW)
		opt |= CD1400_COR2_CCTS_OFLOW;
#endif
	if (opt != com->cor[1]) {
		cor_change |= CD1400_CCR_COR2;
		cd_outb(iobase, CD1400_COR2, com->cor[1] = opt);
	}

	/*
	 * set channel option register 3 -
	 *	receiver FIFO interrupt threshold
	 *	flow control
	 */
	opt = RxFifoThreshold;
#ifdef Smarts
	if (t->c_lflag & ICANON)
		opt |= CD1400_COR3_SCD34;	/* detect INTR & SUSP chars */
	if (iflag & IXOFF)
		/* detect and transparently handle START and STOP chars */
		opt |= CD1400_COR3_FCT | CD1400_COR3_SCD12;
#endif
	if (opt != com->cor[2]) {
		cor_change |= CD1400_CCR_COR3;
		cd_outb(iobase, CD1400_COR3, com->cor[2] = opt);
	}

	/* notify the CD1400 if COR1-3 have changed */
	if (cor_change)
		cd1400_channel_cmd(iobase, CD1400_CCR_CMDCORCHG | cor_change);

	/*
	 * set channel option register 4 -
	 *	CR/NL processing
	 *	break processing
	 *	received exception processing
	 */
	opt = 0;
	if (iflag & IGNCR)
		opt |= CD1400_COR4_IGNCR;
#ifdef Smarts
	/*
	 * we need a new ttyinput() for this, as we don't want to
	 * have ICRNL && INLCR being done in both layers, or to have
	 * synchronisation problems
	 */
	if (iflag & ICRNL)
		opt |= CD1400_COR4_ICRNL;
	if (iflag & INLCR)
		opt |= CD1400_COR4_INLCR;
#endif
	if (iflag & IGNBRK)
		opt |= CD1400_COR4_IGNBRK;
	if (!(iflag & BRKINT))
		opt |= CD1400_COR4_NOBRKINT;
#if 0
	/* XXX using this "intelligence" breaks reporting of overruns. */
	if (iflag & IGNPAR)
		opt |= CD1400_COR4_PFO_DISCARD;
	else {
		if (iflag & PARMRK)
			opt |= CD1400_COR4_PFO_ESC;
		else
			opt |= CD1400_COR4_PFO_NUL;
	}
#else
	opt |= CD1400_COR4_PFO_EXCEPTION;
#endif
	cd_outb(iobase, CD1400_COR4, opt);

	/*
	 * set channel option register 5 -
	 */
	opt = 0;
	if (iflag & ISTRIP)
		opt |= CD1400_COR5_ISTRIP;
	if (t->c_iflag & IEXTEN)
		/* enable LNEXT (e.g. ctrl-v quoting) handling */
		opt |= CD1400_COR5_LNEXT;
#ifdef Smarts
	if (t->c_oflag & ONLCR)
		opt |= CD1400_COR5_ONLCR;
	if (t->c_oflag & OCRNL)
		opt |= CD1400_COR5_OCRNL;
#endif
	cd_outb(iobase, CD1400_COR5, opt);

	/*
	 * XXX we probably alway want to track carrier changes, so that
	 * TS_CARR_ON gives the true carrier.  If we don't track them,
	 * then we should set TS_CARR_ON when CLOCAL drops.
	 */
	/*
	 * set modem change option register 1
	 *	generate modem interrupts on which 1 -> 0 input transitions
	 *	also controls auto-DTR output flow-control, which we don't use
	 */
	opt = cflag & CLOCAL ? 0 : CD1400_MCOR1_CDzd;
#ifdef SOFT_CTS_OFLOW
	if (cflag & CCTS_OFLOW)
		opt |= CD1400_MCOR1_CTSzd;
#endif
	cd_outb(iobase, CD1400_MCOR1, opt);

	/*
	 * set modem change option register 2
	 *	generate modem interrupts on specific 0 -> 1 input transitions
	 */
	opt = cflag & CLOCAL ? 0 : CD1400_MCOR2_CDod;
#ifdef SOFT_CTS_OFLOW
	if (cflag & CCTS_OFLOW)
		opt |= CD1400_MCOR2_CTSod;
#endif
	cd_outb(iobase, CD1400_MCOR2, opt);

	/*
	 * XXX should have done this long ago, but there is too much state
	 * to change all atomically.
	 */
	disable_intr();

	com->state &= ~CS_TTGO;
	if (!(tp->t_state & TS_TTSTOP))
		com->state |= CS_TTGO;
	if (cflag & CRTS_IFLOW)
		com->state |= CS_RTS_IFLOW;	/* XXX - secondary changes? */
	else
		com->state &= ~CS_RTS_IFLOW;

	/*
	 * Set up state to handle output flow control.
	 * XXX - worth handling MDMBUF (DCD) flow control at the lowest level?
	 * Now has 10+ msec latency, while CTS flow has 50- usec latency.
	 */
	com->state |= CS_ODEVREADY;
#ifdef SOFT_CTS_OFLOW
	com->state &= ~CS_CTS_OFLOW;
	if (cflag & CCTS_OFLOW) {
		com->state |= CS_CTS_OFLOW;
		if (!(com->last_modem_status & MSR_CTS))
			com->state &= ~CS_ODEVREADY;
	}
#endif
	/* XXX shouldn't call functions while intrs are disabled. */
	disc_optim(tp, t, com);
#if 0
	/*
	 * Recover from fiddling with CS_TTGO.  We used to call siointr1()
	 * unconditionally, but that defeated the careful discarding of
	 * stale input in sioopen().
	 */
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);
#endif
	if (com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
		if (!(com->intr_enable & CD1400_SRER_TXRDY))
			cd_outb(iobase, CD1400_SRER,
				com->intr_enable |= CD1400_SRER_TXRDY);
	} else {
		if (com->intr_enable & CD1400_SRER_TXRDY)
			cd_outb(iobase, CD1400_SRER,
				com->intr_enable &= ~CD1400_SRER_TXRDY);
	}

	enable_intr();
	splx(s);
	return (0);
}

static void
comstart(tp)
	struct tty	*tp;
{
	struct com_s	*com;
	cy_addr		iobase;
	int		s;
#ifdef CyDebug
	bool_t		started;
#endif
	int		unit;

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
	s = spltty();

#ifdef CyDebug
	++com->start_count;
	started = FALSE;
#endif

	disable_intr();
	cd_outb(iobase, CD1400_CAR, unit & CD1400_CAR_CHAN);
	if (tp->t_state & TS_TTSTOP) {
		com->state &= ~CS_TTGO;
		if (com->intr_enable & CD1400_SRER_TXRDY)
			cd_outb(iobase, CD1400_SRER,
				com->intr_enable &= ~CD1400_SRER_TXRDY);
	} else {
		com->state |= CS_TTGO;
		if (com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)
		    && !(com->intr_enable & CD1400_SRER_TXRDY))
			cd_outb(iobase, CD1400_SRER,
				com->intr_enable |= CD1400_SRER_TXRDY);
	}
	if (tp->t_state & TS_TBLOCK) {
		if (com->mcr_image & MCR_RTS && com->state & CS_RTS_IFLOW)
#if 0
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
#else
			cd_outb(iobase, CD1400_MSVR1,
				com->mcr_image &= ~MCR_RTS);
#endif
	} else {
		/*
		 * XXX don't raise MCR_RTS if CTS_RTS_IFLOW is off.  Set it
		 * appropriately in comparam() if RTS-flow is being changed.
		 * Check for races.
		 */
		if (!(com->mcr_image & MCR_RTS) && com->iptr < com->ihighwater)
#if 0
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#else
			cd_outb(iobase, CD1400_MSVR1,
				com->mcr_image |= MCR_RTS);
#endif
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc != 0) {
		struct lbq	*qp;
		struct lbq	*next;

		if (!com->obufs[0].l_queued) {
#ifdef CyDebug
			started = TRUE;
#endif
			com->obufs[0].l_tail
			    = com->obuf1 + q_to_b(&tp->t_outq, com->obuf1,
						  sizeof com->obuf1);
			com->obufs[0].l_next = NULL;
			com->obufs[0].l_queued = TRUE;
			disable_intr();
			if (com->state & CS_BUSY) {
				qp = com->obufq.l_next;
				while ((next = qp->l_next) != NULL)
					qp = next;
				qp->l_next = &com->obufs[0];
			} else {
				com->obufq.l_head = com->obufs[0].l_head;
				com->obufq.l_tail = com->obufs[0].l_tail;
				com->obufq.l_next = &com->obufs[0];
				com->state |= CS_BUSY;
				if (com->state >= (CS_BUSY | CS_TTGO
						   | CS_ODEVREADY))
					cd_outb(iobase, CD1400_SRER,
						com->intr_enable
						|= CD1400_SRER_TXRDY);
			}
			enable_intr();
		}
		if (tp->t_outq.c_cc != 0 && !com->obufs[1].l_queued) {
#ifdef CyDebug
			started = TRUE;
#endif
			com->obufs[1].l_tail
			    = com->obuf2 + q_to_b(&tp->t_outq, com->obuf2,
						  sizeof com->obuf2);
			com->obufs[1].l_next = NULL;
			com->obufs[1].l_queued = TRUE;
			disable_intr();
			if (com->state & CS_BUSY) {
				qp = com->obufq.l_next;
				while ((next = qp->l_next) != NULL)
					qp = next;
				qp->l_next = &com->obufs[1];
			} else {
				com->obufq.l_head = com->obufs[1].l_head;
				com->obufq.l_tail = com->obufs[1].l_tail;
				com->obufq.l_next = &com->obufs[1];
				com->state |= CS_BUSY;
				if (com->state >= (CS_BUSY | CS_TTGO
						   | CS_ODEVREADY))
					cd_outb(iobase, CD1400_SRER,
						com->intr_enable
						|= CD1400_SRER_TXRDY);
			}
			enable_intr();
		}
		tp->t_state |= TS_BUSY;
	}
#ifdef CyDebug
	if (started)
		++com->start_real;
#endif
#if 0
	disable_intr();
	if (com->state >= (CS_BUSY | CS_TTGO)) {
		siointr1(com);	/* fake interrupt to start output */
	enable_intr();
#endif
	ttwwakeup(tp);
	splx(s);
}

static void
siostop(tp, rw)
	struct tty	*tp;
	int		rw;
{
	struct com_s	*com;

	com = com_addr(DEV_TO_UNIT(tp->t_dev));
	disable_intr();
	if (rw & FWRITE) {
		com->obufs[0].l_queued = FALSE;
		com->obufs[1].l_queued = FALSE;
		if (com->state & CS_ODONE)
			com_events -= LOTS_OF_EVENTS;
		com->state &= ~(CS_ODONE | CS_BUSY);
		com->tp->t_state &= ~TS_BUSY;
	}
	if (rw & FREAD) {
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	enable_intr();
	comstart(tp);

	/* XXX should clear h/w fifos too. */
}

struct tty *
siodevtotty(dev)
	dev_t	dev;
{
	int	mynor;
	int	unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (NULL);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIO)
		return (NULL);
	return (&sio_tty[unit]);
}

static int
commctl(com, bits, how)
	struct com_s	*com;
	int		bits;
	int		how;
{
	cy_addr	iobase;
	int	mcr;
	int	msr;

	if (how == DMGET) {
		if (com->channel_control & CD1400_CCR_RCVEN)
			bits |= TIOCM_LE;
		mcr = com->mcr_image;
		if (mcr & MCR_DTR)
			bits |= TIOCM_DTR;
		if (mcr & MCR_RTS)
			/* XXX wired on for Cyclom-8Ys */
			bits |= TIOCM_RTS;
		msr = com->prev_modem_status;
		if (msr & MSR_CTS)
			bits |= TIOCM_CTS;
		if (msr & MSR_DCD)
			bits |= TIOCM_CD;
		if (msr & MSR_DSR)
			bits |= TIOCM_DSR;
		if (msr & MSR_RI)
			/* XXX not connected except for Cyclom-16Y? */
			bits |= TIOCM_RI;
		return (bits);
	}
	iobase = com->iobase;
	mcr = 0;
	if (bits & TIOCM_DTR)
		mcr |= MCR_DTR;
	if (bits & TIOCM_RTS)
		mcr |= MCR_RTS;
	disable_intr();
	switch (how) {
	case DMSET:
		com->mcr_image = mcr;
		cd_outb(iobase, CD1400_MSVR1, mcr);
		cd_outb(iobase, CD1400_MSVR2, mcr);
		break;
	case DMBIS:
		com->mcr_image = mcr = com->mcr_image | mcr;
		cd_outb(iobase, CD1400_MSVR1, mcr);
		cd_outb(iobase, CD1400_MSVR2, mcr);
		break;
	case DMBIC:
		com->mcr_image = mcr = com->mcr_image & ~mcr;
		cd_outb(iobase, CD1400_MSVR1, mcr);
		cd_outb(iobase, CD1400_MSVR2, mcr);
		break;
	}
	enable_intr();
	return (0);
}

static void
siosettimeout()
{
	struct com_s	*com;
	bool_t		someopen;
	int		unit;

	/*
	 * Set our timeout period to 1 second if no polled devices are open.
	 * Otherwise set it to max(1/200, 1/hz).
	 * Enable timeouts iff some device is open.
	 */
	untimeout(comwakeup, (void *)NULL);
	sio_timeout = hz;
	someopen = FALSE;
	for (unit = 0; unit < NSIO; ++unit) {
		com = com_addr(unit);
		if (com != NULL && com->tp != NULL
		    && com->tp->t_state & TS_ISOPEN) {
			someopen = TRUE;
#if 0
			if (com->poll || com->poll_output) {
				sio_timeout = hz > 200 ? hz / 200 : 1;
				break;
			}
#endif
		}
	}
	if (someopen) {
		sio_timeouts_until_log = hz / sio_timeout;
		timeout(comwakeup, (void *)NULL, sio_timeout);
	} else {
		/* Flush error messages, if any. */
		sio_timeouts_until_log = 1;
		comwakeup((void *)NULL);
		untimeout(comwakeup, (void *)NULL);
	}
}

static void
comwakeup(chan)
	void	*chan;
{
	struct com_s	*com;
	int		unit;

	timeout(comwakeup, (void *)NULL, sio_timeout);

#if 0
	/*
	 * Recover from lost output interrupts.
	 * Poll any lines that don't use interrupts.
	 */
	for (unit = 0; unit < NSIO; ++unit) {
		com = com_addr(unit);
		if (com != NULL
		    && (com->state >= (CS_BUSY | CS_TTGO) || com->poll)) {
			disable_intr();
			siointr1(com);
			enable_intr();
		}
	}
#endif

	/*
	 * Check for and log errors, but not too often.
	 */
	if (--sio_timeouts_until_log > 0)
		return;
	sio_timeouts_until_log = hz / sio_timeout;
	for (unit = 0; unit < NSIO; ++unit) {
		int	errnum;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		for (errnum = 0; errnum < CE_NTYPES; ++errnum) {
			u_int	delta;
			u_long	total;

			disable_intr();
			delta = com->delta_error_counts[errnum];
			com->delta_error_counts[errnum] = 0;
			enable_intr();
			if (delta == 0)
				continue;
			total = com->error_counts[errnum] += delta;
			log(LOG_ERR, "cy%d: %u more %s%s (total %lu)\n",
			    unit, delta, error_desc[errnum],
			    delta == 1 ? "" : "s", total);
#if 0
			/*
			 * XXX if we resurrect this then we should move
			 * the dropping of the ftl to somewhere with less
			 * latency.
			 */
			if (errnum == CE_OVERRUN && com->hasfifo
			    && com->ftl > FIFO_TRIGGER_1) {
				static	u_char	ftl_in_bytes[] =
					{ 1, 4, 8, 14, };

				com->ftl_init = FIFO_TRIGGER_8;
#define	FIFO_TRIGGER_DELTA	FIFO_TRIGGER_4
				com->ftl_max =
				com->ftl -= FIFO_TRIGGER_DELTA;
				outb(com->iobase + com_fifo,
				     FIFO_ENABLE | com->ftl);
				log(LOG_DEBUG,
				    "sio%d: reduced fifo trigger level to %d\n",
				    unit,
				    ftl_in_bytes[com->ftl
						 / FIFO_TRIGGER_DELTA]);
			}
#endif
		}
	}
}

static void
disc_optim(tp, t, com)
	struct tty	*tp;
	struct termios	*t;
	struct com_s	*com;
{
#ifndef SOFT_HOTCHAR
	cy_addr	iobase;
	u_char	opt;
#endif

	/*
	 * XXX can skip a lot more cases if Smarts.  Maybe
	 * (IGNCR | ISTRIP | IXON) in c_iflag.  But perhaps we
	 * shouldn't skip if (TS_CNTTB | TS_LNCH) is set in t_state.
	 */
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK)
		|| (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
	    && linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;
	/*
	 * Prepare to reduce input latency for packet
	 * discplines with a end of packet character.
	 */
	if (tp->t_line == SLIPDISC)
		com->hotchar = 0xc0;
	else if (tp->t_line == PPPDISC)
		com->hotchar = 0x7e;
	else
		com->hotchar = 0;
#ifndef SOFT_HOTCHAR
	iobase = com->iobase;
	cd_outb(iobase, CD1400_CAR, com->unit & CD1400_CAR_CHAN);
	opt = com->cor[2] & ~CD1400_COR3_SCD34;
	if (com->hotchar != 0) {
		cd_outb(iobase, CD1400_SCHR3, com->hotchar);
		cd_outb(iobase, CD1400_SCHR4, com->hotchar);
		opt |= CD1400_COR3_SCD34;
	}
	if (opt != com->cor[2]) {
		cd_outb(iobase, CD1400_COR3, com->cor[2] = opt);
		cd1400_channel_cmd(com->iobase,
				   CD1400_CCR_CMDCORCHG | CD1400_CCR_COR3);
	}
#endif
}

#ifdef Smarts
/* standard line discipline input routine */
int
cyinput(c, tp)
	int		c;
	struct tty	*tp;
{
	/* XXX duplicate ttyinput(), but without the IXOFF/IXON/ISTRIP/IPARMRK
	 * bits, as they are done by the CD1400.  Hardly worth the effort,
	 * given that high-throughput sessions are raw anyhow.
	 */
}
#endif /* Smarts */

static int
comspeed(speed, prescaler_io)
	speed_t	speed;
	int	*prescaler_io;
{
	int	actual;
	int	error;
	int	divider;
	int	prescaler;
	int	prescaler_unit;

	if (speed == 0)
		return (0);
	if (speed < 0 || speed > 150000)
		return (-1);

	/* determine which prescaler to use */
	for (prescaler_unit = 4, prescaler = 2048; prescaler_unit;
		prescaler_unit--, prescaler >>= 2) {
		if (CY_CLOCK / prescaler / speed > 63)
			break;
	}

	divider = (CY_CLOCK / prescaler * 2 / speed + 1) / 2; /* round off */
	if (divider > 255)
		divider = 255;
	actual = CY_CLOCK/prescaler/divider;
	error = ((actual - speed) * 2000 / speed + 1) / 2;	/* percentage */

	/* 3.0% max error tolerance */
	if (error < -30 || error > 30)
		return (-1);

#if 0
	printf("prescaler = %d (%d)\n", prescaler, prescaler_unit);
	printf("divider = %d (%x)\n", divider, divider);
	printf("actual = %d\n", actual);
	printf("error = %d\n", error);
#endif

	*prescaler_io = prescaler_unit;
	return (divider);
}

static void
cd1400_channel_cmd(iobase, cmd)
	cy_addr	iobase;
	int	cmd;
{
	/* XXX hsu@clinet.fi: This is always more dependent on ISA bus speed,
	   as the card is probed every round?  Replaced delaycount with 8k.
	   Either delaycount has to be implemented in FreeBSD or more sensible
	   way of doing these should be implemented.  DELAY isn't enough here.
	   */
	u_int	maxwait = 5 * 8 * 1024;	/* approx. 5 ms */

	/* wait for processing of previous command to complete */
	while (cd_inb(iobase, CD1400_CCR) && maxwait--)
		;

	if (!maxwait)
		log(LOG_ERR, "cy: channel command timeout (%d loops) - arrgh\n",
		    5 * 8 * 1024);

	cd_outb(iobase, CD1400_CCR, cmd);
}

#ifdef CyDebug
/* useful in ddb */
void
cystatus(unit)
	int	unit;
{
	struct com_s	*com;
	cy_addr		iobase;
	u_int		ocount;
	struct tty	*tp;

	com = com_addr(unit);
	printf("info for channel %d\n", unit);
	printf("------------------\n");
	printf("total cyclom service probes:\t%d\n", cy_svrr_probes);
	printf("calls to upper layer:\t\t%d\n", cy_timeouts);
	if (com == NULL)
		return;
	iobase = com->iobase;
	printf("\n");
	printf("cd1400 base address:\\tt%p\n", iobase);
	cd_outb(iobase, CD1400_CAR, unit & CD1400_CAR_CHAN);
	printf("saved channel_control:\t\t0x%02x\n", com->channel_control);
	printf("saved cor1-3:\t\t\t0x%02x 0x%02x 0x%02x\n",
	       com->cor[0], com->cor[1], com->cor[2]);
	printf("service request enable reg:\t0x%02x (0x%02x cached)\n",
	       cd_inb(iobase, CD1400_SRER), com->intr_enable);
	printf("service request register:\t0x%02x\n",
	       cd_inb(iobase, CD1400_SVRR));
	printf("modem status:\t\t\t0x%02x (0x%02x cached)\n",
	       cd_inb(iobase, CD1400_MSVR2), com->prev_modem_status);
	printf("rx/tx/mdm interrupt registers:\t0x%02x 0x%02x 0x%02x\n",
	       cd_inb(iobase, CD1400_RIR), cd_inb(iobase, CD1400_TIR),
	       cd_inb(iobase, CD1400_MIR));
	printf("\n");
	printf("com state:\t\t\t0x%02x\n", com->state);
	printf("calls to comstart():\t\t%d (%d useful)\n",
	       com->start_count, com->start_real);
	printf("rx buffer chars free:\t\t%d\n", com->iptr - com->ibuf);
	ocount = 0;
	if (com->obufs[0].l_queued)
		ocount += com->obufs[0].l_tail - com->obufs[0].l_head;
	if (com->obufs[1].l_queued)
		ocount += com->obufs[1].l_tail - com->obufs[1].l_head;
	printf("tx buffer chars:\t\t%u\n", ocount);
	printf("received chars:\t\t\t%d\n", com->bytes_in);
	printf("received exceptions:\t\t%d\n", com->recv_exception);
	printf("modem signal deltas:\t\t%d\n", com->mdm);
	printf("transmitted chars:\t\t%d\n", com->bytes_out);
	printf("\n");
	tp = com->tp;
	if (tp != NULL) {
		printf("tty state:\t\t\t0x%08x\n", tp->t_state);
		printf("upper layer queue lengths:\t%d raw, %d canon, %d output\n",
		       tp->t_rawq.c_cc, tp->t_canq.c_cc, tp->t_outq.c_cc);
	} else
		printf("tty state:\t\t\tclosed\n");
}
#endif /* CyDebug */



static cy_devsw_installed = 0;

static void 
cy_drvinit(void *unused)
{
	dev_t dev;

	if( ! cy_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&cy_cdevsw, NULL);
		cy_devsw_installed = 1;
    	}
}

SYSINIT(cydev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,cy_drvinit,NULL)

#endif /* NCY > 0 */
