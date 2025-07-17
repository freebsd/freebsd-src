/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "adf_gen2_hw_data.h"
#include "icp_qat_hw.h"

static u64
build_csr_ring_base_addr(bus_addr_t addr, u32 size)
{
	return BUILD_RING_BASE_ADDR(addr, size);
}

static u32
read_csr_ring_head(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_HEAD(csr_base_addr, bank, ring);
}

static void
write_csr_ring_head(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    u32 value)
{
	WRITE_CSR_RING_HEAD(csr_base_addr, bank, ring, value);
}

static u32
read_csr_ring_tail(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_TAIL(csr_base_addr, bank, ring);
}

static void
write_csr_ring_tail(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    u32 value)
{
	WRITE_CSR_RING_TAIL(csr_base_addr, bank, ring, value);
}

static u32
read_csr_e_stat(struct resource *csr_base_addr, u32 bank)
{
	return READ_CSR_E_STAT(csr_base_addr, bank);
}

static void
write_csr_ring_config(struct resource *csr_base_addr,
		      u32 bank,
		      u32 ring,
		      u32 value)
{
	WRITE_CSR_RING_CONFIG(csr_base_addr, bank, ring, value);
}

static bus_addr_t
read_csr_ring_base(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	return READ_CSR_RING_BASE(csr_base_addr, bank, ring);
}

static void
write_csr_ring_base(struct resource *csr_base_addr,
		    u32 bank,
		    u32 ring,
		    bus_addr_t addr)
{
	WRITE_CSR_RING_BASE(csr_base_addr, bank, ring, addr);
}

static void
write_csr_int_flag(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_FLAG(csr_base_addr, bank, value);
}

static void
write_csr_int_srcsel(struct resource *csr_base_addr, u32 bank)
{
	WRITE_CSR_INT_SRCSEL(csr_base_addr, bank);
}

static void
write_csr_int_col_en(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_COL_EN(csr_base_addr, bank, value);
}

static void
write_csr_int_col_ctl(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_COL_CTL(csr_base_addr, bank, value);
}

static void
write_csr_int_flag_and_col(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_INT_FLAG_AND_COL(csr_base_addr, bank, value);
}

static u32
read_csr_ring_srv_arb_en(struct resource *csr_base_addr, u32 bank)
{
	return READ_CSR_RING_SRV_ARB_EN(csr_base_addr, bank);
}

static void
write_csr_ring_srv_arb_en(struct resource *csr_base_addr, u32 bank, u32 value)
{
	WRITE_CSR_RING_SRV_ARB_EN(csr_base_addr, bank, value);
}

static u32
get_int_col_ctl_enable_mask(void)
{
	return ADF_RING_CSR_INT_COL_CTL_ENABLE;
}

void
adf_gen2_init_hw_csr_info(struct adf_hw_csr_info *csr_info)
{
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;

	csr_info->arb_enable_mask = 0xFF;

	csr_info->csr_addr_offset = ADF_RING_CSR_ADDR_OFFSET;
	csr_info->ring_bundle_size = ADF_RING_BUNDLE_SIZE;

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
	csr_ops->get_int_col_ctl_enable_mask = get_int_col_ctl_enable_mask;
}
