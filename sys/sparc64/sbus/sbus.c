/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 1999 Eduardo Horvath
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sbus.c	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: sbus.c,v 1.46 2001/10/07 20:30:41 eeh Exp
 *
 * $FreeBSD$
 */

/*
 * Sbus support.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/pcpu.h>
#include <sys/reboot.h>

#include <ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/iommureg.h>
#include <machine/bus_common.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/iommuvar.h>

#include <sparc64/sbus/ofw_sbus.h>
#include <sparc64/sbus/sbusreg.h>
#include <sparc64/sbus/sbusvar.h>


#ifdef DEBUG
#define SDB_DVMA	0x1
#define SDB_INTR	0x2
int sbus_debug = 0;
#define DPRINTF(l, s)   do { if (sbus_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

struct sbus_devinfo {
	int			sdi_burstsz;
	char			*sdi_compat;
	char			*sdi_name;	/* PROM name */
	phandle_t		sdi_node;	/* PROM node */
	int			sdi_slot;
	char			*sdi_type;	/* PROM name */

	struct resource_list	sdi_rl;
};

/* Range descriptor, allocated for each sc_range. */
struct sbus_rd {
	bus_addr_t		rd_poffset;
	bus_addr_t		rd_pend;
	int			rd_slot;
	bus_addr_t		rd_coffset;
	bus_addr_t		rd_cend;
	struct rman		rd_rman;
	bus_space_handle_t	rd_bushandle;
	struct resource		*rd_res;
};

struct sbus_softc {
	bus_space_tag_t		sc_bustag;
	bus_space_handle_t	sc_bushandle;
	bus_dma_tag_t		sc_dmatag;
	bus_dma_tag_t		sc_cdmatag;
	bus_space_tag_t		sc_cbustag;
	int			sc_clockfreq;	/* clock frequency (in Hz) */
	struct upa_regs		*sc_reg;
	int			sc_nreg;
	int			sc_nrange;
	struct sbus_rd		*sc_rd;
	int			sc_burst;	/* burst transfer sizes supported */
	int			*sc_intr_compat;/* `intr' property to sbus compat */

	struct resource		*sc_sysio_res;
	int			sc_ign;		/* Interrupt group number for this sysio */
	struct iommu_state	sc_is;		/* IOMMU state, see iommureg.h */

	struct resource		*sc_ot_ires;
	void			*sc_ot_ihand;
	struct resource		*sc_pf_ires;
	void			*sc_pf_ihand;
};

struct sbus_clr {
	struct sbus_softc	*scl_sc;
	bus_addr_t	scl_clr;		/* clear register */
	driver_intr_t	*scl_handler;		/* handler to call */
	void		*scl_arg;		/* argument for the handler */
	void		*scl_cookie;		/* interrupt cookie of parent bus */
};

#define	SYSIO_READ8(sc, off) \
	bus_space_read_8((sc)->sc_bustag, (sc)->sc_bushandle, (off))
#define	SYSIO_WRITE8(sc, off, v) \
	bus_space_write_8((sc)->sc_bustag, (sc)->sc_bushandle, (off), (v))

static int sbus_probe(device_t dev);
static int sbus_print_child(device_t dev, device_t child);
static void sbus_probe_nomatch(device_t dev, device_t child);
static int sbus_read_ivar(device_t, device_t, int, u_long *);
static struct resource_list *sbus_get_resource_list(device_t dev,
    device_t child);
static int sbus_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int sbus_teardown_intr(device_t, device_t, struct resource *, void *);
static struct resource *sbus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int sbus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int sbus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static int sbus_release_resource(device_t, device_t, int, int,
    struct resource *);

static struct sbus_devinfo * sbus_setup_dinfo(struct sbus_softc *sc,
    phandle_t node, char *name);
static void sbus_destroy_dinfo(struct sbus_devinfo *dinfo);
static void sbus_intr_stub(void *);
static bus_space_tag_t sbus_alloc_bustag(struct sbus_softc *);
static void sbus_overtemp(void *);
static void sbus_pwrfail(void *);

