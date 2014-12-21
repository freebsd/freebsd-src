/*-
 * Copyright (c) 2004 M. Warner Losh
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*-
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/poll.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#include <sys/mouse.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/mse/msevar.h>

static	int		mse_cbus_probe(device_t dev);
static	int		mse_cbus_attach(device_t dev);

static	device_method_t	mse_methods[] = {
	DEVMETHOD(device_probe,		mse_cbus_probe),
	DEVMETHOD(device_attach,	mse_cbus_attach),
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
#if 0
	{ 0x001fa3b8, "PC-98 bus mouse" },		/* NEC1F00 */
#endif
	{ 0 }
};

/*
 * PC-9801 Bus mouse definitions
 */

#define	MODE	MSE_PORTD
#define	HC	MSE_PORTD
#define	INT	MSE_PORTD

#define	XL	0x00
#define	XH	0x20
#define	YL	0x40
#define	YH	0x60

#define	INT_ENABLE	0x8
#define	INT_DISABLE	0x9
#define	HC_NO_CLEAR	0xe
#define	HC_CLEAR	0xf

static	bus_addr_t	mse_port[] = {0, 2, 4, 6};

static	int		mse_probe98m(device_t dev, mse_softc_t *sc);
static	void		mse_disable98m(struct resource *port);
static	void		mse_get98m(struct resource *port,
			    int *dx, int *dy, int *but);
static	void		mse_enable98m(struct resource *port);

static struct mse_types mse_types[] = {
	{ MSE_98BUSMOUSE,
	  mse_probe98m, mse_enable98m, mse_disable98m, mse_get98m,
	  { 2, MOUSE_IF_BUS, MOUSE_MOUSE, MOUSE_MODEL_GENERIC, 0, },
	  { MOUSE_PROTO_BUS, -1, -1, 0, 0, MOUSE_MSC_PACKETSIZE, 
	    { MOUSE_MSC_SYNCMASK, MOUSE_MSC_SYNC, }, }, },
	{ 0, },
};

static int
mse_cbus_probe(device_t dev)
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
	sc->sc_port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, mse_port,
					  MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;
	if (isa_load_resourcev(sc->sc_port, mse_port, MSE_IOSIZE)) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		return ENXIO;
	}

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

static int
mse_cbus_attach(device_t dev)
{
	mse_softc_t *sc;
	int rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->sc_port = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, mse_port,
					  MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;
	if (isa_load_resourcev(sc->sc_port, mse_port, MSE_IOSIZE)) {
		bus_release_resource(dev, SYS_RES_IOPORT, rid, sc->sc_port);
		return ENXIO;
	}

	return (mse_common_attach(dev));
}

/*
 * Routines for the PC98 bus mouse.
 */

/*
 * Test for a PC98 bus mouse and return 1 if it is.
 * (do not enable interrupts)
 */
static int
mse_probe98m(device_t dev, mse_softc_t *sc)
{
	/* mode set */
	bus_write_1(sc->sc_port, MODE, 0x93);

	/* initialize */
	/* INT disable */
	bus_write_1(sc->sc_port, INT, INT_DISABLE);
	/* HC = 0 */
	bus_write_1(sc->sc_port, HC, HC_NO_CLEAR);
	/* HC = 1 */
	bus_write_1(sc->sc_port, HC, HC_CLEAR);

	return (1);
}

/*
 * Initialize PC98 bus mouse and enable interrupts.
 */
static void
mse_enable98m(struct resource *port)
{
	bus_write_1(port, INT, INT_ENABLE);    /* INT enable */
	bus_write_1(port, HC, HC_NO_CLEAR);    /* HC = 0 */
	bus_write_1(port, HC, HC_CLEAR);       /* HC = 1 */
}
 
/*
 * Disable interrupts for PC98 Bus mouse.
 */
static void
mse_disable98m(struct resource *port)
{
	bus_write_1(port, INT, INT_DISABLE);   /* INT disable */
	bus_write_1(port, HC, HC_NO_CLEAR);    /* HC = 0 */
	bus_write_1(port, HC, HC_CLEAR);       /* HC = 1 */
}

/*
 * Get current dx, dy and up/down button state.
 */
static void
mse_get98m(struct resource *port, int *dx, int *dy, int *but)
{
	register char x, y;

	bus_write_1(port, INT, INT_DISABLE);   /* INT disable */

	bus_write_1(port, HC, HC_CLEAR);       /* HC = 1 */

	/* X low */
	bus_write_1(port, MSE_PORTC, 0x90 | XL);
	x = bus_read_1(port, MSE_PORTA) & 0x0f;
	/* X high */
	bus_write_1(port, MSE_PORTC, 0x90 | XH);
	x |= ((bus_read_1(port, MSE_PORTA)  & 0x0f) << 4);

	/* Y low */
	bus_write_1(port, MSE_PORTC, 0x90 | YL);
	y = (bus_read_1(port, MSE_PORTA) & 0x0f);
	/* Y high */
	bus_write_1(port, MSE_PORTC, 0x90 | YH);
	y |= ((bus_read_1(port, MSE_PORTA) & 0x0f) << 4);

	*but = (bus_read_1(port, MSE_PORTA) >> 5) & 7;

	*dx = x;
	*dy = y;

	bus_write_1(port, HC, HC_NO_CLEAR);    /* HC = 0 */

	bus_write_1(port, INT, INT_ENABLE);    /* INT enable */
}
