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
 * $FreeBSD: src/sys/powerpc/powermac/uninorthvar.h,v 1.4 2005/01/07 02:29:20 imp Exp $
 */

#ifndef	_POWERPC_POWERMAC_UNINORTHVAR_H_
#define	_POWERPC_POWERMAC_UNINORTHVAR_H_

struct uninorth_range {
	u_int32_t	pci_hi;
	u_int32_t	pci_mid;
	u_int32_t	pci_lo;
	u_int32_t	host;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct uninorth_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	vm_offset_t		sc_addr;
	vm_offset_t		sc_data;
	int			sc_bus;
	struct			uninorth_range sc_range[6];
	int			sc_nrange;
	int			sc_iostart;
	struct			rman sc_io_rman;
	struct			rman sc_mem_rman;
	bus_space_tag_t		sc_iot;
	bus_space_tag_t		sc_memt;
	bus_dma_tag_t		sc_dmat;
};

struct unin_chip_softc {
	vm_offset_t		sc_physaddr;
	vm_offset_t		sc_addr;
	u_int			sc_size;
	int			sc_version;
};

/*
 * Version register
 */
#define UNIN_VERS       0x0

/*
 * Clock-control register
 */
#define UNIN_CLOCKCNTL  0x20
#define UNIN_CLOCKCNTL_GMAC   0x2

#endif  /* _POWERPC_POWERMAC_UNINORTHVAR_H_ */
