/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_cfg.h>
#include <adf_pfvf_msg.h>
#include <adf_dev_err.h>
#include <adf_gen2_hw_data.h>
#include <adf_gen2_pfvf.h>
#include "adf_c3xxx_hw_data.h"
#include "icp_qat_hw.h"
#include "adf_heartbeat.h"

/* Worker thread to service arbiter mappings */
static const u32 thrd_to_arb_map[ADF_C3XXX_MAX_ACCELENGINES] =
    { 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA };

enum { DEV_C3XXX_SKU_1 = 0, DEV_C3XXX_SKU_2 = 1, DEV_C3XXX_SKU_3 = 2 };

static u32 thrd_to_arb_map_gen[ADF_C3XXX_MAX_ACCELENGINES] = { 0 };

static struct adf_hw_device_class c3xxx_class = {.name = ADF_C3XXX_DEVICE_NAME,
						 .type = DEV_C3XXX,
						 .instances = 0 };

static u32
get_accel_mask(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;

	u32 fuse;
	u32 straps;

	fuse = pci_read_config(pdev, ADF_DEVICE_FUSECTL_OFFSET, 4);
	straps = pci_read_config(pdev, ADF_C3XXX_SOFTSTRAP_CSR_OFFSET, 4);

	return (~(fuse | straps)) >> ADF_C3XXX_ACCELERATORS_REG_OFFSET &
	    ADF_C3XXX_ACCELERATORS_MASK;
}

static u32
get_ae_mask(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fuse;
	u32 me_straps;
	u32 me_disable;
	u32 ssms_disabled;

	fuse = pci_read_config(pdev, ADF_DEVICE_FUSECTL_OFFSET, 4);
	me_straps = pci_read_config(pdev, ADF_C3XXX_SOFTSTRAP_CSR_OFFSET, 4);

	/* If SSMs are disabled, then disable the corresponding MEs */
	ssms_disabled =
	    (~get_accel_mask(accel_dev)) & ADF_C3XXX_ACCELERATORS_MASK;
	me_disable = 0x3;
	while (ssms_disabled) {
		if (ssms_disabled & 1)
			me_straps |= me_disable;
		ssms_disabled >>= 1;
		me_disable <<= 2;
	}

	return (~(fuse | me_straps)) & ADF_C3XXX_ACCELENGINES_MASK;
}

static u32
get_num_accels(struct adf_hw_device_data *self)
{
	u32 i, ctr = 0;

	if (!self || !self->accel_mask)
		return 0;

	for (i = 0; i < ADF_C3XXX_MAX_ACCELERATORS; i++) {
		if (self->accel_mask & (1 << i))
			ctr++;
	}
	return ctr;
}

static u32
get_num_aes(struct adf_hw_device_data *self)
{
	u32 i, ctr = 0;

	if (!self || !self->ae_mask)
		return 0;

	for (i = 0; i < ADF_C3XXX_MAX_ACCELENGINES; i++) {
		if (self->ae_mask & (1 << i))
			ctr++;
	}
	return ctr;
}

static u32
get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C3XXX_PMISC_BAR;
}

static u32
get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C3XXX_ETR_BAR;
}

static u32
get_sram_bar_id(struct adf_hw_device_data *self)
{
	return 0;
}

static enum dev_sku_info
get_sku(struct adf_hw_device_data *self)
{
	int aes = get_num_aes(self);

	if (aes == 6)
		return DEV_SKU_4;

	return DEV_SKU_UNKNOWN;
}

static void
adf_get_arbiter_mapping(struct adf_accel_dev *accel_dev,
			u32 const **arb_map_config)
{
	int i;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	for (i = 0; i < ADF_C3XXX_MAX_ACCELENGINES; i++) {
		thrd_to_arb_map_gen[i] = 0;
		if (hw_device->ae_mask & (1 << i))
			thrd_to_arb_map_gen[i] = thrd_to_arb_map[i];
	}
	adf_cfg_gen_dispatch_arbiter(accel_dev,
				     thrd_to_arb_map,
				     thrd_to_arb_map_gen,
				     ADF_C3XXX_MAX_ACCELENGINES);
	*arb_map_config = thrd_to_arb_map_gen;
}

static void
get_arb_info(struct arb_info *arb_csrs_info)
{
	arb_csrs_info->arbiter_offset = ADF_C3XXX_ARB_OFFSET;
	arb_csrs_info->wrk_thd_2_srv_arb_map =
	    ADF_C3XXX_ARB_WRK_2_SER_MAP_OFFSET;
	arb_csrs_info->wrk_cfg_offset = ADF_C3XXX_ARB_WQCFG_OFFSET;
}

static void
get_admin_info(struct admin_info *admin_csrs_info)
{
	admin_csrs_info->mailbox_offset = ADF_C3XXX_MAILBOX_BASE_OFFSET;
	admin_csrs_info->admin_msg_ur = ADF_C3XXX_ADMINMSGUR_OFFSET;
	admin_csrs_info->admin_msg_lr = ADF_C3XXX_ADMINMSGLR_OFFSET;
}

