/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	$Id: sio.c,v 1.225 1999/04/18 14:11:01 dfr Exp $
 *	from: @(#)com.c	7.5 (Berkeley) 5/16/91
 *	from: i386/isa sio.c,v 1.234
 */

#include "opt_comconsole.h"
#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_devfs.h"
/* #include "opt_sio.h" */
#include "sio.h"
/* #include "pnp.h" */
#define NPNP 0

/*
 * Serial driver, based on 386BSD-0.1 com driver.
 * Mostly rewritten to use pseudo-DMA.
 * Works for National Semiconductor NS8250-NS16550AF UARTs.
 * COM driver, based on HP dca driver.
 *
 * Changes for PC-Card integration:
 *	- Added PC-Card driver table and handlers
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/malloc.h>
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/fcntl.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <sys/timepps.h>

#include <isa/isareg.h>
#include <isa/isavar.h>
#include <machine/lock.h>

#include <machine/clock.h>
#include <machine/ipl.h>
#ifndef SMP
#include <machine/lock.h>
#endif
#include <machine/resource.h>

#include <isa/sioreg.h>

#ifdef COM_ESP
#include <isa/ic/esp.h>
#endif
#include <isa/ic/ns16550.h>

#if 0

#include "card.h"
#if NCARD > 0
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#endif

#if NPNP > 0
#include <i386/isa/pnp.h>
#endif

#endif

#ifndef __i386__
#define disable_intr()	0
#define enable_intr()	0
#endif

#ifdef SMP
#define disable_intr()	COM_DISABLE_INTR()
#define enable_intr()	COM_ENABLE_INTR()
#endif /* SMP */

#ifndef EXTRA_SIO
#if NPNP > 0
#define EXTRA_SIO MAX_PNP_CARDS
#else
#define EXTRA_SIO 0
#endif
#endif

#define NSIOTOT (NSIO + EXTRA_SIO)

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */

#define	CALLOUT_MASK		0x80
#define	CONTROL_MASK		0x60
#define	CONTROL_INIT_STATE	0x20
#define	CONTROL_LOCK_STATE	0x40
#define	DEV_TO_UNIT(dev)	(MINOR_TO_UNIT(minor(dev)))
#define	MINOR_MAGIC_MASK	(CALLOUT_MASK | CONTROL_MASK)
#define	MINOR_TO_UNIT(mynor)	((mynor) & ~MINOR_MAGIC_MASK)

#ifdef COM_MULTIPORT
/* checks in flags for multiport and which is multiport "master chip"
 * for a given card
 */
#define	COM_ISMULTIPORT(flags)	((flags) & 0x01)
#define	COM_MPMASTER(flags)	(((flags) >> 8) & 0x0ff)
#define	COM_NOTAST4(flags)	((flags) & 0x04)
#endif /* COM_MULTIPORT */

#define	COM_CONSOLE(flags)	((flags) & 0x10)
#define	COM_FORCECONSOLE(flags)	((flags) & 0x20)
#define	COM_LLCONSOLE(flags)	((flags) & 0x40)
#define	COM_LOSESOUTINTS(flags)	((flags) & 0x08)
#define	COM_NOFIFO(flags)		((flags) & 0x02)
#define COM_ST16650A(flags)	((flags) & 0x20000)
#define COM_C_NOPROBE		(0x40000)
#define COM_NOPROBE(flags)	((flags) & COM_C_NOPROBE)
#define COM_C_IIR_TXRDYBUG	(0x80000)
#define COM_IIR_TXRDYBUG(flags)	((flags) & COM_C_IIR_TXRDYBUG)
#define	COM_FIFOSIZE(flags)	(((flags) & 0xff000000) >> 24)

#define	com_scr		7	/* scratch register for 16450-16550 (R/W) */

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
#define	CSE_BUSYCHECK	1	/* siobusycheck() scheduled */

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
typedef u_int	Port_t;		/* hardware port */
typedef u_char	bool_t;		/* boolean */

/* queue of linear buffers */
struct lbq {
	u_char	*l_head;	/* next char to process */
	u_char	*l_tail;	/* one past the last char to process */
	struct lbq *l_next;	/* next in queue */
	bool_t	l_queued;	/* nonzero if queued */
};

/* com device structure */
struct com_s {
	u_int	flags;		/* Copy isa device flags */
	u_char	state;		/* miscellaneous flag bits */
	bool_t  active_out;	/* nonzero if the callout device is open */
	u_char	cfcr_image;	/* copy of value written to CFCR */
#ifdef COM_ESP
	bool_t	esp;		/* is this unit a hayes esp board? */
#endif
	u_char	extra_state;	/* more flag bits, separate for order trick */
	u_char	fifo_image;	/* copy of value written to FIFO */
	bool_t	hasfifo;	/* nonzero for 16550 UARTs */
	bool_t	st16650a;	/* Is a Startech 16650A or RTS/CTS compat */
	bool_t	loses_outints;	/* nonzero if device loses output interrupts */
	u_char	mcr_image;	/* copy of value written to MCR */
#ifdef COM_MULTIPORT
	bool_t	multiport;	/* is this unit part of a multiport device? */
#endif /* COM_MULTIPORT */
	bool_t	no_irq;		/* nonzero if irq is not attached */
	bool_t  gone;		/* hardware disappeared */
	bool_t	poll;		/* nonzero if polling is required */
	bool_t	poll_output;	/* nonzero if polling for output is required */
	int	unit;		/* unit	number */
	int	dtr_wait;	/* time to hold DTR down on close (* 1/hz) */
	u_int	tx_fifo_size;
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

	Port_t	data_port;	/* i/o ports */
#ifdef COM_ESP
	Port_t	esp_port;
#endif
	Port_t	int_id_port;
	Port_t	iobase;
	Port_t	modem_ctl_port;
	Port_t	line_status_port;
	Port_t	modem_status_port;
	Port_t	intr_ctl_port;	/* Ports of IIR register */

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
	struct	pps_state pps;

	u_long	bytes_in;	/* statistics */
	u_long	bytes_out;
	u_int	delta_error_counts[CE_NTYPES];
	u_long	error_counts[CE_NTYPES];

	/*
	 * Data area for output buffers.  Someday we should build the output
	 * buffer queue without copying data.
	 */
	u_char	obuf1[256];
	u_char	obuf2[256];
#ifdef DEVFS
	void	*devfs_token_ttyd;
	void	*devfs_token_ttyl;
	void	*devfs_token_ttyi;
	void	*devfs_token_cuaa;
	void	*devfs_token_cual;
	void	*devfs_token_cuai;
#endif
};

#ifdef COM_ESP
static	int	espattach	__P((struct isa_device *isdp, struct com_s *com,
				     Port_t esp_port));
#endif
static	int	sioattach	__P((device_t dev));

static	timeout_t siobusycheck;
static	timeout_t siodtrwakeup;
static	void	comhardclose	__P((struct com_s *com));
static	void	sioinput	__P((struct com_s *com));
static	void	siointr1	__P((struct com_s *com));
static	void	siointr		__P((void *arg));
static	int	commctl		__P((struct com_s *com, int bits, int how));
static	int	comparam	__P((struct tty *tp, struct termios *t));
static	swihand_t siopoll;
static	int	sioprobe	__P((device_t dev));
static	void	siosettimeout	__P((void));
static	int	siosetwater	__P((struct com_s *com, speed_t speed));
static	void	comstart	__P((struct tty *tp));
static	timeout_t comwakeup;
static	void	disc_optim	__P((struct tty	*tp, struct termios *t,
				     struct com_s *com));


static char driver_name[] = "sio";

/* table and macro for fast conversion from a unit number to its com struct */
static	devclass_t	sio_devclass;
#define	com_addr(unit)	((struct com_s *) \
			 devclass_get_softc(sio_devclass, unit))

static device_method_t sio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sioprobe),
	DEVMETHOD(device_attach,	sioattach),

	{ 0, 0 }
};

static driver_t sio_driver = {
	driver_name,
	sio_methods,
	DRIVER_TYPE_TTY|DRIVER_TYPE_FAST,
	sizeof(struct com_s),
};

static	d_open_t	sioopen;
static	d_close_t	sioclose;
static	d_read_t	sioread;
static	d_write_t	siowrite;
static	d_ioctl_t	sioioctl;
static	d_stop_t	siostop;
static	d_devtotty_t	siodevtotty;

#define	CDEV_MAJOR	28
static	struct cdevsw	sio_cdevsw = {
	sioopen,	sioclose,	sioread,	siowrite,
	sioioctl,	siostop,	noreset,	siodevtotty,
	ttpoll,		nommap,		NULL,		driver_name,
	NULL,		-1,		nodump,		nopsize,
	D_TTY,
};

int	comconsole = -1;
static	volatile speed_t	comdefaultrate = CONSPEED;
#ifdef __alpha__
static	volatile speed_t	gdbdefaultrate = CONSPEED;
#endif
static	u_int	com_events;	/* input chars + weighted output completions */
static	Port_t	siocniobase;
#ifdef __alpha__
static	Port_t	siogdbiobase;
#endif
static	bool_t	sio_registered;
static	int	sio_timeout;
static	int	sio_timeouts_until_log;
static	struct	callout_handle sio_timeout_handle
    = CALLOUT_HANDLE_INITIALIZER(&sio_timeout_handle);
