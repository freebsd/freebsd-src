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
 *	$Id: com.c,v 1.10 1994/05/30 03:14:13 ache Exp $
 */

#include "com.h"
#if NCOM > 0
/*
 * COM driver, based on HP dca driver
 * uses National Semiconductor NS16450/NS16550AF UART
 */
#include "param.h"
#include "systm.h"
#include "ioctl.h"
#include "tty.h"
#include "proc.h"
#include "user.h"
#include "conf.h"
#include "file.h"
#include "uio.h"
#include "kernel.h"
#include "syslog.h"

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/comreg.h"
#include "i386/isa/ic/ns16550.h"
#define cominor(d)

static int commctl(int /*dev_t*/, int, int);
static int comprobe();
static int comattach();
void comintr(int);
static void comstart(struct tty *);
static int comparam(struct tty *, struct termios *);

static void comeint(int, int, int);
static void commint(int, int);
static void cominit(int, int);

struct	isa_driver comdriver = {
	comprobe, comattach, "com"
};

int	comsoftCAR;
int	com_active;
int	com_hasfifo;
int	ncom = NCOM;
#ifdef COMCONSOLE
int	comconsole = COMCONSOLE;
#else
int	comconsole = -1;
#endif
int	comconsinit;
int	comdefaultrate = TTYDEF_SPEED;
int	commajor;
short com_addr[NCOM];
struct	tty *com_tty[NCOM];

struct speedtab comspeedtab[] = {
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
	-1,	-1
};

extern	struct tty *constty;
#ifdef KGDB
#include "machine/remote-sl.h"

extern int kgdb_dev;
extern int kgdb_rate;
extern int kgdb_debug_init;
#endif

#define	UNIT(x)		(minor(x))

int
comprobe(dev)
struct isa_device *dev;
{
	/* force access to id reg */
	outb(dev->id_iobase+com_cfcr, 0);
	outb(dev->id_iobase+com_iir, 0);
	DELAY(100);
	if ((inb(dev->id_iobase+com_iir) & 0x38) == 0)
		return(IO_COMSIZE);
	return(0);
}


int
comattach(isdp)
struct isa_device *isdp;
{
	struct	tty	*tp;
	u_char		unit;
	int		port = isdp->id_iobase;

	unit = isdp->id_unit;
	if (unit == comconsole)
		DELAY(1000);
	com_addr[unit] = port;
	com_active |= 1 << unit;
	comsoftCAR |= 1 << unit;	/* XXX */

	/* look for a NS 16550AF UART with FIFOs */
	outb(port+com_fifo, FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_4);
	DELAY(100);
	if ((inb(port+com_iir) & IIR_FIFO_MASK) == IIR_FIFO_MASK) {
		com_hasfifo |= 1 << unit;
		printf("com%d: fifo\n", unit);
	}

	outb(port+com_ier, 0);
	outb(port+com_mcr, 0 | MCR_IENABLE);
#ifdef KGDB
	if (kgdb_dev == makedev(commajor, unit)) {
		if (comconsole == unit)
			kgdb_dev = -1;	/* can't debug over console port */
		else {
			(void) cominit(unit, kgdb_rate);
			if (kgdb_debug_init) {
				/*
				 * Print prefix of device name,
				 * let kgdb_connect print the rest.
				 */
				printf("com%d: ", unit);
				kgdb_connect(1);
			} else
				printf("com%d: kgdb enabled\n", unit);
		}
	}
#endif
	/*
	 * Need to reset baud rate, etc. of next print so reset comconsinit.
	 * Also make sure console is always "hardwired"
	 */
	if (unit == comconsole) {
		comconsinit = 0;
		comsoftCAR |= (1 << unit);
	}
	return (1);
}

/* ARGSUSED */
int
comopen(int /*dev_t*/ dev, int flag, int mode, struct proc *p)
{
	register struct tty *tp;
	register int unit;
	int error = 0;
 
	unit = UNIT(dev);
	if (unit >= NCOM || (com_active & (1 << unit)) == 0)
		return (ENXIO);
	tp = com_tty[unit] = ttymalloc(com_tty[unit]);
	tp->t_oproc = comstart;
	tp->t_param = comparam;
	tp->t_dev = dev;
	if ((tp->t_state & TS_ISOPEN) == 0) {
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_iflag = TTYDEF_IFLAG;
			tp->t_oflag = TTYDEF_OFLAG;
			tp->t_cflag = TTYDEF_CFLAG;
			tp->t_lflag = TTYDEF_LFLAG;
			tp->t_ispeed = tp->t_ospeed = comdefaultrate;
		}
		comparam(tp, &tp->t_termios);
		ttsetwater(tp);
	} else if (tp->t_state&TS_XCLUDE && p->p_ucred->cr_uid != 0)
		return (EBUSY);
	(void) spltty();
	(void) commctl(dev, MCR_DTR | MCR_RTS, DMSET);
	if ((comsoftCAR & (1 << unit)) || (commctl(dev, 0, DMGET) & MSR_DCD))
		tp->t_state |= TS_CARR_ON;
	while ((flag&O_NONBLOCK) == 0 && (tp->t_cflag&CLOCAL) == 0 &&
	       (tp->t_state & TS_CARR_ON) == 0) {
		if (error = tsleep(tp, TSA_CARR_ON(tp), TTIPRI | PCATCH,
				   "comdcd", 0))
			break;
	}
	(void) spl0();
	if (error == 0)
		error = (*linesw[tp->t_line].l_open)(dev, tp, 0);
	return (error);
}
 
