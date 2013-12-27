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
 * Gateworks Cambria Octal LED Latch driver.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/armreg.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include <dev/led/led.h>

struct led_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	struct cdev		*sc_leds[8];
	uint8_t			sc_latch;
};

static void
update_latch(struct led_softc *sc, int bit, int onoff)
{
	if (onoff)
		sc->sc_latch &= ~bit;
	else
		sc->sc_latch |= bit;
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, 0, sc->sc_latch);
}
static void led_A(void *arg, int onoff) { update_latch(arg, 1<<0, onoff); }
static void led_B(void *arg, int onoff) { update_latch(arg, 1<<1, onoff); }
static void led_C(void *arg, int onoff) { update_latch(arg, 1<<2, onoff); }
static void led_D(void *arg, int onoff) { update_latch(arg, 1<<3, onoff); }
static void led_E(void *arg, int onoff) { update_latch(arg, 1<<4, onoff); }
static void led_F(void *arg, int onoff) { update_latch(arg, 1<<5, onoff); }
static void led_G(void *arg, int onoff) { update_latch(arg, 1<<6, onoff); }
static void led_H(void *arg, int onoff) { update_latch(arg, 1<<7, onoff); }

static int
led_probe(device_t dev)
{
	device_set_desc(dev, "Gateworks Octal LED Latch");
	return (0);
}

static int
led_attach(device_t dev)
{
	struct led_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	sc->sc_dev = dev;
	sc->sc_iot = sa->sc_iot;
	/* NB: write anywhere works, use first location */
	if (bus_space_map(sc->sc_iot, CAMBRIA_OCTAL_LED_HWBASE, sizeof(uint8_t),
	    0, &sc->sc_ioh)) {
		device_printf(dev, "cannot map LED latch (0x%lx)",
		    CAMBRIA_OCTAL_LED_HWBASE);
		return ENXIO;
	}

	sc->sc_leds[0] = led_create(led_A, sc, "A");
	sc->sc_leds[1] = led_create(led_B, sc, "B");
	sc->sc_leds[2] = led_create(led_C, sc, "C");
	sc->sc_leds[3] = led_create(led_D, sc, "D");
	sc->sc_leds[4] = led_create(led_E, sc, "E");
	sc->sc_leds[5] = led_create(led_F, sc, "F");
	sc->sc_leds[6] = led_create(led_G, sc, "G");
	sc->sc_leds[7] = led_create(led_H, sc, "H");

	return 0;
}

static int
led_detach(device_t dev)
{
	struct led_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < 8; i++) {
		struct cdev *led = sc->sc_leds[i];
		if (led != NULL)
			led_destroy(led);
	}
	return (0);
}

static device_method_t led_methods[] = {
	DEVMETHOD(device_probe,		led_probe),
	DEVMETHOD(device_attach,	led_attach),
	DEVMETHOD(device_detach,	led_detach),

	{0, 0},
};

static driver_t led_driver = {
	"led_cambria",
	led_methods,
	sizeof(struct led_softc),
};
static devclass_t led_devclass;
DRIVER_MODULE(led_cambria, ixp, led_driver, led_devclass, 0, 0);