#if 0 /* XXX */
static struct tty	*sio_tty[NSIOTOT];
#else
static struct tty	sio_tty[NSIOTOT];
#endif
static	const int	nsio_tty = NSIOTOT;

static	struct speedtab comspeedtab[] = {
	{ 0,		0 },
	{ 50,		COMBRD(50) },
	{ 75,		COMBRD(75) },
	{ 110,		COMBRD(110) },
	{ 134,		COMBRD(134) },
	{ 150,		COMBRD(150) },
	{ 200,		COMBRD(200) },
	{ 300,		COMBRD(300) },
	{ 600,		COMBRD(600) },
	{ 1200,		COMBRD(1200) },
	{ 1800,		COMBRD(1800) },
	{ 2400,		COMBRD(2400) },
	{ 4800,		COMBRD(4800) },
	{ 9600,		COMBRD(9600) },
	{ 19200,	COMBRD(19200) },
	{ 38400,	COMBRD(38400) },
	{ 57600,	COMBRD(57600) },
	{ 115200,	COMBRD(115200) },
	{ -1,		-1 }
};

#ifdef COM_ESP
/* XXX configure this properly. */
static	Port_t	likely_com_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, };
static	Port_t	likely_esp_ports[] = { 0x140, 0x180, 0x280, 0 };
#endif

/*
 * handle sysctl read/write requests for console speed
 * 
 * In addition to setting comdefaultrate for I/O through /dev/console,
 * also set the initial and lock values for the /dev/ttyXX device
 * if there is one associated with the console.  Finally, if the /dev/tty
 * device has already been open, change the speed on the open running port
 * itself.
 */

static int
sysctl_machdep_comdefaultrate SYSCTL_HANDLER_ARGS
{
	int error, s;
	speed_t newspeed;
	struct com_s *com;
	struct tty *tp;

	newspeed = comdefaultrate;

	error = sysctl_handle_opaque(oidp, &newspeed, sizeof newspeed, req);
	if (error || !req->newptr)
		return (error);

	comdefaultrate = newspeed;

	if (comconsole < 0)		/* serial console not selected? */
		return (0);

	com = com_addr(comconsole);
	if (!com)
		return (ENXIO);

	/*
	 * set the initial and lock rates for /dev/ttydXX and /dev/cuaXX
	 * (note, the lock rates really are boolean -- if non-zero, disallow
	 *  speed changes)
	 */
	com->it_in.c_ispeed  = com->it_in.c_ospeed =
	com->lt_in.c_ispeed  = com->lt_in.c_ospeed =
	com->it_out.c_ispeed = com->it_out.c_ospeed =
	com->lt_out.c_ispeed = com->lt_out.c_ospeed = comdefaultrate;

	/*
	 * if we're open, change the running rate too
	 */
	tp = com->tp;
	if (tp && (tp->t_state & TS_ISOPEN)) {
		tp->t_termios.c_ispeed =
		tp->t_termios.c_ospeed = comdefaultrate;
		s = spltty();
		error = comparam(tp, &tp->t_termios);
		splx(s);
	}
	return error;
}

SYSCTL_PROC(_machdep, OID_AUTO, conspeed, CTLTYPE_INT | CTLFLAG_RW,
	    0, 0, sysctl_machdep_comdefaultrate, "I", "");

#if NCARD > 0
/*
 *	PC-Card (PCMCIA) specific code.
 */
static int	sioinit		__P((struct pccard_devinfo *));
static void	siounload	__P((struct pccard_devinfo *));
static int	card_intr	__P((struct pccard_devinfo *));

PCCARD_MODULE(sio, sioinit, siounload, card_intr, 0, tty_imask);

/*
 *	Initialize the device - called from Slot manager.
 */
int
sioinit(struct pccard_devinfo *devi)
{

	/* validate unit number. */
	if (devi->isahd.id_unit >= (NSIOTOT))
		return(ENODEV);
	/* Make sure it isn't already probed. */
	if (com_addr(devi->isahd.id_unit))
		return(EBUSY);

	/* It's already probed as serial by Upper */
	devi->isahd.id_flags |= COM_C_NOPROBE; 

	/*
	 * Probe the device. If a value is returned, the
	 * device was found at the location.
	 */
	if (sioprobe(&devi->isahd) == 0)
		return(ENXIO);
	if (sioattach(&devi->isahd) == 0)
		return(ENXIO);

	return(0);
}

/*
 *	siounload - unload the driver and clear the table.
 *	XXX TODO:
 *	This is usually called when the card is ejected, but
 *	can be caused by a modunload of a controller driver.
 *	The idea is to reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
static void
siounload(struct pccard_devinfo *devi)
{
	struct com_s	*com;

	if (!devi) {
		printf("NULL devi in siounload\n");
		return;
	}
	com = com_addr(devi->isahd.id_unit);
	if (!com) {
		printf("NULL com in siounload\n");
		return;
	}
	if (!com->iobase) {
		printf("sio%d already unloaded!\n",devi->isahd.id_unit);
		return;
	}
	if (com->tp && (com->tp->t_state & TS_ISOPEN)) {
		com->gone = 1;
		printf("sio%d: unload\n", devi->isahd.id_unit);
		com->tp->t_gen++;
		ttyclose(com->tp);
		ttwakeup(com->tp);
		ttwwakeup(com->tp);
	} else {
		com_addr(com->unit) = NULL;
		if (com->ibuf != NULL)
			free(com->ibuf, M_DEVBUF);
		free(com, M_DEVBUF);
		printf("sio%d: unload,gone\n", devi->isahd.id_unit);
	}
}

/*
 *	card_intr - Shared interrupt called from
 *	front end of PC-Card handler.
 */
static int
card_intr(struct pccard_devinfo *devi)
{
	struct com_s	*com;

	COM_LOCK();
	com = com_addr(devi->isahd.id_unit);
	if (com && !com->gone)
		siointr1(com_addr(devi->isahd.id_unit));
	COM_UNLOCK();
	return(1);
}
#endif /* NCARD > 0 */

#define SET_FLAG(dev, bit)	isa_set_flags(dev, isa_get_flags(dev) | (bit))
#define CLR_FLAG(dev, bit)	isa_set_flags(dev, isa_get_flags(dev) & ~(bit))

static int
sioprobe(dev)
	device_t	dev;
{
	static bool_t	already_init;
	bool_t		failures[10];
	int		fn;
	device_t	idev;
	Port_t		iobase;
	intrmask_t	irqmap[4];
	intrmask_t	irqs;
	u_char		mcr_image;
	int		result;
	device_t	xdev;
	u_int		flags = isa_get_flags(dev);

	if (!already_init) {
		/*
		 * Turn off MCR_IENABLE for all likely serial ports.  An unused
		 * port with its MCR_IENABLE gate open will inhibit interrupts
		 * from any used port that shares the interrupt vector.
		 * XXX the gate enable is elsewhere for some multiports.
		 */
		device_t *devs;
		int count, i;

		devclass_get_devices(sio_devclass, &devs, &count);
		for (i = 0; i < count; i++) {
			xdev = devs[i];
			outb(isa_get_port(xdev) + com_mcr, 0);
		}
		free(devs, M_TEMP);
		already_init = TRUE;
	}

	if (COM_LLCONSOLE(flags)) {
		printf("sio%d: reserved for low-level i/o\n",
		       device_get_unit(dev));
		return (ENXIO);
	}

	/*
	 * If the device is on a multiport card and has an AST/4
	 * compatible interrupt control register, initialize this
	 * register and prepare to leave MCR_IENABLE clear in the mcr.
	 * Otherwise, prepare to set MCR_IENABLE in the mcr.
	 * Point idev to the device struct giving the correct id_irq.
	 * This is the struct for the master device if there is one.
	 */
	idev = dev;
	mcr_image = MCR_IENABLE;
#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(flags)) {
		idev = devclass_get_device(sio_devclass, COM_MPMASTER(flags));
		if (idev == NULL) {
			printf("sio%d: master device %d not configured\n",
			       device_get_unit(dev), COM_MPMASTER(flags));
			isa_set_irq(dev, 0);
			idev = dev;
		}
		if (!COM_NOTAST4(flags)) {
			outb(isa_get_port(idev) + com_scr,
			     isa_get_irq(idev) >= 0 ? 0x80 : 0);
			mcr_image = 0;
		}
	}
#endif /* COM_MULTIPORT */
	if (isa_get_irq(idev) < 0)
		mcr_image = 0;

	bzero(failures, sizeof failures);
	iobase = isa_get_port(dev);

	/*
	 * We don't want to get actual interrupts, just masked ones.
	 * Interrupts from this line should already be masked in the ICU,
	 * but mask them in the processor as well in case there are some
	 * (misconfigured) shared interrupts.
	 */
	disable_intr();
