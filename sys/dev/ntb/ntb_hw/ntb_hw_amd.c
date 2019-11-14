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
 * Copyright (c) 2019 Advanced Micro Devices, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of AMD corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * Contact Information :
 * Rajesh Kumar <rajesh1.kumar@amd.com>
 */

/*
 * The Non-Transparent Bridge (NTB) is a device that allows you to connect
 * two or more systems using a PCI-e links, providing remote memory access.
 *
 * This module contains a driver for NTB hardware in AMD CPUs
 *
 * Much of the code in this module is shared with Linux. Any patches may
 * be picked up and redistributed in Linux with a dual GPL/BSD license.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "ntb_hw_amd.h"
#include "dev/ntb/ntb.h"

MALLOC_DEFINE(M_AMD_NTB, "amd_ntb_hw", "amd_ntb_hw driver memory allocations");

static const struct amd_ntb_hw_info amd_ntb_hw_info_list[] = {

	{ .vendor_id = NTB_HW_AMD_VENDOR_ID,
	  .device_id = NTB_HW_AMD_DEVICE_ID1,
	  .mw_count = 3,
	  .bar_start_idx = 1,
	  .spad_count = 16,
	  .db_count = 16,
	  .msix_vector_count = 24,
	  .quirks = QUIRK_MW0_32BIT,
	  .desc = "AMD Non-Transparent Bridge"},

	{ .vendor_id = NTB_HW_AMD_VENDOR_ID,
	  .device_id = NTB_HW_AMD_DEVICE_ID2,
	  .mw_count = 2,
	  .bar_start_idx = 2,
	  .spad_count = 16,
	  .db_count = 16,
	  .msix_vector_count = 24,
	  .quirks = 0,
	  .desc = "AMD Non-Transparent Bridge"},
};

static const struct pci_device_table amd_ntb_devs[] = {
	{ PCI_DEV(NTB_HW_AMD_VENDOR_ID, NTB_HW_AMD_DEVICE_ID1),
	  .driver_data = (uintptr_t)&amd_ntb_hw_info_list[0],
	  PCI_DESCR("AMD Non-Transparent Bridge") },
	{ PCI_DEV(NTB_HW_AMD_VENDOR_ID, NTB_HW_AMD_DEVICE_ID2),
	  .driver_data = (uintptr_t)&amd_ntb_hw_info_list[1],
	  PCI_DESCR("AMD Non-Transparent Bridge") }
};

static unsigned g_amd_ntb_hw_debug_level;
SYSCTL_UINT(_hw_ntb, OID_AUTO, debug_level, CTLFLAG_RWTUN,
    &g_amd_ntb_hw_debug_level, 0, "amd_ntb_hw log level -- higher is verbose");

#define amd_ntb_printf(lvl, ...) do {				\
        if (lvl <= g_amd_ntb_hw_debug_level)			\
                device_printf(ntb->device, __VA_ARGS__);	\
} while (0)

#ifdef __i386__
static __inline uint64_t
bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset)
{

	return (bus_space_read_4(tag, handle, offset) |
	    ((uint64_t)bus_space_read_4(tag, handle, offset + 4)) << 32);
}

static __inline void
bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t offset, uint64_t val)
{

	bus_space_write_4(tag, handle, offset, val);
	bus_space_write_4(tag, handle, offset + 4, val >> 32);
}
#endif

/*
 * AMD NTB INTERFACE ROUTINES
 */
static int
amd_ntb_port_number(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: conn_type %d\n", __func__, ntb->conn_type);

	switch (ntb->conn_type) {
	case NTB_CONN_PRI:
		return (NTB_PORT_PRI_USD);
	case NTB_CONN_SEC:
		return (NTB_PORT_SEC_DSD);
	default:
		break;
	}

	return (-EINVAL);
}

static int
amd_ntb_peer_port_count(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: peer cnt %d\n", __func__, NTB_DEF_PEER_CNT);
	return (NTB_DEF_PEER_CNT);
}

static int
amd_ntb_peer_port_number(device_t dev, int pidx)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: pidx %d conn type %d\n",
	    __func__, pidx, ntb->conn_type);

	if (pidx != NTB_DEF_PEER_IDX)
		return (-EINVAL);

	switch (ntb->conn_type) {
	case NTB_CONN_PRI:
		return (NTB_PORT_SEC_DSD);
	case NTB_CONN_SEC:
		return (NTB_PORT_PRI_USD);
	default:
		break;
	}

	return (-EINVAL);
}

static int
amd_ntb_peer_port_idx(device_t dev, int port)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	int peer_port;

	peer_port = amd_ntb_peer_port_number(dev, NTB_DEF_PEER_IDX);

	amd_ntb_printf(1, "%s: port %d peer_port %d\n",
	    __func__, port, peer_port);

	if (peer_port == -EINVAL || port != peer_port)
		return (-EINVAL);

	return (0);
}

/*
 * AMD NTB INTERFACE - LINK ROUTINES
 */
static inline int
amd_link_is_up(struct amd_ntb_softc *ntb)
{

	amd_ntb_printf(2, "%s: peer_sta 0x%x cntl_sta 0x%x\n",
	    __func__, ntb->peer_sta, ntb->cntl_sta);

	if (!ntb->peer_sta)
		return (NTB_LNK_STA_ACTIVE(ntb->cntl_sta));

	return (0);
}

static inline enum ntb_speed
amd_ntb_link_sta_speed(struct amd_ntb_softc *ntb)
{

	if (!amd_link_is_up(ntb))
		return (NTB_SPEED_NONE);

	return (NTB_LNK_STA_SPEED(ntb->lnk_sta));
}

