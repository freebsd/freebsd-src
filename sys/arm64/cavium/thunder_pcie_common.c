/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Common PCIe functions for Cavium Thunder SOC */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include "thunder_pcie_common.h"


int
thunder_common_map_msi(device_t pcib, device_t child, int irq,
    uint64_t *addr, uint32_t *data)
{
	int error;

	error = arm_map_msix(child, irq, addr, data);
	return (error);
}

int
thunder_common_alloc_msix(device_t pcib, device_t child, int *irq)
{
	int error;

	error = arm_alloc_msix(child, irq);
	return (error);
}

int
thunder_common_release_msix(device_t pcib, device_t child, int irq)
{
	int error;

	error = arm_release_msix(child, irq);
	return (error);
}

int
thunder_common_alloc_msi(device_t pcib, device_t child, int count, int maxcount,
    int *irqs)
{
	int error;

	error = arm_alloc_msi(child, count, irqs);
	return (error);
}

int
thunder_common_release_msi(device_t pcib, device_t child, int count, int *irqs)
{
	int error;

	error = arm_release_msi(child, count, irqs);
	return (error);
}

uint32_t
range_addr_is_pci(struct pcie_range *ranges, uint64_t addr, uint64_t size)
{
	struct pcie_range *r;
	int tuple;

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		r = &ranges[tuple];
		if (addr >= r->pci_base &&
		    addr < (r->pci_base + r->size) &&
		    size < r->size) {
			/* Address is within PCI range */
			return (1);
		}
	}

	/* Address is outside PCI range */
	return (0);
}

uint32_t
range_addr_is_phys(struct pcie_range *ranges, uint64_t addr, uint64_t size)
{
	struct pcie_range *r;
	int tuple;

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		r = &ranges[tuple];
		if (addr >= r->phys_base &&
		    addr < (r->phys_base + r->size) &&
		    size < r->size) {
			/* Address is within Physical range */
			return (1);
		}
	}

	/* Address is outside Physical range */
	return (0);
}

uint64_t
range_addr_pci_to_phys(struct pcie_range *ranges, uint64_t pci_addr)
{
	struct pcie_range *r;
	uint64_t offset;
	int tuple;

	/* Find physical address corresponding to given bus address */
	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		r = &ranges[tuple];
		if (pci_addr >= r->pci_base &&
		    pci_addr < (r->pci_base + r->size)) {
			/* Given pci addr is in this range.
			 * Translate bus addr to phys addr.
			 */
			offset = pci_addr - r->pci_base;
			return (r->phys_base + offset);
		}
	}
	return (0);
}

