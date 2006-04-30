/*-
 * Device tsfsdriver for Specialix I/O8+ multiport serial card.
 *
 * Copyright 2003 Frank Mayhar <frank@exit.com>
 *
 * Derived from the "si" driver by Peter Wemm <peter@netplex.com.au>, using
 * lots of information from the Linux "specialix" driver by Roger Wolff
 * <R.E.Wolff@BitWizard.nl> and from the Intel CD1865 "Intelligent Eight-
 * Channel Communications Controller" datasheet.  Roger was also nice
 * enough to answer numerous questions about stuff specific to the I/O8+
 * not covered by the CD1865 datasheet.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the foljxowing disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 * $FreeBSD$
 */


/* Main tty driver routines for the Specialix I/O8+ device driver. */

#include "opt_compat.h"
#include "opt_debug_sx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/serial.h>
#include <sys/sysctl.h>
#include <sys/tty.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <machine/stdarg.h>

#include <dev/sx/cd1865.h>
#include <dev/sx/sxvar.h>
#include <dev/sx/sx.h>
#include <dev/sx/sx_util.h>

#define SX_BROKEN_CTS


static t_modem_t sx_modem;
static void sx_start(struct tty *);
static void sx_stop(struct tty *, int);
static t_close_t sxclose;
static t_open_t	sxopen;
static void sx_shutdown_chan(struct sx_port *);

static t_break_t sxbreak;

static int	sxparam(struct tty *, struct termios *);

static void sx_modem_state(struct sx_softc *sc, struct sx_port *pp, int card);

static int sx_debug = 0; /* DBG_ALL|DBG_PRINTF|DBG_MODEM|DBG_IOCTL|DBG_PARAM;e */
SYSCTL_INT(_machdep, OID_AUTO, sx_debug, CTLFLAG_RW, &sx_debug, 0, "");

static int sx_numunits;

devclass_t sx_devclass;

/*
 * See sx.h for these values.
 */
static struct speedtab bdrates[] = {
	{ B75,		CLK75, },
	{ B110,		CLK110, },
	{ B150,		CLK150, },
	{ B300,		CLK300, },
	{ B600,		CLK600, },
	{ B1200,	CLK1200, },
	{ B2400,	CLK2400, },
	{ B4800,	CLK4800, },
	{ B9600,	CLK9600, },
	{ B19200,	CLK19200, },
	{ B38400,	CLK38400, },
	{ B57600,	CLK57600, },
	{ B115200,	CLK115200, },
	{ -1,		-1 },
};


/*
 * Approximate (rounded) character per second rates.  Translated at card
 * initialization time to characters per clock tick.
 */
static int done_chartimes = 0;
static struct speedtab chartimes[] = {
	{ B75,		8, },
	{ B110,		11, },
	{ B150,		15, },
	{ B300,		30, },
	{ B600,		60, },
	{ B1200,	120, },
	{ B2400,	240, },
	{ B4800,	480, },
	{ B9600,	960, },
	{ B19200,	1920, },
	{ B38400,	3840, },
	{ B57600,	5760, },
	{ B115200,	11520, },
	{ -1,		-1 },
};
static volatile int in_interrupt = 0;	/* Inside interrupt handler?          */

static int sx_flags;			/* The flags we were configured with. */
SYSCTL_INT(_machdep, OID_AUTO, sx_flags, CTLFLAG_RW, &sx_flags, 0, "");

#ifdef POLL
static int sx_pollrate;			/* in addition to irq */
static int sx_realpoll = 0;		/* poll HW on timer */

SYSCTL_INT(_machdep, OID_AUTO, sx_pollrate, CTLFLAG_RW, &sx_pollrate, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, sx_realpoll, CTLFLAG_RW, &sx_realpoll, 0, "");

static int init_finished = 0;
static void sx_poll(void *);
#endif

/*
 * sxattach()
 *	Initialize and attach the card, initialize the driver.
 *
 * Description:
 *	This is the standard attach routine.  It initializes the I/O8+
 *	card, identifies the chip on that card, then allocates and
 *	initializes the various data structures used by the driver
 *	itself.
 */
int
sxattach(
	device_t dev)
{
	int unit;
	struct sx_softc *sc;
	struct tty *tp;
	struct speedtab *spt;
	int chip, x;
	char rev;
	int error;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sx_flags = device_get_flags(dev);

	if (sx_numunits < unit + 1)
		sx_numunits = unit + 1;

	DPRINT((0, DBG_AUTOBOOT, "sx%d: sxattach\n", unit));

	/* Reset the CD1865.  */
	if ((error = sx_init_cd1865(sc, unit)) != 0) {
		return(error);
	}

	/*
	 * ID the chip:
	 *
	 * Chip           revcode   pkgtype
	 *                GFRCR     SRCR bit 7
	 * CD180 rev B    0x81      0
	 * CD180 rev C    0x82      0
	 * CD1864 rev A   0x82      1
	 * CD1865 rev A   0x83      1  -- Do not use!!! Does not work. 
	 * CD1865 rev B   0x84      1
	 *       -- Thanks to Gwen Wang, Cirrus Logic (via Roger Wollf).
	 */
	switch (sx_cd1865_in(sc, CD1865_GFRCR)) {
		case 0x82:
			chip = 1864;
			rev = 'A';
			break;
		case 0x83:
			chip = 1865;
			rev = 'A';
			break;
		case 0x84:
			chip = 1865;
			rev = 'B';
			break;
		case 0x85:
			chip = 1865;
			rev = 'C';
			break;
		default:
			chip = -1;
			rev = '\0';
			break;
	}

	if (bootverbose && chip != -1)
		printf("sx%d: Specialix I/O8+ CD%d processor rev %c\n",
							unit, chip, rev);
	DPRINT((0, DBG_AUTOBOOT, "sx%d: GFRCR 0x%02x\n",
					unit, sx_cd1865_in(sc, CD1865_GFRCR)));
#ifdef POLL
	if (sx_pollrate == 0) {
		sx_pollrate = POLLHZ;		/* in addition to irq */
#ifdef REALPOLL
		sx_realpoll = 1;		/* scan always */
#endif
	}
#endif
	sc->sc_ports = (struct sx_port *)malloc(
					   sizeof(struct sx_port) * SX_NUMCHANS,
					   M_DEVBUF,
					   M_NOWAIT);
	if (sc->sc_ports == NULL) {
		printf("sx%d:  No memory for sx_port structs!\n", unit);
		return(EINVAL);
	}
	bzero(sc->sc_ports, sizeof(struct sx_port) * SX_NUMCHANS);
	/*
	 * Initialize the channels.
	 */
	for (x = 0; x < SX_NUMCHANS; x++) {
		tp = ttyalloc();
		tp->t_oproc = sx_start;
		tp->t_stop = sx_stop;
		tp->t_param = sxparam;
		tp->t_break = sxbreak;
		tp->t_open = sxopen;
		tp->t_close = sxclose;
		sc->sc_ports[x].sp_tty = tp;
		sc->sc_ports[x].sp_state = 0;	/* internal flag */
		sc->sc_ports[x].sp_chan = x;
		sc->sc_ports[x].sp_sc = sc;
		tp->t_sc = &sc->sc_ports[x];
		ttycreate(tp, NULL, 0, MINOR_CALLOUT, "G%r%r", unit, x);
	}
	if (done_chartimes == 0) {
		for (spt = chartimes ; spt->sp_speed != -1; spt++) {
			if ((spt->sp_code /= hz) == 0)
				spt->sp_code = 1;
		}
		done_chartimes = 1;
	}
	return (0);
}

