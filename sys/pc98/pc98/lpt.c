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
 *	$Id: lpt.c,v 1.18 1998/02/20 12:25:23 kato Exp $
 */

/*
 * Device Driver for AT parallel printer port
 * Written by William Jolitz 12/18/90
 */

/*
 * Parallel port TCP/IP interfaces added.  I looked at the driver from
 * MACH but this is a complete rewrite, and btw. incompatible, and it
 * should perform better too.  I have never run the MACH driver though.
 *
 * This driver sends two bytes (0x08, 0x00) in front of each packet,
 * to allow us to distinguish another format later.
 *
 * Now added an Linux/Crynwr compatibility mode which is enabled using
 * IF_LINK0 - Tim Wilkinson.
 *
 * TODO:
 *    Make HDLC/PPP mode, use IF_LLC1 to enable.
 *
 * Connect the two computers using a Laplink parallel cable to use this
 * feature:
 *
 *      +----------------------------------------+
 * 	|A-name	A-End	B-End	Descr.	Port/Bit |
 *      +----------------------------------------+
 *	|DATA0	2	15	Data	0/0x01   |
 *	|-ERROR	15	2	   	1/0x08   |
 *      +----------------------------------------+
 *	|DATA1	3	13	Data	0/0x02	 |
 *	|+SLCT	13	3	   	1/0x10   |
 *      +----------------------------------------+
 *	|DATA2	4	12	Data	0/0x04   |
 *	|+PE	12	4	   	1/0x20   |
 *      +----------------------------------------+
 *	|DATA3	5	10	Strobe	0/0x08   |
 *	|-ACK	10	5	   	1/0x40   |
 *      +----------------------------------------+
 *	|DATA4	6	11	Data	0/0x10   |
 *	|BUSY	11	6	   	1/~0x80  |
 *      +----------------------------------------+
 *	|GND	18-25	18-25	GND	-        |
 *      +----------------------------------------+
 *
 * Expect transfer-rates up to 75 kbyte/sec.
 *
 * If GCC could correctly grok
 *	register int port asm("edx")
 * the code would be cleaner
 *
 * Poul-Henning Kamp <phk@freebsd.org>
 */

#include "lpt.h"
#include "opt_devfs.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>
#include <machine/lpt.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif
#include <i386/isa/isa_device.h>
#include <i386/isa/lptreg.h>

#ifdef INET
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#endif /* INET */


#define	LPINITRDY	4	/* wait up to 4 seconds for a ready */
#define	LPTOUTINITIAL	10	/* initial timeout to wait for ready 1/10 s */
#define	LPTOUTMAX	1	/* maximal timeout 1 s */
#define	LPPRI		(PZERO+8)
#define	BUFSIZE		1024

#ifdef INET
#ifndef LPMTU			/* MTU for the lp# interfaces */
#define	LPMTU	1500
#endif

#ifndef LPMAXSPIN1		/* DELAY factor for the lp# interfaces */
#define	LPMAXSPIN1	8000   /* Spinning for remote intr to happen */
#endif

#ifndef LPMAXSPIN2		/* DELAY factor for the lp# interfaces */
#define	LPMAXSPIN2	500	/* Spinning for remote handshake to happen */
#endif

#ifndef LPMAXERRS		/* Max errors before !RUNNING */
#define	LPMAXERRS	100
#endif

#define CLPIPHDRLEN	14	/* We send dummy ethernet addresses (two) + packet type in front of packet */
#define	CLPIP_SHAKE	0x80	/* This bit toggles between nibble reception */
#define MLPIPHDRLEN	CLPIPHDRLEN

#define LPIPHDRLEN	2	/* We send 0x08, 0x00 in front of packet */
#define	LPIP_SHAKE	0x40	/* This bit toggles between nibble reception */
#if !defined(MLPIPHDRLEN) || LPIPHDRLEN > MLPIPHDRLEN
#define MLPIPHDRLEN	LPIPHDRLEN
#endif

#define	LPIPTBLSIZE	256	/* Size of octet translation table */

#endif /* INET */

