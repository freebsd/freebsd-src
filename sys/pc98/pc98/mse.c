/*
 * Copyright 1992 by the University of Guelph
 *
 * Permission to use, copy and modify this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation.
 * University of Guelph makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * $Id: mse.c,v 1.11 1997/07/21 13:11:06 kato Exp $
 */
/*
 * Driver for the Logitech and ATI Inport Bus mice for use with 386bsd and
 * the X386 port, courtesy of
 * Rick Macklem, rick@snowhite.cis.uoguelph.ca
 * Caveats: The driver currently uses spltty(), but doesn't use any
 * generic tty code. It could use splmse() (that only masks off the
 * bus mouse interrupt, but that would require hacking in i386/isa/icu.s.
 * (This may be worth the effort, since the Logitech generates 30/60
 * interrupts/sec continuously while it is open.)
 * NB: The ATI has NOT been tested yet!
 */

/*
 * Modification history:
 * Sep 6, 1994 -- Lars Fredriksen(fredriks@mcs.com)
 *   improved probe based on input from Logitech.
 *
 * Oct 19, 1992 -- E. Stark (stark@cs.sunysb.edu)
 *   fixes to make it work with Microsoft InPort busmouse
 *
 * Jan, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   added patches for new "select" interface
 *
 * May 4, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   changed position of some spl()'s in mseread
 *
 * October 8, 1993 -- E. Stark (stark@cs.sunysb.edu)
 *   limit maximum negative x/y value to -127 to work around XFree problem
 *   that causes spurious button pushes.
 */

#include "mse.h"
#if NMSE > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/uio.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>


static int mseprobe(struct isa_device *);
static int mseattach(struct isa_device *);

struct	isa_driver msedriver = {
	mseprobe, mseattach, "mse"
};


static	d_open_t	mseopen;
static	d_close_t	mseclose;
static	d_read_t	mseread;
static	d_poll_t	msepoll;

#define CDEV_MAJOR 27
static struct cdevsw mse_cdevsw = 
	{ mseopen,	mseclose,	mseread,	nowrite,	/*27*/
	  noioc,	nostop,		nullreset,	nodevtotty,/* mse */
	  msepoll,	nommap,		NULL,	"mse",	NULL,	-1 };


/*
 * Software control structure for mouse. The sc_enablemouse(),
 * sc_disablemouse() and sc_getmouse() routines must be called spl'd().
 */
#define	PROTOBYTES	5
static struct mse_softc {
	int	sc_flags;
	int	sc_mousetype;
	struct	selinfo sc_selp;
	u_int	sc_port;
	void	(*sc_enablemouse) __P((u_int port));
	void	(*sc_disablemouse) __P((u_int port));
	void	(*sc_getmouse) __P((u_int port, int *dx, int *dy, int *but));
	int	sc_deltax;
	int	sc_deltay;
	int	sc_obuttons;
	int	sc_buttons;
	int	sc_bytesread;
	u_char	sc_bytes[PROTOBYTES];
#ifdef DEVFS
	void 	*devfs_token;
	void	*n_devfs_token;
#endif
} mse_sc[NMSE];

/* Flags */
#define	MSESC_OPEN	0x1
#define	MSESC_WANT	0x2

/* and Mouse Types */
#define	MSE_NONE	0	/* don't move this! */
#ifdef PC98
#define	MSE_98BUSMOUSE	0x1
#else
#define	MSE_LOGITECH	0x1
#define	MSE_ATIINPORT	0x2
#define	MSE_LOGI_SIG	0xA5
#endif

#ifdef PC98
#define NORMAL_MSPORT	0x7fd9
#define       PORT_A  0
#define       PORT_B  2
#define       PORT_C  4

#else /* IBM_PC */

#define	MSE_PORTA	0
#define	MSE_PORTB	1
#define	MSE_PORTC	2
#define	MSE_PORTD	3
#endif

#define	MSE_UNIT(dev)		(minor(dev) >> 1)
#define	MSE_NBLOCKIO(dev)	(minor(dev) & 0x1)

#ifdef PC98
/*
 * PC-9801 Bus mouse definitions
 */

#define	MODE	6
#define	HC	6
#define	INT	6

#define	XL	0x00
#define	XH	0x20
#define	YL	0x40
#define	YH	0x60

#define	INT_ENABLE	0x8
#define	INT_DISABLE	0x9
#define	HC_NO_CLEAR	0xe
#define	HC_CLEAR	0xf

#define	NORMAL_MSIRQ	IRQ13	/* INT6 */

static	int	msport;
static	int	msirq;