static int
sxopen(struct tty *tp, struct cdev *dev)
{

#ifdef	POLL
	/*
	 * We've now got a device, so start the poller.
	 */
	if (init_finished == 0) {
		timeout(sx_poll, (caddr_t)0L, sx_pollrate);
		init_finished = 1;
	}
#endif
	return (0);
}

/*
 * sxclose()
 *	Do hard-close processing.
 *
 * Description:
 *	Called on last close.  Handle DTR and RTS, do cleanup.  If we have
 *	pending output in the FIFO, wait for it to clear before we shut down
 *	the hardware.
 */
static void
sxclose(struct tty *tp)
{
	struct sx_port *pp;
	struct sx_softc *sc;
	int oldspl, dcd;

	oldspl = spltty();

	pp = tp->t_sc;
	DPRINT((pp, DBG_CLOSE, "sxhardclose sp_state:%x\n", pp->sp_state));
	sc = pp->sp_sc;
	dcd = sx_modem(tp, 0, 0) & SER_DCD;
	if (tp->t_cflag & HUPCL ||
	    (!tp->t_actout && !dcd && !(tp->t_init_in.c_cflag && CLOCAL)) ||
	    !(tp->t_state & TS_ISOPEN)) {
		disable_intr();
		sx_cd1865_out(sc, CD1865_CAR, pp->sp_chan);
		if (sx_cd1865_in(sc, CD1865_IER|SX_EI) & CD1865_IER_TXRDY) {
			sx_cd1865_bic(sc, CD1865_IER, CD1865_IER_TXRDY);
			sx_cd1865_bis(sc, CD1865_IER, CD1865_IER_TXEMPTY);
			enable_intr();
			splx(oldspl);
			ttysleep(tp, (caddr_t)pp,
				 TTOPRI|PCATCH, "sxclose", tp->t_timeout);
			oldspl = spltty();
		}
		else {
			enable_intr();
		}
		(void)sx_modem(tp, 0, SER_DTR | SER_RTS);

	}
	(void)sx_shutdown_chan(pp);	/* Turn off the hardware.             */
	tp->t_actout = FALSE;
	wakeup((caddr_t)&tp->t_actout);
	wakeup(TSA_CARR_ON(tp));

	splx(oldspl);
}


static void
sxbreak(struct tty *tp, int sig)
{
	struct sx_port *pp;

	pp = tp->t_sc;
	if (sig) {
		/*
		 * If there's already a break state change pending or
		 * we're already sending a break, just ignore this.
		 * Otherwise, just set our flag and start the
		 * transmitter.
		 */
		if (!SX_DOBRK(pp) && !SX_BREAK(pp)) {
			pp->sp_state |= SX_SS_DOBRK;
			sx_start(tp);
		}
	} else {
		/*
		 * If a break is going, set our flag so we turn it off
		 * when we can, then kick the transmitter.  If a break
		 * isn't going and the flag is set, turn it off.
		 */
		if (SX_BREAK(pp)) {
			pp->sp_state |= SX_SS_DOBRK;
			sx_start(tp);
		}
		else {
			if (SX_DOBRK(pp))
				pp->sp_state &= SX_SS_DOBRK;
		}
	}
}

/*
 * sxparam()
 *	Configure line parameters.
 *
 * Description:
 *	Configure the bitrate, wordsize, flow control and various other serial
 *	port parameters for this line.
 *
 * Environment:
 *	Called at spltty(); this may sleep, does not flush nor wait for drain,
 *	nor block writes.  Caller must arrange this if it's important..
 */
static int
sxparam(
	struct tty *tp,
	struct termios *t)
{
	struct sx_softc *sc;
	struct sx_port *pp = tp->t_sc;
	int oldspl, cflag, iflag, oflag, lflag;
	int error = 0;
	int ispd = 0;
	int ospd = 0;
	unsigned char val, cor1, cor2, cor3, ier;

	sc = pp->sp_sc;
	DPRINT((pp, DBG_ENTRY|DBG_PARAM, "sxparam %x/%x\n", tp, t));
	cflag = t->c_cflag;
	iflag = t->c_iflag;
	oflag = t->c_oflag;
	lflag = t->c_lflag;
	DPRINT((pp, DBG_PARAM, "OF 0x%x CF 0x%x IF 0x%x LF 0x%x\n",
		oflag, cflag, iflag, lflag));

