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
 * $FreeBSD$
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/mouse.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

/* driver configuration flags (config) */
#define MSE_CONFIG_ACCEL	0x00f0  /* acceleration factor */
#define MSE_CONFIG_FLAGS	(MSE_CONFIG_ACCEL)

/*
 * Software control structure for mouse. The sc_enablemouse(),
 * sc_disablemouse() and sc_getmouse() routines must be called spl'd().
 */
typedef struct mse_softc {
	int		sc_flags;
	int		sc_mousetype;
	struct selinfo	sc_selp;
	struct resource	*sc_port;
	struct resource	*sc_intr;
	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;
	void		*sc_ih;
	void		(*sc_enablemouse)(bus_space_tag_t t,
			    bus_space_handle_t h);
	void		(*sc_disablemouse)(bus_space_tag_t t,
			    bus_space_handle_t h);
	void		(*sc_getmouse)(bus_space_tag_t t, bus_space_handle_t h,
			    int *dx, int *dy, int *but);
	int		sc_deltax;
	int		sc_deltay;
	int		sc_obuttons;
	int		sc_buttons;
	int		sc_bytesread;
	u_char		sc_bytes[MOUSE_SYS_PACKETSIZE];
	struct		callout_handle sc_callout;
	int		sc_watchdog;
	dev_t		sc_dev;
	dev_t		sc_ndev;
	mousehw_t	hw;
	mousemode_t	mode;
	mousestatus_t	status;
} mse_softc_t;

static	devclass_t	mse_devclass;

static	int		mse_probe(device_t dev);
static	int		mse_attach(device_t dev);
static	int		mse_detach(device_t dev);

static	device_method_t	mse_methods[] = {
	DEVMETHOD(device_probe,		mse_probe),
	DEVMETHOD(device_attach,	mse_attach),
	DEVMETHOD(device_detach,	mse_detach),
	{ 0, 0 }
};

static	driver_t	mse_driver = {
	"mse",
	mse_methods,
	sizeof(mse_softc_t),
};

DRIVER_MODULE(mse, isa, mse_driver, mse_devclass, 0, 0);

static struct isa_pnp_id mse_ids[] = {
	{ 0x000fd041, "Bus mouse" },			/* PNP0F00 */
	{ 0x020fd041, "InPort mouse" },			/* PNP0F02 */
	{ 0x0d0fd041, "InPort mouse compatible" },	/* PNP0F0D */
	{ 0x110fd041, "Bus mouse compatible" },		/* PNP0F11 */
	{ 0x150fd041, "Logitech bus mouse" },		/* PNP0F15 */
	{ 0x180fd041, "Logitech bus mouse compatible" },/* PNP0F18 */
	{ 0 }
};

static	d_open_t	mseopen;
static	d_close_t	mseclose;
static	d_read_t	mseread;
static  d_ioctl_t	mseioctl;
static	d_poll_t	msepoll;

