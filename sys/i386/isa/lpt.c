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
 *	$Id: lpt.c,v 1.13 1994/06/08 14:34:54 davidg Exp $
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
#include "syslog.h"

#include "i386/isa/isa.h"
#include "i386/isa/isa_device.h"
#include "i386/isa/lptreg.h"

#include "i386/include/lpt.h"

#define	LPINITRDY	4	/* wait up to 4 seconds for a ready */
#define	LPTOUTTIME	4	/* wait up to 4 seconds for a ready */
#define	LPPRI		(PZERO+8)
#define	BUFSIZE		1024


/* BIOS printer list - used by BIOS probe*/
#define	BIOS_LPT_PORTS	0x408
#define	BIOS_PORTS	(short *)(KERNBASE+BIOS_LPT_PORTS)
#define	BIOS_MAX_LPT	4


#ifndef DEBUG
#define lprintf (void)
#else
#define lprintf		if (lptflag) printf
int lptflag = 1;
#endif

#define	LPTUNIT(s)	((s)&0x03)
#define	LPTFLAGS(s)	((s)&0xfc)

struct lpt_softc {
	short	sc_port;
	short	sc_state;
	/* default case: negative prime, negative ack, handshake strobe,
	   prime once */
	u_char	sc_control;
	char	sc_flags;
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
	u_char	sc_irq ;	/* IRQ status of port */
#define LP_HAS_IRQ	0x01	/* we have an irq available */
#define LP_USE_IRQ	0x02	/* we are using our irq */
#define LP_ENABLE_IRQ	0x04	/* enable IRQ on open */

} lpt_sc[NLPT] ;

/* bits for state */
#define	OPEN		(1<<0)	/* device is open */
#define	ASLP		(1<<1)	/* awaiting draining of printer */
#define	ERROR		(1<<2)	/* error was received from printer */
#define	OBUSY		(1<<3)	/* printer is busy doing output */
#define LPTOUT		(1<<4)	/* timeout while not selected */
#define TOUT		(1<<5)	/* timeout while not selected */
#define INIT		(1<<6)	/* waiting to initialize for open */
#define INTERRUPTED	(1<<7)	/* write call was interrupted */


/* status masks to interrogate printer status */
#define RDY_MASK	(LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)	/* ready ? */
#define LP_READY	(LPS_SEL|LPS_NBSY|LPS_NERR)

/* Printer Ready condition  - from lpa.c */
/* Only used in polling code */
#define	LPS_INVERT	(LPS_NBSY | LPS_NACK |           LPS_SEL | LPS_NERR)
#define	LPS_MASK	(LPS_NBSY | LPS_NACK | LPS_OUT | LPS_SEL | LPS_NERR)
#define	NOT_READY(x)	((inb(x)^LPS_INVERT)&LPS_MASK)

#define	MAX_SLEEP	(hz*5)	/* Timeout while waiting for device ready */
#define	MAX_SPIN	20	/* Max delay for device ready in usecs */


static void	lptout (struct lpt_softc * sc);
int		lptprobe (struct isa_device *dvp);
int		lptattach (struct isa_device *isdp);
void		lptintr (int unit);

struct	isa_driver lptdriver = {
	lptprobe, lptattach, "lpt"
};



/*
 * Internal routine to lptprobe to do port tests of one byte value
 */
static int
lpt_port_test(short port, u_char data, u_char mask)
{
	int	temp, timeout;

	data = data & mask;
	outb(port, data);
	timeout = 10000;
	do {
		DELAY(10);
		temp = inb(port) & mask;
	}
	while (temp != data && --timeout);
	lprintf("Port 0x%x\tout=%x\tin=%x\ttout=%d\n",
		port, data, temp, timeout);
	return (temp == data);
}

/*
 * New lpt port probe Geoff Rehmet - Rhodes University - 14/2/94
 * Based partially on Rod Grimes' printer probe
 *
 * Logic:
 *	1) If no port address was given, use the bios detected ports
 *	   and autodetect what ports the printers are on.
 *	2) Otherwise, probe the data port at the address given,
 *	   using the method in Rod Grimes' port probe.
 *	   (Much code ripped off directly from Rod's probe.)
 *
 * Comments from Rod's probe:
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
 *	   We are finally left with a mask of 0x14, due to some printers
 *	   being adamant about holding other bits high ........
 *
 *	   Before probing the control port, we write a 0 to the data port -
 *	   If not, some printers chuck out garbage when the strobe line
 *	   gets toggled.
 *
 *	3) Set the data and control ports to a value of 0
 *
 *	This probe routine has been tested on Epson Lx-800, HP LJ3P,
 *	Epson FX-1170 and C.Itoh 8510RM
 *	printers.
 *	Quick exit on fail added.
 */
