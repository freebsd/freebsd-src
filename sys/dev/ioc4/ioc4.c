/*-
 * Copyright (c) 2012 Marcel Moolenaar
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
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ioc4/ioc4_bus.h>
#include <dev/ioc4/ioc4_reg.h>

struct ioc4_softc;

struct ioc4_child {
	struct ioc4_softc *ch_softc;
	device_t	ch_dev;
	struct resource *ch_ires;
	struct resource *ch_mres;
	u_int		ch_type;
	u_int		ch_unit;
	u_int		ch_imask;
};

struct ioc4_softc {
	device_t	sc_dev;

	struct resource	*sc_mres;
	int		sc_mrid;

	int		sc_irid;
	struct resource	*sc_ires;
	void		*sc_icookie;

	struct rman	sc_rm;
};

static int ioc4_probe(device_t dev);
static int ioc4_attach(device_t dev);
static int ioc4_detach(device_t dev);

static int ioc4_bus_read_ivar(device_t, device_t, int, uintptr_t *);
static struct resource *ioc4_bus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int ioc4_bus_release_resource(device_t, device_t, int, int,
    struct resource *);
static int ioc4_bus_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);

static char ioc4_name[] = "ioc4";
static devclass_t ioc4_devclass;

static device_method_t ioc4_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ioc4_probe),
	DEVMETHOD(device_attach,	ioc4_attach),
	DEVMETHOD(device_detach,	ioc4_detach),

	DEVMETHOD(bus_alloc_resource,	ioc4_bus_alloc_resource),
	DEVMETHOD(bus_get_resource,	ioc4_bus_get_resource),
	DEVMETHOD(bus_read_ivar,	ioc4_bus_read_ivar),
	DEVMETHOD(bus_release_resource,	ioc4_bus_release_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	DEVMETHOD_END
};

static driver_t ioc4_driver = {
	ioc4_name,
	ioc4_methods,
	sizeof(struct ioc4_softc),
};

static int
ioc4_filt(void *arg)
{
	struct ioc4_softc *sc = arg;
	uint32_t mask;

	mask = bus_read_4(sc->sc_mres, IOC4_CTL_MISC_INT);
	bus_write_4(sc->sc_mres, IOC4_CTL_MISC_INT, mask & ~0x03);
	return ((mask & 0x03) ? FILTER_SCHEDULE_THREAD: FILTER_STRAY);
}

static void
ioc4_intr(void *arg)
{
	struct ioc4_softc *sc = arg;

	bus_write_4(sc->sc_mres, IOC4_CTL_MISC_INT, 0x03);
}

static int
ioc4_child_add(struct ioc4_softc *sc, u_int type, u_int unit, u_int imask)
{
	struct ioc4_child *ch;
	bus_space_handle_t bsh;
	bus_space_tag_t bst;
	u_long base, ofs, len;
	int error;

	ch = malloc(sizeof(*ch), M_DEVBUF, M_WAITOK | M_ZERO);
	ch->ch_softc = sc;
	ch->ch_type = type;
	ch->ch_unit = unit;

	error = ENXIO;

	ch->ch_dev = device_add_child(sc->sc_dev, NULL, -1);
	if (ch->ch_dev == NULL)
		goto fail_free;

	error = ENOMEM;

	base = rman_get_start(sc->sc_mres);

	switch (type) {
	case IOC4_TYPE_UART:
		ofs = IOC4_UART_REG(unit);
		len = IOC4_UART_REG_SIZE;
		break;
	case IOC4_TYPE_ATA:
		bus_write_4(sc->sc_mres, IOC4_CTL_MISC_INT_SET, imask);
		ofs = IOC4_ATA_BASE;
		len = IOC4_ATA_SIZE;
		break;
	default:
		ofs = len = 0;
		break;
	}
	if (len == 0 || ofs == 0)
		goto fail_free;

	ch->ch_mres = rman_reserve_resource(&sc->sc_rm, base + ofs,
	    base + ofs + len - 1, len, 0, NULL);
	if (ch->ch_mres == NULL)
		goto fail_delete;

	ch->ch_ires = sc->sc_ires;
	ch->ch_imask = imask;

	bsh = rman_get_bushandle(sc->sc_mres);
	bst = rman_get_bustag(sc->sc_mres);
	bus_space_subregion(bst, bsh, ofs, len, &bsh);
	rman_set_bushandle(ch->ch_mres, bsh);
	rman_set_bustag(ch->ch_mres, bst);

	device_set_ivars(ch->ch_dev, (void *)ch);

	error = device_probe_and_attach(ch->ch_dev);
	if (error)
		goto fail_release;

	return (0);

 fail_release:
	rman_release_resource(ch->ch_mres);

 fail_delete:
	device_delete_child(sc->sc_dev, ch->ch_dev);

 fail_free:
	free(ch, M_DEVBUF);
	device_printf(sc->sc_dev, "%s: error=%d\n", __func__, error);
	return (error);
}

static int
ioc4_probe(device_t dev)
{

	if (pci_get_vendor(dev) != 0x10a9)
		return (ENXIO);
	if (pci_get_device(dev) != 0x100a)
		return (ENXIO);

	device_set_desc(dev, "SGI IOC4 I/O controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ioc4_attach(device_t dev)
{
	char descr[RM_TEXTLEN];
	struct ioc4_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	error = ENXIO;

	sc->sc_mrid = PCIR_BAR(0);
	sc->sc_mres = bus_alloc_resource_any(sc->sc_dev, SYS_RES_MEMORY,
	    &sc->sc_mrid, RF_ACTIVE);
	if (sc->sc_mres == NULL)
		goto fail_return;

	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &sc->sc_irid, RF_ACTIVE|RF_SHAREABLE);
	if (sc->sc_ires == NULL)
		goto fail_rel_mres;

	error = bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_MISC | INTR_MPSAFE,
	    ioc4_filt, ioc4_intr, sc, &sc->sc_icookie);
	if (error)
		goto fail_rel_ires;

	sc->sc_rm.rm_type = RMAN_ARRAY;
	error = rman_init(&sc->sc_rm);
	if (error)
		goto fail_teardown;
	error = rman_manage_region(&sc->sc_rm, rman_get_start(sc->sc_mres),
	    rman_get_end(sc->sc_mres));
	if (error)
		goto fail_teardown;

	snprintf(descr, sizeof(descr), "%s I/O memory",
	    device_get_nameunit(sc->sc_dev));
	sc->sc_rm.rm_descr = strdup(descr, M_DEVBUF);

	pci_enable_busmaster(sc->sc_dev);

	bus_write_4(sc->sc_mres, IOC4_CTL_CR, 0xf);
	bus_write_4(sc->sc_mres, IOC4_CTL_GPIO_SET, 0xf0);

	bus_write_4(sc->sc_mres, IOC4_CTL_UART_INT_CLR, ~0U);
	bus_write_4(sc->sc_mres, IOC4_CTL_UART_INT, ~0U);

	bus_write_4(sc->sc_mres, IOC4_CTL_MISC_INT_CLR, ~0U);
	bus_write_4(sc->sc_mres, IOC4_CTL_MISC_INT, ~0U);

	/*
	 * Create, probe and attach children
	 */