static int mse_probe98m __P((struct isa_device *idp));
static void mse_disable98m __P((u_int port));
static void mse_get98m __P((u_int port, int *dx, int *dy, int *but));
static void mse_enable98m __P((u_int port));
#else
/*
 * Logitech bus mouse definitions
 */
#define	MSE_SETUP	0x91	/* What does this mean? */
				/* The definition for the control port */
				/* is as follows: */

				/* D7 	 =  Mode set flag (1 = active) 	*/
				/* D6,D5 =  Mode selection (port A) 	*/
				/* 	    00 = Mode 0 = Basic I/O 	*/
				/* 	    01 = Mode 1 = Strobed I/O 	*/
				/* 	    10 = Mode 2 = Bi-dir bus 	*/
				/* D4	 =  Port A direction (1 = input)*/
				/* D3	 =  Port C (upper 4 bits) 	*/
				/*	    direction. (1 = input)	*/
				/* D2	 =  Mode selection (port B & C) */
				/*	    0 = Mode 0 = Basic I/O	*/
				/*	    1 = Mode 1 = Strobed I/O	*/
				/* D1	 =  Port B direction (1 = input)*/
				/* D0	 =  Port C (lower 4 bits)	*/
				/*	    direction. (1 = input)	*/

				/* So 91 means Basic I/O on all 3 ports,*/
				/* Port A is an input port, B is an 	*/
				/* output port, C is split with upper	*/
				/* 4 bits being an output port and lower*/
				/* 4 bits an input port, and enable the */
				/* sucker.				*/
				/* Courtesy Intel 8255 databook. Lars   */
#define	MSE_HOLD	0x80
#define	MSE_RXLOW	0x00
#define	MSE_RXHIGH	0x20
#define	MSE_RYLOW	0x40
#define	MSE_RYHIGH	0x60
#define	MSE_DISINTR	0x10
#define MSE_INTREN	0x00

static int mse_probelogi __P((struct isa_device *idp));
static void mse_disablelogi __P((u_int port));
static void mse_getlogi __P((u_int port, int *dx, int *dy, int *but));
static void mse_enablelogi __P((u_int port));

/*
 * ATI Inport mouse definitions
 */
#define	MSE_INPORT_RESET	0x80
#define	MSE_INPORT_STATUS	0x00
#define	MSE_INPORT_DX		0x01
#define	MSE_INPORT_DY		0x02
#define	MSE_INPORT_MODE		0x07
#define	MSE_INPORT_HOLD		0x20
#define	MSE_INPORT_INTREN	0x09

static int mse_probeati __P((struct isa_device *idp));
static void mse_enableati __P((u_int port));
static void mse_disableati __P((u_int port));
static void mse_getati __P((u_int port, int *dx, int *dy, int *but));
#endif

#define	MSEPRI	(PZERO + 3)

/*
 * Table of mouse types.
 * Keep the Logitech last, since I haven't figured out how to probe it
 * properly yet. (Someday I'll have the documentation.)
 */
static struct mse_types {
	int	m_type;		/* Type of bus mouse */
	int	(*m_probe) __P((struct isa_device *idp));
				/* Probe routine to test for it */
	void	(*m_enable) __P((u_int port));
				/* Start routine */
	void	(*m_disable) __P((u_int port));
				/* Disable interrupts routine */
	void	(*m_get) __P((u_int port, int *dx, int *dy, int *but));
				/* and get mouse status */
} mse_types[] = {
#ifdef PC98
	{ MSE_98BUSMOUSE, mse_probe98m, mse_enable98m, mse_disable98m, mse_get98m },
#else
	{ MSE_ATIINPORT, mse_probeati, mse_enableati, mse_disableati, mse_getati },
	{ MSE_LOGITECH, mse_probelogi, mse_enablelogi, mse_disablelogi, mse_getlogi },
#endif
	{ 0, },
};

int
mseprobe(idp)
	register struct isa_device *idp;
{
	register struct mse_softc *sc = &mse_sc[idp->id_unit];
	register int i;

	/*
	 * Check for each mouse type in the table.
	 */
	i = 0;
	while (mse_types[i].m_type) {
		if ((*mse_types[i].m_probe)(idp)) {
			sc->sc_mousetype = mse_types[i].m_type;
			sc->sc_enablemouse = mse_types[i].m_enable;
			sc->sc_disablemouse = mse_types[i].m_disable;
			sc->sc_getmouse = mse_types[i].m_get;
			return (1);
		}
		i++;
	}
	return (0);
}

