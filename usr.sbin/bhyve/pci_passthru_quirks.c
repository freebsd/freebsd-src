/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <dev/pci/pcireg.h>

#include <errno.h>

#include "pci_passthru.h"

#define PCI_VENDOR_NVIDIA 0x10DE

static int
nvidia_gpu_probe(struct pci_devinst *const pi)
{
	struct passthru_softc *sc;
	uint16_t vendor;
	uint8_t class;

	sc = pi->pi_arg;

	vendor = pci_host_read_config(passthru_get_sel(sc), PCIR_VENDOR, 0x02);
	if (vendor != PCI_VENDOR_NVIDIA)
		return (ENXIO);

	class = pci_host_read_config(passthru_get_sel(sc), PCIR_CLASS, 0x01);
	if (class != PCIC_DISPLAY)
		return (ENXIO);

	return (0);
}

static int
nvidia_gpu_init(struct pci_devinst *const pi, nvlist_t *const nvl __unused)
{
	pci_set_cfgdata8(pi, PCIR_INTPIN, 1);

	return (0);
}

static struct passthru_dev nvidia_gpu = {
	.probe = nvidia_gpu_probe,
	.init = nvidia_gpu_init,
};
PASSTHRU_DEV_SET(nvidia_gpu);
