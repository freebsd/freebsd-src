/*
 * Copyright (c) 1999, 2000 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: psychovar.h,v 1.6 2001/07/20 00:07:13 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _SPARC64_PCI_PSYCHOVAR_H_
#define _SPARC64_PCI_PSYCHOVAR_H_

/*
 * per-PCI bus on mainbus softc structure; one for sabre, or two
 * per pair of psycho's.
 */
struct psycho_softc {
	device_t			sc_dev;
	/*
	 * PSYCHO register.  we record the base physical address of these
	 * also as it is the base of the entire PSYCHO
	 */
	vm_offset_t			sc_basepaddr;

	/* Interrupt Group Number for this device */
	int				sc_ign;

	/* our tags (from parent) */
	bus_space_tag_t			sc_bustag;
	bus_space_handle_t		sc_bushandle;
	bus_dma_tag_t			sc_dmatag;

	bus_addr_t			sc_pcictl;

	int				sc_clockfreq;
	phandle_t			sc_node;	/* prom node */
	int				sc_mode;
#define	PSYCHO_MODE_SABRE	1
#define	PSYCHO_MODE_PSYCHO	2

	struct iommu_state		*sc_is;

	struct resource			*sc_mem_res;
	struct resource			*sc_irq_res[6];
	void				*sc_ihand[6];

	/*
	 * note that the sabre really only has one ranges property,
	 * used for both simba a and simba b (but the ranges for
	 * real psychos are the same for PCI A and PCI B anyway).
	 */
	struct upa_ranges		*sc_range;
	int				sc_nrange;
	struct ofw_pci_imap	 	*sc_intrmap;
	int				sc_nintrmap;
	struct ofw_pci_imap_msk		sc_intrmapmsk;

	/* our tags */
	bus_space_tag_t			sc_cfgt;
	bus_space_tag_t			sc_memt;
	bus_space_tag_t			sc_iot;
	bus_dma_tag_t			sc_dmat;

	bus_space_handle_t		sc_bh[4];

	int				sc_busno;

	struct rman			sc_mem_rman;
	struct rman			sc_io_rman;
};

#endif /* _SPARC64_PCI_PSYCHOVAR_H_ */
