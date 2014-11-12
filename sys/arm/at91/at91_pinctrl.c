/*-
 * Copyright (c) 2014 Warner Losh.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>
#include <sys/systm.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <arm/at91/at91var.h>
#include <arm/at91/at91_piovar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define BUS_PASS_PINMUX (BUS_PASS_INTERRUPT + 1)

struct pinctrl_range {
	uint64_t bus;
	uint64_t host;
	uint64_t size;
};

struct pinctrl_softc {
	device_t dev;
	phandle_t node;

	struct pinctrl_range *ranges;
	int nranges;

	pcell_t acells, scells;
	int done_pinmux;
};

struct pinctrl_devinfo {
	struct ofw_bus_devinfo	obdinfo;
	struct resource_list	rl;
};

static int
at91_pinctrl_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "atmel,at91rm9200-pinctrl"))
		return (ENXIO);
	device_set_desc(dev, "pincontrol bus");
        return (0);
}

/* XXX Make this a subclass of simplebus */

static struct pinctrl_devinfo *
at91_pinctrl_setup_dinfo(device_t dev, phandle_t node)
{
	struct pinctrl_softc *sc;
	struct pinctrl_devinfo *ndi;
	uint32_t *reg, *intr, icells;
	uint64_t phys, size;
	phandle_t iparent;
	int i, j, k;
	int nintr;
	int nreg;

	sc = device_get_softc(dev);

	ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	if (ofw_bus_gen_setup_devinfo(&ndi->obdinfo, node) != 0) {
		free(ndi, M_DEVBUF);
		return (NULL);
	}

	resource_list_init(&ndi->rl);
	nreg = OF_getencprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1)
		nreg = 0;
	if (nreg % (sc->acells + sc->scells) != 0) {
//		if (bootverbose)
			device_printf(dev, "Malformed reg property on <%s>\n",
			    ndi->obdinfo.obd_name);
		nreg = 0;
	}

	for (i = 0, k = 0; i < nreg; i += sc->acells + sc->scells, k++) {
		phys = size = 0;
		for (j = 0; j < sc->acells; j++) {
			phys <<= 32;
			phys |= reg[i + j];
		}
		for (j = 0; j < sc->scells; j++) {
			size <<= 32;
			size |= reg[i + sc->acells + j];
		}
		
		resource_list_add(&ndi->rl, SYS_RES_MEMORY, k,
		    phys, phys + size - 1, size);
	}
	free(reg, M_OFWPROP);

	nintr = OF_getencprop_alloc(node, "interrupts",  sizeof(*intr),
	    (void **)&intr);
	if (nintr > 0) {
		if (OF_searchencprop(node, "interrupt-parent", &iparent,
		    sizeof(iparent)) == -1) {
			device_printf(dev, "No interrupt-parent found, "
			    "assuming direct parent\n");
			iparent = OF_parent(node);
		}
		if (OF_searchencprop(OF_node_from_xref(iparent), 
		    "#interrupt-cells", &icells, sizeof(icells)) == -1) {
			device_printf(dev, "Missing #interrupt-cells property,"
			    " assuming <1>\n");
			icells = 1;
		}
		if (icells < 1 || icells > nintr) {
			device_printf(dev, "Invalid #interrupt-cells property "
			    "value <%d>, assuming <1>\n", icells);
			icells = 1;
		}
		for (i = 0, k = 0; i < nintr; i += icells, k++) {
			intr[i] = ofw_bus_map_intr(dev, iparent, icells,
			    &intr[i]);
			resource_list_add(&ndi->rl, SYS_RES_IRQ, k, intr[i],
			    intr[i], 1);
		}
		free(intr, M_OFWPROP);
	}

	return (ndi);
}

static int
at91_pinctrl_fill_ranges(phandle_t node, struct pinctrl_softc *sc)
{
	int host_address_cells;
	cell_t *base_ranges;
	ssize_t nbase_ranges;
	int err;
	int i, j, k;

	err = OF_searchencprop(OF_parent(node), "#address-cells",
	    &host_address_cells, sizeof(host_address_cells));
	if (err <= 0)
		return (-1);

	nbase_ranges = OF_getproplen(node, "ranges");
	if (nbase_ranges < 0)
		return (-1);
	sc->nranges = nbase_ranges / sizeof(cell_t) /
	    (sc->acells + host_address_cells + sc->scells);
	if (sc->nranges == 0)
		return (0);

	sc->ranges = malloc(sc->nranges * sizeof(sc->ranges[0]),
	    M_DEVBUF, M_WAITOK);
	base_ranges = malloc(nbase_ranges, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "ranges", base_ranges, nbase_ranges);

	for (i = 0, j = 0; i < sc->nranges; i++) {
		sc->ranges[i].bus = 0;
		for (k = 0; k < sc->acells; k++) {
			sc->ranges[i].bus <<= 32;
			sc->ranges[i].bus |= base_ranges[j++];
		}
		sc->ranges[i].host = 0;
		for (k = 0; k < host_address_cells; k++) {
			sc->ranges[i].host <<= 32;
			sc->ranges[i].host |= base_ranges[j++];
		}
		sc->ranges[i].size = 0;
		for (k = 0; k < sc->scells; k++) {
			sc->ranges[i].size <<= 32;
			sc->ranges[i].size |= base_ranges[j++];
		}
	}

	free(base_ranges, M_DEVBUF);
	return (sc->nranges);
}