#if 0
	for (n = 0; n < 4; n++)
		ioc4_child_add(sc, IOC4_TYPE_UART, n);
#endif
	ioc4_child_add(sc, IOC4_TYPE_ATA, 0, 0x03);

	return (0);

 fail_teardown:
	bus_teardown_intr(sc->sc_dev, sc->sc_ires, sc->sc_icookie);

 fail_rel_ires:
	bus_release_resource(sc->sc_dev, SYS_RES_IRQ, sc->sc_irid, sc->sc_ires);

 fail_rel_mres:
	bus_release_resource(sc->sc_dev, SYS_RES_MEMORY, sc->sc_mrid,
	    sc->sc_mres);

 fail_return:
	return (error);
}

static int
ioc4_detach(device_t dev)
{
	struct ioc4_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Detach children and destroy children
	 */

	free(__DECONST(void *, sc->sc_rm.rm_descr), M_DEVBUF);
	rman_fini(&sc->sc_rm);

	bus_teardown_intr(sc->sc_dev, sc->sc_ires, sc->sc_icookie);
	bus_release_resource(sc->sc_dev, SYS_RES_IRQ, sc->sc_irid,
	    sc->sc_ires);
	bus_release_resource(sc->sc_dev, SYS_RES_MEMORY, sc->sc_mrid,
	    sc->sc_mres);
	return (0);
}

