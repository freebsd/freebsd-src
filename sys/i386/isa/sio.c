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
 *	from: @(#)com.c	7.5 (Berkeley) 5/16/91
 *	$Id: sio.c,v 1.56 1994/06/16 08:08:44 ache Exp $
 */

#include "sio.h"
#if NSIO > 0
/*
 * Serial driver, based on 386BSD-0.1 com driver.
 * Mostly rewritten to use pseudo-DMA.
 * Works for National Semiconductor NS8250-NS16550AF UARTs.
 * COM driver, based on HP dca driver.
 */
#include "param.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "proc.h"
#include "user.h"
#include "conf.h"
#include "dkstat.h"
#include "file.h"
#include "uio.h"
#include "kernel.h"
#include "malloc.h"
#include "syslog.h"

#include "i386/isa/icu.h"	/* XXX */
#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/comreg.h"
#include "i386/isa/ic/ns16550.h"

#define	LOTS_OF_EVENTS	64	/* helps separate urgent events from input */
#define	RB_I_HIGH_WATER	(RBSZ - 2 * RS_IBUFSIZE)
#define	RB_I_LOW_WATER	((RBSZ - 2 * RS_IBUFSIZE) * 7 / 8)
#define	RS_IBUFSIZE	256
#define	TTY_BI		TTY_FE		/* XXX */
#define	TTY_OE		TTY_PE		/* XXX */

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
#define	COM_ISMULTIPORT(dev)	((dev)->id_flags & 0x01)
#define	COM_MPMASTER(dev)	(((dev)->id_flags >> 8) & 0x0ff)
#define	COM_NOTAST4(dev)	((dev)->id_flags & 0x04)
#endif /* COM_MULTIPORT */

#define	COM_NOFIFO(dev)	((dev)->id_flags & 0x02)
#define	COM_VERBOSE(dev) ((dev)->id_flags & 0x80)

#define	com_scr		7	/* scratch register for 16450-16550 (R/W) */

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
 *	TS_BUSY		= CS_BUSY (maintained by comstart() and comflush())
 *	CS_TTGO		= ~TS_TTSTOP (maintained by comstart() and siostop())
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
typedef u_int	Port_t;		/* hardware port */
typedef u_char	bool_t;		/* boolean */

/* com device structure */
struct com_s {
	u_char	state;		/* miscellaneous flag bits */
	bool_t  active_out;	/* nonzero if the callout device is open */
	u_char	cfcr_image;	/* copy of value written to CFCR */
	u_char	ftl;		/* current rx fifo trigger level */
	u_char	ftl_init;	/* ftl_max for next open() */
	u_char	ftl_max;	/* maximum ftl for curent open() */
	bool_t	hasfifo;	/* nonzero for 16550 UARTs */
	u_char	mcr_image;	/* copy of value written to MCR */
#ifdef COM_MULTIPORT
	bool_t	multiport;	/* is this unit part of a multiport device? */
#endif /* COM_MULTIPORT */
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
	u_char	*ihighwater;	/* threshold in input buffer */
	u_char	*iptr;		/* next free spot in input buffer */

	u_char	*obufend;	/* end of output buffer */
	int	ocount;		/* original count for current output */
	u_char	*optr;		/* next char to output */

	Port_t	data_port;	/* i/o ports */
	Port_t	int_id_port;
	Port_t	iobase;
	Port_t	modem_ctl_port;
	Port_t	line_status_port;
	Port_t	modem_status_port;

	struct tty	*tp;	/* cross reference */

	/* Initial state. */
	struct termios	it_in;	/* should be in struct tty */
	struct termios	it_out;

	/* Lock state. */
	struct termios	lt_in;	/* should be in struct tty */
	struct termios	lt_out;

#ifdef TIOCTIMESTAMP
	bool_t	do_timestamp;
	struct timeval	timestamp;
#endif

	u_long	bytes_in;	/* statistics */
	u_long	bytes_out;
	u_int	delta_error_counts[CE_NTYPES];
	u_int	error_counts[CE_NTYPES];

	/*
	 * Ping-pong input buffers.  The extra factor of 2 in the sizes is
	 * to allow for an error byte for each input byte.
	 */
#define	CE_INPUT_OFFSET		RS_IBUFSIZE
	u_char	ibuf1[2 * RS_IBUFSIZE];
	u_char	ibuf2[2 * RS_IBUFSIZE];
};

/*
 * The public functions in the com module ought to be declared in a com-driver
 * system header.
 */
#define	Dev_t	int		/* promoted dev_t */

/* Interrupt handling entry points. */
void	siointr		__P((int unit));
void	siopoll		__P((void));

/* Device switch entry points. */
int	sioopen		__P((Dev_t dev, int oflags, int devtype,
			     struct proc *p));
int	sioclose	__P((Dev_t dev, int fflag, int devtype,
			     struct proc *p));
int	sioread		__P((Dev_t dev, struct uio *uio, int ioflag));
int	siowrite	__P((Dev_t dev, struct uio *uio, int ioflag));
int	sioioctl	__P((Dev_t dev, int cmd, caddr_t data,
			     int fflag, struct proc *p));
void	siostop		__P((struct tty *tp, int rw));
#define	sioreset	noreset
int	sioselect	__P((Dev_t dev, int rw, struct proc *p));
#define	siommap		nommap
#define	siostrategy	nostrategy

/* Console device entry points. */
int	siocngetc	__P((Dev_t dev));
struct consdev;
void	siocninit	__P((struct consdev *cp));
void	siocnprobe	__P((struct consdev *cp));
void	siocnputc	__P((Dev_t dev, int c));

static	int	sioattach	__P((struct isa_device *dev));
static	void	siodtrwakeup	__P((caddr_t chan, int ticks));
static	void	comflush	__P((struct com_s *com));
static	void	comhardclose	__P((struct com_s *com));
static	void	siointr1	__P((struct com_s *com));
static	void	commctl		__P((struct com_s *com, int bits, int how));
static	int	comparam	__P((struct tty *tp, struct termios *t));
static	int	sioprobe	__P((struct isa_device *dev));
static	void	comstart	__P((struct tty *tp));
static	void	comwakeup	__P((caddr_t chan, int ticks));
static	int	tiocm_xxx2mcr	__P((int tiocm_xxx));

/* table and macro for fast conversion from a unit number to its com struct */
static	struct com_s	*p_com_addr[NSIO];
#define	com_addr(unit)	(p_com_addr[unit])

#ifdef TIOCTIMESTAMP
static  struct timeval	intr_timestamp;
#endif

struct isa_driver	siodriver = {
	sioprobe, sioattach, "sio"
};

#ifdef COMCONSOLE
static	int	comconsole = COMCONSOLE;
#else
static	int	comconsole = -1;
#endif
static	speed_t	comdefaultrate = TTYDEF_SPEED;
static	u_int	com_events;	/* input chars + weighted output completions */
static	int	commajor;
struct tty	*sio_tty[NSIO];
extern	struct tty	*constty;	/* XXX */

#ifdef KGDB
#include "machine/remote-sl.h"

extern	int	kgdb_dev;
extern	int	kgdb_rate;
extern	int	kgdb_debug_init;
#endif

static	struct speedtab comspeedtab[] = {
	0,	0,
	50,	COMBRD(50),
	75,	COMBRD(75),
	110,	COMBRD(110),
	134,	COMBRD(134),
	150,	COMBRD(150),
	200,	COMBRD(200),
	300,	COMBRD(300),
	600,	COMBRD(600),
	1200,	COMBRD(1200),
	1800,	COMBRD(1800),
	2400,	COMBRD(2400),
	4800,	COMBRD(4800),
	9600,	COMBRD(9600),
	19200,	COMBRD(19200),
	38400,	COMBRD(38400),
	57600,	COMBRD(57600),
	115200,	COMBRD(115200),
	-1,	-1
};

/* XXX - configure this list */
static Port_t likely_com_ports[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8, };

static int
sioprobe(dev)
	struct isa_device	*dev;
{
	static bool_t	already_init;
	Port_t		*com_ptr;
	bool_t		failures[10];
	int		fn;
	struct isa_device	*idev;
	Port_t		iobase;
	u_char		mcr_image;
	int		result;

	if (!already_init) {
		/*
		 * Turn off MCR_IENABLE for all likely serial ports.  An unused
		 * port with its MCR_IENABLE gate open will inhibit interrupts
		 * from any used port that shares the interrupt vector.
		 * XXX the gate enable is elsewhere for some multiports.
		 */
		for (com_ptr = likely_com_ports;
		     com_ptr < &likely_com_ports[sizeof likely_com_ports
						 / sizeof likely_com_ports[0]];
		     ++com_ptr)
			outb(*com_ptr + com_mcr, 0);
		already_init = TRUE;
	}

	/*
	 * If the port is on a multiport card and has a master port,
	 * initialize the common interrupt control register in the
	 * master and prepare to leave MCR_IENABLE clear in the mcr.
	 * Otherwise, prepare to set MCR_IENABLE in the mcr.
	 * Point idev to the device struct giving the correct id_irq.
	 * This is the struct for the master device if there is one.
	 */
	idev = dev;
	mcr_image = MCR_IENABLE;
#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(dev)) {
		idev = find_isadev(isa_devtab_tty, &siodriver,
				   COM_MPMASTER(dev));
		if (idev == NULL) {
			printf("sio%d: master device %d not found\n",
			       dev->id_unit, COM_MPMASTER(dev));
			return (0);
		}
		if (idev->id_irq == 0) {
			printf("sio%d: master device %d irq not configured\n",
			       dev->id_unit, COM_MPMASTER(dev));
			return (0);
		}
		if (!COM_NOTAST4(dev)) {
			outb(idev->id_iobase + com_scr,	0x80);
			mcr_image = 0;
		}
	}
	else
#endif /* COM_MULTIPORT */
	if (idev->id_irq == 0) {
		printf("sio%d: irq not configured\n", dev->id_unit);
		return (0);
	}

	bzero(failures, sizeof failures);
	iobase = dev->id_iobase;

	/*
	 * We don't want to get actual interrupts, just masked ones.
	 * Interrupts from this line should already be masked in the ICU,
	 * but mask them in the processor as well in case there are some
	 * (misconfigured) shared interrupts.
	 */
	disable_intr();

	/*
	 * XXX DELAY() reenables CPU interrupts.  This is a problem for
	 * shared interrupts after the first device using one has been
	 * successfully probed - config_isadev() has enabled the interrupt
	 * in the ICU.
	 */
	outb(IO_ICU1 + 1, 0xff);

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
	outb(iobase + com_cfcr, CFCR_DLAB);
	outb(iobase + com_dlbl, COMBRD(9600) & 0xff);
	outb(iobase + com_dlbh, (u_int) COMBRD(9600) >> 8);
	outb(iobase + com_cfcr, CFCR_8BITS);
	DELAY((16 + 1) * 9600 / 10);

	/*
	 * Enable the interrupt gate and disable device interupts.  This
	 * should leave the device driving the interrupt line low and
	 * guarantee an edge trigger if an interrupt can be generated.
	 */
	outb(iobase + com_mcr, mcr_image);
	outb(iobase + com_ier, 0);

	/*
	 * Attempt to set loopback mode so that we can send a null byte
	 * without annoying any external device.
	 */
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
	DELAY((2 + 1) * 9600 / 10);

	/*
	 * Turn off loopback mode so that the interrupt gate works again
	 * (MCR_IENABLE was hidden).  This should leave the device driving
	 * an interrupt line high.  It doesn't matter if the interrupt
	 * line oscillates while we are not looking at it, since interrupts
	 * are disabled.
	 */
	outb(iobase + com_mcr, mcr_image);

	/*
	 * Check that
	 *	o the CFCR, IER and MCR in UART hold the values written to them
	 *	  (the values happen to be all distinct - this is good for
	 *	  avoiding false positive tests from bus echoes).
	 *	o an output interrupt is generated and its vector is correct.
	 *	o the interrupt goes away when the IIR in the UART is read.
	 */
	failures[0] = inb(iobase + com_cfcr) - CFCR_8BITS;
	failures[1] = inb(iobase + com_ier) - IER_ETXRDY;
	failures[2] = inb(iobase + com_mcr) - mcr_image;
	if (idev->id_irq != 0)
		failures[3] = isa_irq_pending(idev) ? 0 : 1;
	failures[4] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_TXRDY;
	failures[5] = isa_irq_pending(idev) ? 1	: 0;
	failures[6] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	/*
	 * Turn off all device interrupts and check that they go off properly.
	 * Leave MCR_IENABLE alone.  For ports without a master port, it gates
	 * the OUT2 output of the UART to
	 * the ICU input.  Closing the gate would give a floating ICU input
	 * (unless there is another device driving at) and spurious interrupts.
	 * (On the system that this was first tested on, the input floats high
	 * and gives a (masked) interrupt as soon as the gate is closed.)
	 */
	outb(iobase + com_ier, 0);
	outb(iobase + com_cfcr, CFCR_8BITS);	/* dummy to avoid bus echo */
	failures[7] = inb(iobase + com_ier);
	failures[8] = isa_irq_pending(idev) ? 1	: 0;
	failures[9] = (inb(iobase + com_iir) & IIR_IMASK) - IIR_NOPEND;

	outb(IO_ICU1 + 1, imen);	/* XXX */
	enable_intr();

	result = IO_COMSIZE;
	for (fn = 0; fn < sizeof failures; ++fn)
		if (failures[fn]) {
			outb(iobase + com_mcr, 0);
			result = 0;
			if (COM_VERBOSE(dev))
				printf("sio%d: probe test %d failed\n",
				       dev->id_unit, fn);
		}
	return (result);
}

