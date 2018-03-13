/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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

static	int		mse_isa_probe(device_t dev);
static	int		mse_isa_attach(device_t dev);

static	device_method_t	mse_methods[] = {
	DEVMETHOD(device_probe,		mse_isa_probe),
	DEVMETHOD(device_attach,	mse_isa_attach),
	DEVMETHOD(device_detach,	mse_detach),
	{ 0, 0 }
};

static	driver_t	mse_driver = {
	"mse",
	mse_methods,
	sizeof(mse_softc_t),
};

static struct isa_pnp_id mse_ids[] = {
	{ 0x000fd041, "Bus mouse" },			/* PNP0F00 */
	{ 0x020fd041, "InPort mouse" },			/* PNP0F02 */
	{ 0x0d0fd041, "InPort mouse compatible" },	/* PNP0F0D */
	{ 0x110fd041, "Bus mouse compatible" },		/* PNP0F11 */
	{ 0x150fd041, "Logitech bus mouse" },		/* PNP0F15 */
	{ 0x180fd041, "Logitech bus mouse compatible" },/* PNP0F18 */
	{ 0 }
};

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
static	void		mse_disablelogi(struct resource *port);
static	void		mse_getlogi(struct resource *port, int *dx, int *dy,
			    int *but);
static	void		mse_enablelogi(struct resource *port);

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
static	void		mse_enableati(struct resource *port);
static	void		mse_disableati(struct resource *port);
static	void		mse_getati(struct resource *port, int *dx, int *dy,
			    int *but);

static struct mse_types mse_types[] = {
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
mse_isa_probe(device_t dev)
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
	sc->sc_port = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT, &rid,
						  MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;

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
mse_isa_attach(device_t dev)
{
	mse_softc_t *sc;
	int rid;

	sc = device_get_softc(dev);

	rid = 0;
	sc->sc_port = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT, &rid,
						  MSE_IOSIZE, RF_ACTIVE);
	if (sc->sc_port == NULL)
		return ENXIO;

	return (mse_common_attach(dev));
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
mse_probelogi(device_t dev, mse_softc_t *sc)
{

	int sig;

	bus_write_1(sc->sc_port, MSE_PORTD, MSE_SETUP);
		/* set the signature port */
	bus_write_1(sc->sc_port, MSE_PORTB, MSE_LOGI_SIG);

	DELAY(30000); /* 30 ms delay */
	sig = bus_read_1(sc->sc_port, MSE_PORTB) & 0xFF;
	if (sig == MSE_LOGI_SIG) {
		bus_write_1(sc->sc_port, MSE_PORTC, MSE_DISINTR);
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
mse_enablelogi(struct resource *port)
{
	int dx, dy, but;

	bus_write_1(port, MSE_PORTD, MSE_SETUP);
	mse_getlogi(port, &dx, &dy, &but);
}

/*
 * Disable interrupts for Logitech mouse.
 */
static void
mse_disablelogi(struct resource *port)
{

	bus_write_1(port, MSE_PORTC, MSE_DISINTR);
}

/*
 * Get the current dx, dy and button up/down state.
 */
static void
mse_getlogi(struct resource *port, int *dx, int *dy, int *but)
{
	char x, y;

	bus_write_1(port, MSE_PORTC, MSE_HOLD | MSE_RXLOW);
	x = bus_read_1(port, MSE_PORTA);
	*but = (x >> 5) & MOUSE_MSC_BUTTONS;
	x &= 0xf;
	bus_write_1(port, MSE_PORTC, MSE_HOLD | MSE_RXHIGH);
	x |= (bus_read_1(port, MSE_PORTA) << 4);
	bus_write_1(port, MSE_PORTC, MSE_HOLD | MSE_RYLOW);
	y = (bus_read_1(port, MSE_PORTA) & 0xf);
	bus_write_1(port, MSE_PORTC, MSE_HOLD | MSE_RYHIGH);
	y |= (bus_read_1(port, MSE_PORTA) << 4);
	*dx = x;
	*dy = y;
	bus_write_1(port, MSE_PORTC, MSE_INTREN);
}

/*
 * Routines for the ATI Inport bus mouse.
 */
/*
 * Test for an ATI Inport bus mouse and return 1 if it is.
 * (do not enable interrupts)
 */
static int
mse_probeati(device_t dev, mse_softc_t *sc)
{
	int i;

	for (i = 0; i < 2; i++)
		if (bus_read_1(sc->sc_port, MSE_PORTC) == 0xde)
			return (1);
	return (0);
}

/*
 * Initialize ATI Inport mouse and enable interrupts.
 */
static void
mse_enableati(struct resource *port)
{

	bus_write_1(port, MSE_PORTA, MSE_INPORT_RESET);
	bus_write_1(port, MSE_PORTA, MSE_INPORT_MODE);
	bus_write_1(port, MSE_PORTB, MSE_INPORT_INTREN);
}

/*
 * Disable interrupts for ATI Inport mouse.
 */
static void
mse_disableati(struct resource *port)
{

	bus_write_1(port, MSE_PORTA, MSE_INPORT_MODE);
	bus_write_1(port, MSE_PORTB, 0);
}

/*
 * Get current dx, dy and up/down button state.
 */
static void
mse_getati(struct resource *port, int *dx, int *dy, int *but)
{
	char byte;

	bus_write_1(port, MSE_PORTA, MSE_INPORT_MODE);
	bus_write_1(port, MSE_PORTB, MSE_INPORT_HOLD);
	bus_write_1(port, MSE_PORTA, MSE_INPORT_STATUS);
	*but = ~bus_read_1(port, MSE_PORTB) & MOUSE_MSC_BUTTONS;
	bus_write_1(port, MSE_PORTA, MSE_INPORT_DX);
	byte = bus_read_1(port, MSE_PORTB);
	*dx = byte;
	bus_write_1(port, MSE_PORTA, MSE_INPORT_DY);
	byte = bus_read_1(port, MSE_PORTB);
	*dy = byte;
	bus_write_1(port, MSE_PORTA, MSE_INPORT_MODE);
	bus_write_1(port, MSE_PORTB, MSE_INPORT_INTREN);
}

DRIVER_MODULE(mse, isa, mse_driver, mse_devclass, 0, 0);
ISA_PNP_INFO(mse_ids);
