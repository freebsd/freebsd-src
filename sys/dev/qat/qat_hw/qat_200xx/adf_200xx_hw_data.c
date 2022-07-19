/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_cfg.h>
#include <adf_pf2vf_msg.h>
#include <adf_dev_err.h>
#include "adf_200xx_hw_data.h"
#include "icp_qat_hw.h"
#include "adf_heartbeat.h"

/* Worker thread to service arbiter mappings */
static const u32 thrd_to_arb_map[ADF_200XX_MAX_ACCELENGINES] =
    { 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA, 0x12222AAA, 0x11222AAA };

enum { DEV_200XX_SKU_1 = 0, DEV_200XX_SKU_2 = 1, DEV_200XX_SKU_3 = 2 };

static u32 thrd_to_arb_map_gen[ADF_200XX_MAX_ACCELENGINES] = { 0 };

static struct adf_hw_device_class qat_200xx_class = {.name =
							 ADF_200XX_DEVICE_NAME,
						     .type = DEV_200XX,
						     .instances = 0 };

static u32
get_accel_mask(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;

	u32 fuse;
	u32 straps;

	fuse = pci_read_config(pdev, ADF_DEVICE_FUSECTL_OFFSET, 4);
	straps = pci_read_config(pdev, ADF_200XX_SOFTSTRAP_CSR_OFFSET, 4);

	return (~(fuse | straps)) >> ADF_200XX_ACCELERATORS_REG_OFFSET &
	    ADF_200XX_ACCELERATORS_MASK;
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
	me_straps = pci_read_config(pdev, ADF_200XX_SOFTSTRAP_CSR_OFFSET, 4);

	/* If SSMs are disabled, then disable the corresponding MEs */
	ssms_disabled =
	    (~get_accel_mask(accel_dev)) & ADF_200XX_ACCELERATORS_MASK;
	me_disable = 0x3;
	while (ssms_disabled) {
		if (ssms_disabled & 1)
			me_straps |= me_disable;
		ssms_disabled >>= 1;
		me_disable <<= 2;
	}

	return (~(fuse | me_straps)) & ADF_200XX_ACCELENGINES_MASK;
}

static u32
get_num_accels(struct adf_hw_device_data *self)
{
	u32 i, ctr = 0;

	if (!self || !self->accel_mask)
		return 0;

	for (i = 0; i < ADF_200XX_MAX_ACCELERATORS; i++) {
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

	for (i = 0; i < ADF_200XX_MAX_ACCELENGINES; i++) {
		if (self->ae_mask & (1 << i))
			ctr++;
	}
	return ctr;
}

static u32
get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_200XX_PMISC_BAR;
}

static u32
get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_200XX_ETR_BAR;
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

	for (i = 0; i < ADF_200XX_MAX_ACCELENGINES; i++) {
		thrd_to_arb_map_gen[i] = 0;
		if (hw_device->ae_mask & (1 << i))
			thrd_to_arb_map_gen[i] = thrd_to_arb_map[i];
	}
	adf_cfg_gen_dispatch_arbiter(accel_dev,
				     thrd_to_arb_map,
				     thrd_to_arb_map_gen,
				     ADF_200XX_MAX_ACCELENGINES);
	*arb_map_config = thrd_to_arb_map_gen;
}

static u32
get_pf2vf_offset(u32 i)
{
	return ADF_200XX_PF2VF_OFFSET(i);
}

static u32
get_vintmsk_offset(u32 i)
{
	return ADF_200XX_VINTMSK_OFFSET(i);
}

static void
get_arb_info(struct arb_info *arb_csrs_info)
{
	arb_csrs_info->arbiter_offset = ADF_200XX_ARB_OFFSET;
	arb_csrs_info->wrk_thd_2_srv_arb_map =
	    ADF_200XX_ARB_WRK_2_SER_MAP_OFFSET;
	arb_csrs_info->wrk_cfg_offset = ADF_200XX_ARB_WQCFG_OFFSET;
}

