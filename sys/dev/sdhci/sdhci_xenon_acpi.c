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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/taskqueue.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#include <dev/mmc/mmc_helpers.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <dev/acpica/acpivar.h>

#include <dev/regulator/regulator.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_xenon.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"
#include "opt_soc.h"

static char *sdhci_xenon_hids[] = {
	"MRVL0002",
	"MRVL0003",
	"MRVL0004",
	NULL
};

static int
sdhci_xenon_acpi_probe(device_t dev)
{
	device_t bus;
	int err;

	bus = device_get_parent(dev);

	err = ACPI_ID_PROBE(bus, dev, sdhci_xenon_hids, NULL);
	if (err <= 0) {
		device_set_desc(dev, "Armada Xenon SDHCI controller");
		return (err);
	}

	return (ENXIO);
}

static int
sdhci_xenon_acpi_attach(device_t dev)
{
	struct sdhci_xenon_softc *sc;
	struct sdhci_slot *slot;
	struct mmc_helper mmc_helper;

	sc = device_get_softc(dev);
	memset(&mmc_helper, 0, sizeof(mmc_helper));

	slot = malloc(sizeof(*slot), M_DEVBUF, M_ZERO | M_WAITOK);

	/*
	 * Don't use regularators.
	 * In ACPI mode the firmware takes care of them for us
	 */
	sc->skip_regulators = true;
	sc->slot = slot;

	if (mmc_parse(dev, &mmc_helper, &slot->host) != 0)
		return (ENXIO);

	if (mmc_helper.props & MMC_PROP_NON_REMOVABLE) {
		slot->opt |= SDHCI_NON_REMOVABLE;
		if (bootverbose)
			device_printf(dev, "Non-removable media\n");
	}

	sc->wp_inverted = mmc_helper.props & MMC_PROP_WP_INVERTED;

	return (sdhci_xenon_attach(dev));
}

static device_method_t sdhci_xenon_acpi_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_xenon_acpi_probe),
	DEVMETHOD(device_attach,	sdhci_xenon_acpi_attach),
	DEVMETHOD(device_detach,	sdhci_xenon_detach),

	DEVMETHOD(sdhci_get_card_present,	sdhci_generic_get_card_present),

	DEVMETHOD_END
};

DEFINE_CLASS_1(sdhci_xenon, sdhci_xenon_acpi_driver, sdhci_xenon_acpi_methods,
	sizeof(struct sdhci_xenon_softc), sdhci_xenon_driver);

DRIVER_MODULE(sdhci_xenon, acpi, sdhci_xenon_acpi_driver, NULL, NULL);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_xenon_acpi);
#endif