#define CDEV_MAJOR 27
static struct cdevsw mse_cdevsw = {
	/* open */	mseopen,
	/* close */	mseclose,
	/* read */	mseread,
	/* write */	nowrite,
	/* ioctl */	mseioctl,
	/* poll */	msepoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"mse",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static	void		mseintr(void *);
static	timeout_t	msetimeout;

/* Flags */
#define	MSESC_OPEN	0x1
#define	MSESC_WANT	0x2

/* and Mouse Types */
#define	MSE_NONE	0	/* don't move this! */
#define	MSE_LOGITECH	0x1
#define	MSE_ATIINPORT	0x2
#define	MSE_LOGI_SIG	0xA5

#define	MSE_PORTA	0
#define	MSE_PORTB	1
#define	MSE_PORTC	2
#define	MSE_PORTD	3
#define MSE_IOSIZE	4

#define	MSE_UNIT(dev)		(minor(dev) >> 1)
#define	MSE_NBLOCKIO(dev)	(minor(dev) & 0x1)

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

static	int		mse_probelogi(device_t dev, mse_softc_t *sc);
static	void		mse_disablelogi(bus_space_tag_t t,
			    bus_space_handle_t h);
static	void		mse_getlogi(bus_space_tag_t t, bus_space_handle_t h,
			    int *dx, int *dy, int *but);
static	void		mse_enablelogi(bus_space_tag_t t,
			    bus_space_handle_t h);

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

static	int		mse_probeati(device_t dev, mse_softc_t *sc);
static	void		mse_enableati(bus_space_tag_t t, bus_space_handle_t h);
static	void		mse_disableati(bus_space_tag_t t, bus_space_handle_t h);
static	void		mse_getati(bus_space_tag_t t, bus_space_handle_t h,
			    int *dx, int *dy, int *but);

#define	MSEPRI	(PZERO + 3)

/*
 * Table of mouse types.
 * Keep the Logitech last, since I haven't figured out how to probe it
 * properly yet. (Someday I'll have the documentation.)
 */
static struct mse_types {
	int	m_type;		/* Type of bus mouse */
	int	(*m_probe)(device_t dev, mse_softc_t *sc);
				/* Probe routine to test for it */
	void	(*m_enable)(bus_space_tag_t t, bus_space_handle_t h);
				/* Start routine */
	void	(*m_disable)(bus_space_tag_t t, bus_space_handle_t h);
				/* Disable interrupts routine */
	void	(*m_get)(bus_space_tag_t t, bus_space_handle_t h, int *dx,
		    int *dy, int *but);
				/* and get mouse status */
	mousehw_t   m_hw;	/* buttons iftype type model hwid */
	mousemode_t m_mode;	/* proto rate res accel level size mask */
} mse_types[] = {
	{ MSE_ATIINPORT, 
	  mse_probeati, mse_enableati, mse_disableati, mse_getati,
	  { 2, MOUSE_IF_INPORT, MOUSE_MOUSE, MOUSE_MODEL_GENERIC, 0, },
	  { MOUSE_PROTO_INPORT, -1, -1, 0, 0, MOUSE_MSC_PACKETSIZE, 
	    { MOUSE_MSC_SYNCMASK, MOUSE_MSC_SYNC, }, }, },
	{ MSE_LOGITECH, 
	  mse_probelogi, mse_enablelogi, mse_disablelogi, mse_getlogi,
	  { 2, MOUSE_IF_BUS, MOUSE_MOUSE, MOUSE_MODEL_GENERIC, 0, },
	  { MOUSE_PROTO_BUS, -1, -1, 0, 0, MOUSE_MSC_PACKETSIZE, 
	    { MOUSE_MSC_SYNCMASK, MOUSE_MSC_SYNC, }, }, },
	{ 0, },
};

static	int
mse_probe(dev)
	device_t dev;
{
	mse_softc_t *sc;
	int error;
	int rid;
	int i;

	/* check PnP IDs */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, mse_ids);
	if (error == ENXIO)
		return error;

	sc = device_get_softc(dev);
	rid = 0;
	sc->sc_port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
					 MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;
	sc->sc_iot = rman_get_bustag(sc->sc_port);
	sc->sc_ioh = rman_get_bushandle(sc->sc_port);

	/*
	 * Check for each mouse type in the table.
	 */
	i = 0;
	while (mse_types[i].m_type) {
		if ((*mse_types[i].m_probe)(dev, sc)) {
			sc->sc_mousetype = mse_types[i].m_type;
			sc->sc_enablemouse = mse_types[i].m_enable;
			sc->sc_disablemouse = mse_types[i].m_disable;
			sc->sc_getmouse = mse_types[i].m_get;
			sc->hw = mse_types[i].m_hw;
			sc->mode = mse_types[i].m_mode;
			bus_release_resource(dev, SYS_RES_IOPORT, rid,
					     sc->sc_port);
			device_set_desc(dev, "Bus/InPort Mouse");
			return 0;
		}
		i++;
	}
	bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
	return ENXIO;
}