static device_method_t sbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sbus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	sbus_print_child),
	DEVMETHOD(bus_probe_nomatch,	sbus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	sbus_read_ivar),
	DEVMETHOD(bus_setup_intr, 	sbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	sbus_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	sbus_alloc_resource),
	DEVMETHOD(bus_activate_resource,	sbus_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	sbus_deactivate_resource),
	DEVMETHOD(bus_release_resource,	sbus_release_resource),
	DEVMETHOD(bus_get_resource_list, sbus_get_resource_list),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

	{ 0, 0 }
};

static driver_t sbus_driver = {
	"sbus",
	sbus_methods,
	sizeof(struct sbus_softc),
};

static devclass_t sbus_devclass;

DRIVER_MODULE(sbus, nexus, sbus_driver, sbus_devclass, 0, 0);

#define	OFW_SBUS_TYPE	"sbus"
#define	OFW_SBUS_NAME	"sbus"

static int
sbus_probe(device_t dev)
{
	struct sbus_softc *sc = device_get_softc(dev);
	struct sbus_devinfo *sdi;
	struct sbus_ranges *range;
	struct resource *res;
	device_t cdev;
	bus_addr_t phys;
	bus_size_t size;
	char *name, *cname, *t;
	phandle_t child, node = nexus_get_node(dev);
	u_int64_t mr;
	int intr, clock, rid, vec, i;

	t = nexus_get_device_type(dev);
	if (((t == NULL || strcmp(t, OFW_SBUS_TYPE) != 0)) &&
	    strcmp(nexus_get_name(dev), OFW_SBUS_NAME) != 0)
		return (ENXIO);
	device_set_desc(dev, "U2S UPA-SBus bridge");

	if ((sc->sc_nreg = OF_getprop_alloc(node, "reg", sizeof(*sc->sc_reg),
	    (void **)&sc->sc_reg)) == -1) {
		panic("sbus_probe: error getting reg property");
	}
	if (sc->sc_nreg < 1)
		panic("sbus_probe: bogus properties");
	phys = UPA_REG_PHYS(&sc->sc_reg[0]);
	size = UPA_REG_SIZE(&sc->sc_reg[0]);
	rid = 0;
	sc->sc_sysio_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, phys,
	    phys + size - 1, size, RF_ACTIVE);
	if (sc->sc_sysio_res == NULL ||
	    rman_get_start(sc->sc_sysio_res) != phys)
		panic("sbus_probe: can't allocate device memory");
	sc->sc_bustag = rman_get_bustag(sc->sc_sysio_res);
	sc->sc_bushandle = rman_get_bushandle(sc->sc_sysio_res);

	if (OF_getprop(node, "interrupts", &intr, sizeof(intr)) == -1)
		panic("sbus_probe: cannot get IGN");
	sc->sc_ign = intr & INTMAP_IGN_MASK;	/* Find interrupt group no */
	sc->sc_cbustag = sbus_alloc_bustag(sc);

	/*
	 * Record clock frequency for synchronous SCSI.
	 * IS THIS THE CORRECT DEFAULT??
	 */
	if (OF_getprop(node, "clock-frequency", &clock, sizeof(clock)) == -1)
		clock = 25000000;
	sc->sc_clockfreq = clock;
	clock /= 1000;
	device_printf(dev, "clock %d.%03d MHz\n", clock / 1000, clock % 1000);

	/*
	 * Collect address translations from the OBP.
	 */
	if ((sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*range), (void **)&range)) == -1) {
		panic("%s: error getting ranges property",
		    device_get_name(dev));
	}
	sc->sc_rd = (struct sbus_rd *)malloc(sizeof(*sc->sc_rd) * sc->sc_nrange,
	    M_DEVBUF, M_NOWAIT);
	if (sc->sc_rd == NULL)
		panic("sbus_probe: could not allocate rmans");
	/*
	 * Preallocate all space that the SBus bridge decodes, so that nothing
	 * else gets in the way; set up rmans etc.
	 */
	for (i = 0; i < sc->sc_nrange; i++) {
		phys = range[i].poffset | ((bus_addr_t)range[i].pspace << 32);
		size = range[i].size;
		sc->sc_rd[i].rd_slot = range[i].cspace;
		sc->sc_rd[i].rd_coffset = range[i].coffset;
		sc->sc_rd[i].rd_cend = sc->sc_rd[i].rd_coffset + size;
		rid = 0;
		if ((res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, phys,
		    phys + size - 1, size, RF_ACTIVE)) == NULL)
			panic("sbus_probe: could not allocate decoded range");
		sc->sc_rd[i].rd_bushandle = rman_get_bushandle(res);
		sc->sc_rd[i].rd_rman.rm_type = RMAN_ARRAY;
		sc->sc_rd[i].rd_rman.rm_descr = "SBus Device Memory";
		if (rman_init(&sc->sc_rd[i].rd_rman) != 0 ||
		    rman_manage_region(&sc->sc_rd[i].rd_rman, 0, size) != 0)
			panic("sbus_probe: failed to set up memory rman");
		sc->sc_rd[i].rd_poffset = phys;
		sc->sc_rd[i].rd_pend = phys + size;
		sc->sc_rd[i].rd_res = res;
	}
	free(range, M_OFWPROP);

	/*
	 * Get the SBus burst transfer size if burst transfers are supported.
	 * XXX: is the default correct?
	 */
	if (OF_getprop(node, "burst-sizes", &sc->sc_burst,
	    sizeof(sc->sc_burst)) == -1 || sc->sc_burst == 0)
		sc->sc_burst = SBUS_BURST_DEF;

	/* initalise the IOMMU */

	/* punch in our copies */
	sc->sc_is.is_bustag = sc->sc_bustag;
	sc->sc_is.is_bushandle = sc->sc_bushandle;
	sc->sc_is.is_iommu = SBR_IOMMU;
	sc->sc_is.is_dtag = SBR_IOMMU_TLB_TAG_DIAG;
	sc->sc_is.is_ddram = SBR_IOMMU_TLB_DATA_DIAG;
	sc->sc_is.is_dqueue = SBR_IOMMU_QUEUE_DIAG;
	sc->sc_is.is_dva = SBR_IOMMU_SVADIAG;
	sc->sc_is.is_dtcmp = 0;
	sc->sc_is.is_sb[0] = SBR_STRBUF;
	sc->sc_is.is_sb[1] = NULL;

	/* give us a nice name.. */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == 0)
		panic("sbus_probe: couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", device_get_name(dev));

	/*
	 * Note: the SBus IOMMU ignores the high bits of an address, so a NULL
	 * DMA pointer will be translated by the first page of the IOTSB.
	 * To detect bugs we'll allocate and ignore the first entry.
	 */
	iommu_init(name, &sc->sc_is, 3, -1, 1);

	/* Create the DMA tag. */
	sc->sc_dmatag = nexus_get_dmatag(dev);
	if (bus_dma_tag_create(sc->sc_dmatag, 8, 1, 0, 0x3ffffffff, NULL, NULL,
	    0x3ffffffff, 0xff, 0xffffffff, 0, &sc->sc_cdmatag) != 0)
		panic("bus_dma_tag_create failed");
	/* Customize the tag. */
	sc->sc_cdmatag->dt_cookie = &sc->sc_is;
	sc->sc_cdmatag->dt_mt = &iommu_dma_methods;
	/* XXX: register as root dma tag (kludge). */
	sparc64_root_dma_tag = sc->sc_cdmatag;

	/* Enable the over-temperature and power-fail intrrupts. */
	rid = 0;
	mr = SYSIO_READ8(sc, SBR_THERM_INT_MAP);
	vec = INTVEC(mr);
	if ((sc->sc_ot_ires = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, vec,
	    vec, 1, RF_ACTIVE)) == NULL)
		panic("sbus_probe: failed to get temperature interrupt");
	bus_setup_intr(dev, sc->sc_ot_ires, INTR_TYPE_MISC | INTR_FAST,
	    sbus_overtemp, sc, &sc->sc_ot_ihand);
	SYSIO_WRITE8(sc, SBR_THERM_INT_MAP, INTMAP_ENABLE(mr, PCPU_GET(mid)));
	rid = 0;
	mr = SYSIO_READ8(sc, SBR_POWER_INT_MAP);
	vec = INTVEC(mr);
	if ((sc->sc_pf_ires = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, vec,
	    vec, 1, RF_ACTIVE)) == NULL)
		panic("sbus_probe: failed to get power fail interrupt");
	bus_setup_intr(dev, sc->sc_pf_ires, INTR_TYPE_MISC | INTR_FAST,
	    sbus_pwrfail, sc, &sc->sc_pf_ihand);
	SYSIO_WRITE8(sc, SBR_POWER_INT_MAP, INTMAP_ENABLE(mr, PCPU_GET(mid)));

	/* Initialize the counter-timer. */
	sparc64_counter_init(sc->sc_bustag, sc->sc_bushandle, SBR_TC0);

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 * `specials' is an array of device names that are treated
	 * specially:
	 */
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&cname)) == -1)
			continue;

		if ((sdi = sbus_setup_dinfo(sc, child, cname)) == NULL) {
			device_printf(dev, "<%s>: incomplete\n", cname);
			free(cname, M_OFWPROP);
			continue;
		}
		if ((cdev = device_add_child(dev, NULL, -1)) == NULL)
			panic("sbus_probe: device_add_child failed");
		device_set_ivars(cdev, sdi);
	}
	return (0);
}

static struct sbus_devinfo *
sbus_setup_dinfo(struct sbus_softc *sc, phandle_t node, char *name)
{
	struct sbus_devinfo *sdi;
	struct sbus_regs *reg;
	u_int32_t base, iv, *intr;
	int i, nreg, nintr, slot, rslot;

	sdi = malloc(sizeof(*sdi), M_DEVBUF, M_ZERO | M_WAITOK);
	if (sdi == NULL)
		return (NULL);
	resource_list_init(&sdi->sdi_rl);
	sdi->sdi_name = name;
	sdi->sdi_node = node;
	OF_getprop_alloc(node, "compat", 1, (void **)&sdi->sdi_compat);
	OF_getprop_alloc(node, "device_type", 1, (void **)&sdi->sdi_type);
	slot = -1;
	nreg = OF_getprop_alloc(node, "reg", sizeof(*reg), (void **)&reg);
	if (nreg == -1) {
		if (sdi->sdi_type == NULL ||
		    strcmp(sdi->sdi_type, "hierarchical") != 0) {
			sbus_destroy_dinfo(sdi);
			return (NULL);
		}
	} else {
		for (i = 0; i < nreg; i++) {
			base = reg[i].sbr_offset;
			if (SBUS_ABS(base)) {
				rslot = SBUS_ABS_TO_SLOT(base);
				base = SBUS_ABS_TO_OFFSET(base);
			} else
				rslot = reg[i].sbr_slot;
			if (slot != -1 && slot != rslot)
				panic("sbus_setup_dinfo: multiple slots");
			slot = rslot;

			resource_list_add(&sdi->sdi_rl, SYS_RES_MEMORY, i,
			    base, base + reg[i].sbr_size, reg[i].sbr_size);
		}
		free(reg, M_OFWPROP);
	}
	sdi->sdi_slot = slot;

	/*
	 * The `interrupts' property contains the Sbus interrupt level.
	 */
	nintr = OF_getprop_alloc(node, "interrupts", sizeof(*intr), (void **)&intr);
	if (nintr != -1) {
		for (i = 0; i < nintr; i++) {
			iv = intr[i];
			/*
			 * Sbus card devices need the slot number encoded into
			 * the vector as this is generally not done.
			 */
			if ((iv & INTMAP_OBIO_MASK) == 0)
				iv |= slot << 3;
			/* Set the ign as appropriate. */
			iv |= sc->sc_ign;
			resource_list_add(&sdi->sdi_rl, SYS_RES_IRQ, i,
			    iv, iv, 1);
		}
		free(intr, M_OFWPROP);
	}
	if (OF_getprop(node, "burst-sizes", &sdi->sdi_burstsz,
	    sizeof(sdi->sdi_burstsz)) == -1)
		sdi->sdi_burstsz = sc->sc_burst;
	else
		sdi->sdi_burstsz &= sc->sc_burst;

	return (sdi);
}

