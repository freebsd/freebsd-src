/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>
#include <sys/sysctl.h>

#include <err.h>

#include <dev/pci/pcireg.h>

#include <errno.h>

#include "pci_gvt-d-opregion.h"
#include "pci_passthru.h"

#define PCI_VENDOR_INTEL 0x8086

#define GVT_D_MAP_GSM 0

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

/*
 * Note that the graphics stolen memory is somehow confusing. On the one hand
 * the Intel Open Source HD Graphics Programmers' Reference Manual states that
 * it's only GPU accessible. As the CPU can't access the area, the guest
 * shouldn't need it. On the other hand, the Intel GOP driver refuses to work
 * properly, if it's not set to a proper address.
 *
 * Intel itself maps it into the guest by EPT [1]. At the moment, we're not
 * aware of any situation where this EPT mapping is required, so we don't do it
 * yet.
 *
 * Intel also states that the Windows driver for Tiger Lake reads the address of
 * the graphics stolen memory [2]. As the GVT-d code doesn't support Tiger Lake
 * in its first implementation, we can't check how it behaves. We should keep an
 * eye on it.
 *
 * [1]
 * https://github.com/projectacrn/acrn-hypervisor/blob/e28d6fbfdfd556ff1bc3ff330e41d4ddbaa0f897/devicemodel/hw/pci/passthrough.c#L655-L657
 * [2]
 * https://github.com/projectacrn/acrn-hypervisor/blob/e28d6fbfdfd556ff1bc3ff330e41d4ddbaa0f897/devicemodel/hw/pci/passthrough.c#L626-L629
 */
static int
gvt_d_setup_gsm(struct pci_devinst *const pi)
{
	struct passthru_softc *sc;
	struct passthru_mmio_mapping *gsm;
	size_t sysctl_len;
	int error;

	sc = pi->pi_arg;

	gsm = passthru_get_mmio(sc, GVT_D_MAP_GSM);
	if (gsm == NULL) {
		warnx("%s: Unable to access gsm", __func__);
		return (-1);
	}

	sysctl_len = sizeof(gsm->hpa);
	error = sysctlbyname("hw.intel_graphics_stolen_base", &gsm->hpa,
	    &sysctl_len, NULL, 0);
	if (error) {
		warn("%s: Unable to get graphics stolen memory base",
		    __func__);
		return (-1);
	}
	sysctl_len = sizeof(gsm->len);
	error = sysctlbyname("hw.intel_graphics_stolen_size", &gsm->len,
	    &sysctl_len, NULL, 0);
	if (error) {
		warn("%s: Unable to get graphics stolen memory length",
		    __func__);
		return (-1);
	}
	gsm->hva = NULL; /* unused */

	return (0);
}

static int
gvt_d_init(struct pci_devinst *const pi, nvlist_t *const nvl __unused)
{
	int error;

	if ((error = gvt_d_setup_gsm(pi)) != 0) {
		warnx("%s: Unable to setup Graphics Stolen Memory", __func__);
		goto done;
	}

done:
	return (error);
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
