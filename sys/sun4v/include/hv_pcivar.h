/*-
 * Copyright 2006 John-Mark Gurney.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *
 * $FreeBSD: src/sys/sun4v/include/hv_pcivar.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _HV_PCIVAR_H_
#define _HV_PCIVAR_H_

struct hvpci_softc {
	devhandle_t		hs_devhandle;
	phandle_t		hs_node;
	uint8_t			hs_busnum;

	struct ofw_bus_iinfo	hs_pci_iinfo;

	struct bus_dma_tag	hs_dmatag;

	struct rman		hs_pci_mem_rman;
	bus_space_tag_t		hs_pci_memt;
	bus_space_handle_t	hs_pci_memh;

	struct rman		hs_pci_io_rman;
	bus_space_tag_t		hs_pci_iot;
	bus_space_handle_t	hs_pci_ioh;
};

#endif /* _HV_PCIVAR_H_ */
