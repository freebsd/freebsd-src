/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2021 Intel Corporation */
/* $FreeBSD$ */
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_gen4_hw_data.h"

#define ADF_RPRESET_TIMEOUT_MS 5000
#define ADF_RPRESET_POLLING_INTERVAL 20

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
adf_gen4_init_hw_csr_info(struct adf_hw_csr_info *csr_info)
{
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;

	csr_info->arb_enable_mask = 0x1;

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

static int
reset_ring_pair(struct resource *csr, u32 bank_number)
{
	int reset_timeout = ADF_RPRESET_TIMEOUT_MS;
	const int timeout_step = ADF_RPRESET_POLLING_INTERVAL;
	u32 val;

	/* Write rpresetctl register bit#0 as 1
	 * As rpresetctl registers have no RW bits, no need to preserve
	 * values for other bits, just write bit#0
	 * NOTE: bit#12-bit#31 are WO, the write operation only takes
	 * effect when bit#1 is written 1 for pasid level reset
	 */
	ADF_CSR_WR(csr,
		   ADF_WQM_CSR_RPRESETCTL(bank_number),
		   BIT(ADF_WQM_CSR_RPRESETCTL_SHIFT));

	/* Read rpresetsts register to wait for rp reset complete */
	while (reset_timeout > 0) {
		val = ADF_CSR_RD(csr, ADF_WQM_CSR_RPRESETSTS(bank_number));
		if (val & ADF_WQM_CSR_RPRESETSTS_MASK)
			break;
		pause_ms("adfstop", timeout_step);
		reset_timeout -= timeout_step;
	}
	if (reset_timeout <= 0)
		return EFAULT;

	/* When rp reset is done, clear rpresetsts bit0 */
	ADF_CSR_WR(csr,
		   ADF_WQM_CSR_RPRESETSTS(bank_number),
		   BIT(ADF_WQM_CSR_RPRESETSTS_SHIFT));
	return 0;
}

int
adf_gen4_ring_pair_reset(struct adf_accel_dev *accel_dev, u32 bank_number)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 etr_bar_id = hw_data->get_etr_bar_id(hw_data);
	struct resource *csr;
	int ret;

	if (bank_number >= hw_data->num_banks)
		return -EINVAL;

	csr = (&GET_BARS(accel_dev)[etr_bar_id])->virt_addr;

	ret = reset_ring_pair(csr, bank_number);
	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "ring pair reset failure (timeout)\n");

	return ret;
}

static inline void
adf_gen4_unpack_ssm_wdtimer(u64 value, u32 *upper, u32 *lower)
{
	*lower = lower_32_bits(value);
	*upper = upper_32_bits(value);
}

int
adf_gen4_set_ssm_wdtimer(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u64 timer_val_pke = ADF_SSM_WDT_PKE_DEFAULT_VALUE;
	u64 timer_val = ADF_SSM_WDT_DEFAULT_VALUE;
	u32 ssm_wdt_pke_high = 0;
	u32 ssm_wdt_pke_low = 0;
	u32 ssm_wdt_high = 0;
	u32 ssm_wdt_low = 0;
	struct resource *pmisc_addr;
	struct adf_bar *pmisc;
	int pmisc_id;

	pmisc_id = hw_data->get_misc_bar_id(hw_data);
	pmisc = &GET_BARS(accel_dev)[pmisc_id];
	pmisc_addr = pmisc->virt_addr;

	/* Convert 64bit WDT timer value into 32bit values for
	 * mmio write to 32bit CSRs.
	 */
	adf_gen4_unpack_ssm_wdtimer(timer_val, &ssm_wdt_high, &ssm_wdt_low);
	adf_gen4_unpack_ssm_wdtimer(timer_val_pke,
				    &ssm_wdt_pke_high,
				    &ssm_wdt_pke_low);

	/* Enable WDT for sym and dc */
	ADF_CSR_WR(pmisc_addr, ADF_SSMWDTL_OFFSET, ssm_wdt_low);
	ADF_CSR_WR(pmisc_addr, ADF_SSMWDTH_OFFSET, ssm_wdt_high);
	/* Enable WDT for pke */
	ADF_CSR_WR(pmisc_addr, ADF_SSMWDTPKEL_OFFSET, ssm_wdt_pke_low);
	ADF_CSR_WR(pmisc_addr, ADF_SSMWDTPKEH_OFFSET, ssm_wdt_pke_high);

	return 0;
}

int
adf_pfvf_comms_disabled(struct adf_accel_dev *accel_dev)
{
	return 0;
}