/* Free everything except sdi_name, which is handled separately. */
static void
sbus_destroy_dinfo(struct sbus_devinfo *dinfo)
{

	resource_list_free(&dinfo->sdi_rl);
	if (dinfo->sdi_compat != NULL)
		free(dinfo->sdi_compat, M_OFWPROP);
	if (dinfo->sdi_type != NULL)
		free(dinfo->sdi_type, M_OFWPROP);
	free(dinfo, M_DEVBUF);
}

static int
sbus_print_child(device_t dev, device_t child)
{
	struct sbus_devinfo *dinfo;
	struct resource_list *rl;
	int rv;

	dinfo = device_get_ivars(child);
	rl = &dinfo->sdi_rl;
	rv = bus_print_child_header(dev, child);
	rv += resource_list_print_type(rl, "mem", SYS_RES_MEMORY, "%#lx");
	rv += resource_list_print_type(rl, "irq", SYS_RES_IRQ, "%ld");
	rv += bus_print_child_footer(dev, child);
	return (rv);
}

static void
sbus_probe_nomatch(device_t dev, device_t child)
{
	char *name;
	char *type;

	if (BUS_READ_IVAR(dev, child, SBUS_IVAR_NAME,
	    (uintptr_t *)&name) != 0 ||
	    BUS_READ_IVAR(dev, child, SBUS_IVAR_DEVICE_TYPE,
	    (uintptr_t *)&type) != 0)
		return;

	if (type == NULL)
		type = "(unknown)";
	device_printf(dev, "<%s>, type %s (no driver attached)\n",
	    name, type);
}

