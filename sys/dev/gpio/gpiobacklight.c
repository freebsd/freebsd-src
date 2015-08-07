/*-
 * Copyright (c) 2015 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
#include <sys/sysctl.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>

#include <dev/gpio/gpiobusvar.h>

#include "gpiobus_if.h"

/*
 * Only one pin for led
 */
#define	GPIOBL_PIN	0

#define GPIOBL_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIOBL_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define GPIOBL_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "gpiobacklight", MTX_DEF)
#define GPIOBL_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);

struct gpiobacklight_softc 
{
	device_t	sc_dev;
	device_t	sc_busdev;
	struct mtx	sc_mtx;

	struct sysctl_oid	*sc_oid;
	int		sc_brightness;
};

static int gpiobacklight_sysctl(SYSCTL_HANDLER_ARGS);
static void gpiobacklight_update_brightness(struct gpiobacklight_softc *);
static int gpiobacklight_probe(device_t);
static int gpiobacklight_attach(device_t);
static int gpiobacklight_detach(device_t);

static void 
gpiobacklight_update_brightness(struct gpiobacklight_softc *sc)
{
	int error;

	GPIOBL_LOCK(sc);
	error = GPIOBUS_ACQUIRE_BUS(sc->sc_busdev, sc->sc_dev,
	    GPIOBUS_DONTWAIT);
	if (error != 0) {
		GPIOBL_UNLOCK(sc);
		return;
	}
	error = GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev,
	    GPIOBL_PIN, GPIO_PIN_OUTPUT);
	if (error == 0)
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, GPIOBL_PIN,
		    sc->sc_brightness ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	GPIOBUS_RELEASE_BUS(sc->sc_busdev, sc->sc_dev);
	GPIOBL_UNLOCK(sc);
}

static int
gpiobacklight_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct gpiobacklight_softc *sc;
	int error;
	int brightness;

	sc = (struct gpiobacklight_softc*)arg1;
	brightness = sc->sc_brightness;
	error = sysctl_handle_int(oidp, &brightness, 0, req);

	if (error != 0 || req->newptr == NULL)
		return (error);

	if (brightness <= 0)
		sc->sc_brightness = 0;
	else
		sc->sc_brightness = 1;

	gpiobacklight_update_brightness(sc);

	return (error);
}

static void
gpiobacklight_identify(driver_t *driver, device_t bus)
{
	phandle_t node, root;

	root = OF_finddevice("/");
	if (root == 0)
		return;
	for (node = OF_child(root); node != 0; node = OF_peer(node)) {
		if (!fdt_is_compatible_strict(node, "gpio-backlight"))
			continue;
		ofw_gpiobus_add_fdt_child(bus, driver->name, node);
	}
}

static int
gpiobacklight_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "gpio-backlight"))
		return (ENXIO);

	device_set_desc(dev, "GPIO backlight");

	return (0);
}

static int
gpiobacklight_attach(device_t dev)
{
	struct gpiobacklight_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	phandle_t node;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);

	if ((node = ofw_bus_get_node(dev)) == -1)
		return (ENXIO);

	GPIOBL_LOCK_INIT(sc);
	if (OF_hasprop(node, "default-on"))
		sc->sc_brightness = 1;
	else
		sc->sc_brightness = 0;

	/* Init backlight interface */
	ctx = device_get_sysctl_ctx(sc->sc_dev);
	tree = device_get_sysctl_tree(sc->sc_dev);
	sc->sc_oid = SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "brightness", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    gpiobacklight_sysctl, "I", "backlight brightness");

	gpiobacklight_update_brightness(sc);

	return (0);
}

static int
gpiobacklight_detach(device_t dev)
{
	struct gpiobacklight_softc *sc;

	sc = device_get_softc(dev);
	GPIOBL_LOCK_DESTROY(sc);
	return (0);
}

static devclass_t gpiobacklight_devclass;

static device_method_t gpiobacklight_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	gpiobacklight_identify),
	DEVMETHOD(device_probe,		gpiobacklight_probe),
	DEVMETHOD(device_attach,	gpiobacklight_attach),
	DEVMETHOD(device_detach,	gpiobacklight_detach),

	DEVMETHOD_END
};

static driver_t gpiobacklight_driver = {
	"gpiobacklight",
	gpiobacklight_methods,
	sizeof(struct gpiobacklight_softc),
};

DRIVER_MODULE(gpiobacklight, gpiobus, gpiobacklight_driver, gpiobacklight_devclass, 0, 0);