static int
sioattach(isdp)
	struct isa_device	*isdp;
{
	struct com_s	*com;
	static bool_t	comwakeup_started = FALSE;
	Port_t		iobase;
	int		s;
	int		unit;

	iobase = isdp->id_iobase;
	unit = isdp->id_unit;
	com = malloc(sizeof *com, M_TTYS, M_NOWAIT);
	if (com == NULL)
		return (0);

	/*
	 * sioprobe() has initialized the device registers as follows:
	 *	o cfcr = CFCR_8BITS.
	 *	  It is most important that CFCR_DLAB is off, so that the
	 *	  data port is not hidden when we enable interrupts.
	 *	o ier = 0.
	 *	  Interrupts are only enabled when the line is open.
	 *	o mcr = MCR_IENABLE, or 0 if the port has a master port.
	 *	  Keeping MCR_DTR and MCR_RTS off might stop the external
	 *	  device from sending before we are ready.
	 */
	bzero(com, sizeof *com);
	com->cfcr_image = CFCR_8BITS;
	com->dtr_wait = 3 * hz;
	com->tx_fifo_size = 1;
	com->iptr = com->ibuf = com->ibuf1;
	com->ibufend = com->ibuf1 + RS_IBUFSIZE;
	com->ihighwater = com->ibuf1 + RS_IHIGHWATER;
	com->iobase = iobase;
	com->data_port = iobase + com_data;
	com->int_id_port = iobase + com_iir;
	com->modem_ctl_port = iobase + com_mcr;
	com->mcr_image = inb(com->modem_ctl_port);
	com->line_status_port = iobase + com_lsr;
	com->modem_status_port = iobase + com_msr;

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

