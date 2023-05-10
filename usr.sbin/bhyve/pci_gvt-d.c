/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>

#include <dev/pci/pcireg.h>

#include <errno.h>

#include "pci_gvt-d-opregion.h"
#include "pci_passthru.h"

#define PCI_VENDOR_INTEL 0x8086

static int
gvt_d_probe(struct pci_devinst *const pi)
{
	struct passthru_softc *sc;
	uint16_t vendor;
	uint8_t class;

	sc = pi->pi_arg;

	vendor = read_config(passthru_get_sel(sc), PCIR_VENDOR, 0x02);
	if (vendor != PCI_VENDOR_INTEL)
		return (ENXIO);

	class = read_config(passthru_get_sel(sc), PCIR_CLASS, 0x01);
	if (class != PCIC_DISPLAY)
		return (ENXIO);

	return (0);
}

static int
gvt_d_init(struct pci_devinst *const pi __unused, nvlist_t *const nvl __unused)
{
	return (0);
}

static void
gvt_d_deinit(struct pci_devinst *const pi __unused)
{
}

static struct passthru_dev gvt_d_dev = {
	.probe = gvt_d_probe,
	.init = gvt_d_init,
	.deinit = gvt_d_deinit,
};
PASSTHRU_DEV_SET(gvt_d_dev);