/* EXTRA DELAY? */

	/*
	 * Initialize the speed and the word size and wait long enough to
	 * drain the maximum of 16 bytes of junk in device output queues.
	 * The speed is undefined after a master reset and must be set
	 * before relying on anything related to output.  There may be
	 * junk after a (very fast) soft reboot and (apparently) after
	 * master reset.
	 * XXX what about the UART bug avoided by waiting in comparam()?
	 * We don't want to to wait long enough to drain at 2 bps.
	 */
	if (iobase == siocniobase)
		DELAY((16 + 1) * 1000000 / (comdefaultrate / 10));
	else {
		outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
		outb(iobase + com_dlbl, COMBRD(SIO_TEST_SPEED) & 0xff);
		outb(iobase + com_dlbh, (u_int) COMBRD(SIO_TEST_SPEED) >> 8);
		outb(iobase + com_cfcr, CFCR_8BITS);
		DELAY((16 + 1) * 1000000 / (SIO_TEST_SPEED / 10));
	}

	/*
	 * Enable the interrupt gate and disable device interupts.  This
	 * should leave the device driving the interrupt line low and
	 * guarantee an edge trigger if an interrupt can be generated.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image);
	outb(iobase + com_ier, 0);
	DELAY(1000);		/* XXX */
	irqmap[0] = isa_irq_pending();

	/*
	 * Attempt to set loopback mode so that we can send a null byte
	 * without annoying any external device.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image | MCR_LOOPBACK);

	/*
	 * Attempt to generate an output interrupt.  On 8250's, setting
	 * IER_ETXRDY generates an interrupt independent of the current
	 * setting and independent of whether the THR is empty.  On 16450's,
	 * setting IER_ETXRDY generates an interrupt independent of the
	 * current setting.  On 16550A's, setting IER_ETXRDY only
	 * generates an interrupt when IER_ETXRDY is not already set.
	 */
	outb(iobase + com_ier, IER_ETXRDY);

	/*
	 * On some 16x50 incompatibles, setting IER_ETXRDY doesn't generate
	 * an interrupt.  They'd better generate one for actually doing
	 * output.  Loopback may be broken on the same incompatibles but
	 * it's unlikely to do more than allow the null byte out.
	 */
	outb(iobase + com_data, 0);
	DELAY((1 + 2) * 1000000 / (SIO_TEST_SPEED / 10));

	/*
	 * Turn off loopback mode so that the interrupt gate works again
	 * (MCR_IENABLE was hidden).  This should leave the device driving
	 * an interrupt line high.  It doesn't matter if the interrupt
	 * line oscillates while we are not looking at it, since interrupts
	 * are disabled.
	 */
/* EXTRA DELAY? */
	outb(iobase + com_mcr, mcr_image);

    /*
	 * It's a definitly Serial PCMCIA(16550A), but still be required
	 * for IIR_TXRDY implementation ( Palido 321s, DC-1S... )
	 */
	if ( COM_NOPROBE(flags) ) {
		/* Reading IIR register twice */
		for ( fn = 0; fn < 2; fn ++ ) {
			DELAY(10000);
			failures[6] = inb(iobase + com_iir);
		}
		/* Check IIR_TXRDY clear ? */
		isa_set_portsize(dev, IO_COMSIZE);
		result = 0;
		if ( failures[6] & IIR_TXRDY ) {
			/* Nop, Double check with clearing IER */
			outb(iobase + com_ier, 0);
			if ( inb(iobase + com_iir) & IIR_NOPEND ) {
				/* Ok. we're familia this gang */
				SET_FLAG(dev, COM_C_IIR_TXRDYBUG); /* Set IIR_TXRDYBUG */
			} else {
				/* Unknow, Just omit this chip.. XXX*/
				result = ENXIO;
			}
		} else {
			/* OK. this is well-known guys */
			CLR_FLAG(dev, COM_C_IIR_TXRDYBUG); /*Clear IIR_TXRDYBUG*/
		}
		outb(iobase + com_cfcr, CFCR_8BITS);
		enable_intr();
		return (iobase == siocniobase ? 0 : result);
	}

	/*
	 * Check that
	 *	o the CFCR, IER and MCR in UART hold the values written to them
	 *	  (the values happen to be all distinct - this is good for
	 *	  avoiding false positive tests from bus echoes).
	 *	o an output interrupt is generated and its vector is correct.
	 *	o the interrupt goes away when the IIR in the UART is read.
	 */
/* EXTRA DELAY? */
	failures[0] = inb(iobase + com_cfcr) - CFCR_8BITS;
	failures[1] = inb(iobase + com_ier) - IER_ETXRDY;
	failures[2] = inb(iobase + com_mcr) - mcr_image;
	DELAY(10000);		/* Some internal modems need this time */
	irqmap[1] = isa_irq_pending();
	failures[4] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_TXRDY;
	DELAY(1000);		/* XXX */
	irqmap[2] = isa_irq_pending();
	failures[6] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	/*
	 * Turn off all device interrupts and check that they go off properly.
	 * Leave MCR_IENABLE alone.  For ports without a master port, it gates
	 * the OUT2 output of the UART to
	 * the ICU input.  Closing the gate would give a floating ICU input
	 * (unless there is another device driving it) and spurious interrupts.
	 * (On the system that this was first tested on, the input floats high
	 * and gives a (masked) interrupt as soon as the gate is closed.)
	 */
	outb(iobase + com_ier, 0);
	outb(iobase + com_cfcr, CFCR_8BITS);	/* dummy to avoid bus echo */
	failures[7] = inb(iobase + com_ier);
	DELAY(1000);		/* XXX */
	irqmap[3] = isa_irq_pending();
	failures[9] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	enable_intr();

	irqs = irqmap[1] & ~irqmap[0];
	if (isa_get_irq(idev) >= 0 && ((1 << isa_get_irq(idev)) & irqs) == 0)
		printf(
		"sio%d: configured irq %d not in bitmap of probed irqs %#x\n",
		    device_get_unit(dev), isa_get_irq(idev), irqs);
	if (bootverbose)
		printf("sio%d: irq maps: %#x %#x %#x %#x\n",
		    device_get_unit(dev),
		    irqmap[0], irqmap[1], irqmap[2], irqmap[3]);

	isa_set_portsize(dev, IO_COMSIZE);
	result = 0;
	for (fn = 0; fn < sizeof failures; ++fn)
		if (failures[fn]) {
			outb(iobase + com_mcr, 0);
			result = ENXIO;
			if (bootverbose) {
				printf("sio%d: probe failed test(s):",
				    device_get_unit(dev));
				for (fn = 0; fn < sizeof failures; ++fn)
					if (failures[fn])
						printf(" %d", fn);
				printf("\n");
			}
			break;
		}
	return (iobase == siocniobase ? 0 : result);
}

#ifdef COM_ESP
static int
espattach(isdp, com, esp_port)
	struct isa_device	*isdp;
	struct com_s		*com;
	Port_t			esp_port;
{
	u_char	dips;
	u_char	val;

	/*
	 * Check the ESP-specific I/O port to see if we're an ESP
	 * card.  If not, return failure immediately.
	 */
	if ((inb(esp_port) & 0xf3) == 0) {
		printf(" port 0x%x is not an ESP board?\n", esp_port);
		return (0);
	}

	/*
	 * We've got something that claims to be a Hayes ESP card.
	 * Let's hope so.
	 */

	/* Get the dip-switch configuration */
	outb(esp_port + ESP_CMD1, ESP_GETDIPS);
	dips = inb(esp_port + ESP_STATUS1);

	/*
	 * Bits 0,1 of dips say which COM port we are.
	 */
	if (com->iobase == likely_com_ports[dips & 0x03])
		printf(" : ESP");
	else {
		printf(" esp_port has com %d\n", dips & 0x03);
		return (0);
	}

	/*
	 * Check for ESP version 2.0 or later:  bits 4,5,6 = 010.
	 */
	outb(esp_port + ESP_CMD1, ESP_GETTEST);
	val = inb(esp_port + ESP_STATUS1);	/* clear reg 1 */
	val = inb(esp_port + ESP_STATUS2);
	if ((val & 0x70) < 0x20) {
		printf("-old (%o)", val & 0x70);
		return (0);
	}

	/*
	 * Check for ability to emulate 16550:  bit 7 == 1
	 */
	if ((dips & 0x80) == 0) {
		printf(" slave");
		return (0);
	}

	/*
	 * Okay, we seem to be a Hayes ESP card.  Whee.
	 */
	com->esp = TRUE;
	com->esp_port = esp_port;
	return (1);
}
#endif /* COM_ESP */