static void
get_admin_info(struct admin_info *admin_csrs_info)
{
	admin_csrs_info->mailbox_offset = ADF_200XX_MAILBOX_BASE_OFFSET;
	admin_csrs_info->admin_msg_ur = ADF_200XX_ADMINMSGUR_OFFSET;
	admin_csrs_info->admin_msg_lr = ADF_200XX_ADMINMSGLR_OFFSET;
}

static void
get_errsou_offset(u32 *errsou3, u32 *errsou5)
{
	*errsou3 = ADF_200XX_ERRSOU3;
	*errsou5 = ADF_200XX_ERRSOU5;
}

static u32
get_clock_speed(struct adf_hw_device_data *self)
{
	/* CPP clock is half high-speed clock */
	return self->clock_frequency / 2;
}

static void
adf_enable_error_interrupts(struct resource *csr)
{
	ADF_CSR_WR(csr, ADF_ERRMSK0, ADF_200XX_ERRMSK0_CERR); /* ME0-ME3 */
	ADF_CSR_WR(csr, ADF_ERRMSK1, ADF_200XX_ERRMSK1_CERR); /* ME4-ME5 */
	ADF_CSR_WR(csr, ADF_ERRMSK5, ADF_200XX_ERRMSK5_CERR); /* SSM2 */

	/* Reset everything except VFtoPF1_16. */
	adf_csr_fetch_and_and(csr, ADF_ERRMSK3, ADF_200XX_VF2PF1_16);

	/* RI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_200XX_RICPPINTCTL, ADF_200XX_RICPP_EN);

	/* TI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_200XX_TICPPINTCTL, ADF_200XX_TICPP_EN);

	/* Enable CFC Error interrupts and logging. */
	ADF_CSR_WR(csr, ADF_200XX_CPP_CFC_ERR_CTRL, ADF_200XX_CPP_CFC_UE);
}

static void
adf_disable_error_interrupts(struct adf_accel_dev *accel_dev)
{
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_200XX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;

	/* ME0-ME3 */
	ADF_CSR_WR(csr,
		   ADF_ERRMSK0,
		   ADF_200XX_ERRMSK0_UERR | ADF_200XX_ERRMSK0_CERR);
	/* ME4-ME5 */
	ADF_CSR_WR(csr,
		   ADF_ERRMSK1,
		   ADF_200XX_ERRMSK1_UERR | ADF_200XX_ERRMSK1_CERR);
	/* CPP Push Pull, RI, TI, SSM0-SSM1, CFC */
	ADF_CSR_WR(csr, ADF_ERRMSK3, ADF_200XX_ERRMSK3_UERR);
	/* SSM2 */
	ADF_CSR_WR(csr, ADF_ERRMSK5, ADF_200XX_ERRMSK5_UERR);
}

static int
adf_check_uncorrectable_error(struct adf_accel_dev *accel_dev)
{
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_200XX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;

	u32 errsou0 = ADF_CSR_RD(csr, ADF_ERRSOU0) & ADF_200XX_ERRMSK0_UERR;
	u32 errsou1 = ADF_CSR_RD(csr, ADF_ERRSOU1) & ADF_200XX_ERRMSK1_UERR;
	u32 errsou3 = ADF_CSR_RD(csr, ADF_ERRSOU3) & ADF_200XX_ERRMSK3_UERR;
	u32 errsou5 = ADF_CSR_RD(csr, ADF_ERRSOU5) & ADF_200XX_ERRMSK5_UERR;

	return (errsou0 | errsou1 | errsou3 | errsou5);
}

static void
adf_enable_mmp_error_correction(struct resource *csr,
				struct adf_hw_device_data *hw_data)
{
	unsigned int dev, mmp;
	unsigned int mask;