	/* If the port isn't hung up... */
	if (t->c_ospeed != 0) {
		/* Convert bit rate to hardware divisor values. */
		ospd = ttspeedtab(t->c_ospeed, bdrates);
		ispd = t->c_ispeed ? ttspeedtab(t->c_ispeed, bdrates) : ospd;
		/* We only allow standard bit rates. */
		if (ospd < 0 || ispd < 0)
			return(EINVAL);
	}
	oldspl = spltty();		/* Block other activity.              */
	cor1 = 0;
	cor2 = 0;
	cor3 = 0;
	ier = CD1865_IER_RXD | CD1865_IER_CD;
#ifdef notyet
	/* We don't yet handle this stuff. */
	val = 0;
	if (iflag & IGNBRK)		/* Breaks */
		val |= BR_IGN;
	if (iflag & BRKINT)		/* Interrupt on break? */
		val |= BR_INT;
	if (iflag & PARMRK)		/* Parity mark? */
		val |= BR_PARMRK;
#endif /* notyet */
	/*
	 * If the device isn't hung up, set the serial port bitrates.
	 */
	if (t->c_ospeed != 0) {
		disable_intr();
		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
		sx_cd1865_out(sc, CD1865_RBPRH|SX_EI, (ispd >> 8) & 0xff);
		sx_cd1865_out(sc, CD1865_RBPRL|SX_EI, ispd & 0xff);
		sx_cd1865_out(sc, CD1865_TBPRH|SX_EI, (ospd >> 8) & 0xff);
		sx_cd1865_out(sc, CD1865_TBPRL|SX_EI, ospd & 0xff);
		enable_intr();
	}
	if (cflag & CSTOPB)		/* Two stop bits?                     */
		cor1 |= CD1865_COR1_2SB; /* Yep.                              */
	/*
	 * Parity settings.
	 */
	val = 0;
	if (cflag & PARENB) {		/* Parity enabled?                    */
		val = CD1865_COR1_NORMPAR; /* Turn on normal parity handling. */
		if (cflag & PARODD)	/* Odd Parity?                        */
			val |= CD1865_COR1_ODDP; /* Turn it on.               */
	}
	else
		val = CD1865_COR1_NOPAR; /* Turn off parity detection.        */
	cor1 |= val;
	if (iflag & IGNPAR)		/* Ignore chars with parity errors?   */
		cor1 |= CD1865_COR1_IGNORE;
	/*
	 * Set word length.
	 */
	if ((cflag & CS8) == CS8)
		val = CD1865_COR1_8BITS;
	else if ((cflag & CS7) == CS7)
		val = CD1865_COR1_7BITS;
	else if ((cflag & CS6) == CS6)
		val = CD1865_COR1_6BITS;
	else
		val = CD1865_COR1_5BITS;
	cor1 |= val;
	/*
	 * Enable hardware RTS/CTS flow control.  We can handle output flow
	 * control at any time, since we have a dedicated CTS pin.
	 * Unfortunately, though, the RTS pin is really the DTR pin.  This
	 * means that we can't ever use the automatic input flow control of
	 * the CD1865 and that we can only use the pin for input flow
	 * control when it's wired as RTS.
	 */
	if (cflag & CCTS_OFLOW) {	/* Output flow control...             */
		pp->sp_state |= SX_SS_OFLOW;
#ifdef SX_BROKEN_CTS
		disable_intr();
		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
		if (sx_cd1865_in(sc, CD1865_MSVR|SX_EI) & CD1865_MSVR_CTS) {
			enable_intr();
			pp->sp_state |= SX_SS_OSTOP;
		}
		else {
			enable_intr();
		}
		ier |= CD1865_IER_CTS;
#else /* SX_BROKEN_CTS */
		cor2 |= CD1865_COR2_CTSAE; /* Set CTS automatic enable.       */
#endif /* SX_BROKEN_CTS */
	}
	else {
		pp->sp_state &= ~SX_SS_OFLOW;
	}
	if (cflag & CRTS_IFLOW && !SX_DTRPIN(pp)) /* Input flow control.      */
		pp->sp_state |= SX_SS_IFLOW;
	else
		pp->sp_state &= ~SX_SS_IFLOW;
	if (iflag & IXANY)
		cor2 |= CD1865_COR2_IXM;	/* Any character is XON.      */
	if (iflag & IXOFF) {
		cor2 |= CD1865_COR2_TXIBE;	/* Enable inband flow control.*/
		cor3 |= CD1865_COR3_FCT | CD1865_COR3_SCDE; /* Hide from host */
		disable_intr();
		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan); /* Sel chan.*/
		sx_cd1865_out(sc, CD1865_SCHR1|SX_EI, t->c_cc[VSTART]);
		sx_cd1865_out(sc, CD1865_SCHR2|SX_EI, t->c_cc[VSTOP]);
		sx_cd1865_out(sc, CD1865_SCHR3|SX_EI, t->c_cc[VSTART]);
		sx_cd1865_out(sc, CD1865_SCHR4|SX_EI, t->c_cc[VSTOP]);
		enable_intr();
	}
	/*
	 * All set, now program the hardware.
	 */
	disable_intr();
	sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan); /* Select channel.  */
	sx_cd1865_out(sc, CD1865_COR1|SX_EI, cor1);
	sx_cd1865_out(sc, CD1865_COR2|SX_EI, cor2);
	sx_cd1865_out(sc, CD1865_COR3|SX_EI, cor3);
	sx_cd1865_wait_CCR(sc, SX_EI);
	sx_cd1865_out(sc, CD1865_CCR|SX_EI,
		      CD1865_CCR_CORCHG1|CD1865_CCR_CORCHG2|CD1865_CCR_CORCHG3);
	sx_cd1865_wait_CCR(sc, SX_EI);
	enable_intr();
	if (SX_DTRPIN(pp))
		val = SER_DTR;
	else
		val = SER_RTS;
	if (t->c_ospeed == 0)		/* Clear DTR/RTS if we're hung up.    */
		(void)sx_modem(tp, 0, val);
	else				/* If we were hung up, we may have to */
		(void)sx_modem(tp, 0, val);
	/*
	 * Last, enable the receiver and transmitter and turn on the
	 * interrupts we need (receive, carrier-detect and possibly CTS
	 * (iff we're built with SX_BROKEN_CTS and CCTS_OFLOW is on).
	 */
	disable_intr();
	sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan); /* Select channel.  */
	sx_cd1865_wait_CCR(sc, SX_EI);
	sx_cd1865_out(sc, CD1865_CCR|SX_EI, CD1865_CCR_RXEN|CD1865_CCR_TXEN);
	sx_cd1865_wait_CCR(sc, SX_EI);
	sx_cd1865_out(sc, CD1865_IER|SX_EI, ier);
	enable_intr();
	DPRINT((pp, DBG_PARAM, "sxparam out\n"));
	splx(oldspl);
	return(error);
}

/*
 * sx_shutdown_chan()
 *	Shut down a channel on the I/O8+.
 *
 * Description:
 *	This does all hardware shutdown processing for a channel on the I/O8+.
 *	It is called from sxhardclose().  We reset the channel and turn off
 *	interrupts.
 */