static	int
mse_attach(dev)
	device_t dev;
{
	mse_softc_t *sc;
	int flags;
	int unit;
	int rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	rid = 0;
	sc->sc_port = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
					 MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;
	sc->sc_intr = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
					 RF_ACTIVE);
	if (sc->sc_intr == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		return ENXIO;
	}
	sc->sc_iot = rman_get_bustag(sc->sc_port);
	sc->sc_ioh = rman_get_bushandle(sc->sc_port);

	if (BUS_SETUP_INTR(device_get_parent(dev), dev, sc->sc_intr,
			   INTR_TYPE_TTY, mseintr, sc, &sc->sc_ih)) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		bus_release_resource(dev, SYS_RES_IRQ, rid, sc->sc_intr);
		return ENXIO;
	}

	flags = device_get_flags(dev);
	sc->mode.accelfactor = (flags & MSE_CONFIG_ACCEL) >> 4;
	callout_handle_init(&sc->sc_callout);

	sc->sc_dev = make_dev(&mse_cdevsw, unit << 1, 0, 0, 0600,
			      "mse%d", unit);
	sc->sc_ndev = make_dev(&mse_cdevsw, (unit<<1)+1, 0, 0, 0600,
			       "nmse%d", unit);

	return 0;
}

static	int
mse_detach(dev)
	device_t dev;
{
	mse_softc_t *sc;
	int rid;

	sc = device_get_softc(dev);
	if (sc->sc_flags & MSESC_OPEN)
		return EBUSY;

	rid = 0;
	BUS_TEARDOWN_INTR(device_get_parent(dev), dev, sc->sc_intr, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, rid, sc->sc_intr);
	bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);

	destroy_dev(sc->sc_dev);
	destroy_dev(sc->sc_ndev);

	return 0;
}

/*
 * Exclusive open the mouse, initialize it and enable interrupts.
 */
static	int
mseopen(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{
	mse_softc_t *sc;
	int s;

	sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_mousetype == MSE_NONE)
		return (ENXIO);
	if (sc->sc_flags & MSESC_OPEN)
		return (EBUSY);
	sc->sc_flags |= MSESC_OPEN;
	sc->sc_obuttons = sc->sc_buttons = MOUSE_MSC_BUTTONS;
	sc->sc_deltax = sc->sc_deltay = 0;
	sc->sc_bytesread = sc->mode.packetsize = MOUSE_MSC_PACKETSIZE;
	sc->sc_watchdog = FALSE;
	sc->sc_callout = timeout(msetimeout, dev, hz*2);
	sc->mode.level = 0;
	sc->status.flags = 0;
	sc->status.button = sc->status.obutton = 0;
	sc->status.dx = sc->status.dy = sc->status.dz = 0;

	/*
	 * Initialize mouse interface and enable interrupts.
	 */
	s = spltty();
	(*sc->sc_enablemouse)(sc->sc_iot, sc->sc_ioh);
	splx(s);
	return (0);
}

/*
 * mseclose: just turn off mouse innterrupts.
 */