int
mseattach(idp)
	struct isa_device *idp;
{
	int unit = idp->id_unit;
	struct mse_softc *sc = &mse_sc[unit];

#ifdef PC98
	if (msport != idp->id_iobase) {
		idp->id_iobase = msport;
		printf(" [ioport is changed to #0x%x]", msport);
	}

	if (msirq != idp->id_irq) {
		idp->id_irq = msirq;
		printf(" [irq is changed to IR%d]", ffs(msirq)-1);
	}
#endif

	sc->sc_port = idp->id_iobase;
#ifdef	DEVFS
	sc->devfs_token = 
		devfs_add_devswf(&mse_cdevsw, unit << 1, DV_CHR, 0, 0, 
				 0600, "mse%d", unit);
	sc->n_devfs_token = 
		devfs_add_devswf(&mse_cdevsw, (unit<<1)+1, DV_CHR,0, 0, 
				 0600, "nmse%d", unit);
#endif
	return (1);
}

/*
 * Exclusive open the mouse, initialize it and enable interrupts.
 */
static	int
mseopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	register struct mse_softc *sc;
	int s;

	if (MSE_UNIT(dev) >= NMSE)
		return (ENXIO);
	sc = &mse_sc[MSE_UNIT(dev)];
	if (sc->sc_mousetype == MSE_NONE)
		return (ENXIO);
	if (sc->sc_flags & MSESC_OPEN)
		return (EBUSY);
	sc->sc_flags |= MSESC_OPEN;
	sc->sc_obuttons = sc->sc_buttons = 0x7;
	sc->sc_deltax = sc->sc_deltay = 0;
	sc->sc_bytesread = PROTOBYTES;

	/*
	 * Initialize mouse interface and enable interrupts.
	 */
	s = spltty();
	(*sc->sc_enablemouse)(sc->sc_port);
	splx(s);
	return (0);
}

/*
 * mseclose: just turn off mouse innterrupts.
 */
static	int
mseclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct mse_softc *sc = &mse_sc[MSE_UNIT(dev)];
	int s;

	s = spltty();
	(*sc->sc_disablemouse)(sc->sc_port);
	sc->sc_flags &= ~MSESC_OPEN;
	splx(s);
	return(0);
}

/*
 * mseread: return mouse info using the MSC serial protocol, but without
 * using bytes 4 and 5.
 * (Yes this is cheesy, but it makes the X386 server happy, so...)
 */
static	int
mseread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{
	register struct mse_softc *sc = &mse_sc[MSE_UNIT(dev)];
	int xfer, s, error;

	/*
	 * If there are no protocol bytes to be read, set up a new protocol
	 * packet.
	 */
	s = spltty(); /* XXX Should be its own spl, but where is imlXX() */
	if (sc->sc_bytesread >= PROTOBYTES) {
		while (sc->sc_deltax == 0 && sc->sc_deltay == 0 &&
		       (sc->sc_obuttons ^ sc->sc_buttons) == 0) {
			if (MSE_NBLOCKIO(dev)) {
				splx(s);
				return (0);
			}
			sc->sc_flags |= MSESC_WANT;
			if (error = tsleep((caddr_t)sc, MSEPRI | PCATCH,
				"mseread", 0)) {
				splx(s);
				return (error);
			}
		}

		/*
		 * Generate protocol bytes.
		 * For some reason X386 expects 5 bytes but never uses
		 * the fourth or fifth?
		 */
		sc->sc_bytes[0] = 0x80 | (sc->sc_buttons & ~0xf8);
		if (sc->sc_deltax > 127)
			sc->sc_deltax = 127;
		if (sc->sc_deltax < -127)
			sc->sc_deltax = -127;
		sc->sc_deltay = -sc->sc_deltay;	/* Otherwise mousey goes wrong way */
		if (sc->sc_deltay > 127)
			sc->sc_deltay = 127;
		if (sc->sc_deltay < -127)
			sc->sc_deltay = -127;
		sc->sc_bytes[1] = sc->sc_deltax;
		sc->sc_bytes[2] = sc->sc_deltay;
		sc->sc_bytes[3] = sc->sc_bytes[4] = 0;
		sc->sc_obuttons = sc->sc_buttons;
		sc->sc_deltax = sc->sc_deltay = 0;
		sc->sc_bytesread = 0;
	}
	splx(s);
	xfer = min(uio->uio_resid, PROTOBYTES - sc->sc_bytesread);
	if (error = uiomove(&sc->sc_bytes[sc->sc_bytesread], xfer, uio))
		return (error);
	sc->sc_bytesread += xfer;
	return(0);
}