static int
sioattach(dev)
	device_t	dev;
{
	struct com_s	*com;
#ifdef COM_ESP
	Port_t		*espp;
#endif
	Port_t		iobase;
#if 0
	int		s;
#endif
	int		unit;
	void		*ih;
	struct resource *res;
	int		zero = 0;
	u_int		flags = isa_get_flags(dev);

#if 0
	isdp->id_ri_flags |= RI_FAST;
#endif
	iobase = isa_get_port(dev);
	unit = device_get_unit(dev);
	com = device_get_softc(dev);

	/*
	 * sioprobe() has initialized the device registers as follows:
	 *	o cfcr = CFCR_8BITS.
	 *	  It is most important that CFCR_DLAB is off, so that the
	 *	  data port is not hidden when we enable interrupts.
	 *	o ier = 0.
	 *	  Interrupts are only enabled when the line is open.
	 *	o mcr = MCR_IENABLE, or 0 if the port has AST/4 compatible
	 *	  interrupt control register or the config specifies no irq.
	 *	  Keeping MCR_DTR and MCR_RTS off might stop the external
	 *	  device from sending before we are ready.
	 */
	bzero(com, sizeof *com);
	com->unit = unit;
	com->cfcr_image = CFCR_8BITS;
	com->dtr_wait = 3 * hz;
	com->loses_outints = COM_LOSESOUTINTS(flags) != 0;
	com->no_irq = isa_get_irq(dev) < 0;
	com->tx_fifo_size = 1;
	com->obufs[0].l_head = com->obuf1;
	com->obufs[1].l_head = com->obuf2;

	com->iobase = iobase;
	com->data_port = iobase + com_data;
	com->int_id_port = iobase + com_iir;
	com->modem_ctl_port = iobase + com_mcr;
	com->mcr_image = inb(com->modem_ctl_port);
	com->line_status_port = iobase + com_lsr;
	com->modem_status_port = iobase + com_msr;
	com->intr_ctl_port = iobase + com_ier;

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
		com->lt_out.c_ispeed = com->lt_out.c_ospeed =
		com->lt_in.c_ispeed = com->lt_in.c_ospeed =
		com->it_in.c_ispeed = com->it_in.c_ospeed = comdefaultrate;
	} else
		com->it_in.c_ispeed = com->it_in.c_ospeed = TTYDEF_SPEED;
	if (siosetwater(com, com->it_in.c_ispeed) != 0) {
		enable_intr();
		free(com, M_DEVBUF);
		return (0);
	}
	enable_intr();
	termioschars(&com->it_in);
	com->it_out = com->it_in;

	/* attempt to determine UART type */
	printf("sio%d: type", unit);


#ifdef COM_MULTIPORT
	if (!COM_ISMULTIPORT(flags) && !COM_IIR_TXRDYBUG(flags))
#else
	if (!COM_IIR_TXRDYBUG(flags))
#endif
	{
		u_char	scr;
		u_char	scr1;
		u_char	scr2;

		scr = inb(iobase + com_scr);
		outb(iobase + com_scr, 0xa5);
		scr1 = inb(iobase + com_scr);
		outb(iobase + com_scr, 0x5a);
		scr2 = inb(iobase + com_scr);
		outb(iobase + com_scr, scr);
		if (scr1 != 0xa5 || scr2 != 0x5a) {
			printf(" 8250");
			goto determined_type;
		}
	}
	outb(iobase + com_fifo, FIFO_ENABLE | FIFO_RX_HIGH);
	DELAY(100);
	com->st16650a = 0;
	switch (inb(com->int_id_port) & IIR_FIFO_MASK) {
	case FIFO_RX_LOW:
		printf(" 16450");
		break;
	case FIFO_RX_MEDL:
		printf(" 16450?");
		break;
	case FIFO_RX_MEDH:
		printf(" 16550?");
		break;
	case FIFO_RX_HIGH:
		if (COM_NOFIFO(flags)) {
			printf(" 16550A fifo disabled");
		} else {
			com->hasfifo = TRUE;
			if (COM_ST16650A(flags)) {
				com->st16650a = 1;
				com->tx_fifo_size = 32;
				printf(" ST16650A");
			} else {
				com->tx_fifo_size = COM_FIFOSIZE(flags);
				printf(" 16550A");
			}
		}
#ifdef COM_ESP
		for (espp = likely_esp_ports; *espp != 0; espp++)
			if (espattach(dev, com, *espp)) {
				com->tx_fifo_size = 1024;
				break;
			}
#endif
		if (!com->st16650a) {
			if (!com->tx_fifo_size)
				com->tx_fifo_size = 16;
			else
				printf(" lookalike with %d bytes FIFO",
				    com->tx_fifo_size);
		}

		break;
	}
	
#ifdef COM_ESP
	if (com->esp) {
		/*
		 * Set 16550 compatibility mode.
		 * We don't use the ESP_MODE_SCALE bit to increase the
		 * fifo trigger levels because we can't handle large
		 * bursts of input.
		 * XXX flow control should be set in comparam(), not here.
		 */
		outb(com->esp_port + ESP_CMD1, ESP_SETMODE);
		outb(com->esp_port + ESP_CMD2, ESP_MODE_RTS | ESP_MODE_FIFO);

		/* Set RTS/CTS flow control. */
		outb(com->esp_port + ESP_CMD1, ESP_SETFLOWTYPE);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_RTS);
		outb(com->esp_port + ESP_CMD2, ESP_FLOW_CTS);

		/* Set flow-control levels. */
		outb(com->esp_port + ESP_CMD1, ESP_SETRXFLOW);
		outb(com->esp_port + ESP_CMD2, HIBYTE(768));
		outb(com->esp_port + ESP_CMD2, LOBYTE(768));
		outb(com->esp_port + ESP_CMD2, HIBYTE(512));
		outb(com->esp_port + ESP_CMD2, LOBYTE(512));
	}
#endif /* COM_ESP */
	outb(iobase + com_fifo, 0);
determined_type: ;

#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(flags)) {
		com->multiport = TRUE;
		printf(" (multiport");
		if (unit == COM_MPMASTER(flags))
			printf(" master");
		printf(")");
		com->no_irq =
			isa_get_irq(devclass_get_device
				    (sio_devclass, COM_MPMASTER(flags))) < 0;
	 }
#endif /* COM_MULTIPORT */
	if (unit == comconsole)
		printf(", console");
	if ( COM_IIR_TXRDYBUG(flags) )
		printf(" with a bogus IIR_TXRDY register");
	printf("\n");

#if 0
	s = spltty();
	com_addr(unit) = com;
	splx(s);
#endif

	if (!sio_registered) {
		register_swi(SWI_TTY, siopoll);
		sio_registered = TRUE;
	}
#ifdef DEVFS
	com->devfs_token_ttyd = devfs_add_devswf(&sio_cdevsw,
		unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyd%r", unit);
	com->devfs_token_ttyi = devfs_add_devswf(&sio_cdevsw,
		unit | CONTROL_INIT_STATE, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyid%r", unit);
	com->devfs_token_ttyl = devfs_add_devswf(&sio_cdevsw,
		unit | CONTROL_LOCK_STATE, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "ttyld%r", unit);
	com->devfs_token_cuaa = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuaa%r", unit);
	com->devfs_token_cuai = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK | CONTROL_INIT_STATE, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuaia%r", unit);
	com->devfs_token_cual = devfs_add_devswf(&sio_cdevsw,
		unit | CALLOUT_MASK | CONTROL_LOCK_STATE, DV_CHR,
		UID_UUCP, GID_DIALER, 0660, "cuala%r", unit);
#endif
	com->flags = isa_get_flags(dev); /* Heritate id_flags for later */
	com->pps.ppscap = PPS_CAPTUREASSERT | PPS_CAPTURECLEAR;
	pps_init(&com->pps);

	res = bus_alloc_resource(dev, SYS_RES_IRQ, &zero, 0ul, ~0ul, 1,
				 RF_SHAREABLE | RF_ACTIVE);
	BUS_SETUP_INTR(device_get_parent(dev), dev, res, siointr, com,
		       &ih);

	return (0);
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
	Port_t		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
	int		unit;

	mynor = minor(dev);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIOTOT || (com = com_addr(unit)) == NULL)
		return (ENXIO);
	if (com->gone)
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
		error = tsleep(&com->dtr_wait, TTIPRI | PCATCH, "siodtr", 0);
		if (com_addr(unit) == NULL)
			return (ENXIO);
		if (error != 0 || com->gone)
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
					       TTIPRI | PCATCH, "siobi", 0);
				if (com_addr(unit) == NULL)
					return (ENXIO);
				if (error != 0 || com->gone)
					goto out;
				goto open_top;
			}
		}
		if (tp->t_state & TS_XCLUDE &&
		    suser(p->p_ucred, &p->p_acflag)) {
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
		(void)commctl(com, TIOCM_DTR | TIOCM_RTS, DMSET);
		com->poll = com->no_irq;
		com->poll_output = com->loses_outints;
		++com->wopeners;
		error = comparam(tp, &tp->t_termios);
		--com->wopeners;
		if (error != 0)
			goto out;
		/*
		 * XXX we should goto open_top if comparam() slept.
		 */
		iobase = com->iobase;
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
				     | com->fifo_image);
				/*
				 * XXX the delays are for superstitious
				 * historical reasons.  It must be less than
				 * the character time at the maximum
				 * supported speed (87 usec at 115200 bps
				 * 8N1).  Otherwise we might loop endlessly
				 * if data is streaming in.  We used to use
				 * delays of 100.  That usually worked
				 * because DELAY(100) used to usually delay
				 * for about 85 usec instead of 100.
				 */
				DELAY(50);
				if (!(inb(com->line_status_port) & LSR_RXRDY))
					break;
				outb(iobase + com_fifo, 0);
				DELAY(50);
				(void) inb(com->data_port);
			}
		}

		disable_intr();
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
		com->prev_modem_status = com->last_modem_status
		    = inb(com->modem_status_port);
		if (COM_IIR_TXRDYBUG(com->flags)) {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ERLS
						| IER_EMSC);
		} else {
			outb(com->intr_ctl_port, IER_ERXRDY | IER_ETXRDY
						| IER_ERLS | IER_EMSC);
		}
		enable_intr();
		/*
		 * Handle initial DCD.  Callout devices get a fake initial
		 * DCD (trapdoor DCD).  If we are callout, then any sleeping
		 * callin opens get woken up and resume sleeping on "siobi"
		 * instead of "siodcd".
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
		error = tsleep(TSA_CARR_ON(tp), TTIPRI | PCATCH, "siodcd", 0);
		if (com_addr(unit) == NULL)
			return (ENXIO);
		--com->wopeners;
		if (error != 0 || com->gone)
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
	if (com->gone) {
		printf("sio%d: gone\n", com->unit);
		s = spltty();
#if 0
		com_addr(com->unit) = NULL;
#endif
		if (com->ibuf != NULL)
			free(com->ibuf, M_DEVBUF);
		bzero(tp, sizeof *tp);
		free(com, M_DEVBUF);
		splx(s);
	}
	return (0);
}

static void
comhardclose(com)
	struct com_s	*com;
{
	Port_t		iobase;
	int		s;
	struct tty	*tp;
	int		unit;

	unit = com->unit;
	iobase = com->iobase;
	s = spltty();
	com->poll = FALSE;
	com->poll_output = FALSE;
	com->do_timestamp = FALSE;
	com->do_dcd_timestamp = FALSE;
	com->pps.ppsparam.mode = 0;
	outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
	{
		outb(iobase + com_ier, 0);
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
			if (com->dtr_wait != 0 && !(com->state & CS_DTR_OFF)) {
				timeout(siodtrwakeup, com, com->dtr_wait);
				com->state |= CS_DTR_OFF;
			}
		}
	}
	if (com->hasfifo) {
		/*
		 * Disable fifos so that they are off after controlled
		 * reboots.  Some BIOSes fail to detect 16550s when the
		 * fifos are enabled.
		 */
		outb(iobase + com_fifo, 0);
	}
	com->active_out = FALSE;
	wakeup(&com->active_out);
	wakeup(TSA_CARR_ON(tp));	/* restart any wopeners */
	splx(s);
}

