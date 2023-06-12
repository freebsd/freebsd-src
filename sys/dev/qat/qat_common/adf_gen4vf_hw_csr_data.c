/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_accel_devices.h"
#include "adf_gen4vf_hw_csr_data.h"

static u64
build_csr_ring_base_addr(dma_addr_t addr, u32 size)
{
	return BUILD_RING_BASE_ADDR_GEN4(addr, size);
}

static u32
read_csr_ring_head(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_HEAD_GEN4VF(csr_base_addr, bank, ring);
}

static void
write_csr_ring_head(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    u32 value)
{
	WRITE_CSR_RING_HEAD_GEN4VF(csr_base_addr, bank, ring, value);
}

static u32
read_csr_ring_tail(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_TAIL_GEN4VF(csr_base_addr, bank, ring);
}

static void
write_csr_ring_tail(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    u32 value)
{
	WRITE_CSR_RING_TAIL_GEN4VF(csr_base_addr, bank, ring, value);
}

static u32
read_csr_e_stat(struct resource *csr_base_addr, u32 bank)
{
	return READ_CSR_E_STAT_GEN4VF(csr_base_addr, bank);
}

static void
write_csr_ring_config(struct resource *csr_base_addr,
		      u32 bank,
		      u32 ring,
		      u32 value)
{
	WRITE_CSR_RING_CONFIG_GEN4VF(csr_base_addr, bank, ring, value);
}

static dma_addr_t
read_csr_ring_base(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_BASE_GEN4VF(csr_base_addr, bank, ring);
}

static void
write_csr_ring_base(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    dma_addr_t addr)
{
	WRITE_CSR_RING_BASE_GEN4VF(csr_base_addr, bank, ring, addr);
}

static void
write_csr_int_flag(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_FLAG_GEN4VF(csr_base_addr, bank, value);
}

static void
write_csr_int_srcsel(struct resource *csr_base_addr, u32 bank)
{
	WRITE_CSR_INT_SRCSEL_GEN4VF(csr_base_addr, bank);
}

static void
write_csr_int_col_en(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_COL_EN_GEN4VF(csr_base_addr, bank, value);
}

static void
write_csr_int_col_ctl(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_COL_CTL_GEN4VF(csr_base_addr, bank, value);
}

static void
write_csr_int_flag_and_col(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_FLAG_AND_COL_GEN4VF(csr_base_addr, bank, value);
}

static u32
read_csr_ring_srv_arb_en(struct resource *csr_base_addr, u32 bank)
{
	return READ_CSR_RING_SRV_ARB_EN_GEN4VF(csr_base_addr, bank);
}

static void
write_csr_ring_srv_arb_en(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_RING_SRV_ARB_EN_GEN4VF(csr_base_addr, bank, value);
}

static u32
get_src_sel_mask(void)
{
	return ADF_BANK_INT_SRC_SEL_MASK_GEN4;
}

static u32
get_int_col_ctl_enable_mask(void)
{
	return ADF_RING_CSR_INT_COL_CTL_ENABLE;
}

static u32
get_bank_irq_mask(u32 irq_mask)
{
	return 0x1;
}

void
gen4vf_init_hw_csr_info(struct adf_hw_csr_info *csr_info)
{
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;

	csr_info->csr_addr_offset = ADF_RING_CSR_ADDR_OFFSET_GEN4VF;
	csr_info->ring_bundle_size = ADF_RING_BUNDLE_SIZE_GEN4;
	csr_info->bank_int_flag_clear_mask = ADF_BANK_INT_FLAG_CLEAR_MASK_GEN4;
	csr_info->num_rings_per_int_srcsel = ADF_RINGS_PER_INT_SRCSEL_GEN4;
	csr_info->arb_enable_mask = 0x1;
	csr_ops->build_csr_ring_base_addr = build_csr_ring_base_addr;
	csr_ops->read_csr_ring_head = read_csr_ring_head;
	csr_ops->write_csr_ring_head = write_csr_ring_head;
	csr_ops->read_csr_ring_tail = read_csr_ring_tail;
	csr_ops->write_csr_ring_tail = write_csr_ring_tail;
	csr_ops->read_csr_e_stat = read_csr_e_stat;
	csr_ops->write_csr_ring_config = write_csr_ring_config;
	csr_ops->read_csr_ring_base = read_csr_ring_base;
	csr_ops->write_csr_ring_base = write_csr_ring_base;
	csr_ops->write_csr_int_flag = write_csr_int_flag;
	csr_ops->write_csr_int_srcsel = write_csr_int_srcsel;
	csr_ops->write_csr_int_col_en = write_csr_int_col_en;
	csr_ops->write_csr_int_col_ctl = write_csr_int_col_ctl;
	csr_ops->write_csr_int_flag_and_col = write_csr_int_flag_and_col;
	csr_ops->read_csr_ring_srv_arb_en = read_csr_ring_srv_arb_en;
	csr_ops->write_csr_ring_srv_arb_en = write_csr_ring_srv_arb_en;
	csr_ops->get_src_sel_mask = get_src_sel_mask;
	csr_ops->get_int_col_ctl_enable_mask = get_int_col_ctl_enable_mask;
	csr_ops->get_bank_irq_mask = get_bank_irq_mask;
}
