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

/* SAL functions */
#define	SAL_SGISN_MASTER_NASID		0x02000004
#define	SAL_SGISN_KLCFG_ADDR		0x02000005
#define	SAL_SGISN_SAPIC_INFO		0x0200001d
#define	SAL_SGISN_SN_INFO		0x0200001e
#define	SAL_SGISN_PUTC			0x02000021
#define	SAL_SGISN_GETC			0x02000022
#define	SAL_SGISN_POLL			0x02000026
#define	SAL_SGISN_CON_INTR		0x02000027
#define	SAL_SGISN_TXBUF			0x02000028
#define	SAL_SGISN_IOHUB_INFO		0x02000055
#define	SAL_SGISN_IOBUS_INFO		0x02000056
#define	SAL_SGISN_IODEV_INFO		0x02000057
#define	SAL_SGISN_FEATURE_GET_PROM	0x02000065
#define	SAL_SGISN_FEATURE_SET_OS	0x02000066
#define	SAL_SGISN_SET_CPUID		0x02000068

#define	SGISN_HUB_NITTES	8
#define	SGISN_HUB_NWIDGETS	16

#define	SHUB_IVAR_PCIBUS	1
#define	SHUB_IVAR_PCISEG	2
#define	SHUB_IVAR_EVENT		3

#define	SHUB_EVENT_CONSOLE	0x100000

struct sgisn_geoid {
	uint32_t	sg_module;
	uint8_t		sg_type;
#define	SGISN_GEOID_TYPE_INVALID	0
#define	SGISN_GEOID_TYPE_MODULE		1
#define	SGISN_GEOID_TYPE_NODE		2
#define	SGISN_GEOID_TYPE_RTR		3
#define	SGISN_GEOID_TYPE_IOC		4
#define	SGISN_GEOID_TYPE_DEV		5	/* PCI device */
#define	SGISN_GEOID_TYPE_CPU		6
#define	SGISN_GEOID_TYPE_MEM		7
	uint8_t		sg_slab:4;
	uint8_t		sg_slot:4;
	union {
		struct {
			uint8_t slice;
		} cpu;
		struct {
			uint8_t bus;
			uint8_t slot;
		} dev;
		struct {
			uint8_t	bus;
			uint8_t	slot;
		} mem;
	} sg_u;
};

struct sgisn_fwdev;
struct sgisn_fwhub;

struct sgisn_widget {
	uint32_t		wgt_hwmfg;
	uint32_t		wgt_hwrev;
	uint32_t		wgt_hwpn;
	uint8_t			wgt_port;
	uint8_t			_pad[3];
	struct sgisn_fwhub	*wgt_hub;
	uint64_t		wgt_funcs;
	uint64_t		wgt_vertex;
};

struct sgisn_fwbus {
	uint32_t		bus_asic;
	uint32_t		bus_xid;
	uint32_t		bus_busnr;
	uint32_t		bus_segment;
	uint64_t		bus_ioport_addr;
	uint64_t		bus_memio_addr;
	uint64_t		bus_base;
	struct sgisn_widget	*bus_wgt_info;
};

struct sgisn_fwflush_dev {
	uint32_t		fld_bus;
	uint32_t		fld_slot;
	uint32_t		fld_pin;
	uint32_t		_pad;
	struct {
		uint64_t	start;
		uint64_t	end;
	} fld_bar[6];
	uint64_t		*fld_intr;
	uint64_t		fld_value;
	uint64_t		*fld_flush;
	uint32_t		fld_pci_bus;
	uint32_t		fld_pci_segment;
	struct sgisn_fwbus	*fld_parent;
	uint64_t		fld_xxx;
};

struct sgisn_fwflush_widget {
	struct sgisn_fwflush_dev flw_dev[32];
};