/*ARGSUSED*/
int
comclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct tty *tp;
	register com;
	register int unit;
 
	unit = UNIT(dev);
	com = com_addr[unit];
	tp = com_tty[unit];
	(*linesw[tp->t_line].l_close)(tp, flag);
	outb(com+com_cfcr, inb(com+com_cfcr) & ~CFCR_SBREAK);
#ifdef KGDB
	/* do not disable interrupts if debugging */
	if (kgdb_dev != makedev(commajor, unit))
#endif
	outb(com+com_ier, 0);
	if (tp->t_cflag&HUPCL || (tp->t_state&TS_ISOPEN) == 0)
		(void) commctl(dev, 0, DMSET);
	ttyclose(tp);
#ifdef broken /* session holds a ref to the tty; can't deallocate */
	ttyfree(tp);
	com_tty[unit] = (struct tty *)NULL;
#endif
	return (0);


	return(0);
}
 
int
comread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp = com_tty[UNIT(dev)];
 
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}
 
int
comwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	int unit = UNIT(dev);
	register struct tty *tp = com_tty[unit];
 
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
 
void
comintr(unit)
	register int unit;
{
	register com;
	register u_char code;
	register struct tty *tp;

	com = com_addr[unit];
	while (1) {
		code = inb(com+com_iir);
		switch (code & IIR_IMASK) {
		case IIR_NOPEND:
			return;
		case IIR_RXTOUT:
		case IIR_RXRDY:
			tp = com_tty[unit];
/*
 * Process received bytes.  Inline for speed...
 */
#ifdef KGDB
#define	RCVBYTE() \
			code = inb(com+com_data); \
			if ((tp->t_state & TS_ISOPEN) == 0) { \
				if (kgdb_dev == makedev(commajor, unit) && \
				    code == FRAME_END) \
					kgdb_connect(0); /* trap into kgdb */ \
			} else \
				(*linesw[tp->t_line].l_rint)(code, tp)
#else
#define	RCVBYTE() \
			code = inb(com+com_data); \
			if (tp->t_state & TS_ISOPEN) \
				(*linesw[tp->t_line].l_rint)(code, tp)
#endif

			RCVBYTE();

			if (com_hasfifo & (1 << unit))
				while ((code = inb(com+com_lsr)) & LSR_RCV_MASK) {
					if (code == LSR_RXRDY) {
						RCVBYTE();
					} else
						comeint(unit, code, com);
				}
			break;
		case IIR_TXRDY:
			tp = com_tty[unit];
			tp->t_state &=~ (TS_BUSY|TS_FLUSH);
			if (tp->t_line)
				(*linesw[tp->t_line].l_start)(tp);
			else
				comstart(tp);
			break;
		case IIR_RLS:
			comeint(unit, inb(com+com_lsr), com);
			break;
		default:
			if (code & IIR_NOPEND)
				return;
			log(LOG_WARNING, "com%d: weird interrupt: 0x%x\n",
			    unit, code);
			/* fall through */
		case IIR_MLSC:
			commint(unit, com);
			break;
		}
	}
}

static void
comeint(unit, stat, com)
	register int unit, stat;
	register int com;
{
	register struct tty *tp;
	register int c;

	tp = com_tty[unit];
	c = inb(com+com_data);
	if ((tp->t_state & TS_ISOPEN) == 0) {
#ifdef KGDB
		/* we don't care about parity errors */
		if (((stat & (LSR_BI|LSR_FE|LSR_PE)) == LSR_PE) &&
		    kgdb_dev == makedev(commajor, unit) && c == FRAME_END)
			kgdb_connect(0); /* trap into kgdb */
#endif
		return;
	}
	if (stat & (LSR_BI | LSR_FE))
		c |= TTY_FE;
	else if (stat & LSR_PE)
		c |= TTY_PE;
	else if (stat & LSR_OE) {			/* 30 Aug 92*/
		c |= TTY_PE;	/* Ought to have it's own define... */
		log(LOG_WARNING, "com%d: silo overflow\n", unit);
	}
	(*linesw[tp->t_line].l_rint)(c, tp);
}