static void
sx_shutdown_chan(
	struct sx_port *pp)
{
	int s;
	struct sx_softc *sc;

	DPRINT((pp, DBG_ENTRY, "sx_shutdown_chan %x %x\n", pp, pp->sp_state));
	sc = pp->sp_sc;
	s = spltty();
	disable_intr();
	sx_cd1865_out(sc, CD1865_CAR, pp->sp_chan); /* Select channel.        */
	sx_cd1865_wait_CCR(sc, 0);	/* Wait for any commands to complete. */
	sx_cd1865_out(sc, CD1865_CCR, CD1865_CCR_SOFTRESET); /* Reset chan.   */
	sx_cd1865_wait_CCR(sc, 0);
	sx_cd1865_out(sc, CD1865_IER, 0); /* Disable all interrupts.          */
	enable_intr();
	splx(s);
}

/*
 * sx_modem()
 *	Set/Get state of modem control lines.
 *
 * Description:
 *	Get and set the state of the modem control lines that we have available
 *	on the I/O8+.  The only lines we are guaranteed to have are CD and CTS.
 *	We have DTR if the "DTR/RTS pin is DTR" flag is set, otherwise we have
 *	RTS through the DTR pin.
 */
static int
sx_modem(struct tty *tp, int sigon, int sigoff)
{
	int s, x;
	struct sx_softc *sc;
	struct sx_port *pp;

	pp = tp->t_sc;
	sc = pp->sp_sc;

	s = spltty();			/* Block interrupts.                  */
	disable_intr();
	sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan); /* Select our port. */
	x = sx_cd1865_in(sc, CD1865_MSVR|SX_EI); /* Get the current signals.  */
#ifdef SX_DEBUG
	DPRINT((pp, DBG_MODEM, "sx_modem MSVR 0x%x, CCSR %x GIVR %x SRSR %x\n",
		x, sx_cd1865_in(sc, CD1865_CCSR|SX_EI),
		sx_cd1865_in(sc, CD1865_GIVR|SX_EI),
		sx_cd1865_in(sc, CD1865_SRSR|SX_EI)));
#endif
	enable_intr();			/* Allow other interrupts.            */
	if (sigon == 0 && sigoff == 0) {
		if ((x & CD1865_MSVR_CD) == 0)
			sigon |= SER_DCD;
		if ((x & CD1865_MSVR_CTS) == 0)
			sigon |= SER_CTS;
		if ((x & CD1865_MSVR_DTR) == 0) {
			if (SX_DTRPIN(pp)) /* Odd pin is DTR?         */
				sigon |= SER_DTR; /* Report DTR.     */
			else		   /* Odd pin is RTS.         */
				sigon |= SER_RTS; /* Report RTS.     */
		}
		return (sigon);
	}
	if ((sigon & SER_RTS && !SX_DTRPIN(pp)) ||
	    (sigon & SER_DTR && SX_DTRPIN(pp)))
		x &= ~CD1865_MSVR_DTR;
	if ((sigoff & SER_RTS && !SX_DTRPIN(pp)) ||
	    (sigoff & SER_DTR && SX_DTRPIN(pp)))
		x |= CD1865_MSVR_DTR;
	disable_intr();
	/*
	 * Set the new modem signals.
	 */
	sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
	sx_cd1865_out(sc, CD1865_MSVR|SX_EI, x);
	enable_intr();
	splx(s);
	return 0;
}

#ifdef POLL

/*
 * sx_poll()
 * 	Poller to catch missed interrupts.
 *
 * Description:
 *	Only used if we're complied with POLL.  This routine is called every
 *	sx_pollrate ticks to check for missed interrupts.  We check each card
 *	in the system; if we missed an interrupt, we complain about each one
 *	and later call sx_intr() to handle them.
 */
static void
sx_poll(
	void *dummy)
{
	struct sx_softc *sc;
	struct sx_port *pp;
	int card, lost, oldspl, chan;

	DPRINT((0, DBG_POLL, "sx_poll\n"));
	oldspl = spltty();
	if (in_interrupt)
		goto out;
	lost = 0;
	for (card = 0; card < sx_numunits; card++) {
		sc = devclass_get_softc(sx_devclass, card);
		if (sc == NULL)
			continue;
		if (sx_cd1865_in(sc, CD1865_SRSR|SX_EI) & CD1865_SRSR_REQint) {
			printf("sx%d: lost interrupt\n", card);
			lost++;
		}
		/*
		 * Gripe about no input flow control.
		 */
		for (chan = 0; chan < SX_NUMCHANS; pp++, chan++) {
			pp = &(sc->sc_ports[chan]);
			if (pp->sp_delta_overflows > 0) {
				printf("sx%d: %d tty level buffer overflows\n",
				       card, pp->sp_delta_overflows);
				pp->sp_delta_overflows = 0;
			}
		}
	}
	if (lost || sx_realpoll)
		sx_intr(NULL);	/* call intr with fake vector */
out:	splx(oldspl);
	timeout(sx_poll, (caddr_t)0L, sx_pollrate);
}

#endif	/* POLL */


/*
 * sx_transmit()
 *	Handle transmit request interrupt.
 *
 * Description:
 *	This routine handles the transmit request interrupt from the CD1865
 *	chip on the I/O8+ card.  The CD1865 interrupts us for a transmit
 *	request under two circumstances:  When the last character in the
 *	transmit FIFO is sent and the channel is ready for more characters
 *	("transmit ready"), or when the last bit of the last character in the
 *	FIFO is actually transmitted ("transmit empty").  In the former case,
 *	we just pass processing off to sx_start() (via the line discipline)
 *	to queue more characters.  In the latter case, we were waiting for
 *	the line to flush in sxhardclose() so we need to wake the sleeper.
 */
