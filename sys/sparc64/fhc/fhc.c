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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_common.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/fhc/fhcvar.h>
#include <sparc64/sbus/ofw_sbus.h>

#define	INTIGN(map)	(((map) & INTMAP_IGN_MASK) >> INTMAP_IGN_SHIFT)

struct fhc_clr {
	driver_intr_t		*fc_func;
	void			*fc_arg;
	void			*fc_cookie;
	bus_space_tag_t		fc_bt;
	bus_space_handle_t	fc_bh;
};

struct fhc_devinfo {
	char			*fdi_name;
	char			*fdi_type;
	phandle_t		fdi_node;
	struct resource_list	fdi_rl;
};

static void fhc_intr_stub(void *);

int
fhc_probe(device_t dev)
{

	return (0);
}

int
fhc_attach(device_t dev)
{
	struct fhc_devinfo *fdi;
	struct sbus_regs *reg;
	struct fhc_softc *sc;
	phandle_t child;
	phandle_t node;
	bus_addr_t size;
	bus_addr_t off;
	device_t cdev;
	uint32_t ctrl;
	char *name;
	int nreg;
	int i;

	sc = device_get_softc(dev);
	node = sc->sc_node;

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
	ctrl = bus_space_read_4(sc->sc_bt[FHC_INTERNAL],
	    sc->sc_bh[FHC_INTERNAL], FHC_CTRL);

	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_ranges), (void **)&sc->sc_ranges);
	if (sc->sc_nrange == -1) {
		device_printf(dev, "can't get ranges");
		return (ENXIO);
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&name)) == -1)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			fdi = malloc(sizeof(*fdi), M_DEVBUF, M_ZERO);
			fdi->fdi_name = name;
			fdi->fdi_node = child;
			OF_getprop_alloc(child, "device_type", 1,
			    (void **)&fdi->fdi_type);
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
	printf(" type %s (no driver attached)\n",
	    fdi->fdi_type != NULL ? fdi->fdi_type : "unknown");
}

int
fhc_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct fhc_devinfo *fdi;

	if ((fdi = device_get_ivars(child)) == 0)
		return (ENOENT);
	switch (which) {
	case FHC_IVAR_NAME:
		*result = (uintptr_t)fdi->fdi_name;
		break;
	case FHC_IVAR_NODE:
		*result = fdi->fdi_node;
		break;
	case FHC_IVAR_TYPE:
		*result = (uintptr_t)fdi->fdi_type;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

int
fhc_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct fhc_devinfo *fdi;

	if ((fdi = device_get_ivars(child)) == 0)
		return (ENOENT);
	switch (which) {
	case FHC_IVAR_NAME:
	case FHC_IVAR_NODE:
	case FHC_IVAR_TYPE:
		return (EINVAL);
	default:
		return (ENOENT);
	}
	return (0);
}

int
fhc_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
    driver_intr_t *func, void *arg, void **cookiep)
{
	struct fhc_softc *sc;
	struct fhc_clr *fc;
	int error;
	int rid;

	sc = device_get_softc(bus);
	rid = rman_get_rid(r);

	fc = malloc(sizeof(*fc), M_DEVBUF, M_ZERO);
	fc->fc_func = func;
	fc->fc_arg = arg;
	fc->fc_bt = sc->sc_bt[rid];
	fc->fc_bh = sc->sc_bh[rid];

	bus_space_write_4(sc->sc_bt[rid], sc->sc_bh[rid], FHC_IMAP,
	    r->r_start);
	bus_space_read_4(sc->sc_bt[rid], sc->sc_bh[rid], FHC_IMAP);

	error = bus_generic_setup_intr(bus, child, r, flags, fhc_intr_stub,
	    fc, cookiep);
	if (error != 0) {
		free(fc, M_DEVBUF);
		return (error);
	}
	fc->fc_cookie = *cookiep;
	*cookiep = fc;

	bus_space_write_4(sc->sc_bt[rid], sc->sc_bh[rid], FHC_ICLR, 0x0);
	bus_space_write_4(sc->sc_bt[rid], sc->sc_bh[rid], FHC_IMAP,
	    INTMAP_ENABLE(r->r_start, PCPU_GET(mid)));
	bus_space_read_4(sc->sc_bt[rid], sc->sc_bh[rid], FHC_IMAP);

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
	struct resource_list_entry *rle;
	struct fhc_devinfo *fdi;
	struct fhc_softc *sc;
	struct resource *res;
	bus_addr_t coffset;
	bus_addr_t cend;
	bus_addr_t phys;
	int isdefault;
	uint32_t map;
	uint32_t vec;
	int i;

	isdefault = (start == 0UL && end == ~0UL);
	res = NULL;
	sc = device_get_softc(bus);
	switch (type) {
	case SYS_RES_IRQ:
		if (!isdefault || count != 1 || *rid < FHC_FANFAIL ||
		    *rid > FHC_TOD)
			break;

		map = bus_space_read_4(sc->sc_bt[*rid], sc->sc_bh[*rid],
		    FHC_IMAP);
		vec = INTINO(map) | (sc->sc_ign << INTMAP_IGN_SHIFT);
		bus_space_write_4(sc->sc_bt[*rid], sc->sc_bh[*rid],
		    FHC_IMAP, vec);
		bus_space_read_4(sc->sc_bt[*rid], sc->sc_bh[*rid], FHC_IMAP);

		res = bus_generic_alloc_resource(bus, child, type, rid,
		    vec, vec, 1, flags);
		if (res != NULL)
			rman_set_rid(res, *rid);
		break;
	case SYS_RES_MEMORY:
		fdi = device_get_ivars(child);
		rle = resource_list_find(&fdi->fdi_rl, type, *rid);
		if (rle == NULL)
			return (NULL);
		if (rle->res != NULL)
			panic("fhc_alloc_resource: resource entry is busy");
		if (isdefault) {
			start = rle->start;
			count = ulmax(count, rle->count);
			end = ulmax(rle->end, start + count - 1);
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

int
fhc_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	struct resource_list_entry *rle;
	struct fhc_devinfo *fdi;
	int error;

	error = bus_generic_release_resource(bus, child, type, rid, r);
	if (error != 0)
		return (error);
	fdi = device_get_ivars(child);
	rle = resource_list_find(&fdi->fdi_rl, type, rid);
	if (rle == NULL)
		panic("fhc_release_resource: can't find resource");
	if (rle->res == NULL)
		panic("fhc_release_resource: resource entry is not busy");
	rle->res = NULL;
	return (error);
}