/*
 * msepoll: check for mouse input to be processed.
 */
static	int
msepoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	register struct mse_softc *sc = &mse_sc[MSE_UNIT(dev)];
	int s;
	int revents = 0;

	s = spltty();
	if (events & (POLLIN | POLLRDNORM))
		if (sc->sc_bytesread != PROTOBYTES || sc->sc_deltax != 0 ||
		    sc->sc_deltay != 0 ||
		    (sc->sc_obuttons ^ sc->sc_buttons) != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			/*
			 * Since this is an exclusive open device, any previous
			 * proc pointer is trash now, so we can just assign it.
			 */
			selrecord(p, &sc->sc_selp);
		}

	splx(s);
	return (revents);
}

/*
 * mseintr: update mouse status. sc_deltax and sc_deltay are accumulative.
 */
void
mseintr(unit)
	int unit;
{
	register struct mse_softc *sc = &mse_sc[unit];

#ifdef DEBUG
	static int mse_intrcnt = 0;
	if((mse_intrcnt++ % 10000) == 0)
		printf("mseintr\n");
#endif /* DEBUG */
	if ((sc->sc_flags & MSESC_OPEN) == 0)
		return;

	(*sc->sc_getmouse)(sc->sc_port, &sc->sc_deltax, &sc->sc_deltay, &sc->sc_buttons);

	/*
	 * If mouse state has changed, wake up anyone wanting to know.
	 */
	if (sc->sc_deltax != 0 || sc->sc_deltay != 0 ||
	    (sc->sc_obuttons ^ sc->sc_buttons) != 0) {
		if (sc->sc_flags & MSESC_WANT) {
			sc->sc_flags &= ~MSESC_WANT;
			wakeup((caddr_t)sc);
		}
		selwakeup(&sc->sc_selp);
	}
}

#ifndef PC98
/*
 * Routines for the Logitech mouse.
 */
/*
 * Test for a Logitech bus mouse and return 1 if it is.
 * (until I know how to use the signature port properly, just disable
 *  interrupts and return 1)
 */
static int
mse_probelogi(idp)
	register struct isa_device *idp;
{

	int sig;

	outb(idp->id_iobase + MSE_PORTD, MSE_SETUP);
		/* set the signature port */
	outb(idp->id_iobase + MSE_PORTB, MSE_LOGI_SIG);

	DELAY(30000); /* 30 ms delay */
	sig = inb(idp->id_iobase + MSE_PORTB) & 0xFF;
	if (sig == MSE_LOGI_SIG) {
		outb(idp->id_iobase + MSE_PORTC, MSE_DISINTR);
		return(1);
	} else {
		if (bootverbose)
			printf("mse%d: wrong signature %x\n",idp->id_unit,sig);
		return(0);
	}
}

/*
 * Initialize Logitech mouse and enable interrupts.
 */
static void
mse_enablelogi(port)
	register u_int port;
{
	int dx, dy, but;

	outb(port + MSE_PORTD, MSE_SETUP);
	mse_getlogi(port, &dx, &dy, &but);
}

/*
 * Disable interrupts for Logitech mouse.
 */
static void
mse_disablelogi(port)
	register u_int port;
{

	outb(port + MSE_PORTC, MSE_DISINTR);
}

/*
 * Get the current dx, dy and button up/down state.
 */
static void
mse_getlogi(port, dx, dy, but)
	register u_int port;
	int *dx;
	int *dy;
	int *but;
{
	register char x, y;

	outb(port + MSE_PORTC, MSE_HOLD | MSE_RXLOW);
	x = inb(port + MSE_PORTA);
	*but = (x >> 5) & 0x7;
	x &= 0xf;
	outb(port + MSE_PORTC, MSE_HOLD | MSE_RXHIGH);
	x |= (inb(port + MSE_PORTA) << 4);
	outb(port + MSE_PORTC, MSE_HOLD | MSE_RYLOW);
	y = (inb(port + MSE_PORTA) & 0xf);
	outb(port + MSE_PORTC, MSE_HOLD | MSE_RYHIGH);
	y |= (inb(port + MSE_PORTA) << 4);
	*dx += x;
	*dy += y;
	outb(port + MSE_PORTC, MSE_INTREN);
}

/*
 * Routines for the ATI Inport bus mouse.
 */
/*
 * Test for a ATI Inport bus mouse and return 1 if it is.
 * (do not enable interrupts)
 */
static int
mse_probeati(idp)
	register struct isa_device *idp;
{
	int i;

	for (i = 0; i < 2; i++)
		if (inb(idp->id_iobase + MSE_PORTC) == 0xde)
			return (1);
	return (0);
}

