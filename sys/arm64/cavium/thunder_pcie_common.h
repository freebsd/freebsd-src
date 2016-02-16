/*-
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _CAVIUM_THUNDER_PCIE_COMMON_H_
#define	_CAVIUM_THUNDER_PCIE_COMMON_H_

#define	RANGES_TUPLES_MAX	6
#define	RANGES_TUPLES_INVALID (RANGES_TUPLES_MAX + 1)

DECLARE_CLASS(thunder_pcie_driver);
DECLARE_CLASS(thunder_pem_driver);

MALLOC_DECLARE(M_THUNDER_PCIE);

struct pcie_range {
	uint64_t	pci_base;
	uint64_t	phys_base;
	uint64_t	size;
	uint64_t	flags;
};

struct thunder_pcie_softc {
	struct pcie_range	ranges[RANGES_TUPLES_MAX];
	struct rman		mem_rman;
	struct resource		*res;
	int			ecam;
	device_t		dev;
};

uint32_t range_addr_is_pci(struct pcie_range *, uint64_t, uint64_t);
uint32_t range_addr_is_phys(struct pcie_range *, uint64_t, uint64_t);
uint64_t range_addr_pci_to_phys(struct pcie_range *, uint64_t);
int thunder_common_alloc_msi(device_t, device_t, int, int, int *);
int thunder_common_alloc_msix(device_t, device_t, int *);
int thunder_common_map_msi(device_t, device_t, int, uint64_t *, uint32_t *);
int thunder_common_release_msi(device_t, device_t, int, int *);
int thunder_common_release_msix(device_t, device_t, int);

struct resource *thunder_pcie_alloc_resource(device_t,
    device_t, int, int *, rman_res_t, rman_res_t, rman_res_t, u_int);
int thunder_pcie_release_resource(device_t, device_t, int, int,
    struct resource *);

int thunder_pcie_attach(device_t);

#endif /* _CAVIUM_THUNDER_PCIE_COMMON_H_ */