static	int
mseclose(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{
	mse_softc_t *sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	int s;

	untimeout(msetimeout, dev, sc->sc_callout);
	callout_handle_init(&sc->sc_callout);
	s = spltty();
	(*sc->sc_disablemouse)(sc->sc_iot, sc->sc_ioh);
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
	mse_softc_t *sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	int xfer, s, error;

	/*
	 * If there are no protocol bytes to be read, set up a new protocol
	 * packet.
	 */
	s = spltty(); /* XXX Should be its own spl, but where is imlXX() */
	if (sc->sc_bytesread >= sc->mode.packetsize) {
		while (sc->sc_deltax == 0 && sc->sc_deltay == 0 &&
		       (sc->sc_obuttons ^ sc->sc_buttons) == 0) {
			if (MSE_NBLOCKIO(dev)) {
				splx(s);
				return (0);
			}
			sc->sc_flags |= MSESC_WANT;
			error = tsleep((caddr_t)sc, MSEPRI | PCATCH,
				"mseread", 0);
			if (error) {
				splx(s);
				return (error);
			}
		}

		/*
		 * Generate protocol bytes.
		 * For some reason X386 expects 5 bytes but never uses
		 * the fourth or fifth?
		 */
		sc->sc_bytes[0] = sc->mode.syncmask[1] 
		    | (sc->sc_buttons & ~sc->mode.syncmask[0]);
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
		sc->sc_bytes[5] = sc->sc_bytes[6] = 0;
		sc->sc_bytes[7] = MOUSE_SYS_EXTBUTTONS;
		sc->sc_obuttons = sc->sc_buttons;
		sc->sc_deltax = sc->sc_deltay = 0;
		sc->sc_bytesread = 0;
	}
	splx(s);
	xfer = min(uio->uio_resid, sc->mode.packetsize - sc->sc_bytesread);
	error = uiomove(&sc->sc_bytes[sc->sc_bytesread], xfer, uio);
	if (error)
		return (error);
	sc->sc_bytesread += xfer;
	return(0);
}

/*
 * mseioctl: process ioctl commands.
 */
static int
mseioctl(dev, cmd, addr, flag, td)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct thread *td;
{
	mse_softc_t *sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	mousestatus_t status;
	int err = 0;
	int s;

	switch (cmd) {

	case MOUSE_GETHWINFO:
		s = spltty();
		*(mousehw_t *)addr = sc->hw;
		if (sc->mode.level == 0)
			((mousehw_t *)addr)->model = MOUSE_MODEL_GENERIC;
		splx(s);
		break;

	case MOUSE_GETMODE:
		s = spltty();
		*(mousemode_t *)addr = sc->mode;
		switch (sc->mode.level) {
		case 0:
			break;
		case 1:
			((mousemode_t *)addr)->protocol = MOUSE_PROTO_SYSMOUSE;
	    		((mousemode_t *)addr)->syncmask[0] = MOUSE_SYS_SYNCMASK;
	    		((mousemode_t *)addr)->syncmask[1] = MOUSE_SYS_SYNC;
			break;
		}
		splx(s);
		break;

	case MOUSE_SETMODE:
		switch (((mousemode_t *)addr)->level) {
		case 0:
		case 1:
			break;
		default:
			return (EINVAL);
		}
		if (((mousemode_t *)addr)->accelfactor < -1)
			return (EINVAL);
		else if (((mousemode_t *)addr)->accelfactor >= 0)
			sc->mode.accelfactor = 
			    ((mousemode_t *)addr)->accelfactor;
		sc->mode.level = ((mousemode_t *)addr)->level;
		switch (sc->mode.level) {
		case 0:
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_MSC_PACKETSIZE;
			break;
		case 1:
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_SYS_PACKETSIZE;
			break;
		}
		break;

	case MOUSE_GETLEVEL:
		*(int *)addr = sc->mode.level;
		break;

	case MOUSE_SETLEVEL:
		switch (*(int *)addr) {
		case 0:
			sc->mode.level = *(int *)addr;
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_MSC_PACKETSIZE;
			break;
		case 1:
			sc->mode.level = *(int *)addr;
			sc->sc_bytesread = sc->mode.packetsize 
			    = MOUSE_SYS_PACKETSIZE;
			break;
		default:
			return (EINVAL);
		}
		break;

	case MOUSE_GETSTATUS:
		s = spltty();
		status = sc->status;
		sc->status.flags = 0;
		sc->status.obutton = sc->status.button;
		sc->status.button = 0;
		sc->status.dx = 0;
		sc->status.dy = 0;
		sc->status.dz = 0;
		splx(s);
		*(mousestatus_t *)addr = status;
		break;

	case MOUSE_READSTATE:
	case MOUSE_READDATA:
		return (ENODEV);

#if (defined(MOUSE_GETVARS))
	case MOUSE_GETVARS:
	case MOUSE_SETVARS:
		return (ENODEV);
#endif

	default:
		return (ENOTTY);
	}
	return (err);
}

/*
 * msepoll: check for mouse input to be processed.
 */
static	int
msepoll(dev, events, td)
	dev_t dev;
	int events;
	struct thread *td;
{
	mse_softc_t *sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	int s;
	int revents = 0;

	s = spltty();
	if (events & (POLLIN | POLLRDNORM)) {
		if (sc->sc_bytesread != sc->mode.packetsize ||
		    sc->sc_deltax != 0 || sc->sc_deltay != 0 ||
		    (sc->sc_obuttons ^ sc->sc_buttons) != 0)
			revents |= events & (POLLIN | POLLRDNORM);
		else {
			/*
			 * Since this is an exclusive open device, any previous
			 * proc pointer is trash now, so we can just assign it.
			 */
			selrecord(td, &sc->sc_selp);
		}
	}
	splx(s);
	return (revents);
}

/*
 * msetimeout: watchdog timer routine.
 */
static void
msetimeout(arg)
	void *arg;
{
	dev_t dev;
	mse_softc_t *sc;

	dev = (dev_t)arg;
	sc = devclass_get_softc(mse_devclass, MSE_UNIT(dev));
	if (sc->sc_watchdog) {
		if (bootverbose)
			printf("mse%d: lost interrupt?\n", MSE_UNIT(dev));
		mseintr(sc);
	}
	sc->sc_watchdog = TRUE;
	sc->sc_callout = timeout(msetimeout, dev, hz);
}

/*
 * mseintr: update mouse status. sc_deltax and sc_deltay are accumulative.
 */
static void
mseintr(arg)
	void *arg;
{
	/*
	 * the table to turn MouseSystem button bits (MOUSE_MSC_BUTTON?UP)
	 * into `mousestatus' button bits (MOUSE_BUTTON?DOWN).
	 */
	static int butmap[8] = {
		0, 
		MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON2DOWN, 
		MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON1DOWN, 
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON3DOWN, 
		MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN,
        	MOUSE_BUTTON1DOWN | MOUSE_BUTTON2DOWN | MOUSE_BUTTON3DOWN
	};
	mse_softc_t *sc = arg;
	int dx, dy, but;
	int sign;

#ifdef DEBUG
	static int mse_intrcnt = 0;
	if((mse_intrcnt++ % 10000) == 0)
		printf("mseintr\n");
#endif /* DEBUG */
	if ((sc->sc_flags & MSESC_OPEN) == 0)
		return;

	(*sc->sc_getmouse)(sc->sc_iot, sc->sc_ioh, &dx, &dy, &but);
	if (sc->mode.accelfactor > 0) {
		sign = (dx < 0);
		dx = dx * dx / sc->mode.accelfactor;
		if (dx == 0)
			dx = 1;
		if (sign)
			dx = -dx;
		sign = (dy < 0);
		dy = dy * dy / sc->mode.accelfactor;
		if (dy == 0)
			dy = 1;
		if (sign)
			dy = -dy;
	}
	sc->sc_deltax += dx;
	sc->sc_deltay += dy;
	sc->sc_buttons = but;

	but = butmap[~but & MOUSE_MSC_BUTTONS];
	sc->status.dx += dx;
	sc->status.dy += dy;
	sc->status.flags |= ((dx || dy) ? MOUSE_POSCHANGED : 0)
	    | (sc->status.button ^ but);
	sc->status.button = but;

	sc->sc_watchdog = FALSE;

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

/*
 * Routines for the Logitech mouse.
 */
/*
 * Test for a Logitech bus mouse and return 1 if it is.
 * (until I know how to use the signature port properly, just disable
 *  interrupts and return 1)
 */
static int
mse_probelogi(dev, sc)
	device_t dev;
	mse_softc_t *sc;
{

	int sig;

	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MSE_PORTD, MSE_SETUP);
		/* set the signature port */
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, MSE_PORTB, MSE_LOGI_SIG);

	DELAY(30000); /* 30 ms delay */
	sig = bus_space_read_1(sc->sc_iot, sc->sc_ioh, MSE_PORTB) & 0xFF;
	if (sig == MSE_LOGI_SIG) {
		bus_space_write_1(sc->sc_iot, sc->sc_ioh, MSE_PORTC,
				  MSE_DISINTR);
		return(1);
	} else {
		if (bootverbose)
			device_printf(dev, "wrong signature %x\n", sig);
		return(0);
	}
}

