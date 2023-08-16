/*-
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright (C) 2019 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * BSD LICENSE
 *
 * Copyright (C) 2019 Advanced Micro Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copy
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the distribution.
 * 3. Neither the name of AMD corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information :
 * Rajesh Kumar <rajesh1.kumar@amd.com>
 */

#ifndef	NTB_HW_AMD_H
#define	NTB_HW_AMD_H

#define	NTB_HW_AMD_VENDOR_ID	0x1022
#define	NTB_HW_AMD_DEVICE_ID1	0x145B
#define	NTB_HW_AMD_DEVICE_ID2	0x148B

#define	NTB_HW_HYGON_VENDOR_ID	0x19D4
#define	NTB_HW_HYGON_DEVICE_ID1	0x145B

#define	NTB_DEF_PEER_CNT	1
#define	NTB_DEF_PEER_IDX	0

#define	BIT(n)			(1 << n)
#define	AMD_LINK_HB_TIMEOUT	(1 * hz)

#define	NTB_LIN_STA_ACTIVE_BIT	0x00000002
#define	NTB_LNK_STA_SPEED_MASK	0x000F0000
#define	NTB_LNK_STA_WIDTH_MASK	0x03F00000
#define	NTB_LNK_STA_ACTIVE(x)	(!!((x) & NTB_LIN_STA_ACTIVE_BIT))
#define	NTB_LNK_STA_SPEED(x)	(((x) & NTB_LNK_STA_SPEED_MASK) >> 16)
#define	NTB_LNK_STA_WIDTH(x)	(((x) & NTB_LNK_STA_WIDTH_MASK) >> 20)

#define	amd_ntb_bar_read(SIZE, bar, offset) \
	    bus_space_read_ ## SIZE (ntb->bar_info[(bar)].pci_bus_tag, \
	    ntb->bar_info[(bar)].pci_bus_handle, (offset))
#define	amd_ntb_bar_write(SIZE, bar, offset, val) \
	    bus_space_write_ ## SIZE (ntb->bar_info[(bar)].pci_bus_tag, \
	    ntb->bar_info[(bar)].pci_bus_handle, (offset), (val))
#define	amd_ntb_reg_read(SIZE, offset) \
	    amd_ntb_bar_read(SIZE, NTB_CONFIG_BAR, offset)
#define	amd_ntb_reg_write(SIZE, offset, val) \
	    amd_ntb_bar_write(SIZE, NTB_CONFIG_BAR, offset, val)
#define	amd_ntb_peer_reg_read(SIZE, offset) \
	    amd_ntb_bar_read(SIZE, NTB_CONFIG_BAR, offset + AMD_PEER_OFFSET)
#define	amd_ntb_peer_reg_write(SIZE, offset, val) \
	    amd_ntb_bar_write(SIZE, NTB_CONFIG_BAR, offset + AMD_PEER_OFFSET, val)

#define	DB_MASK_LOCK(sc)	mtx_lock_spin(&(sc)->db_mask_lock)
#define	DB_MASK_UNLOCK(sc)	mtx_unlock_spin(&(sc)->db_mask_lock)
#define	DB_MASK_ASSERT(sc, f)	mtx_assert(&(sc)->db_mask_lock, (f))

#define QUIRK_MW0_32BIT	0x01

/* amd_ntb_conn_type are hardware numbers, cannot change. */
enum amd_ntb_conn_type {
	NTB_CONN_NONE = -1,
	NTB_CONN_PRI,
	NTB_CONN_SEC,
};

enum ntb_default_port {
	NTB_PORT_PRI_USD,
	NTB_PORT_SEC_DSD
};

enum amd_ntb_bar {
	NTB_CONFIG_BAR = 0,
	NTB_BAR_1,
	NTB_BAR_2,
	NTB_BAR_3,
	NTB_MAX_BARS
};

struct amd_ntb_hw_info {
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t	 mw_count;
	uint8_t	 bar_start_idx;
	uint8_t	 spad_count;
	uint8_t	 db_count;
	uint8_t	 msix_vector_count;
	uint8_t	 quirks;
	char	 *desc;
};

struct amd_ntb_pci_bar_info {
	bus_space_tag_t		pci_bus_tag;
	bus_space_handle_t	pci_bus_handle;
	struct resource		*pci_resource;
	vm_paddr_t		pbase;
	caddr_t			vbase;
	vm_size_t		size;
	vm_memattr_t		map_mode;
	int			pci_resource_id;

	/* Configuration register offsets */
	uint32_t		xlat_off;
	uint32_t		limit_off;
};

struct amd_ntb_int_info {
	struct resource	*res;
	void		*tag;
	int		rid;
};

struct amd_ntb_vec {
	struct amd_ntb_softc	*ntb;
	uint32_t		num;
	unsigned		masked;
};

enum {
	/* AMD NTB Link Status Offset */
	AMD_LINK_STATUS_OFFSET	= 0x68,

