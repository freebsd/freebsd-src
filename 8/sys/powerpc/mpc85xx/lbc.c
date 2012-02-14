/*-
 * Copyright (c) 2006-2008, Juniper Networks, Inc.
 * Copyright (c) 2008 Semihalf, Rafal Czubak
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/ocpbus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <powerpc/mpc85xx/lbc.h>
#include <powerpc/mpc85xx/mpc85xx.h>
#include <powerpc/mpc85xx/ocpbus.h>

struct lbc_softc {
	device_t		sc_dev;

	struct resource		*sc_res;
	bus_space_handle_t	sc_bsh;
	bus_space_tag_t		sc_bst;
	int			sc_rid;

	struct rman		sc_rman;
	vm_offset_t		sc_kva[LBC_DEV_MAX];
};

struct lbc_devinfo {
	int		lbc_devtype;
	/* LBC child unit. It also represents resource table entry number */
	int		lbc_unit;
};

/* Resources for MPC8555CDS system */
const struct lbc_resource mpc85xx_lbc_resources[] = {
	/* Boot flash bank */
	{
		LBC_DEVTYPE_CFI, 0, 0xff800000, 0x00800000, 16,
		LBCRES_MSEL_GPCM, LBCRES_DECC_DISABLED,
		LBCRES_ATOM_DISABLED, 0
	},

	/* Second flash bank */
	{
		LBC_DEVTYPE_CFI, 1, 0xff000000, 0x00800000, 16,
		LBCRES_MSEL_GPCM, LBCRES_DECC_DISABLED,
		LBCRES_ATOM_DISABLED, 0
	},

	/* DS1553 RTC/NVRAM */
	{
		LBC_DEVTYPE_RTC, 2, 0xf8000000, 0x8000, 8,
		LBCRES_MSEL_GPCM, LBCRES_DECC_DISABLED,
		LBCRES_ATOM_DISABLED, 0
	},

	{0}
};

static int lbc_probe(device_t);
static int lbc_attach(device_t);
static int lbc_shutdown(device_t);
static int lbc_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);
static struct resource *lbc_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int lbc_print_child(device_t, device_t);
static int lbc_release_resource(device_t, device_t, int, int,
    struct resource *);
static int lbc_read_ivar(device_t, device_t, int, uintptr_t *);

/*
 * Bus interface definition
 */
static device_method_t lbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lbc_probe),
	DEVMETHOD(device_attach,	lbc_attach),
	DEVMETHOD(device_shutdown,	lbc_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	lbc_print_child),
	DEVMETHOD(bus_read_ivar,	lbc_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	NULL),

	DEVMETHOD(bus_get_resource,	NULL),
	DEVMETHOD(bus_alloc_resource,	lbc_alloc_resource),
	DEVMETHOD(bus_release_resource,	lbc_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

	{ 0, 0 }
};

static driver_t lbc_driver = {
	"lbc",
	lbc_methods,
	sizeof(struct lbc_softc)
};
devclass_t lbc_devclass;
DRIVER_MODULE(lbc, ocpbus, lbc_driver, lbc_devclass, 0, 0);

static __inline void
lbc_write_reg(struct lbc_softc *sc, bus_size_t off, uint32_t val)
{

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, off, val);
}

static __inline uint32_t
lbc_read_reg(struct lbc_softc *sc, bus_size_t off)
{

	return (bus_space_read_4(sc->sc_bst, sc->sc_bsh, off));
}

/*
 * Calculate address mask used by OR(n) registers. Use memory region size to
 * determine mask value. The size must be a power of two and within the range
 * of 32KB - 4GB. Otherwise error code is returned. Value representing
 * 4GB size can be passed as 0xffffffff.
 */
static uint32_t
lbc_address_mask(uint32_t size)
{
	int n = 15;

	if (size == ~0UL)
		return (0);

	while (n < 32) {
		if (size == (1UL << n))
			break;
		n++;
	}

	if (n == 32)
		return (EINVAL);

	return (0xffff8000 << (n - 15));
}