static int
sioread(dev, uio, flag)
	dev_t		dev;
	struct uio	*uio;
	int		flag;
{
	int		mynor;
	int		unit;
	struct tty	*tp;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (ENODEV);
	unit = MINOR_TO_UNIT(mynor);
	if (com_addr(unit)->gone)
		return (ENODEV);
	tp = com_addr(unit)->tp;
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
	if (com_addr(unit)->gone)
		return (ENODEV);
	tp = com_addr(unit)->tp;
	/*
	 * (XXX) We disallow virtual consoles if the physical console is
	 * a serial port.  This is in case there is a display attached that
	 * is not the console.  In that situation we don't need/want the X
	 * server taking over the console.
	 */
	if (constty != NULL && unit == comconsole)
		constty = NULL;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

static void
siobusycheck(chan)
	void	*chan;
{
	struct com_s	*com;
	int		s;

	com = (struct com_s *)chan;

	/*
	 * Clear TS_BUSY if low-level output is complete.
	 * spl locking is sufficient because siointr1() does not set CS_BUSY.
	 * If siointr1() clears CS_BUSY after we look at it, then we'll get
	 * called again.  Reading the line status port outside of siointr1()
	 * is safe because CS_BUSY is clear so there are no output interrupts
	 * to lose.
	 */
	s = spltty();
	if (com->state & CS_BUSY)
		com->extra_state &= ~CSE_BUSYCHECK;	/* False alarm. */
	else if ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	    == (LSR_TSRE | LSR_TXRDY)) {
		com->tp->t_state &= ~TS_BUSY;
		ttwwakeup(com->tp);
		com->extra_state &= ~CSE_BUSYCHECK;
	} else
		timeout(siobusycheck, com, hz / 100);
	splx(s);
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
	if (!(tp->t_state & TS_ISOPEN) || !(tp->t_cflag & CREAD)) {
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
			enable_intr();
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
			disable_intr();
		} while (buf < com->iptr);
	} else {
		do {
			enable_intr();
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
			disable_intr();
		} while (buf < com->iptr);
	}
	com_events -= (com->iptr - com->ibuf);
	com->iptr = com->ibuf;

	/*
	 * There is now room for another low-level buffer full of input,
	 * so enable RTS if it is now disabled and there is room in the
	 * high-level buffer.
	 */
	if ((com->state & CS_RTS_IFLOW) && !(com->mcr_image & MCR_RTS) &&
	    !(tp->t_state & TS_TBLOCK))
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
}

void
siointr(arg)
	void		*arg;
{
#ifndef COM_MULTIPORT
	COM_LOCK();
	siointr1((struct com_s *) arg);
	COM_UNLOCK();
#else /* COM_MULTIPORT */
	bool_t		possibly_more_intrs;

	/*
	 * Loop until there is no activity on any port.  This is necessary
	 * to get an interrupt edge more than to avoid another interrupt.
	 * If the IRQ signal is just an OR of the IRQ signals from several
	 * devices, then the edge from one may be lost because another is
	 * on.
	 */
	COM_LOCK();
	do {
		possibly_more_intrs = FALSE;
		for (unit = 0; unit < NSIOTOT; ++unit) {
			com = com_addr(unit);
			/*
			 * XXX COM_LOCK();
			 * would it work here, or be counter-productive?
			 */
			if (com != NULL 
			    && !com->gone
			    && (inb(com->int_id_port) & IIR_IMASK)
			       != IIR_NOPEND) {
				siointr1(com);
				possibly_more_intrs = TRUE;
			}
			/* XXX COM_UNLOCK(); */
		}
	} while (possibly_more_intrs);
	COM_UNLOCK();
#endif /* COM_MULTIPORT */
}

static void
siointr1(com)
	struct com_s	*com;
{
	u_char	line_status;
	u_char	modem_status;
	u_char	*ioptr;
	u_char	recv_data;
	u_char	int_ctl;
	u_char	int_ctl_new;
	struct	timecounter *tc;
	u_int	count;

	int_ctl = inb(com->intr_ctl_port);
	int_ctl_new = int_ctl;

	while (!com->gone) {
		if (com->pps.ppsparam.mode & PPS_CAPTUREBOTH) {
			modem_status = inb(com->modem_status_port);
		        if ((modem_status ^ com->last_modem_status) & MSR_DCD) {
				tc = timecounter;
				count = tc->tc_get_timecount(tc);
				pps_event(&com->pps, tc, count, 
				    (modem_status & MSR_DCD) ? 
				    PPS_CAPTURECLEAR : PPS_CAPTUREASSERT);
			}
		}
		line_status = inb(com->line_status_port);

		/* input event? (check first to help avoid overruns) */
		while (line_status & LSR_RCV_MASK) {
			/* break/unnattached error bits or real input? */
			if (!(line_status & LSR_RXRDY))
				recv_data = 0;
			else
				recv_data = inb(com->data_port);
			if (line_status & (LSR_BI | LSR_FE | LSR_PE)) {
				/*
				 * Don't store BI if IGNBRK or FE/PE if IGNPAR.
				 * Otherwise, push the work to a higher level
				 * (to handle PARMRK) if we're bypassing.
				 * Otherwise, convert BI/FE and PE+INPCK to 0.
				 *
				 * This makes bypassing work right in the
				 * usual "raw" case (IGNBRK set, and IGNPAR
				 * and INPCK clear).
				 *
				 * Note: BI together with FE/PE means just BI.
				 */
				if (line_status & LSR_BI) {
#if defined(DDB) && defined(BREAK_TO_DEBUGGER)
					if (com->unit == comconsole) {
						breakpoint();
						goto cont;
					}
#endif
					if (com->tp == NULL
					    || com->tp->t_iflag & IGNBRK)
						goto cont;
				} else {
					if (com->tp == NULL
					    || com->tp->t_iflag & IGNPAR)
						goto cont;
				}
				if (com->tp->t_state & TS_CAN_BYPASS_L_RINT
				    && (line_status & (LSR_BI | LSR_FE)
					|| com->tp->t_iflag & INPCK))
					recv_data = 0;
			}
			++com->bytes_in;
			if (com->hotchar != 0 && recv_data == com->hotchar)
				setsofttty();
			ioptr = com->iptr;
			if (ioptr >= com->ibufend)
				CE_RECORD(com, CE_INTERRUPT_BUF_OVERFLOW);
			else {
				if (com->do_timestamp)
					microtime(&com->timestamp);
				++com_events;
				schedsofttty();
#if 0 /* for testing input latency vs efficiency */
if (com->iptr - com->ibuf == 8)
	setsofttty();
#endif
				ioptr[0] = recv_data;
				ioptr[com->ierroff] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
				if (line_status & LSR_OE)
					CE_RECORD(com, CE_OVERRUN);
			}
cont:
			/*
			 * "& 0x7F" is to avoid the gcc-1.40 generating a slow
			 * jump from the top of the loop to here
			 */
			line_status = inb(com->line_status_port) & 0x7F;
		}

		/* modem status change? (always check before doing output) */
		modem_status = inb(com->modem_status_port);
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
				setsofttty();
			}