static int
sbus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct sbus_softc *sc = device_get_softc(dev);
	struct sbus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(child)) == NULL)
		return (ENOENT);
	switch (which) {
	case SBUS_IVAR_BURSTSZ:
		*result = dinfo->sdi_burstsz;
		break;
	case SBUS_IVAR_CLOCKFREQ:
		*result = sc->sc_clockfreq;
		break;
	case SBUS_IVAR_COMPAT:
		*result = (uintptr_t)dinfo->sdi_compat;
		break;
	case SBUS_IVAR_NAME:
		*result = (uintptr_t)dinfo->sdi_name;
		break;
	case SBUS_IVAR_NODE:
		*result = dinfo->sdi_node;
		break;
	case SBUS_IVAR_SLOT:
		*result = dinfo->sdi_slot;
		break;
	case SBUS_IVAR_DEVICE_TYPE:
		*result = (uintptr_t)dinfo->sdi_type;
		break;
	default:
		return (ENOENT);
	}
	return 0;
}

static struct resource_list *
sbus_get_resource_list(device_t dev, device_t child)
{
	struct sbus_devinfo *sdi;

	sdi = device_get_ivars(child);
	return (&sdi->sdi_rl);
}

/* Write to the correct clr register, and call the actual handler. */
static void
sbus_intr_stub(void *arg)
{
	struct sbus_clr *scl;

	scl = (struct sbus_clr *)arg;
	scl->scl_handler(scl->scl_arg);
	SYSIO_WRITE8(scl->scl_sc, scl->scl_clr, 0);
}