	/* attempt to determine UART type */
	printf("sio%d: type", unit);
#ifdef COM_MULTIPORT
	if (!COM_ISMULTIPORT(isdp))
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
	outb(iobase + com_fifo, FIFO_ENABLE | FIFO_TRIGGER_14);
	DELAY(100);
	switch (inb(com->int_id_port) & IIR_FIFO_MASK) {
	case FIFO_TRIGGER_1:
		printf(" 16450");
		break;
	case FIFO_TRIGGER_4:
		printf(" 16450?");
		break;
	case FIFO_TRIGGER_8:
		printf(" 16550?");
		break;
	case FIFO_TRIGGER_14:
		printf(" 16550A");
		if (COM_NOFIFO(isdp))
			printf(" fifo disabled");
		else {
			com->hasfifo = TRUE;
			com->ftl_init = FIFO_TRIGGER_14;
			com->tx_fifo_size = 16;
		}
		break;
	}
	outb(iobase + com_fifo, 0);
determined_type: ;

#ifdef COM_MULTIPORT
	if (COM_ISMULTIPORT(isdp)) {
		com->multiport = TRUE;
		printf(" (multiport");
		if (unit == COM_MPMASTER(isdp))
			printf(" master");
		printf(")");
	 }
#endif /* COM_MULTIPORT */
	printf("\n");

#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (unit == comconsole)
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			int divisor;