			/* handle CTS change immediately for crisp flow ctl */
			if (com->state & CS_CTS_OFLOW) {
				if (modem_status & MSR_CTS)
					com->state |= CS_ODEVREADY;
				else
					com->state &= ~CS_ODEVREADY;
			}
		}

		/* output queued and everything ready? */
		if (line_status & LSR_TXRDY
		    && com->state >= (CS_BUSY | CS_TTGO | CS_ODEVREADY)) {
			ioptr = com->obufq.l_head;
			if (com->tx_fifo_size > 1) {
				u_int	ocount;

				ocount = com->obufq.l_tail - ioptr;
				if (ocount > com->tx_fifo_size)
					ocount = com->tx_fifo_size;
				com->bytes_out += ocount;
				do
					outb(com->data_port, *ioptr++);
				while (--ocount != 0);
			} else {
				outb(com->data_port, *ioptr++);
				++com->bytes_out;
			}
			com->obufq.l_head = ioptr;
			if (COM_IIR_TXRDYBUG(com->flags)) {
				int_ctl_new = int_ctl | IER_ETXRDY;
			}
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
					if ( COM_IIR_TXRDYBUG(com->flags) ) {
						int_ctl_new = int_ctl & ~IER_ETXRDY;
					}
					com->state &= ~CS_BUSY;
				}
				if (!(com->state & CS_ODONE)) {
					com_events += LOTS_OF_EVENTS;
					com->state |= CS_ODONE;
					setsofttty();	/* handle at high level ASAP */
				}
			}
			if ( COM_IIR_TXRDYBUG(com->flags) && (int_ctl != int_ctl_new)) {
				outb(com->intr_ctl_port, int_ctl_new);
			}
		}

		/* finished? */
#ifndef COM_MULTIPORT
		if ((inb(com->int_id_port) & IIR_IMASK) == IIR_NOPEND)
#endif /* COM_MULTIPORT */
			return;
	}
}

static int
sioioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	u_long		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	Port_t		iobase;
	int		mynor;
	int		s;
	struct tty	*tp;
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
	u_long		oldcmd;
	struct termios	term;
#endif

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
	if (com->gone)
		return (ENODEV);
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
		outb(iobase + com_cfcr, com->cfcr_image |= CFCR_SBREAK);
		break;
	case TIOCCBRK:
		outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
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
	case TIOCDCDTIMESTAMP:
		com->do_dcd_timestamp = TRUE;
		*(struct timeval *)data = com->dcd_timestamp;
		break;
	default:
		splx(s);
		error = pps_ioctl(cmd, data, &com->pps);
		if (error == ENODEV)
			error = ENOTTY;
		return (error);
	}
	splx(s);
	return (0);
}

static void
siopoll()
{
	int		unit;

	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < NSIOTOT; ++unit) {
		struct com_s	*com;
		int		incc;
		struct tty	*tp;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		tp = com->tp;
		if (tp == NULL || com->gone) {
			/*
			 * Discard any events related to never-opened or
			 * going-away devices.
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
			continue;
		}
		if (com->iptr != com->ibuf) {
			disable_intr();
			sioinput(com);
			enable_intr();
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
			enable_intr();
			if (!(com->state & CS_BUSY)
			    && !(com->extra_state & CSE_BUSYCHECK)) {
				timeout(siobusycheck, com, hz / 100);
				com->extra_state |= CSE_BUSYCHECK;
			}
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
	u_int		cfcr;
	int		cflag;
	struct com_s	*com;
	int		divisor;
	u_char		dlbh;
	u_char		dlbl;
	Port_t		iobase;
	int		s;
	int		unit;

	/* do historical conversions */
	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;

	/* check requested parameters */
	divisor = ttspeedtab(t->c_ospeed, comspeedtab);
	if (divisor < 0 || divisor > 0 && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/* parameters are OK, convert them to the com struct and the device */
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
	s = spltty();
	if (divisor == 0)
		(void)commctl(com, TIOCM_DTR, DMBIC);	/* hang up line */
	else
		(void)commctl(com, TIOCM_DTR, DMBIS);
	cflag = t->c_cflag;
	switch (cflag & CSIZE) {
	case CS5:
		cfcr = CFCR_5BITS;
		break;
	case CS6:
		cfcr = CFCR_6BITS;
		break;
	case CS7:
		cfcr = CFCR_7BITS;
		break;
	default:
		cfcr = CFCR_8BITS;
		break;
	}
	if (cflag & PARENB) {
		cfcr |= CFCR_PENAB;
		if (!(cflag & PARODD))
			cfcr |= CFCR_PEVEN;
	}
	if (cflag & CSTOPB)
		cfcr |= CFCR_STOPB;

	if (com->hasfifo && divisor != 0) {
		/*
		 * Use a fifo trigger level low enough so that the input
		 * latency from the fifo is less than about 16 msec and
		 * the total latency is less than about 30 msec.  These
		 * latencies are reasonable for humans.  Serial comms
		 * protocols shouldn't expect anything better since modem
		 * latencies are larger.
		 */
		com->fifo_image = t->c_ospeed <= 4800
				  ? FIFO_ENABLE : FIFO_ENABLE | FIFO_RX_HIGH;
#ifdef COM_ESP
		/*
		 * The Hayes ESP card needs the fifo DMA mode bit set
		 * in compatibility mode.  If not, it will interrupt
		 * for each character received.
		 */
		if (com->esp)
			com->fifo_image |= FIFO_DMA_MODE;
#endif
		outb(iobase + com_fifo, com->fifo_image);
	}

	/*
	 * This returns with interrupts disabled so that we can complete
	 * the speed change atomically.  Keeping interrupts disabled is
	 * especially important while com_data is hidden.
	 */
	(void) siosetwater(com, t->c_ispeed);

	if (divisor != 0) {
		outb(iobase + com_cfcr, cfcr | CFCR_DLAB);
		/*
		 * Only set the divisor registers if they would change,
		 * since on some 16550 incompatibles (UMC8669F), setting
		 * them while input is arriving them loses sync until
		 * data stops arriving.
		 */
		dlbl = divisor & 0xFF;
		if (inb(iobase + com_dlbl) != dlbl)
			outb(iobase + com_dlbl, dlbl);
		dlbh = (u_int) divisor >> 8;
		if (inb(iobase + com_dlbh) != dlbh)
			outb(iobase + com_dlbh, dlbh);
	}


	outb(iobase + com_cfcr, com->cfcr_image = cfcr);

	if (!(tp->t_state & TS_TTSTOP))
		com->state |= CS_TTGO;

	if (cflag & CRTS_IFLOW) {
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) | 0x40);
		}
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
		outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) & ~0x40);
		}
	}


	/*
	 * Set up state to handle output flow control.
	 * XXX - worth handling MDMBUF (DCD) flow control at the lowest level?
	 * Now has 10+ msec latency, while CTS flow has 50- usec latency.
	 */
	com->state |= CS_ODEVREADY;
	com->state &= ~CS_CTS_OFLOW;
	if (cflag & CCTS_OFLOW) {
		com->state |= CS_CTS_OFLOW;
		if (!(com->last_modem_status & MSR_CTS))
			com->state &= ~CS_ODEVREADY;
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) | 0x80);
		}
	} else {
		if (com->st16650a) {
			outb(iobase + com_cfcr, 0xbf);
			outb(iobase + com_fifo, inb(iobase + com_fifo) & ~0x80);
		}
	}


	outb(iobase + com_cfcr, com->cfcr_image);


	/* XXX shouldn't call functions while intrs are disabled. */
	disc_optim(tp, t, com);
	/*
	 * Recover from fiddling with CS_TTGO.  We used to call siointr1()
	 * unconditionally, but that defeated the careful discarding of
	 * stale input in sioopen().
	 */
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);

	enable_intr();
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
		disable_intr();
		return (0);
	}

	/*
	 * Allocate input buffer.  The extra factor of 2 in the size is
	 * to allow for an error byte for each input byte.
	 */
	ibuf = malloc(2 * ibufsize, M_DEVBUF, M_NOWAIT);
	if (ibuf == NULL) {
		disable_intr();
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
	disable_intr();
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
	return (0);
}

static void
comstart(tp)
	struct tty	*tp;
{
	struct com_s	*com;
	int		s;
	int		unit;

	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	s = spltty();
	disable_intr();
	if (tp->t_state & TS_TTSTOP)
		com->state &= ~CS_TTGO;
	else
		com->state |= CS_TTGO;
	if (tp->t_state & TS_TBLOCK) {
		if (com->mcr_image & MCR_RTS && com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
	} else {
		if (!(com->mcr_image & MCR_RTS) && com->iptr < com->ihighwater
		    && com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(s);
		return;
	}
	if (tp->t_outq.c_cc != 0) {
		struct lbq	*qp;
		struct lbq	*next;

		if (!com->obufs[0].l_queued) {
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
			}
			enable_intr();
		}
		if (tp->t_outq.c_cc != 0 && !com->obufs[1].l_queued) {
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
			}
			enable_intr();
		}
		tp->t_state |= TS_BUSY;
	}
	disable_intr();
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);	/* fake interrupt to start output */
	enable_intr();
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
	if (com->gone)
		return;
	disable_intr();
	if (rw & FWRITE) {
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			outb(com->iobase + com_fifo,
			     FIFO_XMT_RST | com->fifo_image);
		com->obufs[0].l_queued = FALSE;
		com->obufs[1].l_queued = FALSE;
		if (com->state & CS_ODONE)
			com_events -= LOTS_OF_EVENTS;
		com->state &= ~(CS_ODONE | CS_BUSY);
		com->tp->t_state &= ~TS_BUSY;
	}
	if (rw & FREAD) {
		if (com->hasfifo)
#ifdef COM_ESP
		    /* XXX avoid h/w bug. */
		    if (!com->esp)
#endif
			outb(com->iobase + com_fifo,
			     FIFO_RCV_RST | com->fifo_image);
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	enable_intr();
	comstart(tp);
}