static int
at91_pinctrl_attach(device_t dev)
{
	struct pinctrl_softc *sc;
	struct pinctrl_devinfo *di;
	phandle_t	node;
	device_t	cdev;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	sc->dev = dev;
	sc->node = node;
	
	/*
	 * Some important numbers
	 */
	sc->acells = 2;
	OF_getencprop(node, "#address-cells", &sc->acells, sizeof(sc->acells));
	sc->scells = 1;
	OF_getencprop(node, "#size-cells", &sc->scells, sizeof(sc->scells));

	if (at91_pinctrl_fill_ranges(node, sc) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		if ((di = at91_pinctrl_setup_dinfo(dev, node)) == NULL)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    di->obdinfo.obd_name);
			resource_list_free(&di->rl);
			ofw_bus_gen_destroy_devinfo(&di->obdinfo);
			free(di, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, di);
	}

	return (bus_generic_attach(dev));
}

static const struct ofw_bus_devinfo *
pinctrl_get_devinfo(device_t bus __unused, device_t child)
{
        struct pinctrl_devinfo *ndi;
        
        ndi = device_get_ivars(child);
        return (&ndi->obdinfo);
}

static struct resource *
pinctrl_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct pinctrl_softc *sc;
	struct pinctrl_devinfo *di;
	struct resource_list_entry *rle;
	int j;

	sc = device_get_softc(bus);

	/*
	 * Request for the default allocation with a given rid: use resource
	 * list stored in the local device info.
	 */
	if ((start == 0UL) && (end == ~0UL)) {
		if ((di = device_get_ivars(child)) == NULL)
			return (NULL);

		if (type == SYS_RES_IOPORT)
			type = SYS_RES_MEMORY;

		rle = resource_list_find(&di->rl, type, *rid);
		if (rle == NULL) {
//			if (bootverbose)
				device_printf(bus, "no default resources for "
				    "rid = %d, type = %d\n", *rid, type);
			return (NULL);
		}
		start = rle->start;
		end = rle->end;
		count = rle->count;
        }

	if (type == SYS_RES_MEMORY) {
		/* Remap through ranges property */
		for (j = 0; j < sc->nranges; j++) {
			if (start >= sc->ranges[j].bus && end <
			    sc->ranges[j].bus + sc->ranges[j].size) {
				start -= sc->ranges[j].bus;
				start += sc->ranges[j].host;
				end -= sc->ranges[j].bus;
				end += sc->ranges[j].host;
				break;
			}
		}
		if (j == sc->nranges && sc->nranges != 0) {
//			if (bootverbose)
				device_printf(bus, "Could not map resource "
				    "%#lx-%#lx\n", start, end);

			return (NULL);
		}
	}

	return (bus_generic_alloc_resource(bus, child, type, rid, start, end,
	    count, flags));
}