static inline enum ntb_width
amd_ntb_link_sta_width(struct amd_ntb_softc *ntb)
{

	if (!amd_link_is_up(ntb))
		return (NTB_WIDTH_NONE);

	return (NTB_LNK_STA_WIDTH(ntb->lnk_sta));
}

static bool
amd_ntb_link_is_up(device_t dev, enum ntb_speed *speed, enum ntb_width *width)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	if (speed != NULL)
		*speed = amd_ntb_link_sta_speed(ntb);
	if (width != NULL)
		*width = amd_ntb_link_sta_width(ntb);

	return (amd_link_is_up(ntb));
}

static int
amd_ntb_link_enable(device_t dev, enum ntb_speed max_speed,
    enum ntb_width max_width)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t ntb_ctl;

	amd_ntb_printf(1, "%s: int_mask 0x%x conn_type %d\n",
	    __func__, ntb->int_mask, ntb->conn_type);

	amd_init_side_info(ntb);

	/* Enable event interrupt */
	ntb->int_mask &= ~AMD_EVENT_INTMASK;
	amd_ntb_reg_write(4, AMD_INTMASK_OFFSET, ntb->int_mask);

	if (ntb->conn_type == NTB_CONN_SEC)
		return (EINVAL);

	amd_ntb_printf(0, "%s: Enabling Link.\n", __func__);

	ntb_ctl = amd_ntb_reg_read(4, AMD_CNTL_OFFSET);
	ntb_ctl |= (PMM_REG_CTL | SMM_REG_CTL);
	amd_ntb_printf(1, "%s: ntb_ctl 0x%x\n", __func__, ntb_ctl);
	amd_ntb_reg_write(4, AMD_CNTL_OFFSET, ntb_ctl);

	return (0);
}

static int
amd_ntb_link_disable(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t ntb_ctl;

	amd_ntb_printf(1, "%s: int_mask 0x%x conn_type %d\n",
	    __func__, ntb->int_mask, ntb->conn_type);

	amd_deinit_side_info(ntb);

	/* Disable event interrupt */
	ntb->int_mask |= AMD_EVENT_INTMASK;
	amd_ntb_reg_write(4, AMD_INTMASK_OFFSET, ntb->int_mask);

	if (ntb->conn_type == NTB_CONN_SEC)
		return (EINVAL);

	amd_ntb_printf(0, "%s: Disabling Link.\n", __func__);

	ntb_ctl = amd_ntb_reg_read(4, AMD_CNTL_OFFSET);
	ntb_ctl &= ~(PMM_REG_CTL | SMM_REG_CTL);
	amd_ntb_printf(1, "%s: ntb_ctl 0x%x\n", __func__, ntb_ctl);
	amd_ntb_reg_write(4, AMD_CNTL_OFFSET, ntb_ctl);

	return (0);
}

/*
 * AMD NTB memory window routines
 */
static uint8_t
amd_ntb_mw_count(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	return (ntb->hw_info->mw_count);
}

static int
amd_ntb_mw_get_range(device_t dev, unsigned mw_idx, vm_paddr_t *base,
    caddr_t *vbase, size_t *size, size_t *align, size_t *align_size,
    bus_addr_t *plimit)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	struct amd_ntb_pci_bar_info *bar_info;

	if (mw_idx < 0 || mw_idx >= ntb->hw_info->mw_count)
		return (EINVAL);

	bar_info = &ntb->bar_info[ntb->hw_info->bar_start_idx + mw_idx];

	if (base != NULL)
		*base = bar_info->pbase;

	if (vbase != NULL)
		*vbase = bar_info->vbase;

	if (align != NULL)
		*align = bar_info->size;

	if (size != NULL)
		*size = bar_info->size;

	if (align_size != NULL)
		*align_size = 1;

	if (plimit != NULL) {
		/*
		 * For Device ID 0x145B (which has 3 memory windows),
		 * memory window 0 use a 32-bit bar. The remaining
		 * cases all use 64-bit bar.
		 */
		if ((mw_idx == 0) && (ntb->hw_info->quirks & QUIRK_MW0_32BIT))
			*plimit = BUS_SPACE_MAXADDR_32BIT;
		else
			*plimit = BUS_SPACE_MAXADDR;
	}

	return (0);
}