static struct tty *
siodevtotty(dev)
	dev_t	dev;
{
	int	mynor;
	int	unit;

	mynor = minor(dev);
	if (mynor & CONTROL_MASK)
		return (NULL);
	unit = MINOR_TO_UNIT(mynor);
	if ((u_int) unit >= NSIOTOT)
		return (NULL);
	return (&sio_tty[unit]);
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
		bits = TIOCM_LE;	/* XXX - always enabled while open */
		mcr = com->mcr_image;
		if (mcr & MCR_DTR)
			bits |= TIOCM_DTR;
		if (mcr & MCR_RTS)
			bits |= TIOCM_RTS;
		msr = com->prev_modem_status;
		if (msr & MSR_CTS)
			bits |= TIOCM_CTS;
		if (msr & MSR_DCD)
			bits |= TIOCM_CD;
		if (msr & MSR_DSR)
			bits |= TIOCM_DSR;
		/*
		 * XXX - MSR_RI is naturally volatile, and we make MSR_TERI
		 * more volatile by reading the modem status a lot.  Perhaps
		 * we should latch both bits until the status is read here.
		 */
		if (msr & (MSR_RI | MSR_TERI))
			bits |= TIOCM_RI;
		return (bits);
	}
	mcr = 0;
	if (bits & TIOCM_DTR)
		mcr |= MCR_DTR;
	if (bits & TIOCM_RTS)
		mcr |= MCR_RTS;
	if (com->gone)
		return(0);
	disable_intr();
	switch (how) {
	case DMSET:
		outb(com->modem_ctl_port,
		     com->mcr_image = mcr | (com->mcr_image & MCR_IENABLE));
		break;
	case DMBIS:
		outb(com->modem_ctl_port, com->mcr_image |= mcr);
		break;
	case DMBIC:
		outb(com->modem_ctl_port, com->mcr_image &= ~mcr);
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
	untimeout(comwakeup, (void *)NULL, sio_timeout_handle);
	sio_timeout = hz;
	someopen = FALSE;
	for (unit = 0; unit < NSIOTOT; ++unit) {
		com = com_addr(unit);
		if (com != NULL && com->tp != NULL
		    && com->tp->t_state & TS_ISOPEN && !com->gone) {
			someopen = TRUE;
			if (com->poll || com->poll_output) {
				sio_timeout = hz > 200 ? hz / 200 : 1;
				break;
			}
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

	/*
	 * Recover from lost output interrupts.
	 * Poll any lines that don't use interrupts.
	 */
	for (unit = 0; unit < NSIOTOT; ++unit) {
		com = com_addr(unit);
		if (com != NULL && !com->gone
		    && (com->state >= (CS_BUSY | CS_TTGO) || com->poll)) {
			disable_intr();
			siointr1(com);
			enable_intr();
		}
	}

	/*
	 * Check for and log errors, but not too often.
	 */
	if (--sio_timeouts_until_log > 0)
		return;
	sio_timeouts_until_log = hz / sio_timeout;
	for (unit = 0; unit < NSIOTOT; ++unit) {
		int	errnum;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		if (com->gone)
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
			log(LOG_ERR, "sio%d: %u more %s%s (total %lu)\n",
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
}

/*
 * Following are all routines needed for SIO to act as console
 */
#include <machine/cons.h>

struct siocnstate {
	u_char	dlbl;
	u_char	dlbh;
	u_char	ier;
	u_char	cfcr;
	u_char	mcr;
};

static speed_t siocngetspeed __P((Port_t, struct speedtab *));
static void siocnclose	__P((struct siocnstate *sp, Port_t iobase));
static void siocnopen	__P((struct siocnstate *sp, Port_t iobase, int speed));
static void siocntxwait	__P((Port_t iobase));

#ifdef __i386__
/*
 * XXX: sciocnget() and sciocnputc() are not declared static, as they are
 * referred to from i386/i386/i386-gdbstub.c.
 */
static cn_probe_t siocnprobe;
static cn_init_t siocninit;
static cn_checkc_t siocncheckc;
       cn_getc_t siocngetc;
       cn_putc_t siocnputc;

CONS_DRIVER(sio, siocnprobe, siocninit, siocngetc, siocncheckc, siocnputc);

#endif

static void
siocntxwait(iobase)
	Port_t	iobase;
{
	int	timo;

	/*
	 * Wait for any pending transmission to finish.  Required to avoid
	 * the UART lockup bug when the speed is changed, and for normal
	 * transmits.
	 */
	timo = 100000;
	while ((inb(iobase + com_lsr) & (LSR_TSRE | LSR_TXRDY))
	       != (LSR_TSRE | LSR_TXRDY) && --timo != 0)
		;
}

/*
 * Read the serial port specified and try to figure out what speed
 * it's currently running at.  We're assuming the serial port has
 * been initialized and is basicly idle.  This routine is only intended
 * to be run at system startup.
 *
 * If the value read from the serial port doesn't make sense, return 0.
 */

static speed_t
siocngetspeed(iobase, table)
	Port_t iobase;
	struct speedtab *table;
{
	int	code;
	u_char	dlbh;
	u_char	dlbl;
	u_char  cfcr;

	cfcr = inb(iobase + com_cfcr);
	outb(iobase + com_cfcr, CFCR_DLAB | cfcr);

	dlbl = inb(iobase + com_dlbl);
	dlbh = inb(iobase + com_dlbh);

	outb(iobase + com_cfcr, cfcr);

	code = dlbh << 8 | dlbl;

	for ( ; table->sp_speed != -1; table++)
		if (table->sp_code == code)
			return (table->sp_speed);

	return 0;	/* didn't match anything sane */
}

static void
siocnopen(sp, iobase, speed)
	struct siocnstate	*sp;
	Port_t			iobase;
	int			speed;
{
	int	divisor;
	u_char	dlbh;
	u_char	dlbl;

	/*
	 * Save all the device control registers except the fifo register
	 * and set our default ones (cs8 -parenb speed=comdefaultrate).
	 * We can't save the fifo register since it is read-only.
	 */
	sp->ier = inb(iobase + com_ier);
	outb(iobase + com_ier, 0);	/* spltty() doesn't stop siointr() */
	siocntxwait(iobase);
	sp->cfcr = inb(iobase + com_cfcr);
	outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
	sp->dlbl = inb(iobase + com_dlbl);
	sp->dlbh = inb(iobase + com_dlbh);
	/*
	 * Only set the divisor registers if they would change, since on
	 * some 16550 incompatibles (Startech), setting them clears the
	 * data input register.  This also reduces the effects of the
	 * UMC8669F bug.
	 */
	divisor = ttspeedtab(speed, comspeedtab);
	dlbl = divisor & 0xFF;
	if (sp->dlbl != dlbl)
		outb(iobase + com_dlbl, dlbl);
	dlbh = (u_int) divisor >> 8;
	if (sp->dlbh != dlbh)
		outb(iobase + com_dlbh, dlbh);
	outb(iobase + com_cfcr, CFCR_8BITS);
	sp->mcr = inb(iobase + com_mcr);
	/*
	 * We don't want interrupts, but must be careful not to "disable"
	 * them by clearing the MCR_IENABLE bit, since that might cause
	 * an interrupt by floating the IRQ line.
	 */
	outb(iobase + com_mcr, (sp->mcr & MCR_IENABLE) | MCR_DTR | MCR_RTS);
}

static void
siocnclose(sp, iobase)
	struct siocnstate	*sp;
	Port_t			iobase;
{
	/*
	 * Restore the device control registers.
	 */
	siocntxwait(iobase);
	outb(iobase + com_cfcr, CFCR_DLAB | CFCR_8BITS);
	if (sp->dlbl != inb(iobase + com_dlbl))
		outb(iobase + com_dlbl, sp->dlbl);
	if (sp->dlbh != inb(iobase + com_dlbh))
		outb(iobase + com_dlbh, sp->dlbh);
	outb(iobase + com_cfcr, sp->cfcr);
	/*
	 * XXX damp oscillations of MCR_DTR and MCR_RTS by not restoring them.
	 */
	outb(iobase + com_mcr, sp->mcr | MCR_DTR | MCR_RTS);
	outb(iobase + com_ier, sp->ier);
}

#ifdef __i386__
static
#endif
void
siocnprobe(cp)
	struct consdev	*cp;
{
	speed_t			boot_speed;
	u_char			cfcr;
	int			s, unit;
	struct siocnstate	sp;

	/*
	 * Find our first enabled console, if any.  If it is a high-level
	 * console device, then initialize it and return successfully.
	 * If it is a low-level console device, then initialize it and
	 * return unsuccessfully.  It must be initialized in both cases
	 * for early use by console drivers and debuggers.  Initializing
	 * the hardware is not necessary in all cases, since the i/o
	 * routines initialize it on the fly, but it is necessary if
	 * input might arrive while the hardware is switched back to an
	 * uninitialized state.  We can't handle multiple console devices
	 * yet because our low-level routines don't take a device arg.
	 * We trust the user to set the console flags properly so that we
	 * don't need to probe.
	 */
	cp->cn_pri = CN_DEAD;

	for (unit = 0; unit < 16; unit++) { /* XXX need to know how many */
		int flags;
		if (resource_int_value("sio", unit, "flags", &flags))
			continue;
		if (COM_CONSOLE(flags)) {
			int port;
			if (resource_int_value("sio", unit, "port", &port))
				continue;
			siocniobase = port;
			s = spltty();
			if (boothowto & RB_SERIAL) {
				boot_speed = siocngetspeed(siocniobase,
							   comspeedtab);
				if (boot_speed)
					comdefaultrate = boot_speed;
			}

			/*
			 * Initialize the divisor latch.  We can't rely on
			 * siocnopen() to do this the first time, since it 
			 * avoids writing to the latch if the latch appears
			 * to have the correct value.  Also, if we didn't
			 * just read the speed from the hardware, then we
			 * need to set the speed in hardware so that
			 * switching it later is null.
			 */
			cfcr = inb(siocniobase + com_cfcr);
			outb(siocniobase + com_cfcr, CFCR_DLAB | cfcr);
			outb(siocniobase + com_dlbl,
			     COMBRD(comdefaultrate) & 0xff);
			outb(siocniobase + com_dlbh,
			     (u_int) COMBRD(comdefaultrate) >> 8);
			outb(siocniobase + com_cfcr, cfcr);

			siocnopen(&sp, siocniobase, comdefaultrate);
			splx(s);
			if (!COM_LLCONSOLE(flags)) {
				cp->cn_dev = makedev(CDEV_MAJOR, unit);
				cp->cn_pri = COM_FORCECONSOLE(flags)
					     || boothowto & RB_SERIAL
					     ? CN_REMOTE : CN_NORMAL;
			}
			break;
		}
	}
}

#ifdef __alpha__

struct consdev siocons = {
	NULL, NULL, siocngetc, siocncheckc, siocnputc,
	NULL, makedev(CDEV_MAJOR, 0), CN_NORMAL,
};

extern struct consdev *cn_tab;

int
siocnattach(port, speed)
	int port;
	int speed;
{
	int			s;
	u_char			cfcr;
	struct siocnstate	sp;

	siocniobase = port;
	comdefaultrate = speed;

	s = spltty();

	/*
	 * Initialize the divisor latch.  We can't rely on
	 * siocnopen() to do this the first time, since it 
	 * avoids writing to the latch if the latch appears
	 * to have the correct value.  Also, if we didn't
	 * just read the speed from the hardware, then we
	 * need to set the speed in hardware so that
	 * switching it later is null.
	 */
	cfcr = inb(siocniobase + com_cfcr);
	outb(siocniobase + com_cfcr, CFCR_DLAB | cfcr);
	outb(siocniobase + com_dlbl,
	     COMBRD(comdefaultrate) & 0xff);
	outb(siocniobase + com_dlbh,
	     (u_int) COMBRD(comdefaultrate) >> 8);
	outb(siocniobase + com_cfcr, cfcr);

	siocnopen(&sp, siocniobase, comdefaultrate);
	splx(s);

	cn_tab = &siocons;
	return 0;
}

int
siogdbattach(port, speed)
	int port;
	int speed;
{
	int			s;
	u_char			cfcr;
	struct siocnstate	sp;

	siogdbiobase = port;
	gdbdefaultrate = speed;

	s = spltty();

	/*
	 * Initialize the divisor latch.  We can't rely on
	 * siocnopen() to do this the first time, since it 
	 * avoids writing to the latch if the latch appears
	 * to have the correct value.  Also, if we didn't
	 * just read the speed from the hardware, then we
	 * need to set the speed in hardware so that
	 * switching it later is null.
	 */
	cfcr = inb(siogdbiobase + com_cfcr);
	outb(siogdbiobase + com_cfcr, CFCR_DLAB | cfcr);
	outb(siogdbiobase + com_dlbl,
	     COMBRD(gdbdefaultrate) & 0xff);
	outb(siogdbiobase + com_dlbh,
	     (u_int) COMBRD(gdbdefaultrate) >> 8);
	outb(siogdbiobase + com_cfcr, cfcr);

	siocnopen(&sp, siogdbiobase, gdbdefaultrate);
	splx(s);

	return 0;
}

#endif

#ifdef __i386__
static
#endif
void
siocninit(cp)
	struct consdev	*cp;
{
	comconsole = DEV_TO_UNIT(cp->cn_dev);
}

#ifdef __i386__
static
#endif
int
siocncheckc(dev)
	dev_t	dev;
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siocniobase;
	s = spltty();
	siocnopen(&sp, iobase, comdefaultrate);
	if (inb(iobase + com_lsr) & LSR_RXRDY)
		c = inb(iobase + com_data);
	else
		c = -1;
	siocnclose(&sp, iobase);
	splx(s);
	return (c);
}


int
siocngetc(dev)
	dev_t	dev;
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siocniobase;
	s = spltty();
	siocnopen(&sp, iobase, comdefaultrate);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp, iobase);
	splx(s);
	return (c);
}

void
siocnputc(dev, c)
	dev_t	dev;
	int	c;
{
	int	s;
	struct siocnstate	sp;

	s = spltty();
	siocnopen(&sp, siocniobase, comdefaultrate);
	siocntxwait(siocniobase);
	outb(siocniobase + com_data, c);
	siocnclose(&sp, siocniobase);
	splx(s);
}

#ifdef __alpha__
int
siogdbgetc()
{
	int	c;
	Port_t	iobase;
	int	s;
	struct siocnstate	sp;

	iobase = siogdbiobase;
	s = spltty();
	siocnopen(&sp, iobase, gdbdefaultrate);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp, iobase);
	splx(s);
	return (c);
}

