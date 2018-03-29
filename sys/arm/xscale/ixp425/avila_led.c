/*-
 * Copyright (c) 2006 Kevin Lo.  All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include <dev/led/led.h>

#define	GPIO_LED_STATUS	3
#define	GPIO_LED_STATUS_BIT	(1U << GPIO_LED_STATUS)

struct led_avila_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_gpio_ioh;
	struct cdev		*sc_led;
};

static void
led_func(void *arg, int onoff)
{
	struct led_avila_softc *sc = arg;
	uint32_t reg;

	IXP4XX_GPIO_LOCK();
	reg = GPIO_CONF_READ_4(sc, IXP425_GPIO_GPOUTR);
	if (onoff)
		reg &= ~GPIO_LED_STATUS_BIT;
	else
		reg |= GPIO_LED_STATUS_BIT;
	GPIO_CONF_WRITE_4(sc, IXP425_GPIO_GPOUTR, reg);
	IXP4XX_GPIO_UNLOCK();
}

static int
led_avila_probe(device_t dev)
{
	device_set_desc(dev, "Gateworks Avila Front Panel LED");
	return (0);
}

static int
led_avila_attach(device_t dev)
{
	struct led_avila_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	sc->sc_gpio_ioh = sa->sc_gpio_ioh;

	/* Configure LED GPIO pin as output */
	GPIO_CONF_WRITE_4(sc, IXP425_GPIO_GPOER,
	    GPIO_CONF_READ_4(sc, IXP425_GPIO_GPOER) &~ GPIO_LED_STATUS_BIT);

	sc->sc_led = led_create(led_func, sc, "gpioled");

	led_func(sc, 1);		/* Turn on LED */

	return (0);
}

static int
led_avila_detach(device_t dev)
{
	struct led_avila_softc *sc = device_get_softc(dev);

	if (sc->sc_led != NULL)
		led_destroy(sc->sc_led);
	return (0);
}

static device_method_t led_avila_methods[] = {
	DEVMETHOD(device_probe,		led_avila_probe),
	DEVMETHOD(device_attach,	led_avila_attach),
	DEVMETHOD(device_detach,	led_avila_detach),

	{0, 0},
};

static driver_t led_avila_driver = {
	"led_avila",
	led_avila_methods,
	sizeof(struct led_avila_softc),
};
static devclass_t led_avila_devclass;

DRIVER_MODULE(led_avila, ixp, led_avila_driver, led_avila_devclass, 0, 0);