static int
pinctrl_print_res(struct pinctrl_devinfo *di)
{
	int rv;

	rv = 0;
	rv += resource_list_print_type(&di->rl, "mem", SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(&di->rl, "irq", SYS_RES_IRQ, "%ld");
	return (rv);
}

static void
pinctrl_probe_nomatch(device_t bus, device_t child)
{
	const char *name, *type, *compat;

//	if (!bootverbose)
		return;

	name = ofw_bus_get_name(child);
	type = ofw_bus_get_type(child);
	compat = ofw_bus_get_compat(child);

	device_printf(bus, "<%s>", name != NULL ? name : "unknown");
	pinctrl_print_res(device_get_ivars(child));
	if (!ofw_bus_status_okay(child))
		printf(" disabled");
	if (type)
		printf(" type %s", type);
	if (compat)
		printf(" compat %s", compat);
	printf(" (no driver attached)\n");
}

static int
pinctrl_print_child(device_t bus, device_t child)
{
	int rv;

	rv = bus_print_child_header(bus, child);
	rv += pinctrl_print_res(device_get_ivars(child));
	if (!ofw_bus_status_okay(child))
		rv += printf(" disabled");
	rv += bus_print_child_footer(bus, child);
	return (rv);
}

const char *periphs[] = {"gpio", "periph A", "periph B", "periph C", "periph D", "periph E" };

static void
pinctrl_walk_tree(device_t bus, phandle_t node)
{
	struct pinctrl_softc *sc;
	char status[10];
	char name[32];
	phandle_t pinctrl[32], pins[32 * 4], scratch;
	ssize_t len, npins;
	int i, j;

	sc = device_get_softc(bus);
	for (node = OF_child(node); node > 0; node = OF_peer(node)) {
		pinctrl_walk_tree(bus, node);
		memset(status, 0, sizeof(status));
		memset(name, 0, sizeof(name));
		OF_getprop(node, "status", status, sizeof(status));
		OF_getprop(node, "name", name, sizeof(name));
		if (strcmp(status, "okay") != 0) {
//			printf("pinctrl: skipping node %s status %s\n", name,
//			    status);
			continue;
		}
		len = OF_getencprop(node, "pinctrl-0", pinctrl, sizeof(pinctrl));
		if (len <= 0) {
//			printf("pinctrl: skipping node %s no pinctrl-0\n",
//			    name, status);
			continue;
		}
		len /= sizeof(phandle_t);
		printf("pinctrl: Found active node %s\n", name);
		for (i = 0; i < len; i++) {
			scratch = OF_node_from_xref(pinctrl[i]);
			npins = OF_getencprop(scratch, "atmel,pins", pins,
			    sizeof(pins));
			if (npins <= 0) {
				printf("We're doing it wrong %s\n", name);
				continue;
			}
			memset(name, 0, sizeof(name));
			OF_getprop(scratch, "name", name, sizeof(name));
			npins /= (4 * 4);
			printf("----> need to cope with %d more pins for %s\n",
			    npins, name);
			for (j = 0; j < npins; j++) {
				uint32_t unit = pins[j * 4];
				uint32_t pin = pins[j * 4 + 1];
				uint32_t periph = pins[j * 4 + 2];
				uint32_t flags = pins[j * 4 + 3];
				uint32_t pio;

				pio = (0xfffffff & sc->ranges[0].bus) +
				    0x200 * unit;
				printf("P%c%d %s %#x\n", unit + 'A', pin,
				    periphs[periph], flags);
				switch (periph) {
				case 0:
					at91_pio_use_gpio(pio, 1u << pin);
					at91_pio_gpio_pullup(pio, 1u << pin,
					    !!(flags & 1));
					at91_pio_gpio_high_z(pio, 1u << pin,
					    !!(flags & 2));
					at91_pio_gpio_set_deglitch(pio,
					    1u << pin, !!(flags & 4));
//					at91_pio_gpio_pulldown(pio, 1u << pin,
//					    !!(flags & 8));
//					at91_pio_gpio_dis_schmidt(pio,
//					    1u << pin, !!(flags & 16));
					break;
				case 1:
					at91_pio_use_periph_a(pio, 1u << pin,
					    flags);
					break;
				case 2:
					at91_pio_use_periph_b(pio, 1u << pin,
					    flags);
					break;
				}
			}
		}
	}
}

static void
pinctrl_new_pass(device_t bus)
{
	struct pinctrl_softc *sc;
	phandle_t node;

	sc = device_get_softc(bus);

	bus_generic_new_pass(bus);

	if (sc->done_pinmux || bus_current_pass < BUS_PASS_PINMUX)
		return;
	sc->done_pinmux++;

	node = OF_peer(0);
	if (node == -1)
		return;
	pinctrl_walk_tree(bus, node);
}

static device_method_t at91_pinctrl_methods[] = {
	DEVMETHOD(device_probe, at91_pinctrl_probe),
	DEVMETHOD(device_attach, at91_pinctrl_attach),

	DEVMETHOD(bus_print_child,	pinctrl_print_child),
	DEVMETHOD(bus_probe_nomatch,	pinctrl_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	pinctrl_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource,	bus_generic_adjust_resource),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),
	DEVMETHOD(bus_new_pass,		pinctrl_new_pass),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	pinctrl_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

static driver_t at91_pinctrl_driver = {
	"at91_pinctrl",
	at91_pinctrl_methods,
	sizeof(struct pinctrl_softc),
};

static devclass_t at91_pinctrl_devclass;

EARLY_DRIVER_MODULE(at91_pinctrl, simplebus, at91_pinctrl_driver,
    at91_pinctrl_devclass, NULL, NULL, BUS_PASS_BUS);

/*
 * dummy driver to force pass BUS_PASS_PINMUX to happen.
 */
static int
at91_pingroup_probe(device_t dev)
{
	return ENXIO;
}

static device_method_t at91_pingroup_methods[] = {
	DEVMETHOD(device_probe, at91_pingroup_probe),

	DEVMETHOD_END
};
	

static driver_t at91_pingroup_driver = {
	"at91_pingroup",
	at91_pingroup_methods,
	0,
};

static devclass_t at91_pingroup_devclass;

EARLY_DRIVER_MODULE(at91_pingroup, at91_pinctrl, at91_pingroup_driver,
    at91_pingroup_devclass, NULL, NULL, BUS_PASS_PINMUX);