static void
get_errsou_offset(u32 *errsou3, u32 *errsou5)
{
	*errsou3 = ADF_C3XXX_ERRSOU3;
	*errsou5 = ADF_C3XXX_ERRSOU5;
}

static u32
get_clock_speed(struct adf_hw_device_data *self)
{
	/* CPP clock is half high-speed clock */
	return self->clock_frequency / 2;
}

static void
adf_enable_error_correction(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_C3XXX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;
	unsigned int val, i;
	unsigned int mask;

	/* Enable Accel Engine error detection & correction */
	mask = hw_device->ae_mask;
	for (i = 0; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		val = ADF_CSR_RD(csr, ADF_C3XXX_AE_CTX_ENABLES(i));
		val |= ADF_C3XXX_ENABLE_AE_ECC_ERR;
		ADF_CSR_WR(csr, ADF_C3XXX_AE_CTX_ENABLES(i), val);
		val = ADF_CSR_RD(csr, ADF_C3XXX_AE_MISC_CONTROL(i));
		val |= ADF_C3XXX_ENABLE_AE_ECC_PARITY_CORR;
		ADF_CSR_WR(csr, ADF_C3XXX_AE_MISC_CONTROL(i), val);
	}

	/* Enable shared memory error detection & correction */
	mask = hw_device->accel_mask;
	for (i = 0; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		val = ADF_CSR_RD(csr, ADF_C3XXX_UERRSSMSH(i));
		val |= ADF_C3XXX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C3XXX_UERRSSMSH(i), val);
		val = ADF_CSR_RD(csr, ADF_C3XXX_CERRSSMSH(i));
		val |= ADF_C3XXX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C3XXX_CERRSSMSH(i), val);
	}
}

static void
adf_enable_ints(struct adf_accel_dev *accel_dev)
{
	struct resource *addr;

	addr = (&GET_BARS(accel_dev)[ADF_C3XXX_PMISC_BAR])->virt_addr;

	/* Enable bundle and misc interrupts */
	ADF_CSR_WR(addr, ADF_C3XXX_SMIAPF0_MASK_OFFSET, ADF_C3XXX_SMIA0_MASK);
	ADF_CSR_WR(addr, ADF_C3XXX_SMIAPF1_MASK_OFFSET, ADF_C3XXX_SMIA1_MASK);
}

static u32
get_ae_clock(struct adf_hw_device_data *self)
{
	/*
	 * Clock update interval is <16> ticks for c3xxx.
	 */
	return self->clock_frequency / 16;
}

static int
get_storage_enabled(struct adf_accel_dev *accel_dev, uint32_t *storage_enabled)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	strlcpy(key, ADF_STORAGE_FIRMWARE_ENABLED, sizeof(key));
	if (!adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val)) {
		if (kstrtouint(val, 0, storage_enabled))
			return -EFAULT;
	}
	return 0;
}

static int
measure_clock(struct adf_accel_dev *accel_dev)
{
	u32 frequency;
	int ret = 0;

	ret = adf_dev_measure_clock(accel_dev,
				    &frequency,
				    ADF_C3XXX_MIN_AE_FREQ,
				    ADF_C3XXX_MAX_AE_FREQ);
	if (ret)
		return ret;

	accel_dev->hw_device->clock_frequency = frequency;
	return 0;
}

