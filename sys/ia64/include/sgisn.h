/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
 * $FreeBSD$
 */
#ifndef _MACHINE_SGISN_H_
#define _MACHINE_SGISN_H_

#define	SGISN_GEOID_MODULE(id)		(((id) >> 0) & 0xffffffffu)
#define	SGISN_GEOID_TYPE(id)		(((id) >> 32) & 0xff)
#define	SGISN_GEOID_SLAB(id)		(((id) >> 40) & 0xff)
#define	SGISN_GEOID_ADDIT(id)		(((id) >> 48) & 0xffff);
#define	SGISN_GEOID_CPU_SLICE(id)	((SGISN_GEOID_ADDIT(id) >> 0) & 0xff)
#define	SGISN_GEOID_DEV_BUS(id)		((SGISN_GEOID_ADDIT(id) >> 0) & 0xff)
#define	SGISN_GEOID_DEV_SLOT(id)	((SGISN_GEOID_ADDIT(id) >> 8) & 0xff)
#define	SGISN_GEOID_MEM_BUS(id)		((SGISN_GEOID_ADDIT(id) >> 0) & 0xff)
#define	SGISN_GEOID_MEM_SLOT(id)	((SGISN_GEOID_ADDIT(id) >> 8) & 0xff)

#define	SGISN_GEO_TYPE_INVALID	0
#define	SGISN_GEO_TYPE_MODULE	1
#define	SGISN_GEO_TYPE_NODE	2
#define	SGISN_GEO_TYPE_RTR	3
#define	SGISN_GEO_TYPE_IOC	4
#define	SGISN_GEO_TYPE_DEV	5	/* PCI device */
#define	SGISN_GEO_TYPE_CPU	6
#define	SGISN_GEO_TYPE_MEM	7

#define	SGISN_HUB_NITTES	8
#define	SGISN_HUB_NWIDGETS	16

struct sgisn_widget {
	uint32_t		wgt_hwmfg;
	uint32_t		wgt_hwrev;
	uint32_t		wgt_hwpn;
	uint8_t			wgt_port;
	char	_pad[3];
	uint64_t		wgt_private;
	uint64_t		wgt_provider;
	uint64_t		wgt_vertex;
};

struct sgisn_hub {
	uint64_t		hub_geoid;
	uint16_t		hub_nasid;
	uint16_t		hub_peer_nasid;
	char	_pad[4];
	uint64_t		hub_pointer;
	uint64_t		hub_dma_itte[SGISN_HUB_NITTES];
	struct sgisn_widget	hub_widget[SGISN_HUB_NWIDGETS];

	void	*hdi_nodepda;
	void	*hdi_node_vertex;

	uint32_t		hub_pci_maxseg;
	uint32_t		hub_pci_maxbus;
};

struct sgisn_irq {
	uint64_t		irq_unused;
	uint16_t		irq_nasid;
	char	_pad1[2];
	u_int			irq_slice;
	u_int			irq_cpuid;
	u_int			irq_no;
	u_int			irq_pin;
	uint64_t		irq_xtaddr;
	u_int			irq_br_type;
	char	_pad2[4];
	uint64_t		irq_bridge;
	uint64_t		irq_io_info;
	u_int			irq_last;
	u_int			irq_cookie;
	u_int			irq_flags;
	u_int			irq_refcnt;
};

struct sgisn_dev {
	uint64_t		dev_bar[6];
	uint64_t		dev_rom;
	uint64_t		dev_handle;
};

#endif /* !_MACHINE_SGISN_H_ */