static void
commint(unit, com)
	register int unit;
	register int com;
{
	register struct tty *tp;
	register int stat;

	tp = com_tty[unit];
	stat = inb(com+com_msr);
	if ((stat & MSR_DDCD) && (comsoftCAR & (1 << unit)) == 0) {
		if (stat & MSR_DCD)
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		else if ((*linesw[tp->t_line].l_modem)(tp, 0) == 0)
			outb(com+com_mcr,
				inb(com+com_mcr) & ~(MCR_DTR | MCR_RTS) | MCR_IENABLE);
	} else if ((stat & MSR_DCTS) && (tp->t_state & TS_ISOPEN) &&
		   (tp->t_cflag & CRTSCTS)) {
		/* the line is up and we want to do rts/cts flow control */
		if (stat & MSR_CTS) {
			tp->t_state &=~ TS_TTSTOP;
			ttstart(tp);
		} else
			tp->t_state |= TS_TTSTOP;
	}
}

int
comioctl(dev, cmd, data, flag)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
{
	register struct tty *tp;
	register int unit = UNIT(dev);
	register com;
	register int error;
 
	tp = com_tty[unit];
	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag);
	if (error >= 0)
		return (error);
	error = ttioctl(tp, cmd, data, flag);
	if (error >= 0)
		return (error);

	com = com_addr[unit];
	switch (cmd) {

	case TIOCSBRK:
		outb(com+com_cfcr, inb(com+com_cfcr) | CFCR_SBREAK);
		break;

	case TIOCCBRK:
		outb(com+com_cfcr, inb(com+com_cfcr) & ~CFCR_SBREAK);
		break;

	case TIOCSDTR:
		(void) commctl(dev, MCR_DTR | MCR_RTS, DMBIS);
		break;

	case TIOCCDTR:
		(void) commctl(dev, MCR_DTR | MCR_RTS, DMBIC);
		break;

	case TIOCMSET:
		(void) commctl(dev, *(int *)data, DMSET);
		break;

	case TIOCMBIS:
		(void) commctl(dev, *(int *)data, DMBIS);
		break;

	case TIOCMBIC:
		(void) commctl(dev, *(int *)data, DMBIC);
		break;

	case TIOCMGET:
		*(int *)data = commctl(dev, 0, DMGET);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

static int
comparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register int com;
	register int cfcr = 0, cflag = t->c_cflag;
	int unit = UNIT(tp->t_dev);
	int ospeed = ttspeedtab(t->c_ospeed, comspeedtab);
 
	/* check requested parameters */
        if (ospeed < 0 || (t->c_ispeed && t->c_ispeed != t->c_ospeed))
                return(EINVAL);
        /* and copy to tty */
        tp->t_ispeed = t->c_ispeed;
        tp->t_ospeed = t->c_ospeed;
        tp->t_cflag = cflag;

	com = com_addr[unit];
	outb(com+com_ier, IER_ERXRDY | IER_ETXRDY | IER_ERLS /*| IER_EMSC*/);
	if (ospeed == 0) {
		(void) commctl(unit, 0, DMSET);	/* hang up line */
		return(0);
	}
	outb(com+com_cfcr, inb(com+com_cfcr) | CFCR_DLAB);
	outb(com+com_data, ospeed & 0xFF);
	outb(com+com_ier, ospeed >> 8);
	switch (cflag&CSIZE) {
	case CS5:
		cfcr = CFCR_5BITS; break;
	case CS6:
		cfcr = CFCR_6BITS; break;
	case CS7:
		cfcr = CFCR_7BITS; break;
	case CS8:
		cfcr = CFCR_8BITS; break;
	}
	if (cflag&PARENB) {
		cfcr |= CFCR_PENAB;
		if ((cflag&PARODD) == 0)
			cfcr |= CFCR_PEVEN;
	}
	if (cflag&CSTOPB)
		cfcr |= CFCR_STOPB;
	outb(com+com_cfcr, cfcr);

	if (com_hasfifo & (1 << unit))
		outb(com+com_fifo, FIFO_ENABLE | FIFO_TRIGGER_4);

	return(0);
}
 
void
comstart(tp)
	register struct tty *tp;
{
	register com;
	int s, unit, c;
 
	unit = UNIT(tp->t_dev);
	com = com_addr[unit];
	s = spltty();
	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;
	if (tp->t_state & (TS_SO_OCOMPLETE | TS_SO_OLOWAT) || tp->t_wsel)
		ttwwakeup(tp);
	if (RB_LEN(tp->t_out) == 0)
		goto out;
	if (inb(com+com_lsr) & LSR_TXRDY) {
		c = getc(tp->t_out);
		tp->t_state |= TS_BUSY;
		outb(com+com_data, c);
		if (com_hasfifo & (1 << unit))
			for (c = 1; c < 16 && RB_LEN(tp->t_out); ++c)
				outb(com+com_data, getc(tp->t_out));
	}
out:
	splx(s);
}
 
