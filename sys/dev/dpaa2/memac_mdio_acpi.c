/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Bjoern A. Zeeb
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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/endian.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "memac_mdio.h"
#include "memac_mdio_if.h"
#include "acpi_bus_if.h"
#include "miibus_if.h"

/* -------------------------------------------------------------------------- */

struct memacphy_softc_acpi {
	struct memacphy_softc_common	scc;
	int				uid;
	uint64_t			phy_channel;
	char				compatible[64];
};

static void
memacphy_acpi_miibus_statchg(device_t dev)
{
	struct memacphy_softc_acpi *sc;

	sc = device_get_softc(dev);
	memacphy_miibus_statchg(&sc->scc);
}

static int
memacphy_acpi_set_ni_dev(device_t dev, device_t nidev)
{
	struct memacphy_softc_acpi *sc;

	sc = device_get_softc(dev);
	return (memacphy_set_ni_dev(&sc->scc, nidev));
}

static int
memacphy_acpi_get_phy_loc(device_t dev, int *phy_loc)
{
	struct memacphy_softc_acpi *sc;

	sc = device_get_softc(dev);
	return (memacphy_get_phy_loc(&sc->scc, phy_loc));
}

static int
memacphy_acpi_probe(device_t dev)
{

	device_set_desc(dev, "MEMAC PHY (acpi)");
	return (BUS_PROBE_DEFAULT);
}

static int
memacphy_acpi_attach(device_t dev)
{
	struct memacphy_softc_acpi *sc;
	ACPI_HANDLE h;
	ssize_t s;

	sc = device_get_softc(dev);
	sc->scc.dev = dev;
	h = acpi_get_handle(dev);

	s = acpi_GetInteger(h, "_UID", &sc->uid);
	if (ACPI_FAILURE(s)) {
		device_printf(dev, "Cannot get '_UID' property: %zd\n", s);
		return (ENXIO);
	}

	s = device_get_property(dev, "phy-channel",
	    &sc->phy_channel, sizeof(sc->phy_channel), DEVICE_PROP_UINT64);
	if (s != -1)
		sc->scc.phy = sc->phy_channel;
	else
		sc->scc.phy = -1;
	s = device_get_property(dev, "compatible",
	    sc->compatible, sizeof(sc->compatible), DEVICE_PROP_ANY);

	if (bootverbose)
		device_printf(dev, "UID %#04x phy-channel %ju compatible '%s' phy %u\n",
		    sc->uid, sc->phy_channel,
		    sc->compatible[0] != '\0' ? sc->compatible : "", sc->scc.phy);

	if (sc->scc.phy == -1)
		return (ENXIO);
	return (0);
}

static device_method_t memacphy_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		memacphy_acpi_probe),
	DEVMETHOD(device_attach,	memacphy_acpi_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	memacphy_miibus_readreg),
	DEVMETHOD(miibus_writereg,	memacphy_miibus_writereg),
	DEVMETHOD(miibus_statchg,	memacphy_acpi_miibus_statchg),

	/* memac */
	DEVMETHOD(memac_mdio_set_ni_dev, memacphy_acpi_set_ni_dev),
	DEVMETHOD(memac_mdio_get_phy_loc, memacphy_acpi_get_phy_loc),

	DEVMETHOD_END
};

DEFINE_CLASS_0(memacphy_acpi, memacphy_acpi_driver, memacphy_acpi_methods,
    sizeof(struct memacphy_softc_acpi));

EARLY_DRIVER_MODULE(memacphy_acpi, memac_mdio_acpi, memacphy_acpi_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
DRIVER_MODULE(miibus, memacphy_acpi, miibus_driver, 0, 0);
MODULE_DEPEND(memacphy_acpi, miibus, 1, 1, 1);

/* -------------------------------------------------------------------------- */

struct memac_mdio_softc_acpi {
	struct memac_mdio_softc_common	scc;
};

static int
memac_acpi_miibus_readreg(device_t dev, int phy, int reg)
{
	struct memac_mdio_softc_acpi *sc;

	sc = device_get_softc(dev);
	return (memac_miibus_readreg(&sc->scc, phy, reg));
}

static int
memac_acpi_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct memac_mdio_softc_acpi *sc;

	sc = device_get_softc(dev);
	return (memac_miibus_writereg(&sc->scc, phy, reg, data));
}

