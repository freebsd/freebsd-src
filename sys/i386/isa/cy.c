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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "cy.h"

/*
 * TODO:
 * Atomic COR change.
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
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/syslog.h>
#include <sys/tty.h>

#include <machine/psl.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/cyreg.h>
#include <i386/isa/ic/cd1400.h>

#ifndef COMPAT_OLDISA
#error "The cy device requires the old isa compatibility shims"
#endif

#ifdef SMP

#include <machine/smptests.h>                   /** xxx_LOCK */

#ifdef USE_COMLOCK
#define COM_LOCK()	mtx_lock_spin(&com_mtx)
#define COM_UNLOCK()	mtx_unlock_spin(&com_mtx)
#else
#define COM_LOCK()
#define COM_UNLOCK()
#endif /* USE_COMLOCK */

#else /* SMP */

#define COM_LOCK()
#define COM_UNLOCK()

#endif /* SMP */

extern struct mtx	com_mtx;

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
#define	commctl		cymctl
#define	comparam	cyparam
#define	comspeed	cyspeed
#define	comstart	cystart
#define	comwakeup	cywakeup
#define	p_com_addr	p_cy_addr
#define	sioattach	cyattach
#define	sioclose	cyclose
#define	siodriver	cydriver
#define	siodtrwakeup	cydtrwakeup
#define	sioinput	cyinput
#define	siointr		cyintr
#define	siointr1	cyintr1
#define	sioioctl	cyioctl
#define	sioopen		cyopen
#define	siopoll		cypoll
#define	sioprobe	cyprobe
#define	siosettimeout	cysettimeout
#define	siosetwater	cysetwater
#define	comstop		cystop
#define	siowrite	cywrite
#define	sio_ih		cy_ih
#define	sio_irec	cy_irec
#define	sio_timeout	cy_timeout
#define	sio_timeout_handle cy_timeout_handle
#define	sio_timeouts_until_log	cy_timeouts_until_log

#define	CY_MAX_PORTS		(CD1400_NO_OF_CHANNELS * CY_MAX_CD1400s)

/* We encode the cyclom unit number (cyu) in spare bits in the IVR's. */
#define	CD1400_xIVR_CHAN_SHIFT	3
#define	CD1400_xIVR_CHAN	0x1F

/*
 * ETC states.  com->etc may also contain a hardware ETC command value,
 * meaning that execution of that command is pending.
 */
#define	ETC_NONE		0	/* we depend on bzero() setting this */
#define	ETC_BREAK_STARTING	1
#define	ETC_BREAK_STARTED	2
#define	ETC_BREAK_ENDING	3
#define	ETC_BREAK_ENDED		4

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */

#define	CALLOUT_MASK		0x80
#define	CONTROL_MASK		0x60
#define	CONTROL_INIT_STATE	0x20
#define	CONTROL_LOCK_STATE	0x40
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
/*
 * Not all of the magic is parametrized in the following macros.  16 and
 * 0xff are related to the bitfields in a udev_t.  CY_MAX_PORTS must be
 * ((0xff & ~MINOR_MAGIC_MASK) + 1) for things to work.
 */
#define	MINOR_TO_UNIT(mynor)	(((mynor) >> 16) * CY_MAX_PORTS \
				 | (((mynor) & 0xff) & ~MINOR_MAGIC_MASK))
#define	UNIT_TO_MINOR(unit)	(((unit) / CY_MAX_PORTS) << 16 \
				 | (((unit) & 0xff) & ~MINOR_MAGIC_MASK))

/*
 * com state bits.
 * (CS_BUSY | CS_TTGO) and (CS_BUSY | CS_TTGO | CS_ODEVREADY) must be higher
 * than the other bits so that they can be tested as a group without masking
 * off the low bits.
 *
 * The following com and tty flags correspond closely:
 *	CS_BUSY		= TS_BUSY (maintained by comstart(), siopoll() and
 *				   comstop())
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
#define	CSE_ODONE	1	/* output transmitted */

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
#endif
	u_char	etc;		/* pending Embedded Transmit Command */
	u_char	extra_state;	/* more flag bits, separate for order trick */
#if 0
	u_char	fifo_image;	/* copy of value written to FIFO */
#endif
	u_char	gfrcr_image;	/* copy of value read from GFRCR */
#if 0
	bool_t	hasfifo;	/* nonzero for 16550 UARTs */
	bool_t	loses_outints;	/* nonzero if device loses output interrupts */
#endif
	u_char	mcr_dtr;	/* MCR bit that is wired to DTR */
	u_char	mcr_image;	/* copy of value written to MCR */
	u_char	mcr_rts;	/* MCR bit that is wired to RTS */
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
	u_char	*ibufold;	/* old input buffer, to be freed */
	u_char	*ihighwater;	/* threshold in input buffer */
	u_char	*iptr;		/* next free spot in input buffer */
	int	ibufsize;	/* size of ibuf (not include error bytes) */
	int	ierroff;	/* offset of error bytes in ibuf */

	struct lbq	obufq;	/* head of queue of output buffers */
	struct lbq	obufs[2];	/* output buffers */

	int	cy_align;	/* index for register alignment */
	cy_addr	cy_iobase;	/* base address of this port's cyclom */
	cy_addr	iobase;		/* base address of this port's cd1400 */
	int	mcr_rts_reg;	/* cd1400 reg number of reg holding mcr_rts */

	struct tty	*tp;	/* cross reference */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

	bool_t	do_timestamp;
	bool_t	do_dcd_timestamp;
	struct timeval	timestamp;
	struct timeval	dcd_timestamp;

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
	u_char	car;		/* CD1400 CAR shadow (if first unit in cd) */
	u_char	channel_control;/* CD1400 CCR control command shadow */
	u_char	cor[3];		/* CD1400 COR1-3 shadows */
	u_char	intr_enable;	/* CD1400 SRER shadow */

	/*
	 * Data area for output buffers.  Someday we should build the output
	 * buffer queue without copying data.
	 */
	u_char	obuf1[256];
	u_char	obuf2[256];
};

/* PCI driver entry point. */
int	cyattach_common(cy_addr cy_iobase, int cy_align);
ointhand2_t	siointr;

static	int	cy_units(cy_addr cy_iobase, int cy_align);
static	int	sioattach(struct isa_device *dev);
static	void	cd1400_channel_cmd(struct com_s *com, int cmd);
static	void	cd1400_channel_cmd_wait(struct com_s *com);
static	void	cd_etc(struct com_s *com, int etc);
static	int	cd_getreg(struct com_s *com, int reg);
static	void	cd_setreg(struct com_s *com, int reg, int val);
static	timeout_t siodtrwakeup;
static	void	comhardclose(struct com_s *com);
static	void	sioinput(struct com_s *com);
#if 0
static	void	siointr1(struct com_s *com);
#endif
static	int	commctl(struct com_s *com, int bits, int how);
static	int	comparam(struct tty *tp, struct termios *t);
static	void	siopoll(void *arg);
static	int	sioprobe(struct isa_device *dev);
static	void	siosettimeout(void);
static	int	siosetwater(struct com_s *com, speed_t speed);
static	int	comspeed(speed_t speed, u_long cy_clock, int *prescaler_io);
static	void	comstart(struct tty *tp);
static	void	comstop(struct tty *tp, int rw);
static	timeout_t comwakeup;
static	void	disc_optim(struct tty *tp, struct termios *t,
		    struct com_s *com);

#ifdef CyDebug
void	cystatus(int unit);
#endif

static char driver_name[] = "cy";

/* table and macro for fast conversion from a unit number to its com struct */
static	struct com_s	*p_com_addr[NSIO];
#define	com_addr(unit)	(p_com_addr[unit])

struct isa_driver	siodriver = {
	INTR_TYPE_TTY | INTR_FAST,
	sioprobe,
	sioattach,
	driver_name
};
COMPAT_ISA_DRIVER(cy, cydriver);	/* XXX */

static	d_open_t	sioopen;
static	d_close_t	sioclose;
static	d_write_t	siowrite;
static	d_ioctl_t	sioioctl;