int
lptprobe(struct isa_device *dvp)
{
	short		port;
	static short	next_bios_lpt = 0;
	int		status;
	u_char		data;
	u_char		mask;
	int		i;

	/*
	 * Make sure there is some way for lptopen to see that
	 * the port is not configured
	 * This 0 will remain if the port isn't attached
	 */
	(lpt_sc + dvp->id_unit)->sc_port = 0;

	status = IO_LPTSIZE;
	/* If port not specified, use bios list */
	if(dvp->id_iobase < 0) {	/* port? */
		if((next_bios_lpt < BIOS_MAX_LPT) &&
				(*(BIOS_PORTS+next_bios_lpt) != 0) ) {
			dvp->id_iobase = *(BIOS_PORTS+next_bios_lpt++);
			goto end_probe;
		} else 
			return (0);
	} 

	/* Port was explicitly specified */
	/* This allows probing of ports unknown to the BIOS */

	port = dvp->id_iobase + lpt_data;
	mask = 0xff;
	data = 0x55;				/* Alternating zeros */
	if (!lpt_port_test(port, data, mask)) 
		{ status = 0 ; goto end_probe ; }

	data = 0xaa;				/* Alternating ones */
	if (!lpt_port_test(port, data, mask)) 
		{ status = 0 ; goto end_probe ; }

	for (i = 0; i < 8; i++)	{		/* Walking zero */
		data = ~(1 << i);
		if (!lpt_port_test(port, data, mask))
			{ status = 0 ; goto end_probe ; }
	}

	for (i = 0; i < 8; i++)	{		/* Walking one */
		data = (1 << i);
		if (!lpt_port_test(port, data, mask)) 
			{ status = 0 ; goto end_probe ; }
	}

end_probe:
	/* write 0's to control and data ports */
	outb(dvp->id_iobase+lpt_data, 0);
	outb(dvp->id_iobase+lpt_control, 0);

	return (status);
}

/* XXX Todo - try and detect if interrupt is working */
int
lptattach(struct isa_device *isdp)
{
	struct	lpt_softc	*sc;

	sc = lpt_sc + isdp->id_unit;
	sc->sc_port = isdp->id_iobase;
	sc->sc_primed = 0;	/* not primed yet */
	outb(sc->sc_port+lpt_control, LPC_NINIT);

	/* check if we can use interrupt */
	lprintf("oldirq %x\n", sc->sc_irq);
	if(isdp->id_irq) {
		sc->sc_irq = LP_HAS_IRQ | LP_USE_IRQ | LP_ENABLE_IRQ;
		printf("lpt%d: Interrupt-driven port\n", isdp->id_unit);
	} else {
		sc->sc_irq = 0;
		lprintf("lpt%d: Polled port\n", isdp->id_unit);
	}
	lprintf("irq %x\n", sc->sc_irq);

	return (1);
}

/*
 * lptopen -- reset the printer, then wait until it's selected and not busy.
 */