	/* Enable MMP Logging */
	for (dev = 0, mask = hw_data->accel_mask; mask; dev++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		/* Set power-up */
		adf_csr_fetch_and_and(csr,
				      ADF_200XX_SLICEPWRDOWN(dev),
				      ~ADF_200XX_MMP_PWR_UP_MSK);

		if (hw_data->accel_capabilities_mask &
		    ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) {
			for (mmp = 0; mmp < ADF_MAX_MMP; ++mmp) {
				/*
				 * The device supports PKE,
				 * so enable error reporting from MMP memory
				 */
				adf_csr_fetch_and_or(csr,
						     ADF_UERRSSMMMP(dev, mmp),
						     ADF_200XX_UERRSSMMMP_EN);
				/*
				 * The device supports PKE,
				 * so enable error correction from MMP memory
				 */
				adf_csr_fetch_and_or(csr,
						     ADF_CERRSSMMMP(dev, mmp),
						     ADF_200XX_CERRSSMMMP_EN);
			}
		} else {
			for (mmp = 0; mmp < ADF_MAX_MMP; ++mmp) {
				/*
				 * The device doesn't support PKE,
				 * so disable error reporting from MMP memory
				 */
				adf_csr_fetch_and_and(csr,
						      ADF_UERRSSMMMP(dev, mmp),
						      ~ADF_200XX_UERRSSMMMP_EN);
				/*
				 * The device doesn't support PKE,
				 * so disable error correction from MMP memory
				 */
				adf_csr_fetch_and_and(csr,
						      ADF_CERRSSMMMP(dev, mmp),
						      ~ADF_200XX_CERRSSMMMP_EN);
			}
		}

		/* Restore power-down value */
		adf_csr_fetch_and_or(csr,
				     ADF_200XX_SLICEPWRDOWN(dev),
				     ADF_200XX_MMP_PWR_UP_MSK);

		/* Disabling correctable error interrupts. */
		ADF_CSR_WR(csr,
			   ADF_200XX_INTMASKSSM(dev),
			   ADF_200XX_INTMASKSSM_UERR);
	}
}

static void
adf_enable_error_correction(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_200XX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;
	unsigned int val, i;
	unsigned int mask;

	/* Enable Accel Engine error detection & correction */
	mask = hw_device->ae_mask;
	for (i = 0; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		val = ADF_CSR_RD(csr, ADF_200XX_AE_CTX_ENABLES(i));
		val |= ADF_200XX_ENABLE_AE_ECC_ERR;
		ADF_CSR_WR(csr, ADF_200XX_AE_CTX_ENABLES(i), val);
		val = ADF_CSR_RD(csr, ADF_200XX_AE_MISC_CONTROL(i));
		val |= ADF_200XX_ENABLE_AE_ECC_PARITY_CORR;
		ADF_CSR_WR(csr, ADF_200XX_AE_MISC_CONTROL(i), val);
	}

	/* Enable shared memory error detection & correction */
	mask = hw_device->accel_mask;
	for (i = 0; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		val = ADF_CSR_RD(csr, ADF_200XX_UERRSSMSH(i));
		val |= ADF_200XX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_200XX_UERRSSMSH(i), val);
		val = ADF_CSR_RD(csr, ADF_200XX_CERRSSMSH(i));
		val |= ADF_200XX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_200XX_CERRSSMSH(i), val);
		val = ADF_CSR_RD(csr, ADF_PPERR(i));
		val |= ADF_200XX_PPERR_EN;
		ADF_CSR_WR(csr, ADF_PPERR(i), val);
	}

	adf_enable_error_interrupts(csr);
	adf_enable_mmp_error_correction(csr, hw_device);
}

static void
adf_enable_ints(struct adf_accel_dev *accel_dev)
{
	struct resource *addr;

	addr = (&GET_BARS(accel_dev)[ADF_200XX_PMISC_BAR])->virt_addr;

	/* Enable bundle and misc interrupts */
	ADF_CSR_WR(addr, ADF_200XX_SMIAPF0_MASK_OFFSET, ADF_200XX_SMIA0_MASK);
	ADF_CSR_WR(addr, ADF_200XX_SMIAPF1_MASK_OFFSET, ADF_200XX_SMIA1_MASK);
}

