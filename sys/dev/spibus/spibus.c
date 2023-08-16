/*-
 * Copyright (c) 2006 M. Warner Losh <imp@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/spibus/spibusvar.h>
#include <dev/spibus/spi.h>
#include "spibus_if.h"

static int
spibus_probe(device_t dev)
{

	device_set_desc(dev, "SPI bus");
	return (BUS_PROBE_DEFAULT);
}

int
spibus_attach(device_t dev)
{
	struct spibus_softc *sc = SPIBUS_SOFTC(dev);

	sc->dev = dev;
	bus_enumerate_hinted_children(dev);
	return (bus_generic_attach(dev));
}

/*
 * Since this is not a self-enumerating bus, and since we always add
 * children in attach, we have to always delete children here.
 */
int
spibus_detach(device_t dev)
{
	return (device_delete_children(dev));
}

static int
spibus_suspend(device_t dev)
{
	return (bus_generic_suspend(dev));
}

static
int
spibus_resume(device_t dev)
{
	return (bus_generic_resume(dev));
}

static int
spibus_print_child(device_t dev, device_t child)
{
	struct spibus_ivar *devi = SPIBUS_IVAR(child);
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at cs %d", devi->cs);
	retval += printf(" mode %d", devi->mode);
	retval += resource_list_print_type(&devi->rl, "irq",
	    SYS_RES_IRQ, "%jd");
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

void
spibus_probe_nomatch(device_t bus, device_t child)
{
	struct spibus_ivar *devi = SPIBUS_IVAR(child);

	device_printf(bus, "<unknown card> at cs %d mode %d\n", devi->cs,
	    devi->mode);
	return;
}

int
spibus_child_location(device_t bus, device_t child, struct sbuf *sb)
{
	struct spibus_ivar *devi = SPIBUS_IVAR(child);
	int cs;

	cs = devi->cs & ~SPIBUS_CS_HIGH; /* trim 'cs high' bit */
	sbuf_printf(sb, "bus=%d cs=%d", device_get_unit(bus), cs);
	return (0);
}

int
spibus_read_ivar(device_t bus, device_t child, int which, uintptr_t *result)
{
	struct spibus_ivar *devi = SPIBUS_IVAR(child);

	switch (which) {
	default:
		return (EINVAL);
	case SPIBUS_IVAR_CS:
		*(uint32_t *)result = devi->cs;
		break;
	case SPIBUS_IVAR_MODE:
		*(uint32_t *)result = devi->mode;
		break;
	case SPIBUS_IVAR_CLOCK:
		*(uint32_t *)result = devi->clock;
		break;
	case SPIBUS_IVAR_CS_DELAY:
		*(uint32_t *)result = devi->cs_delay;
		break;
	}
	return (0);
}

int
spibus_write_ivar(device_t bus, device_t child, int which, uintptr_t value)
{
	struct spibus_ivar *devi = SPIBUS_IVAR(child);

	if (devi == NULL || device_get_parent(child) != bus)
		return (EDOOFUS);

	switch (which) {
	case SPIBUS_IVAR_CLOCK:
		/* Any non-zero value is allowed for max clock frequency. */
		if (value == 0)
			return (EINVAL);
		devi->clock = (uint32_t)value;
		break;
	case SPIBUS_IVAR_CS:
		 /* Chip select cannot be changed. */
		return (EINVAL);
	case SPIBUS_IVAR_MODE:
		/* Valid SPI modes are 0-3. */
		if (value > 3)
			return (EINVAL);
		devi->mode = (uint32_t)value;
		break;
	case SPIBUS_IVAR_CS_DELAY:
		devi->cs_delay = (uint32_t)value;
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

device_t
spibus_add_child_common(device_t dev, u_int order, const char *name, int unit,
    size_t ivars_size)
{
	device_t child;
	struct spibus_ivar *devi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL) 
		return (child);
	devi = malloc(ivars_size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (devi == NULL) {
		device_delete_child(dev, child);
		return (0);
	}
	resource_list_init(&devi->rl);
	device_set_ivars(child, devi);
	return (child);
}

static device_t
spibus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	return (spibus_add_child_common(
	    dev, order, name, unit, sizeof(struct spibus_ivar)));
}

static void
spibus_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t child;
	int irq;
	struct spibus_ivar *devi;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);
	devi = SPIBUS_IVAR(child);
	devi->mode = SPIBUS_MODE_NONE;
	resource_int_value(dname, dunit, "clock", &devi->clock);
	resource_int_value(dname, dunit, "cs", &devi->cs);
	resource_int_value(dname, dunit, "mode", &devi->mode);
	if (resource_int_value(dname, dunit, "irq", &irq) == 0) {
		if (bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1) != 0)
			device_printf(bus,
			    "Warning: bus_set_resource() failed\n");
	}
}

static struct resource_list *
spibus_get_resource_list(device_t bus __unused, device_t child)
{
	struct spibus_ivar *devi;

	devi = SPIBUS_IVAR(child);
	return (&devi->rl);
}

static int
spibus_transfer_impl(device_t dev, device_t child, struct spi_command *cmd)
{
	return (SPIBUS_TRANSFER(device_get_parent(dev), child, cmd));
}

static device_method_t spibus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		spibus_probe),
	DEVMETHOD(device_attach,	spibus_attach),
	DEVMETHOD(device_detach,	spibus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	spibus_suspend),
	DEVMETHOD(device_resume,	spibus_resume),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_alloc_resource,	bus_generic_rl_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource_list, spibus_get_resource_list),

	DEVMETHOD(bus_add_child,	spibus_add_child),
	DEVMETHOD(bus_print_child,	spibus_print_child),
	DEVMETHOD(bus_probe_nomatch,	spibus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	spibus_read_ivar),
	DEVMETHOD(bus_write_ivar,	spibus_write_ivar),
	DEVMETHOD(bus_child_location,	spibus_child_location),
	DEVMETHOD(bus_hinted_child,	spibus_hinted_child),

	/* spibus interface */
	DEVMETHOD(spibus_transfer,	spibus_transfer_impl),

	DEVMETHOD_END
};

driver_t spibus_driver = {
	"spibus",
	spibus_methods,
	sizeof(struct spibus_softc)
};

DRIVER_MODULE(spibus, spi, spibus_driver, 0, 0);
MODULE_VERSION(spibus, 1);
