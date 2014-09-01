/*-
 * Copyright (c) 2009, Nathan Whitehorn <nwhitehorn@FreeBSD.org>
 * Copyright (c) 2013, Luiz Otavio O Souza <loos@FreeBSD.org>
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>

static int ofw_gpiobus_parse_gpios(struct gpiobus_softc *,
    struct gpiobus_ivar *, phandle_t);
static struct ofw_gpiobus_devinfo *ofw_gpiobus_setup_devinfo(device_t,
    phandle_t);
static void ofw_gpiobus_destroy_devinfo(struct ofw_gpiobus_devinfo *);

device_t
ofw_gpiobus_add_fdt_child(device_t bus, phandle_t child)
{
	struct ofw_gpiobus_devinfo *dinfo;
	device_t childdev;

	/*
	 * Set up the GPIO child and OFW bus layer devinfo and add it to bus.
	 */
	dinfo = ofw_gpiobus_setup_devinfo(bus, child);
	if (dinfo == NULL)
		return (NULL);
	childdev = device_add_child(bus, NULL, -1);
	if (childdev == NULL) {
		device_printf(bus, "could not add child: %s\n",
		    dinfo->opd_obdinfo.obd_name);
		ofw_gpiobus_destroy_devinfo(dinfo);
		return (NULL);
	}
	device_set_ivars(childdev, dinfo);

	return (childdev);
}

static int
ofw_gpiobus_alloc_ivars(struct gpiobus_ivar *dinfo)
{

	/* Allocate pins and flags memory. */
	dinfo->pins = malloc(sizeof(uint32_t) * dinfo->npins, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (dinfo->pins == NULL)
		return (ENOMEM);
	dinfo->flags = malloc(sizeof(uint32_t) * dinfo->npins, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (dinfo->flags == NULL) {
		free(dinfo->pins, M_DEVBUF);
		return (ENOMEM);
	}

	return (0);
}

static void
ofw_gpiobus_free_ivars(struct gpiobus_ivar *dinfo)
{

	free(dinfo->flags, M_DEVBUF);
	free(dinfo->pins, M_DEVBUF);
}

static int
ofw_gpiobus_parse_gpios(struct gpiobus_softc *sc, struct gpiobus_ivar *dinfo,
	phandle_t child)
{
	int cells, i, j, len;
	pcell_t *gpios;
	phandle_t gpio;

	/* Retrieve the gpios property. */
	if ((len = OF_getproplen(child, "gpios")) < 0)
		return (EINVAL);
	gpios = malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (gpios == NULL)
		return (ENOMEM);
	if (OF_getencprop(child, "gpios", gpios, len) < 0) {
		free(gpios, M_DEVBUF);
		return (EINVAL);
	}

	/*
	 * The gpio-specifier is controller independent, but the first pcell
	 * has the reference to the GPIO controller phandler.
	 * One the first pass we count the number of encoded gpio-specifiers.
	 */
	i = 0;
	len /= sizeof(pcell_t);
	while (i < len) {
		/* Allow NULL specifiers. */
		if (gpios[i] == 0) {
			dinfo->npins++;
			i++;
			continue;
		}
		gpio = OF_node_from_xref(gpios[i]);
		/* Verify if we're attaching to the correct GPIO controller. */
		if (!OF_hasprop(gpio, "gpio-controller") ||
		    gpio != ofw_bus_get_node(sc->sc_dev)) {
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}
		/* Read gpio-cells property for this GPIO controller. */
		if (OF_getencprop(gpio, "#gpio-cells", &cells,
		    sizeof(cells)) < 0) {
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}
		dinfo->npins++;
		i += cells + 1;
	}

	if (dinfo->npins == 0) {
		free(gpios, M_DEVBUF);
		return (EINVAL);
	}

	/* Allocate the child resources. */
	if (ofw_gpiobus_alloc_ivars(dinfo) != 0) {
		free(gpios, M_DEVBUF);
		return (ENOMEM);
	}

	/* Decode the gpio specifier on the second pass. */
	i = 0;
	j = 0;
	while (i < len) {
		/* Allow NULL specifiers. */
		if (gpios[i] == 0) {
			i++;
			j++;
			continue;
		}

		gpio = OF_node_from_xref(gpios[i]);
		/* Read gpio-cells property for this GPIO controller. */
		if (OF_getencprop(gpio, "#gpio-cells", &cells,
		    sizeof(cells)) < 0) {
			ofw_gpiobus_free_ivars(dinfo);
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}

		/* Get the GPIO pin number and flags. */
		if (gpio_map_gpios(sc->sc_dev, child, gpio, cells,
		    &gpios[i + 1], &dinfo->pins[j], &dinfo->flags[j]) != 0) {
			ofw_gpiobus_free_ivars(dinfo);
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}

		/* Consistency check. */
		if (dinfo->pins[j] > sc->sc_npins) {
			device_printf(sc->sc_busdev,
			    "invalid pin %d, max: %d\n",
			    dinfo->pins[j], sc->sc_npins - 1);
			ofw_gpiobus_free_ivars(dinfo);
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}

		/*
		 * Mark pin as mapped and give warning if it's already mapped.
		 */
		if (sc->sc_pins_mapped[dinfo->pins[j]]) {
			device_printf(sc->sc_busdev,
			    "warning: pin %d is already mapped\n",
			    dinfo->pins[j]);
			ofw_gpiobus_free_ivars(dinfo);
			free(gpios, M_DEVBUF);
			return (EINVAL);
		}
		sc->sc_pins_mapped[dinfo->pins[j]] = 1;

		i += cells + 1;
		j++;
	}

	free(gpios, M_DEVBUF);

	return (0);
}

static struct ofw_gpiobus_devinfo *
ofw_gpiobus_setup_devinfo(device_t dev, phandle_t node)
{
	struct gpiobus_softc *sc;
	struct ofw_gpiobus_devinfo *dinfo;

	sc = device_get_softc(dev);
	dinfo = malloc(sizeof(*dinfo), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (dinfo == NULL)
		return (NULL);
	if (ofw_bus_gen_setup_devinfo(&dinfo->opd_obdinfo, node) != 0) {
		free(dinfo, M_DEVBUF);
		return (NULL);
	}

	/* Parse the gpios property for the child. */
	if (ofw_gpiobus_parse_gpios(sc, &dinfo->opd_dinfo, node) != 0) {
		ofw_bus_gen_destroy_devinfo(&dinfo->opd_obdinfo);
		free(dinfo, M_DEVBUF);
		return (NULL);
	}

	return (dinfo);
}

static void
ofw_gpiobus_destroy_devinfo(struct ofw_gpiobus_devinfo *dinfo)
{

	ofw_bus_gen_destroy_devinfo(&dinfo->opd_obdinfo);
	free(dinfo, M_DEVBUF);
}

static int
ofw_gpiobus_probe(device_t dev)
{

	if (ofw_bus_get_node(dev) == -1)
		return (ENXIO);
	device_set_desc(dev, "OFW GPIO bus");

	return (0);
}

static int
ofw_gpiobus_attach(device_t dev)
{
	struct gpiobus_softc *sc;
	phandle_t child;

	sc = GPIOBUS_SOFTC(dev);
	sc->sc_busdev = dev;
	sc->sc_dev = device_get_parent(dev);

	/* Read the pin max. value */
	if (GPIO_PIN_MAX(sc->sc_dev, &sc->sc_npins) != 0)
		return (ENXIO);

	KASSERT(sc->sc_npins != 0, ("GPIO device with no pins"));

	/*
	 * Increase to get number of pins.
	 */
	sc->sc_npins++;

	sc->sc_pins_mapped = malloc(sizeof(int) * sc->sc_npins, M_DEVBUF, 
	    M_NOWAIT | M_ZERO);

	if (!sc->sc_pins_mapped)
		return (ENOMEM);

	/* Init the bus lock. */
	GPIOBUS_LOCK_INIT(sc);

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);

	/*
	 * Attach the children represented in the device tree.
	 */
	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child))
		if (ofw_gpiobus_add_fdt_child(dev, child) == NULL)
			continue;

	return (bus_generic_attach(dev));
}

