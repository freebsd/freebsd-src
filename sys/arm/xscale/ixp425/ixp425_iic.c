/*
 * Copyright (c) 2006 Kevin Lo. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/ixp425_iic.c,v 1.1.4.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/uio.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>
#include <arm/xscale/ixp425/ixdp425reg.h>

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbb_if.h"

#define I2C_DELAY	10

/* bit clr/set shorthands */
#define	GPIO_CONF_CLR(sc, reg, mask)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, reg) &~ (mask))
#define	GPIO_CONF_SET(sc, reg, mask)	\
	GPIO_CONF_WRITE_4(sc, reg, GPIO_CONF_READ_4(sc, reg) | (mask))

struct ixpiic_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;

	device_t		iicbb;
};

static struct ixpiic_softc *ixpiic_sc = NULL;

static int
ixpiic_probe(device_t dev)
{
	device_set_desc(dev, "IXP425 GPIO-Based I2C Interface");
	return (0);
}

static int
ixpiic_attach(device_t dev)
{
	struct ixpiic_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	ixpiic_sc = sc;

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	sc->sc_gpio_ioh = sa->sc_gpio_ioh;

	GPIO_CONF_SET(sc, IXP425_GPIO_GPOER,
		GPIO_I2C_SCL_BIT | GPIO_I2C_SDA_BIT);
	GPIO_CONF_CLR(sc, IXP425_GPIO_GPOUTR,
		GPIO_I2C_SCL_BIT | GPIO_I2C_SDA_BIT);

	/* add generic bit-banging code */	
	if ((sc->iicbb = device_add_child(dev, "iicbb", -1)) == NULL)
		device_printf(dev, "could not add iicbb\n");

	/* probe and attach the bit-banging code */
	device_probe_and_attach(sc->iicbb);

	return (0);
}

static int
ixpiic_callback(device_t dev, int index, caddr_t *data)
{
	return (0);
}

static int 
ixpiic_getscl(device_t dev)
{
	struct ixpiic_softc *sc = ixpiic_sc;
	uint32_t reg;

	GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SCL_BIT);

	reg = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR);
	return (reg & GPIO_I2C_SCL_BIT);
}

static int 
ixpiic_getsda(device_t dev)
{
	struct ixpiic_softc *sc = ixpiic_sc;
	uint32_t reg;

	GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);

	reg = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPINR);
	return (reg & GPIO_I2C_SDA_BIT);
}

static void 
ixpiic_setsda(device_t dev, char val)
{
	struct ixpiic_softc *sc = ixpiic_sc;

	GPIO_CONF_CLR(sc, IXP425_GPIO_GPOUTR, GPIO_I2C_SDA_BIT);
	if (val)
		GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);
	else
		GPIO_CONF_CLR(sc, IXP425_GPIO_GPOER, GPIO_I2C_SDA_BIT);
	DELAY(I2C_DELAY);
}

static void 
ixpiic_setscl(device_t dev, char val)
{
	struct ixpiic_softc *sc = ixpiic_sc;

	GPIO_CONF_CLR(sc, IXP425_GPIO_GPOUTR, GPIO_I2C_SCL_BIT);
	if (val)
		GPIO_CONF_SET(sc, IXP425_GPIO_GPOER, GPIO_I2C_SCL_BIT);
	else
		GPIO_CONF_CLR(sc, IXP425_GPIO_GPOER, GPIO_I2C_SCL_BIT);
	DELAY(I2C_DELAY);
}

static int
ixpiic_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	/* reset bus */
	ixpiic_setsda(dev, 1);
	ixpiic_setscl(dev, 1);

	return (IIC_ENOADDR);
}

static device_method_t ixpiic_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ixpiic_probe),
	DEVMETHOD(device_attach,	ixpiic_attach),

	/* iicbb interface */
	DEVMETHOD(iicbb_callback,	ixpiic_callback),
	DEVMETHOD(iicbb_setsda,		ixpiic_setsda),
	DEVMETHOD(iicbb_setscl,		ixpiic_setscl),
	DEVMETHOD(iicbb_getsda,		ixpiic_getsda),
	DEVMETHOD(iicbb_getscl,		ixpiic_getscl),
	DEVMETHOD(iicbb_reset,		ixpiic_reset),

	{ 0, 0 }
};

static driver_t ixpiic_driver = {
	"ixpiic",
	ixpiic_methods,
	sizeof(struct ixpiic_softc),
};
static devclass_t ixpiic_devclass;

DRIVER_MODULE(ixpiic, ixp, ixpiic_driver, ixpiic_devclass, 0, 0);