static u32
get_ae_clock(struct adf_hw_device_data *self)
{
	/*
	 * Clock update interval is <16> ticks for 200xx.
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
				    ADF_200XX_MIN_AE_FREQ,
				    ADF_200XX_MAX_AE_FREQ);
	if (ret)
		return ret;

	accel_dev->hw_device->clock_frequency = frequency;
	return 0;
}

static u32
adf_200xx_get_hw_cap(struct adf_accel_dev *accel_dev)
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

	straps = pci_read_config(pdev, ADF_200XX_SOFTSTRAP_CSR_OFFSET, 4);
	if ((straps | fuses) & ADF_200XX_POWERGATE_PKE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	if ((straps | fuses) & ADF_200XX_POWERGATE_CY)
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
adf_init_hw_data_200xx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class = &qat_200xx_class;
	hw_data->instance_id = qat_200xx_class.instances++;
	hw_data->num_banks = ADF_200XX_ETR_MAX_BANKS;
	hw_data->num_rings_per_bank = ADF_ETR_MAX_RINGS_PER_BANK;
	hw_data->num_accel = ADF_200XX_MAX_ACCELERATORS;
	hw_data->num_logical_accel = 1;
	hw_data->num_engines = ADF_200XX_MAX_ACCELENGINES;
	hw_data->tx_rx_gap = ADF_200XX_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_200XX_TX_RINGS_MASK;
	hw_data->alloc_irq = adf_isr_resource_alloc;
	hw_data->free_irq = adf_isr_resource_free;
	hw_data->enable_error_correction = adf_enable_error_correction;
	hw_data->check_uncorrectable_error = adf_check_uncorrectable_error;
	hw_data->print_err_registers = adf_print_err_registers;
	hw_data->disable_error_interrupts = adf_disable_error_interrupts;
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_sram_bar_id = get_sram_bar_id;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_pf2vf_offset = get_pf2vf_offset;
	hw_data->get_vintmsk_offset = get_vintmsk_offset;
	hw_data->get_arb_info = get_arb_info;
	hw_data->get_admin_info = get_admin_info;
	hw_data->get_errsou_offset = get_errsou_offset;
	hw_data->get_clock_speed = get_clock_speed;
	hw_data->get_sku = get_sku;
	hw_data->fw_name = ADF_200XX_FW;
	hw_data->fw_mmp_name = ADF_200XX_MMP;
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
	hw_data->enable_vf2pf_comms = adf_pf_enable_vf2pf_comms;
	hw_data->disable_vf2pf_comms = adf_pf_disable_vf2pf_comms;
	hw_data->restore_device = adf_dev_restore;
	hw_data->reset_device = adf_reset_flr;
	hw_data->min_iov_compat_ver = ADF_PFVF_COMPATIBILITY_VERSION;
	hw_data->measure_clock = measure_clock;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->reset_device = adf_reset_flr;
	hw_data->get_objs_num = get_objs_num;
	hw_data->get_obj_name = get_obj_name;
	hw_data->get_obj_cfg_ae_mask = get_obj_cfg_ae_mask;
	hw_data->get_accel_cap = adf_200xx_get_hw_cap;
	hw_data->clock_frequency = ADF_200XX_AE_FREQ;
	hw_data->extended_dc_capabilities = 0;
	hw_data->get_storage_enabled = get_storage_enabled;
	hw_data->query_storage_cap = 1;
	hw_data->get_heartbeat_status = adf_get_heartbeat_status;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->storage_enable = 0;
	hw_data->get_ring_to_svc_map = adf_cfg_get_services_enabled;
	hw_data->config_device = adf_config_device;
	hw_data->set_asym_rings_mask = adf_cfg_set_asym_rings_mask;
	hw_data->ring_to_svc_map = ADF_DEFAULT_RING_TO_SRV_MAP;
	hw_data->pre_reset = adf_dev_pre_reset;
	hw_data->post_reset = adf_dev_post_reset;
}

void
adf_clean_hw_data_200xx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
}