/*
 * Initialize Logitech mouse and enable interrupts.
 */
static void
mse_enablelogi(tag, handle)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
{
	int dx, dy, but;

	bus_space_write_1(tag, handle, MSE_PORTD, MSE_SETUP);
	mse_getlogi(tag, handle, &dx, &dy, &but);
}

/*
 * Disable interrupts for Logitech mouse.
 */
static void
mse_disablelogi(tag, handle)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
{

	bus_space_write_1(tag, handle, MSE_PORTC, MSE_DISINTR);
}

/*
 * Get the current dx, dy and button up/down state.
 */
static void
mse_getlogi(tag, handle, dx, dy, but)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	int *dx;
	int *dy;
	int *but;
{
	register char x, y;

	bus_space_write_1(tag, handle, MSE_PORTC, MSE_HOLD | MSE_RXLOW);
	x = bus_space_read_1(tag, handle, MSE_PORTA);
	*but = (x >> 5) & MOUSE_MSC_BUTTONS;
	x &= 0xf;
	bus_space_write_1(tag, handle, MSE_PORTC, MSE_HOLD | MSE_RXHIGH);
	x |= (bus_space_read_1(tag, handle, MSE_PORTA) << 4);
	bus_space_write_1(tag, handle, MSE_PORTC, MSE_HOLD | MSE_RYLOW);
	y = (bus_space_read_1(tag, handle, MSE_PORTA) & 0xf);
	bus_space_write_1(tag, handle, MSE_PORTC, MSE_HOLD | MSE_RYHIGH);
	y |= (bus_space_read_1(tag, handle, MSE_PORTA) << 4);
	*dx = x;
	*dy = y;
	bus_space_write_1(tag, handle, MSE_PORTC, MSE_INTREN);
}