#ifndef PC98
/* BIOS printer list - used by BIOS probe*/
#define	BIOS_LPT_PORTS	0x408
#define	BIOS_PORTS	(short *)(KERNBASE+BIOS_LPT_PORTS)
#define	BIOS_MAX_LPT	4
#endif


#ifndef DEBUG
#define lprintf (void)
#else
#define lprintf		if (lptflag) printf
static int volatile lptflag = 1;
#endif

#define	LPTUNIT(s)	((s)&0x03)
#define	LPTFLAGS(s)	((s)&0xfc)

static struct lpt_softc {
	int	sc_port;
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
	u_char	sc_backoff ;	/* time to call lptout() again */

#ifdef INET
	struct  ifnet	sc_if;
	u_char		*sc_ifbuf;
	int		sc_iferrs;
#endif
#ifdef DEVFS
	void	*devfs_token;
	void	*devfs_token_ctl;
#endif
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
#ifdef PC98
#define	NOT_READY(x)	((inb(x) & LPS_NBSY) != LPS_NBSY)
#else	/* IBM-PC */
#define	LPS_INVERT	(LPS_NBSY | LPS_NACK |           LPS_SEL | LPS_NERR)
#define	LPS_MASK	(LPS_NBSY | LPS_NACK | LPS_OUT | LPS_SEL | LPS_NERR)
#define	NOT_READY(x)	((inb(x)^LPS_INVERT)&LPS_MASK)
#endif

#define	MAX_SLEEP	(hz*5)	/* Timeout while waiting for device ready */
#define	MAX_SPIN	20	/* Max delay for device ready in usecs */

static timeout_t lptout;
static int	lptprobe (struct isa_device *dvp);
static int	lptattach (struct isa_device *isdp);

#ifdef INET

/* Tables for the lp# interface */
static u_char *txmith;
#define txmitl (txmith+(1*LPIPTBLSIZE))
#define trecvh (txmith+(2*LPIPTBLSIZE))
#define trecvl (txmith+(3*LPIPTBLSIZE))

static u_char *ctxmith;
#define ctxmitl (ctxmith+(1*LPIPTBLSIZE))
#define ctrecvh (ctxmith+(2*LPIPTBLSIZE))
#define ctrecvl (ctxmith+(3*LPIPTBLSIZE))

/* Functions for the lp# interface */
static void lpattach(struct lpt_softc *,int);
static int lpinittables(void);
static int lpioctl(struct ifnet *, int, caddr_t);
static int lpoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	struct rtentry *);
static void lpintr(int);
#endif /* INET */

#ifdef PC98
#ifndef PC98_LPT_INTR
void lptintr(int unit);
#endif
#endif

struct	isa_driver lptdriver = {
	lptprobe, lptattach, "lpt"
};

static	d_open_t	lptopen;
static	d_close_t	lptclose;
static	d_write_t	lptwrite;
static	d_ioctl_t	lptioctl;

#define CDEV_MAJOR 16
static struct cdevsw lpt_cdevsw = 
	{ lptopen,	lptclose,	noread,		lptwrite,	/*16*/
	  lptioctl,	nullstop,	nullreset,	nodevtotty,/* lpt */
	  seltrue,	nommap,		nostrat,	"lpt",	NULL,	-1 };


/*
 * Internal routine to lptprobe to do port tests of one byte value
 */
static int
lpt_port_test (int port, u_char data, u_char mask)
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
#ifdef PC98
	return 8;
#else
	int		port;
	static short	next_bios_lpt = 0;
	int		status;
	static u_char	testbyte[18] = {
		0x55,			/* alternating zeros */
		0xaa,			/* alternating ones */
		0xfe, 0xfd, 0xfb, 0xf7,
		0xef, 0xdf, 0xbf, 0x7f,	/* walking zero */
		0x01, 0x02, 0x04, 0x08,
		0x10, 0x20, 0x40, 0x80	/* walking one */
	};
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
	for (i = 0; i < 18; i++) {
		if (!lpt_port_test(port, testbyte[i], 0xff)) {
			status = 0;
			goto end_probe;
		}
	}

end_probe:
	/* write 0's to control and data ports */
	outb(dvp->id_iobase+lpt_data, 0);
	outb(dvp->id_iobase+lpt_control, 0);

	return (status);
#endif
}