static int
sbus_setup_intr(device_t dev, device_t child,
    struct resource *ires,  int flags, driver_intr_t *intr, void *arg,
    void **cookiep)
{
	struct sbus_softc *sc;
	struct sbus_clr *scl;
	bus_addr_t intrmapptr, intrclrptr, intrptr;
	u_int64_t intrmap;
	u_int32_t inr, slot;
	int error, i;
	long vec = rman_get_start(ires);

	sc = (struct sbus_softc *)device_get_softc(dev);
	scl = (struct sbus_clr *)malloc(sizeof(*scl), M_DEVBUF, M_NOWAIT);
	if (scl == NULL)
		return (NULL);
	intrptr = intrmapptr = intrclrptr = 0;
	intrmap = 0;
	inr = INTVEC(vec);
	if ((inr & INTMAP_OBIO_MASK) == 0) {
		/*
		 * We're in an SBUS slot, register the map and clear
		 * intr registers.
		 */
		slot = INTSLOT(vec);
		intrmapptr = SBR_SLOT0_INT_MAP + slot * 8;
		intrclrptr = SBR_SLOT0_INT_CLR +
		    (slot * 8 * 8) + (INTPRI(vec) * 8);
		/* Enable the interrupt, insert IGN. */
		intrmap = inr | sc->sc_ign;
	} else {
		intrptr = SBR_SCSI_INT_MAP;
		/* Insert IGN */
		inr |= sc->sc_ign;
		for (i = 0; intrptr <= SBR_RESERVED_INT_MAP &&
			 INTVEC(intrmap = SYSIO_READ8(sc, intrptr)) !=
			 INTVEC(inr); intrptr += 8, i++)
			;
		if (INTVEC(intrmap) == INTVEC(inr)) {
			/* Register the map and clear intr registers */
			intrmapptr = intrptr;
			intrclrptr = SBR_SCSI_INT_CLR + i * 8;
			/* Enable the interrupt */
		} else
			panic("sbus_setup_intr: IRQ not found!");
	}

	scl->scl_sc = sc;
	scl->scl_arg = arg;
	scl->scl_handler = intr;
	scl->scl_clr = intrclrptr;
	/* Disable the interrupt while we fiddle with it */
	SYSIO_WRITE8(sc, intrmapptr, intrmap);
	error = BUS_SETUP_INTR(device_get_parent(dev), child, ires, flags,
	    sbus_intr_stub, scl, cookiep);
	if (error != 0) {
		free(scl, M_DEVBUF);
		return (error);
	}
	scl->scl_cookie = *cookiep;
	*cookiep = scl;

	/*
	 * Clear the interrupt, it might have been triggered before it was
	 * set up.
	 */
	SYSIO_WRITE8(sc, intrclrptr, 0);
	/*
	 * Enable the interrupt and program the target module now we have the
	 * handler installed.
	 */
	SYSIO_WRITE8(sc, intrmapptr, INTMAP_ENABLE(intrmap, PCPU_GET(mid)));
	return (error);
}

