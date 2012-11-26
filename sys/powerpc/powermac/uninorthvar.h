/*-
 * Copyright (C) 2002 Benno Rice.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef	_POWERPC_POWERMAC_UNINORTHVAR_H_
#define	_POWERPC_POWERMAC_UNINORTHVAR_H_

#include <powerpc/ofw/ofw_pci.h>

struct uninorth_softc {
	struct ofw_pci_softc	pci_sc;
	vm_offset_t		sc_addr;
	vm_offset_t		sc_data;
	int			sc_ver;
};

struct unin_chip_softc {
	u_int32_t		sc_physaddr;
	vm_offset_t		sc_addr;
	u_int32_t		sc_size;
	struct rman  		sc_mem_rman;
	int			sc_version;
};

/*
 * Format of a unin reg property entry.
 */
struct unin_chip_reg {
        u_int32_t       mr_base;
        u_int32_t       mr_size;
};

/*
 * Per unin device structure.
 */
struct unin_chip_devinfo {
        int        udi_interrupts[6];
        int        udi_ninterrupts;
        int        udi_base;   
        struct ofw_bus_devinfo udi_obdinfo;
        struct resource_list udi_resources;
};

/*
 * Version register
 */
#define UNIN_VERS       0x0

/*
 * Clock-control register
 */
#define UNIN_CLOCKCNTL		0x20
#define UNIN_CLOCKCNTL_GMAC	0x2

/*
 * Toggle registers
 */
#define UNIN_TOGGLE_REG		0xe0
#define UNIN_MPIC_RESET		0x2
#define UNIN_MPIC_OUTPUT_ENABLE	0x4

#endif  /* _POWERPC_POWERMAC_UNINORTHVAR_H_ */