/* XXX Todo - try and detect if interrupt is working */
int
lptattach(struct isa_device *isdp)
{
	struct	lpt_softc	*sc;
	int	unit;

	unit = isdp->id_unit;
	sc = lpt_sc + unit;
	sc->sc_port = isdp->id_iobase;
	sc->sc_primed = 0;	/* not primed yet */
#ifdef PC98
	outb(sc->sc_port+lpt_pstb_ctrl,	LPC_DIS_PSTB);	/* PSTB disable */
	outb(sc->sc_port+lpt_control,	LPC_MODE8255);	/* 8255 mode set */
	outb(sc->sc_port+lpt_control,	LPC_NIRQ8);	/* IRQ8 inactive */
	outb(sc->sc_port+lpt_control,	LPC_NPSTB);	/* PSTB inactive */
	outb(sc->sc_port+lpt_pstb_ctrl,	LPC_EN_PSTB);	/* PSTB enable */
#else
	outb(sc->sc_port+lpt_control, LPC_NINIT);
#endif

	/* check if we can use interrupt */
	lprintf("oldirq %x\n", sc->sc_irq);
	if (isdp->id_irq) {
		sc->sc_irq = LP_HAS_IRQ | LP_USE_IRQ | LP_ENABLE_IRQ;
		printf("lpt%d: Interrupt-driven port\n", unit);
#ifdef INET
		lpattach(sc, unit);
#endif
	} else {
		sc->sc_irq = 0;
		lprintf("lpt%d: Polled port\n", unit);
	}
	lprintf("irq %x\n", sc->sc_irq);

#ifdef DEVFS
	/* XXX what to do about the flags in the minor number? */
	sc->devfs_token = devfs_add_devswf(&lpt_cdevsw,
		unit, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "lpt%d", unit);
	sc->devfs_token_ctl = devfs_add_devswf(&lpt_cdevsw,
		unit | LP_BYPASS, DV_CHR,
		UID_ROOT, GID_WHEEL, 0600, "lpctl%d", unit);
#endif
	return (1);
}

/*
 * lptopen -- reset the printer, then wait until it's selected and not busy.
 *	If LP_BYPASS flag is selected, then we do not try to select the
 *	printer -- this is just used for passing ioctls.
 */

static	int
lptopen (dev_t dev, int flags, int fmt, struct proc *p)
{
	struct lpt_softc *sc;
	int s;
	int trys, port;
	u_int unit = LPTUNIT(minor(dev));

	sc = lpt_sc + unit;
	if ((unit >= NLPT) || (sc->sc_port == 0))
		return (ENXIO);

#ifdef INET
	if (sc->sc_if.if_flags & IFF_UP)
		return(EBUSY);
#endif

	if (sc->sc_state) {
		lprintf("lp: still open %x\n", sc->sc_state);
		return(EBUSY);
	} else
		sc->sc_state |= INIT;

	sc->sc_flags = LPTFLAGS(minor(dev));

	/* Check for open with BYPASS flag set. */
	if (sc->sc_flags & LP_BYPASS) {
		sc->sc_state = OPEN;
		return(0);
	}

	s = spltty();
	lprintf("lp flags 0x%x\n", sc->sc_flags);
	port = sc->sc_port;

	/* set IRQ status according to ENABLE_IRQ flag */
	if (sc->sc_irq & LP_ENABLE_IRQ)
		sc->sc_irq |= LP_USE_IRQ;
	else
		sc->sc_irq &= ~LP_USE_IRQ;

	/* init printer */
#ifndef PC98
	if ((sc->sc_flags & LP_NO_PRIME) == 0) {
		if((sc->sc_flags & LP_PRIMEOPEN) || sc->sc_primed == 0) {
			outb(port+lpt_control, 0);
			sc->sc_primed++;
			DELAY(500);
		}
	}

	outb (port+lpt_control, LPC_SEL|LPC_NINIT);

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
		if (tsleep ((caddr_t)sc, LPPRI|PCATCH, "lptinit", hz/4) !=
		    EWOULDBLOCK) {
			sc->sc_state = 0;
			splx(s);
			return (EBUSY);
		}

		/* is printer online and ready for output */
	} while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
		 (LPS_SEL|LPS_NBSY|LPS_NERR));

	sc->sc_control = LPC_SEL|LPC_NINIT;
	if (sc->sc_flags & LP_AUTOLF)
		sc->sc_control |= LPC_AUTOL;

	/* enable interrupt if interrupt-driven */
	if (sc->sc_irq & LP_USE_IRQ)
		sc->sc_control |= LPC_ENA;

	outb(port+lpt_control, sc->sc_control);