static struct ioc4_child *
_ioc4_get_child(device_t dev, device_t child)
{
	struct ioc4_child *ch;

	/* Get our immediate child. */
	while (child != NULL && device_get_parent(child) != dev)
		child = device_get_parent(child);
	if (child == NULL)
		return (NULL);

	ch = device_get_ivars(child);
	KASSERT(ch != NULL, ("%s %d", __func__, __LINE__));
	return (ch);
}

static int
ioc4_bus_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct ioc4_child *ch;
	struct ioc4_softc *sc;
	uint32_t pci;

	if (result == NULL)
		return (EINVAL);

	ch = _ioc4_get_child(dev, child);
	if (ch == NULL)
		return (EINVAL);

	sc = ch->ch_softc;

	switch(index) {
	case IOC4_IVAR_CLOCK:
		pci = bus_read_4(sc->sc_mres, IOC4_CTL_PCI);
		*result = (pci & 1) ? 66666667 : 33333333;
		break;
	case IOC4_IVAR_TYPE:
		*result = ch->ch_type;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static struct resource *
ioc4_bus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ioc4_child *ch;
	struct resource *res;
	device_t assigned;
	int error;

	if (rid == NULL || *rid != 0)
		return (NULL);

	/* We only support default allocations. */
	if (start != 0UL || end != ~0UL)
		return (NULL);

	ch = _ioc4_get_child(dev, child);
	if (ch == NULL)
		return (NULL);

	if (type == SYS_RES_IRQ)
		return (ch->ch_ires);
	if (type != SYS_RES_MEMORY)
		return (NULL);

	res = ch->ch_mres;

	assigned = rman_get_device(res);
	if (assigned == NULL)   /* Not allocated */
		rman_set_device(res, child);
	else if (assigned != child)
		return (NULL);

	if (flags & RF_ACTIVE) {
		error = rman_activate_resource(res);
		if (error) {
			if (assigned == NULL)
				rman_set_device(res, NULL);
			res = NULL;
		}
	}

	return (res);
}

static int
ioc4_bus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	struct ioc4_child *ch;

	if (rid != 0 || res == NULL)
		return (EINVAL);

	ch = _ioc4_get_child(dev, child);
	if (ch == NULL)
		return (EINVAL);

	if (type == SYS_RES_MEMORY) {
		if (res != ch->ch_mres)
			return (EINVAL);
	} else if (type == SYS_RES_IRQ) {
		if (res != ch->ch_ires)
			return (EINVAL);
	} else
		return (EINVAL);

	if (rman_get_device(res) != child)
		return (ENXIO);
	if (rman_get_flags(res) & RF_ACTIVE)
		rman_deactivate_resource(res);
	rman_set_device(res, NULL);
	return (0);
}

static int
ioc4_bus_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct ioc4_child *ch;
	struct resource *res;
	u_long start;

	ch = _ioc4_get_child(dev, child);
	if (ch == NULL)
		return (EINVAL);

	if (type == SYS_RES_MEMORY)
		res = ch->ch_mres;
	else if (type == SYS_RES_IRQ)
		res = ch->ch_ires;
	else
		return (ENXIO);

	if (rid != 0 || res == NULL)
		return (ENXIO);

	start = rman_get_start(res);
	if (startp != NULL)
		*startp = start;
	if (countp != NULL)
		*countp = rman_get_end(res) - start + 1;
	return (0);
}

DRIVER_MODULE(ioc4, pci, ioc4_driver, ioc4_devclass, 0, 0);
