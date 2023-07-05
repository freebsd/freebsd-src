/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysctl.h>

#include <dev/pci/pcireg.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "e820.h"
#include "pci_gvt-d-opregion.h"
#include "pci_passthru.h"

#define KB (1024UL)
#define MB (1024 * KB)
#define GB (1024 * MB)

#ifndef _PATH_MEM
#define _PATH_MEM "/dev/mem"
#endif

#define PCI_VENDOR_INTEL 0x8086

#define PCIR_BDSM 0x5C	   /* Base of Data Stolen Memory register */
#define PCIR_ASLS_CTL 0xFC /* Opregion start address register */

#define PCIM_BDSM_GSM_ALIGNMENT \
	0x00100000 /* Graphics Stolen Memory is 1 MB aligned */

#define GVT_D_MAP_GSM 0
#define GVT_D_MAP_OPREGION 1

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

static vm_paddr_t
gvt_d_alloc_mmio_memory(const vm_paddr_t host_address, const vm_paddr_t length,
    const vm_paddr_t alignment, const enum e820_memory_type type)
{
	vm_paddr_t address;

	/* Try to reuse host address. */
	address = e820_alloc(host_address, length, E820_ALIGNMENT_NONE, type,
	    E820_ALLOCATE_SPECIFIC);
	if (address != 0) {
		return (address);
	}

	/*
	 * We're not able to reuse the host address. Fall back to the highest usable
	 * address below 4 GB.
	 */
	return (
	    e820_alloc(4 * GB, length, alignment, type, E820_ALLOCATE_HIGHEST));
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
	uint32_t bdsm;
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
	gsm->gva = NULL; /* unused */
	gsm->gpa = gvt_d_alloc_mmio_memory(gsm->hpa, gsm->len,
	    PCIM_BDSM_GSM_ALIGNMENT, E820_TYPE_RESERVED);
	if (gsm->gpa == 0) {
		warnx(
		    "%s: Unable to add Graphics Stolen Memory to E820 table (hpa 0x%lx len 0x%lx)",
		    __func__, gsm->hpa, gsm->len);
		e820_dump_table();
		return (-1);
	}
	if (gsm->gpa != gsm->hpa) {
		/*
		 * ACRN source code implies that graphics driver for newer Intel
		 * platforms like Tiger Lake will read the Graphics Stolen Memory
		 * address from an MMIO register. We have three options to solve this
		 * issue:
		 *    1. Patch the value in the MMIO register
		 *       This could have unintended side effects. Without any
		 *       documentation how this register is used by the GPU, don't do
		 *       it.
		 *    2. Trap the MMIO register
		 *       It's not possible to trap a single MMIO register. We need to
		 *       trap a whole page. Trapping a bunch of MMIO register could
		 *       degrade the performance noticeably. We have to test it.
		 *    3. Use an 1:1 host to guest mapping
		 *       Maybe not always possible. As far as we know, no supported
		 *       platform requires a 1:1 mapping. For that reason, just log a
		 *       warning.
		 */
		warnx(
		    "Warning: Unable to reuse host address of Graphics Stolen Memory. GPU passthrough might not work properly.");
	}

	bdsm = read_config(passthru_get_sel(sc), PCIR_BDSM, 4);
	pci_set_cfgdata32(pi, PCIR_BDSM,
	    gsm->gpa | (bdsm & (PCIM_BDSM_GSM_ALIGNMENT - 1)));

	return (set_pcir_handler(sc, PCIR_BDSM, 4, passthru_cfgread_emulate,
	    passthru_cfgwrite_emulate));
}