static int
amd_ntb_mw_set_trans(device_t dev, unsigned mw_idx, bus_addr_t addr, size_t size)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	struct amd_ntb_pci_bar_info *bar_info;

	if (mw_idx < 0 || mw_idx >= ntb->hw_info->mw_count)
		return (EINVAL);

	bar_info = &ntb->bar_info[ntb->hw_info->bar_start_idx + mw_idx];

	/* Make sure the range fits in the usable mw size. */
	if (size > bar_info->size) {
		amd_ntb_printf(0, "%s: size 0x%jx greater than mw_size 0x%jx\n",
		    __func__, (uintmax_t)size, (uintmax_t)bar_info->size);
		return (EINVAL);
	}

	amd_ntb_printf(1, "%s: mw %d mw_size 0x%jx size 0x%jx base %p\n",
	    __func__, mw_idx, (uintmax_t)bar_info->size,
	    (uintmax_t)size, (void *)bar_info->pci_bus_handle);

	/*
	 * AMD NTB XLAT and Limit registers needs to be written only after
	 * link enable.
	 *
	 * Set and verify setting the translation address register.
	 */
	amd_ntb_peer_reg_write(8, bar_info->xlat_off, (uint64_t)addr);
	amd_ntb_printf(0, "%s: mw %d xlat_off 0x%x cur_val 0x%jx addr %p\n",
	    __func__, mw_idx, bar_info->xlat_off,
	    amd_ntb_peer_reg_read(8, bar_info->xlat_off), (void *)addr);

	/*
	 * Set and verify setting the limit register.
	 *
	 * For Device ID 0x145B (which has 3 memory windows),
	 * memory window 0 use a 32-bit bar. The remaining
	 * cases all use 64-bit bar.
	 */
	if ((mw_idx == 0) && (ntb->hw_info->quirks & QUIRK_MW0_32BIT)) {
		amd_ntb_reg_write(4, bar_info->limit_off, (uint32_t)size);
		amd_ntb_printf(1, "%s: limit_off 0x%x cur_val 0x%x limit 0x%x\n",
		    __func__, bar_info->limit_off,
		    amd_ntb_peer_reg_read(4, bar_info->limit_off),
		    (uint32_t)size);
	} else {
		amd_ntb_reg_write(8, bar_info->limit_off, (uint64_t)size);
		amd_ntb_printf(1, "%s: limit_off 0x%x cur_val 0x%jx limit 0x%jx\n",
		    __func__, bar_info->limit_off,
		    amd_ntb_peer_reg_read(8, bar_info->limit_off),
		    (uintmax_t)size);
	}

	return (0);
}

static int
amd_ntb_mw_clear_trans(device_t dev, unsigned mw_idx)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: mw_idx %d\n", __func__, mw_idx);

	if (mw_idx < 0 || mw_idx >= ntb->hw_info->mw_count)
		return (EINVAL);

	return (amd_ntb_mw_set_trans(dev, mw_idx, 0, 0));
}

static int
amd_ntb_mw_set_wc(device_t dev, unsigned int mw_idx, vm_memattr_t mode)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	struct amd_ntb_pci_bar_info *bar_info;
	int rc;

	if (mw_idx < 0 || mw_idx >= ntb->hw_info->mw_count)
		return (EINVAL);

	bar_info = &ntb->bar_info[ntb->hw_info->bar_start_idx + mw_idx];
	if (mode == bar_info->map_mode)
		return (0);

	rc = pmap_change_attr((vm_offset_t)bar_info->vbase, bar_info->size, mode);
	if (rc == 0)
		bar_info->map_mode = mode;

	return (rc);
}

static int
amd_ntb_mw_get_wc(device_t dev, unsigned mw_idx, vm_memattr_t *mode)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	struct amd_ntb_pci_bar_info *bar_info;

	amd_ntb_printf(1, "%s: mw_idx %d\n", __func__, mw_idx);

	if (mw_idx < 0 || mw_idx >= ntb->hw_info->mw_count)
		return (EINVAL);

	bar_info = &ntb->bar_info[ntb->hw_info->bar_start_idx + mw_idx];
	*mode = bar_info->map_mode;

	return (0);
}

/*
 * AMD NTB doorbell routines
 */
static int
amd_ntb_db_vector_count(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: db_count 0x%x\n", __func__,
	    ntb->hw_info->db_count);

	return (ntb->hw_info->db_count);
}

static uint64_t
amd_ntb_db_valid_mask(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: db_valid_mask 0x%x\n",
	    __func__, ntb->db_valid_mask);

	return (ntb->db_valid_mask);
}

static uint64_t
amd_ntb_db_vector_mask(device_t dev, uint32_t vector)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: vector %d db_count 0x%x db_valid_mask 0x%x\n",
	    __func__, vector, ntb->hw_info->db_count, ntb->db_valid_mask);

	if (vector < 0 || vector >= ntb->hw_info->db_count)
		return (0);

	return (ntb->db_valid_mask & (1 << vector));
}

static uint64_t
amd_ntb_db_read(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint64_t dbstat_off;

	dbstat_off = (uint64_t)amd_ntb_reg_read(2, AMD_DBSTAT_OFFSET);

	amd_ntb_printf(1, "%s: dbstat_off 0x%jx\n", __func__, dbstat_off);

	return (dbstat_off);
}

static void
amd_ntb_db_clear(device_t dev, uint64_t db_bits)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: db_bits 0x%jx\n", __func__, db_bits);
	amd_ntb_reg_write(2, AMD_DBSTAT_OFFSET, (uint16_t)db_bits);
}

static void
amd_ntb_db_set_mask(device_t dev, uint64_t db_bits)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	DB_MASK_LOCK(ntb);
	amd_ntb_printf(1, "%s: db_mask 0x%x db_bits 0x%jx\n",
	    __func__, ntb->db_mask, db_bits);

	ntb->db_mask |= db_bits;
	amd_ntb_reg_write(2, AMD_DBMASK_OFFSET, ntb->db_mask);
	DB_MASK_UNLOCK(ntb);
}

static void
amd_ntb_db_clear_mask(device_t dev, uint64_t db_bits)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	DB_MASK_LOCK(ntb);
	amd_ntb_printf(1, "%s: db_mask 0x%x db_bits 0x%jx\n",
	    __func__, ntb->db_mask, db_bits);

	ntb->db_mask &= ~db_bits;
	amd_ntb_reg_write(2, AMD_DBMASK_OFFSET, ntb->db_mask);
	DB_MASK_UNLOCK(ntb);
}