#define	CDEV_MAJOR	48
static struct cdevsw sio_cdevsw = {
	.d_open =	sioopen,
	.d_close =	sioclose,
	.d_read =	ttyread,
	.d_write =	siowrite,
	.d_ioctl =	sioioctl,
	.d_poll =	ttypoll,
	.d_name =	driver_name,
	.d_maj =	CDEV_MAJOR,
	.d_flags =	D_TTY,
	.d_kqfilter =	ttykqfilter,
};

static	int	comconsole = -1;
static	speed_t	comdefaultrate = TTYDEF_SPEED;
static	u_int	com_events;	/* input chars + weighted output completions */
static	void	*sio_ih;
static	int	sio_timeout;
static	int	sio_timeouts_until_log;
static	struct	callout_handle sio_timeout_handle
    = CALLOUT_HANDLE_INITIALIZER(&sio_timeout_handle);

#ifdef CyDebug
static	u_int	cd_inbs;
static	u_int	cy_inbs;
static	u_int	cd_outbs;
static	u_int	cy_outbs;
static	u_int	cy_svrr_probes;
static	u_int	cy_timeouts;
#endif

static	int	cy_chip_offset[] = {
	0x0000, 0x0400, 0x0800, 0x0c00, 0x0200, 0x0600, 0x0a00, 0x0e00,
};
static	int	cy_nr_cd1400s[NCY];
static	int	cy_total_devices;
#undef	RxFifoThreshold
static	int	volatile RxFifoThreshold = (CD1400_RX_FIFO_SIZE / 2);

static int
sioprobe(dev)
	struct isa_device	*dev;
{
	cy_addr	iobase;

	iobase = (cy_addr)dev->id_maddr;

	/* Cyclom-16Y hardware reset (Cyclom-8Ys don't care) */
	cy_inb(iobase, CY16_RESET, 0);	/* XXX? */
	DELAY(500);	/* wait for the board to get its act together */

	/* this is needed to get the board out of reset */
	cy_outb(iobase, CY_CLEAR_INTR, 0, 0);
	DELAY(500);

	return (cy_units(iobase, 0) == 0 ? 0 : -1);
}