static void
sx_transmit(
	struct sx_softc *sc,
	struct sx_port *pp,
	int card)
{
	struct tty *tp;
	unsigned char flags;

	tp = pp->sp_tty;
	/*
	 * Let others know what we're doing.
	 */
	pp->sp_state |= SX_SS_IXMIT;
	/*
	 * Get the service request enable register to see what we're waiting
	 * for.
	 */
	flags = sx_cd1865_in(sc, CD1865_SRER|SX_EI);

	DPRINT((pp, DBG_TRANSMIT, "sx_xmit %x SRER %x\n", tp, flags));
	/*
	 * "Transmit ready."  The transmit FIFO is empty (but there are still
	 * two characters being transmitted), so we need to tell the line
	 * discipline to send more.
	 */
	if (flags & CD1865_IER_TXRDY) {
		ttyld_start(tp);
		pp->sp_state &= ~SX_SS_IXMIT;
		DPRINT((pp, DBG_TRANSMIT, "sx_xmit TXRDY out\n"));
		return;
	}
	/*
	 * "Transmit empty."  The transmitter is completely empty; turn off the
	 * service request and wake up the guy in sxhardclose() who is waiting
	 * for this.
	 */
	if (flags & CD1865_IER_TXEMPTY) {
		flags &= ~CD1865_IER_TXEMPTY;
		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
		sx_cd1865_out(sc, CD1865_SRER|SX_EI, flags);
		wakeup((caddr_t)pp);
	}
	pp->sp_state &= ~SX_SS_IXMIT;
	DPRINT((pp, DBG_TRANSMIT, "sx_xmit out\n"));
}

/*
 * sx_modem_state()
 *	Handle modem state-change request interrupt.
 *
 * Description:
 *	Handles changed modem signals CD and CTS.  We pass the CD change
 *	off to the line discipline.  We can't handle DSR since there isn't a
 *	pin for it.
 */
static void
sx_modem_state(
	struct sx_softc *sc,
	struct sx_port *pp,
	int card)
{
	struct tty *tp;
	unsigned char mcr;

	/*
	 * Let others know what we're doing.
	 */
	pp->sp_state |= SX_SS_IMODEM;
	tp = pp->sp_tty;
	/* Grab the Modem Change Register. */
	mcr = sx_cd1865_in(sc, CD1865_MCR|SX_EI);
	DPRINT((pp, DBG_MODEM_STATE,
		"sx_mdmst %x st %x sp %x mcr %x\n",
		tp, tp->t_state, pp->sp_state, mcr));
	if (mcr & CD1865_MCR_CDCHG) {	/* CD changed?                        */
		if ((sx_cd1865_in(sc, CD1865_MSVR) & CD1865_MSVR_CD) == 0) {
			DPRINT((pp, DBG_INTR, "modem carr on t_line %d\n",
				tp->t_line));
			(void)ttyld_modem(tp, 1);
		}
		else {			/* CD went down.                      */
			DPRINT((pp, DBG_INTR, "modem carr off\n"));
			if (ttyld_modem(tp, 0))
				(void)sx_modem(tp, 0, SER_DTR | SER_RTS);
		}
	}
#ifdef SX_BROKEN_CTS
	if (mcr & CD1865_MCR_CTSCHG) {	/* CTS changed?                       */
		if (sx_cd1865_in(sc, CD1865_MSVR|SX_EI) & CD1865_MSVR_CTS) {
			pp->sp_state |= SX_SS_OSTOP;
			sx_cd1865_bic(sc, CD1865_IER|SX_EI, CD1865_IER_TXRDY);
		}
		else {
			pp->sp_state &= ~SX_SS_OSTOP;
			sx_cd1865_bis(sc, CD1865_IER|SX_EI, CD1865_IER_TXRDY);
		}
	}
#endif /* SX_BROKEN_CTS */
	/* Clear state-change indicator bits. */
	sx_cd1865_out(sc, CD1865_MCR|SX_EI, 0);
	pp->sp_state &= ~SX_SS_IMODEM;
}

/*
 * sx_receive()
 *	Handle receive request interrupt.
 *
 * Description:
 *	Handle a receive request interrupt from the CD1865.  This is just a
 *	standard "we have characters to process" request, we don't have to
 *	worry about exceptions like BREAK and such.  Exceptions are handled
 *	by sx_receive_exception().
 */
