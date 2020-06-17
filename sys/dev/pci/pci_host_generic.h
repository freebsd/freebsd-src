/*
 * Copyright (c) 2015, 2020 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * $FreeBSD$
 *
 */

#ifndef __PCI_HOST_GENERIC_H_
#define	__PCI_HOST_GENERIC_H_

#include "pci_if.h"

/* Assembling ECAM Configuration Address */
#define	PCIE_BUS_SHIFT		20
#define	PCIE_SLOT_SHIFT		15
#define	PCIE_FUNC_SHIFT		12
#define	PCIE_BUS_MASK		0xFF
#define	PCIE_SLOT_MASK		0x1F
#define	PCIE_FUNC_MASK		0x07
#define	PCIE_REG_MASK		0xFFF

#define	PCIE_ADDR_OFFSET(bus, slot, func, reg)			\
	((((bus) & PCIE_BUS_MASK) << PCIE_BUS_SHIFT)	|	\
	(((slot) & PCIE_SLOT_MASK) << PCIE_SLOT_SHIFT)	|	\
	(((func) & PCIE_FUNC_MASK) << PCIE_FUNC_SHIFT)	|	\
	((reg) & PCIE_REG_MASK))

#define	MAX_RANGES_TUPLES	16
#define	MIN_RANGES_TUPLES	2

struct pcie_range {
	uint64_t	pci_base;
	uint64_t	phys_base;
	uint64_t	size;
	uint64_t	flags;
#define	FLAG_TYPE(x)		((x) & FLAG_TYPE_MASK)
#define	FLAG_TYPE_MASK		0x3
#define	FLAG_TYPE_INVALID	0x0
#define	FLAG_TYPE_IO		0x1
#define	FLAG_TYPE_MEM		0x2
#define	FLAG_TYPE_PMEM		0x3
};

struct generic_pcie_core_softc {
	struct pcie_range	ranges[MAX_RANGES_TUPLES];
	int			nranges;
	int			coherent;
	bool			has_pmem;
	struct rman		pmem_rman;
	struct rman		mem_rman;
	struct rman		io_rman;
	struct resource		*res;
	struct resource		*res1;
	int			bus_start;
	int			bus_end;
	int			ecam;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	device_t		dev;
	bus_space_handle_t	ioh;
	bus_dma_tag_t		dmat;
};

DECLARE_CLASS(generic_pcie_core_driver);

int pci_host_generic_core_attach(device_t);
struct resource *pci_host_generic_core_alloc_resource(device_t, device_t, int,
    int *, rman_res_t, rman_res_t, rman_res_t, u_int);
int pci_host_generic_core_release_resource(device_t, device_t, int, int,
    struct resource *);

#endif /* __PCI_HOST_GENERIC_H_ */