			/*
			 * XXX now unfinished and broken.  Need to do
			 * something more like a full open().  There's no
			 * suitable interrupt handler so don't enable device
			 * interrupts.  Watch out for null tp's.
			 */
			outb(iobase + com_cfcr, CFCR_DLAB);
			divisor = ttspeedtab(kgdb_rate, comspeedtab);
			outb(iobase + com_dlbl, divisor & 0xFF);
			outb(iobase + com_dlbh, (u_int) divisor >> 8);
			outb(iobase + com_cfcr, CFCR_8BITS);
			outb(com->modem_status_port,
			     com->mcr_image |= MCR_DTR | MCR_RTS);

			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("sio%d: ", unit);
				kgdb_connect(1);
			} else
				printf("sio%d: kgdb enabled\n", unit);
		}
	}
#endif

	s = spltty();
	com_addr(unit) = com;
	splx(s);
	if (!comwakeup_started) {
		comwakeup((caddr_t)NULL, 0);
		comwakeup_started = TRUE;
	}
	return (1);
}

/* ARGSUSED */
int
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
	if ((u_int) unit >= NSIO || (com = com_addr(unit)) == NULL)
		return (ENXIO);
	if (mynor & CONTROL_MASK)
		return (0);
	tp = com->tp = sio_tty[unit] = ttymalloc(sio_tty[unit]);
	s = spltty();
	/*
	 * We jump to this label after all non-interrupted sleeps to pick
	 * up any changes of the device state.
	 */
open_top:
	while (com->state & CS_DTR_OFF) {
		error = tsleep((caddr_t)&com->dtr_wait, TTIPRI | PCATCH,
			       "siodtr", 0);
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
				error =	tsleep((caddr_t)&com->active_out,
					       TTIPRI | PCATCH, "siobi", 0);
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
		commctl(com, MCR_DTR | MCR_RTS, DMSET);
		com->ftl_max = com->ftl_init;
		++com->wopeners;
		error = comparam(tp, &tp->t_termios);
		--com->wopeners;
		if (error != 0)
			goto out;
		/*
		 * XXX we should goto open_top if comparam() slept.
		 */
		ttsetwater(tp);
		iobase = com->iobase;
		if (com->hasfifo) {
			/* Drain fifo. */
			outb(iobase + com_fifo,
			     FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST
			     | com->ftl);
			DELAY(100);
		}
		disable_intr();
		(void) inb(com->line_status_port);
		(void) inb(com->data_port);
			com->prev_modem_status =
			com->last_modem_status = inb(com->modem_status_port);
		outb(iobase + com_ier, IER_ERXRDY | IER_ETXRDY | IER_ERLS
				       | IER_EMSC);
		enable_intr();
		/*
		 * Handle initial DCD.  Callout devices get a fake initial
		 * DCD (trapdoor DCD).  If we are callout, then any sleeping
		 * callin opens get woken up and resume sleeping on "siobi"
		 * instead of "siodcd".
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
		--com->wopeners;
		if (error != 0)
			goto out;
		goto open_top;
	}
	error =	(*linesw[tp->t_line].l_open)(dev, tp, 0);
	if (tp->t_state & TS_ISOPEN && mynor & CALLOUT_MASK)
		com->active_out = TRUE;
out:
	splx(s);
	if (!(tp->t_state & TS_ISOPEN) && com->wopeners == 0)
		comhardclose(com);
	return (error);
}

/*ARGSUSED*/
int
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
	siostop(tp, FREAD | FWRITE);
	comhardclose(com);
	ttyclose(tp);
	splx(s);
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

	unit = DEV_TO_UNIT(com->tp->t_dev);
	iobase = com->iobase;
	s = spltty();
#ifdef TIOCTIMESTAMP
	com->do_timestamp = 0;
#endif
	outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
#ifdef KGDB
	/* do not disable interrupts or hang up if debugging */
	if (kgdb_dev != makedev(commajor, unit))
#endif
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
			commctl(com, MCR_RTS, DMSET);
			if (com->dtr_wait != 0) {
				timeout(siodtrwakeup, (caddr_t)com,
					com->dtr_wait);
				com->state |= CS_DTR_OFF;
			}
		}
	}
	com->active_out = FALSE;
	wakeup((caddr_t)&com->active_out);
	wakeup(TSA_CARR_ON(tp));	/* restart any wopeners */
	splx(s);
}

int
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

int
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
	if (constty && unit == comconsole)
		constty = NULL;
	return ((*linesw[tp->t_line].l_write)(tp, uio, flag));
}

static void
siodtrwakeup(chan, ticks)
	caddr_t	chan;
	int	ticks;
{
	struct com_s	*com;

	com = (struct com_s *)chan;
	com->state &= ~CS_DTR_OFF;
	wakeup((caddr_t)&com->dtr_wait);
}

#ifdef TIOCTIMESTAMP
/* Interrupt routine for timekeeping purposes */
void
siointrts(unit)
	int unit;
{
	microtime(&intr_timestamp);
	siointr(unit);
}
#endif

void
siointr(unit)
	int	unit;
{
#ifndef COM_MULTIPORT
	siointr1(com_addr(unit));
#else /* COM_MULTIPORT */
	struct com_s    *com;
	bool_t		possibly_more_intrs;

	/*
	 * Loop until there is no activity on any port.  This is necessary
	 * to get an interrupt edge more than to avoid another interrupt.
	 * If the IRQ signal is just an OR of the IRQ signals from several
	 * devices, then the edge from one may be lost because another is
	 * on.
	 */
	do {
		possibly_more_intrs = FALSE;
		for (unit = 0; unit < NSIO; ++unit) {
			com = com_addr(unit);
			if (com != NULL
			    && (inb(com->int_id_port) & IIR_IMASK)
			       != IIR_NOPEND) {
				siointr1(com);
				possibly_more_intrs = TRUE;
			}
		}
	} while (possibly_more_intrs);
#endif /* COM_MULTIPORT */
}