#endif

	sc->sc_state = OPEN;
	sc->sc_inbuf = geteblk(BUFSIZE);
	sc->sc_xfercnt = 0;
	splx(s);

	/* only use timeout if using interrupt */
	lprintf("irq %x\n", sc->sc_irq);
	if (sc->sc_irq & LP_USE_IRQ) {
		sc->sc_state |= TOUT;
		timeout (lptout, (caddr_t)sc,
			 (sc->sc_backoff = hz/LPTOUTINITIAL));
	}

	lprintf("opened.\n");
	return(0);
}

static void
lptout (void *arg)
{
	struct lpt_softc *sc = arg;
	int pl;

	lprintf ("T %x ", inb(sc->sc_port+lpt_status));
	if (sc->sc_state & OPEN) {
		sc->sc_backoff++;
		if (sc->sc_backoff > hz/LPTOUTMAX)
			sc->sc_backoff = sc->sc_backoff > hz/LPTOUTMAX;
		timeout (lptout, (caddr_t)sc, sc->sc_backoff);
	} else
		sc->sc_state &= ~TOUT;

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

static	int
lptclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));
	int port = sc->sc_port;

	if(sc->sc_flags & LP_BYPASS)
		goto end_close;

	sc->sc_state &= ~OPEN;

#ifndef PC98
	/* if the last write was interrupted, don't complete it */
	if((!(sc->sc_state  & INTERRUPTED)) && (sc->sc_irq & LP_USE_IRQ))
		while ((inb(port+lpt_status) & (LPS_SEL|LPS_OUT|LPS_NBSY|LPS_NERR)) !=
			(LPS_SEL|LPS_NBSY|LPS_NERR) || sc->sc_xfercnt)
			/* wait 1/4 second, give up if we get a signal */
			if (tsleep ((caddr_t)sc, LPPRI|PCATCH,
				"lpclose", hz) != EWOULDBLOCK)
				break;

	outb(sc->sc_port+lpt_control, LPC_NINIT);
#endif
	brelse(sc->sc_inbuf);

end_close:
	sc->sc_state = 0;
	sc->sc_xfercnt = 0;
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
#ifdef PC98
		DELAY(1);
		outb(port+lpt_control, LPC_PSTB);
		DELAY(1);
		outb(port+lpt_control, LPC_NPSTB);
#else
		/* strobe */
		outb(port+lpt_control, sc->sc_control|LPC_STB);
		outb(port+lpt_control, sc->sc_control);
#endif

	}
	return(0);
}

/*
 * lptwrite --copy a line from user space to a local buffer, then call
 * putc to get the chars moved to the output queue.
 *
 * Flagging of interrupted write added.
 */

