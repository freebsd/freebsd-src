/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/types.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <machine/bus_dma.h>
#include <dev/pci/pcireg.h>

MALLOC_DEFINE(M_QAT, "qat", "qat");

struct bus_dma_mem_cb_data {
	struct bus_dmamem *mem;
	int error;
};

static void
bus_dma_mem_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bus_dma_mem_cb_data *d;

	d = arg;
	d->error = error;
	if (error)
		return;
	d->mem->dma_baddr = segs[0].ds_addr;
}

int
bus_dma_mem_create(struct bus_dmamem *mem,
		   bus_dma_tag_t parent,
		   bus_size_t alignment,
		   bus_addr_t lowaddr,
		   bus_size_t len,
		   int flags)
{
	struct bus_dma_mem_cb_data d;
	int error;

	bzero(mem, sizeof(*mem));
	error = bus_dma_tag_create(parent,
				   alignment,
				   0,
				   lowaddr,
				   BUS_SPACE_MAXADDR,
				   NULL,
				   NULL,
				   len,
				   1,
				   len,
				   0,
				   NULL,
				   NULL,
				   &mem->dma_tag);
	if (error) {
		bus_dma_mem_free(mem);
		return (error);
	}
	error = bus_dmamem_alloc(mem->dma_tag,
				 &mem->dma_vaddr,
				 flags,
				 &mem->dma_map);
	if (error) {
		bus_dma_mem_free(mem);
		return (error);
	}
	d.mem = mem;
	error = bus_dmamap_load(mem->dma_tag,
				mem->dma_map,
				mem->dma_vaddr,
				len,
				bus_dma_mem_cb,
				&d,
				BUS_DMA_NOWAIT);
	if (error == 0)
		error = d.error;
	if (error) {
		bus_dma_mem_free(mem);
		return (error);
	}
	return (0);
}

void
bus_dma_mem_free(struct bus_dmamem *mem)
{

	if (mem->dma_baddr != 0)
		bus_dmamap_unload(mem->dma_tag, mem->dma_map);
	if (mem->dma_vaddr != NULL)
		bus_dmamem_free(mem->dma_tag, mem->dma_vaddr, mem->dma_map);
	if (mem->dma_tag != NULL)
		bus_dma_tag_destroy(mem->dma_tag);
	bzero(mem, sizeof(*mem));
}

device_t
pci_find_pf(device_t vf)
{
	return (NULL);
}

int
pci_set_max_payload(device_t dev, int payload_size)
{
	const int packet_sizes[6] = { 128, 256, 512, 1024, 2048, 4096 };
	int cap_reg = 0, reg_value = 0, mask = 0;

	for (mask = 0; mask < 6; mask++) {
		if (payload_size == packet_sizes[mask])
			break;
	}
	if (mask == 6)
		return -1;

	if (pci_find_cap(dev, PCIY_EXPRESS, &cap_reg) != 0)
		return -1;

	cap_reg += PCIER_DEVICE_CTL; /* Offset for Device Control Register. */
	reg_value = pci_read_config(dev, cap_reg, 1);
	reg_value = (reg_value & 0x1f) | (mask << 5);
	pci_write_config(dev, cap_reg, reg_value, 1);
	return 0;
}
