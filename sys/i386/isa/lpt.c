/*
 * Copyright (c) 1990 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: unknown origin, 386BSD 0.1
 *	$Id: lpt.c,v 1.6 1993/11/25 01:31:41 wollman Exp $
 */

/*
 * Device Driver for AT parallel printer port
 * Written by William Jolitz 12/18/90
 */

#include "lpt.h"
#if NLPT > 0

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "user.h"
#include "buf.h"
#include "kernel.h"
#include "ioctl.h"
#include "tty.h"
#include "uio.h"

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/lptreg.h"

#define	LPINITRDY	4	/* wait up to 4 seconds for a ready */
#define	LPTOUTTIME	4	/* wait up to 4 seconds for a ready */
#define	LPPRI		(PZERO+8)
#define	BUFSIZE		1024

#ifndef DEBUG
#define lprintf (void)
#else
#define lprintf		if (lptflag) printf
#endif

void lptout();
#ifdef DEBUG
int lptflag = 1;
#endif

int 	lptprobe(), lptattach();
void	lptintr();

struct	isa_driver lptdriver = {
	lptprobe, lptattach, "lpt"
};

#define	LPTUNIT(s)	((s)&0x03)
#define	LPTFLAGS(s)	((s)&0xfc)

struct lpt_softc {
	short	sc_port;
	short	sc_state;
	/* default case: negative prime, negative ack, handshake strobe,
	   prime once */
	u_char	sc_control;
	u_char	sc_flags;
#define LP_POS_INIT	0x04	/* if we are a postive init signal */
#define LP_POS_ACK	0x08	/* if we are a positive going ack */
#define LP_NO_PRIME	0x10	/* don't prime the printer at all */
#define LP_PRIMEOPEN	0x20	/* prime on every open */
#define LP_AUTOLF	0x40	/* tell printer to do an automatic lf */
#define LP_BYPASS	0x80	/* bypass  printer ready checks */
	struct	buf *sc_inbuf;
	short	sc_xfercnt ;
	char	sc_primed;
	char	*sc_cp ;
} lpt_sc[NLPT] ;

/* bits for state */
#define	OPEN		(1<<0)	/* device is open */
#define	ASLP		(1<<1)	/* awaiting draining of printer */
#define	ERROR		(1<<2)	/* error was received from printer */
#define	OBUSY		(1<<3)	/* printer is busy doing output */
#define LPTOUT		(1<<4)	/* timeout while not selected	*/
#define TOUT		(1<<5)	/* timeout while not selected	*/
#define INIT		(1<<6)	/* waiting to initialize for open */

/*
 * Internal routine to lptprobe to do port tests of one byte value
 */
int
lpt_port_test(port, data, mask)
	short port;
	u_char data;
	u_char mask;
{
	int	temp, timeout;

	data = data & mask;
	outb(port, data);
	timeout = 100;
	do
		temp = inb(port) & mask;
	while (temp != data && --timeout);
	lprintf("Port 0x%x\tout=%x\tin=%x\n", port, data, temp);
	return (temp == data);
}

/*
 * New lptprobe routine written by Rodney W. Grimes, 3/25/1993
 *
 * Logic:
 *	1) You should be able to write to and read back the same value
 *	   to the data port.  Do an alternating zeros, alternating ones,
 *	   walking zero, and walking one test to check for stuck bits.
 *
 *	2) You should be able to write to and read back the same value
 *	   to the control port lower 5 bits, the upper 3 bits are reserved
 *	   per the IBM PC technical reference manauls and different boards
 *	   do different things with them.  Do an alternating zeros, alternating
 *	   ones, walking zero, and walking one test to check for stuck bits.
 *
 *	   Some printers drag the strobe line down when the are powered off
 * 	   so this bit has been masked out of the control port test.
 *
 *	   XXX Some printers may not like a fast pulse on init or strobe, I
 *	   don't know at this point, if that becomes a problem these bits
 *	   should be turned off in the mask byte for the control port test.
 *
 *	3) Set the data and control ports to a value of 0
 */

int
lptprobe(dvp)
	struct isa_device *dvp;
{
	int	status;
	short	port;
	u_char	data;
	u_char	mask;
	int	i;

