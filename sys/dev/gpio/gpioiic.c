/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (c) 2010 Luiz Otavio O Souza
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <sys/gpio.h>
#include "gpiobus_if.h"

#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>

#include "iicbb_if.h"

#define	SCL_PIN_DEFAULT	0	/* default index of SCL pin on gpiobus */
#define	SDA_PIN_DEFAULT	1

struct gpioiic_softc 
{
	device_t	sc_dev;
	device_t	sc_busdev;
	struct cdev	*sc_leddev;
	int		scl_pin;
	int		sda_pin;
};

static int gpioiic_probe(device_t);
static int gpioiic_attach(device_t);

/* iicbb interface */
static void gpioiic_reset_bus(device_t);
static int gpioiic_callback(device_t, int, caddr_t);
static void gpioiic_setsda(device_t, int);
static void gpioiic_setscl(device_t, int);
static int gpioiic_getsda(device_t);
static int gpioiic_getscl(device_t);
static int gpioiic_reset(device_t, u_char, u_char, u_char *);


static int
gpioiic_probe(device_t dev)
{

	device_set_desc(dev, "GPIO I2C bit-banging driver");
	return (0);
}

static int
gpioiic_attach(device_t dev)
{
	struct gpioiic_softc	*sc = device_get_softc(dev);
	device_t		bitbang;

	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);
	if (resource_int_value(device_get_name(dev),
		device_get_unit(dev), "scl", &sc->scl_pin))
		sc->scl_pin = SCL_PIN_DEFAULT;
	if (resource_int_value(device_get_name(dev),
		device_get_unit(dev), "sda", &sc->sda_pin))
		sc->sda_pin = SDA_PIN_DEFAULT;

	/* add generic bit-banging code */
	bitbang = device_add_child(dev, "iicbb", -1);
	device_probe_and_attach(bitbang);

	return (0);
}

/*
 * Reset bus by setting SDA first and then SCL. 
 * Must always be called with gpio bus locked.
 */
static void
gpioiic_reset_bus(device_t dev)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);

	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->sda_pin,
	    GPIO_PIN_INPUT);
	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->scl_pin,
	    GPIO_PIN_INPUT);
}

static int
gpioiic_callback(device_t dev, int index, caddr_t data)
{
	struct gpioiic_softc	*sc = device_get_softc(dev);
	int			error = 0;

	switch (index) {
	case IIC_REQUEST_BUS:
		GPIOBUS_LOCK_BUS(sc->sc_busdev);
		GPIOBUS_ACQUIRE_BUS(sc->sc_busdev, sc->sc_dev);
		GPIOBUS_UNLOCK_BUS(sc->sc_busdev);
		break;
	case IIC_RELEASE_BUS:
		GPIOBUS_LOCK_BUS(sc->sc_busdev);
		GPIOBUS_RELEASE_BUS(sc->sc_busdev, sc->sc_dev);
		GPIOBUS_UNLOCK_BUS(sc->sc_busdev);
		break;
	default:
		error = EINVAL;
	}

	return(error);
}

static void
gpioiic_setsda(device_t dev, int val)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);

	if (val == 0) {
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->sda_pin, 0);
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->sda_pin,
		    GPIO_PIN_OUTPUT);
	} else {
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->sda_pin,
		    GPIO_PIN_INPUT);
	}
}

static void
gpioiic_setscl(device_t dev, int val)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);

	if (val == 0) {
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->scl_pin, 0);
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->scl_pin,
		    GPIO_PIN_OUTPUT);
	} else {
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->scl_pin,
		    GPIO_PIN_INPUT);
	}
}

static int
gpioiic_getscl(device_t dev)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);
	unsigned int			val;

	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->scl_pin,
	    GPIO_PIN_INPUT);
	GPIOBUS_PIN_GET(sc->sc_busdev, sc->sc_dev, sc->scl_pin, &val);

	return ((int)val);
}

static int
gpioiic_getsda(device_t dev)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);
	unsigned int			val;

	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->sda_pin,
	    GPIO_PIN_INPUT);
	GPIOBUS_PIN_GET(sc->sc_busdev, sc->sc_dev, sc->sda_pin, &val);

	return ((int)val);
}

static int
gpioiic_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct gpioiic_softc		*sc = device_get_softc(dev);

	GPIOBUS_LOCK_BUS(sc->sc_busdev);
	GPIOBUS_ACQUIRE_BUS(sc->sc_busdev, sc->sc_dev);

	gpioiic_reset_bus(sc->sc_dev);

	GPIOBUS_RELEASE_BUS(sc->sc_busdev, sc->sc_dev);
	GPIOBUS_UNLOCK_BUS(sc->sc_busdev);

	return (IIC_ENOADDR);
}

static devclass_t gpioiic_devclass;

static device_method_t gpioiic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpioiic_probe),
	DEVMETHOD(device_attach,	gpioiic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* iicbb interface */
	DEVMETHOD(iicbb_callback,	gpioiic_callback),
	DEVMETHOD(iicbb_setsda,		gpioiic_setsda),
	DEVMETHOD(iicbb_setscl,		gpioiic_setscl),
	DEVMETHOD(iicbb_getsda,		gpioiic_getsda),
	DEVMETHOD(iicbb_getscl,		gpioiic_getscl),
	DEVMETHOD(iicbb_reset,		gpioiic_reset),

	{ 0, 0 }
};

static driver_t gpioiic_driver = {
	"gpioiic",
	gpioiic_methods,
	sizeof(struct gpioiic_softc),
};

DRIVER_MODULE(gpioiic, gpiobus, gpioiic_driver, gpioiic_devclass, 0, 0);
DRIVER_MODULE(iicbb, gpioiic, iicbb_driver, iicbb_devclass, 0, 0);
MODULE_DEPEND(gpioiic, iicbb, IICBB_MINVER, IICBB_PREFVER, IICBB_MAXVER);
MODULE_DEPEND(gpioiic, gpiobus, 1, 1, 1);