/*
 * Stop output on a line.
 */
/*ARGSUSED*/
void
comstop(tp, flag)
	register struct tty *tp;
	int flag;
{
	register int s;

	s = spltty();
	if (tp->t_state & TS_BUSY) {
		if ((tp->t_state&TS_TTSTOP)==0)
			tp->t_state |= TS_FLUSH;
	}
	splx(s);
}
 
static int
commctl(dev, bits, how)
	dev_t dev;
	int bits, how;
{
	register com;
	register int unit;
	int s;

	unit = UNIT(dev);
	com = com_addr[unit];
	s = spltty();
	switch (how) {

	case DMSET:
		outb(com+com_mcr, bits | MCR_IENABLE);
		break;

	case DMBIS:
		outb(com+com_mcr, inb(com+com_mcr) | bits | MCR_IENABLE);
		break;

	case DMBIC:
		outb(com+com_mcr, inb(com+com_mcr) & ~bits | MCR_IENABLE);
		break;

	case DMGET:
		bits = inb(com+com_msr);
		break;
	}
	(void) splx(s);
	return(bits);
}

/*
 * Following are all routines needed for COM to act as console
 */
#include "i386/i386/cons.h"

void
comcnprobe(cp)
	struct consdev *cp;
{
	int unit;

	/* locate the major number */
	for (commajor = 0; commajor < nchrdev; commajor++)
		if (cdevsw[commajor].d_open == comopen)
			break;

	/* XXX: ick */
	unit = CONUNIT;
	com_addr[CONUNIT] = CONADDR;

	/* make sure hardware exists?  XXX */

	/* initialize required fields */
	cp->cn_dev = makedev(commajor, unit);
	cp->cn_tp = com_tty[unit];
#ifdef	COMCONSOLE
	cp->cn_pri = CN_REMOTE;		/* Force a serial port console */
#else
	cp->cn_pri = CN_NORMAL;
#endif
}

void
comcninit(cp)
	struct consdev *cp;
{
	int unit = UNIT(cp->cn_dev);

	cominit(unit, comdefaultrate);
	comconsole = unit;
	comconsinit = 1;
}

static void
cominit(unit, rate)
	int unit, rate;
{
	register int com;
	int s;
	short stat;

#ifdef lint
	stat = unit; if (stat) return;
#endif
	com = com_addr[unit];
	s = splhigh();
	outb(com+com_cfcr, CFCR_DLAB);
	rate = ttspeedtab(comdefaultrate, comspeedtab);
	outb(com+com_data, rate & 0xFF);
	outb(com+com_ier, rate >> 8);
	outb(com+com_cfcr, CFCR_8BITS);
	outb(com+com_ier, IER_ERXRDY | IER_ETXRDY);
	outb(com+com_fifo, FIFO_ENABLE|FIFO_RCV_RST|FIFO_XMT_RST|FIFO_TRIGGER_4);
	stat = inb(com+com_iir);
	splx(s);
}

int
comcngetc(dev)
	dev_t dev;
{
	register com = com_addr[UNIT(dev)];
	short stat;
	int c, s;

#ifdef lint
	stat = dev; if (stat) return(0);
#endif
	s = splhigh();
	while (((stat = inb(com+com_lsr)) & LSR_RXRDY) == 0)
		;
	c = inb(com+com_data);
	stat = inb(com+com_iir);
	splx(s);
	return(c);
}

/*
 * Console kernel output character routine.
 */
void
comcnputc(dev, c)
	dev_t dev;
	register int c;
{
	register com = com_addr[UNIT(dev)];
	register int timo;
	short stat;
	int s = splhigh();

#ifdef lint
	stat = dev; if (stat) return;
#endif
#ifdef KGDB
	if (dev != kgdb_dev)
#endif
	if (comconsinit == 0) {
		(void) cominit(UNIT(dev), comdefaultrate);
		comconsinit = 1;
	}
	/* wait for any pending transmission to finish */
	timo = 50000;
	while (((stat = inb(com+com_lsr)) & LSR_TXRDY) == 0 && --timo)
		;
	outb(com+com_data, c);
	/* wait for this transmission to complete */
	timo = 1500000;
	while (((stat = inb(com+com_lsr)) & LSR_TXRDY) == 0 && --timo)
		;
	/* clear any interrupts generated by this transmission */
	stat = inb(com+com_iir);
	splx(s);
}
#endif
