/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
 * Copyright © 2021 Bjoern A. Zeeb
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
/*
 * The DPAA2 Management Complex (MC) Bus Driver (ACPI-based).
 *
 * MC is a hardware resource manager which can be found in several NXP
 * SoCs (LX2160A, for example) and provides an access to the specialized
 * hardware objects used in network-oriented packet processing applications.
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>

#include "acpi_bus_if.h"
#include "pcib_if.h"
#include "pci_if.h"

#include "dpaa2_mcp.h"
#include "dpaa2_mc.h"
#include "dpaa2_mc_if.h"

#define	_COMPONENT	ACPI_BUS
ACPI_MODULE_NAME("DPAA2_MC")

struct dpaa2_mac_dev_softc {
	int			uid;
	uint64_t		reg;
	char			managed[64];
	char			phy_conn_type[64];
	char			phy_mode[64];
	ACPI_HANDLE		phy_channel;
};

static int
dpaa2_mac_dev_probe(device_t dev)
{
	uint64_t reg;
	ssize_t s;

	s = device_get_property(dev, "reg", &reg, sizeof(reg),
	    DEVICE_PROP_UINT64);
	if (s == -1)
		return (ENXIO);

	device_set_desc(dev, "DPAA2 MAC DEV");
	return (BUS_PROBE_DEFAULT);
}

static int
dpaa2_mac_dev_attach(device_t dev)
{
	struct dpaa2_mac_dev_softc *sc;
	ACPI_HANDLE h;
	ssize_t s;

	sc = device_get_softc(dev);
	h = acpi_get_handle(dev);
	if (h == NULL)
		return (ENXIO);

	s = acpi_GetInteger(h, "_UID", &sc->uid);
	if (ACPI_FAILURE(s)) {
		device_printf(dev, "Cannot find '_UID' property: %zd\n", s);
		return (ENXIO);
	}

	s = device_get_property(dev, "reg", &sc->reg, sizeof(sc->reg),
	    DEVICE_PROP_UINT64);
	if (s == -1) {
		device_printf(dev, "Cannot find 'reg' property: %zd\n", s);
		return (ENXIO);
	}

	s = device_get_property(dev, "managed", sc->managed,
	    sizeof(sc->managed), DEVICE_PROP_ANY);
	s = device_get_property(dev, "phy-connection-type", sc->phy_conn_type,
	    sizeof(sc->phy_conn_type), DEVICE_PROP_ANY);
	s = device_get_property(dev, "phy-mode", sc->phy_mode,
	    sizeof(sc->phy_mode), DEVICE_PROP_ANY);
	s = device_get_property(dev, "phy-handle", &sc->phy_channel,
	    sizeof(sc->phy_channel), DEVICE_PROP_HANDLE);

	if (bootverbose)
		device_printf(dev, "UID %#04x reg %#04jx managed '%s' "
		    "phy-connection-type '%s' phy-mode '%s' phy-handle '%s'\n",
		    sc->uid, sc->reg, sc->managed[0] != '\0' ? sc->managed : "",
		    sc->phy_conn_type[0] != '\0' ? sc->phy_conn_type : "",
		    sc->phy_mode[0] != '\0' ? sc->phy_mode : "",
		    sc->phy_channel != NULL ? acpi_name(sc->phy_channel) : "");

	return (0);
}

static bool
dpaa2_mac_dev_match_id(device_t dev, uint32_t id)
{
	struct dpaa2_mac_dev_softc *sc;

	if (dev == NULL)
		return (false);

	sc = device_get_softc(dev);
	if (sc->uid == id)
		return (true);

	return (false);
}

static device_t
dpaa2_mac_dev_get_phy_dev(device_t dev)
{
	struct dpaa2_mac_dev_softc *sc;

	if (dev == NULL)
		return (NULL);

	sc = device_get_softc(dev);
	if (sc->phy_channel == NULL)
		return (NULL);

	return (acpi_get_device(sc->phy_channel));
}

static device_method_t dpaa2_mac_dev_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mac_dev_probe),
	DEVMETHOD(device_attach,	dpaa2_mac_dev_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(dpaa2_mac_dev, dpaa2_mac_dev_driver, dpaa2_mac_dev_methods,
    sizeof(struct dpaa2_mac_dev_softc));

DRIVER_MODULE(dpaa2_mac_dev, dpaa2_mc, dpaa2_mac_dev_driver, 0, 0);

MODULE_DEPEND(dpaa2_mac_dev, memac_mdio_acpi, 1, 1, 1);

/*
 * Device interface.
 */