void
siogdbputc(c)
	int	c;
{
	int	s;
	struct siocnstate	sp;

	s = spltty();
	siocnopen(&sp, siogdbiobase, gdbdefaultrate);
	siocntxwait(siogdbiobase);
	outb(siogdbiobase + com_data, c);
	siocnclose(&sp, siogdbiobase);
	splx(s);
}
#endif


/*
 * support PnP cards if we are using 'em
 */

#if NPNP > 0

static pnpid_t siopnp_ids[] = {
	{ 0x5015f435, "MOT1550"},
	{ 0x8113b04e, "Supra1381"},
	{ 0x9012b04e, "Supra1290"},
	{ 0x7121b04e, "SupraExpress 56i Sp"},
	{ 0x11007256, "USR0011"},
	{ 0x30207256, "USR2030"},
	{ 0x31307256, "USR3031"},
	{ 0x90307256, "USR3090"},
	{ 0x0100440e, "Cardinal MVP288IV"},
	{ 0 }
};

static char *siopnp_probe(u_long csn, u_long vend_id);
static void siopnp_attach(u_long csn, u_long vend_id, char *name,
	struct isa_device *dev);
static u_long nsiopnp = NSIO;

static struct pnp_device siopnp = {
	"siopnp",
	siopnp_probe,
	siopnp_attach,
	&nsiopnp,
	&tty_imask
};
DATA_SET (pnpdevice_set, siopnp);

static char *
siopnp_probe(u_long csn, u_long vend_id)
{
	pnpid_t *id;
	char *s = NULL;

	for(id = siopnp_ids; id->vend_id != 0; id++) {
		if (vend_id == id->vend_id) {
			s = id->id_str;
			break;
		}
	}

	if (s) {
		struct pnp_cinfo d;
		read_pnp_parms(&d, 0);
		if (d.enable == 0 || d.flags & 1) {
			printf("CSN %lu is disabled.\n", csn);
			return (NULL);
		}

	}

	return (s);
}

static void
siopnp_attach(u_long csn, u_long vend_id, char *name, struct isa_device *dev)
{
	struct pnp_cinfo d;

	if (dev->id_unit >= NSIOTOT)
		return;

	if (read_pnp_parms(&d, 0) == 0) {
		printf("failed to read pnp parms\n");
		return;
	}

	write_pnp_parms(&d, 0);

	enable_pnp_card();

	dev->id_iobase = d.port[0];
	dev->id_irq = (1 << d.irq[0]);
	dev->id_ointr = siointr;
	dev->id_ri_flags = RI_FAST;
	dev->id_drq = -1;

	if (dev->id_driver == NULL) {
		dev->id_driver = &siodriver;
		dev->id_id = isa_compat_nextid();
	}

	if ((dev->id_alive = sioprobe(dev)) != 0)
		sioattach(dev);
	else
		printf("sio%d: probe failed\n", dev->id_unit);
}
#endif

CDEV_DRIVER_MODULE(sio, isa, sio_driver, sio_devclass,
		   CDEV_MAJOR, sio_cdevsw, 0, 0);