static int
gvt_d_setup_opregion(struct pci_devinst *const pi)
{
	struct passthru_softc *sc;
	struct passthru_mmio_mapping *opregion;
	struct igd_opregion_header *header;
	uint64_t asls;
	int memfd;

	sc = pi->pi_arg;

	memfd = open(_PATH_MEM, O_RDONLY, 0);
	if (memfd < 0) {
		warn("%s: Failed to open %s", __func__, _PATH_MEM);
		return (-1);
	}

	opregion = passthru_get_mmio(sc, GVT_D_MAP_OPREGION);
	if (opregion == NULL) {
		warnx("%s: Unable to access opregion", __func__);
		close(memfd);
		return (-1);
	}

	asls = read_config(passthru_get_sel(sc), PCIR_ASLS_CTL, 4);

	header = mmap(NULL, sizeof(*header), PROT_READ, MAP_SHARED, memfd,
	    asls);
	if (header == MAP_FAILED) {
		warn("%s: Unable to map OpRegion header", __func__);
		close(memfd);
		return (-1);
	}
	if (memcmp(header->sign, IGD_OPREGION_HEADER_SIGN,
	    sizeof(header->sign)) != 0) {
		warnx("%s: Invalid OpRegion signature", __func__);
		munmap(header, sizeof(*header));
		close(memfd);
		return (-1);
	}

	opregion->hpa = asls;
	opregion->len = header->size * KB;
	munmap(header, sizeof(header));

	opregion->hva = mmap(NULL, opregion->len * KB, PROT_READ, MAP_SHARED,
	    memfd, opregion->hpa);
	if (opregion->hva == MAP_FAILED) {
		warn("%s: Unable to map host OpRegion", __func__);
		close(memfd);
		return (-1);
	}
	close(memfd);

	opregion->gpa = gvt_d_alloc_mmio_memory(opregion->hpa, opregion->len,
	    E820_ALIGNMENT_NONE, E820_TYPE_NVS);
	if (opregion->gpa == 0) {
		warnx(
		    "%s: Unable to add OpRegion to E820 table (hpa 0x%lx len 0x%lx)",
		    __func__, opregion->hpa, opregion->len);
		e820_dump_table();
		return (-1);
	}
	opregion->gva = vm_map_gpa(pi->pi_vmctx, opregion->gpa, opregion->len);
	if (opregion->gva == NULL) {
		warnx("%s: Unable to map guest OpRegion", __func__);
		return (-1);
	}
	if (opregion->gpa != opregion->hpa) {
		/*
		 * A 1:1 host to guest mapping is not required but this could
		 * change in the future.
		 */
		warnx(
		    "Warning: Unable to reuse host address of OpRegion. GPU passthrough might not work properly.");
	}

	memcpy(opregion->gva, opregion->hva, opregion->len);

	pci_set_cfgdata32(pi, PCIR_ASLS_CTL, opregion->gpa);

	return (set_pcir_handler(sc, PCIR_ASLS_CTL, 4, passthru_cfgread_emulate,
	    passthru_cfgwrite_emulate));
}

static int
gvt_d_init(struct pci_devinst *const pi, nvlist_t *const nvl __unused)
{
	int error;

	if ((error = gvt_d_setup_gsm(pi)) != 0) {
		warnx("%s: Unable to setup Graphics Stolen Memory", __func__);
		goto done;
	}

	if ((error = gvt_d_setup_opregion(pi)) != 0) {
		warnx("%s: Unable to setup OpRegion", __func__);
		goto done;
	}

done:
	return (error);
}

static void
gvt_d_deinit(struct pci_devinst *const pi)
{
	struct passthru_softc *sc;
	struct passthru_mmio_mapping *opregion;

	sc = pi->pi_arg;

	opregion = passthru_get_mmio(sc, GVT_D_MAP_OPREGION);

	/* HVA is only set, if it's initialized */
	if (opregion->hva)
		munmap((void *)opregion->hva, opregion->len);
}

static struct passthru_dev gvt_d_dev = {
	.probe = gvt_d_probe,
	.init = gvt_d_init,
	.deinit = gvt_d_deinit,
};
PASSTHRU_DEV_SET(gvt_d_dev);
