/*-
 * Copyright (c) 2003 Jake Burkholder.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>

#include <dev/led/led.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/fhc/fhcvar.h>
#include <sparc64/sbus/ofw_sbus.h>

struct fhc_clr {
	driver_intr_t		*fc_func;
	void			*fc_arg;
	void			*fc_cookie;
	bus_space_tag_t		fc_bt;
	bus_space_handle_t	fc_bh;
};

struct fhc_devinfo {
	char			*fdi_compat;
	char			*fdi_model;
	char			*fdi_name;
	char			*fdi_type;
	phandle_t		fdi_node;
	struct resource_list	fdi_rl;
};

static void fhc_intr_stub(void *);
static void fhc_led_func(void *, int);

int
fhc_probe(device_t dev)
{

	return (0);
}

int
fhc_attach(device_t dev)
{
	char ledname[sizeof("boardXX")];
	struct fhc_devinfo *fdi;
	struct sbus_regs *reg;
	struct fhc_softc *sc;
	phandle_t child;
	phandle_t node;
	bus_addr_t size;
	bus_addr_t off;
	device_t cdev;
	uint32_t ctrl;
	uint32_t *intr;
	uint32_t iv;
	char *name;
	int nintr;
	int nreg;
	int i;

	sc = device_get_softc(dev);
	node = sc->sc_node;

	device_printf(dev, "board %d, ", sc->sc_board);
	if (OF_getprop_alloc(node, "board-model", 1, (void **)&name) != -1) {
		printf("model %s\n", name);
		free(name, M_OFWPROP);
	} else
		printf("model unknown\n");

	for (i = FHC_FANFAIL; i <= FHC_TOD; i++) {
		bus_space_write_4(sc->sc_bt[i], sc->sc_bh[i], FHC_ICLR, 0x0);
		bus_space_read_4(sc->sc_bt[i], sc->sc_bh[i], FHC_ICLR);
	}

	sc->sc_ign = sc->sc_board << 1;
	bus_space_write_4(sc->sc_bt[FHC_IGN], sc->sc_bh[FHC_IGN], 0x0,
	    sc->sc_ign);
	sc->sc_ign = bus_space_read_4(sc->sc_bt[FHC_IGN],
	    sc->sc_bh[FHC_IGN], 0x0);

	ctrl = bus_space_read_4(sc->sc_bt[FHC_INTERNAL],
	    sc->sc_bh[FHC_INTERNAL], FHC_CTRL);
	if ((sc->sc_flags & FHC_CENTRAL) == 0)
		ctrl |= FHC_CTRL_IXIST;
	ctrl &= ~(FHC_CTRL_AOFF | FHC_CTRL_BOFF | FHC_CTRL_SLINE);
	bus_space_write_4(sc->sc_bt[FHC_INTERNAL], sc->sc_bh[FHC_INTERNAL],
	    FHC_CTRL, ctrl);
	bus_space_read_4(sc->sc_bt[FHC_INTERNAL], sc->sc_bh[FHC_INTERNAL],
	    FHC_CTRL);

	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_ranges), (void **)&sc->sc_ranges);
	if (sc->sc_nrange == -1) {
		device_printf(dev, "can't get ranges\n");
		return (ENXIO);
	}

	if ((sc->sc_flags & FHC_CENTRAL) == 0) {
		snprintf(ledname, sizeof(ledname), "board%d", sc->sc_board);
		sc->sc_led_dev = led_create(fhc_led_func, sc, ledname);
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&name)) == -1)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			fdi = malloc(sizeof(*fdi), M_DEVBUF, M_WAITOK | M_ZERO);
			if (fdi == NULL)
				continue;
			fdi->fdi_name = name;
			fdi->fdi_node = child;
			OF_getprop_alloc(child, "compatible", 1,
			    (void **)&fdi->fdi_compat);
			OF_getprop_alloc(child, "device_type", 1,
			    (void **)&fdi->fdi_type);
			OF_getprop_alloc(child, "model", 1,
			    (void **)&fdi->fdi_model);
			resource_list_init(&fdi->fdi_rl);
			nreg = OF_getprop_alloc(child, "reg", sizeof(*reg),
			    (void **)&reg);
			if (nreg != -1) {
				for (i = 0; i < nreg; i++) {
					off = reg[i].sbr_offset;
					size = reg[i].sbr_size;
					resource_list_add(&fdi->fdi_rl,
					    SYS_RES_MEMORY, i, off, off + size,
					    size);
				}
				free(reg, M_OFWPROP);
			}
			nintr = OF_getprop_alloc(child, "interrupts",
			    sizeof(*intr), (void **)&intr);
			if (nintr != -1) {
				for (i = 0; i < nintr; i++) {
					iv = INTINO(intr[i]) |
					    (sc->sc_ign << INTMAP_IGN_SHIFT);
					resource_list_add(&fdi->fdi_rl,
					    SYS_RES_IRQ, i, iv, iv, 1);
				}
				free(intr, M_OFWPROP);
			}
			device_set_ivars(cdev, fdi);
		} else
			free(name, M_OFWPROP);
	}

	return (bus_generic_attach(dev));
}

int
fhc_print_child(device_t dev, device_t child)
{
	struct fhc_devinfo *fdi;
	int rv;

	fdi = device_get_ivars(child);
	rv = bus_print_child_header(dev, child);
	rv += resource_list_print_type(&fdi->fdi_rl, "mem",
	    SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(&fdi->fdi_rl, "irq", SYS_RES_IRQ, "%ld");
	rv += bus_print_child_footer(dev, child);
	return (rv);
}

void
fhc_probe_nomatch(device_t dev, device_t child)
{
	struct fhc_devinfo *fdi;

	fdi = device_get_ivars(child);
	device_printf(dev, "<%s>", fdi->fdi_name);
	resource_list_print_type(&fdi->fdi_rl, "mem", SYS_RES_MEMORY, "%#lx");
	resource_list_print_type(&fdi->fdi_rl, "irq", SYS_RES_IRQ, "%ld");
	printf(" type %s (no driver attached)\n",
	    fdi->fdi_type != NULL ? fdi->fdi_type : "unknown");
}

int
fhc_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
    driver_intr_t *func, void *arg, void **cookiep)
{
	struct fhc_softc *sc;
	struct fhc_clr *fc;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int error;
	int i;
	long vec;
	uint32_t inr;

	sc = device_get_softc(bus);
	vec = rman_get_start(r);

	bt = NULL;
	bh = 0;
	inr = 0;
	for (i = FHC_FANFAIL; i <= FHC_TOD; i++) {
		if (INTINO(bus_space_read_4(sc->sc_bt[i], sc->sc_bh[i],
		    FHC_IMAP)) == INTINO(vec)){
			bt = sc->sc_bt[i];
			bh = sc->sc_bh[i];
			inr = INTINO(vec) | (sc->sc_ign << INTMAP_IGN_SHIFT);
			break;
		}
	}
	if (inr == 0)
		return (0);

	fc = malloc(sizeof(*fc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (fc == NULL)
		return (0);
	fc->fc_func = func;
	fc->fc_arg = arg;
	fc->fc_bt = bt;
	fc->fc_bh = bh;

	bus_space_write_4(bt, bh, FHC_IMAP, inr);
	bus_space_read_4(bt, bh, FHC_IMAP);

	error = bus_generic_setup_intr(bus, child, r, flags, fhc_intr_stub,
	    fc, cookiep);
	if (error != 0) {
		free(fc, M_DEVBUF);
		return (error);
	}
	fc->fc_cookie = *cookiep;
	*cookiep = fc;

	bus_space_write_4(bt, bh, FHC_ICLR, 0x0);
	bus_space_write_4(bt, bh, FHC_IMAP, INTMAP_ENABLE(inr, PCPU_GET(mid)));
	bus_space_read_4(bt, bh, FHC_IMAP);

	return (error);
}

int
fhc_teardown_intr(device_t bus, device_t child, struct resource *r,
    void *cookie)
{
	struct fhc_clr *fc;
	int error;

	fc = cookie;
	error = bus_generic_teardown_intr(bus, child, r, fc->fc_cookie);
	if (error != 0)
		free(fc, M_DEVBUF);
	return (error);
}

static void
fhc_intr_stub(void *arg)
{
	struct fhc_clr *fc = arg;

	fc->fc_func(fc->fc_arg);

	bus_space_write_4(fc->fc_bt, fc->fc_bh, FHC_ICLR, 0x0);
	bus_space_read_4(fc->fc_bt, fc->fc_bh, FHC_ICLR);
}

struct resource *
fhc_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct resource_list *rl;
	struct resource_list_entry *rle;
	struct fhc_softc *sc;
	struct resource *res;
	bus_addr_t coffset;
	bus_addr_t cend;
	bus_addr_t phys;
	int isdefault;
	int passthrough;
	int i;

	isdefault = (start == 0UL && end == ~0UL);
	passthrough = (device_get_parent(child) != bus);
	res = NULL;
	rle = NULL;
	rl = BUS_GET_RESOURCE_LIST(bus, child);
	sc = device_get_softc(bus);
	rle = resource_list_find(rl, type, *rid);
	switch (type) {
	case SYS_RES_IRQ:
		return (resource_list_alloc(rl, bus, child, type, rid, start,
		    end, count, flags));
		break;
	case SYS_RES_MEMORY:
		if (!passthrough) {
			if (rle == NULL)
				return (NULL);
			if (rle->res != NULL)
				panic("%s: resource entry is busy", __func__);
			if (isdefault) {
				start = rle->start;
				count = ulmax(count, rle->count);
				end = ulmax(rle->end, start + count - 1);
			}
		}
		for (i = 0; i < sc->sc_nrange; i++) {
			coffset = sc->sc_ranges[i].coffset;
			cend = coffset + sc->sc_ranges[i].size - 1;
			if (start >= coffset && end <= cend) {
				start -= coffset;
				end -= coffset;
				phys = sc->sc_ranges[i].poffset |
				    ((bus_addr_t)sc->sc_ranges[i].pspace << 32);
				res = bus_generic_alloc_resource(bus, child,
				    type, rid, phys + start, phys + end,
				    count, flags);
				if (!passthrough)
					rle->res = res;
				break;
			}
		}
		break;
	default:
		break;
	}
	return (res);
}

struct resource_list *
fhc_get_resource_list(device_t bus, device_t child)
{
	struct fhc_devinfo *fdi;

	fdi = device_get_ivars(child);
	return (&fdi->fdi_rl);
}

static void
fhc_led_func(void *arg, int onoff)
{
	struct fhc_softc *sc;
	uint32_t ctrl;

	sc = (struct fhc_softc *)arg;

	ctrl = bus_space_read_4(sc->sc_bt[FHC_INTERNAL],
	    sc->sc_bh[FHC_INTERNAL], FHC_CTRL);
	if (onoff)
		ctrl |= FHC_CTRL_RLED;
	else
		ctrl &= ~FHC_CTRL_RLED;
	ctrl &= ~(FHC_CTRL_AOFF | FHC_CTRL_BOFF | FHC_CTRL_SLINE);
	bus_space_write_4(sc->sc_bt[FHC_INTERNAL], sc->sc_bh[FHC_INTERNAL],
	    FHC_CTRL, ctrl);
	bus_space_read_4(sc->sc_bt[FHC_INTERNAL], sc->sc_bh[FHC_INTERNAL],
	    FHC_CTRL);
}

const char *
fhc_get_compat(device_t bus, device_t dev)
{   
	struct fhc_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->fdi_compat);
}

const char *
fhc_get_model(device_t bus, device_t dev)
{
	struct fhc_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->fdi_model);
}

const char *
fhc_get_name(device_t bus, device_t dev)
{
	struct fhc_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->fdi_name);
}

phandle_t
fhc_get_node(device_t bus, device_t dev)
{
	struct fhc_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->fdi_node);
}

const char *
fhc_get_type(device_t bus, device_t dev)
{
	struct fhc_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (dinfo->fdi_type);
}
