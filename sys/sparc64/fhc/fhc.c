/*-
 * Copyright (c) 2003 Jake Burkholder.
 * Copyright (c) 2005 Marius Strobl <marius@FreeBSD.org>
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
#include <sys/module.h>
#include <sys/pcpu.h>

#include <dev/led/led.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/sbus/ofw_sbus.h>

struct fhc_clr {
	driver_filter_t		*fc_filter;
	driver_intr_t		*fc_func;
	void			*fc_arg;
	void			*fc_cookie;
	bus_space_tag_t		fc_bt;
	bus_space_handle_t	fc_bh;
};

struct fhc_devinfo {
	struct ofw_bus_devinfo	fdi_obdinfo;
	struct resource_list	fdi_rl;
};

struct fhc_softc {
	struct resource *	sc_memres[FHC_NREG];
	bus_space_handle_t	sc_bh[FHC_NREG];
	bus_space_tag_t		sc_bt[FHC_NREG];
	int			sc_nrange;
	struct sbus_ranges	*sc_ranges;
	uint32_t		sc_board;
	int			sc_ign;
	struct cdev		*sc_led_dev;
	int			sc_flags;
#define	FHC_CENTRAL		(1 << 0)
};

static device_probe_t fhc_probe;
static device_attach_t fhc_attach;
static bus_print_child_t fhc_print_child;
static bus_probe_nomatch_t fhc_probe_nomatch;
static bus_setup_intr_t fhc_setup_intr;
static bus_teardown_intr_t fhc_teardown_intr;
static bus_alloc_resource_t fhc_alloc_resource;
static bus_get_resource_list_t fhc_get_resource_list;
static ofw_bus_get_devinfo_t fhc_get_devinfo;

static driver_filter_t fhc_filter_stub;
static driver_intr_t fhc_intr_stub;
static void fhc_led_func(void *, int);
static int fhc_print_res(struct fhc_devinfo *);

static device_method_t fhc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fhc_probe),
	DEVMETHOD(device_attach,	fhc_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fhc_print_child),
	DEVMETHOD(bus_probe_nomatch,	fhc_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	fhc_setup_intr),
	DEVMETHOD(bus_teardown_intr,	fhc_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	fhc_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource_list, fhc_get_resource_list),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	fhc_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{ NULL, NULL }
};

static driver_t fhc_driver = {
	"fhc",
	fhc_methods,
	sizeof(struct fhc_softc),
};

static devclass_t fhc_devclass;

DRIVER_MODULE(fhc, central, fhc_driver, fhc_devclass, 0, 0);
DRIVER_MODULE(fhc, nexus, fhc_driver, fhc_devclass, 0, 0);

static int
fhc_probe(device_t dev)
{

	if (strcmp(ofw_bus_get_name(dev), "fhc") == 0) {
		device_set_desc(dev, "fhc");
		return (0);
	}
	return (ENXIO);
}

static int
fhc_attach(device_t dev)
{
	char ledname[sizeof("boardXX")];
	struct fhc_devinfo *fdi;
	struct sbus_regs *reg;
	struct fhc_softc *sc;
	phandle_t child;
	phandle_t node;
	device_t cdev;
	uint32_t board;
	uint32_t ctrl;
	uint32_t *intr;
	uint32_t iv;
	char *name;
	int error;
	int i;
	int nintr;
	int nreg;
	int rid;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (strcmp(device_get_name(device_get_parent(dev)), "central") == 0)
		sc->sc_flags |= FHC_CENTRAL;

	for (i = 0; i < FHC_NREG; i++) {
		rid = i;
		sc->sc_memres[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &rid, RF_ACTIVE);
		if (sc->sc_memres[i] == NULL) {
			device_printf(dev, "cannot allocate resource %d\n", i);
			error = ENXIO;
			goto fail_memres;
		}
		sc->sc_bt[i] = rman_get_bustag(sc->sc_memres[i]);
		sc->sc_bh[i] = rman_get_bushandle(sc->sc_memres[i]);
	}

	if ((sc->sc_flags & FHC_CENTRAL) != 0) {
		board = bus_space_read_4(sc->sc_bt[FHC_INTERNAL],
		    sc->sc_bh[FHC_INTERNAL], FHC_BSR);
		sc->sc_board = ((board >> 16) & 0x1) | ((board >> 12) & 0xe);
	} else {
		if (OF_getprop(node, "board#", &sc->sc_board,
		    sizeof(sc->sc_board)) == -1) {
			device_printf(dev, "cannot get board number\n");
			error = ENXIO;
			goto fail_memres;
		}
	}

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
		device_printf(dev, "cannot get ranges\n");
		error = ENXIO;
		goto fail_memres;
	}

	if ((sc->sc_flags & FHC_CENTRAL) == 0) {
		snprintf(ledname, sizeof(ledname), "board%d", sc->sc_board);
		sc->sc_led_dev = led_create(fhc_led_func, sc, ledname);
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		fdi = malloc(sizeof(*fdi), M_DEVBUF, M_WAITOK | M_ZERO);
		if (ofw_bus_gen_setup_devinfo(&fdi->fdi_obdinfo, child) != 0) {
			free(fdi, M_DEVBUF);
			continue;
		}
		nreg = OF_getprop_alloc(child, "reg", sizeof(*reg),
		    (void **)&reg);
		if (nreg == -1) {
			device_printf(dev, "<%s>: incomplete\n",
			    fdi->fdi_obdinfo.obd_name);
			ofw_bus_gen_destroy_devinfo(&fdi->fdi_obdinfo);
			free(fdi, M_DEVBUF);
			continue;
		}
		resource_list_init(&fdi->fdi_rl);
		for (i = 0; i < nreg; i++)
			resource_list_add(&fdi->fdi_rl, SYS_RES_MEMORY, i,
			    reg[i].sbr_offset, reg[i].sbr_offset +
			    reg[i].sbr_size, reg[i].sbr_size);
		free(reg, M_OFWPROP);
		nintr = OF_getprop_alloc(child, "interrupts", sizeof(*intr),
		    (void **)&intr);
		if (nintr != -1) {
			for (i = 0; i < nintr; i++) {
				iv = INTINO(intr[i]) |
				    (sc->sc_ign << INTMAP_IGN_SHIFT);
				resource_list_add(&fdi->fdi_rl, SYS_RES_IRQ, i,
				    iv, iv, 1);
			}
			free(intr, M_OFWPROP);
		}
		cdev = device_add_child(dev, NULL, -1);
		if (cdev == NULL) {
			device_printf(dev, "<%s>: device_add_child failed\n",
			    fdi->fdi_obdinfo.obd_name);
			resource_list_free(&fdi->fdi_rl);
			ofw_bus_gen_destroy_devinfo(&fdi->fdi_obdinfo);
			free(fdi, M_DEVBUF);
			continue;
		}
		device_set_ivars(cdev, fdi);
	}

	return (bus_generic_attach(dev));

 fail_memres:
	for (i = 0; i < FHC_NREG; i++)
		if (sc->sc_memres[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY,
			    rman_get_rid(sc->sc_memres[i]), sc->sc_memres[i]);
 	return (error);
}

static int
fhc_print_child(device_t dev, device_t child)
{
	int rv;

	rv = bus_print_child_header(dev, child);
	rv += fhc_print_res(device_get_ivars(child));
	rv += bus_print_child_footer(dev, child);
	return (rv);
}

static void
fhc_probe_nomatch(device_t dev, device_t child)
{
	const char *type;

	device_printf(dev, "<%s>", ofw_bus_get_name(child));
	fhc_print_res(device_get_ivars(child));
	type = ofw_bus_get_type(child);
	printf(" type %s (no driver attached)\n",
	    type != NULL ? type : "unknown");
}

static int
fhc_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
    driver_filter_t *filt, driver_intr_t *func, void *arg, void **cookiep)
{
	struct fhc_softc *sc;
	struct fhc_clr *fc;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int error;
	int i;
	long vec;
	uint32_t inr;

	if (filt != NULL && func != NULL)
		return (EINVAL);

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
	fc->fc_filter = filt;
	fc->fc_func = func;
	fc->fc_arg = arg;
	fc->fc_bt = bt;
	fc->fc_bh = bh;

	bus_space_write_4(bt, bh, FHC_IMAP, inr);
	bus_space_read_4(bt, bh, FHC_IMAP);

	error = bus_generic_setup_intr(bus, child, r, flags, fhc_filter_stub, 
	    fhc_intr_stub, fc, cookiep);	    
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

static int
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

static int
fhc_filter_stub(void *arg)
{
	struct fhc_clr *fc = arg;
	int res;

	if (fc->fc_filter != NULL) {
		res = fc->fc_filter(fc->fc_arg);
		bus_space_write_4(fc->fc_bt, fc->fc_bh, FHC_ICLR, 0x0);
		bus_space_read_4(fc->fc_bt, fc->fc_bh, FHC_ICLR);
	} else 
		res = FILTER_SCHEDULE_THREAD;

	return (res);
}

static void
fhc_intr_stub(void *arg)
{
	struct fhc_clr *fc = arg;

	fc->fc_func(fc->fc_arg);
	if (fc->fc_filter == NULL) {
		bus_space_write_4(fc->fc_bt, fc->fc_bh, FHC_ICLR, 0x0);
		bus_space_read_4(fc->fc_bt, fc->fc_bh, FHC_ICLR);
	}
}

static struct resource *
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
	}
	return (res);
}

static struct resource_list *
fhc_get_resource_list(device_t bus, device_t child)
{
	struct fhc_devinfo *fdi;

	fdi = device_get_ivars(child);
	return (&fdi->fdi_rl);
}

static const struct ofw_bus_devinfo *
fhc_get_devinfo(device_t bus, device_t child)
{
	struct fhc_devinfo *fdi;

	fdi = device_get_ivars(child);
	return (&fdi->fdi_obdinfo);
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

static int
fhc_print_res(struct fhc_devinfo *fdi)
{
	int rv;

	rv = 0;
	rv += resource_list_print_type(&fdi->fdi_rl, "mem", SYS_RES_MEMORY,
	    "%#lx");
	rv += resource_list_print_type(&fdi->fdi_rl, "irq", SYS_RES_IRQ, "%ld");
	return (rv);
}