struct sgisn_fwhub {
	struct sgisn_geoid	hub_geoid;
	uint16_t		hub_nasid;
	uint16_t		hub_peer_nasid;
	uint32_t		_pad;
	struct sgisn_fwflush_widget *hub_flush;
	uint64_t		hub_dma_itte[SGISN_HUB_NITTES];
	struct sgisn_widget	hub_widget[SGISN_HUB_NWIDGETS];

	void	*hdi_nodepda;
	void	*hdi_node_vertex;

	uint32_t		hub_pci_maxseg;
	uint32_t		hub_pci_maxbus;
};

struct sgisn_fwirq {
	uint64_t		_obsolete;
	uint16_t		irq_nasid;
	uint16_t		_pad1;
	uint32_t		irq_slice;
	uint32_t		irq_cpuid;
	uint32_t		irq_nr;
	uint32_t		irq_pin;
	uint32_t		_pad2;
	uint64_t		irq_xtaddr;
	uint32_t		irq_br_type;
	uint32_t		_pad3;
	void			*irq_bridge;	/* Originating */
	struct sgisn_fwdev	*irq_dev;
	uint32_t		irq_last;
	uint32_t		irq_cookie;
	uint32_t		irq_flags;
	uint32_t		irq_refcnt;
};

struct sgisn_fwdev {
	uint64_t		dev_bar[6];
	uint64_t		dev_romaddr;
	uint64_t		dev_handle;
	struct sgisn_fwbus	*dev_parent;
	uint64_t		dev_os_private[2];
	struct sgisn_fwirq	*dev_irq;
};

/*
 * KLCFG stuff...
 */

static __inline void *
sgisn_klcfg_ptr(uint64_t base, int32_t ofs)
{
	void *ptr;

	ptr = (void *)IA64_PHYS_TO_RR7(base + ofs);
	return (ptr);
}

struct sgisn_klcfg_hdr {
	uint64_t	skh_magic;
#define	SGISN_KLCFG_MAGIC		0xbeedbabe
	uint32_t	skh_version;
	int32_t		skh_ofs_mallocs;
	int32_t		skh_ofs_console;
	int32_t		skh_board_info;
	/* more fields here. */
};

struct sgisn_klcfg_board {
	int32_t		skb_next;
	uint8_t		skb_affinity;	/* local or remote */
#define	SGISN_KLCFG_BOARD_TYPE_LOCAL	1
#define	SGISN_KLCFG_BOARD_TYPE_REMOTE	2
	uint8_t		skb_type;
	uint8_t		skb_version;	/* structure version */
	uint8_t		skb_revision;	/* board revision */
	uint8_t		skb_promver;
	uint8_t		skb_flags;
	uint8_t		skb_slot;
	uint16_t	skb_dbgsw;
	struct sgisn_geoid skb_geoid;
	int8_t		skb_partition;
	uint16_t	skb_diag[4];	/* xxx */
	uint8_t		skb_inventory;
	uint8_t		skb_ncompts;
	uint64_t	skb_nic;
	int16_t		skb_nasid;
	int32_t		skb_compts[24];
	int32_t		skb_errinfo;
	int32_t		skb_parent;
	uint32_t	_pad1;
	uint8_t		skb_badness;
	int16_t		skb_owner;
	uint8_t		skb_nicflags;
	uint8_t		_pad2[28];
	char		skb_name[32];
	int16_t		skb_peer_host;
	int32_t		skb_peer;
};

struct sgisn_klcfg_compt {
	uint8_t		skc_type;
	uint8_t		skc_version;
	uint8_t		skc_flags;
	uint8_t		skc_revision;
	uint16_t	skc_diag[2];
	uint8_t		skc_inventory;
	uint16_t	skc_partid;
	uint64_t	skc_nic;
	uint8_t		skc_physid;
	uint32_t	skc_virtid;
	uint8_t		skc_wdgtid;
	int16_t		skc_nasid;
	uint64_t	skc_data;
	int32_t		skc_errinfo;
};

#endif /* !_MACHINE_SGISN_H_ */