static u32
c3xxx_get_hw_cap(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 legfuses;
	u32 capabilities;
	u32 straps;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 fuses = hw_data->fuses;

	/* Read accelerator capabilities mask */
	legfuses = pci_read_config(pdev, ADF_DEVICE_LEGFUSE_OFFSET, 4);
	capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC +
	    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC +
	    ICP_ACCEL_CAPABILITIES_CIPHER +
	    ICP_ACCEL_CAPABILITIES_AUTHENTICATION +
	    ICP_ACCEL_CAPABILITIES_COMPRESSION + ICP_ACCEL_CAPABILITIES_ZUC +
	    ICP_ACCEL_CAPABILITIES_SHA3 + ICP_ACCEL_CAPABILITIES_HKDF +
	    ICP_ACCEL_CAPABILITIES_ECEDMONT +
	    ICP_ACCEL_CAPABILITIES_EXT_ALGCHAIN;
	if (legfuses & ICP_ACCEL_MASK_CIPHER_SLICE)
		capabilities &= ~(ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
				  ICP_ACCEL_CAPABILITIES_CIPHER |
				  ICP_ACCEL_CAPABILITIES_HKDF |
				  ICP_ACCEL_CAPABILITIES_EXT_ALGCHAIN);
	if (legfuses & ICP_ACCEL_MASK_AUTH_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
	if (legfuses & ICP_ACCEL_MASK_PKE_SLICE)
		capabilities &= ~(ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
				  ICP_ACCEL_CAPABILITIES_ECEDMONT);
	if (legfuses & ICP_ACCEL_MASK_COMPRESS_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;
	if (legfuses & ICP_ACCEL_MASK_EIA3_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_ZUC;
	if (legfuses & ICP_ACCEL_MASK_SHA3_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SHA3;

	straps = pci_read_config(pdev, ADF_C3XXX_SOFTSTRAP_CSR_OFFSET, 4);
	if ((straps | fuses) & ADF_C3XXX_POWERGATE_PKE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	if ((straps | fuses) & ADF_C3XXX_POWERGATE_CY)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;

	return capabilities;
}

static const char *
get_obj_name(struct adf_accel_dev *accel_dev,
	     enum adf_accel_unit_services service)
{
	return ADF_CXXX_AE_FW_NAME_CUSTOM1;
}

static uint32_t
get_objs_num(struct adf_accel_dev *accel_dev)
{
	return 1;
}

static uint32_t
get_obj_cfg_ae_mask(struct adf_accel_dev *accel_dev,
		    enum adf_accel_unit_services services)
{
	return accel_dev->hw_device->ae_mask;
}

void
adf_init_hw_data_c3xxx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class = &c3xxx_class;
	hw_data->instance_id = c3xxx_class.instances++;
	hw_data->num_banks = ADF_C3XXX_ETR_MAX_BANKS;
	hw_data->num_rings_per_bank = ADF_ETR_MAX_RINGS_PER_BANK;
	hw_data->num_accel = ADF_C3XXX_MAX_ACCELERATORS;
	hw_data->num_logical_accel = 1;
	hw_data->num_engines = ADF_C3XXX_MAX_ACCELENGINES;
	hw_data->tx_rx_gap = ADF_C3XXX_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_C3XXX_TX_RINGS_MASK;
	hw_data->alloc_irq = adf_isr_resource_alloc;
	hw_data->free_irq = adf_isr_resource_free;
	hw_data->enable_error_correction = adf_enable_error_correction;
	hw_data->print_err_registers = adf_print_err_registers;
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_sram_bar_id = get_sram_bar_id;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_arb_info = get_arb_info;
	hw_data->get_admin_info = get_admin_info;
	hw_data->get_errsou_offset = get_errsou_offset;
	hw_data->get_clock_speed = get_clock_speed;
	hw_data->get_sku = get_sku;
	hw_data->heartbeat_ctr_num = ADF_NUM_HB_CNT_PER_AE;
	hw_data->fw_name = ADF_C3XXX_FW;
	hw_data->fw_mmp_name = ADF_C3XXX_MMP;
	hw_data->init_admin_comms = adf_init_admin_comms;
	hw_data->exit_admin_comms = adf_exit_admin_comms;
	hw_data->disable_iov = adf_disable_sriov;
	hw_data->send_admin_init = adf_send_admin_init;
	hw_data->init_arb = adf_init_gen2_arb;
	hw_data->exit_arb = adf_exit_arb;
	hw_data->get_arb_mapping = adf_get_arbiter_mapping;
	hw_data->enable_ints = adf_enable_ints;
	hw_data->set_ssm_wdtimer = adf_set_ssm_wdtimer;
	hw_data->check_slice_hang = adf_check_slice_hang;
	hw_data->restore_device = adf_dev_restore;
	hw_data->reset_device = adf_reset_flr;
	hw_data->measure_clock = measure_clock;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->reset_device = adf_reset_flr;
	hw_data->get_objs_num = get_objs_num;
	hw_data->get_obj_name = get_obj_name;
	hw_data->get_obj_cfg_ae_mask = get_obj_cfg_ae_mask;
	hw_data->get_accel_cap = c3xxx_get_hw_cap;
	hw_data->clock_frequency = ADF_C3XXX_AE_FREQ;
	hw_data->extended_dc_capabilities = 0;
	hw_data->get_storage_enabled = get_storage_enabled;
	hw_data->query_storage_cap = 1;
	hw_data->get_heartbeat_status = adf_get_heartbeat_status;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->storage_enable = 0;
	hw_data->get_fw_image_type = adf_cfg_get_fw_image_type;
	hw_data->get_ring_to_svc_map = adf_cfg_get_services_enabled;
	hw_data->config_device = adf_config_device;
	hw_data->set_asym_rings_mask = adf_cfg_set_asym_rings_mask;
	hw_data->ring_to_svc_map = ADF_DEFAULT_RING_TO_SRV_MAP;
	hw_data->pre_reset = adf_dev_pre_reset;
	hw_data->post_reset = adf_dev_post_reset;

	adf_gen2_init_hw_csr_info(&hw_data->csr_info);
	adf_gen2_init_pf_pfvf_ops(&hw_data->csr_info.pfvf_ops);
}

void
adf_clean_hw_data_c3xxx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
}