	status = IO_LPTSIZE;

	port = dvp->id_iobase + lpt_data;
	mask = 0xff;
	while (mask != 0)
	{
		data = 0x55;				/* Alternating zeros */
		if (!lpt_port_test(port, data, mask)) status = 0;

		data = 0xaa;				/* Alternating ones */
		if (!lpt_port_test(port, data, mask)) status = 0;

		for (i = 0; i < 8; i++)			/* Walking zero */
			{
			data = ~(1 << i);
			if (!lpt_port_test(port, data, mask)) status = 0;
			}

		for (i = 0; i < 8; i++)			/* Walking one */
			{
			data = (1 << i);
			if (!lpt_port_test(port, data, mask)) status = 0;
			}

		if (port == dvp->id_iobase + lpt_data)
			{
			port = dvp->id_iobase + lpt_control;
			mask = 0x1e;
			}
		else
			mask = 0;
		}
	outb(dvp->id_iobase+lpt_data, 0);
	outb(dvp->id_iobase+lpt_control, 0);
	return (status);
}

int
lptattach(isdp)
	struct isa_device *isdp;
{
	struct	lpt_softc	*sc;

	sc = lpt_sc + isdp->id_unit;
	sc->sc_port = isdp->id_iobase;
	outb(sc->sc_port+lpt_control, LPC_NINIT);
	return (1);
}

/*
 * lptopen -- reset the printer, then wait until it's selected and not busy.
 */

int
lptopen(dev, flag)
	dev_t dev;
	int flag;
{
	struct lpt_softc *sc;
	int s;
	int trys, port;
	u_int unit = LPTUNIT(minor(dev));
 
	if (unit >= NLPT)
		return (ENXIO);
	sc = lpt_sc + unit;
	if (sc->sc_state) {
lprintf("lp: still open\n") ;
lprintf("still open %x\n", sc->sc_state);
		return(EBUSY);
	} else	sc->sc_state |= INIT;

	s = spltty();
	sc->sc_flags = LPTFLAGS(minor(dev));
lprintf("lp flags 0x%x\n", sc->sc_flags);
	port = sc->sc_port;

	/* init printer */
	if((sc->sc_flags & LP_NO_PRIME) == 0) {
		if((sc->sc_flags & LP_PRIMEOPEN) || sc->sc_primed == 0) {
			outb(port+lpt_control, 0);
			sc->sc_primed++;
			DELAY(500);
		}
	}
	outb(port+lpt_control, LPC_SEL|LPC_NINIT);

	/* wait till ready (printer running diagnostics) */
	trys = 0;
	do {
		/* ran out of waiting for the printer */
		if (trys++ >= LPINITRDY*4) {
			splx(s);
			sc->sc_state = 0;
lprintf ("status %x\n", inb(port+lpt_status) );
			return (EBUSY);
		}

		/* wait 1/4 second, give up if we get a signal */
		if (tsleep ((caddr_t)sc, LPPRI|PCATCH, "lptinit", hz/4)
                    != EWOULDBLOCK) {
			sc->sc_state = 0;
			splx(s);
			return (EBUSY);
		}
		
		/* is printer online and ready for output */
	} while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
			(LPS_SEL|LPS_NBSY|LPS_NERR));

	if(sc->sc_flags&LP_AUTOLF) {
		outb(port+lpt_control, LPC_SEL|LPC_NINIT|LPC_ENA|LPC_AUTOL);
		sc->sc_control = LPC_SEL|LPC_NINIT|LPC_ENA|LPC_AUTOL;
	} else {
		outb(port+lpt_control, LPC_SEL|LPC_NINIT|LPC_ENA);
		sc->sc_control = LPC_SEL|LPC_NINIT|LPC_ENA;
	}

	sc->sc_state = OPEN | TOUT;
	sc->sc_inbuf = geteblk(BUFSIZE);
	sc->sc_xfercnt = 0;
	splx(s);
	timeout (lptout, (caddr_t)sc, hz/2);
lprintf("opened.\n");
	return(0);
}