static void
sx_receive(
	struct sx_softc *sc,
	struct sx_port *pp,
	int card)
{
	struct tty *tp;
	unsigned char count;
	int i, x;
	static unsigned char sx_rxbuf[SX_BUFFERSIZE];	/* input staging area */

	tp = pp->sp_tty;
	DPRINT((pp, DBG_RECEIVE,
		"sx_rcv %x st %x sp %x\n",
		tp, tp->t_state, pp->sp_state));
	/*
	 * Let others know what we're doing.
	 */
	pp->sp_state |= SX_SS_IRCV;
	/*
	 * How many characters are waiting for us?
	 */
	count = sx_cd1865_in(sc, CD1865_RDCR|SX_EI);
	if (count == 0)			/* None?  Bail.                       */
		return;
	DPRINT((pp, DBG_RECEIVE, "sx_receive count %d\n", count));
	/*
	 * Pull the characters off the card into our local buffer, then
	 * process that.
	 */
	for (i = 0; i < count; i++)
		sx_rxbuf[i] = sx_cd1865_in(sc, CD1865_RDR|SX_EI);
	/*
	 * If we're not open and connected, bail.
	 */
	if (!(tp->t_state & TS_CONNECTED && tp->t_state & TS_ISOPEN)) {
		pp->sp_state &= ~SX_SS_IRCV;
		DPRINT((pp, DBG_RECEIVE, "sx_rcv not open\n"));
		return;
	}
	/*
	 * If the tty input buffers are blocked and we have an RTS pin,
	 * drop RTS and bail.
	 */
	if (tp->t_state & TS_TBLOCK) {
		if (!SX_DTRPIN(pp) && SX_IFLOW(pp)) {
			(void)sx_modem(tp, 0, SER_RTS);
			pp->sp_state |= SX_SS_ISTOP;
		}
		pp->sp_state &= ~SX_SS_IRCV;
		return;
	}
	if (tp->t_state & TS_CAN_BYPASS_L_RINT) {
		DPRINT((pp, DBG_RECEIVE, "sx_rcv BYPASS\n"));
		/*
		 * Avoid the grotesquely inefficient lineswitch routine
		 * (ttyinput) in "raw" mode. It usually takes about 450
		 * instructions (that's without canonical processing or
		 * echo!).  slinput is reasonably fast (usually 40
		 * instructions plus call overhead).
		 */
		if (tp->t_rawq.c_cc + count >= SX_I_HIGH_WATER &&
		    (tp->t_cflag & CRTS_IFLOW || tp->t_iflag & IXOFF) &&
		    !(tp->t_state & TS_TBLOCK)) {
			ttyblock(tp);
			DPRINT((pp, DBG_RECEIVE, "sx_rcv block\n"));
		    }
		tk_nin += count;
		tk_rawcc += count;
		tp->t_rawcc += count;

		pp->sp_delta_overflows +=
				b_to_q((char *)sx_rxbuf, count, &tp->t_rawq);
		ttwakeup(tp);
		/*
		 * If we were stopped and need to start again because of this
		 * receive, kick the output routine to get things going again.
		 */
		if (tp->t_state & TS_TTSTOP && (tp->t_iflag & IXANY ||
					tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
			tp->t_state &= ~TS_TTSTOP;
			tp->t_lflag &= ~FLUSHO;
			sx_start(tp);
		}
	} 
	else {
		DPRINT((pp, DBG_RECEIVE, "sx_rcv l_rint\n"));
		/*
		 * It'd be nice to not have to go through the function call
		 * overhead for each char here.  It'd be nice to block input
		 * it, saving a loop here and the call/return overhead.
		 */
		for (x = 0; x < count; x++) {
			i = sx_rxbuf[x];
			if (ttyld_rint(tp, i) == -1)
				pp->sp_delta_overflows++;
		}
	}
	pp->sp_state &= ~SX_SS_IRCV;
	DPRINT((pp, DBG_RECEIVE, "sx_rcv out\n"));
}



/*
 * sx_receive_exception()
 *	Handle receive exception request interrupt processing.
 *
 * Description:
 *	Handle a receive exception request interrupt from the CD1865.
 *	Possible exceptions include BREAK, overrun, receiver timeout
 *	and parity and frame errors.  We don't handle receiver timeout,
 *	we just complain.  The rest are passed to ttyinput().
 */
static void
sx_receive_exception(
	struct sx_softc *sc,
	struct sx_port *pp,
	int card)
{
	struct tty *tp;
	unsigned char st;
	int ch, isopen;
	
	tp = pp->sp_tty;
	/*
	 * Let others know what we're doing.
	 */
	pp->sp_state |= SX_SS_IRCVEXC;
	/*
	 * Check to see whether we should receive characters.
	 */
	if (tp->t_state & TS_CONNECTED &&
	    tp->t_state & TS_ISOPEN)
		isopen = 1;
	else
		isopen = 0;

	st = sx_cd1865_in(sc, CD1865_RCSR|SX_EI); /* Get the character status.*/
	ch = (int)sx_cd1865_in(sc, CD1865_RDR|SX_EI); /* Get the character.   */
	DPRINT((pp, DBG_RECEIVE_EXC,
		"sx_rexc %x st %x sp %x st 0x%x ch 0x%x ('%c')\n",
		tp, tp->t_state, pp->sp_state, st, ch, ch));
	/* If there's no status or the tty isn't open, bail. */
	if (!st || !isopen) {
		pp->sp_state &= ~SX_SS_IRCVEXC;
		DPRINT((pp, DBG_RECEIVE_EXC, "sx_rexc not open\n"));
		return;
	}
	if (st & CD1865_RCSR_TOUT)	/* Receiver timeout; just complain.   */
		printf("sx%d: port %d: Receiver timeout.\n", card, pp->sp_chan);
	else if (st & CD1865_RCSR_BREAK)
		ch |= TTY_BI;
	else if (st & CD1865_RCSR_PE) 
		ch |= TTY_PE;
	else if (st & CD1865_RCSR_FE) 
		ch |= TTY_FE;
	else if (st & CD1865_RCSR_OE)
		ch |= TTY_OE;
	ttyld_rint(tp, ch);
	pp->sp_state &= ~SX_SS_IRCVEXC;
}

/*
 * sx_intr()
 *	Field interrupts from the I/O8+.
 *
 * Description:
 * The interrupt handler polls ALL ports on ALL adapters each time
 * it is called.
 */
void
sx_intr(
	void *arg)
{
	struct sx_softc *sc;
	struct sx_port *pp = NULL;
	int card;
	unsigned char ack;

	sc = arg;

	DPRINT((0, arg == NULL ? DBG_POLL:DBG_INTR, "sx_intr\n"));
	if (in_interrupt)
		return;
	in_interrupt = 1;

	/*
	 * When we get an int we poll all the channels and do ALL pending
	 * work, not just the first one we find. This allows all cards to
	 * share the same vector.
	 *
	 * On the other hand, if we're sharing the vector with something
	 * that's not an I/O8+, we may be making extra work for ourselves.
	 */
	for (card = 0; card < sx_numunits; card++) {
		unsigned char st;

		sc = devclass_get_softc(sx_devclass, card);
		if (sc == NULL)
			continue;
		/*
		 * Check the Service Request Status Register to see who
		 * interrupted us and why.  May be a receive, transmit or
		 * modem-signal-change interrupt.  Reading the appropriate
		 * Request Acknowledge Register acknowledges the request and
		 * gives us the contents of the Global Service Vector Register,
		 * which in a daisy-chained configuration (not ours) uniquely
		 * identifies the particular CD1865 and gives us the request
		 * type.  We mask off the ID part and use the rest.
		 *
		 * From the CD1865 specs, it appears that only one request can
		 * happen at a time, but in testing it's pretty obvious that
		 * the specs lie.  Or perhaps we're just slow enough that the
		 * requests pile up.  Regardless, if we try to process more
		 * than one at a time without clearing the previous request
		 * (writing zero to EOIR) first, we hang the card.  Thus the
		 * "else if" logic here.
		 */
		while ((st = (sx_cd1865_in(sc, CD1865_SRSR|SX_EI)) &
							  CD1865_SRSR_REQint)) {
			/*
			 * Transmit request interrupt.
			 */
			if (st & CD1865_SRSR_TREQint) {
				ack = sx_cd1865_in(sc, CD1865_TRAR|SX_EI) &
							     CD1865_GIVR_ITMASK;
				pp = sx_int_port(sc, card);
				if (pp == NULL) /* Bad channel.               */
					goto skip;
				pp->sp_state |= SX_SS_INTR; /* In interrupt.  */
				if (ack == CD1865_GIVR_IT_TX)
					sx_transmit(sc, pp, card);
				else
					printf("sx%d: Bad transmit ack 0x%02x.\n",
					       card, ack);
			}
			/*
			 * Modem signal change request interrupt.
			 */
			else if (st & CD1865_SRSR_MREQint) {
				ack = sx_cd1865_in(sc, CD1865_MRAR|SX_EI) &
							     CD1865_GIVR_ITMASK;
				pp = sx_int_port(sc, card);
				if (pp == NULL) /* Bad channel.               */
					goto skip;
				pp->sp_state |= SX_SS_INTR; /* In interrupt.  */
				if (ack == CD1865_GIVR_IT_MODEM)
					sx_modem_state(sc, pp, card);
				else
					printf("sx%d: Bad modem ack 0x%02x.\n",
					       card, ack);
			}
			/*
			 * Receive request interrupt.
			 */
			else if (st & CD1865_SRSR_RREQint) {
				ack = sx_cd1865_in(sc, CD1865_RRAR|SX_EI) &
							     CD1865_GIVR_ITMASK;
				pp = sx_int_port(sc, card);
				if (pp == NULL) /* Bad channel.               */
					goto skip;
				pp->sp_state |= SX_SS_INTR; /* In interrupt.  */
				if (ack == CD1865_GIVR_IT_RCV)
					sx_receive(sc, pp, card);
				else if (ack == CD1865_GIVR_IT_REXC)
					sx_receive_exception(sc, pp, card);
				else
					printf("sx%d: Bad receive ack 0x%02x.\n",
					       card, ack);
			}
			/*
			 * None of the above; this is a "can't happen," but
			 * you never know...
			 */
			else {
				printf("sx%d: Bad service request 0x%02x.\n",
				       card, st);
			}
			pp->sp_state &= ~SX_SS_INTR;
skip:			sx_cd1865_out(sc, CD1865_EOIR|SX_EI, 0); /* EOI.      */
		}		/* while (st & CD1865_SRSR_REQint)            */
	}			/* for (card = 0; card < sx_numunits; card++) */
	in_interrupt = 0;
	DPRINT((0, arg == NULL ? DBG_POLL:DBG_INTR, "sx_intr out\n"));
}

/*
 * sx_start()
 *	Handle transmit and state-change stuff.
 *
 * Description:
 *	This is part of the line discipline processing; at various points in
 *	the line discipline he calls ttstart() which calls the oproc routine,
 *	which is this function.  We're called by the line discipline to start
 *	data transmission and to change signal states (for RTS flow control).
 *	We're also called by this driver to perform line-breaks and to actually
 *	do the data transmission.

 *	We can only fill the FIFO from interrupt since the card only makes it
 *	available to us during a service request such as TXRDY; this only
 *	happens at interrupt.
 *
 *	All paths through this code call ttwwakeup().
 */
static void
sx_start(
	struct tty *tp)
{
	struct sx_softc *sc;
	struct sx_port *pp;
	struct clist *qp;
	int s;
	int count = CD1865_TFIFOSZ;

	s = spltty();
	pp = tp->t_sc;
	qp = &tp->t_outq;
	DPRINT((pp, DBG_ENTRY|DBG_START,
		"sx_start %x st %x sp %x cc %d\n",
		tp, tp->t_state, pp->sp_state, qp->c_cc));

	/*
	 * If we're stopped, just wake up sleepers and get out.
	 */
	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP)) {
		ttwwakeup(tp);
		splx(s);
		DPRINT((pp, DBG_EXIT|DBG_START, "sx_start out\n", tp->t_state));
		return;
	}
	sc = pp->sp_sc;
	/*
	 * If we're not transmitting, we may have been called to crank up the
	 * transmitter and start things rolling or we may have been called to
	 * get a bit of tty state.  If the latter, handle it.  Either way, if
	 * we have data to transmit, turn on the transmit-ready interrupt,
	 * set the XMIT flag and we're done.  As soon as we allow interrupts
	 * the card will interrupt for the first chunk of data.  Note that
	 * we don't mark the tty as busy until we are actually sending data
	 * and then only if we have more than will fill the FIFO.  If there's
	 * no data to transmit, just handle the tty state.
	 */
	if (!SX_XMITTING(pp)) {
		/*
		 * If we were flow-controlled and input is no longer blocked,
		 * raise RTS if we can.
		 */
		if (SX_ISTOP(pp) && !(tp->t_state & TS_TBLOCK)) {
			if (!SX_DTRPIN(pp) && SX_IFLOW(pp))
				(void)sx_modem(tp, SER_RTS, 0);
			pp->sp_state &= ~SX_SS_ISTOP;
		}
		/*
		 * If input is blocked, drop RTS if we can and set our flag.
		 */
		if (tp->t_state & TS_TBLOCK) {
			if (!SX_DTRPIN(pp) && SX_IFLOW(pp))
				(void)sx_modem(tp, 0, SER_RTS);
			pp->sp_state |= SX_SS_ISTOP;
		}
		if ((qp->c_cc > 0 && !SX_OSTOP(pp)) || SX_DOBRK(pp)) {
			disable_intr();
			sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
			sx_cd1865_bis(sc, CD1865_IER|SX_EI, CD1865_IER_TXRDY);
			enable_intr();
			pp->sp_state |= SX_SS_XMIT;
		}
		ttwwakeup(tp);
		splx(s);
		DPRINT((pp, DBG_EXIT|DBG_START,
			"sx_start out B st %x sp %x cc %d\n",
			tp->t_state, pp->sp_state, qp->c_cc));
		return;
	}
	/*
	 * If we weren't called from an interrupt or it wasn't a transmit
	 * interrupt, we've done all we need to do.  Everything else is done
	 * in the transmit interrupt.
	 */
	if (!SX_INTR(pp) || !SX_IXMIT(pp)) {
		ttwwakeup(tp);
		splx(s);
		DPRINT((pp, DBG_EXIT|DBG_START, "sx_start out X\n"));
		return;
	}
	/*
	 * We're transmitting.  If the clist is empty and we don't have a break
	 * to send, turn off transmit-ready interrupts, and clear the XMIT
	 * flag.  Mark the tty as no longer busy, in case we haven't done
	 * that yet. A future call to sxwrite() with more characters will
	 * start up the process once more.
	 */
	if (qp->c_cc == 0 && !SX_DOBRK(pp)) {
		disable_intr();
/*		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);*/
		sx_cd1865_bic(sc, CD1865_IER|SX_EI, CD1865_IER_TXRDY);
		enable_intr();
		pp->sp_state &= ~SX_SS_XMIT;
		tp->t_state &= ~TS_BUSY;
		ttwwakeup(tp);
		splx(s);
		DPRINT((pp, DBG_EXIT|DBG_START,
			"sx_start out E st %x sp %x\n",
			tp->t_state, pp->sp_state));
		return;
	}
	disable_intr();
	/*
	 * If we have a BREAK state-change pending, handle it.  If we aren't
	 * sending a break, start one.  If we are, turn it off.
	 */
	if (SX_DOBRK(pp)) {
		count -= 2;		/* Account for escape chars in FIFO.  */
		if (SX_BREAK(pp)) {	/* Doing break, stop it.              */
			sx_cd1865_out(sc, CD1865_TDR, CD1865_C_ESC);
			sx_cd1865_out(sc, CD1865_TDR, CD1865_C_EBRK);
			sx_cd1865_etcmode(sc, SX_EI, pp->sp_chan, 0);
			pp->sp_state &= ~SX_SS_BREAK;
		}
		else {			/* Start doing break.                 */
			sx_cd1865_etcmode(sc, SX_EI, pp->sp_chan, 1);
			sx_cd1865_out(sc, CD1865_TDR, CD1865_C_ESC);
			sx_cd1865_out(sc, CD1865_TDR, CD1865_C_SBRK);
			pp->sp_state |= SX_SS_BREAK;
		}
		pp->sp_state &= ~SX_SS_DOBRK;
	}
	/*
	 * We've still got data in the clist, fill the channel's FIFO.  The
	 * CD1865 only gives us access to the FIFO during a transmit ready
	 * request [interrupt] for this channel.
	 */
	while (qp->c_cc > 0 && count-- >= 0) {
		register unsigned char ch, *cp;
		int nch;

		ch = (char)getc(qp);
		/*
		 * If we're doing a break we're in ETC mode, so we need to
		 * double any NULs in the stream.
		 */
		if (SX_BREAK(pp)) {	/* Doing break, in ETC mode.          */
			if (ch == '\0') { /* NUL?  Double it.                 */
				sx_cd1865_out(sc, CD1865_TDR, ch);
				count--;
			}
			/*
			 * Peek the next character; if it's a NUL, we need
			 * to escape it, but we can't if we're out of FIFO.
			 * We'll do it on the next pass and leave the FIFO
			 * incompletely filled.
			 */
			if (qp->c_cc > 0) {
				cp = qp->c_cf;
				cp = nextc(qp, cp, &nch);
				if (nch == '\0' && count < 1)
					count = -1;
			}
		}
		sx_cd1865_out(sc, CD1865_TDR, ch);
	}
	enable_intr();
	/*
	 * If we still have data to transmit, mark the tty busy for the
	 * line discipline.
	 */
	if (qp->c_cc > 0)
		tp->t_state |= TS_BUSY;
	else
		tp->t_state &= ~TS_BUSY;
	/* Wake up sleepers if necessary. */
	ttwwakeup(tp);
	splx(s);
	DPRINT((pp, DBG_EXIT|DBG_START,
		"sx_start out R %d/%d\n",
		count, qp->c_cc));
}