static void
amd_ntb_peer_db_set(device_t dev, uint64_t db_bits)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: db_bits 0x%jx\n", __func__, db_bits);
	amd_ntb_reg_write(2, AMD_DBREQ_OFFSET, (uint16_t)db_bits);
}

/*
 * AMD NTB scratchpad routines
 */
static uint8_t
amd_ntb_spad_count(device_t dev)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);

	amd_ntb_printf(1, "%s: spad_count 0x%x\n", __func__,
	    ntb->spad_count);

	return (ntb->spad_count);
}

static int
amd_ntb_spad_read(device_t dev, unsigned int idx, uint32_t *val)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t offset;

	amd_ntb_printf(2, "%s: idx %d\n", __func__, idx);

	if (idx < 0 || idx >= ntb->spad_count)
		return (EINVAL);

	offset = ntb->self_spad + (idx << 2);
	*val = amd_ntb_reg_read(4, AMD_SPAD_OFFSET + offset);
	amd_ntb_printf(2, "%s: offset 0x%x val 0x%x\n", __func__, offset, *val);

	return (0);
}

static int
amd_ntb_spad_write(device_t dev, unsigned int idx, uint32_t val)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t offset;

	amd_ntb_printf(2, "%s: idx %d\n", __func__, idx);

	if (idx < 0 || idx >= ntb->spad_count)
		return (EINVAL);

	offset = ntb->self_spad + (idx << 2);
	amd_ntb_reg_write(4, AMD_SPAD_OFFSET + offset, val);
	amd_ntb_printf(2, "%s: offset 0x%x val 0x%x\n", __func__, offset, val);

	return (0);
}

static void
amd_ntb_spad_clear(struct amd_ntb_softc *ntb)
{
	uint8_t i;

	for (i = 0; i < ntb->spad_count; i++)
		amd_ntb_spad_write(ntb->device, i, 0);
}

static int
amd_ntb_peer_spad_read(device_t dev, unsigned int idx, uint32_t *val)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t offset;

	amd_ntb_printf(2, "%s: idx %d\n", __func__, idx);

	if (idx < 0 || idx >= ntb->spad_count)
		return (EINVAL);

	offset = ntb->peer_spad + (idx << 2);
	*val = amd_ntb_reg_read(4, AMD_SPAD_OFFSET + offset);
	amd_ntb_printf(2, "%s: offset 0x%x val 0x%x\n", __func__, offset, *val);

	return (0);
}

static int
amd_ntb_peer_spad_write(device_t dev, unsigned int idx, uint32_t val)
{
	struct amd_ntb_softc *ntb = device_get_softc(dev);
	uint32_t offset;

	amd_ntb_printf(2, "%s: idx %d\n", __func__, idx);

	if (idx < 0 || idx >= ntb->spad_count)
		return (EINVAL);

	offset = ntb->peer_spad + (idx << 2);
	amd_ntb_reg_write(4, AMD_SPAD_OFFSET + offset, val);
	amd_ntb_printf(2, "%s: offset 0x%x val 0x%x\n", __func__, offset, val);

	return (0);
}


/*
 * AMD NTB INIT
 */
static int
amd_ntb_hw_info_handler(SYSCTL_HANDLER_ARGS)
{
	struct amd_ntb_softc* ntb = arg1;
	struct sbuf *sb;
	int rc = 0;

	sb = sbuf_new_for_sysctl(NULL, NULL, 4096, req);
	if (sb == NULL)
		return (sb->s_error);

	sbuf_printf(sb, "NTB AMD Hardware info:\n\n");
	sbuf_printf(sb, "AMD NTB side: %s\n",
	    (ntb->conn_type == NTB_CONN_PRI)? "PRIMARY" : "SECONDARY");
	sbuf_printf(sb, "AMD LNK STA: 0x%#06x\n", ntb->lnk_sta);

	if (!amd_link_is_up(ntb))
		sbuf_printf(sb, "AMD Link Status: Down\n");
	else {
		sbuf_printf(sb, "AMD Link Status: Up\n");
		sbuf_printf(sb, "AMD Link Speed: PCI-E Gen %u\n",
		    NTB_LNK_STA_SPEED(ntb->lnk_sta));
		sbuf_printf(sb, "AMD Link Width: PCI-E Width %u\n",
		    NTB_LNK_STA_WIDTH(ntb->lnk_sta));
	}

	sbuf_printf(sb, "AMD Memory window count: %d\n",
	    ntb->hw_info->mw_count);
	sbuf_printf(sb, "AMD Spad count: %d\n",
	    ntb->spad_count);
	sbuf_printf(sb, "AMD Doorbell count: %d\n",
	    ntb->hw_info->db_count);
	sbuf_printf(sb, "AMD MSI-X vec count: %d\n\n",
	    ntb->msix_vec_count);
	sbuf_printf(sb, "AMD Doorbell valid mask: 0x%x\n",
	    ntb->db_valid_mask);
	sbuf_printf(sb, "AMD Doorbell Mask: 0x%x\n",
	    amd_ntb_reg_read(4, AMD_DBMASK_OFFSET));
	sbuf_printf(sb, "AMD Doorbell: 0x%x\n",
	    amd_ntb_reg_read(4, AMD_DBSTAT_OFFSET));
	sbuf_printf(sb, "AMD NTB Incoming XLAT: \n");
	sbuf_printf(sb, "AMD XLAT1: 0x%jx\n",
	    amd_ntb_peer_reg_read(8, AMD_BAR1XLAT_OFFSET));
	sbuf_printf(sb, "AMD XLAT23: 0x%jx\n",
	    amd_ntb_peer_reg_read(8, AMD_BAR23XLAT_OFFSET));
	sbuf_printf(sb, "AMD XLAT45: 0x%jx\n",
	    amd_ntb_peer_reg_read(8, AMD_BAR45XLAT_OFFSET));
	sbuf_printf(sb, "AMD LMT1: 0x%x\n",
	    amd_ntb_reg_read(4, AMD_BAR1LMT_OFFSET));
	sbuf_printf(sb, "AMD LMT23: 0x%jx\n",
	    amd_ntb_reg_read(8, AMD_BAR23LMT_OFFSET));
	sbuf_printf(sb, "AMD LMT45: 0x%jx\n",
	    amd_ntb_reg_read(8, AMD_BAR45LMT_OFFSET));

	rc = sbuf_finish(sb);
	sbuf_delete(sb);
	return (rc);
}