/*
 * Initialize ATI Inport mouse and enable interrupts.
 */
static void
mse_enableati(port)
	register u_int port;
{

	outb(port + MSE_PORTA, MSE_INPORT_RESET);
	outb(port + MSE_PORTA, MSE_INPORT_MODE);
	outb(port + MSE_PORTB, MSE_INPORT_INTREN);
}

/*
 * Disable interrupts for ATI Inport mouse.
 */
static void
mse_disableati(port)
	register u_int port;
{

	outb(port + MSE_PORTA, MSE_INPORT_MODE);
	outb(port + MSE_PORTB, 0);
}

/*
 * Get current dx, dy and up/down button state.
 */
static void
mse_getati(port, dx, dy, but)
	register u_int port;
	int *dx;
	int *dy;
	int *but;
{
	register char byte;

	outb(port + MSE_PORTA, MSE_INPORT_MODE);
	outb(port + MSE_PORTB, MSE_INPORT_HOLD);
	outb(port + MSE_PORTA, MSE_INPORT_STATUS);
	*but = ~(inb(port + MSE_PORTB) & 0x7);
	outb(port + MSE_PORTA, MSE_INPORT_DX);
	byte = inb(port + MSE_PORTB);
	*dx += byte;
	outb(port + MSE_PORTA, MSE_INPORT_DY);
	byte = inb(port + MSE_PORTB);
	*dy += byte;
	outb(port + MSE_PORTA, MSE_INPORT_MODE);
	outb(port + MSE_PORTB, MSE_INPORT_INTREN);
}
#endif

#ifdef PC98

/*
 * Routines for the PC98 bus mouse.
 */

/*
 * Test for a PC98 bus mouse and return 1 if it is.
 * (do not enable interrupts)
 */
static int
mse_probe98m(idp)
	register struct isa_device *idp;
{

	msport = NORMAL_MSPORT;
	msirq = NORMAL_MSIRQ;
	
	/* mode set */
	outb(msport + MODE, 0x93);
	/* initialize */
	outb(msport + INT, INT_DISABLE);	/* INT disable */
	outb(msport + HC, HC_NO_CLEAR);		/* HC = 0 */
	outb(msport + HC, HC_CLEAR);		/* HC = 1 */
	return (1);
}

/*
 * Initialize PC98 bus mouse and enable interrupts.
 */
static void
mse_enable98m(port)
	register u_int port;
{
	outb(port + INT, INT_ENABLE);/* INT enable */
	outb(port + HC, HC_NO_CLEAR); /* HC = 0 */
	outb(port + HC, HC_CLEAR);	/* HC = 1 */
}
 
/*
 * Disable interrupts for PC98 Bus mouse.
 */
static void
mse_disable98m(port)
	register u_int port;
{
	outb(port + INT, INT_DISABLE);/* INT disable */
	outb(port + HC, HC_NO_CLEAR); /* HC = 0 */
	outb(port + HC, HC_CLEAR);	/* HC = 1 */
}

/*
 * Get current dx, dy and up/down button state.
 */
static void
mse_get98m(port, dx, dy, but)
	register u_int port;
	int *dx;
	int *dy;
	int *but;
{
	register char x, y;

	outb(port + INT, INT_DISABLE); /* INT disable */

	outb(port + HC, HC_CLEAR);	/* HC = 1 */

	outb(port + PORT_C, 0x90 | XL);
	x = inb(port + PORT_A) & 0x0f;			/* X low */
	outb(port + PORT_C, 0x90 | XH);
	x |= ((inb(port + PORT_A)  & 0x0f) << 4);	/* X high */

	outb(port + PORT_C, 0x90 | YL);
	y = (inb(port + PORT_A) & 0x0f);		/* Y low */
	outb(port + PORT_C, 0x90 | YH);
	y |= ((inb(port + PORT_A) & 0x0f) << 4);

	*but = (inb(port + PORT_A) >> 5) & 7;

	*dx += x;
	*dy += y;

	outb(port + HC, HC_NO_CLEAR);	/* HC = 0 */

	outb(port + INT, INT_ENABLE); /* INT enable */
}
#endif

static mse_devsw_installed = 0;

static void 	mse_drvinit(void *unused)
{
	dev_t dev;

	if( ! mse_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&mse_cdevsw, NULL);
		mse_devsw_installed = 1;
    	}
}

SYSINIT(msedev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,mse_drvinit,NULL)


#endif /* NMSE */
