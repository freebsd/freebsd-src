/*-
 * Copyright (c) 2015 M. Warner Losh <imp@FreeBSD.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ow/owll.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static struct ofw_compat_data compat_data[] = {
	{"w1-gpio",  true},
	{NULL,       false}
};
OFWBUS_PNP_INFO(compat_data);
SIMPLEBUS_PNP_INFO(compat_data);
#endif /* FDT */

#define	OW_PIN		0

#define OWC_GPIOBUS_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	OWC_GPIOBUS_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define OWC_GPIOBUS_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "owc_gpiobus", MTX_DEF)
#define OWC_GPIOBUS_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

struct owc_gpiobus_softc 
{
	device_t	sc_dev;
	gpio_pin_t	sc_pin;
	struct mtx	sc_mtx;
};

static int owc_gpiobus_probe(device_t);
static int owc_gpiobus_attach(device_t);
static int owc_gpiobus_detach(device_t);

static int
owc_gpiobus_probe(device_t dev)
{
	int rv;

	/*
	 * By default we only bid to attach if specifically added by our parent
	 * (usually via hint.owc_gpiobus.#.at=busname).  On FDT systems we bid
	 * as the default driver based on being configured in the FDT data.
	 */
	rv = BUS_PROBE_NOWILDCARD;

#ifdef FDT
	if (ofw_bus_status_okay(dev) &&
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		rv = BUS_PROBE_DEFAULT;
#endif

	device_set_desc(dev, "GPIO one-wire bus");

	return (rv);
}

static int
owc_gpiobus_attach(device_t dev)
{
	struct owc_gpiobus_softc *sc;
	int err;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

#ifdef FDT
	/* Try to configure our pin from fdt data on fdt-based systems. */
	err = gpio_pin_get_by_ofw_idx(dev, ofw_bus_get_node(dev), OW_PIN,
	    &sc->sc_pin);
#else
	err = ENOENT;
#endif
	/*
	 * If we didn't get configured by fdt data and our parent is gpiobus,
	 * see if we can be configured by the bus (allows hinted attachment even
	 * on fdt-based systems).
	 */
	if (err != 0 &&
	    strcmp("gpiobus", device_get_name(device_get_parent(dev))) == 0)
		err = gpio_pin_get_by_child_index(dev, OW_PIN, &sc->sc_pin);

	/* If we didn't get configured by either method, whine and punt. */
	if (err != 0) {
		device_printf(sc->sc_dev,
		    "cannot acquire gpio pin (config error)\n");
		return (err);
	}

	OWC_GPIOBUS_LOCK_INIT(sc);

	/*
	 * Add the ow bus as a child, but defer probing and attaching it until
	 * interrupts work, because we can't do IO for them until we can read
	 * the system timecounter (which initializes after device attachments).
	 */
	device_add_child(sc->sc_dev, "ow", -1);
	return (bus_delayed_attach_children(dev));
}

static int
owc_gpiobus_detach(device_t dev)
{
	struct owc_gpiobus_softc *sc;
	int err;

	sc = device_get_softc(dev);

	if ((err = device_delete_children(dev)) != 0)
		return (err);

	gpio_pin_release(sc->sc_pin);
	OWC_GPIOBUS_LOCK_DESTROY(sc);

	return (0);
}

/*
 * In the diagrams below, R is driven by the resistor pullup, M is driven by the
 * master, and S is driven by the slave / target.
 */

/*
 * These macros let what why we're doing stuff shine in the code
 * below, and let the how be confined to here.
 */
#define OUTPIN(sc)	gpio_pin_setflags((sc)->sc_pin, GPIO_PIN_OUTPUT)
#define INPIN(sc)	gpio_pin_setflags((sc)->sc_pin, GPIO_PIN_INPUT)
#define GETPIN(sc, bp)	gpio_pin_is_active((sc)->sc_pin, (bp))
#define LOW(sc)		gpio_pin_set_active((sc)->sc_pin, false)

/*
 * WRITE-ONE (see owll_if.m for timings) From Figure 4-1 AN-937
 *
 *		       |<---------tSLOT---->|<-tREC->|
 *	High	RRRRM  | 	RRRRRRRRRRRR|RRRRRRRRM
 *		     M |       R |     |  |	      M
 *		      M|      R	 |     |  |	       M
 *	Low	       MMMMMMM	 |     |  |    	        MMMMMM...
 *		       |<-tLOW1->|     |  |
 *		       |<------15us--->|  |
 *                     |<--------60us---->|
 */
static int
owc_gpiobus_write_one(device_t dev, struct ow_timing *t)
{
	struct owc_gpiobus_softc *sc;

	sc = device_get_softc(dev);

	critical_enter();

	/* Force low */
	OUTPIN(sc);
	LOW(sc);
	DELAY(t->t_low1);

	/* Allow resistor to float line high */
	INPIN(sc);
	DELAY(t->t_slot - t->t_low1 + t->t_rec);

	critical_exit();

	return (0);
}

/*
 * WRITE-ZERO (see owll_if.m for timings) From Figure 4-2 AN-937
 *
 *		       |<---------tSLOT------>|<-tREC->|
 *	High	RRRRM  | 	            | |RRRRRRRM
 *		     M |                    | R	       M
 *		      M|       	 |     |    |R 	        M
 *	Low	       MMMMMMMMMMMMMMMMMMMMMR  	         MMMMMM...
 *     	       	       |<--15us->|     |    |
 *     	       	       |<------60us--->|    |
 *                     |<-------tLOW0------>|
 */
static int
owc_gpiobus_write_zero(device_t dev, struct ow_timing *t)
{
	struct owc_gpiobus_softc *sc;

	sc = device_get_softc(dev);

	critical_enter();

	/* Force low */
	OUTPIN(sc);
	LOW(sc);
	DELAY(t->t_low0);

	/* Allow resistor to float line high */
	INPIN(sc);
	DELAY(t->t_slot - t->t_low0 + t->t_rec);

	critical_exit();

	return (0);
}

/*
 * READ-DATA (see owll_if.m for timings) From Figure 4-3 AN-937
 *
 *		       |<---------tSLOT------>|<-tREC->|
 *	High	RRRRM  |        rrrrrrrrrrrrrrrRRRRRRRM
 *		     M |       r            | R	       M
 *		      M|      r	        |   |R 	        M
 *	Low	       MMMMMMMSSSSSSSSSSSSSSR  	         MMMMMM...
 *     	       	       |<tLOWR>< sample	>   |
 *     	       	       |<------tRDV---->|   |
 *                                    ->|   |<-tRELEASE
 *
 * r -- allowed to pull high via the resitor when slave writes a 1-bit
 *
 */
static int
owc_gpiobus_read_data(device_t dev, struct ow_timing *t, int *bit)
{
	struct owc_gpiobus_softc *sc;
	bool sample;
	sbintime_t then, now;

	sc = device_get_softc(dev);

	critical_enter();

	/* Force low for t_lowr microseconds */
	then = sbinuptime();
	OUTPIN(sc);
	LOW(sc);
	DELAY(t->t_lowr);

	/*
	 * Slave is supposed to hold the line low for t_rdv microseconds for 0
	 * and immediately float it high for a 1. This is measured from the
	 * master's pushing the line low.
	 */
	INPIN(sc);
	do {
		now = sbinuptime();
		GETPIN(sc, &sample);
	} while (now - then < (t->t_rdv + 2) * SBT_1US && sample == false);
	critical_exit();

	if (now - then < t->t_rdv * SBT_1US)
		*bit = 1;
	else
		*bit = 0;

	/* Wait out the rest of t_slot */
	do {
		now = sbinuptime();
	} while (now - then < (t->t_slot + t->t_rec) * SBT_1US);

	return (0);
}

/*
 * RESET AND PRESENCE PULSE (see owll_if.m for timings) From Figure 4-4 AN-937
 *
 *				    |<---------tRSTH------------>|
 *	High RRRM  |		  | RRRRRRRS	       |  RRRR RRM
 *		 M |		  |R|  	   |S  	       | R	  M
 *		  M|		  R |	   | S	       |R	   M
 *	Low	   MMMMMMMM MMMMMM| |	   |  SSSSSSSSSS	    MMMMMM
 *     	       	   |<----tRSTL--->| |  	   |<-tPDL---->|
 *		   |   	       	->| |<-tR  |	       |
 *				    |<tPDH>|
 *
 * Note: for Regular Speed operations, tRSTL + tR should be less than 960us to
 * avoid interfering with other devices on the bus.
 *
 * Return values in *bit:
 *  -1 = Bus wiring error (stuck low).
 *   0 = no presence pulse
 *   1 = presence pulse detected
 */
static int
owc_gpiobus_reset_and_presence(device_t dev, struct ow_timing *t, int *bit)
{
	struct owc_gpiobus_softc *sc;
	bool sample;

	sc = device_get_softc(dev);

	/*
	 * Read the current state of the bus. The steady state of an idle bus is
	 * high. Badly wired buses that are missing the required pull up, or
	 * that have a short circuit to ground cause all kinds of mischief when
	 * we try to read them later. Return EIO if the bus is currently low.
	 */
	INPIN(sc);
	GETPIN(sc, &sample);
	if (sample == false) {
		*bit = -1;
		return (EIO);
	}

	critical_enter();

	/* Force low */
	OUTPIN(sc);
	LOW(sc);
	DELAY(t->t_rstl);

	/* Allow resistor to float line high and then wait for reset pulse */
	INPIN(sc);
	DELAY(t->t_pdh + t->t_pdl / 2);

	/* Read presence pulse  */
	GETPIN(sc, &sample);
	*bit = sample;

	critical_exit();

	DELAY(t->t_rsth - (t->t_pdh + t->t_pdl / 2));	/* Timing not critical for this one */

	/*
	 * Read the state of the bus after we've waited past the end of the rest
	 * window. It should return to high. If it is low, then we have some
	 * problem and should abort the reset.
	 */
	GETPIN(sc, &sample);
	if (sample == false) {
		*bit = -1;
		return (EIO);
	}

	return (0);
}

static device_method_t owc_gpiobus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		owc_gpiobus_probe),
	DEVMETHOD(device_attach,	owc_gpiobus_attach),
	DEVMETHOD(device_detach,	owc_gpiobus_detach),

	DEVMETHOD(owll_write_one,	owc_gpiobus_write_one),
	DEVMETHOD(owll_write_zero,	owc_gpiobus_write_zero),
	DEVMETHOD(owll_read_data,	owc_gpiobus_read_data),
	DEVMETHOD(owll_reset_and_presence,	owc_gpiobus_reset_and_presence),
	{ 0, 0 }
};

static driver_t owc_gpiobus_driver = {
	"owc",
	owc_gpiobus_methods,
	sizeof(struct owc_gpiobus_softc),
};

#ifdef FDT
DRIVER_MODULE(owc_gpiobus, simplebus, owc_gpiobus_driver, 0, 0);
#endif

DRIVER_MODULE(owc_gpiobus, gpiobus, owc_gpiobus_driver, 0, 0);
MODULE_DEPEND(owc_gpiobus, ow, 1, 1, 1);
MODULE_DEPEND(owc_gpiobus, gpiobus, 1, 1, 1);
MODULE_VERSION(owc_gpiobus, 1);
