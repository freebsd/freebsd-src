/*-
 * Copyright (C) 2008 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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

#ifndef	_POWERPC_POWERMAC_CPCHTVAR_H_
#define	_POWERPC_POWERMAC_CPCHTVAR_H_

struct cpcpci_range {
	u_int32_t	pci_hi;
	u_int32_t	pci_mid;
	u_int32_t	pci_lo;
	u_int32_t	junk;
	u_int32_t	host_hi;
	u_int32_t	host_lo;
	u_int32_t	size_hi;
	u_int32_t	size_lo;
};

struct cpcpci_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	vm_offset_t		sc_data;
	int			sc_bus;
	struct			cpcpci_range sc_range[6];
	int			sc_nrange;
	int			sc_iostart;
	struct			rman sc_io_rman;
	struct			rman sc_mem_rman;
	bus_space_tag_t		sc_iot;
	bus_space_tag_t		sc_memt;
	bus_dma_tag_t		sc_dmat;
	struct ofw_bus_iinfo	sc_pci_iinfo;
};

#endif  /* _POWERPC_POWERMAC_CPCHTVAR_H_ */