	/*  AMD NTB register offset */
	AMD_CNTL_OFFSET		= 0x200,

	/* NTB control register bits */
	PMM_REG_CTL		= BIT(21),
	SMM_REG_CTL		= BIT(20),
	SMM_REG_ACC_PATH	= BIT(18),
	PMM_REG_ACC_PATH	= BIT(17),
	NTB_CLK_EN		= BIT(16),

	AMD_STA_OFFSET		= 0x204,
	AMD_PGSLV_OFFSET	= 0x208,
	AMD_SPAD_MUX_OFFSET	= 0x20C,
	AMD_SPAD_OFFSET		= 0x210,
	AMD_RSMU_HCID		= 0x250,
	AMD_RSMU_SIID		= 0x254,
	AMD_PSION_OFFSET	= 0x300,
	AMD_SSION_OFFSET	= 0x330,
	AMD_MMINDEX_OFFSET	= 0x400,
	AMD_MMDATA_OFFSET	= 0x404,
	AMD_SIDEINFO_OFFSET	= 0x408,

	AMD_SIDE_MASK		= BIT(0),
	AMD_SIDE_READY		= BIT(1),

	/* limit register */
	AMD_ROMBARLMT_OFFSET	= 0x410,
	AMD_BAR1LMT_OFFSET	= 0x414,
	AMD_BAR23LMT_OFFSET	= 0x418,
	AMD_BAR45LMT_OFFSET	= 0x420,

	/* xlat address */
	AMD_ROMBARXLAT_OFFSET	= 0x428,
	AMD_BAR1XLAT_OFFSET	= 0x430,
	AMD_BAR23XLAT_OFFSET	= 0x438,
	AMD_BAR45XLAT_OFFSET	= 0x440,

	/* doorbell and interrupt */
	AMD_DBFM_OFFSET		= 0x450,
	AMD_DBREQ_OFFSET	= 0x454,
	AMD_MIRRDBSTAT_OFFSET	= 0x458,
	AMD_DBMASK_OFFSET	= 0x45C,
	AMD_DBSTAT_OFFSET	= 0x460,
	AMD_INTMASK_OFFSET	= 0x470,
	AMD_INTSTAT_OFFSET	= 0x474,

	/* event type */
	AMD_PEER_FLUSH_EVENT	= BIT(0),
	AMD_PEER_RESET_EVENT	= BIT(1),
	AMD_PEER_D3_EVENT	= BIT(2),
	AMD_PEER_PMETO_EVENT	= BIT(3),
	AMD_PEER_D0_EVENT	= BIT(4),
	AMD_LINK_UP_EVENT	= BIT(5),
	AMD_LINK_DOWN_EVENT	= BIT(6),
	AMD_EVENT_INTMASK	= (AMD_PEER_FLUSH_EVENT |
				AMD_PEER_RESET_EVENT | AMD_PEER_D3_EVENT |
				AMD_PEER_PMETO_EVENT | AMD_PEER_D0_EVENT |
				AMD_LINK_UP_EVENT | AMD_LINK_DOWN_EVENT),

	AMD_PMESTAT_OFFSET	= 0x480,
	AMD_PMSGTRIG_OFFSET	= 0x490,
	AMD_LTRLATENCY_OFFSET	= 0x494,
	AMD_FLUSHTRIG_OFFSET	= 0x498,

	/* SMU register*/
	AMD_SMUACK_OFFSET	= 0x4A0,
	AMD_SINRST_OFFSET	= 0x4A4,
	AMD_RSPNUM_OFFSET	= 0x4A8,
	AMD_SMU_SPADMUTEX	= 0x4B0,
	AMD_SMU_SPADOFFSET	= 0x4B4,

	AMD_PEER_OFFSET		= 0x400,
};

struct amd_ntb_softc {
	/* ntb.c context. Do not move! Must go first! */
	void			*ntb_store;

	device_t		device;
	enum amd_ntb_conn_type	conn_type;

	struct amd_ntb_pci_bar_info	bar_info[NTB_MAX_BARS];
	struct amd_ntb_int_info	int_info[16];
	struct amd_ntb_vec	*msix_vec;
	uint16_t		allocated_interrupts;

	struct callout		hb_timer;

	struct amd_ntb_hw_info	*hw_info;
	uint8_t			spad_count;
	uint8_t			msix_vec_count;

	struct mtx		db_mask_lock;

	volatile uint32_t	ntb_ctl;
	volatile uint32_t	lnk_sta;
	volatile uint32_t	peer_sta;
	volatile uint32_t	cntl_sta;

	uint16_t		db_valid_mask;
	uint16_t		db_mask;
	uint32_t		int_mask;

	unsigned int		self_spad;
	unsigned int		peer_spad;
};

static void amd_init_side_info(struct amd_ntb_softc *ntb);
static void amd_deinit_side_info(struct amd_ntb_softc *ntb);
static int amd_ntb_detach(device_t device);

#endif