int
lptopen(dev_t dev, int flag)
{
	struct lpt_softc *sc;
	int s;
	int trys, port;
	u_int unit = LPTUNIT(minor(dev));

	sc = lpt_sc + unit;
	if ((unit >= NLPT) || (sc->sc_port == 0))
		return (ENXIO);

	if (sc->sc_state) {
	lprintf("lp: still open\n") ;
	lprintf("still open %x\n", sc->sc_state);
		return(EBUSY);
	} else	sc->sc_state |= INIT;

	s = spltty();
	sc->sc_flags = LPTFLAGS(minor(dev));
	lprintf("lp flags 0x%x\n", sc->sc_flags);
	port = sc->sc_port;

	/* set IRQ status according to ENABLE_IRQ flag */
	if(sc->sc_irq & LP_ENABLE_IRQ)
		sc->sc_irq |= LP_USE_IRQ;
	else
		sc->sc_irq &= ~LP_USE_IRQ;


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
		if (tsleep ((caddr_t)sc, LPPRI|PCATCH, "lptinit", 
				hz/4) != EWOULDBLOCK) {
			sc->sc_state = 0;
			splx(s);
			return (EBUSY);
		}
		
		/* is printer online and ready for output */
	} while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
			(LPS_SEL|LPS_NBSY|LPS_NERR));

	sc->sc_control = LPC_SEL|LPC_NINIT;
	if(sc->sc_flags&LP_AUTOLF)
		sc->sc_control |= LPC_AUTOL;
	/* enable interrupt if interrupt-driven */
	if(sc->sc_irq & LP_USE_IRQ)
		sc->sc_control |= LPC_ENA;

	outb(port+lpt_control, sc->sc_control);

	sc->sc_state = OPEN;
	sc->sc_inbuf = geteblk(BUFSIZE);
	sc->sc_xfercnt = 0;
	splx(s);

	/* only use timeout if using interrupt */
	lprintf("irq %x\n", sc->sc_irq);
	if(sc->sc_irq & LP_USE_IRQ) {
		sc->sc_state |= TOUT;
		timeout ((timeout_func_t)lptout, (caddr_t)sc, hz/2);
	}
	lprintf("opened.\n");
	return(0);
}

static void
lptout (struct lpt_softc * sc)
{	int pl;

	lprintf ("T %x ", inb(sc->sc_port+lpt_status));
	if (sc->sc_state&OPEN)
		timeout ((timeout_func_t)lptout, (caddr_t)sc, hz/2);
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
 *
 * Check for interrupted write call added.
 */

int
lptclose(dev_t dev, int flag)
{
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));
	int port = sc->sc_port;

	sc->sc_state &= ~OPEN;

	/* if the last write was interrupted, don't complete it */
	if((!(sc->sc_state  & INTERRUPTED)) && (sc->sc_irq & LP_USE_IRQ))
		while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
			(LPS_SEL|LPS_NBSY|LPS_NERR) || sc->sc_xfercnt)
			/* wait 1/4 second, give up if we get a signal */
			if (tsleep ((caddr_t)sc, LPPRI|PCATCH, 
				"lpclose", hz) != EWOULDBLOCK)
				break;

	sc->sc_state = 0;
	sc->sc_xfercnt = 0;
	outb(sc->sc_port+lpt_control, LPC_NINIT);
	brelse(sc->sc_inbuf);
	lprintf("closed.\n");
	return(0);
}

/*
 * pushbytes()
 *	Workhorse for actually spinning and writing bytes to printer
 *	Derived from lpa.c
 *	Originally by ?
 *
 *	This code is only used when we are polling the port
 */
static int 
pushbytes(struct lpt_softc * sc) 
{
	int spin, err, tic;
	char ch;
	int port = sc->sc_port;

	lprintf("p");
	/* loop for every character .. */
	while (sc->sc_xfercnt > 0) {
		/* printer data */
		ch = *(sc->sc_cp);
		sc->sc_cp++;
		sc->sc_xfercnt--;

		/*
		 * Wait for printer ready.
		 * Loop 20 usecs testing BUSY bit, then sleep
		 * for exponentially increasing timeout. (vak)
		 */
		for (spin=0; NOT_READY(port+lpt_status) && spin<MAX_SPIN; ++spin)
			DELAY(1);	/* XXX delay is NOT this accurate! */
		if (spin >= MAX_SPIN) {
			tic = 0;
			while (NOT_READY(port+lpt_status)) {
				/*
				 * Now sleep, every cycle a
				 * little longer ..
				 */
				tic = tic + tic + 1;
				/*
				 * But no more than 10 seconds. (vak)
				 */
				if (tic > MAX_SLEEP)
					tic = MAX_SLEEP;
				err = tsleep((caddr_t)sc, LPPRI,
					"lptpoll", tic);
				if (err != EWOULDBLOCK) {
					return (err);
				}
			}
		}

		/* output data */
		outb(port+lpt_data, ch);
		/* strobe */
		outb(port+lpt_control, sc->sc_control|LPC_STB);
		outb(port+lpt_control, sc->sc_control);

	}
	return(0);
}