static void
amd_ntb_sysctl_init(struct amd_ntb_softc *ntb)
{
	struct sysctl_oid_list *globals;
	struct sysctl_ctx_list *ctx;

	ctx = device_get_sysctl_ctx(ntb->device);
	globals = SYSCTL_CHILDREN(device_get_sysctl_tree(ntb->device));

	SYSCTL_ADD_PROC(ctx, globals, OID_AUTO, "info",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, ntb, 0,
	    amd_ntb_hw_info_handler, "A", "AMD NTB HW Information");
}

/*
 * Polls the HW link status register(s); returns true if something has changed.
 */
static bool
amd_ntb_poll_link(struct amd_ntb_softc *ntb)
{
	uint32_t fullreg, reg, stat;

	fullreg = amd_ntb_peer_reg_read(4, AMD_SIDEINFO_OFFSET);
	reg = fullreg & NTB_LIN_STA_ACTIVE_BIT;

	if (reg == ntb->cntl_sta)
		return (false);

	amd_ntb_printf(0, "%s: SIDEINFO reg_val = 0x%x cntl_sta 0x%x\n",
	    __func__, fullreg, ntb->cntl_sta);

	ntb->cntl_sta = reg;

	stat = pci_read_config(ntb->device, AMD_LINK_STATUS_OFFSET, 4);

	amd_ntb_printf(0, "%s: LINK_STATUS stat = 0x%x lnk_sta 0x%x.\n",
	    __func__, stat, ntb->lnk_sta);

	ntb->lnk_sta = stat;

	return (true);
}

static void
amd_link_hb(void *arg)
{
	struct amd_ntb_softc *ntb = arg;

	if (amd_ntb_poll_link(ntb))
		ntb_link_event(ntb->device);

	if (!amd_link_is_up(ntb)) {
		callout_reset(&ntb->hb_timer, AMD_LINK_HB_TIMEOUT,
		    amd_link_hb, ntb);
	} else {
		callout_reset(&ntb->hb_timer, (AMD_LINK_HB_TIMEOUT * 10),
		    amd_link_hb, ntb);
	}
}

static void
amd_ntb_interrupt(struct amd_ntb_softc *ntb, uint16_t vec)
{
	if (vec < ntb->hw_info->db_count)
		ntb_db_event(ntb->device, vec);
	else
		amd_ntb_printf(0, "Invalid vector %d\n", vec);
}

static void
amd_ntb_vec_isr(void *arg)
{
	struct amd_ntb_vec *nvec = arg;

	amd_ntb_interrupt(nvec->ntb, nvec->num);
}

static void
amd_ntb_irq_isr(void *arg)
{
	/* If we couldn't set up MSI-X, we only have the one vector. */
	amd_ntb_interrupt(arg, 0);
}

static void
amd_init_side_info(struct amd_ntb_softc *ntb)
{
	unsigned int reg;

	reg = amd_ntb_reg_read(4, AMD_SIDEINFO_OFFSET);
	if (!(reg & AMD_SIDE_READY)) {
		reg |= AMD_SIDE_READY;
		amd_ntb_reg_write(4, AMD_SIDEINFO_OFFSET, reg);
	}
	reg = amd_ntb_reg_read(4, AMD_SIDEINFO_OFFSET);
}

static void
amd_deinit_side_info(struct amd_ntb_softc *ntb)
{
	unsigned int reg;

	reg = amd_ntb_reg_read(4, AMD_SIDEINFO_OFFSET);
	if (reg & AMD_SIDE_READY) {
		reg &= ~AMD_SIDE_READY;
		amd_ntb_reg_write(4, AMD_SIDEINFO_OFFSET, reg);
		amd_ntb_reg_read(4, AMD_SIDEINFO_OFFSET);
	}
}

static int
amd_ntb_setup_isr(struct amd_ntb_softc *ntb, uint16_t num_vectors, bool msi,
    bool intx)
{
	uint16_t i;
	int flags = 0, rc = 0;

	flags |= RF_ACTIVE;
	if (intx)
		flags |= RF_SHAREABLE;

	for (i = 0; i < num_vectors; i++) {

		/* RID should be 0 for intx */
		if (intx)
			ntb->int_info[i].rid = i;
		else
			ntb->int_info[i].rid = i + 1;

		ntb->int_info[i].res = bus_alloc_resource_any(ntb->device,
		    SYS_RES_IRQ, &ntb->int_info[i].rid, flags);
		if (ntb->int_info[i].res == NULL) {
			amd_ntb_printf(0, "bus_alloc_resource IRQ failed\n");
			return (ENOMEM);
		}

		ntb->int_info[i].tag = NULL;
		ntb->allocated_interrupts++;

		if (msi || intx) {
			rc = bus_setup_intr(ntb->device, ntb->int_info[i].res,
			    INTR_MPSAFE | INTR_TYPE_MISC, NULL, amd_ntb_irq_isr,
			    ntb, &ntb->int_info[i].tag);
		} else {
			rc = bus_setup_intr(ntb->device, ntb->int_info[i].res,
			    INTR_MPSAFE | INTR_TYPE_MISC, NULL, amd_ntb_vec_isr,
			    &ntb->msix_vec[i], &ntb->int_info[i].tag);
		}

		if (rc != 0) {
			amd_ntb_printf(0, "bus_setup_intr %d failed\n", i);
			return (ENXIO);
		}
	}

	return (0);
}