static device_t
lbc_mk_child(device_t dev, const struct lbc_resource *lbcres)
{
	struct lbc_devinfo *dinfo;
	device_t child;

	if (lbcres->lbr_unit > LBC_DEV_MAX - 1)
		return (NULL);

	child = device_add_child(dev, NULL, -1);
	if (child == NULL) {
		device_printf(dev, "could not add LBC child device\n");
		return (NULL);
	}
	dinfo = malloc(sizeof(struct lbc_devinfo), M_DEVBUF, M_WAITOK | M_ZERO);
	dinfo->lbc_devtype = lbcres->lbr_devtype;
	dinfo->lbc_unit = lbcres->lbr_unit;
	device_set_ivars(child, dinfo);
	return (child);
}

static int
lbc_init_child(device_t dev, device_t child)
{
	struct lbc_softc *sc;
	struct lbc_devinfo *dinfo;
	const struct lbc_resource *res;
	u_long start, size;
	uint32_t regbuff;
	int error, unit;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	res = mpc85xx_lbc_resources;

	regbuff = 0;
	unit = -1;
	for (; res->lbr_devtype; res++) {
		if (res->lbr_unit != dinfo->lbc_unit)
			continue;

		start = res->lbr_base_addr;
		size = res->lbr_size;
		unit = res->lbr_unit;

		/*
		 * Configure LAW for this LBC device and map its physical
		 * memory region into KVA
		 */
		error = law_enable(OCP85XX_TGTIF_LBC, start, size);
		if (error)
			return (error);

		sc->sc_kva[unit] = (vm_offset_t)pmap_mapdev(start, size);
		if (sc->sc_kva[unit] == 0) {
			law_disable(OCP85XX_TGTIF_LBC, start, size);
			return (ENOSPC);
		}

		/*
		 * Compute and program BR value
		 */
		regbuff |= start;

		switch (res->lbr_port_size) {
		case 8:
			regbuff |= (1 << 11);
			break;
		case 16:
			regbuff |= (2 << 11);
			break;
		case 32:
			regbuff |= (3 << 11);
			break;
		default:
			error = EINVAL;
			goto fail;
		}
		regbuff |= (res->lbr_decc << 9);
		regbuff |= (res->lbr_wp << 8);
		regbuff |= (res->lbr_msel << 5);
		regbuff |= (res->lbr_atom << 2);
		regbuff |= 1;

		lbc_write_reg(sc, LBC85XX_BR(unit), regbuff);

		/*
		 * Compute and program OR value
		 */
		regbuff = 0;
		regbuff |= lbc_address_mask(size);

		switch (res->lbr_msel) {
		case LBCRES_MSEL_GPCM:
			/* TODO Add flag support for option registers */
			regbuff |= 0x00000ff7;
			break;
		case LBCRES_MSEL_FCM:
			printf("FCM mode not supported yet!");
			error = ENOSYS;
			goto fail;
		case LBCRES_MSEL_UPMA:
		case LBCRES_MSEL_UPMB:
		case LBCRES_MSEL_UPMC:
			printf("UPM mode not supported yet!");
			error = ENOSYS;
			goto fail;
		}

		lbc_write_reg(sc, LBC85XX_OR(unit), regbuff);

		return (0);
	}
fail:
	if (unit != -1) {
		law_disable(OCP85XX_TGTIF_LBC, start, size);
		pmap_unmapdev(sc->sc_kva[unit], size);
		return (error);
	} else
		return (ENOENT);
}