static device_t
ofw_gpiobus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct ofw_gpiobus_devinfo *devi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (child);
	devi = malloc(sizeof(struct ofw_gpiobus_devinfo), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (devi == NULL) {
		device_delete_child(dev, child);
		return (0);
	}

	/*
	 * NULL all the OFW-related parts of the ivars for non-OFW
	 * children.
	 */
	devi->opd_obdinfo.obd_node = -1;
	devi->opd_obdinfo.obd_name = NULL;
	devi->opd_obdinfo.obd_compat = NULL;
	devi->opd_obdinfo.obd_type = NULL;
	devi->opd_obdinfo.obd_model = NULL;

	device_set_ivars(child, devi);

	return (child);
}

static int
ofw_gpiobus_print_child(device_t dev, device_t child)
{
	struct ofw_gpiobus_devinfo *devi;
	int retval = 0;

	devi = device_get_ivars(child);
	retval += bus_print_child_header(dev, child);
	retval += printf(" at pin(s) ");
	gpiobus_print_pins(&devi->opd_dinfo);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static const struct ofw_bus_devinfo *
ofw_gpiobus_get_devinfo(device_t bus, device_t dev)
{
	struct ofw_gpiobus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);

	return (&dinfo->opd_obdinfo);
}

static device_method_t ofw_gpiobus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_gpiobus_probe),
	DEVMETHOD(device_attach,	ofw_gpiobus_attach),

	/* Bus interface */
	DEVMETHOD(bus_child_pnpinfo_str,	ofw_bus_gen_child_pnpinfo_str),
	DEVMETHOD(bus_print_child,	ofw_gpiobus_print_child),
	DEVMETHOD(bus_add_child,	ofw_gpiobus_add_child),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofw_gpiobus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static devclass_t ofwgpiobus_devclass;

DEFINE_CLASS_1(gpiobus, ofw_gpiobus_driver, ofw_gpiobus_methods,
    sizeof(struct gpiobus_softc), gpiobus_driver);
DRIVER_MODULE(ofw_gpiobus, gpio, ofw_gpiobus_driver, ofwgpiobus_devclass, 0, 0);
MODULE_VERSION(ofw_gpiobus, 1);
MODULE_DEPEND(ofw_gpiobus, gpiobus, 1, 1, 1);