/*
 * Stop output on a line. called at spltty();
 */
void
sx_stop(
	struct tty *tp,
	int rw)
{
	struct sx_softc *sc;
	struct sx_port *pp;
	int s;

	pp = tp->t_sc;
	sc = pp->sp_sc;
	DPRINT((pp, DBG_ENTRY|DBG_STOP, "sx_stop(%x,%x)\n", tp, rw));

	s = spltty();
	/* XXX: must check (rw & FWRITE | FREAD) etc flushing... */
	if (rw & FWRITE) {
		disable_intr();
		sx_cd1865_out(sc, CD1865_CAR|SX_EI, pp->sp_chan);
		sx_cd1865_bic(sc, CD1865_IER|SX_EI, CD1865_IER_TXRDY);
		sx_cd1865_wait_CCR(sc, SX_EI);	/* Wait for CCR to go idle.   */
		sx_cd1865_out(sc, CD1865_CCR|SX_EI, CD1865_CCR_TXDIS);
		sx_cd1865_wait_CCR(sc, SX_EI);
		enable_intr();
	/* what level are we meant to be flushing anyway? */
		if (tp->t_state & TS_BUSY) {
			if ((tp->t_state & TS_TTSTOP) == 0)
				tp->t_state |= TS_FLUSH;
			tp->t_state &= ~TS_BUSY;
			ttwwakeup(tp);
		}
	}
	/*
	 * Nothing to do for FREAD.
	 */
	splx(s);
}