static int
amd_ntb_create_msix_vec(struct amd_ntb_softc *ntb, uint32_t max_vectors)
{
	uint8_t i;
	
	ntb->msix_vec = malloc(max_vectors * sizeof(*ntb->msix_vec), M_AMD_NTB,
	    M_ZERO | M_WAITOK);

	for (i = 0; i < max_vectors; i++) {
		ntb->msix_vec[i].num = i;
		ntb->msix_vec[i].ntb = ntb;
	}

	return (0);
}

static void
amd_ntb_free_msix_vec(struct amd_ntb_softc *ntb)
{
	if (ntb->msix_vec_count) {
		pci_release_msi(ntb->device);
		ntb->msix_vec_count = 0;
	}

	if (ntb->msix_vec != NULL) {
		free(ntb->msix_vec, M_AMD_NTB);
		ntb->msix_vec = NULL;
	}
}

static int
amd_ntb_init_isr(struct amd_ntb_softc *ntb)
{
	uint32_t supported_vectors, num_vectors;
	bool msi = false, intx = false;
	int rc = 0;

	ntb->db_mask = ntb->db_valid_mask;

	rc = amd_ntb_create_msix_vec(ntb, ntb->hw_info->msix_vector_count);
	if (rc != 0) {
		amd_ntb_printf(0, "Error creating msix vectors: %d\n", rc);
		return (ENOMEM);
	}

	/*
	 * Check the number of MSI-X message supported by the device.
	 * Minimum necessary MSI-X message count should be equal to db_count.
	 */
	supported_vectors = pci_msix_count(ntb->device);
	num_vectors = MIN(supported_vectors, ntb->hw_info->db_count);
	if (num_vectors < ntb->hw_info->db_count) {
		amd_ntb_printf(0, "No minimum msix: supported %d db %d\n",
		    supported_vectors, ntb->hw_info->db_count);
		msi = true;
		goto err_msix_enable;
	}

	/* Allocate the necessary number of MSI-x messages */
	rc = pci_alloc_msix(ntb->device, &num_vectors);
	if (rc != 0) {
		amd_ntb_printf(0, "Error allocating msix vectors: %d\n", rc);
		msi = true;
		goto err_msix_enable;
	}

	if (num_vectors < ntb->hw_info->db_count) {
		amd_ntb_printf(0, "Allocated only %d MSI-X\n", num_vectors);
		msi = true;
		/*
		 * Else set ntb->hw_info->db_count = ntb->msix_vec_count =
		 * num_vectors, msi=false and dont release msi.
		 */
	}

err_msix_enable:

	if (msi) {
		free(ntb->msix_vec, M_AMD_NTB);
		ntb->msix_vec = NULL;
		pci_release_msi(ntb->device);
		num_vectors = 1;
		rc = pci_alloc_msi(ntb->device, &num_vectors);
		if (rc != 0) {
			amd_ntb_printf(0, "Error allocating msix vectors: %d\n", rc);
			msi = false;
			intx = true;
		}
	}

	ntb->hw_info->db_count = ntb->msix_vec_count = num_vectors;

	if (intx) {
		num_vectors = 1;
		ntb->hw_info->db_count = 1;
		ntb->msix_vec_count = 0;
	}

	amd_ntb_printf(0, "%s: db %d msix %d msi %d intx %d\n",
	    __func__, ntb->hw_info->db_count, ntb->msix_vec_count, (int)msi, (int)intx);

	rc = amd_ntb_setup_isr(ntb, num_vectors, msi, intx);
	if (rc != 0) {
		amd_ntb_printf(0, "Error setting up isr: %d\n", rc);
		amd_ntb_free_msix_vec(ntb);
	}

	return (rc);
}

static void
amd_ntb_deinit_isr(struct amd_ntb_softc *ntb)
{
	struct amd_ntb_int_info *current_int;
	int i;

	/* Mask all doorbell interrupts */
	ntb->db_mask = ntb->db_valid_mask;
	amd_ntb_reg_write(4, AMD_DBMASK_OFFSET, ntb->db_mask);

	for (i = 0; i < ntb->allocated_interrupts; i++) {
		current_int = &ntb->int_info[i];
		if (current_int->tag != NULL)
			bus_teardown_intr(ntb->device, current_int->res,
			    current_int->tag);

		if (current_int->res != NULL)
			bus_release_resource(ntb->device, SYS_RES_IRQ,
			    rman_get_rid(current_int->res), current_int->res);
	}

	amd_ntb_free_msix_vec(ntb);
}

static enum amd_ntb_conn_type
amd_ntb_get_topo(struct amd_ntb_softc *ntb)
{
	uint32_t info;

	info = amd_ntb_reg_read(4, AMD_SIDEINFO_OFFSET);

	if (info & AMD_SIDE_MASK)
		return (NTB_CONN_SEC);

	return (NTB_CONN_PRI);
}