static void
siointr1(com)
	struct com_s *com;
{
	u_char	line_status;
	u_char	modem_status;
	u_char	*ioptr;
	u_char	recv_data;

#ifdef TIOCTIMESTAMP
	if (com->do_timestamp)
		/* XXX a little bloat here... */
		com->timestamp = intr_timestamp;
#endif
	while (TRUE) {
		line_status = inb(com->line_status_port);

		/* input event? (check first to help avoid overruns) */
		while (line_status & LSR_RCV_MASK) {
			/* break/unnattached error bits or real input? */
			if (!(line_status & LSR_RXRDY))
				recv_data = 0;
			else
				recv_data = inb(com->data_port);
			++com->bytes_in;
			if (com->hotchar != 0 && recv_data == com->hotchar)
				setsofttty();
#ifdef KGDB
			/* trap into kgdb? (XXX - needs testing and optim) */
			if (recv_data == FRAME_END
			    && !(com->tp->t_state & TS_ISOPEN)
			    && kgdb_dev == makedev(commajor, unit)) {
				kgdb_connect(0);
				continue;
			}
#endif /* KGDB */
			ioptr = com->iptr;
			if (ioptr >= com->ibufend)
				CE_RECORD(com, CE_INTERRUPT_BUF_OVERFLOW);
			else {
				++com_events;
#if 0 /* for testing input latency vs efficiency */
if (com->iptr - com->ibuf == 8)
	setsofttty();
#endif
				ioptr[0] = recv_data;
				ioptr[CE_INPUT_OFFSET] = line_status;
				com->iptr = ++ioptr;
				if (ioptr == com->ihighwater
				    && com->state & CS_RTS_IFLOW)
					outb(com->modem_ctl_port,
					     com->mcr_image &= ~MCR_RTS);
				/* XXX - move this out of isr */
				if (line_status & LSR_OE)
					CE_RECORD(com, CE_OVERRUN);
			}

			/*
			 * "& 0x7F" is to avoid the gcc-1.40 generating a slow
			 * jump from the top of the loop to here
			 */
			line_status = inb(com->line_status_port) & 0x7F;
		}

		/* modem status change? (always check before doing output) */
		modem_status = inb(com->modem_status_port);
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
		    && com->state >= (CS_ODEVREADY | CS_BUSY | CS_TTGO)) {
			ioptr = com->optr;
			if (com->tx_fifo_size > 1) {
				u_int	ocount;

				ocount = com->obufend - ioptr;
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
			com->optr = ioptr;
			if (ioptr >= com->obufend) {
				/* output just completed */
				com_events += LOTS_OF_EVENTS;
				com->state ^= (CS_ODONE | CS_BUSY);
				setsofttty();	/* handle at high level ASAP */
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
tiocm_xxx2mcr(tiocm_xxx)
	int	tiocm_xxx;
{
	int	mcr;

	mcr = 0;
	if (tiocm_xxx & TIOCM_DTR)
		mcr |= MCR_DTR;
	if (tiocm_xxx & TIOCM_RTS)
		mcr |= MCR_RTS;
	return (mcr);
}

int
sioioctl(dev, cmd, data, flag, p)
	dev_t		dev;
	int		cmd;
	caddr_t		data;
	int		flag;
	struct proc	*p;
{
	struct com_s	*com;
	int		error;
	Port_t		iobase;
	int		mcr;
	int		msr;
	int		mynor;
	int		s;
	int		tiocm_xxx;
	struct tty	*tp;

	mynor = minor(dev);
	com = com_addr(MINOR_TO_UNIT(mynor));
	if (mynor & CONTROL_MASK) {
		struct termios *ct;

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
			if (error)
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
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int cc;
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
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	iobase = com->iobase;
	s = spltty();
	switch (cmd) {
	case TIOCSBRK:
		outb(iobase + com_cfcr, com->cfcr_image |= CFCR_SBREAK);
		break;
	case TIOCCBRK:
		outb(iobase + com_cfcr, com->cfcr_image &= ~CFCR_SBREAK);
		break;
	case TIOCSDTR:
		commctl(com, MCR_DTR, DMBIS);
		break;
	case TIOCCDTR:
		commctl(com, MCR_DTR, DMBIC);
		break;
	case TIOCMSET:
		commctl(com, tiocm_xxx2mcr(*(int *)data), DMSET);
		break;
	case TIOCMBIS:
		commctl(com, tiocm_xxx2mcr(*(int *)data), DMBIS);
		break;
	case TIOCMBIC:
		commctl(com, tiocm_xxx2mcr(*(int *)data), DMBIC);
		break;
	case TIOCMGET:
		tiocm_xxx = TIOCM_LE;	/* XXX - always enabled while open */
		mcr = com->mcr_image;
		if (mcr & MCR_DTR)
			tiocm_xxx |= TIOCM_DTR;
		if (mcr & MCR_RTS)
			tiocm_xxx |= TIOCM_RTS;
		msr = com->prev_modem_status;
		if (msr & MSR_CTS)
			tiocm_xxx |= TIOCM_CTS;
		if (msr & MSR_DCD)
			tiocm_xxx |= TIOCM_CD;
		if (msr & MSR_DSR)
			tiocm_xxx |= TIOCM_DSR;
		/*
		 * XXX - MSR_RI is naturally volatile, and we make MSR_TERI
		 * more volatile by reading the modem status a lot.  Perhaps
		 * we should latch both bits until the status is read here.
		 */
		if (msr & (MSR_RI | MSR_TERI))
			tiocm_xxx |= TIOCM_RI;
		*(int *)data = tiocm_xxx;
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0) {
			splx(s);
			return (EPERM);
		}
		com->dtr_wait = *(int *)data * 100 / hz;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = com->dtr_wait;
		break;
#ifdef TIOCTIMESTAMP
	case TIOCTIMESTAMP:
		com->do_timestamp = TRUE;
		*(struct timeval *)data = com->timestamp;
		break;
#endif
	default:
		splx(s);
		return (ENOTTY);
	}
	splx(s);
	return (0);
}

/* cancel pending output */
static void
comflush(com)
	struct com_s	*com;
{
	struct ringb	*rbp;

	disable_intr();
	if (com->state & CS_ODONE)
		com_events -= LOTS_OF_EVENTS;
	com->state &= ~(CS_ODONE | CS_BUSY);
	enable_intr();
	rbp = com->tp->t_out;
	rbp->rb_hd += com->ocount;
	rbp->rb_hd = RB_ROLLOVER(rbp, rbp->rb_hd);
	com->ocount = 0;
	com->tp->t_state &= ~TS_BUSY;
}

void
siopoll()
{
	int		unit;

	if (com_events == 0)
		return;
repeat:
	for (unit = 0; unit < NSIO; ++unit) {
		u_char		*buf;
		struct com_s	*com;
		u_char		*ibuf;
		int		incc;
		struct tty	*tp;

		com = com_addr(unit);
		if (com == NULL)
			continue;
		tp = com->tp;
		if (tp == NULL)
			continue;

		/* switch the role of the low-level input buffers */
		if (com->iptr == (ibuf = com->ibuf)) {
			buf = NULL;     /* not used, but compiler can't tell */
			incc = 0;
		} else {
			/*
			 * Prepare to reduce input latency for packet
			 * discplines with a end of packet character.
			 * XXX should be elsewhere.
			 */
			if (tp->t_line == SLIPDISC)
				com->hotchar = 0xc0;
			else if (tp->t_line == PPPDISC)
				com->hotchar = 0x7e;
			else
				com->hotchar = 0;
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
			    && !(tp->t_state & TS_RTS_IFLOW))
				outb(com->modem_ctl_port,
				     com->mcr_image |= MCR_RTS);
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

		/* XXX */
		if (TRUE) {
			u_int	delta;
			int	errnum;
			u_long	total;

			for (errnum = 0; errnum < CE_NTYPES; ++errnum) {
				disable_intr();
				delta = com->delta_error_counts[errnum];
				com->delta_error_counts[errnum] = 0;
				enable_intr();
				if (delta == 0 || !(tp->t_state & TS_ISOPEN))
					continue;
				total = com->error_counts[errnum] += delta;
					log(LOG_WARNING,
					"sio%d: %u more %s%s (total %lu)\n",
					    unit, delta, error_desc[errnum],
					    delta == 1 ? "" : "s", total);
				if (errnum == CE_OVERRUN && com->hasfifo
				    && com->ftl > FIFO_TRIGGER_1) {
					static	u_char ftl_in_bytes[] =
						{ 1, 4, 8, 14, };

					com->ftl_init = FIFO_TRIGGER_8;
#define	FIFO_TRIGGER_DELTA	FIFO_TRIGGER_4
					com->ftl_max =
					com->ftl -= FIFO_TRIGGER_DELTA;
					outb(com->iobase + com_fifo,
					     FIFO_ENABLE | com->ftl);
					log(LOG_WARNING,
				"sio%d: reduced fifo trigger level to %d\n",
					    unit,
					    ftl_in_bytes[com->ftl
							 / FIFO_TRIGGER_DELTA]);
				}
			}
		}
		if (com->state & CS_ODONE) {
			comflush(com);
			/* XXX - why isn't the table used for t_line == 0? */
			if (tp->t_line != 0)
				(*linesw[tp->t_line].l_start)(tp);
			else
				comstart(tp);
		}
		if (incc <= 0 || !(tp->t_state & TS_ISOPEN))
			continue;
		if (com->state & CS_RTS_IFLOW
		    && RB_LEN(tp->t_raw) + incc >= RB_I_HIGH_WATER
		    && !(tp->t_state & TS_RTS_IFLOW)
		    /*
		     * XXX - need RTS flow control for all line disciplines.
		     * Only have it in standard one now.
		     */
		    && linesw[tp->t_line].l_rint == ttyinput) {
			tp->t_state |= TS_RTS_IFLOW;
			ttstart(tp);
		}
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode.  It usually takes about 450
		 * instructions (that's without canonical processing or echo!).
		 * slinput is reasonably fast (usually 40 instructions plus
		 * call overhead).
		 */
		if (!(tp->t_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP
				   | IXOFF | IXON))
		    && !(tp->t_lflag & (ECHO | ECHONL | ICANON | IEXTEN | ISIG
				   | PENDIN))
		    && !(tp->t_state & (TS_CNTTB | TS_LNCH))
		    && linesw[tp->t_line].l_rint == ttyinput) {
			tk_nin += incc;
			tk_rawcc += incc;
			tp->t_rawcc += incc;
			com->delta_error_counts[CE_TTY_BUF_OVERFLOW]
				+= incc - rb_write(tp->t_raw, (char *) buf,
						   incc);
			ttwakeup(tp);
			if (tp->t_state & TS_TTSTOP
			    && (tp->t_iflag & IXANY
				|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
				tp->t_state &= ~TS_TTSTOP;
				tp->t_lflag &= ~FLUSHO;
				ttstart(tp);
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
	u_int		cfcr;
	int		cflag;
	struct com_s	*com;
	int		divisor;
	int		error;
	Port_t		iobase;
	int		s;
	int		unit;

	/* check requested parameters */
	divisor = ttspeedtab(t->c_ospeed, comspeedtab);
	if (t->c_ispeed == 0)
		t->c_ispeed = t->c_ospeed;
	if (divisor < 0 || divisor > 0 && t->c_ispeed != t->c_ospeed)
		return (EINVAL);

	/* parameters are OK, convert them to the com struct and the device */
	unit = DEV_TO_UNIT(tp->t_dev);
	com = com_addr(unit);
	iobase = com->iobase;
	s = spltty();
	if (divisor == 0)
		commctl(com, MCR_DTR, DMBIC);	/* hang up line */
	else
		commctl(com, MCR_DTR, DMBIS);
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

	if (com->hasfifo) {
		/*
		 * Use a fifo trigger level low enough so that the input
		 * latency from the fifo is less than about 16 msec and
		 * the total latency is less than about 30 msec.  These
		 * latencies are reasonable for humans.  Serial comms
		 * protocols shouldn't expect anything better since modem
		 * latencies are larger.
		 */
		com->ftl = t->c_ospeed <= 4800
			   ? FIFO_TRIGGER_1 : FIFO_TRIGGER_14;
		if (com->ftl > com->ftl_max)
			com->ftl = com->ftl_max;
		outb(iobase + com_fifo, FIFO_ENABLE | com->ftl);
	}

	/*
	 * Some UARTs lock up if the divisor latch registers are selected
	 * while the UART is doing output (they refuse to transmit anything
	 * more until given a hard reset).  Fix this by stopping filling
	 * the device buffers and waiting for them to drain.  Reading the
	 * line status port outside of siointr1() might lose some receiver
	 * error bits, but that is acceptable here.
	 */
	disable_intr();
retry:
	com->state &= ~CS_TTGO;
	enable_intr();
	while ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	       != (LSR_TSRE | LSR_TXRDY)) {
		error = ttysleep(tp, TSA_OCOMPLETE(tp), TTIPRI | PCATCH,
				 "siotx", hz / 100);
		if (error != 0 && error != EAGAIN) {
			if (!(tp->t_state & TS_TTSTOP)) {
				disable_intr();
				com->state |= CS_TTGO;
				enable_intr();
			}
			splx(s);
			return (error);
		}
	}

	disable_intr();		/* very important while com_data is hidden */

	/*
	 * XXX - clearing CS_TTGO is not sufficient to stop further output,
	 * because siopoll() calls comstart() which usually sets it again
	 * because TS_TTSTOP is clear.  Setting TS_TTSTOP would not be
	 * sufficient, for similar reasons.
	 */
	if ((inb(com->line_status_port) & (LSR_TSRE | LSR_TXRDY))
	    != (LSR_TSRE | LSR_TXRDY))
		goto retry;

	if (divisor != 0) {
		outb(iobase + com_cfcr, cfcr | CFCR_DLAB);
		outb(iobase + com_dlbl, divisor & 0xFF);
		outb(iobase + com_dlbh, (u_int) divisor >> 8);
	}
	outb(iobase + com_cfcr, com->cfcr_image = cfcr);
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
	com->state &= ~CS_CTS_OFLOW;
	com->state |= CS_ODEVREADY;
	if (cflag & CCTS_OFLOW) {
		com->state |= CS_CTS_OFLOW;
		if (!(com->last_modem_status & MSR_CTS))
			com->state &= ~CS_ODEVREADY;
	}

	/*
	 * Recover from fiddling with CS_TTGO.  We used to call siointr1()
	 * unconditionally, but that defeated the careful discarding of
	 * stale input in sioopen().
	 */
	if (com->state >= (CS_BUSY | CS_TTGO))
		siointr1(com);

	enable_intr();
	splx(s);
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
	if (tp->t_state & TS_RTS_IFLOW) {
		if (com->mcr_image & MCR_RTS && com->state & CS_RTS_IFLOW)
			outb(com->modem_ctl_port, com->mcr_image &= ~MCR_RTS);
	} else {
		/*
		 * XXX don't raise MCR_RTS if CTS_RTS_IFLOW is off.  Set it
		 * appropriately in comparam() if RTS-flow is being changed.
		 * Check for races.
		 */
		if (!(com->mcr_image & MCR_RTS) && com->iptr < com->ihighwater)
			outb(com->modem_ctl_port, com->mcr_image |= MCR_RTS);
	}
	enable_intr();
	if (tp->t_state & (TS_TIMEOUT | TS_TTSTOP))
		goto out;
	if (tp->t_state & (TS_SO_OCOMPLETE | TS_SO_OLOWAT) || tp->t_wsel)
		ttwwakeup(tp);
	if (com->ocount != 0) {
		disable_intr();
		siointr1(com);
		enable_intr();
	} else if (RB_LEN(tp->t_out) != 0) {
		tp->t_state |= TS_BUSY;
		com->ocount = RB_CONTIGGET(tp->t_out);
		disable_intr();
		com->obufend = (com->optr = (u_char *)tp->t_out->rb_hd)
			      + com->ocount;
		com->state |= CS_BUSY;
		siointr1(com);	/* fake interrupt to start output */
		enable_intr();
	}
out:
	splx(s);
}

void
siostop(tp, rw)
	struct tty	*tp;
	int		rw;
{
	struct com_s	*com;

	com = com_addr(DEV_TO_UNIT(tp->t_dev));
	if (rw & FWRITE)
		comflush(com);
	disable_intr();
	if (rw & FREAD) {
		com_events -= (com->iptr - com->ibuf);
		com->iptr = com->ibuf;
	}
	if (tp->t_state & TS_TTSTOP)
		com->state &= ~CS_TTGO;
	else
		com->state |= CS_TTGO;
	enable_intr();
}

int
sioselect(dev, rw, p)
	dev_t		dev;
	int		rw;
	struct proc	*p;
{
	if (minor(dev) & CONTROL_MASK)
		return (ENODEV);
	return (ttselect(dev & ~MINOR_MAGIC_MASK, rw, p));
}

static void
commctl(com, bits, how)
	struct com_s	*com;
	int		bits;
	int		how;
{
	disable_intr();
	switch (how) {
	case DMSET:
		outb(com->modem_ctl_port,
		     com->mcr_image = bits | (com->mcr_image & MCR_IENABLE));
		break;
	case DMBIS:
		outb(com->modem_ctl_port, com->mcr_image |= bits);
		break;
	case DMBIC:
		outb(com->modem_ctl_port, com->mcr_image &= ~bits);
		break;
	}
	enable_intr();
}

static void
comwakeup(chan, ticks)
	caddr_t	chan;
	int	ticks;
{
	int	unit;

	timeout(comwakeup, (caddr_t)NULL, hz / 100);

	if (com_events != 0) {
		int	s;

		s = splsofttty();
		siopoll();
		splx(s);
	}

	/* recover from lost output interrupts */
	for (unit = 0; unit < NSIO; ++unit) {
		struct com_s	*com;

		com = com_addr(unit);
		if (com != NULL && com->state >= (CS_BUSY | CS_TTGO)) {
			disable_intr();
			siointr1(com);
			enable_intr();
		}
	}
}

/*
 * Following are all routines needed for SIO to act as console
 */
#include "i386/i386/cons.h"

struct siocnstate {
	u_char	dlbl;
	u_char	dlbh;
	u_char	ier;
	u_char	cfcr;
	u_char	mcr;
};

static	Port_t	siocniobase;

static void siocnclose	__P((struct siocnstate *sp));
static void siocnopen	__P((struct siocnstate *sp));
static void siocntxwait	__P((void));

static void
siocntxwait()
{
	int	timo;

	/*
	 * Wait for any pending transmission to finish.  Required to avoid
	 * the UART lockup bug when the speed is changed, and for normal
	 * transmits.
	 */
	timo = 100000;
	while ((inb(siocniobase + com_lsr) & (LSR_TSRE | LSR_TXRDY))
	       != (LSR_TSRE | LSR_TXRDY) && --timo != 0)
		;
}

static void
siocnopen(sp)
	struct siocnstate	*sp;
{
	int	divisor;
	Port_t	iobase;

	/*
	 * Save all the device control registers except the fifo register
	 * and set our default ones (cs8 -parenb speed=comdefaultrate).
	 * We can't save the fifo register since it is read-only.
	 */
	iobase = siocniobase;
	sp->ier = inb(iobase + com_ier);
	outb(iobase + com_ier, 0);	/* spltty() doesn't stop siointr() */
	siocntxwait();
	sp->cfcr = inb(iobase + com_cfcr);
	outb(iobase + com_cfcr, CFCR_DLAB);
	sp->dlbl = inb(iobase + com_dlbl);
	sp->dlbh = inb(iobase + com_dlbh);
	divisor = ttspeedtab(comdefaultrate, comspeedtab);
	outb(iobase + com_dlbl, divisor & 0xFF);
	outb(iobase + com_dlbh, (u_int) divisor >> 8);
	outb(iobase + com_cfcr, CFCR_8BITS);
	sp->mcr = inb(iobase + com_mcr);
	outb(iobase + com_mcr, MCR_DTR | MCR_RTS);
}

static void
siocnclose(sp)
	struct siocnstate	*sp;
{
	Port_t	iobase;

	/*
	 * Restore the device control registers.
	 */
	siocntxwait();
	iobase = siocniobase;
	outb(iobase + com_cfcr, CFCR_DLAB);
	outb(iobase + com_dlbl, sp->dlbl);
	outb(iobase + com_dlbh, sp->dlbh);
	outb(iobase + com_cfcr, sp->cfcr);
	/*
	 * XXX damp oscllations of MCR_DTR and MCR_RTS by not restoring them.
	 */
	outb(iobase + com_mcr, sp->mcr | MCR_DTR | MCR_RTS);
	outb(iobase + com_ier, sp->ier);
}

void
siocnprobe(cp)
	struct consdev	*cp;
{
	int	unit;

	/* locate the major number */
	/* XXX - should be elsewhere since KGDB uses it */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == sioopen)
			break;

	/* XXX: ick */
	unit = DEV_TO_UNIT(CONUNIT);
	siocniobase = CONADDR;

	/* make sure hardware exists?  XXX */

	/* initialize required fields */
	cp->cn_dev = makedev(commajor, unit);
#ifdef	COMCONSOLE
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
siocninit(cp)
	struct consdev	*cp;
{
	/*
	 * XXX can delete more comconsole stuff now that i/o routines are
	 * fairly reentrant.
	 */
	comconsole = DEV_TO_UNIT(cp->cn_dev);
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
	siocnopen(&sp);
	while (!(inb(iobase + com_lsr) & LSR_RXRDY))
		;
	c = inb(iobase + com_data);
	siocnclose(&sp);
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
	siocnopen(&sp);
	siocntxwait();
	outb(siocniobase + com_data, c);
	siocnclose(&sp);
	splx(s);
}

#endif /* NSIO > 0 */
