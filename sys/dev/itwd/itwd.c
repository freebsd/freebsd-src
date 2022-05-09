/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andriy Gapon
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
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>
#include <sys/watchdog.h>

#include <dev/superio/superio.h>

#include <machine/bus.h>
#include <machine/resource.h>


struct itwd_softc {
	eventhandler_tag	wd_ev;
	void			*intr_handle;
	struct resource		*intr_res;
	int			intr_rid;
};

static void
wd_func(void *priv, u_int cmd, int *error)
{
	device_t dev = priv;
	uint64_t timeout;
	uint8_t val;


	if (cmd != 0) {
		cmd &= WD_INTERVAL;

		/*
		 * Convert the requested timeout to seconds.
		 * If the timeout is smaller than the minimal supported value
		 * then set it to the minimum.
		 * TODO This hardware actually supports 64ms resolution
		 * when bit 5 of 0x72 is set.  Switch to that resolution when
		 * needed.
		 */
		if (cmd >= WD_TO_1SEC)
			timeout = (uint64_t)1 << (cmd - WD_TO_1SEC);
		else
			timeout = 1;

		/* TODO If timeout is greater than maximum value
		 * that can be specified in seconds, we should
		 * switch the timer to minutes mode by clearing
		 * bit 7 of 0x72 (ensure that bit 5 is also cleared).
		 *
		 * For now, just disable the timer to honor the
		 * watchdog(9) protocol.
		 *
		 * XXX The timeout actually can be up to 65535 units
		 * as it is set via two registers 0x73, LSB, and 0x74,
		 * MSB.  But it is not clear what the protocol for writing
		 * those two registers is.
		 */
		if (timeout <= UINT8_MAX) {
			val = timeout;
			*error = 0;
		} else {
			/* error left unchanged */
			val = 0;
		}
	} else {
		val = 0;
	}
#ifdef DIAGNOSTIC
	if (bootverbose)
		device_printf(dev, "setting timeout to %d\n", val);
#endif
	superio_write(dev, 0x73, val);
	if (superio_read(dev, 0x73) != val)
		superio_write(dev, 0x73, val);
}

static void
itwd_intr(void *cookie)
{
	device_t dev = cookie;
	uint8_t val;

	val = superio_read(dev, 0x71);
	if (bootverbose)
		device_printf(dev, "got interrupt, wdt status = %d\n", val & 1);
	superio_write(dev, 0x71, val & ~((uint8_t)0x01));
}

static int
itwd_probe(device_t dev)
{

	if (superio_vendor(dev) != SUPERIO_VENDOR_ITE)
		return (ENXIO);
	if (superio_get_type(dev) != SUPERIO_DEV_WDT)
		return (ENXIO);
	device_set_desc(dev, "Watchdog Timer on ITE SuperIO");
	return (BUS_PROBE_DEFAULT);
}

static int
itwd_attach(device_t dev)
{
	struct itwd_softc *sc = device_get_softc(dev);
	int irq = 0;
	int nmi = 0;
	int error;

	/* First, reset the timeout, just in case. */
	superio_write(dev, 0x74, 0);
	superio_write(dev, 0x73, 0);

	TUNABLE_INT_FETCH("dev.itwd.irq", &irq);
	TUNABLE_INT_FETCH("dev.itwd.nmi", &nmi);
	if (irq < 0 || irq > 15) {
		device_printf(dev, "Ignoring invalid IRQ value %d\n", irq);
		irq = 0;
	}
	if (irq == 0 && nmi) {
		device_printf(dev, "Ignoring NMI mode if IRQ is not set\n");
		nmi = 0;
	}

	/*
	 * NB: if the interrupt has been configured for the NMI delivery,
	 * then it is not available for the regular interrupt allocation.
	 * Thus, we configure the hardware to generate the interrupt,
	 * but do not attempt to allocate and setup it as a regular
	 * interrupt.
	 */
	if (irq != 0 && !nmi) {
		sc->intr_rid = 0;
		bus_set_resource(dev, SYS_RES_IRQ, sc->intr_rid, irq, 1);

		sc->intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->intr_rid, RF_ACTIVE);
		if (sc->intr_res == NULL) {
			device_printf(dev, "unable to map interrupt\n");
			return (ENXIO);
		}
		error = bus_setup_intr(dev, sc->intr_res,
		    INTR_TYPE_MISC | INTR_MPSAFE, NULL, itwd_intr, dev,
		    &sc->intr_handle);
		if (error != 0) {
			bus_release_resource(dev, SYS_RES_IRQ,
			    sc->intr_rid, sc->intr_res);
			device_printf(dev, "Unable to setup irq: error %d\n",
			    error);
			return (ENXIO);
		}
	}
	if (irq != 0) {
		device_printf(dev, "Using IRQ%d to signal timeout\n", irq);
	} else {
		/* System reset via KBRST. */
		irq = 0x40;
		device_printf(dev, "Configured for system reset on timeout\n");
	}

	superio_write(dev, 0x71, 0);
	superio_write(dev, 0x72, 0x80 | (uint8_t)irq);

	sc->wd_ev = EVENTHANDLER_REGISTER(watchdog_list, wd_func, dev, 0);
	return (0);
}

static int
itwd_detach(device_t dev)
{
	struct itwd_softc *sc = device_get_softc(dev);
	int dummy;

	if (sc->wd_ev != NULL)
		EVENTHANDLER_DEREGISTER(watchdog_list, sc->wd_ev);
	wd_func(dev, 0, &dummy);
	if (sc->intr_handle)
		bus_teardown_intr(dev, sc->intr_res, sc->intr_handle);
	if (sc->intr_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->intr_rid,
		    sc->intr_res);
	return (0);
}

static device_method_t itwd_methods[] = {
	/* Methods from the device interface */
	DEVMETHOD(device_probe,		itwd_probe),
	DEVMETHOD(device_attach,	itwd_attach),
	DEVMETHOD(device_detach,	itwd_detach),

	/* Terminate method list */
	{ 0, 0 }
};

static driver_t itwd_driver = {
	"itwd",
	itwd_methods,
	sizeof (struct itwd_softc)
};

DRIVER_MODULE(itwd, superio, itwd_driver, NULL, NULL);
MODULE_DEPEND(itwd, superio, 1, 1, 1);
MODULE_VERSION(itwd, 1);