#ifdef	SX_DEBUG

void
sx_dprintf(
	struct sx_port *pp,
	int flags,
	const char *fmt, ...)
{
	static char *logbuf = NULL;
	static char *linebuf = NULL;
	static char *logptr;
	char *lbuf;
	int n, m;
	va_list ap;

	if (logbuf == NULL) {
		logbuf = (char *)malloc(1024*1024, M_DEVBUF, M_WAITOK);
		linebuf = (char *)malloc(256, M_DEVBUF, M_WAITOK);
		logptr = logbuf;
	}
	lbuf = linebuf;
	n = 0;
	if ((pp == NULL && (sx_debug&flags)) ||
	    (pp != NULL && ((pp->sp_debug&flags) || (sx_debug&flags)))) {
		if (pp != NULL &&
		    pp->sp_tty != NULL) {
			n = snprintf(linebuf, 256, "sx%d(%d): ",
				(int)pp->sp_sc->sc_unit, (int)pp->sp_chan);
			if (n > 256)
				n = 256;
			lbuf += n;
		}
		m = n;
		va_start(ap, fmt);
		n = vsnprintf(lbuf, 256 - m, fmt, ap);
		va_end(ap);
		if (n > 256 - m)
			n = 256 - m;
		n += m;
		if (logptr + n + 1 > logbuf + (1024 * 1024)) {
			bzero(logptr, logbuf + (1024 * 1024) - logptr);
			logptr = logbuf;
		}
		bcopy(linebuf, logptr, n);
		logptr += n;
		*logptr = '\0';
		if (sx_debug & DBG_PRINTF)
			printf("%s", linebuf);
	}
}

#endif	/* DEBUG */
