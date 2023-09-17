/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Semihalf
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/taskqueue.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt_gpio.h>
#include <dev/sdhci/sdhci_xenon.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"
#include "opt_soc.h"

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada-3700-sdhci",	1 },
#ifdef SOC_MARVELL_8K
	{ "marvell,armada-cp110-sdhci",	1 },
	{ "marvell,armada-ap806-sdhci",	1 },
	{ "marvell,armada-ap807-sdhci",	1 },
#endif
	{ NULL, 0 }
};

static bool
sdhci_xenon_fdt_get_card_present(device_t dev, struct sdhci_slot *slot)
{
	struct sdhci_xenon_softc *sc;

	sc = device_get_softc(dev);

	return (sdhci_fdt_gpio_get_present(sc->gpio));
}

static int
sdhci_xenon_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Armada Xenon SDHCI controller");

	return (BUS_PROBE_SPECIFIC);
}

static void
sdhci_xenon_fdt_parse(device_t dev, struct sdhci_slot *slot)
{
	struct sdhci_xenon_softc *sc;
	struct mmc_helper mmc_helper;

	sc = device_get_softc(dev);
	memset(&mmc_helper, 0, sizeof(mmc_helper));

	/* MMC helper for parsing FDT */
	mmc_fdt_parse(dev, 0, &mmc_helper, &slot->host);

	sc->skip_regulators = false;
	sc->vmmc_supply = mmc_helper.vmmc_supply;
	sc->vqmmc_supply = mmc_helper.vqmmc_supply;
	sc->wp_inverted = mmc_helper.props & MMC_PROP_WP_INVERTED;

	/* Check if the device is flagged as non-removable. */
	if (mmc_helper.props & MMC_PROP_NON_REMOVABLE) {
		slot->opt |= SDHCI_NON_REMOVABLE;
		if (bootverbose)
			device_printf(dev, "Non-removable media\n");
	}
}

static int
sdhci_xenon_fdt_attach(device_t dev)
{
	struct sdhci_xenon_softc *sc;
	struct sdhci_slot *slot;

	sc = device_get_softc(dev);
	slot = malloc(sizeof(*slot), M_DEVBUF, M_ZERO | M_WAITOK);

	sdhci_xenon_fdt_parse(dev, slot);

	/*
	 * Set up any gpio pin handling described in the FDT data. This cannot
	 * fail; see comments in sdhci_fdt_gpio.h for details.
	 */
	sc->gpio = sdhci_fdt_gpio_setup(dev, slot);
	sc->slot = slot;

	return (sdhci_xenon_attach(dev));
}

static int
sdhci_xenon_fdt_detach(device_t dev)
{
	struct sdhci_xenon_softc *sc;

	sc = device_get_softc(dev);
	if (sc->gpio != NULL)
		sdhci_fdt_gpio_teardown(sc->gpio);

	return (sdhci_xenon_detach(dev));
}

static device_method_t sdhci_xenon_fdt_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_xenon_fdt_probe),
	DEVMETHOD(device_attach,	sdhci_xenon_fdt_attach),
	DEVMETHOD(device_detach,	sdhci_xenon_fdt_detach),

	DEVMETHOD(sdhci_get_card_present, sdhci_xenon_fdt_get_card_present),

	DEVMETHOD_END
};

DEFINE_CLASS_1(sdhci_xenon, sdhci_xenon_fdt_driver, sdhci_xenon_fdt_methods,
    sizeof(struct sdhci_xenon_softc), sdhci_xenon_driver);

DRIVER_MODULE(sdhci_xenon, simplebus, sdhci_xenon_fdt_driver, NULL, NULL);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_xenon_fdt);
#endif