void
lptout (sc)
	struct lpt_softc *sc;
{	int pl;

lprintf ("T %x ", inb(sc->sc_port+lpt_status));
	if (sc->sc_state&OPEN)
		timeout (lptout, (caddr_t)sc, hz/2);
	else	sc->sc_state &= ~TOUT;

	if (sc->sc_state & ERROR)
		sc->sc_state &= ~ERROR;

	/*
	 * Avoid possible hangs do to missed interrupts
	 */
	if (sc->sc_xfercnt) {
		pl = spltty();
		lptintr(sc - lpt_sc);
		splx(pl);
	} else {
		sc->sc_state &= ~OBUSY;
		wakeup((caddr_t)sc);
	}
}

/*
 * lptclose -- close the device, free the local line buffer.
 */

int
lptclose(dev, flag)
	dev_t dev;
	int flag;
{
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));
	int port = sc->sc_port;

	sc->sc_state &= ~OPEN;
	while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
			(LPS_SEL|LPS_NBSY|LPS_NERR) || sc->sc_xfercnt)
		/* wait 1/4 second, give up if we get a signal */
		if (tsleep ((caddr_t)sc, LPPRI|PCATCH, "lpclose", hz)
                    != EWOULDBLOCK)
			break;

	sc->sc_state = 0;
	sc->sc_xfercnt = 0;
	outb(sc->sc_port+lpt_control, LPC_NINIT);
	brelse(sc->sc_inbuf);
lprintf("closed.\n");
	return(0);
}

/* 
 * lptwrite --copy a line from user space to a local buffer, then call 
 * putc to get the chars moved to the output queue.
 */

int
lptwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	register unsigned n;
	int pl, err;
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));

	while (n = MIN(BUFSIZE, uio->uio_resid)) {
		sc->sc_cp = sc->sc_inbuf->b_un.b_addr ;
		uiomove(sc->sc_cp, n, uio);
		sc->sc_xfercnt = n ;
		while (sc->sc_xfercnt > 0) {
			/* if the printer is ready for a char, give it one */
			if ((sc->sc_state & OBUSY) == 0){
lprintf("\nC %d. ", sc->sc_xfercnt);
				pl = spltty();
				lptintr(sc - lpt_sc);
				(void) splx(pl);
			}
			if (sc->sc_state & OBUSY) {
lprintf("W ");
				if (err = tsleep ((caddr_t)sc, LPPRI|PCATCH,
                                                  "lpwrite", 0))
					return(err);
			}
		}
	}
	return(0);
}

/*
 * lptintr -- handle printer interrupts which occur when the printer is
 * ready to accept another char.
 */

void
lptintr(unit)
	int unit;
{
	struct lpt_softc *sc = lpt_sc + unit;
	int port = sc->sc_port,sts,lpc;
	u_char ctrl = sc->sc_control & ~LPC_ENA;

	sts = inb(port+lpt_status);

	/* disable interupts */
	outb(port+lpt_control, ctrl);

	sc->sc_state |= OBUSY;
	while(sc->sc_xfercnt > 0) {
		lpc = 50;
		/* wait for printer ready for output */
		while(((inb(port+lpt_status)&(LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR))
			!=(LPS_SEL|LPS_NBSY|LPS_NERR)) && --lpc);
		if(lpc == 0) break;

		/* send char */
		outb(port+lpt_data, *sc->sc_cp++) ; sc->sc_xfercnt-- ;
		outb(port+lpt_control, ctrl|LPC_STB);
		outb(port+lpt_control, ctrl);
	}

	/* any more bytes for the printer? */
	if(sc->sc_xfercnt == 0) {
		/* none, wake up the top half to get more */
		sc->sc_state &= ~OBUSY;
		wakeup((caddr_t)sc);
	}

	/* reenable interupts */
	outb(port+lpt_control, sc->sc_control);
}

int
lptioctl(dev, cmd, data, flag)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
{
	int	error;

	error = 0;
	switch (cmd) {
#ifdef THISISASAMPLE
	case XXX:
		dothis; andthis; andthat;
		error=x;
		break;
#endif /* THISISASAMPLE */
	default:
		error = ENODEV;
	}

	return(error);
}

#endif	/* NLPT */