static int
sbus_teardown_intr(device_t dev, device_t child,
    struct resource *vec, void *cookie)
{
	struct sbus_clr *scl;
	int error;

	scl = (struct sbus_clr *)cookie;
	error = BUS_TEARDOWN_INTR(device_get_parent(dev), child, vec,
	    scl->scl_cookie);
	/*
	 * Don't disable the interrupt for now, so that stray interupts get
	 * detected...
	 */
	if (error != 0)
		free(scl, M_DEVBUF);
	return (error);
}

/*
 * There is no need to handle pass-throughs here; there are no bridges to
 * SBuses.
 */
static struct resource *
sbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct sbus_softc *sc;
	struct sbus_devinfo *sdi;
	struct rman *rm;
	struct resource *rv;
	struct resource_list *rl;
	struct resource_list_entry *rle;
	bus_space_handle_t bh;
	bus_addr_t toffs;
	bus_size_t tend;
	int i;
	int isdefault = (start == 0UL && end == ~0UL);
	int needactivate = flags & RF_ACTIVE;

	sc = (struct sbus_softc *)device_get_softc(bus);
	sdi = device_get_ivars(child);
	rl = &sdi->sdi_rl;
	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		return (NULL);
	if (rle->res != NULL)
		panic("sbus_alloc_resource: resource entry is busy");
	if (isdefault) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
	}
	switch (type) {
	case SYS_RES_IRQ:
		rv = BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type,
		    rid, start, end, count, flags);
		if (rv == NULL)
			return (NULL);
		break;
	case SYS_RES_MEMORY:
		rm = NULL;
		bh = toffs = tend = 0;
		for (i = 0; i < sc->sc_nrange; i++) {
			if (sc->sc_rd[i].rd_slot != sdi->sdi_slot ||
			    start < sc->sc_rd[i].rd_coffset ||
			    start > sc->sc_rd[i].rd_cend)
				continue;
			/* Disallow cross-range allocations. */
			if (end > sc->sc_rd[i].rd_cend)
				return (NULL);
			/* We've found the connection to the parent bus */
			toffs = start - sc->sc_rd[i].rd_coffset;
			tend = end - sc->sc_rd[i].rd_coffset;
			rm = &sc->sc_rd[i].rd_rman;
			bh = sc->sc_rd[i].rd_bushandle;
		}
		if (toffs == NULL)
			return (NULL);
		flags &= ~RF_ACTIVE;
		rv = rman_reserve_resource(rm, toffs, tend, count, flags,
		    child);
		if (rv == NULL)
			return (NULL);
		rman_set_bustag(rv, sc->sc_cbustag);
		rman_set_bushandle(rv, bh + rman_get_start(rv));
		if (needactivate) {
			if (bus_activate_resource(child, type, *rid, rv)) {
				rman_release_resource(rv);
				return (NULL);
			}
		}
		break;
	default:
		return (NULL);
	}
	rle->res = rv;
	return (rv);
}

