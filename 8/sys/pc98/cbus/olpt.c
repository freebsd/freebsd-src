/*-
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
 * $FreeBSD$
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
 * Now added a Linux/Crynwr compatibility mode which is enabled using
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/uio.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <pc98/cbus/olptreg.h>
#include <dev/ppbus/lptio.h>

#define	LPINITRDY	4	/* wait up to 4 seconds for a ready */
#define	LPTOUTINITIAL	10	/* initial timeout to wait for ready 1/10 s */
#define	LPTOUTMAX	1	/* maximal timeout 1 s */
#define	LPPRI		(PZERO+8)
#define	BUFSIZE		1024

#ifndef DEBUG
#define lprintf(args)
#else
#define lprintf(args)	do {				\
				if (lptflag)		\
					printf args;	\
			} while (0)
static int volatile lptflag = 1;
#endif

struct lpt_softc {
	struct resource *res_port;
	struct resource *res_irq;
	void *sc_ih;

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
	void	*sc_inbuf;
	short	sc_xfercnt ;
	char	sc_primed;
	char	*sc_cp ;
	u_char	sc_irq ;	/* IRQ status of port */
#define LP_HAS_IRQ	0x01	/* we have an irq available */
#define LP_USE_IRQ	0x02	/* we are using our irq */
#define LP_ENABLE_IRQ	0x04	/* enable IRQ on open */
	u_char	sc_backoff ;	/* time to call lptout() again */
};

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
#define	NOT_READY(x)	((inb(x) & LPS_NBSY) != LPS_NBSY)

#define	MAX_SLEEP	(hz*5)	/* Timeout while waiting for device ready */
#define	MAX_SPIN	20	/* Max delay for device ready in usecs */

static timeout_t lptout;
static int lpt_probe(device_t);
static int lpt_attach(device_t);
static void lpt_intr(void *);

static devclass_t olpt_devclass;

static device_method_t olpt_methods[] = {
	DEVMETHOD(device_probe,		lpt_probe),
	DEVMETHOD(device_attach,	lpt_attach),
	{ 0, 0 }
};

static driver_t olpt_driver = {
	"olpt",
	olpt_methods,
	sizeof (struct lpt_softc),
};

DRIVER_MODULE(olpt, isa, olpt_driver, olpt_devclass, 0, 0);

static	d_open_t	lptopen;
static	d_close_t	lptclose;
static	d_write_t	lptwrite;
static	d_ioctl_t	lptioctl;

static struct cdevsw lpt_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	D_NEEDGIANT,
	.d_open =	lptopen,
	.d_close =	lptclose,
	.d_write =	lptwrite,
	.d_ioctl =	lptioctl,
	.d_name =	"lpt",
};

static bus_addr_t lpt_iat[] = {0, 2, 4, 6};

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
lpt_probe(device_t dev)
{
#define PC98_OLD_LPT 0x40
#define PC98_IEEE_1284_FUNCTION 0x149
	int rid;
	struct resource *res;

	/* Check isapnp ids */
	if (isa_get_vendorid(dev))
		return ENXIO;

	rid = 0;
	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, lpt_iat, 4,
				  RF_ACTIVE);
	if (res == NULL)
		return ENXIO;
	isa_load_resourcev(res, lpt_iat, 4);

	if (isa_get_port(dev) == PC98_OLD_LPT) {
		unsigned int pc98_ieee_mode, tmp;

		tmp = inb(PC98_IEEE_1284_FUNCTION);
		pc98_ieee_mode = tmp;
		if ((tmp & 0x10) == 0x10) {
			outb(PC98_IEEE_1284_FUNCTION, tmp & ~0x10);
			tmp = inb(PC98_IEEE_1284_FUNCTION);
			if ((tmp & 0x10) != 0x10) {
				outb(PC98_IEEE_1284_FUNCTION, pc98_ieee_mode);
				bus_release_resource(dev, SYS_RES_IOPORT, rid,
						     res);
				return ENXIO;
			}
		}
	}

	bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	return 0;
}

/* XXX Todo - try and detect if interrupt is working */
int
lpt_attach(device_t dev)
{
	int	rid, unit;
	struct	lpt_softc	*sc;
	struct	cdev		*cdev;

	unit = device_get_unit(dev);
	sc = device_get_softc(dev);

	rid = 0;
	sc->res_port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid,
					   lpt_iat, 4, RF_ACTIVE);
	if (sc->res_port == NULL)
		return ENXIO;
	isa_load_resourcev(sc->res_port, lpt_iat, 4);

	sc->sc_port = rman_get_start(sc->res_port);
	sc->sc_primed = 0;	/* not primed yet */

	outb(sc->sc_port+lpt_pstb_ctrl,	LPC_DIS_PSTB);	/* PSTB disable */
	outb(sc->sc_port+lpt_control,	LPC_MODE8255);	/* 8255 mode set */
	outb(sc->sc_port+lpt_control,	LPC_NIRQ8);	/* IRQ8 inactive */
	outb(sc->sc_port+lpt_control,	LPC_NPSTB);	/* PSTB inactive */
	outb(sc->sc_port+lpt_pstb_ctrl,	LPC_EN_PSTB);	/* PSTB enable */

	sc->sc_irq = 0;
	if (isa_get_irq(dev) != -1) {
		rid = 0;
		sc->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
						     RF_ACTIVE);
		if (sc->res_irq == NULL) {
			bus_release_resource(dev, SYS_RES_IOPORT, 0,
					     sc->res_port);
			return ENXIO;
		}
		if (bus_setup_intr(dev, sc->res_irq, INTR_TYPE_TTY, NULL, lpt_intr,
				   sc, &sc->sc_ih)) {
			bus_release_resource(dev, SYS_RES_IOPORT, 0,
					     sc->res_port);
			bus_release_resource(dev, SYS_RES_IRQ, 0,
					     sc->res_irq);
			return ENXIO;
		}
		sc->sc_irq = LP_HAS_IRQ | LP_USE_IRQ | LP_ENABLE_IRQ;
		device_printf(dev, "Interrupt-driven port");
	}

	cdev = make_dev(&lpt_cdevsw, 0,
			UID_ROOT, GID_WHEEL, 0600, "lpt%d", unit);
	cdev->si_drv1 = sc;
	cdev = make_dev(&lpt_cdevsw, LP_BYPASS,
			UID_ROOT, GID_WHEEL, 0600, "lpctl%d", unit);
	cdev->si_drv1 = sc;

	return 0;
}