static int
amd_ntb_init_dev(struct amd_ntb_softc *ntb)
{
	ntb->db_valid_mask	 = (1ull << ntb->hw_info->db_count) - 1;
	mtx_init(&ntb->db_mask_lock, "amd ntb db bits", NULL, MTX_SPIN);

	switch (ntb->conn_type) {
	case NTB_CONN_PRI:
	case NTB_CONN_SEC:
		ntb->spad_count >>= 1;

		if (ntb->conn_type == NTB_CONN_PRI) {
			ntb->self_spad = 0;
			ntb->peer_spad = 0x20;
		} else {
			ntb->self_spad = 0x20;
			ntb->peer_spad = 0;
		}

		callout_init(&ntb->hb_timer, 1);
		callout_reset(&ntb->hb_timer, AMD_LINK_HB_TIMEOUT,
		    amd_link_hb, ntb);

		break;

	default:
		amd_ntb_printf(0, "Unsupported AMD NTB topology %d\n",
		    ntb->conn_type);
		return (EINVAL);
	}

	ntb->int_mask = AMD_EVENT_INTMASK;
	amd_ntb_reg_write(4, AMD_INTMASK_OFFSET, ntb->int_mask);

	return (0);
}

static int
amd_ntb_init(struct amd_ntb_softc *ntb)
{
	int rc = 0;

	ntb->conn_type = amd_ntb_get_topo(ntb);
	amd_ntb_printf(0, "AMD NTB Side: %s\n",
	    (ntb->conn_type == NTB_CONN_PRI)? "PRIMARY" : "SECONDARY");

	rc = amd_ntb_init_dev(ntb);
	if (rc != 0)
		return (rc);

	rc = amd_ntb_init_isr(ntb);
	if (rc != 0)
		return (rc);

	return (0);
}

static void
print_map_success(struct amd_ntb_softc *ntb, struct amd_ntb_pci_bar_info *bar,
    const char *kind)
{
	amd_ntb_printf(0, "Mapped BAR%d v:[%p-%p] p:[%p-%p] (0x%jx bytes) (%s)\n",
	    PCI_RID2BAR(bar->pci_resource_id), bar->vbase,
	    (char *)bar->vbase + bar->size - 1, (void *)bar->pbase,
	    (void *)(bar->pbase + bar->size - 1), (uintmax_t)bar->size, kind);
}

static void
save_bar_parameters(struct amd_ntb_pci_bar_info *bar)
{
	bar->pci_bus_tag = rman_get_bustag(bar->pci_resource);
	bar->pci_bus_handle = rman_get_bushandle(bar->pci_resource);
	bar->pbase = rman_get_start(bar->pci_resource);
	bar->size = rman_get_size(bar->pci_resource);
	bar->vbase = rman_get_virtual(bar->pci_resource);
	bar->map_mode = VM_MEMATTR_UNCACHEABLE;
}

static int
map_bar(struct amd_ntb_softc *ntb, struct amd_ntb_pci_bar_info *bar)
{
	bar->pci_resource = bus_alloc_resource_any(ntb->device, SYS_RES_MEMORY,
	    &bar->pci_resource_id, RF_ACTIVE);
	if (bar->pci_resource == NULL)
		return (ENXIO);

	save_bar_parameters(bar);
	print_map_success(ntb, bar, "mmr");

	return (0);
}

static int
amd_ntb_map_pci_bars(struct amd_ntb_softc *ntb)
{
	int rc = 0;

	/* NTB Config/Control registers - BAR 0 */
	ntb->bar_info[NTB_CONFIG_BAR].pci_resource_id = PCIR_BAR(0);
	rc = map_bar(ntb, &ntb->bar_info[NTB_CONFIG_BAR]);
	if (rc != 0)
		goto out;

	/* Memory Window 0 BAR - BAR 1 */
	ntb->bar_info[NTB_BAR_1].pci_resource_id = PCIR_BAR(1);
	rc = map_bar(ntb, &ntb->bar_info[NTB_BAR_1]);
	if (rc != 0)
		goto out;
	ntb->bar_info[NTB_BAR_1].xlat_off = AMD_BAR1XLAT_OFFSET;
	ntb->bar_info[NTB_BAR_1].limit_off = AMD_BAR1LMT_OFFSET;

	/* Memory Window 1 BAR - BAR 2&3 */
	ntb->bar_info[NTB_BAR_2].pci_resource_id = PCIR_BAR(2);
	rc = map_bar(ntb, &ntb->bar_info[NTB_BAR_2]);
	if (rc != 0)
		goto out;
	ntb->bar_info[NTB_BAR_2].xlat_off = AMD_BAR23XLAT_OFFSET;
	ntb->bar_info[NTB_BAR_2].limit_off = AMD_BAR23LMT_OFFSET;

	/* Memory Window 2 BAR - BAR 4&5 */
	ntb->bar_info[NTB_BAR_3].pci_resource_id = PCIR_BAR(4);
	rc = map_bar(ntb, &ntb->bar_info[NTB_BAR_3]);
	if (rc != 0)
		goto out;
	ntb->bar_info[NTB_BAR_3].xlat_off = AMD_BAR45XLAT_OFFSET;
	ntb->bar_info[NTB_BAR_3].limit_off = AMD_BAR45LMT_OFFSET;

out:
	if (rc != 0)
		amd_ntb_printf(0, "unable to allocate pci resource\n");

	return (rc);
}

