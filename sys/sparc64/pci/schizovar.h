/*-
 * Copyright (c) 2005 by Marius Strobl <marius@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_PCI_SCHIZOVAR_H_
#define	_SPARC64_PCI_SCHIZOVAR_H_

struct schizo_softc {
	device_t			sc_dev;

	struct mtx			*sc_mtx;

	phandle_t			sc_node;

	u_int				sc_mode;
#define	SCHIZO_MODE_SCZ		1
#define	SCHIZO_MODE_TOM		2
#define	SCHIZO_MODE_XMS		3

	u_int				sc_half;
	uint32_t			sc_ign;
	uint32_t			sc_ver;

	struct resource			*sc_mem_res[TOM_NREG];
	struct resource			*sc_irq_res[STX_NINTR];
	void				*sc_ihand[STX_NINTR];

	struct iommu_state		sc_is;

	struct rman			sc_pci_mem_rman;
	struct rman			sc_pci_io_rman;
	bus_space_handle_t		sc_pci_bh[STX_NRANGE];
	bus_space_tag_t			sc_pci_cfgt;
	bus_space_tag_t			sc_pci_iot;
	bus_space_tag_t			sc_pci_memt;
	bus_dma_tag_t			sc_pci_dmat;

	uint8_t				sc_pci_secbus;

	struct ofw_bus_iinfo		sc_pci_iinfo;

	SLIST_ENTRY(schizo_softc)	sc_link;
};

#endif /* !_SPARC64_PCI_SCHIZOVAR_H_ */