static int
lbc_probe(device_t dev)
{
	device_t parent;
	uintptr_t devtype;
	int error;

	parent = device_get_parent(dev);
	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_LBC)
		return (ENXIO);

	device_set_desc(dev, "Freescale MPC85xx Local Bus Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lbc_attach(device_t dev)
{
	struct lbc_softc *sc;
	struct rman *rm;
	const struct lbc_resource *lbcres;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL)
		return (ENXIO);

	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	rm = &sc->sc_rman;
	rm->rm_type = RMAN_ARRAY;
	rm->rm_descr = "MPC85XX Local Bus Space";
	rm->rm_start = 0UL;
	rm->rm_end = ~0UL;
	error = rman_init(rm);
	if (error)
		goto fail;

	error = rman_manage_region(rm, rm->rm_start, rm->rm_end);
	if (error) {
		rman_fini(rm);
		goto fail;
	}

	/*
	 * Initialize configuration register:
	 * - enable Local Bus
	 * - set data buffer control signal function
	 * - disable parity byte select
	 * - set ECC parity type
	 * - set bus monitor timing and timer prescale
	 */
	lbc_write_reg(sc, LBC85XX_LBCR, 0x00000000);

	/*
	 * Initialize clock ratio register:
	 * - disable PLL bypass mode
	 * - configure LCLK delay cycles for the assertion of LALE
	 * - set system clock divider
	 */
	lbc_write_reg(sc, LBC85XX_LCRR, 0x00030008);

	lbcres = mpc85xx_lbc_resources;

	for (; lbcres->lbr_devtype; lbcres++)
		if (!lbc_mk_child(dev, lbcres)) {
			error = ENXIO;
			goto fail;
		}

	return (bus_generic_attach(dev));

fail:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rid, sc->sc_res);
	return (error);
}

static int
lbc_shutdown(device_t dev)
{

	/* TODO */
	return(0);
}

static struct resource *
lbc_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct lbc_softc *sc;
	struct lbc_devinfo *dinfo;
	struct resource *rv;
	struct rman *rm;
	int error;

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if (type != SYS_RES_MEMORY && type != SYS_RES_IRQ)
		return (NULL);

	/* We only support default allocations. */
	if (start != 0ul || end != ~0ul)
		return (NULL);

	if (type == SYS_RES_IRQ)
		return (bus_alloc_resource(dev, type, rid, start, end, count,
		    flags));

	if (!sc->sc_kva[dinfo->lbc_unit]) {
		error = lbc_init_child(dev, child);
		if (error)
			return (NULL);
	}

	error = lbc_get_resource(dev, child, type, *rid, &start, &count);
	if (error)
		return (NULL);

	rm = &sc->sc_rman;
	end = start + count - 1;
	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv != NULL) {
		rman_set_bustag(rv, &bs_be_tag);
		rman_set_bushandle(rv, rman_get_start(rv));
	}
	return (rv);
}

static int
lbc_print_child(device_t dev, device_t child)
{
	u_long size, start;
	int error, retval, rid;

	retval = bus_print_child_header(dev, child);

	rid = 0;
	while (1) {
		error = lbc_get_resource(dev, child, SYS_RES_MEMORY, rid,
		    &start, &size);
		if (error)
			break;
		retval += (rid == 0) ? printf(" iomem ") : printf(",");
		retval += printf("%#lx", start);
		if (size > 1)
			retval += printf("-%#lx", start + size - 1);
		rid++;
	}

	retval += bus_print_child_footer(dev, child);
	return (retval);
}

static int
lbc_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct lbc_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	dinfo = device_get_ivars(child);

	switch (index) {
	case LBC_IVAR_DEVTYPE:
		*result = dinfo->lbc_devtype;
		return (0);
	default:
		break;
	}
	return (EINVAL);
}

static int
lbc_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	return (rman_release_resource(res));
}

static int
lbc_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct lbc_softc *sc;
	struct lbc_devinfo *dinfo;
	const struct lbc_resource *lbcres;

	if (type != SYS_RES_MEMORY)
		return (ENOENT);

	/* Currently all LBC devices have a single RID per type. */
	if (rid != 0)
		return (ENOENT);

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	if ((dinfo->lbc_unit < 0) || (dinfo->lbc_unit > (LBC_DEV_MAX - 1)))
		return (EINVAL);

	lbcres = mpc85xx_lbc_resources;

	switch (dinfo->lbc_devtype) {
	case LBC_DEVTYPE_CFI:
	case LBC_DEVTYPE_RTC:
		for (; lbcres->lbr_devtype; lbcres++) {
			if (dinfo->lbc_unit == lbcres->lbr_unit) {
				*startp = sc->sc_kva[lbcres->lbr_unit];
				*countp = lbcres->lbr_size;
				return (0);
			}
		}
	default:
		return (EDOOFUS);
	}
	return (0);
}