/*
 * lptopen -- reset the printer, then wait until it's selected and not busy.
 *	If LP_BYPASS flag is selected, then we do not try to select the
 *	printer -- this is just used for passing ioctls.
 */

static	int
lptopen (struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct lpt_softc *sc = dev->si_drv1;
	int s;
	int port;

	if (sc->sc_port == 0)
		return (ENXIO);

	if (sc->sc_state) {
		lprintf(("lp: still open %x\n", sc->sc_state));
		return(EBUSY);
	} else
		sc->sc_state |= INIT;

	sc->sc_flags = dev2unit(dev);

	/* Check for open with BYPASS flag set. */
	if (sc->sc_flags & LP_BYPASS) {
		sc->sc_state = OPEN;
		return(0);
	}

	s = spltty();
	lprintf(("lp flags 0x%x\n", sc->sc_flags));
	port = sc->sc_port;

	/* set IRQ status according to ENABLE_IRQ flag */
	if (sc->sc_irq & LP_ENABLE_IRQ)
		sc->sc_irq |= LP_USE_IRQ;
	else
		sc->sc_irq &= ~LP_USE_IRQ;

	/* init printer */
	sc->sc_state = OPEN;
	sc->sc_inbuf = malloc(BUFSIZE, M_DEVBUF, M_WAITOK);
	sc->sc_xfercnt = 0;
	splx(s);

	/* only use timeout if using interrupt */
	lprintf(("irq %x\n", sc->sc_irq));
	if (sc->sc_irq & LP_USE_IRQ) {
		sc->sc_state |= TOUT;
		timeout (lptout, (caddr_t)sc,
			 (sc->sc_backoff = hz/LPTOUTINITIAL));
	}

	lprintf(("opened.\n"));
	return(0);
}

static void
lptout (void *arg)
{
	struct lpt_softc *sc = arg;
	int pl;

	lprintf(("T %x ", inb(sc->sc_port+lpt_status)));
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
		lpt_intr(sc);
		splx(pl);
	} else {
		sc->sc_state &= ~OBUSY;
		wakeup(sc);
	}
}

/*
 * lptclose -- close the device, free the local line buffer.
 *
 * Check for interrupted write call added.
 */

static	int
lptclose(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	struct lpt_softc *sc = dev->si_drv1;

	if(sc->sc_flags & LP_BYPASS)
		goto end_close;

	sc->sc_state &= ~OPEN;
	free(sc->sc_inbuf, M_DEVBUF);

end_close:
	sc->sc_state = 0;
	sc->sc_xfercnt = 0;
	lprintf(("closed.\n"));
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

	lprintf(("p"));
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
				err = tsleep(sc, LPPRI,
					"lptpoll", tic);
				if (err != EWOULDBLOCK) {
					return (err);
				}
			}
		}

		/* output data */
		outb(port+lpt_data, ch);
		DELAY(1);
		outb(port+lpt_control, LPC_PSTB);
		DELAY(1);
		outb(port+lpt_control, LPC_NPSTB);
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
lptwrite(struct cdev *dev, struct uio * uio, int ioflag)
{
	register unsigned n;
	int pl, err;
	struct lpt_softc *sc = dev->si_drv1;

	if(sc->sc_flags & LP_BYPASS) {
		/* we can't do writes in bypass mode */
		return(EPERM);
	}

	sc->sc_state &= ~INTERRUPTED;
	while ((n = min(BUFSIZE, uio->uio_resid)) != 0) {
		sc->sc_cp = sc->sc_inbuf;
		uiomove(sc->sc_cp, n, uio);
		sc->sc_xfercnt = n ;
		while ((sc->sc_xfercnt > 0)&&(sc->sc_irq & LP_USE_IRQ)) {
			lprintf(("i"));
			/* if the printer is ready for a char, */
			/* give it one */
			if ((sc->sc_state & OBUSY) == 0){
				lprintf(("\nC %d. ", sc->sc_xfercnt));
				pl = spltty();
				lpt_intr(sc);
				(void) splx(pl);
			}
			lprintf(("W "));
			if (sc->sc_state & OBUSY)
				if ((err = tsleep (sc,
					 LPPRI|PCATCH, "lpwrite", 0))) {
					sc->sc_state |= INTERRUPTED;
					return(err);
				}
		}
		/* check to see if we must do a polled write */
		if(!(sc->sc_irq & LP_USE_IRQ) && (sc->sc_xfercnt)) {
			lprintf(("p"));
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

static void
lpt_intr(void *arg)
{
}

static	int
lptioctl(struct cdev *dev, u_long cmd, caddr_t data, int flags, struct thread *td)
{
	int	error = 0;
	struct lpt_softc *sc = dev->si_drv1;
	u_char	old_sc_irq;	/* old printer IRQ status */

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
				log(LOG_NOTICE, "%s switched to %s mode\n",
					devtoname(dev),
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