static int
cy_units(cy_iobase, cy_align)
	cy_addr	cy_iobase;
	int	cy_align;
{
	int	cyu;
	u_char	firmware_version;
	int	i;
	cy_addr	iobase;

	for (cyu = 0; cyu < CY_MAX_CD1400s; ++cyu) {
		iobase = cy_iobase + (cy_chip_offset[cyu] << cy_align);

		/* wait for chip to become ready for new command */
		for (i = 0; i < 10; i++) {
			DELAY(50);
			if (!cd_inb(iobase, CD1400_CCR, cy_align))
				break;
		}

		/* clear the GFRCR register */
		cd_outb(iobase, CD1400_GFRCR, cy_align, 0);

		/* issue a reset command */
		cd_outb(iobase, CD1400_CCR, cy_align,
			CD1400_CCR_CMDRESET | CD1400_CCR_FULLRESET);

		/* XXX bogus initialization to avoid a gcc bug/warning. */
		firmware_version = 0;

		/* wait for the CD1400 to initialize itself */
		for (i = 0; i < 200; i++) {
			DELAY(50);

			/* retrieve firmware version */
			firmware_version = cd_inb(iobase, CD1400_GFRCR,
						  cy_align);
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
	}
	return (cyu);
}

static int
sioattach(isdp)
	struct isa_device	*isdp;
{
	int	adapter;

	adapter = cyattach_common((cy_addr) isdp->id_maddr, 0);
	if (adapter < 0)
		return (0);

	/*
	 * XXX
	 * This kludge is to allow ISA/PCI device specifications in the
	 * kernel config file to be in any order.
	 */
	if (isdp->id_unit != adapter) {
		printf("cy%d: attached as cy%d\n", isdp->id_unit, adapter);
		isdp->id_unit = adapter;	/* XXX */
	}
	isdp->id_ointr = siointr;
	/* isdp->id_ri_flags |= RI_FAST; XXX unimplemented - use newbus! */
	return (1);
}

int
cyattach_common(cy_iobase, cy_align)
	cy_addr	cy_iobase;
	int	cy_align;
{
	int	adapter;
	int	cyu;
	u_char	firmware_version;
	cy_addr	iobase;
	int	minorbase;
	int	ncyu;
	int	unit;

	adapter = cy_total_devices;
	if ((u_int)adapter >= NCY) {
		printf(
	"cy%d: can't attach adapter: insufficient cy devices configured\n",
		       adapter);
		return (-1);
	}
	ncyu = cy_units(cy_iobase, cy_align);
	if (ncyu == 0)
		return (-1);
	cy_nr_cd1400s[adapter] = ncyu;
	cy_total_devices++;

	unit = adapter * CY_MAX_PORTS;
	for (cyu = 0; cyu < ncyu; ++cyu) {
		int	cdu;

		iobase = (cy_addr) (cy_iobase
				    + (cy_chip_offset[cyu] << cy_align));
		firmware_version = cd_inb(iobase, CD1400_GFRCR, cy_align);

		/* Set up a receive timeout period of than 1+ ms. */
		cd_outb(iobase, CD1400_PPR, cy_align,
			howmany(CY_CLOCK(firmware_version)
				/ CD1400_PPR_PRESCALER, 1000));

		for (cdu = 0; cdu < CD1400_NO_OF_CHANNELS; ++cdu, ++unit) {
			struct com_s	*com;
			int		s;

	com = malloc(sizeof *com, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (com == NULL)
		break;
	com->unit = unit;
			com->gfrcr_image = firmware_version;
			if (CY_RTS_DTR_SWAPPED(firmware_version)) {
				com->mcr_dtr = MCR_RTS;
				com->mcr_rts = MCR_DTR;
				com->mcr_rts_reg = CD1400_MSVR2;
			} else {
				com->mcr_dtr = MCR_DTR;
				com->mcr_rts = MCR_RTS;
				com->mcr_rts_reg = CD1400_MSVR1;
			}
	com->dtr_wait = 3 * hz;
	com->obufs[0].l_head = com->obuf1;
	com->obufs[1].l_head = com->obuf2;

			com->cy_align = cy_align;
			com->cy_iobase = cy_iobase;
	com->iobase = iobase;
			com->car = ~CD1400_CAR_CHAN;

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
	if (siosetwater(com, com->it_in.c_ispeed) != 0) {
		free(com, M_DEVBUF);
		return (0);
	}
	termioschars(&com->it_in);
	com->it_in.c_ispeed = com->it_in.c_ospeed = comdefaultrate;
	com->it_out = com->it_in;

	s = spltty();
	com_addr(unit) = com;
	splx(s);

	if (sio_ih == NULL) {
		swi_add(&tty_ithd, "tty:cy", siopoll, NULL, SWI_TTY, 0,
		    &sio_ih);
	}
	minorbase = UNIT_TO_MINOR(unit);
	make_dev(&sio_cdevsw, minorbase,
		UID_ROOT, GID_WHEEL, 0600, "ttyc%r%r", adapter,
		unit % CY_MAX_PORTS);
	make_dev(&sio_cdevsw, minorbase | CONTROL_INIT_STATE,
		UID_ROOT, GID_WHEEL, 0600, "ttyic%r%r", adapter,
		unit % CY_MAX_PORTS);
	make_dev(&sio_cdevsw, minorbase | CONTROL_LOCK_STATE,
		UID_ROOT, GID_WHEEL, 0600, "ttylc%r%r", adapter,
		unit % CY_MAX_PORTS);
	make_dev(&sio_cdevsw, minorbase | CALLOUT_MASK,
		UID_UUCP, GID_DIALER, 0660, "cuac%r%r", adapter,
		unit % CY_MAX_PORTS);
	make_dev(&sio_cdevsw, minorbase | CALLOUT_MASK | CONTROL_INIT_STATE,
		UID_UUCP, GID_DIALER, 0660, "cuaic%r%r", adapter,
		unit % CY_MAX_PORTS);
	make_dev(&sio_cdevsw, minorbase | CALLOUT_MASK | CONTROL_LOCK_STATE,
		UID_UUCP, GID_DIALER, 0660, "cualc%r%r", adapter,
		unit % CY_MAX_PORTS);
		}
	}

	/* ensure an edge for the next interrupt */
	cy_outb(cy_iobase, CY_CLEAR_INTR, cy_align, 0);

	return (adapter);
}

static int
sioopen(dev, flag, mode, td)
	dev_t		dev;
	int		flag;
	int		mode;
	struct thread	*td;
{
	struct com_s	*com;
	int		error;
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
	tp = dev->si_tty = com->tp = ttymalloc(com->tp);
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
		if (tp->t_state & TS_XCLUDE &&
		    suser(td)) {
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
		tp->t_stop = comstop;
		tp->t_param = comparam;
		tp->t_dev = dev;
		tp->t_termios = mynor & CALLOUT_MASK
				? com->it_out : com->it_in;

		/* Encode per-board unit in LIVR for access in intr routines. */
		cd_setreg(com, CD1400_LIVR,
			  (unit & CD1400_xIVR_CHAN) << CD1400_xIVR_CHAN_SHIFT);

		(void)commctl(com, TIOCM_DTR | TIOCM_RTS, DMSET);
#if 0
		com->poll = com->no_irq;
		com->poll_output = com->loses_outints;
#endif
		++com->wopeners;
		error = comparam(tp, &tp->t_termios);
		--com->wopeners;
		if (error != 0)
			goto out;
#if 0
		if (com->hasfifo) {
			/*
			 * (Re)enable and flush fifos.
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
				     | com->fifo_image);
				DELAY(100);
				if (!(inb(com->line_status_port) & LSR_RXRDY))
					break;
				outb(iobase + com_fifo, 0);
				DELAY(100);
				(void) inb(com->data_port);
			}
		}

		critical_enter();
		COM_LOCK();
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
		com->prev_modem_status = com->last_modem_status
		    = inb(com->modem_status_port);
		outb(iobase + com_ier, IER_ERXRDY | IER_ETXRDY | IER_ERLS
				       | IER_EMSC);
		COM_UNLOCK();
		critical_exit();
#else /* !0 */
		/*
		 * Flush fifos.  This requires a full channel reset which
		 * also disables the transmitter and receiver.  Recover
		 * from this.
		 */
		cd1400_channel_cmd(com,
				   CD1400_CCR_CMDRESET | CD1400_CCR_CHANRESET);
		cd1400_channel_cmd(com, com->channel_control);

		critical_enter();
		COM_LOCK();
		com->prev_modem_status = com->last_modem_status
		    = cd_getreg(com, CD1400_MSVR2);
		cd_setreg(com, CD1400_SRER,
			  com->intr_enable
			  = CD1400_SRER_MDMCH | CD1400_SRER_RXDATA);
		COM_UNLOCK();
		critical_exit();
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
sioclose(dev, flag, mode, td)
	dev_t		dev;
	int		flag;
	int		mode;
	struct thread	*td;
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
	cd_etc(com, CD1400_ETC_STOPBREAK);
	(*linesw[tp->t_line].l_close)(tp, flag);
	disc_optim(tp, &tp->t_termios, com);
	comstop(tp, FREAD | FWRITE);
	comhardclose(com);
	ttyclose(tp);
	siosettimeout();
	splx(s);
#ifdef broken /* session holds a ref to the tty; can't deallocate */
	ttyfree(tp);
	com->tp = NULL;
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
#if 0
	outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
#else
	/* XXX */
	critical_enter();
	COM_LOCK();
	com->etc = ETC_NONE;
	cd_setreg(com, CD1400_COR2, com->cor[1] &= ~CD1400_COR2_ETC);
	COM_UNLOCK();
	critical_exit();
	cd1400_channel_cmd(com, CD1400_CCR_CMDRESET | CD1400_CCR_FTF);
#endif

	{
#if 0
		outb(iobase + com_ier, 0);
#else
		critical_enter();
		COM_LOCK();
		cd_setreg(com, CD1400_SRER, com->intr_enable = 0);
		COM_UNLOCK();
		critical_exit();
#endif
		tp = com->tp;
		if ((tp->t_cflag & HUPCL)
		    /*
		     * XXX we will miss any carrier drop between here and the
		     * next open.  Perhaps we should watch DCD even when the
		     * port is closed; it is not sufficient to check it at
		     * the next open because it might go up and down while
		     * we're not watching.
		     */
		    || (!com->active_out
		       && !(com->prev_modem_status & MSR_DCD)
		       && !(com->it_in.c_cflag & CLOCAL))
		    || !(tp->t_state & TS_ISOPEN)) {
			(void)commctl(com, TIOCM_DTR, DMBIC);

			/* Disable receiver (leave transmitter enabled). */
			com->channel_control = CD1400_CCR_CMDCHANCTL
					       | CD1400_CCR_XMTEN
					       | CD1400_CCR_RCVDIS;
			cd1400_channel_cmd(com, com->channel_control);

			if (com->dtr_wait != 0 && !(com->state & CS_DTR_OFF)) {
				timeout(siodtrwakeup, com, com->dtr_wait);
				com->state |= CS_DTR_OFF;
			}
		}
	}
#if 0
	if (com->hasfifo) {
		/*
		 * Disable fifos so that they are off after controlled
		 * reboots.  Some BIOSes fail to detect 16550s when the
		 * fifos are enabled.
		 */
		outb(iobase + com_fifo, 0);
	}
#endif
	com->active_out = FALSE;
	wakeup(&com->active_out);
	wakeup(TSA_CARR_ON(tp));	/* restart any wopeners */
	splx(s);
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
	wakeup(&com->dtr_wait);
}

/*
 * This function:
 *  a) needs to be called with COM_LOCK() held, and
 *  b) needs to return with COM_LOCK() held.
 */
static void
sioinput(com)
	struct com_s	*com;
{
	u_char		*buf;
	int		incc;
	u_char		line_status;
	int		recv_data;
	struct tty	*tp;

	buf = com->ibuf;
	tp = com->tp;
	if (!(tp->t_state & TS_ISOPEN)) {
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
		return;
	}
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode.  It usually takes about 450
		 * instructions (that's without canonical processing or echo!).
		 * slinput is reasonably fast (usually 40 instructions plus
		 * call overhead).
		 */

		do {
			/*
			 * This may look odd, but it is using save-and-enable
			 * semantics instead of the save-and-disable semantics
			 * that are used everywhere else.
			 */
			COM_UNLOCK();
			critical_exit();
			incc = com->iptr - buf;
			if (tp->t_rawq.c_cc + incc > tp->t_ihiwat
			    && (com->state & CS_RTS_IFLOW
				|| tp->t_iflag & IXOFF)
			    && !(tp->t_state & TS_TBLOCK))
				ttyblock(tp);
			com->delta_error_counts[CE_TTY_BUF_OVERFLOW]
				+= b_to_q((char *)buf, incc, &tp->t_rawq);
			buf += incc;
			tk_nin += incc;
			tk_rawcc += incc;
			tp->t_rawcc += incc;
			ttwakeup(tp);
			if (tp->t_state & TS_TTSTOP
			    && (tp->t_iflag & IXANY
				|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
				tp->t_state &= ~TS_TTSTOP;
				tp->t_lflag &= ~FLUSHO;
				comstart(tp);
			}
			critical_enter();
			COM_LOCK();
		} while (buf < com->iptr);
	} else {
		do {
			/*
			 * This may look odd, but it is using save-and-enable
			 * semantics instead of the save-and-disable semantics
			 * that are used everywhere else.
			 */
			COM_UNLOCK();
			critical_exit();
			line_status = buf[com->ierroff];
			recv_data = *buf++;
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
			critical_enter();
			COM_LOCK();
		} while (buf < com->iptr);
	}
	com_events -= (com->iptr - com->ibuf);
	com->iptr = com->ibuf;

	/*
	 * There is now room for another low-level buffer full of input,
	 * so enable RTS if it is now disabled and there is room in the
	 * high-level buffer.
	 */
	if ((com->state & CS_RTS_IFLOW) && !(com->mcr_image & com->mcr_rts) &&
	    !(tp->t_state & TS_TBLOCK))
#if 0
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#else
		cd_setreg(com, com->mcr_rts_reg,
			  com->mcr_image |= com->mcr_rts);
#endif
}

void
siointr(unit)
	int	unit;
{
	int	baseu;
	int	cy_align;
	cy_addr	cy_iobase;
	int	cyu;
	cy_addr	iobase;
	u_char	status;

	COM_LOCK();	/* XXX could this be placed down lower in the loop? */

	baseu = unit * CY_MAX_PORTS;
	cy_align = com_addr(baseu)->cy_align;
	cy_iobase = com_addr(baseu)->cy_iobase;

	/* check each CD1400 in turn */
	for (cyu = 0; cyu < cy_nr_cd1400s[unit]; ++cyu) {
		iobase = (cy_addr) (cy_iobase
				    + (cy_chip_offset[cyu] << cy_align));
		/* poll to see if it has any work */
		status = cd_inb(iobase, CD1400_SVRR, cy_align);
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
			u_char		save_rir;
#endif

#ifdef PollMode
			save_rir = cd_inb(iobase, CD1400_RIR, cy_align);

			/* enter rx service */
			cd_outb(iobase, CD1400_CAR, cy_align, save_rir);
			com_addr(baseu + cyu * CD1400_NO_OF_CHANNELS)->car
			= save_rir & CD1400_CAR_CHAN;

			serv_type = cd_inb(iobase, CD1400_RIVR, cy_align);
			com = com_addr(baseu
				       + ((serv_type >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#else
			/* ack receive service */
			serv_type = cy_inb(iobase, CY8_SVCACKR, cy_align);

			com = com_addr(baseu +
				       + ((serv_type >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif

		if (serv_type & CD1400_RIVR_EXCEPTION) {
			++com->recv_exception;
			line_status = cd_inb(iobase, CD1400_RDSR, cy_align);
			/* break/unnattached error bits or real input? */
			recv_data = cd_inb(iobase, CD1400_RDSR, cy_align);
#ifndef SOFT_HOTCHAR
			if (line_status & CD1400_RDSR_SPECIAL
			    && com->hotchar != 0)
				swi_sched(sio_ih, 0);

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
				    || ((line_status & (LSR_PE|LSR_FE))
				    &&  (com->tp->t_iflag & IGNPAR))
				    || ((line_status & LSR_BI)
				    &&  (com->tp->t_iflag & IGNBRK)))
					goto cont;
				if (   (line_status & (LSR_PE|LSR_FE))
				    && (com->tp->t_state & TS_CAN_BYPASS_L_RINT)
				    && ((line_status & LSR_FE)
				    ||  ((line_status & LSR_PE)
				    &&  (com->tp->t_iflag & INPCK))))
					recv_data = 0;
			}
#endif /* 1 */
			++com->bytes_in;
#ifdef SOFT_HOTCHAR
			if (com->hotchar != 0 && recv_data == com->hotchar)
				swi_sched(sio_ih, 0);
#endif
			ioptr = com->iptr;
			if (ioptr >= com->ibufend)
				CE_RECORD(com, CE_INTERRUPT_BUF_OVERFLOW);
			else {
				if (com->do_timestamp)
					microtime(&com->timestamp);
				++com_events;
				ioptr[0] = recv_data;
				ioptr[com->ierroff] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#if 0
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
#else
					cd_outb(iobase, com->mcr_rts_reg,
						cy_align,
						com->mcr_image &=
						~com->mcr_rts);
#endif
				if (line_status & LSR_OE)
					CE_RECORD(com, CE_OVERRUN);
			}
			goto cont;
		} else {
			int	ifree;

			count = cd_inb(iobase, CD1400_RDCR, cy_align);
			if (!count)
				goto cont;
			com->bytes_in += count;
			ioptr = com->iptr;
			ifree = com->ibufend - ioptr;
			if (count > ifree) {
				count -= ifree;
				com_events += ifree;
				if (ifree != 0) {
					if (com->do_timestamp)
						microtime(&com->timestamp);
					do {
						recv_data = cd_inb(iobase,
								   CD1400_RDSR,
								   cy_align);
#ifdef SOFT_HOTCHAR
						if (com->hotchar != 0
						    && recv_data
						       == com->hotchar)
							swi_sched(sio_ih, 0);
#endif
						ioptr[0] = recv_data;
						ioptr[com->ierroff] = 0;
						++ioptr;
					} while (--ifree != 0);
				}
				com->delta_error_counts
				    [CE_INTERRUPT_BUF_OVERFLOW] += count;
				do {
					recv_data = cd_inb(iobase, CD1400_RDSR,
							   cy_align);
#ifdef SOFT_HOTCHAR
					if (com->hotchar != 0
					    && recv_data == com->hotchar)
						swi_sched(sio_ih, 0);
#endif
				} while (--count != 0);
			} else {
				if (com->do_timestamp)
					microtime(&com->timestamp);
				if (ioptr <= com->ihighwater
				    && ioptr + count > com->ihighwater
				    && com->state & CS_RTS_IFLOW)
#if 0
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
#else
					cd_outb(iobase, com->mcr_rts_reg,
						cy_align,
						com->mcr_image
						&= ~com->mcr_rts);
#endif
				com_events += count;
				do {
					recv_data = cd_inb(iobase, CD1400_RDSR,
							   cy_align);
#ifdef SOFT_HOTCHAR
					if (com->hotchar != 0
					    && recv_data == com->hotchar)
						swi_sched(sio_ih, 0);
#endif
					ioptr[0] = recv_data;
					ioptr[com->ierroff] = 0;
					++ioptr;
				} while (--count != 0);
			}
			com->iptr = ioptr;
		}
cont:

			/* terminate service context */
#ifdef PollMode
			cd_outb(iobase, CD1400_RIR, cy_align,
				save_rir
				& ~(CD1400_RIR_RDIREQ | CD1400_RIR_RBUSY));
#else
			cd_outb(iobase, CD1400_EOSRR, cy_align, 0);
#endif
		}
		if (status & CD1400_SVRR_MDMCH) {
			struct com_s	*com;
			u_char	modem_status;
#ifdef PollMode
			u_char	save_mir;
#else
			u_char	vector;
#endif

#ifdef PollMode
			save_mir = cd_inb(iobase, CD1400_MIR, cy_align);

			/* enter modem service */
			cd_outb(iobase, CD1400_CAR, cy_align, save_mir);
			com_addr(baseu + cyu * CD1400_NO_OF_CHANNELS)->car
			= save_mir & CD1400_CAR_CHAN;

			com = com_addr(baseu + cyu * CD1400_NO_OF_CHANNELS
				       + (save_mir & CD1400_MIR_CHAN));
#else
			/* ack modem service */
			vector = cy_inb(iobase, CY8_SVCACKM, cy_align);

			com = com_addr(baseu
				       + ((vector >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif
			++com->mdm;
			modem_status = cd_inb(iobase, CD1400_MSVR2, cy_align);
		if (modem_status != com->last_modem_status) {
			if (com->do_dcd_timestamp
			    && !(com->last_modem_status & MSR_DCD)
			    && modem_status & MSR_DCD)
				microtime(&com->dcd_timestamp);

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
				swi_sched(sio_ih, 0);
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
							cy_align,
							com->intr_enable
							= com->intr_enable
							  & ~CD1400_SRER_TXMPTY
							  | CD1400_SRER_TXRDY);
				} else {
					com->state &= ~CS_ODEVREADY;
					if (com->intr_enable
					    & CD1400_SRER_TXRDY)
						cd_outb(iobase, CD1400_SRER,
							cy_align,
							com->intr_enable
							= com->intr_enable
							  & ~CD1400_SRER_TXRDY
							  | CD1400_SRER_TXMPTY);
				}
			}
#endif
		}

			/* terminate service context */
#ifdef PollMode
			cd_outb(iobase, CD1400_MIR, cy_align,
				save_mir
				& ~(CD1400_MIR_RDIREQ | CD1400_MIR_RBUSY));
#else
			cd_outb(iobase, CD1400_EOSRR, cy_align, 0);
#endif
		}
		if (status & CD1400_SVRR_TXRDY) {
			struct com_s	*com;
#ifdef PollMode
			u_char	save_tir;
#else
			u_char	vector;
#endif

#ifdef PollMode
			save_tir = cd_inb(iobase, CD1400_TIR, cy_align);

			/* enter tx service */
			cd_outb(iobase, CD1400_CAR, cy_align, save_tir);
			com_addr(baseu + cyu * CD1400_NO_OF_CHANNELS)->car
			= save_tir & CD1400_CAR_CHAN;

			com = com_addr(baseu
				       + cyu * CD1400_NO_OF_CHANNELS
				       + (save_tir & CD1400_TIR_CHAN));
#else
			/* ack transmit service */
			vector = cy_inb(iobase, CY8_SVCACKT, cy_align);

			com = com_addr(baseu
				       + ((vector >> CD1400_xIVR_CHAN_SHIFT)
					  & CD1400_xIVR_CHAN));
#endif

			if (com->etc != ETC_NONE) {
				if (com->intr_enable & CD1400_SRER_TXRDY) {
					/*
					 * Here due to sloppy SRER_TXRDY
					 * enabling.  Ignore.  Come back when
					 * tx is empty.
					 */
					cd_outb(iobase, CD1400_SRER, cy_align,
						com->intr_enable
						= (com->intr_enable
						  & ~CD1400_SRER_TXRDY)
						  | CD1400_SRER_TXMPTY);
					goto terminate_tx_service;
				}
				switch (com->etc) {
				case CD1400_ETC_SENDBREAK:
				case CD1400_ETC_STOPBREAK:
					/*
					 * Start the command.  Come back on
					 * next tx empty interrupt, hopefully
					 * after command has been executed.
					 */
					cd_outb(iobase, CD1400_COR2, cy_align,
						com->cor[1] |= CD1400_COR2_ETC);
					cd_outb(iobase, CD1400_TDR, cy_align,
						CD1400_ETC_CMD);
					cd_outb(iobase, CD1400_TDR, cy_align,
						com->etc);
					if (com->etc == CD1400_ETC_SENDBREAK)
						com->etc = ETC_BREAK_STARTING;
					else
						com->etc = ETC_BREAK_ENDING;
					goto terminate_tx_service;
				case ETC_BREAK_STARTING:
					/*
					 * BREAK is now on.  Continue with
					 * SRER_TXMPTY processing, hopefully
					 * don't come back.
					 */
					com->etc = ETC_BREAK_STARTED;
					break;
				case ETC_BREAK_STARTED:
					/*
					 * Came back due to sloppy SRER_TXMPTY
					 * enabling.  Hope again.
					 */
					break;
				case ETC_BREAK_ENDING:
					/*
					 * BREAK is now off.  Continue with
					 * SRER_TXMPTY processing and don't
					 * come back.  The SWI handler will
					 * restart tx interrupts if necessary.
					 */
					cd_outb(iobase, CD1400_COR2, cy_align,
						com->cor[1]
						&= ~CD1400_COR2_ETC);
					com->etc = ETC_BREAK_ENDED;
					if (!(com->state & CS_ODONE)) {
						com_events += LOTS_OF_EVENTS;
						com->state |= CS_ODONE;
						swi_sched(sio_ih, 0);
					}
					break;
				case ETC_BREAK_ENDED:
					/*
					 * Shouldn't get here.  Hope again.
					 */
					break;
				}
			}
			if (com->intr_enable & CD1400_SRER_TXMPTY) {
				if (!(com->extra_state & CSE_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->extra_state |= CSE_ODONE;
					swi_sched(sio_ih, 0);
				}
				cd_outb(iobase, CD1400_SRER, cy_align,
					com->intr_enable
					&= ~CD1400_SRER_TXMPTY);
				goto terminate_tx_service;
			}
		if (com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
			u_char	*ioptr;
			u_int	ocount;

			ioptr = com->obufq.l_head;
				ocount = com->obufq.l_tail - ioptr;
				if (ocount > CD1400_TX_FIFO_SIZE)
					ocount = CD1400_TX_FIFO_SIZE;
				com->bytes_out += ocount;
				do
					cd_outb(iobase, CD1400_TDR, cy_align,
						*ioptr++);
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

					/*
					 * The setting of CSE_ODONE may be
					 * stale here.  We currently only
					 * use it when CS_BUSY is set, and
					 * fixing it when we clear CS_BUSY
					 * is easiest.
					 */
					if (com->extra_state & CSE_ODONE) {
						com_events -= LOTS_OF_EVENTS;
						com->extra_state &= ~CSE_ODONE;
					}

					cd_outb(iobase, CD1400_SRER, cy_align,
						com->intr_enable
						= (com->intr_enable
						  & ~CD1400_SRER_TXRDY)
						  | CD1400_SRER_TXMPTY);
				}
				if (!(com->state & CS_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->state |= CS_ODONE;

					/* handle at high level ASAP */
					swi_sched(sio_ih, 0);
				}
			}
		}

			/* terminate service context */
terminate_tx_service:
#ifdef PollMode
			cd_outb(iobase, CD1400_TIR, cy_align,
				save_tir
				& ~(CD1400_TIR_RDIREQ | CD1400_TIR_RBUSY));
#else
			cd_outb(iobase, CD1400_EOSRR, cy_align, 0);
#endif
		}
	}

	/* ensure an edge for the next interrupt */
	cy_outb(cy_iobase, CY_CLEAR_INTR, cy_align, 0);

	swi_sched(sio_ih, 0);

	COM_UNLOCK();
}

#if 0
static void
siointr1(com)
	struct com_s	*com;
{
}
#endif

static int
sioioctl(dev, cmd, data, flag, td)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		flag;
	struct thread	*td;
{
	struct com_s	*com;
	int		error;
	int		mynor;
	int		s;
	struct tty	*tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	int		oldcmd;
	struct termios	term;
#endif

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
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
			error = suser(td);
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
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, td);
	if (error != ENOIOCTL)
		return (error);
	s = spltty();
	error = ttioctl(tp, cmd, data, flag);
	disc_optim(tp, &tp->t_termios, com);
	if (error != ENOIOCTL) {
		splx(s);
		return (error);
	}
	switch (cmd) {
	case TIOCSBRK:
#if 0
		outb(iobase + com_cfcr, com->cfcr_image |= CFCR_SBREAK);
#else
		cd_etc(com, CD1400_ETC_SENDBREAK);
#endif
		break;
	case TIOCCBRK:
#if 0
		outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
#else
		cd_etc(com, CD1400_ETC_STOPBREAK);
#endif
		break;
	case TIOCSDTR:
		(void)commctl(com, TIOCM_DTR, DMBIS);
		break;
	case TIOCCDTR:
		(void)commctl(com, TIOCM_DTR, DMBIC);
		break;
	/*
	 * XXX should disallow changing MCR_RTS if CS_RTS_IFLOW is set.  The
	 * changes get undone on the next call to comparam().
	 */
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
		error = suser(td);
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
	case TIOCDCDTIMESTAMP:
		com->do_dcd_timestamp = TRUE;
		*(struct timeval *)data = com->dcd_timestamp;
		break;
	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}

static void
siopoll(void *arg)
{
	int		unit;

#ifdef CyDebug
	++cy_timeouts;
#endif
	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < NSIO; ++unit) {
		struct com_s	*com;
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
			critical_enter();
			COM_LOCK();
			incc = com->iptr - com->ibuf;
			com->iptr = com->ibuf;
			if (com->state & CS_CHECKMSR) {
				incc += LOTS_OF_EVENTS;
				com->state &= ~CS_CHECKMSR;
			}
			com_events -= incc;
			COM_UNLOCK();
			critical_exit();
			if (incc != 0)
				log(LOG_DEBUG,
				    "sio%d: %d events for device with no tp\n",
				    unit, incc);
			continue;
		}
		if (com->iptr != com->ibuf) {
			critical_enter();
			COM_LOCK();
			sioinput(com);
			COM_UNLOCK();
			critical_exit();
		}
		if (com->state & CS_CHECKMSR) {
			u_char	delta_modem_status;

			critical_enter();
			COM_LOCK();
			sioinput(com);
			delta_modem_status = com->last_modem_status
					     ^ com->prev_modem_status;
			com->prev_modem_status = com->last_modem_status;
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_CHECKMSR;
			COM_UNLOCK();
			critical_exit();
			if (delta_modem_status & MSR_DCD)
				(*linesw[tp->t_line].l_modem)
					(tp, com->prev_modem_status & MSR_DCD);
		}
		if (com->extra_state & CSE_ODONE) {
			critical_enter();
			COM_LOCK();
			com_events -= LOTS_OF_EVENTS;
			com->extra_state &= ~CSE_ODONE;
			COM_UNLOCK();
			critical_exit();
			if (!(com->state & CS_BUSY)) {
				tp->t_state &= ~TS_BUSY;
				ttwwakeup(com->tp);
			}
			if (com->etc != ETC_NONE) {
				if (com->etc == ETC_BREAK_ENDED)
					com->etc = ETC_NONE;
				wakeup(&com->etc);
			}
		}
		if (com->state & CS_ODONE) {
			critical_enter();
			COM_LOCK();
			com_events -= LOTS_OF_EVENTS;
			com->state &= ~CS_ODONE;
			COM_UNLOCK();
			critical_exit();
			(*linesw[tp->t_line].l_start)(tp);
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
	u_long		cy_clock;
	int		idivisor;
	int		iflag;
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

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);

	/* check requested parameters */
	cy_clock = CY_CLOCK(com->gfrcr_image);
	idivisor = comspeed(t->c_ispeed, cy_clock, &iprescaler);
	if (idivisor < 0)
		return (EINVAL);
	odivisor = comspeed(t->c_ospeed, cy_clock, &oprescaler);
	if (odivisor < 0)
		return (EINVAL);

	/* parameters are OK, convert them to the com struct and the device */
	s = spltty();
	if (odivisor == 0)
		(void)commctl(com, TIOCM_DTR, DMBIC);	/* hang up line */
	else
		(void)commctl(com, TIOCM_DTR, DMBIS);

	(void) siosetwater(com, t->c_ispeed);

	/* XXX we don't actually change the speed atomically. */

	if (idivisor != 0) {
		cd_setreg(com, CD1400_RBPR, idivisor);
		cd_setreg(com, CD1400_RCOR, iprescaler);
	}
	if (odivisor != 0) {
		cd_setreg(com, CD1400_TBPR, odivisor);
		cd_setreg(com, CD1400_TCOR, oprescaler);
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
		cd1400_channel_cmd(com, opt);
	}

#ifdef Smarts
	/* set special chars */
	/* XXX if one is _POSIX_VDISABLE, can't use some others */
	if (t->c_cc[VSTOP] != _POSIX_VDISABLE)
		cd_setreg(com, CD1400_SCHR1, t->c_cc[VSTOP]);
	if (t->c_cc[VSTART] != _POSIX_VDISABLE)
		cd_setreg(com, CD1400_SCHR2, t->c_cc[VSTART]);
	if (t->c_cc[VINTR] != _POSIX_VDISABLE)
		cd_setreg(com, CD1400_SCHR3, t->c_cc[VINTR]);
	if (t->c_cc[VSUSP] != _POSIX_VDISABLE)
		cd_setreg(com, CD1400_SCHR4, t->c_cc[VSUSP]);
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
		cd_setreg(com, CD1400_COR1, com->cor[0] = opt);
	}

	/*
	 * Set receive time-out period, normally to max(one char time, 5 ms).
	 */
	if (t->c_ispeed == 0)
		itimeout = cd_getreg(com, CD1400_RTPR);
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
	cd_setreg(com, CD1400_RTPR, itimeout);

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
	critical_enter();
	COM_LOCK();
	if (opt != com->cor[1]) {
		cor_change |= CD1400_CCR_COR2;
		cd_setreg(com, CD1400_COR2, com->cor[1] = opt);
	}
	COM_UNLOCK();
	critical_exit();

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
		cd_setreg(com, CD1400_COR3, com->cor[2] = opt);
	}

	/* notify the CD1400 if COR1-3 have changed */
	if (cor_change)
		cd1400_channel_cmd(com, CD1400_CCR_CMDCORCHG | cor_change);

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
		opt |= CD1400_COR4_IGNBRK | CD1400_COR4_NOBRKINT;
	/*
	 * The `-ignbrk -brkint parmrk' case is not handled by the hardware,
	 * so only tell the hardware about -brkint if -parmrk.
	 */
	if (!(iflag & (BRKINT | PARMRK)))
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
	cd_setreg(com, CD1400_COR4, opt);

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
	cd_setreg(com, CD1400_COR5, opt);

	/*
	 * We always generate modem status change interrupts for CD changes.
	 * Among other things, this is necessary to track TS_CARR_ON for
	 * pstat to print even when the driver doesn't care.  CD changes
	 * should be rare so interrupts for them are not worth extra code to
	 * avoid.  We avoid interrupts for other modem status changes (except
	 * for CTS changes when SOFT_CTS_OFLOW is configured) since this is
	 * simplest and best.
	 */

	/*
	 * set modem change option register 1
	 *	generate modem interrupts on which 1 -> 0 input transitions
	 *	also controls auto-DTR output flow-control, which we don't use
	 */
	opt = CD1400_MCOR1_CDzd;
#ifdef SOFT_CTS_OFLOW
	if (cflag & CCTS_OFLOW)
		opt |= CD1400_MCOR1_CTSzd;
#endif
	cd_setreg(com, CD1400_MCOR1, opt);

	/*
	 * set modem change option register 2
	 *	generate modem interrupts on specific 0 -> 1 input transitions
	 */
	opt = CD1400_MCOR2_CDod;
#ifdef SOFT_CTS_OFLOW
	if (cflag & CCTS_OFLOW)
		opt |= CD1400_MCOR2_CTSod;
#endif
	cd_setreg(com, CD1400_MCOR2, opt);

	/*
	 * XXX should have done this long ago, but there is too much state
	 * to change all atomically.
	 */
	critical_enter();
	COM_LOCK();

	com->state &= ~CS_TTGO;
	if (!(tp->t_state & TS_TTSTOP))
		com->state |= CS_TTGO;
	if (cflag & CRTS_IFLOW) {
		com->state |= CS_RTS_IFLOW;
		/*
		 * If CS_RTS_IFLOW just changed from off to on, the change
		 * needs to be propagated to MCR_RTS.  This isn't urgent,
		 * so do it later by calling comstart() instead of repeating
		 * a lot of code from comstart() here.
		 */
	} else if (com->state & CS_RTS_IFLOW) {
		com->state &= ~CS_RTS_IFLOW;
		/*
		 * CS_RTS_IFLOW just changed from on to off.  Force MCR_RTS
		 * on here, since comstart() won't do it later.
		 */
#if 0
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#else
		cd_setreg(com, com->mcr_rts_reg,
			  com->mcr_image |= com->mcr_rts);
#endif
	}

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
			cd_setreg(com, CD1400_SRER,
				  com->intr_enable
				  = (com->intr_enable & ~CD1400_SRER_TXMPTY)
				    | CD1400_SRER_TXRDY);
	} else {
		if (com->intr_enable & CD1400_SRER_TXRDY)
			cd_setreg(com, CD1400_SRER,
				  com->intr_enable
				  = (com->intr_enable & ~CD1400_SRER_TXRDY)
				    | CD1400_SRER_TXMPTY);
	}

	COM_UNLOCK();
	critical_exit();
	splx(s);
	comstart(tp);
	if (com->ibufold != NULL) {
		free(com->ibufold, M_DEVBUF);
		com->ibufold = NULL;
	}
	return (0);
}

static int
siosetwater(com, speed)
	struct com_s	*com;
	speed_t		speed;
{
	int		cp4ticks;
	u_char		*ibuf;
	int		ibufsize;
	struct tty	*tp;

	/*
	 * Make the buffer size large enough to handle a softtty interrupt
	 * latency of about 2 ticks without loss of throughput or data
	 * (about 3 ticks if input flow control is not used or not honoured,
	 * but a bit less for CS5-CS7 modes).
	 */
	cp4ticks = speed / 10 / hz * 4;
	for (ibufsize = 128; ibufsize < cp4ticks;)
		ibufsize <<= 1;
	if (ibufsize == com->ibufsize) {
		return (0);
	}

	/*
	 * Allocate input buffer.  The extra factor of 2 in the size is
	 * to allow for an error byte for each input byte.
	 */
	ibuf = malloc(2 * ibufsize, M_DEVBUF, M_NOWAIT);
	if (ibuf == NULL) {
		return (ENOMEM);
	}

	/* Initialize non-critical variables. */
	com->ibufold = com->ibuf;
	com->ibufsize = ibufsize;
	tp = com->tp;
	if (tp != NULL) {
		tp->t_ififosize = 2 * ibufsize;
		tp->t_ispeedwat = (speed_t)-1;
		tp->t_ospeedwat = (speed_t)-1;
	}

	/*
	 * Read current input buffer, if any.  Continue with interrupts
	 * disabled.
	 */
	critical_enter();
	COM_LOCK();
	if (com->iptr != com->ibuf)
		sioinput(com);

	/*-
	 * Initialize critical variables, including input buffer watermarks.
	 * The external device is asked to stop sending when the buffer
	 * exactly reaches high water, or when the high level requests it.
	 * The high level is notified immediately (rather than at a later
	 * clock tick) when this watermark is reached.
	 * The buffer size is chosen so the watermark should almost never
	 * be reached.
	 * The low watermark is invisibly 0 since the buffer is always
	 * emptied all at once.
	 */
	com->iptr = com->ibuf = ibuf;
	com->ibufend = ibuf + ibufsize;
	com->ierroff = ibufsize;
	com->ihighwater = ibuf + 3 * ibufsize / 4;

	COM_UNLOCK();
	critical_exit();
	return (0);
}

static void
comstart(tp)
	struct tty	*tp;
{
	struct com_s	*com;
	int		s;
#ifdef CyDebug
	bool_t		started;
#endif
	int		unit;

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	s = spltty();

#ifdef CyDebug
	++com->start_count;
	started = FALSE;
#endif

	critical_enter();
	COM_LOCK();
	if (tp->t_state & TS_TTSTOP) {
		com->state &= ~CS_TTGO;
		if (com->intr_enable & CD1400_SRER_TXRDY)
			cd_setreg(com, CD1400_SRER,
				  com->intr_enable
				  = (com->intr_enable & ~CD1400_SRER_TXRDY)
				    | CD1400_SRER_TXMPTY);
	} else {
		com->state |= CS_TTGO;
		if (com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)
		    && !(com->intr_enable & CD1400_SRER_TXRDY))
			cd_setreg(com, CD1400_SRER,
				  com->intr_enable
				  = (com->intr_enable & ~CD1400_SRER_TXMPTY)
				    | CD1400_SRER_TXRDY);
	}
	if (tp->t_state & TS_TBLOCK) {
		if (com->mcr_image & com->mcr_rts && com->state & CS_RTS_IFLOW)
#if 0
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
#else
			cd_setreg(com, com->mcr_rts_reg,
				  com->mcr_image &= ~com->mcr_rts);
#endif
	} else {
		if (!(com->mcr_image & com->mcr_rts)
		    && com->iptr < com->ihighwater
		    && com->state & CS_RTS_IFLOW)
#if 0
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
#else
			cd_setreg(com, com->mcr_rts_reg,
				  com->mcr_image |= com->mcr_rts);
#endif
	}
	COM_UNLOCK();
	critical_exit();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
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
			critical_enter();
			COM_LOCK();
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
					cd_setreg(com, CD1400_SRER,
						  com->intr_enable
						  = (com->intr_enable
						    & ~CD1400_SRER_TXMPTY)
						    | CD1400_SRER_TXRDY);
			}
			COM_UNLOCK();
			critical_exit();
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
			critical_enter();
			COM_LOCK();
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
					cd_setreg(com, CD1400_SRER,
						  com->intr_enable
						  = (com->intr_enable
						    & ~CD1400_SRER_TXMPTY)
						    | CD1400_SRER_TXRDY);
			}
			COM_UNLOCK();
			critical_exit();
		}
		tp->t_state |= TS_BUSY;
	}