/* Context for walking PHY child devices. */
struct memac_mdio_walk_ctx {
	device_t	dev;
	int		count;
	int		countok;
};

static char *memac_mdio_ids[] = {
	"NXP0006",
	NULL
};

static int
memac_mdio_acpi_probe(device_t dev)
{
	int rc;

	if (acpi_disabled("fsl_memac_mdio"))
		return (ENXIO);

	rc = ACPI_ID_PROBE(device_get_parent(dev), dev, memac_mdio_ids, NULL);
	if (rc <= 0)
		device_set_desc(dev, "Freescale XGMAC MDIO Bus");

	return (rc);
}

static ACPI_STATUS
memac_mdio_acpi_probe_child(ACPI_HANDLE h, device_t *dev, int level, void *arg)
{
	struct memac_mdio_walk_ctx *ctx;
	struct acpi_device *ad;
	device_t child;
	uint32_t adr;

	ctx = (struct memac_mdio_walk_ctx *)arg;
	ctx->count++;

	if (ACPI_FAILURE(acpi_GetInteger(h, "_ADR", &adr)))
		return (AE_OK);

	/* Technically M_ACPIDEV */
	if ((ad = malloc(sizeof(*ad), M_DEVBUF, M_NOWAIT | M_ZERO)) == NULL)
		return (AE_OK);

	child = device_add_child(ctx->dev, "memacphy_acpi", DEVICE_UNIT_ANY);
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
memac_mdio_acpi_attach(device_t dev)
{
	struct memac_mdio_softc_acpi *sc;
	struct memac_mdio_walk_ctx ctx;
	int error;

	sc = device_get_softc(dev);
	sc->scc.dev = dev;

	error = memac_mdio_generic_attach(&sc->scc);
	if (error != 0)
		return (error);

	ctx.dev = dev;
	ctx.count = 0;
	ctx.countok = 0;
	ACPI_SCAN_CHILDREN(device_get_parent(dev), dev, 1,
	    memac_mdio_acpi_probe_child, &ctx);
	if (ctx.countok > 0) {
		bus_identify_children(dev);
		bus_attach_children(dev);
	}

	return (0);
}

static int
memac_mdio_acpi_detach(device_t dev)
{
	struct memac_mdio_softc_acpi *sc;

	sc = device_get_softc(dev);
	return (memac_mdio_generic_detach(&sc->scc));
}

static device_method_t memac_mdio_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		memac_mdio_acpi_probe),
	DEVMETHOD(device_attach,	memac_mdio_acpi_attach),
	DEVMETHOD(device_detach,	memac_mdio_acpi_detach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	memac_acpi_miibus_readreg),
	DEVMETHOD(miibus_writereg,	memac_acpi_miibus_writereg),

	/* .. */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_read_ivar,	memac_mdio_read_ivar),
	DEVMETHOD(bus_get_property,	memac_mdio_get_property),

	DEVMETHOD_END
};

DEFINE_CLASS_0(memac_mdio_acpi, memac_mdio_acpi_driver, memac_mdio_acpi_methods,
    sizeof(struct memac_mdio_softc_acpi));

EARLY_DRIVER_MODULE(memac_mdio_acpi, acpi, memac_mdio_acpi_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);

DRIVER_MODULE(miibus, memac_mdio_acpi, miibus_driver, 0, 0);
MODULE_DEPEND(memac_mdio_acpi, miibus, 1, 1, 1);
MODULE_VERSION(memac_mdio_acpi, 1);