static int
dpaa2_mc_acpi_probe(device_t dev)
{
	static char *dpaa2_mc_ids[] = { "NXP0008", NULL };
	int rc;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	rc = ACPI_ID_PROBE(device_get_parent(dev), dev, dpaa2_mc_ids, NULL);
	if (rc <= 0)
		device_set_desc(dev, "DPAA2 Management Complex");

	return (rc);
}

/* Context for walking PRxx child devices. */
struct dpaa2_mc_acpi_prxx_walk_ctx {
	device_t	dev;
	int		count;
	int		countok;
};

static ACPI_STATUS
dpaa2_mc_acpi_probe_child(ACPI_HANDLE h, device_t *dev, int level, void *arg)
{
	struct dpaa2_mc_acpi_prxx_walk_ctx *ctx;
	struct acpi_device *ad;
	device_t child;
	uint32_t uid;

	ctx = (struct dpaa2_mc_acpi_prxx_walk_ctx *)arg;
	ctx->count++;

#if 0
	device_printf(ctx->dev, "%s: %s level %d count %d\n", __func__,
	    acpi_name(h), level, ctx->count);
#endif

	if (ACPI_FAILURE(acpi_GetInteger(h, "_UID", &uid)))
		return (AE_OK);
#if 0
	if (bootverbose)
		device_printf(ctx->dev, "%s: Found child Ports _UID %u\n",
		    __func__, uid);
#endif

	/* Technically M_ACPIDEV */
	if ((ad = malloc(sizeof(*ad), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL)
		return (AE_OK);

	child = device_add_child(ctx->dev, "dpaa2_mac_dev", DEVICE_UNIT_ANY);
	if (child == NULL) {
		free(ad, M_DEVBUF);
		return (AE_OK);
	}
	ad->ad_handle = h;
	ad->ad_cls_class = 0xffffff;
	resource_list_init(&ad->ad_rl);
	device_set_ivars(child, ad);
	*dev = child;

	ctx->countok++;
	return (AE_OK);
}

static int
dpaa2_mc_acpi_attach(device_t dev)
{
	struct dpaa2_mc_softc *sc;

	sc = device_get_softc(dev);
	sc->acpi_based = true;

	struct dpaa2_mc_acpi_prxx_walk_ctx ctx;
	ctx.dev = dev;
	ctx.count = 0;
	ctx.countok = 0;
	ACPI_SCAN_CHILDREN(device_get_parent(dev), dev, 2,
	    dpaa2_mc_acpi_probe_child, &ctx);

#if 0
	device_printf(dev, "Found %d child Ports in ASL, %d ok\n",
	    ctx.count, ctx.countok);
#endif

	return (dpaa2_mc_attach(dev));
}

/*
 * ACPI compat layer.
 */

static device_t
dpaa2_mc_acpi_find_dpaa2_mac_dev(device_t dev, uint32_t id)
{
	int devcount, error, i, len;
	device_t *devlist, mdev;
	const char *mdevname;

	error = device_get_children(dev, &devlist, &devcount);
	if (error != 0)
		return (NULL);

	for (i = 0; i < devcount; i++) {
		mdev = devlist[i];
		mdevname = device_get_name(mdev);
		if (mdevname != NULL) {
			len = strlen(mdevname);
			if (strncmp("dpaa2_mac_dev", mdevname, len) != 0)
				continue;
		} else {
			continue;
		}
		if (!device_is_attached(mdev))
			continue;

		if (dpaa2_mac_dev_match_id(mdev, id))
			return (mdev);
	}

	return (NULL);
}

static int
dpaa2_mc_acpi_get_phy_dev(device_t dev, device_t *phy_dev, uint32_t id)
{
	device_t mdev, pdev;

	mdev = dpaa2_mc_acpi_find_dpaa2_mac_dev(dev, id);
	if (mdev == NULL) {
		device_printf(dev, "%s: error finding dpmac device with id=%u\n",
		    __func__, id);
		return (ENXIO);
	}

	pdev = dpaa2_mac_dev_get_phy_dev(mdev);
	if (pdev == NULL) {
		device_printf(dev, "%s: error getting MDIO device for dpamc %s "
		    "(id=%u)\n", __func__, device_get_nameunit(mdev), id);
		return (ENXIO);
	}

	if (phy_dev != NULL)
		*phy_dev = pdev;

	return (0);
}

static ssize_t
dpaa2_mc_acpi_get_property(device_t dev, device_t child, const char *propname,
    void *propvalue, size_t size, device_property_type_t type)
{
	return (bus_generic_get_property(dev, child, propname, propvalue, size,
	    type));
}

static int
dpaa2_mc_acpi_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	/*
	 * This is special in that it passes "child" as second argument rather
	 * than "dev".  acpi_get_handle() in dpaa2_mac_dev_attach() calls the
	 * read on parent(dev), dev and gets us here not to ACPI.  Hence we
	 * need to keep child as-is and pass it to our parent which is ACPI.
	 * Only that gives the desired result.
	 */
	return (BUS_READ_IVAR(device_get_parent(dev), child, index, result));
}

static device_method_t dpaa2_mc_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa2_mc_acpi_probe),
	DEVMETHOD(device_attach,	dpaa2_mc_acpi_attach),
	DEVMETHOD(device_detach,	dpaa2_mc_detach),

	/* Bus interface */
	DEVMETHOD(bus_get_rman,		dpaa2_mc_rman),
	DEVMETHOD(bus_alloc_resource,	dpaa2_mc_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	dpaa2_mc_adjust_resource),
	DEVMETHOD(bus_release_resource,	dpaa2_mc_release_resource),
	DEVMETHOD(bus_activate_resource, dpaa2_mc_activate_resource),
	DEVMETHOD(bus_deactivate_resource, dpaa2_mc_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* Pseudo-PCIB interface */
	DEVMETHOD(pcib_alloc_msi,	dpaa2_mc_alloc_msi),
	DEVMETHOD(pcib_release_msi,	dpaa2_mc_release_msi),
	DEVMETHOD(pcib_map_msi,		dpaa2_mc_map_msi),
	DEVMETHOD(pcib_get_id,		dpaa2_mc_get_id),

	/* DPAA2 MC bus interface */
	DEVMETHOD(dpaa2_mc_manage_dev,	dpaa2_mc_manage_dev),
	DEVMETHOD(dpaa2_mc_get_free_dev,dpaa2_mc_get_free_dev),
	DEVMETHOD(dpaa2_mc_get_dev,	dpaa2_mc_get_dev),
	DEVMETHOD(dpaa2_mc_get_shared_dev, dpaa2_mc_get_shared_dev),
	DEVMETHOD(dpaa2_mc_reserve_dev,	dpaa2_mc_reserve_dev),
	DEVMETHOD(dpaa2_mc_release_dev, dpaa2_mc_release_dev),
	DEVMETHOD(dpaa2_mc_get_phy_dev,	dpaa2_mc_acpi_get_phy_dev),

	/* ACPI compar layer. */
	DEVMETHOD(bus_read_ivar,	dpaa2_mc_acpi_read_ivar),
	DEVMETHOD(bus_get_property,	dpaa2_mc_acpi_get_property),

	DEVMETHOD_END
};

DEFINE_CLASS_1(dpaa2_mc, dpaa2_mc_acpi_driver, dpaa2_mc_acpi_methods,
    sizeof(struct dpaa2_mc_softc), dpaa2_mc_driver);

/* Make sure miibus gets procesed first. */
DRIVER_MODULE_ORDERED(dpaa2_mc, acpi, dpaa2_mc_acpi_driver, NULL, NULL,
    SI_ORDER_ANY);
MODULE_DEPEND(dpaa2_mc, memac_mdio_acpi, 1, 1, 1);