/* 
 * lptwrite --copy a line from user space to a local buffer, then call 
 * putc to get the chars moved to the output queue.
 *
 * Flagging of interrupted write added.
 */

int
lptwrite(dev_t dev, struct uio * uio)
{
	register unsigned n;
	int pl, err;
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));

	sc->sc_state &= ~INTERRUPTED;
	while (n = MIN(BUFSIZE, uio->uio_resid)) {
		sc->sc_cp = sc->sc_inbuf->b_un.b_addr ;
		uiomove(sc->sc_cp, n, uio);
		sc->sc_xfercnt = n ;
		while ((sc->sc_xfercnt > 0)&&(sc->sc_irq & LP_USE_IRQ)) {
			lprintf("i");
			/* if the printer is ready for a char, */
			/* give it one */
			if ((sc->sc_state & OBUSY) == 0){
				lprintf("\nC %d. ", sc->sc_xfercnt);
				pl = spltty();
				lptintr(sc - lpt_sc);
				(void) splx(pl);
			}
			lprintf("W ");
			if (sc->sc_state & OBUSY)
				if (err = tsleep ((caddr_t)sc, 
					 LPPRI|PCATCH, "lpwrite", 0)) {
					sc->sc_state |= INTERRUPTED;
					return(err);
				}
		}
		/* check to see if we must do a polled write */
		if(!(sc->sc_irq & LP_USE_IRQ) && (sc->sc_xfercnt)) { 
			lprintf("p");
			if((err = pushbytes(sc)))
				return(err);
		}
	}
	return(0);
}

/*
 * lptintr -- handle printer interrupts which occur when the printer is
 * ready to accept another char.
 *
 * do checking for interrupted write call.
 */

void
lptintr(int unit)
{
	struct lpt_softc *sc = lpt_sc + unit;
	int port = sc->sc_port, sts;

	/* is printer online and ready for output */
	if (((sts=inb(port+lpt_status)) & RDY_MASK) == LP_READY) {
		sc->sc_state = (sc->sc_state | OBUSY) & ~ERROR;

		if (sc->sc_xfercnt) {
			/* send char */
			/*lprintf("%x ", *sc->sc_cp); */
			outb(port+lpt_data, *sc->sc_cp++) ; 
			outb(port+lpt_control, sc->sc_control|LPC_STB);
			/* DELAY(X) */
			outb(port+lpt_control, sc->sc_control);

			/* any more data for printer */
			if(--(sc->sc_xfercnt) > 0) return;
		}

		/*
		 * No more data waiting for printer.
		 * Wakeup is not done if write call was interrupted.
		 */
		sc->sc_state &= ~OBUSY;
		if(!(sc->sc_state & INTERRUPTED))
			wakeup((caddr_t)sc);
		lprintf("w ");
		return;
	} else	{	/* check for error */
		if(((sts & (LPS_NERR | LPS_OUT) ) != LPS_NERR) &&
				(sc->sc_state & OPEN))
			sc->sc_state |= ERROR;
	}
	lprintf("sts %x ", sts);
}

int
lptioctl(dev_t dev, int cmd, caddr_t data, int flag)
{
	int	error = 0;
        struct	lpt_softc *sc;
        u_int	unit = LPTUNIT(minor(dev));
	u_char	old_sc_irq;	/* old printer IRQ status */

        sc = lpt_sc + unit;

	switch (cmd) {
	case LPT_IRQ :
		if(sc->sc_irq & LP_HAS_IRQ) {
			/* 
			 * NOTE: 
			 * If the IRQ status is changed,
			 * this will only be visible on the
			 * next open.
			 * 
			 * If interrupt status changes,
			 * this gets syslog'd.
			 */
			old_sc_irq = sc->sc_irq;
			if(*(int*)data == 0)
				sc->sc_irq &= (~LP_ENABLE_IRQ);
			else
				sc->sc_irq |= LP_ENABLE_IRQ;
			if (old_sc_irq != sc->sc_irq ) 
				log(LOG_NOTICE, "lpt%c switched to %s mode\n",
					(char)unit+'0', 
					(sc->sc_irq & LP_ENABLE_IRQ)?
					"interrupt-driven":"polled");
		} else /* polled port */
			error = EOPNOTSUPP;
		break;	
	default:
		error = ENODEV;
	}

	return(error);
}

#endif	/* NLPT */