static int
sbus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ) {
		return (BUS_ACTIVATE_RESOURCE(device_get_parent(bus),
		    child, type, rid, r));
	}
	return (rman_activate_resource(r));
}

static int
sbus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	if (type == SYS_RES_IRQ) {
		return (BUS_DEACTIVATE_RESOURCE(device_get_parent(bus),
		    child, type, rid, r));
	}
	return (rman_deactivate_resource(r));
}

static int
sbus_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{
	struct sbus_devinfo *sdi;
	struct resource_list_entry *rle;
	int error = 0;

	if (type == SYS_RES_IRQ)
		error = BUS_RELEASE_RESOURCE(device_get_parent(bus), child,
		    type, rid, r);
	else {
		if ((rman_get_flags(r) & RF_ACTIVE) != 0)
			error = bus_deactivate_resource(child, type, rid, r);
		if (error != 0)
			return (error);
		error = rman_release_resource(r);
	}
	if (error != 0)
		return (error);
	sdi = device_get_ivars(child);
	rle = resource_list_find(&sdi->sdi_rl, type, rid);
	if (rle == NULL)
		panic("sbus_release_resource: can't find resource");
	if (rle->res == NULL)
		panic("sbus_release_resource: resource entry is not busy");
	rle->res = NULL;
	return (0);
}

/*
 * Handle an overtemp situation.
 *
 * SPARCs have temperature sensors which generate interrupts
 * if the machine's temperature exceeds a certain threshold.
 * This handles the interrupt and powers off the machine.
 * The same needs to be done to PCI controller drivers.
 */
static void
sbus_overtemp(void *arg)
{

	printf("DANGER: OVER TEMPERATURE detected\nShutting down NOW.\n");
	shutdown_nice(RB_POWEROFF);
}

/* Try to shut down in time in case of power failure. */
static void
sbus_pwrfail(void *arg)
{

	printf("Power failure detected\nShutting down NOW.\n");
	shutdown_nice(0);
}

static bus_space_tag_t
sbus_alloc_bustag(struct sbus_softc *sc)
{
	bus_space_tag_t sbt;

	sbt = (bus_space_tag_t)malloc(sizeof(struct bus_space_tag), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (sbt == NULL)
		panic("sbus_alloc_bustag: out of memory");

	bzero(sbt, sizeof *sbt);
	sbt->bst_cookie = sc;
	sbt->bst_parent = sc->sc_bustag;
	sbt->bst_type = SBUS_BUS_SPACE;
	return (sbt);
}