#ifdef CyDebug
	if (started)
		++com->start_real;
#endif
#if 0
	critical_enter();
	COM_LOCK();
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);	/* fake interrupt to start output */
	COM_UNLOCK();
	critical_exit();
#endif
	ttwwakeup(tp);
	splx(s);
}

static void
comstop(tp, rw)
	struct tty	*tp;
	int		rw;
{
	struct com_s	*com;
	bool_t		wakeup_etc;

	com = com_addr(DEV_TO_UNIT(tp->t_dev));
	wakeup_etc = FALSE;
	critical_enter();
	COM_LOCK();
	if (rw & FWRITE) {
		com->obufs[0].l_queued = FALSE;
		com->obufs[1].l_queued = FALSE;
		if (com->extra_state & CSE_ODONE) {
			com_events -= LOTS_OF_EVENTS;
			com->extra_state &= ~CSE_ODONE;
			if (com->etc != ETC_NONE) {
				if (com->etc == ETC_BREAK_ENDED)
					com->etc = ETC_NONE;
				wakeup_etc = TRUE;
			}
		}
		com->tp->t_state &= ~TS_BUSY;
		if (com->state & CS_ODONE)
			com_events -= LOTS_OF_EVENTS;
		com->state &= ~(CS_ODONE | CS_BUSY);
	}
	if (rw & FREAD) {
		/* XXX no way to reset only input fifo. */
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	COM_UNLOCK();
	critical_exit();
	if (wakeup_etc)
		wakeup(&com->etc);
	if (rw & FWRITE && com->etc == ETC_NONE)
		cd1400_channel_cmd(com, CD1400_CCR_CMDRESET | CD1400_CCR_FTF);
	comstart(tp);
}

static int
commctl(com, bits, how)
	struct com_s	*com;
	int		bits;
	int		how;
{
	int	mcr;
	int	msr;

	if (how == DMGET) {
		if (com->channel_control & CD1400_CCR_RCVEN)
			bits |= TIOCM_LE;
		mcr = com->mcr_image;
		if (mcr & com->mcr_dtr)
			bits |= TIOCM_DTR;
		if (mcr & com->mcr_rts)
			/* XXX wired on for Cyclom-8Ys */
			bits |= TIOCM_RTS;

		/*
		 * We must read the modem status from the hardware because
		 * we don't generate modem status change interrupts for all
		 * changes, so com->prev_modem_status is not guaranteed to
		 * be up to date.  This is safe, unlike for sio, because
		 * reading the status register doesn't clear pending modem
		 * status change interrupts.
		 */
		msr = cd_getreg(com, CD1400_MSVR2);

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
	mcr = 0;
	if (bits & TIOCM_DTR)
		mcr |= com->mcr_dtr;
	if (bits & TIOCM_RTS)
		mcr |= com->mcr_rts;
	critical_enter();
	COM_LOCK();
	switch (how) {
	case DMSET:
		com->mcr_image = mcr;
		cd_setreg(com, CD1400_MSVR1, mcr);
		cd_setreg(com, CD1400_MSVR2, mcr);
		break;
	case DMBIS:
		com->mcr_image = mcr = com->mcr_image | mcr;
		cd_setreg(com, CD1400_MSVR1, mcr);
		cd_setreg(com, CD1400_MSVR2, mcr);
		break;
	case DMBIC:
		com->mcr_image = mcr = com->mcr_image & ~mcr;
		cd_setreg(com, CD1400_MSVR1, mcr);
		cd_setreg(com, CD1400_MSVR2, mcr);
		break;
	}
	COM_UNLOCK();
	critical_exit();
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
	untimeout(comwakeup, (void *)NULL, sio_timeout_handle);
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
		sio_timeout_handle = timeout(comwakeup, (void *)NULL,
					     sio_timeout);
	} else {
		/* Flush error messages, if any. */
		sio_timeouts_until_log = 1;
		comwakeup((void *)NULL);
		untimeout(comwakeup, (void *)NULL, sio_timeout_handle);
	}
}

static void
comwakeup(chan)
	void	*chan;
{
	struct com_s	*com;
	int		unit;

	sio_timeout_handle = timeout(comwakeup, (void *)NULL, sio_timeout);

#if 0
	/*
	 * Recover from lost output interrupts.
	 * Poll any lines that don't use interrupts.
	 */
	for (unit = 0; unit < NSIO; ++unit) {
		com = com_addr(unit);
		if (com != NULL
		    && (com->state >= (CS_BUSY | CS_TTGO) || com->poll)) {
			critical_enter();
			COM_LOCK();
			siointr1(com);
			COM_UNLOCK();
			critical_exit();
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

			critical_enter();
			COM_LOCK();
			delta = com->delta_error_counts[errnum];
			com->delta_error_counts[errnum] = 0;
			COM_UNLOCK();
			critical_exit();
			if (delta == 0)
				continue;
			total = com->error_counts[errnum] += delta;
			log(LOG_ERR, "cy%d: %u more %s%s (total %lu)\n",
			    unit, delta, error_desc[errnum],
			    delta == 1 ? "" : "s", total);
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
	com->hotchar = linesw[tp->t_line].l_hotchar;
#ifndef SOFT_HOTCHAR
	opt = com->cor[2] & ~CD1400_COR3_SCD34;
	if (com->hotchar != 0) {
		cd_setreg(com, CD1400_SCHR3, com->hotchar);
		cd_setreg(com, CD1400_SCHR4, com->hotchar);
		opt |= CD1400_COR3_SCD34;
	}
	if (opt != com->cor[2]) {
		cd_setreg(com, CD1400_COR3, com->cor[2] = opt);
		cd1400_channel_cmd(com, CD1400_CCR_CMDCORCHG | CD1400_CCR_COR3);
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
comspeed(speed, cy_clock, prescaler_io)
	speed_t	speed;
	u_long	cy_clock;
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
		if (cy_clock / prescaler / speed > 63)
			break;
	}

	divider = (cy_clock / prescaler * 2 / speed + 1) / 2; /* round off */
	if (divider > 255)
		divider = 255;
	actual = cy_clock/prescaler/divider;

	/* 10 times error in percent: */
	error = ((actual - (long)speed) * 2000 / (long)speed + 1) / 2;

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
cd1400_channel_cmd(com, cmd)
	struct com_s	*com;
	int		cmd;
{
	cd1400_channel_cmd_wait(com);
	cd_setreg(com, CD1400_CCR, cmd);
	cd1400_channel_cmd_wait(com);
}

static void
cd1400_channel_cmd_wait(com)
	struct com_s	*com;
{
	struct timeval	start;
	struct timeval	tv;
	long		usec;

	if (cd_getreg(com, CD1400_CCR) == 0)
		return;
	microtime(&start);
	for (;;) {
		if (cd_getreg(com, CD1400_CCR) == 0)
			return;
		microtime(&tv);
		usec = 1000000 * (tv.tv_sec - start.tv_sec) +
		    tv.tv_usec - start.tv_usec;
		if (usec >= 5000) {
			log(LOG_ERR,
			    "cy%d: channel command timeout (%ld usec)\n",
			    com->unit, usec);
			return;
		}
	}
}

static void
cd_etc(com, etc)
	struct com_s	*com;
	int		etc;
{

	/*
	 * We can't change the hardware's ETC state while there are any
	 * characters in the tx fifo, since those characters would be
	 * interpreted as commands!  Unputting characters from the fifo
	 * is difficult, so we wait up to 12 character times for the fifo
	 * to drain.  The command will be delayed for up to 2 character
	 * times for the tx to become empty.  Unputting characters from
	 * the tx holding and shift registers is impossible, so we wait
	 * for the tx to become empty so that the command is sure to be
	 * executed soon after we issue it.
	 */
	critical_enter();
	COM_LOCK();
	if (com->etc == etc)
		goto wait;
	if ((etc == CD1400_ETC_SENDBREAK
	    && (com->etc == ETC_BREAK_STARTING
		|| com->etc == ETC_BREAK_STARTED))
	    || (etc == CD1400_ETC_STOPBREAK
	       && (com->etc == ETC_BREAK_ENDING || com->etc == ETC_BREAK_ENDED
		   || com->etc == ETC_NONE))) {
		COM_UNLOCK();
		critical_exit();
		return;
	}
	com->etc = etc;
	cd_setreg(com, CD1400_SRER,
		  com->intr_enable
		  = (com->intr_enable & ~CD1400_SRER_TXRDY) | CD1400_SRER_TXMPTY);
wait:
	COM_UNLOCK();
	critical_exit();
	while (com->etc == etc
	       && tsleep(&com->etc, TTIPRI | PCATCH, "cyetc", 0) == 0)
		continue;
}

static int
cd_getreg(com, reg)
	struct com_s	*com;
	int		reg;
{
	struct com_s	*basecom;
	u_char	car;
	int	cy_align;
	register_t	eflags;
	cy_addr	iobase;
	int	val;

	basecom = com_addr(com->unit & ~(CD1400_NO_OF_CHANNELS - 1));
	car = com->unit & CD1400_CAR_CHAN;
	cy_align = com->cy_align;
	iobase = com->iobase;
	eflags = read_eflags();
	critical_enter();
	if (eflags & PSL_I)
		COM_LOCK();
	if (basecom->car != car)
		cd_outb(iobase, CD1400_CAR, cy_align, basecom->car = car);
	val = cd_inb(iobase, reg, cy_align);
	if (eflags & PSL_I)
		COM_UNLOCK();
	critical_exit();
	return (val);
}

static void
cd_setreg(com, reg, val)
	struct com_s	*com;
	int		reg;
	int		val;
{
	struct com_s	*basecom;
	u_char	car;
	int	cy_align;
	register_t	eflags;
	cy_addr	iobase;

	basecom = com_addr(com->unit & ~(CD1400_NO_OF_CHANNELS - 1));
	car = com->unit & CD1400_CAR_CHAN;
	cy_align = com->cy_align;
	iobase = com->iobase;
	eflags = read_eflags();
	critical_enter();
	if (eflags & PSL_I)
		COM_LOCK();
	if (basecom->car != car)
		cd_outb(iobase, CD1400_CAR, cy_align, basecom->car = car);
	cd_outb(iobase, reg, cy_align, val);
	if (eflags & PSL_I)
		COM_UNLOCK();
	critical_exit();
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
	printf("saved channel_control:\t\t0x%02x\n", com->channel_control);
	printf("saved cor1-3:\t\t\t0x%02x 0x%02x 0x%02x\n",
	       com->cor[0], com->cor[1], com->cor[2]);
	printf("service request enable reg:\t0x%02x (0x%02x cached)\n",
	       cd_getreg(com, CD1400_SRER), com->intr_enable);
	printf("service request register:\t0x%02x\n",
	       cd_inb(iobase, CD1400_SVRR, com->cy_align));
	printf("modem status:\t\t\t0x%02x (0x%02x cached)\n",
	       cd_getreg(com, CD1400_MSVR2), com->prev_modem_status);
	printf("rx/tx/mdm interrupt registers:\t0x%02x 0x%02x 0x%02x\n",
	       cd_inb(iobase, CD1400_RIR, com->cy_align),
	       cd_inb(iobase, CD1400_TIR, com->cy_align),
	       cd_inb(iobase, CD1400_MIR, com->cy_align));
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
		printf(
		"upper layer queue lengths:\t%d raw, %d canon, %d output\n",
		       tp->t_rawq.c_cc, tp->t_canq.c_cc, tp->t_outq.c_cc);
	} else
		printf("tty state:\t\t\tclosed\n");
}
#endif /* CyDebug */