/*
 * Routines for the ATI Inport bus mouse.
 */
/*
 * Test for a ATI Inport bus mouse and return 1 if it is.
 * (do not enable interrupts)
 */
static int
mse_probeati(dev, sc)
	device_t dev;
	mse_softc_t *sc;
{
	int i;

	for (i = 0; i < 2; i++)
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, MSE_PORTC) == 0xde)
			return (1);
	return (0);
}

/*
 * Initialize ATI Inport mouse and enable interrupts.
 */
static void
mse_enableati(tag, handle)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
{

	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_RESET);
	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_MODE);
	bus_space_write_1(tag, handle, MSE_PORTB, MSE_INPORT_INTREN);
}

/*
 * Disable interrupts for ATI Inport mouse.
 */
static void
mse_disableati(tag, handle)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
{

	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_MODE);
	bus_space_write_1(tag, handle, MSE_PORTB, 0);
}

/*
 * Get current dx, dy and up/down button state.
 */
static void
mse_getati(tag, handle, dx, dy, but)
	bus_space_tag_t tag;
	bus_space_handle_t handle;
	int *dx;
	int *dy;
	int *but;
{
	register char byte;

	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_MODE);
	bus_space_write_1(tag, handle, MSE_PORTB, MSE_INPORT_HOLD);
	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_STATUS);
	*but = ~bus_space_read_1(tag, handle, MSE_PORTB) & MOUSE_MSC_BUTTONS;
	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_DX);
	byte = bus_space_read_1(tag, handle, MSE_PORTB);
	*dx = byte;
	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_DY);
	byte = bus_space_read_1(tag, handle, MSE_PORTB);
	*dy = byte;
	bus_space_write_1(tag, handle, MSE_PORTA, MSE_INPORT_MODE);
	bus_space_write_1(tag, handle, MSE_PORTB, MSE_INPORT_INTREN);
}