static	int
lptwrite(dev_t dev, struct uio * uio, int ioflag)
{
	register unsigned n;
	int pl, err;
	struct lpt_softc *sc = lpt_sc + LPTUNIT(minor(dev));

	if(sc->sc_flags & LP_BYPASS) {
		/* we can't do writes in bypass mode */
		return(EPERM);
	}

	sc->sc_state &= ~INTERRUPTED;
	while ((n = min(BUFSIZE, uio->uio_resid)) != 0) {
		sc->sc_cp = sc->sc_inbuf->b_data ;
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
				if ((err = tsleep ((caddr_t)sc,
					 LPPRI|PCATCH, "lpwrite", 0))) {
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
	int i;

#ifdef INET
	if(sc->sc_if.if_flags & IFF_UP) {
	    lpintr(unit);
	    return;
	}
#endif /* INET */

#ifndef PC98
	/*
	 * Is printer online and ready for output?
	 *
	 * Avoid falling back to lptout() too quickly.  First spin-loop
	 * to see if the printer will become ready ``really soon now''.
	 */
	for (i = 0;
	     i < 100 &&
	     ((sts=inb(port+lpt_status)) & RDY_MASK) != LP_READY;
	     i++) ;
	if ((sts & RDY_MASK) == LP_READY) {
		sc->sc_state = (sc->sc_state | OBUSY) & ~ERROR;
		sc->sc_backoff = hz/LPTOUTINITIAL;

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
		/* lptout() will jump in and try to restart. */
	}
#endif
	lprintf("sts %x ", sts);
}

static	int
lptioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
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

#ifdef INET

static void
lpattach (struct lpt_softc *sc, int unit)
{
	struct ifnet *ifp = &sc->sc_if;

	ifp->if_softc = sc;
	ifp->if_name = "lp";
	ifp->if_unit = unit;
	ifp->if_mtu = LPMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_ioctl = lpioctl;
	ifp->if_output = lpoutput;
	ifp->if_type = IFT_PARA;
	ifp->if_hdrlen = 0;
	ifp->if_addrlen = 0;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;
	if_attach(ifp);
	printf("lp%d: TCP/IP capable interface\n", unit);

#if NBPFILTER > 0
	bpfattach(ifp, DLT_NULL, LPIPHDRLEN);
#endif
}
/*
 * Build the translation tables for the LPIP (BSD unix) protocol.
 * We don't want to calculate these nasties in our tight loop, so we
 * precalculate them when we initialize.
 */
static int
lpinittables (void)
{
    int i;

    if (!txmith)
	txmith = malloc(4*LPIPTBLSIZE, M_DEVBUF, M_NOWAIT);

    if (!txmith)
	return 1;

    if (!ctxmith)
	ctxmith = malloc(4*LPIPTBLSIZE, M_DEVBUF, M_NOWAIT);

    if (!ctxmith)
	return 1;

    for (i=0; i < LPIPTBLSIZE; i++) {
	ctxmith[i] = (i & 0xF0) >> 4;
	ctxmitl[i] = 0x10 | (i & 0x0F);
	ctrecvh[i] = (i & 0x78) << 1;
	ctrecvl[i] = (i & 0x78) >> 3;
    }

    for (i=0; i < LPIPTBLSIZE; i++) {
	txmith[i] = ((i & 0x80) >> 3) | ((i & 0x70) >> 4) | 0x08;
	txmitl[i] = ((i & 0x08) << 1) | (i & 0x07);
	trecvh[i] = ((~i) & 0x80) | ((i & 0x38) << 1);
	trecvl[i] = (((~i) & 0x80) >> 4) | ((i & 0x38) >> 3);
    }

    return 0;
}

/*
 * Process an ioctl request.
 */

static int
lpioctl (struct ifnet *ifp, int cmd, caddr_t data)
{
    struct lpt_softc *sc = lpt_sc + ifp->if_unit;
    struct ifaddr *ifa = (struct ifaddr *)data;
    struct ifreq *ifr = (struct ifreq *)data;
    u_char *ptr;

    switch (cmd) {

    case SIOCSIFDSTADDR:
    case SIOCAIFADDR:
    case SIOCSIFADDR:
	if (ifa->ifa_addr->sa_family != AF_INET)
	    return EAFNOSUPPORT;
	ifp->if_flags |= IFF_UP;
	/* FALLTHROUGH */
    case SIOCSIFFLAGS:
	if ((!(ifp->if_flags & IFF_UP)) && (ifp->if_flags & IFF_RUNNING)) {
	    outb(sc->sc_port + lpt_control, 0x00);
	    ifp->if_flags &= ~IFF_RUNNING;
	    break;
	}
#ifdef PC98
	/* XXX */
	return ENOBUFS;
#else
	if (((ifp->if_flags & IFF_UP)) && (!(ifp->if_flags & IFF_RUNNING))) {
	    if (lpinittables())
		return ENOBUFS;
	    sc->sc_ifbuf = malloc(sc->sc_if.if_mtu + MLPIPHDRLEN,
				  M_DEVBUF, M_WAITOK);
	    if (!sc->sc_ifbuf)
		return ENOBUFS;

	    outb(sc->sc_port + lpt_control, LPC_ENA);
	    ifp->if_flags |= IFF_RUNNING;
	}
	break;
#endif
    case SIOCSIFMTU:
	ptr = sc->sc_ifbuf;
	sc->sc_ifbuf = malloc(ifr->ifr_mtu+MLPIPHDRLEN, M_DEVBUF, M_NOWAIT);
	if (!sc->sc_ifbuf) {
	    sc->sc_ifbuf = ptr;
	    return ENOBUFS;
	}
	if (ptr)
	    free(ptr,M_DEVBUF);
	sc->sc_if.if_mtu = ifr->ifr_mtu;
	break;

    case SIOCGIFMTU:
	ifr->ifr_mtu = sc->sc_if.if_mtu;
	break;

    case SIOCADDMULTI:
    case SIOCDELMULTI:
	if (ifr == 0) {
	    return EAFNOSUPPORT;		/* XXX */
	}
	switch (ifr->ifr_addr.sa_family) {

#ifdef INET
	case AF_INET:
	    break;
#endif

	default:
	    return EAFNOSUPPORT;
	}
	break;

    default:
	lprintf("LP:ioctl(0x%x)\n",cmd);
	return EINVAL;
    }
    return 0;
}

static __inline int
clpoutbyte (u_char byte, int spin, int data_port, int status_port)
{
	outb(data_port, ctxmitl[byte]);
	while (inb(status_port) & CLPIP_SHAKE)
		if (--spin == 0) {
			return 1;
		}
	outb(data_port, ctxmith[byte]);
	while (!(inb(status_port) & CLPIP_SHAKE))
		if (--spin == 0) {
			return 1;
		}
	return 0;
}

static __inline int
clpinbyte (int spin, int data_port, int status_port)
{
	int c, cl;

	while((inb(status_port) & CLPIP_SHAKE))
	    if(!--spin) {
		return -1;
	    }
	cl = inb(status_port);
	outb(data_port, 0x10);

	while(!(inb(status_port) & CLPIP_SHAKE))
	    if(!--spin) {
		return -1;
	    }
	c = inb(status_port);
	outb(data_port, 0x00);

	return (ctrecvl[cl] | ctrecvh[c]);
}

static void
lpintr (int unit)
{
	struct   lpt_softc *sc = lpt_sc + unit;
	register int lpt_data_port = sc->sc_port + lpt_data;
	register int lpt_stat_port = sc->sc_port + lpt_status;
		 int lpt_ctrl_port = sc->sc_port + lpt_control;
	int len, s, j;
	u_char *bp;
	u_char c, cl;
	struct mbuf *top;

	s = splhigh();

	if (sc->sc_if.if_flags & IFF_LINK0) {

	    /* Ack. the request */
	    outb(lpt_data_port, 0x01);

	    /* Get the packet length */
	    j = clpinbyte(LPMAXSPIN2, lpt_data_port, lpt_stat_port);
	    if (j == -1)
		goto err;
	    len = j;
	    j = clpinbyte(LPMAXSPIN2, lpt_data_port, lpt_stat_port);
	    if (j == -1)
		goto err;
	    len = len + (j << 8);
	    if (len > sc->sc_if.if_mtu + MLPIPHDRLEN)
		goto err;

	    bp  = sc->sc_ifbuf;
	
	    while (len--) {
	        j = clpinbyte(LPMAXSPIN2, lpt_data_port, lpt_stat_port);
	        if (j == -1) {
		    goto err;
	        }
	        *bp++ = j;
	    }
	    /* Get and ignore checksum */
	    j = clpinbyte(LPMAXSPIN2, lpt_data_port, lpt_stat_port);
	    if (j == -1) {
	        goto err;
	    }

	    len = bp - sc->sc_ifbuf;
	    if (len <= CLPIPHDRLEN)
	        goto err;

	    sc->sc_iferrs = 0;

	    if (IF_QFULL(&ipintrq)) {
	        lprintf("DROP");
	        IF_DROP(&ipintrq);
		goto done;
	    }
	    len -= CLPIPHDRLEN;
	    sc->sc_if.if_ipackets++;
	    sc->sc_if.if_ibytes += len;
	    top = m_devget(sc->sc_ifbuf + CLPIPHDRLEN, len, 0, &sc->sc_if, 0);
	    if (top) {
	        IF_ENQUEUE(&ipintrq, top);
	        schednetisr(NETISR_IP);
	    }
	    goto done;
	}
	while ((inb(lpt_stat_port) & LPIP_SHAKE)) {
	    len = sc->sc_if.if_mtu + LPIPHDRLEN;
	    bp  = sc->sc_ifbuf;
	    while (len--) {

		cl = inb(lpt_stat_port);
		outb(lpt_data_port, 8);

		j = LPMAXSPIN2;
		while((inb(lpt_stat_port) & LPIP_SHAKE))
		    if(!--j) goto err;

		c = inb(lpt_stat_port);
		outb(lpt_data_port, 0);

		*bp++= trecvh[cl] | trecvl[c];

		j = LPMAXSPIN2;
		while (!((cl=inb(lpt_stat_port)) & LPIP_SHAKE)) {
		    if (cl != c &&
			(((cl = inb(lpt_stat_port)) ^ 0xb8) & 0xf8) ==
			  (c & 0xf8))
			goto end;
		    if (!--j) goto err;
		}
	    }

	end:
	    len = bp - sc->sc_ifbuf;
	    if (len <= LPIPHDRLEN)
		goto err;

	    sc->sc_iferrs = 0;

	    if (IF_QFULL(&ipintrq)) {
		lprintf("DROP");
		IF_DROP(&ipintrq);
		goto done;
	    }
#if NBPFILTER > 0
	    if (sc->sc_if.if_bpf) {
		bpf_tap(&sc->sc_if, sc->sc_ifbuf, len);
	    }
#endif
	    len -= LPIPHDRLEN;
	    sc->sc_if.if_ipackets++;
	    sc->sc_if.if_ibytes += len;
	    top = m_devget(sc->sc_ifbuf + LPIPHDRLEN, len, 0, &sc->sc_if, 0);
	    if (top) {
		    IF_ENQUEUE(&ipintrq, top);
		    schednetisr(NETISR_IP);
	    }
	}
	goto done;

    err:
	outb(lpt_data_port, 0);
	lprintf("R");
	sc->sc_if.if_ierrors++;
	sc->sc_iferrs++;

	/*
	 * We are not able to send receive anything for now,
	 * so stop wasting our time
	 */
	if (sc->sc_iferrs > LPMAXERRS) {
	    printf("lp%d: Too many errors, Going off-line.\n", unit);
	    outb(lpt_ctrl_port, 0x00);
	    sc->sc_if.if_flags &= ~IFF_RUNNING;
	    sc->sc_iferrs=0;
	}

    done:
	splx(s);
	return;
}

static __inline int
lpoutbyte (u_char byte, int spin, int data_port, int status_port)
{
    outb(data_port, txmith[byte]);
    while (!(inb(status_port) & LPIP_SHAKE))
	if (--spin == 0)
		return 1;
    outb(data_port, txmitl[byte]);
    while (inb(status_port) & LPIP_SHAKE)
	if (--spin == 0)
		return 1;
    return 0;
}

static int
lpoutput (struct ifnet *ifp, struct mbuf *m,
	  struct sockaddr *dst, struct rtentry *rt)
{
    register int lpt_data_port = lpt_sc[ifp->if_unit].sc_port + lpt_data;
    register int lpt_stat_port = lpt_sc[ifp->if_unit].sc_port + lpt_status;
	     int lpt_ctrl_port = lpt_sc[ifp->if_unit].sc_port + lpt_control;

    int s, err;
    struct mbuf *mm;
    u_char *cp = "\0\0";
    u_char chksum = 0;
    int count = 0;
    int i;
    int spin;

    /* We need a sensible value if we abort */
    cp++;
    ifp->if_flags |= IFF_RUNNING;

    err = 1;			/* assume we're aborting because of an error */

    s = splhigh();

#ifndef PC98
    /* Suspend (on laptops) or receive-errors might have taken us offline */
    outb(lpt_ctrl_port, LPC_ENA);
#endif

    if (ifp->if_flags & IFF_LINK0) {

	if (!(inb(lpt_stat_port) & CLPIP_SHAKE)) {
	    lprintf("&");
	    lptintr(ifp->if_unit);
	}

	/* Alert other end to pending packet */
	spin = LPMAXSPIN1;
	outb(lpt_data_port, 0x08);
	while ((inb(lpt_stat_port) & 0x08) == 0)
		if (--spin == 0) {
			goto nend;
		}

	/* Calculate length of packet, then send that */

	count += 14;		/* Ethernet header len */

	mm = m;
	for (mm = m; mm; mm = mm->m_next) {
		count += mm->m_len;
	}
	if (clpoutbyte(count & 0xFF, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
		goto nend;
	if (clpoutbyte((count >> 8) & 0xFF, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
		goto nend;

	/* Send dummy ethernet header */
	for (i = 0; i < 12; i++) {
		if (clpoutbyte(i, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
			goto nend;
		chksum += i;
	}

	if (clpoutbyte(0x08, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
		goto nend;
	if (clpoutbyte(0x00, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
		goto nend;
	chksum += 0x08 + 0x00;		/* Add into checksum */

	mm = m;
	do {
		cp = mtod(mm, u_char *);
		while (mm->m_len--) {
			chksum += *cp;
			if (clpoutbyte(*cp++, LPMAXSPIN2, lpt_data_port, lpt_stat_port))
				goto nend;
		}
	} while ((mm = mm->m_next));

	/* Send checksum */
	if (clpoutbyte(chksum, LPMAXSPIN2, lpt_data_port, lpt_stat_port))
		goto nend;

	/* Go quiescent */
	outb(lpt_data_port, 0);

	err = 0;			/* No errors */

	nend:
	if (err)  {				/* if we didn't timeout... */
		ifp->if_oerrors++;
		lprintf("X");
	} else {
		ifp->if_opackets++;
		ifp->if_obytes += m->m_pkthdr.len;
	}

	m_freem(m);

	if (!(inb(lpt_stat_port) & CLPIP_SHAKE)) {
		lprintf("^");
		lptintr(ifp->if_unit);
	}
	(void) splx(s);
	return 0;
    }

    if (inb(lpt_stat_port) & LPIP_SHAKE) {
        lprintf("&");
        lptintr(ifp->if_unit);
    }

    if (lpoutbyte(0x08, LPMAXSPIN1, lpt_data_port, lpt_stat_port))
        goto end;
    if (lpoutbyte(0x00, LPMAXSPIN2, lpt_data_port, lpt_stat_port))
        goto end;

    mm = m;
    do {
        cp = mtod(mm,u_char *);
        while (mm->m_len--)
	    if (lpoutbyte(*cp++, LPMAXSPIN2, lpt_data_port, lpt_stat_port))
	        goto end;
    } while ((mm = mm->m_next));

    err = 0;				/* no errors were encountered */

    end:
    --cp;
    outb(lpt_data_port, txmitl[*cp] ^ 0x17);

    if (err)  {				/* if we didn't timeout... */
	ifp->if_oerrors++;
        lprintf("X");
    } else {
	ifp->if_opackets++;
	ifp->if_obytes += m->m_pkthdr.len;
#if NBPFILTER > 0
	if (ifp->if_bpf) {
	    /*
	     * We need to prepend the packet type as
	     * a two byte field.  Cons up a dummy header
	     * to pacify bpf.  This is safe because bpf
	     * will only read from the mbuf (i.e., it won't
	     * try to free it or keep a pointer to it).
	     */
	    struct mbuf m0;
	    u_short hdr = 0x800;

	    m0.m_next = m;
	    m0.m_len = 2;
	    m0.m_data = (char *)&hdr;

	    bpf_mtap(ifp, &m0);
	}
#endif
    }

    m_freem(m);

    if (inb(lpt_stat_port) & LPIP_SHAKE) {
	lprintf("^");
	lptintr(ifp->if_unit);
    }

    (void) splx(s);
    return 0;
}

#endif /* INET */

static lpt_devsw_installed = 0;

static void 	lpt_drvinit(void *unused)
{
	dev_t dev;

	if( ! lpt_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&lpt_cdevsw, NULL);
		lpt_devsw_installed = 1;
    	}
}

SYSINIT(lptdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,lpt_drvinit,NULL)

