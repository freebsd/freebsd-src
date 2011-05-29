/*-
 * Copyright (c) 2008 Sam Leffler.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Cambria Front Panel LED sitting on the I2C bus.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/iicbus/iiconf.h>
#include <dev/led/led.h>

#include "iicbus_if.h"

#define	IIC_M_WR	0	/* write operation */
#define	LED_ADDR	0xae	/* slave address */

struct fled_softc {
	struct cdev	*sc_led;
};

static int
fled_probe(device_t dev)
{
	device_set_desc(dev, "Gateworks Cambria Front Panel LED");
	return 0;
}

static void
fled_cb(void *arg, int onoff)
{
	uint8_t data[1];
	struct iic_msg msgs[1] = {
	     { LED_ADDR, IIC_M_WR, 1, data },
	};
	device_t dev = arg;

	data[0] = (onoff == 0);		/* NB: low true */
	(void) iicbus_transfer(dev, msgs, 1);
}

static int
fled_attach(device_t dev)
{
	struct fled_softc *sc = device_get_softc(dev);

	sc->sc_led = led_create(fled_cb, dev, "front");

	fled_cb(dev, 1);		/* Turn on LED */

	return 0;
}

static int
fled_detach(device_t dev)
{
	struct fled_softc *sc = device_get_softc(dev);

	if (sc->sc_led != NULL)
		led_destroy(sc->sc_led);

	return 0;
}

static device_method_t fled_methods[] = {
	DEVMETHOD(device_probe,		fled_probe),
	DEVMETHOD(device_attach,	fled_attach),
	DEVMETHOD(device_detach,	fled_detach),

	{0, 0},
};

static driver_t fled_driver = {
	"fled",
	fled_methods,
	sizeof(struct fled_softc),
};
static devclass_t fled_devclass;

DRIVER_MODULE(fled, iicbus, fled_driver, fled_devclass, 0, 0);
MODULE_VERSION(fled, 1);
MODULE_DEPEND(fled, iicbus, 1, 1, 1);