static void
amd_ntb_unmap_pci_bars(struct amd_ntb_softc *ntb)
{
	struct amd_ntb_pci_bar_info *bar_info;
	int i;

	for (i = 0; i < NTB_MAX_BARS; i++) {
		bar_info = &ntb->bar_info[i];
		if (bar_info->pci_resource != NULL)
			bus_release_resource(ntb->device, SYS_RES_MEMORY,
			    bar_info->pci_resource_id, bar_info->pci_resource);
	}
}

static int
amd_ntb_probe(device_t device)
{
	struct amd_ntb_softc *ntb = device_get_softc(device);
	const struct pci_device_table *tbl;

	tbl = PCI_MATCH(device, amd_ntb_devs);
	if (tbl == NULL)
		return (ENXIO);

	ntb->hw_info = (struct amd_ntb_hw_info *)tbl->driver_data;
	ntb->spad_count = ntb->hw_info->spad_count;
	device_set_desc(device, tbl->descr);

	return (BUS_PROBE_GENERIC);
}

static int
amd_ntb_attach(device_t device)
{
	struct amd_ntb_softc *ntb = device_get_softc(device);
	int error;

	ntb->device = device;

	/* Enable PCI bus mastering for "device" */
	pci_enable_busmaster(ntb->device);

	error = amd_ntb_map_pci_bars(ntb);
	if (error)
		goto out;
		
	error = amd_ntb_init(ntb);
	if (error)
		goto out;

	amd_init_side_info(ntb);

	amd_ntb_spad_clear(ntb);

	amd_ntb_sysctl_init(ntb);

	/* Attach children to this controller */
	error = ntb_register_device(device);

out:
	if (error)
		amd_ntb_detach(device);

	return (error);
}

static int
amd_ntb_detach(device_t device)
{
	struct amd_ntb_softc *ntb = device_get_softc(device);

	ntb_unregister_device(device);
	amd_deinit_side_info(ntb);
	callout_drain(&ntb->hb_timer);
	amd_ntb_deinit_isr(ntb);
	mtx_destroy(&ntb->db_mask_lock);
	pci_disable_busmaster(ntb->device);
	amd_ntb_unmap_pci_bars(ntb);

	return (0);
}

static device_method_t ntb_amd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		amd_ntb_probe),
	DEVMETHOD(device_attach,	amd_ntb_attach),
	DEVMETHOD(device_detach,	amd_ntb_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_location_str, ntb_child_location_str),
	DEVMETHOD(bus_print_child,	ntb_print_child),
	DEVMETHOD(bus_get_dma_tag,	ntb_get_dma_tag),

	/* NTB interface */
	DEVMETHOD(ntb_port_number,	amd_ntb_port_number),
	DEVMETHOD(ntb_peer_port_count,	amd_ntb_peer_port_count),
	DEVMETHOD(ntb_peer_port_number,	amd_ntb_peer_port_number),
	DEVMETHOD(ntb_peer_port_idx, 	amd_ntb_peer_port_idx),
	DEVMETHOD(ntb_link_is_up,	amd_ntb_link_is_up),
	DEVMETHOD(ntb_link_enable,	amd_ntb_link_enable),
	DEVMETHOD(ntb_link_disable,	amd_ntb_link_disable),
	DEVMETHOD(ntb_mw_count,		amd_ntb_mw_count),
	DEVMETHOD(ntb_mw_get_range,	amd_ntb_mw_get_range),
	DEVMETHOD(ntb_mw_set_trans,	amd_ntb_mw_set_trans),
	DEVMETHOD(ntb_mw_clear_trans,	amd_ntb_mw_clear_trans),
	DEVMETHOD(ntb_mw_set_wc,	amd_ntb_mw_set_wc),
	DEVMETHOD(ntb_mw_get_wc,	amd_ntb_mw_get_wc),
	DEVMETHOD(ntb_db_valid_mask,	amd_ntb_db_valid_mask),
	DEVMETHOD(ntb_db_vector_count,	amd_ntb_db_vector_count),
	DEVMETHOD(ntb_db_vector_mask,	amd_ntb_db_vector_mask),
	DEVMETHOD(ntb_db_read,		amd_ntb_db_read),
	DEVMETHOD(ntb_db_clear,		amd_ntb_db_clear),
	DEVMETHOD(ntb_db_set_mask,	amd_ntb_db_set_mask),
	DEVMETHOD(ntb_db_clear_mask,	amd_ntb_db_clear_mask),
	DEVMETHOD(ntb_peer_db_set,	amd_ntb_peer_db_set),
	DEVMETHOD(ntb_spad_count,	amd_ntb_spad_count),
	DEVMETHOD(ntb_spad_read,	amd_ntb_spad_read),
	DEVMETHOD(ntb_spad_write,	amd_ntb_spad_write),
	DEVMETHOD(ntb_peer_spad_read,	amd_ntb_peer_spad_read),
	DEVMETHOD(ntb_peer_spad_write,	amd_ntb_peer_spad_write),
	DEVMETHOD_END
};

static DEFINE_CLASS_0(ntb_hw, ntb_amd_driver, ntb_amd_methods,
    sizeof(struct amd_ntb_softc));
DRIVER_MODULE(ntb_hw_amd, pci, ntb_amd_driver, ntb_hw_devclass, NULL, NULL);
MODULE_DEPEND(ntb_hw_amd, ntb, 1, 1, 1);
MODULE_VERSION(ntb_hw_amd, 1);
PCI_PNP_INFO(amd_ntb_devs);
