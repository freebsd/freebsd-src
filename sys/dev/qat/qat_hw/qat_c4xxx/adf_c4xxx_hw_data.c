/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include <linux/atomic.h>
#include <linux/compiler.h>
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_pfvf_msg.h>
#include <adf_dev_err.h>
#include <adf_cfg.h>
#include <adf_fw_counters.h>
#include <adf_gen2_hw_data.h>
#include <adf_gen2_pfvf.h>
#include "adf_c4xxx_hw_data.h"
#include "adf_c4xxx_reset.h"
#include "adf_c4xxx_inline.h"
#include "adf_c4xxx_ras.h"
#include "adf_c4xxx_misc_error_stats.h"
#include "adf_c4xxx_pke_replay_stats.h"
#include "adf_heartbeat.h"
#include "icp_qat_fw_init_admin.h"
#include "icp_qat_hw.h"

/* accel unit information */
static struct adf_accel_unit adf_c4xxx_au_32_ae[] =
    { { 0x1, 0x3, 0x3F, 0x1B, 6, ADF_ACCEL_SERVICE_NULL },
      { 0x2, 0xC, 0xFC0, 0x6C0, 6, ADF_ACCEL_SERVICE_NULL },
      { 0x4, 0x30, 0xF000, 0xF000, 4, ADF_ACCEL_SERVICE_NULL },
      { 0x8, 0xC0, 0x3F0000, 0x1B0000, 6, ADF_ACCEL_SERVICE_NULL },
      { 0x10, 0x300, 0xFC00000, 0x6C00000, 6, ADF_ACCEL_SERVICE_NULL },
      { 0x20, 0xC00, 0xF0000000, 0xF0000000, 4, ADF_ACCEL_SERVICE_NULL } };

static struct adf_accel_unit adf_c4xxx_au_24_ae[] = {
	{ 0x1, 0x3, 0x3F, 0x1B, 6, ADF_ACCEL_SERVICE_NULL },
	{ 0x2, 0xC, 0xFC0, 0x6C0, 6, ADF_ACCEL_SERVICE_NULL },
	{ 0x8, 0xC0, 0x3F0000, 0x1B0000, 6, ADF_ACCEL_SERVICE_NULL },
	{ 0x10, 0x300, 0xFC00000, 0x6C00000, 6, ADF_ACCEL_SERVICE_NULL },
};

static struct adf_accel_unit adf_c4xxx_au_12_ae[] = {
	{ 0x1, 0x3, 0x3F, 0x1B, 6, ADF_ACCEL_SERVICE_NULL },
	{ 0x8, 0xC0, 0x3F0000, 0x1B0000, 6, ADF_ACCEL_SERVICE_NULL },
};

static struct adf_accel_unit adf_c4xxx_au_emulation[] =
    { { 0x1, 0x3, 0x3F, 0x1B, 6, ADF_ACCEL_SERVICE_NULL },
      { 0x2, 0xC, 0xC0, 0xC0, 2, ADF_ACCEL_SERVICE_NULL } };

/* Accel engine threads for each of the following services
 * <num_asym_thd> , <num_sym_thd> , <num_dc_thd>,
 */

/* Thread mapping for SKU capable of symmetric cryptography */
static const struct adf_ae_info adf_c4xxx_32_ae_sym[] =
    { { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 },
      { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 },
      { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 2, 6, 3 } };

static const struct adf_ae_info adf_c4xxx_24_ae_sym[] =
    { { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 },
      { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 1, 7, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 },
      { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 } };

static const struct adf_ae_info adf_c4xxx_12_ae_sym[] =
    { { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 }, { 2, 6, 3 },
      { 1, 7, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 2, 6, 3 }, { 2, 6, 3 }, { 1, 7, 0 }, { 2, 6, 3 },
      { 2, 6, 3 }, { 1, 7, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 } };

/* Thread mapping for SKU capable of asymmetric and symmetric cryptography */
static const struct adf_ae_info adf_c4xxx_32_ae[] =
    { { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 },
      { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 },
      { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 2, 5, 3 } };

static const struct adf_ae_info adf_c4xxx_24_ae[] =
    { { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 },
      { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 1, 6, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 },
      { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 } };

static const struct adf_ae_info adf_c4xxx_12_ae[] =
    { { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 }, { 2, 5, 3 },
      { 1, 6, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 2, 5, 3 }, { 2, 5, 3 }, { 1, 6, 0 }, { 2, 5, 3 },
      { 2, 5, 3 }, { 1, 6, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
      { 0, 0, 0 }, { 0, 0, 0 } };

static struct adf_hw_device_class c4xxx_class = {.name = ADF_C4XXX_DEVICE_NAME,
						 .type = DEV_C4XXX,
						 .instances = 0 };

struct icp_qat_fw_init_c4xxx_admin_hb_stats {
	struct icp_qat_fw_init_admin_hb_cnt stats[ADF_NUM_THREADS_PER_AE];
};

struct adf_hb_count {
	u16 ae_thread[ADF_NUM_THREADS_PER_AE];
};

static const int sku_cy_au[] = ADF_C4XXX_NUM_CY_AU;
static const int sku_dc_au[] = ADF_C4XXX_NUM_DC_AU;
static const int sku_inline_au[] = ADF_C4XXX_NUM_INLINE_AU;

/*
 * C4xxx devices introduce new fuses and soft straps and
 * are different from previous gen device implementations.
 */

static u32
get_accel_mask(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fusectl0;
	u32 softstrappull0;

	fusectl0 = pci_read_config(pdev, ADF_C4XXX_FUSECTL0_OFFSET, 4);
	softstrappull0 =
	    pci_read_config(pdev, ADF_C4XXX_SOFTSTRAPPULL0_OFFSET, 4);

	return (~(fusectl0 | softstrappull0)) & ADF_C4XXX_ACCELERATORS_MASK;
}

static u32
get_ae_mask(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fusectl1;
	u32 softstrappull1;

	fusectl1 = pci_read_config(pdev, ADF_C4XXX_FUSECTL1_OFFSET, 4);
	softstrappull1 =
	    pci_read_config(pdev, ADF_C4XXX_SOFTSTRAPPULL1_OFFSET, 4);

	/* Assume that AE and AU disable masks are consistent, so no
	 * checks against the AU mask are performed
	 */
	return (~(fusectl1 | softstrappull1)) & ADF_C4XXX_ACCELENGINES_MASK;
}

static u32
get_num_accels(struct adf_hw_device_data *self)
{
	return self ? hweight32(self->accel_mask) : 0;
}

static u32
get_num_aes(struct adf_hw_device_data *self)
{
	return self ? hweight32(self->ae_mask) : 0;
}

static u32
get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C4XXX_PMISC_BAR;
}

static u32
get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C4XXX_ETR_BAR;
}

static u32
get_sram_bar_id(struct adf_hw_device_data *self)
{
	return ADF_C4XXX_SRAM_BAR;
}

static inline void
c4xxx_unpack_ssm_wdtimer(u64 value, u32 *upper, u32 *lower)
{
	*lower = lower_32_bits(value);
	*upper = upper_32_bits(value);
}

/**
 * c4xxx_set_ssm_wdtimer() - Initialize the slice hang watchdog timer.
 *
 * @param accel_dev    Structure holding accelerator data.
 * @return 0 on success, error code otherwise.
 */
static int
c4xxx_set_ssm_wdtimer(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar =
	    &GET_BARS(accel_dev)[hw_device->get_misc_bar_id(hw_device)];
	struct resource *csr = misc_bar->virt_addr;
	unsigned long accel_mask = hw_device->accel_mask;
	u32 accel = 0;
	u64 timer_val = ADF_C4XXX_SSM_WDT_64BIT_DEFAULT_VALUE;
	u64 timer_val_pke = ADF_C4XXX_SSM_WDT_PKE_64BIT_DEFAULT_VALUE;
	u32 ssm_wdt_low = 0, ssm_wdt_high = 0;
	u32 ssm_wdt_pke_low = 0, ssm_wdt_pke_high = 0;

	/* Convert 64bit Slice Hang watchdog value into 32bit values for
	 * mmio write to 32bit CSRs.
	 */
	c4xxx_unpack_ssm_wdtimer(timer_val, &ssm_wdt_high, &ssm_wdt_low);
	c4xxx_unpack_ssm_wdtimer(timer_val_pke,
				 &ssm_wdt_pke_high,
				 &ssm_wdt_pke_low);

	/* Configures Slice Hang watchdogs */
	for_each_set_bit(accel, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		ADF_CSR_WR(csr, ADF_C4XXX_SSMWDTL_OFFSET(accel), ssm_wdt_low);
		ADF_CSR_WR(csr, ADF_C4XXX_SSMWDTH_OFFSET(accel), ssm_wdt_high);
		ADF_CSR_WR(csr,
			   ADF_C4XXX_SSMWDTPKEL_OFFSET(accel),
			   ssm_wdt_pke_low);
		ADF_CSR_WR(csr,
			   ADF_C4XXX_SSMWDTPKEH_OFFSET(accel),
			   ssm_wdt_pke_high);
	}

	return 0;
}

/**
 * c4xxx_check_slice_hang() - Check slice hang status
 *
 * Return: true if a slice hange interrupt is serviced..
 */
static bool
c4xxx_check_slice_hang(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar =
	    &GET_BARS(accel_dev)[hw_device->get_misc_bar_id(hw_device)];
	struct resource *csr = misc_bar->virt_addr;
	u32 slice_hang_offset;
	u32 ia_slice_hang_offset;
	u32 fw_irq_source;
	u32 ia_irq_source;
	u32 accel_num = 0;
	bool handled = false;
	u32 errsou10 = ADF_CSR_RD(csr, ADF_C4XXX_ERRSOU10);
	unsigned long accel_mask;

	accel_mask = hw_device->accel_mask;

	for_each_set_bit(accel_num, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		if (!(errsou10 & ADF_C4XXX_IRQ_SRC_MASK(accel_num)))
			continue;

		fw_irq_source = ADF_CSR_RD(csr, ADF_INTSTATSSM(accel_num));
		ia_irq_source =
		    ADF_CSR_RD(csr, ADF_C4XXX_IAINTSTATSSM(accel_num));
		ia_slice_hang_offset =
		    ADF_C4XXX_IASLICEHANGSTATUS_OFFSET(accel_num);

		/* FW did not clear SliceHang error, IA logs and clears
		 * the error
		 */
		if ((fw_irq_source & ADF_INTSTATSSM_SHANGERR) &&
		    (ia_irq_source & ADF_INTSTATSSM_SHANGERR)) {
			slice_hang_offset =
			    ADF_C4XXX_SLICEHANGSTATUS_OFFSET(accel_num);

			/* Bring hung slice out of reset */
			adf_csr_fetch_and_and(csr, slice_hang_offset, ~0);

			/* Log SliceHang error and clear an interrupt */
			handled = adf_handle_slice_hang(accel_dev,
							accel_num,
							csr,
							ia_slice_hang_offset);
			atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		}
		/* FW cleared SliceHang, IA only logs an error */
		else if (!(fw_irq_source & ADF_INTSTATSSM_SHANGERR) &&
			 (ia_irq_source & ADF_INTSTATSSM_SHANGERR)) {
			/* Log SliceHang error and clear an interrupt */
			handled = adf_handle_slice_hang(accel_dev,
							accel_num,
							csr,
							ia_slice_hang_offset);

			atomic_inc(&accel_dev->ras_counters[ADF_RAS_UNCORR]);
		}

		/* Clear the associated IA interrupt */
		adf_csr_fetch_and_and(csr,
				      ADF_C4XXX_IAINTSTATSSM(accel_num),
				      ~BIT(13));
	}

	return handled;
}

static bool
get_eth_doorbell_msg(struct adf_accel_dev *accel_dev)
{
	struct resource *csr =
	    (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 errsou11 = ADF_CSR_RD(csr, ADF_C4XXX_ERRSOU11);
	u32 doorbell_int = ADF_CSR_RD(csr, ADF_C4XXX_ETH_DOORBELL_INT);
	u32 eth_doorbell_reg[ADF_C4XXX_NUM_ETH_DOORBELL_REGS];
	bool handled = false;
	u32 data_reg;
	u8 i;

	/* Reset cannot be acknowledged until the reset */
	hw_device->reset_ack = false;

	/* Check if doorbell interrupt occurred. */
	if (errsou11 & ADF_C4XXX_DOORBELL_INT_SRC) {
		/* Decode doorbell messages from ethernet device */
		for (i = 0; i < ADF_C4XXX_NUM_ETH_DOORBELL_REGS; i++) {
			eth_doorbell_reg[i] = 0;
			if (doorbell_int & BIT(i)) {
				data_reg = ADF_C4XXX_ETH_DOORBELL(i);
				eth_doorbell_reg[i] = ADF_CSR_RD(csr, data_reg);
				device_printf(
				    GET_DEV(accel_dev),
				    "Receives Doorbell message(0x%08x)\n",
				    eth_doorbell_reg[i]);
			}
		}
		/* Only need to check PF0 */
		if (eth_doorbell_reg[0] == ADF_C4XXX_IOSFSB_RESET_ACK) {
			device_printf(GET_DEV(accel_dev),
				      "Receives pending reset ACK\n");
			hw_device->reset_ack = true;
		}
		/* Clear the interrupt source */
		ADF_CSR_WR(csr,
			   ADF_C4XXX_ETH_DOORBELL_INT,
			   ADF_C4XXX_ETH_DOORBELL_MASK);
		handled = true;
	}

	return handled;
}

static enum dev_sku_info
get_sku(struct adf_hw_device_data *self)
{
	int aes = get_num_aes(self);
	u32 capabilities = self->accel_capabilities_mask;
	bool sym_only_sku = false;

	/* Check if SKU is capable only of symmetric cryptography
	 * via device capabilities.
	 */
	if ((capabilities & ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC) &&
	    !(capabilities & ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) &&
	    !(capabilities & ADF_ACCEL_CAPABILITIES_COMPRESSION))
		sym_only_sku = true;

	switch (aes) {
	case ADF_C4XXX_HIGH_SKU_AES:
		if (sym_only_sku)
			return DEV_SKU_1_CY;
		return DEV_SKU_1;
	case ADF_C4XXX_MED_SKU_AES:
		if (sym_only_sku)
			return DEV_SKU_2_CY;
		return DEV_SKU_2;
	case ADF_C4XXX_LOW_SKU_AES:
		if (sym_only_sku)
			return DEV_SKU_3_CY;
		return DEV_SKU_3;
	};

	return DEV_SKU_UNKNOWN;
}

static bool
c4xxx_check_prod_sku(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fusectl0 = 0;

	fusectl0 = pci_read_config(pdev, ADF_C4XXX_FUSECTL0_OFFSET, 4);

	if (fusectl0 & ADF_C4XXX_FUSE_PROD_SKU_MASK)
		return true;
	else
		return false;
}

static bool
adf_check_sym_only_sku_c4xxx(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 legfuse = 0;

	legfuse = pci_read_config(pdev, ADF_DEVICE_LEGFUSE_OFFSET, 4);

	if (legfuse & ADF_C4XXX_LEGFUSE_BASE_SKU_MASK)
		return true;
	else
		return false;
}

static void
adf_enable_slice_hang_detection(struct adf_accel_dev *accel_dev)
{
	struct resource *csr;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 accel = 0;
	unsigned long accel_mask;

	csr = (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;
	accel_mask = hw_device->accel_mask;

	for_each_set_bit(accel, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		/* Unmasks Slice Hang interrupts so they can be seen by IA. */
		ADF_CSR_WR(csr,
			   ADF_C4XXX_SHINTMASKSSM_OFFSET(accel),
			   ADF_C4XXX_SHINTMASKSSM_VAL);
	}
}

static void
adf_enable_ras(struct adf_accel_dev *accel_dev)
{
	struct resource *csr;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 accel = 0;
	unsigned long accel_mask;

	csr = (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;
	accel_mask = hw_device->accel_mask;

	for_each_set_bit(accel, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		ADF_CSR_WR(csr,
			   ADF_C4XXX_GET_SSMFEATREN_OFFSET(accel),
			   ADF_C4XXX_SSMFEATREN_VAL);
	}
}

static u32
get_clock_speed(struct adf_hw_device_data *self)
{
	/* c4xxx CPP clock is equal to high-speed clock */
	return self->clock_frequency;
}

static void
adf_enable_error_interrupts(struct adf_accel_dev *accel_dev)
{
	struct resource *csr, *aram_csr;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 accel = 0;
	unsigned long accel_mask;

	csr = (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;
	aram_csr = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;
	accel_mask = hw_device->accel_mask;

	for_each_set_bit(accel, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		/* Enable shared memory, MMP, CPP, PPERR interrupts
		 * for a given accel
		 */
		ADF_CSR_WR(csr, ADF_C4XXX_GET_INTMASKSSM_OFFSET(accel), 0);

		/* Enable SPP parity error interrupts for a given accel */
		ADF_CSR_WR(csr, ADF_C4XXX_GET_SPPPARERRMSK_OFFSET(accel), 0);

		/* Enable ssm soft parity errors on given accel */
		ADF_CSR_WR(csr,
			   ADF_C4XXX_GET_SSMSOFTERRORPARITY_MASK_OFFSET(accel),
			   ADF_C4XXX_SSMSOFTERRORPARITY_MASK_VAL);
	}

	/* Enable interrupts for VFtoPF0_127. */
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK4, ADF_C4XXX_VF2PF0_31);
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK5, ADF_C4XXX_VF2PF32_63);
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK6, ADF_C4XXX_VF2PF64_95);
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK7, ADF_C4XXX_VF2PF96_127);

	/* Enable interrupts signaling ECC correctable errors for all AEs */
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK8, ADF_C4XXX_ERRMSK8_COERR);
	ADF_CSR_WR(csr,
		   ADF_C4XXX_HI_ME_COR_ERRLOG_ENABLE,
		   ADF_C4XXX_HI_ME_COR_ERRLOG_ENABLE_MASK);

	/* Enable error interrupts reported by ERRSOU9 */
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK9, ADF_C4XXX_ERRMSK9_IRQ_MASK);

	/* Enable uncorrectable errors on all the AE */
	ADF_CSR_WR(csr,
		   ADF_C4XXX_HI_ME_UNCERR_LOG_ENABLE,
		   ADF_C4XXX_HI_ME_UNCERR_LOG_ENABLE_MASK);

	/* Enable CPP Agent to report command parity errors */
	ADF_CSR_WR(csr,
		   ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG_ENABLE,
		   ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG_ENABLE_MASK);

	/* Enable reporting of RI memory parity errors */
	ADF_CSR_WR(csr,
		   ADF_C4XXX_RI_MEM_PAR_ERR_EN0,
		   ADF_C4XXX_RI_MEM_PAR_ERR_EN0_MASK);

	/* Enable reporting of TI memory parity errors */
	ADF_CSR_WR(csr,
		   ADF_C4XXX_TI_MEM_PAR_ERR_EN0,
		   ADF_C4XXX_TI_MEM_PAR_ERR_EN0_MASK);
	ADF_CSR_WR(csr,
		   ADF_C4XXX_TI_MEM_PAR_ERR_EN1,
		   ADF_C4XXX_TI_MEM_PAR_ERR_EN1_MASK);

	/* Enable SSM errors */
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK10, ADF_C4XXX_ERRMSK10_SSM_ERR);

	/* Enable miscellaneous errors (ethernet doorbell aram, ici, ice) */
	ADF_CSR_WR(csr, ADF_C4XXX_ERRMSK11, ADF_C4XXX_ERRMSK11_ERR);

	/* RI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_C4XXX_RICPPINTCTL, ADF_C4XXX_RICPP_EN);

	/* TI CPP bus interface error detection and reporting. */
	ADF_CSR_WR(csr, ADF_C4XXX_TICPPINTCTL, ADF_C4XXX_TICPP_EN);

	/* Enable CFC Error interrupts and logging. */
	ADF_CSR_WR(csr, ADF_C4XXX_CPP_CFC_ERR_CTRL, ADF_C4XXX_CPP_CFC_UE);

	/* Enable ARAM correctable error detection. */
	ADF_CSR_WR(aram_csr, ADF_C4XXX_ARAMCERR, ADF_C4XXX_ARAM_CERR);

	/* Enable ARAM uncorrectable error detection. */
	ADF_CSR_WR(aram_csr, ADF_C4XXX_ARAMUERR, ADF_C4XXX_ARAM_UERR);

	/* Enable Push/Pull Misc Uncorrectable error interrupts and logging */
	ADF_CSR_WR(aram_csr, ADF_C4XXX_CPPMEMTGTERR, ADF_C4XXX_TGT_UERR);
}

static void
adf_enable_mmp_error_correction(struct resource *csr,
				struct adf_hw_device_data *hw_data)
{
	unsigned int accel = 0, mmp;
	unsigned long uerrssmmmp_mask, cerrssmmmp_mask;
	enum operation op;
	unsigned long accel_mask;

	/* Prepare values and operation that will be performed on
	 * UERRSSMMMP and CERRSSMMMP registers on each MMP
	 */
	if (hw_data->accel_capabilities_mask &
	    ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) {
		uerrssmmmp_mask = ADF_C4XXX_UERRSSMMMP_EN;
		cerrssmmmp_mask = ADF_C4XXX_CERRSSMMMP_EN;
		op = OR;
	} else {
		uerrssmmmp_mask = ~ADF_C4XXX_UERRSSMMMP_EN;
		cerrssmmmp_mask = ~ADF_C4XXX_CERRSSMMMP_EN;
		op = AND;
	}

	accel_mask = hw_data->accel_mask;

	/* Enable MMP Logging */
	for_each_set_bit(accel, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		/* Set power-up */
		adf_csr_fetch_and_and(csr,
				      ADF_C4XXX_SLICEPWRDOWN(accel),
				      ~ADF_C4XXX_MMP_PWR_UP_MSK);

		for (mmp = 0; mmp < ADF_C4XXX_MAX_MMP; ++mmp) {
			adf_csr_fetch_and_update(op,
						 csr,
						 ADF_C4XXX_UERRSSMMMP(accel,
								      mmp),
						 uerrssmmmp_mask);
			adf_csr_fetch_and_update(op,
						 csr,
						 ADF_C4XXX_CERRSSMMMP(accel,
								      mmp),
						 cerrssmmmp_mask);
		}

		/* Restore power-down value */
		adf_csr_fetch_and_or(csr,
				     ADF_C4XXX_SLICEPWRDOWN(accel),
				     ADF_C4XXX_MMP_PWR_UP_MSK);
	}
}

static void
get_arb_info(struct arb_info *arb_csrs_info)
{
	arb_csrs_info->arbiter_offset = ADF_C4XXX_ARB_OFFSET;
	arb_csrs_info->wrk_cfg_offset = ADF_C4XXX_ARB_WQCFG_OFFSET;
}

static void
get_admin_info(struct admin_info *admin_csrs_info)
{
	admin_csrs_info->mailbox_offset = ADF_C4XXX_MAILBOX_BASE_OFFSET;
	admin_csrs_info->admin_msg_ur = ADF_C4XXX_ADMINMSGUR_OFFSET;
	admin_csrs_info->admin_msg_lr = ADF_C4XXX_ADMINMSGLR_OFFSET;
}

static void
get_errsou_offset(u32 *errsou3, u32 *errsou5)
{
	*errsou3 = ADF_C4XXX_ERRSOU3;
	*errsou5 = ADF_C4XXX_ERRSOU5;
}

static void
adf_enable_error_correction(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;
	unsigned int val, i = 0;
	unsigned long ae_mask;
	unsigned long accel_mask;

	ae_mask = hw_device->ae_mask;

	/* Enable Accel Engine error detection & correction */
	for_each_set_bit(i, &ae_mask, ADF_C4XXX_MAX_ACCELENGINES)
	{
		val = ADF_CSR_RD(csr, ADF_C4XXX_AE_CTX_ENABLES(i));
		val |= ADF_C4XXX_ENABLE_AE_ECC_ERR;
		ADF_CSR_WR(csr, ADF_C4XXX_AE_CTX_ENABLES(i), val);
		val = ADF_CSR_RD(csr, ADF_C4XXX_AE_MISC_CONTROL(i));
		val |= ADF_C4XXX_ENABLE_AE_ECC_PARITY_CORR;
		ADF_CSR_WR(csr, ADF_C4XXX_AE_MISC_CONTROL(i), val);
	}

	accel_mask = hw_device->accel_mask;

	/* Enable shared memory error detection & correction */
	for_each_set_bit(i, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		val = ADF_CSR_RD(csr, ADF_C4XXX_UERRSSMSH(i));
		val |= ADF_C4XXX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C4XXX_UERRSSMSH(i), val);
		val = ADF_CSR_RD(csr, ADF_C4XXX_CERRSSMSH(i));
		val |= ADF_C4XXX_ERRSSMSH_EN;
		ADF_CSR_WR(csr, ADF_C4XXX_CERRSSMSH(i), val);
	}

	adf_enable_ras(accel_dev);
	adf_enable_mmp_error_correction(csr, hw_device);
	adf_enable_slice_hang_detection(accel_dev);
	adf_enable_error_interrupts(accel_dev);
}

static void
adf_enable_ints(struct adf_accel_dev *accel_dev)
{
	struct resource *addr;

	addr = (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;

	/* Enable bundle interrupts */
	ADF_CSR_WR(addr, ADF_C4XXX_SMIAPF0_MASK_OFFSET, ADF_C4XXX_SMIA0_MASK);
	ADF_CSR_WR(addr, ADF_C4XXX_SMIAPF1_MASK_OFFSET, ADF_C4XXX_SMIA1_MASK);
	ADF_CSR_WR(addr, ADF_C4XXX_SMIAPF2_MASK_OFFSET, ADF_C4XXX_SMIA2_MASK);
	ADF_CSR_WR(addr, ADF_C4XXX_SMIAPF3_MASK_OFFSET, ADF_C4XXX_SMIA3_MASK);
	/*Enable misc interrupts*/
	ADF_CSR_WR(addr, ADF_C4XXX_SMIAPF4_MASK_OFFSET, ADF_C4XXX_SMIA4_MASK);
}

static u32
get_ae_clock(struct adf_hw_device_data *self)
{
	/* Clock update interval is <16> ticks for c4xxx. */
	return self->clock_frequency / 16;
}

static int
measure_clock(struct adf_accel_dev *accel_dev)
{
	u32 frequency;
	int ret = 0;

	ret = adf_dev_measure_clock(accel_dev,
				    &frequency,
				    ADF_C4XXX_MIN_AE_FREQ,
				    ADF_C4XXX_MAX_AE_FREQ);
	if (ret)
		return ret;

	accel_dev->hw_device->clock_frequency = frequency;
	return 0;
}

static int
get_storage_enabled(struct adf_accel_dev *accel_dev, uint32_t *storage_enabled)
{
	if (accel_dev->au_info->num_dc_au > 0) {
		*storage_enabled = 1;
		GET_HW_DATA(accel_dev)->extended_dc_capabilities =
		    ICP_ACCEL_CAPABILITIES_ADVANCED_COMPRESSION;
	}
	return 0;
}

static u32
c4xxx_get_hw_cap(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 legfuses;
	u32 softstrappull0, softstrappull2;
	u32 fusectl0, fusectl2;
	u32 capabilities;

	/* Read accelerator capabilities mask */
	legfuses = pci_read_config(pdev, ADF_DEVICE_LEGFUSE_OFFSET, 4);
	capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
	    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
	    ICP_ACCEL_CAPABILITIES_CIPHER |
	    ICP_ACCEL_CAPABILITIES_AUTHENTICATION |
	    ICP_ACCEL_CAPABILITIES_COMPRESSION | ICP_ACCEL_CAPABILITIES_ZUC |
	    ICP_ACCEL_CAPABILITIES_HKDF | ICP_ACCEL_CAPABILITIES_SHA3_EXT |
	    ICP_ACCEL_CAPABILITIES_SM3 | ICP_ACCEL_CAPABILITIES_SM4 |
	    ICP_ACCEL_CAPABILITIES_CHACHA_POLY |
	    ICP_ACCEL_CAPABILITIES_AESGCM_SPC |
	    ICP_ACCEL_CAPABILITIES_ECEDMONT;

	if (legfuses & ICP_ACCEL_MASK_CIPHER_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}
	if (legfuses & ICP_ACCEL_MASK_AUTH_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
	if (legfuses & ICP_ACCEL_MASK_PKE_SLICE)
		capabilities &= ~(ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
				  ICP_ACCEL_CAPABILITIES_ECEDMONT);
	if (legfuses & ICP_ACCEL_MASK_COMPRESS_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY;
	}
	if (legfuses & ICP_ACCEL_MASK_EIA3_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_ZUC;
	if (legfuses & ICP_ACCEL_MASK_SM3_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SM3;
	if (legfuses & ICP_ACCEL_MASK_SM4_SLICE)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SM4;

	/* Read fusectl0 & softstrappull0 registers to ensure inline
	 * acceleration is not disabled
	 */
	softstrappull0 =
	    pci_read_config(pdev, ADF_C4XXX_SOFTSTRAPPULL0_OFFSET, 4);
	fusectl0 = pci_read_config(pdev, ADF_C4XXX_FUSECTL0_OFFSET, 4);
	if ((fusectl0 | softstrappull0) & ADF_C4XXX_FUSE_DISABLE_INLINE_MASK)
		capabilities &= ~ICP_ACCEL_CAPABILITIES_INLINE;

	/* Read fusectl2 & softstrappull2 registers to check out if
	 * PKE/DC are enabled/disabled
	 */
	softstrappull2 =
	    pci_read_config(pdev, ADF_C4XXX_SOFTSTRAPPULL2_OFFSET, 4);
	fusectl2 = pci_read_config(pdev, ADF_C4XXX_FUSECTL2_OFFSET, 4);
	/* Disable PKE/DC cap if there are no PKE/DC-enabled AUs. */
	if (!(~fusectl2 & ~softstrappull2 & ADF_C4XXX_FUSE_PKE_MASK))
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	if (!(~fusectl2 & ~softstrappull2 & ADF_C4XXX_FUSE_COMP_MASK))
		capabilities &= ~(ICP_ACCEL_CAPABILITIES_COMPRESSION |
				  ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY);

	return capabilities;
}

static int
c4xxx_configure_accel_units(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES] = { 0 };
	unsigned long val;
	char val_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	int sku;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	sku = get_sku(hw_data);

	if (adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC))
		goto err;

	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);

	/* Base station SKU supports symmetric cryptography only. */
	if (adf_check_sym_only_sku_c4xxx(accel_dev))
		snprintf(val_str, sizeof(val_str), ADF_SERVICE_SYM);
	else
		snprintf(val_str, sizeof(val_str), ADF_SERVICE_CY);

	val = sku_dc_au[sku];
	if (val) {
		strncat(val_str,
			ADF_SERVICES_SEPARATOR ADF_SERVICE_DC,
			ADF_CFG_MAX_VAL_LEN_IN_BYTES -
			    strnlen(val_str, sizeof(val_str)) -
			    ADF_CFG_NULL_TERM_SIZE);
	}

	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)val_str, ADF_STR))
		goto err;

	snprintf(key, sizeof(key), ADF_NUM_CY_ACCEL_UNITS);
	val = sku_cy_au[sku];
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_NUM_DC_ACCEL_UNITS);
	val = sku_dc_au[sku];
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_NUM_INLINE_ACCEL_UNITS);
	val = sku_inline_au[sku];
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	return 0;
err:
	device_printf(GET_DEV(accel_dev), "Failed to configure accel units\n");
	return EINVAL;
}

static void
update_hw_capability(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_unit_info *au_info = accel_dev->au_info;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u32 disabled_caps = 0;

	if (!au_info->asym_ae_msk)
		disabled_caps = ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_AUTHENTICATION;

	if (!au_info->sym_ae_msk)
		disabled_caps |= ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_CIPHER | ICP_ACCEL_CAPABILITIES_ZUC |
		    ICP_ACCEL_CAPABILITIES_SHA3_EXT |
		    ICP_ACCEL_CAPABILITIES_SM3 | ICP_ACCEL_CAPABILITIES_SM4 |
		    ICP_ACCEL_CAPABILITIES_CHACHA_POLY |
		    ICP_ACCEL_CAPABILITIES_AESGCM_SPC;

	if (!au_info->dc_ae_msk) {
		disabled_caps |= ICP_ACCEL_CAPABILITIES_COMPRESSION |
		    ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY;
		hw_device->extended_dc_capabilities = 0;
	}

	if (!au_info->inline_ingress_msk && !au_info->inline_egress_msk)
		disabled_caps |= ICP_ACCEL_CAPABILITIES_INLINE;

	hw_device->accel_capabilities_mask =
	    c4xxx_get_hw_cap(accel_dev) & ~disabled_caps;
}

static void
c4xxx_set_sadb_size(struct adf_accel_dev *accel_dev)
{
	u32 sadb_reg_value = 0;
	struct resource *aram_csr_base;

	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;
	if (accel_dev->au_info->num_inline_au) {
		/* REG_SA_DB_CTRL register initialisation */
		sadb_reg_value = ADF_C4XXX_SADB_REG_VALUE(accel_dev);
		ADF_CSR_WR(aram_csr_base,
			   ADF_C4XXX_REG_SA_DB_CTRL,
			   sadb_reg_value);
	} else {
		/* Zero the SADB size when inline is disabled. */
		adf_csr_fetch_and_and(aram_csr_base,
				      ADF_C4XXX_REG_SA_DB_CTRL,
				      ADF_C4XXX_SADB_SIZE_BIT);
	}
	/* REG_SA_CTRL_LOCK register initialisation. We set the lock
	 * bit in order to prevent the REG_SA_DB_CTRL to be
	 * overwritten
	 */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_REG_SA_CTRL_LOCK,
		   ADF_C4XXX_DEFAULT_SA_CTRL_LOCKOUT);
}

static void
c4xxx_init_error_notification_configuration(struct adf_accel_dev *accel_dev,
					    u32 offset)
{
	struct resource *aram_csr_base;

	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;

	/* configure error notification configuration registers */
	/* Set CD Parity error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_RF_PARITY_ERR_0 + offset,
		   ADF_C4XXX_CD_RF_PARITY_ERR_0_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_RF_PARITY_ERR_1 + offset,
		   ADF_C4XXX_CD_RF_PARITY_ERR_1_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_RF_PARITY_ERR_2 + offset,
		   ADF_C4XXX_CD_RF_PARITY_ERR_2_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_RF_PARITY_ERR_3 + offset,
		   ADF_C4XXX_CD_RF_PARITY_ERR_3_VAL);
	/* Set CD RAM ECC Correctable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_CERR + offset,
		   ADF_C4XXX_CD_CERR_VAL);
	/* Set CD RAM ECC UnCorrectable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CD_UERR + offset,
		   ADF_C4XXX_CD_UERR_VAL);
	/* Set Inline (excl cmd_dis) Parity Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_0 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_0_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_1 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_1_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_2 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_2_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_3 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_3_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_4 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_4_VAL);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_INLN_RF_PARITY_ERR_5 + offset,
		   ADF_C4XXX_INLN_RF_PARITY_ERR_5_VAL);
	/* Set Parser RAM ECC Correctable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSER_CERR + offset,
		   ADF_C4XXX_PARSER_CERR_VAL);
	/* Set Parser RAM ECC UnCorrectable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSER_UERR + offset,
		   ADF_C4XXX_PARSER_UERR_VAL);
	/* Set CTPB RAM ECC Correctable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CTPB_CERR + offset,
		   ADF_C4XXX_CTPB_CERR_VAL);
	/* Set CTPB RAM ECC UnCorrectable Error */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CTPB_UERR + offset,
		   ADF_C4XXX_CTPB_UERR_VAL);
	/* Set CPP Interface Status */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CPPM_ERR_STAT + offset,
		   ADF_C4XXX_CPPM_ERR_STAT_VAL);
	/* Set CGST_MGMT_INT */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CONGESTION_MGMT_INT + offset,
		   ADF_C4XXX_CONGESTION_MGMT_INI_VAL);
	/* CPP Interface Status */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_CPPT_ERR_STAT + offset,
		   ADF_C4XXX_CPPT_ERR_STAT_VAL);
	/* MAC Interrupt Mask */
	ADF_CSR_WR64(aram_csr_base,
		     ADF_C4XXX_IC_MAC_IM + offset,
		     ADF_C4XXX_MAC_IM_VAL);
}

static void
c4xxx_enable_parse_extraction(struct adf_accel_dev *accel_dev)
{
	struct resource *aram_csr_base;

	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;

	/* Enable Inline Parse Extraction CRSs */

	/* Set IC_PARSE_CTRL register */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_CTRL_OFFSET,
		   ADF_C4XXX_IC_PARSE_CTRL_OFFSET_DEFAULT_VALUE);

	/* Set IC_PARSE_FIXED_DATA(0) */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_FIXED_DATA(0),
		   ADF_C4XXX_DEFAULT_IC_PARSE_FIXED_DATA_0);

	/* Set IC_PARSE_FIXED_LENGTH */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_FIXED_LENGTH,
		   ADF_C4XXX_DEFAULT_IC_PARSE_FIXED_LEN);

	/* Configure ESP protocol from an IPv4 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_OFFSET_0,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_0_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_LENGTH_0,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_0_VALUE);
	/* Configure protocol extraction field from an IPv4 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_OFFSET_1,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_1_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_LENGTH_1,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_1_VALUE);
	/* Configure SPI extraction field from an IPv4 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_OFFSET_2,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_2_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_LENGTH_2,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_2_VALUE);
	/* Configure destination field IP address from an IPv4 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_OFFSET_3,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_3_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV4_LENGTH_3,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_3_VALUE);

	/* Configure function number extraction field from an IPv6 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_OFFSET_0,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_0_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_LENGTH_0,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_0_VALUE);
	/* Configure protocol extraction field from an IPv6 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_OFFSET_1,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_1_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_LENGTH_1,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_1_VALUE);
	/* Configure SPI extraction field from an IPv6 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_OFFSET_2,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_2_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_LENGTH_2,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_2_VALUE);
	/* Configure destination field IP address from an IPv6 header */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_OFFSET_3,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_3_VALUE);
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_IC_PARSE_IPV6_LENGTH_3,
		   ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_3_VALUE);
}

static int
adf_get_inline_ipsec_algo_group(struct adf_accel_dev *accel_dev,
				unsigned long *ipsec_algo_group)
{
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	if (adf_cfg_get_param_value(
		accel_dev, ADF_INLINE_SEC, ADF_INLINE_IPSEC_ALGO_GROUP, val))
		return EFAULT;
	if (kstrtoul(val, 0, ipsec_algo_group))
		return EFAULT;

	/* Verify the ipsec_algo_group */
	if (*ipsec_algo_group >= IPSEC_ALGO_GROUP_DELIMITER) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Unsupported IPSEC algo group %lu in config file!\n",
		    *ipsec_algo_group);
		return EFAULT;
	}

	return 0;
}

static int
c4xxx_init_inline_hw(struct adf_accel_dev *accel_dev)
{
	u32 sa_entry_reg_value = 0;
	u32 sa_fn_lim = 0;
	u32 supported_algo = 0;
	struct resource *aram_csr_base;
	u32 offset;
	unsigned long ipsec_algo_group = IPSEC_DEFAUL_ALGO_GROUP;

	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;

	if (adf_get_inline_ipsec_algo_group(accel_dev, &ipsec_algo_group))
		return EFAULT;

	sa_entry_reg_value |=
	    (ADF_C4XXX_DEFAULT_LU_KEY_LEN << ADF_C4XXX_LU_KEY_LEN_BIT_OFFSET);
	if (ipsec_algo_group == IPSEC_DEFAUL_ALGO_GROUP) {
		sa_entry_reg_value |= ADF_C4XXX_DEFAULT_SA_SIZE;
		sa_fn_lim =
		    ADF_C4XXX_FUNC_LIMIT(accel_dev, ADF_C4XXX_DEFAULT_SA_SIZE);
		supported_algo = ADF_C4XXX_DEFAULT_SUPPORTED_ALGORITHMS;
	} else if (ipsec_algo_group == IPSEC_ALGO_GROUP1) {
		sa_entry_reg_value |= ADF_C4XXX_ALGO_GROUP1_SA_SIZE;
		sa_fn_lim = ADF_C4XXX_FUNC_LIMIT(accel_dev,
						 ADF_C4XXX_ALGO_GROUP1_SA_SIZE);
		supported_algo = ADF_C4XXX_SUPPORTED_ALGORITHMS_GROUP1;
	} else {
		return EFAULT;
	}

	/* REG_SA_ENTRY_CTRL register initialisation */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_REG_SA_ENTRY_CTRL,
		   sa_entry_reg_value);

	/* REG_SAL_FUNC_LIMITS register initialisation. Only the first register
	 * needs to be initialised to enable as it is assigned to a physical
	 * function. Other registers will be initialised by the LAN PF driver.
	 * The function limits is initialised to its maximal value.
	 */
	ADF_CSR_WR(aram_csr_base, ADF_C4XXX_REG_SA_FUNC_LIMITS, sa_fn_lim);

	/* Initialize REG_SA_SCRATCH[0] register to
	 * advertise supported crypto algorithms
	 */
	ADF_CSR_WR(aram_csr_base, ADF_C4XXX_REG_SA_SCRATCH_0, supported_algo);

	/* REG_SA_SCRATCH[2] register initialisation
	 * to advertise supported crypto offload features.
	 */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_REG_SA_SCRATCH_2,
		   ADF_C4XXX_DEFAULT_CY_OFFLOAD_FEATURES);

	/* Overwrite default MAC_CFG register in ingress offset */
	ADF_CSR_WR64(aram_csr_base,
		     ADF_C4XXX_MAC_CFG + ADF_C4XXX_INLINE_INGRESS_OFFSET,
		     ADF_C4XXX_MAC_CFG_VALUE);

	/* Overwrite default MAC_CFG register in egress offset */
	ADF_CSR_WR64(aram_csr_base,
		     ADF_C4XXX_MAC_CFG + ADF_C4XXX_INLINE_EGRESS_OFFSET,
		     ADF_C4XXX_MAC_CFG_VALUE);

	/* Overwrite default MAC_PIA_CFG
	 * (Packet Interface Adapter Configuration) registers
	 * in ingress offset
	 */
	ADF_CSR_WR64(aram_csr_base,
		     ADF_C4XXX_MAC_PIA_CFG + ADF_C4XXX_INLINE_INGRESS_OFFSET,
		     ADF_C4XXX_MAC_PIA_CFG_VALUE);

	/* Overwrite default MAC_PIA_CFG in egress offset */
	ADF_CSR_WR64(aram_csr_base,
		     ADF_C4XXX_MAC_PIA_CFG + ADF_C4XXX_INLINE_EGRESS_OFFSET,
		     ADF_C4XXX_MAC_PIA_CFG_VALUE);

	c4xxx_enable_parse_extraction(accel_dev);

	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_INGRESS_CMD_DIS_MISC,
		   ADF_C4XXX_REG_CMD_DIS_MISC_DEFAULT_VALUE);

	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_EGRESS_CMD_DIS_MISC,
		   ADF_C4XXX_REG_CMD_DIS_MISC_DEFAULT_VALUE);

	/* Set bits<1:0> in ADF_C4XXX_INLINE_CAPABILITY register to
	 * advertize that both ingress and egress directions are available
	 */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_REG_SA_INLINE_CAPABILITY,
		   ADF_C4XXX_INLINE_CAPABILITIES);

	/* Set error notification configuration of ingress */
	offset = ADF_C4XXX_INLINE_INGRESS_OFFSET;
	c4xxx_init_error_notification_configuration(accel_dev, offset);
	/* Set error notification configuration of egress */
	offset = ADF_C4XXX_INLINE_EGRESS_OFFSET;
	c4xxx_init_error_notification_configuration(accel_dev, offset);

	return 0;
}

static void
adf_enable_inline_notification(struct adf_accel_dev *accel_dev)
{
	struct resource *aram_csr_base;

	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;

	/* Set bit<0> in ADF_C4XXX_REG_SA_INLINE_ENABLE to advertise
	 * that inline is enabled.
	 */
	ADF_CSR_WR(aram_csr_base,
		   ADF_C4XXX_REG_SA_INLINE_ENABLE,
		   ADF_C4XXX_INLINE_ENABLED);
}

static int
c4xxx_init_aram_config(struct adf_accel_dev *accel_dev)
{
	u32 aram_size = ADF_C4XXX_2MB_ARAM_SIZE;
	u32 ibuff_mem_needed = 0;
	u32 usable_aram_size = 0;
	struct adf_hw_aram_info *aram_info;
	u32 sa_db_ctl_value;
	struct resource *aram_csr_base;
	u8 profile = 0;
	u32 sadb_size = 0;
	u32 sa_size = 0;
	unsigned long ipsec_algo_group = IPSEC_DEFAUL_ALGO_GROUP;
	u32 i;

	if (accel_dev->au_info->num_inline_au > 0)
		if (adf_get_inline_ipsec_algo_group(accel_dev,
						    &ipsec_algo_group))
			return EFAULT;

	/* Allocate memory for adf_hw_aram_info */
	aram_info = kzalloc(sizeof(*accel_dev->aram_info), GFP_KERNEL);
	if (!aram_info)
		return ENOMEM;

	/* Initialise Inline direction */
	aram_info->inline_direction_egress_mask = 0;
	if (accel_dev->au_info->num_inline_au) {
		/* Set inline direction bitmap in the ARAM to
		 * inform firmware which ME is egress
		 */
		aram_info->inline_direction_egress_mask =
		    accel_dev->au_info->inline_egress_msk;

		/* User profile is valid, we can now add it
		 * in the ARAM partition table
		 */
		aram_info->inline_congest_mngt_profile = profile;
	}
	/* Initialise DC ME mask, "1" = ME is used for DC operations */
	aram_info->dc_ae_mask = accel_dev->au_info->dc_ae_msk;

	/* Initialise CY ME mask, "1" = ME is used for CY operations
	 * Since asym service can also be enabled on inline AEs, here
	 * we use the sym ae mask for configuring the cy_ae_msk
	 */
	aram_info->cy_ae_mask = accel_dev->au_info->sym_ae_msk;

	/* Configure number of long words in the ARAM */
	aram_info->num_aram_lw_entries = ADF_C4XXX_NUM_ARAM_ENTRIES;

	/* Reset region offset values to 0xffffffff */
	aram_info->mmp_region_offset = ~aram_info->mmp_region_offset;
	aram_info->skm_region_offset = ~aram_info->skm_region_offset;
	aram_info->inter_buff_aram_region_offset =
	    ~aram_info->inter_buff_aram_region_offset;

	/* Determine ARAM size */
	aram_csr_base = (&GET_BARS(accel_dev)[ADF_C4XXX_SRAM_BAR])->virt_addr;
	sa_db_ctl_value = ADF_CSR_RD(aram_csr_base, ADF_C4XXX_REG_SA_DB_CTRL);

	aram_size = (sa_db_ctl_value & ADF_C4XXX_SADB_SIZE_BIT) ?
	    ADF_C4XXX_2MB_ARAM_SIZE :
	    ADF_C4XXX_4MB_ARAM_SIZE;
	device_printf(GET_DEV(accel_dev),
		      "Total available accelerator memory: %uMB\n",
		      aram_size / ADF_C4XXX_1MB_SIZE);

	/* Compute MMP region offset */
	aram_info->mmp_region_size = ADF_C4XXX_DEFAULT_MMP_REGION_SIZE;
	aram_info->mmp_region_offset = aram_size - aram_info->mmp_region_size;

	if (accel_dev->au_info->num_cy_au ||
	    accel_dev->au_info->num_inline_au) {
		/* Crypto is available therefore we must
		 * include space in the ARAM for SKM.
		 */
		aram_info->skm_region_size = ADF_C4XXX_DEFAULT_SKM_REGION_SIZE;
		/* Compute SKM region offset */
		aram_info->skm_region_offset = aram_size -
		    (aram_info->mmp_region_size + aram_info->skm_region_size);
	}

	/* SADB always start at offset 0. */
	if (accel_dev->au_info->num_inline_au) {
		/* Inline is available therefore we must
		 * use remaining ARAM for the SADB.
		 */
		sadb_size = aram_size -
		    (aram_info->mmp_region_size + aram_info->skm_region_size);

		/*
		 * When the inline service is enabled, the policy is that
		 * compression gives up it's space in ARAM to allow for a
		 * larger SADB. Compression must use DRAM instead of ARAM.
		 */
		aram_info->inter_buff_aram_region_size = 0;

		/* the SADB size must be an integral multiple of the SA size */
		if (ipsec_algo_group == IPSEC_DEFAUL_ALGO_GROUP) {
			sa_size = ADF_C4XXX_DEFAULT_SA_SIZE;
		} else {
			/* IPSEC_ALGO_GROUP1
			 * Total 2 algo groups.
			 */
			sa_size = ADF_C4XXX_ALGO_GROUP1_SA_SIZE;
		}

		sadb_size = sadb_size -
		    (sadb_size % ADF_C4XXX_SA_SIZE_IN_BYTES(sa_size));
		aram_info->sadb_region_size = sadb_size;
	}

	if (accel_dev->au_info->num_dc_au &&
	    !accel_dev->au_info->num_inline_au) {
		/* Compression is available therefore we must see if there is
		 * space in the ARAM for intermediate buffers.
		 */
		aram_info->inter_buff_aram_region_size = 0;
		usable_aram_size = aram_size -
		    (aram_info->mmp_region_size + aram_info->skm_region_size);

		for (i = 1; i <= accel_dev->au_info->num_dc_au; i++) {
			if ((i * ADF_C4XXX_AU_COMPR_INTERM_SIZE) >
			    usable_aram_size)
				break;

			ibuff_mem_needed = i * ADF_C4XXX_AU_COMPR_INTERM_SIZE;
		}

		/* Set remaining ARAM to intermediate buffers. Firmware handles
		 * fallback to DRAM for cases were number of AU assigned
		 * to compression exceeds available ARAM memory.
		 */
		aram_info->inter_buff_aram_region_size = ibuff_mem_needed;

		/* If ARAM is used for compression set its initial offset. */
		if (aram_info->inter_buff_aram_region_size)
			aram_info->inter_buff_aram_region_offset = 0;
	}

	accel_dev->aram_info = aram_info;

	return 0;
}

static void
c4xxx_exit_aram_config(struct adf_accel_dev *accel_dev)
{
	kfree(accel_dev->aram_info);
	accel_dev->aram_info = NULL;
}

static u32
get_num_accel_units(struct adf_hw_device_data *self)
{
	u32 i = 0, num_accel = 0;
	unsigned long accel_mask = 0;

	if (!self || !self->accel_mask)
		return 0;

	accel_mask = self->accel_mask;

	for_each_set_bit(i, &accel_mask, ADF_C4XXX_MAX_ACCELERATORS)
	{
		num_accel++;
	}

	return num_accel / ADF_C4XXX_NUM_ACCEL_PER_AU;
}

static int
get_accel_unit(struct adf_hw_device_data *self,
	       struct adf_accel_unit **accel_unit)
{
	enum dev_sku_info sku;

	sku = get_sku(self);

	switch (sku) {
	case DEV_SKU_1:
	case DEV_SKU_1_CY:
		*accel_unit = adf_c4xxx_au_32_ae;
		break;
	case DEV_SKU_2:
	case DEV_SKU_2_CY:
		*accel_unit = adf_c4xxx_au_24_ae;
		break;
	case DEV_SKU_3:
	case DEV_SKU_3_CY:
		*accel_unit = adf_c4xxx_au_12_ae;
		break;
	default:
		*accel_unit = adf_c4xxx_au_emulation;
		break;
	}
	return 0;
}

static int
get_ae_info(struct adf_hw_device_data *self, const struct adf_ae_info **ae_info)
{
	enum dev_sku_info sku;

	sku = get_sku(self);

	switch (sku) {
	case DEV_SKU_1:
		*ae_info = adf_c4xxx_32_ae;
		break;
	case DEV_SKU_1_CY:
		*ae_info = adf_c4xxx_32_ae_sym;
		break;
	case DEV_SKU_2:
		*ae_info = adf_c4xxx_24_ae;
		break;
	case DEV_SKU_2_CY:
		*ae_info = adf_c4xxx_24_ae_sym;
		break;
	case DEV_SKU_3:
		*ae_info = adf_c4xxx_12_ae;
		break;
	case DEV_SKU_3_CY:
		*ae_info = adf_c4xxx_12_ae_sym;
		break;
	default:
		*ae_info = adf_c4xxx_12_ae;
		break;
	}
	return 0;
}

static int
adf_add_debugfs_info(struct adf_accel_dev *accel_dev)
{
	/* Add Accel Unit configuration table to debug FS interface */
	if (c4xxx_init_ae_config(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to create entry for AE configuration\n");
		return EFAULT;
	}

	return 0;
}

static void
adf_remove_debugfs_info(struct adf_accel_dev *accel_dev)
{
	/* Remove Accel Unit configuration table from debug FS interface */
	c4xxx_exit_ae_config(accel_dev);
}

static int
check_svc_to_hw_capabilities(struct adf_accel_dev *accel_dev,
			     const char *svc_name,
			     enum icp_qat_capabilities_mask cap)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 hw_cap = hw_data->accel_capabilities_mask;

	hw_cap &= cap;
	if (hw_cap != cap) {
		device_printf(GET_DEV(accel_dev),
			      "Service not supported by accelerator: %s\n",
			      svc_name);
		return EPERM;
	}

	return 0;
}

static int
check_accel_unit_config(struct adf_accel_dev *accel_dev,
			u8 num_cy_au,
			u8 num_dc_au,
			u8 num_inline_au)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	u32 service_mask = ADF_ACCEL_SERVICE_NULL;
	char *token, *cur_str;
	int ret = 0;

	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	cur_str = val;
	token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	while (token) {
		if (!strncmp(token, ADF_SERVICE_CY, strlen(ADF_SERVICE_CY))) {
			service_mask |= ADF_ACCEL_CRYPTO;
			ret |= check_svc_to_hw_capabilities(
			    accel_dev,
			    token,
			    ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
				ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC);
		}

		if (!strncmp(token, ADF_CFG_SYM, strlen(ADF_CFG_SYM))) {
			service_mask |= ADF_ACCEL_CRYPTO;
			ret |= check_svc_to_hw_capabilities(
			    accel_dev,
			    token,
			    ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC);
		}

		if (!strncmp(token, ADF_CFG_ASYM, strlen(ADF_CFG_ASYM))) {
			/* Handle a special case of services 'asym;inline'
			 * enabled where ASYM is handled by Inline firmware
			 * at AE level. This configuration allows to enable
			 * ASYM service without accel units assigned to
			 * CRYPTO service, e.g.
			 * num_inline_au = 6
			 * num_cy_au = 0
			 */
			if (num_inline_au < num_au)
				service_mask |= ADF_ACCEL_CRYPTO;

			ret |= check_svc_to_hw_capabilities(
			    accel_dev,
			    token,
			    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC);
		}

		if (!strncmp(token, ADF_SERVICE_DC, strlen(ADF_SERVICE_DC))) {
			service_mask |= ADF_ACCEL_COMPRESSION;
			ret |= check_svc_to_hw_capabilities(
			    accel_dev,
			    token,
			    ICP_ACCEL_CAPABILITIES_COMPRESSION);
		}

		if (!strncmp(token,
			     ADF_SERVICE_INLINE,
			     strlen(ADF_SERVICE_INLINE))) {
			service_mask |= ADF_ACCEL_INLINE_CRYPTO;
			ret |= check_svc_to_hw_capabilities(
			    accel_dev, token, ICP_ACCEL_CAPABILITIES_INLINE);
		}

		token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	}

	/* Ensure the user doesn't enable services that are not supported by
	 * accelerator.
	 */
	if (ret) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid accelerator configuration.\n");
		return EFAULT;
	}

	if (!(service_mask & ADF_ACCEL_COMPRESSION) && num_dc_au > 0) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid accel unit config.\n");
		device_printf(
		    GET_DEV(accel_dev),
		    "DC accel units set when dc service not enabled\n");
		return EFAULT;
	}

	if (!(service_mask & ADF_ACCEL_CRYPTO) && num_cy_au > 0) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid accel unit config.\n");
		device_printf(
		    GET_DEV(accel_dev),
		    "CY accel units set when cy service not enabled\n");
		return EFAULT;
	}

	if (!(service_mask & ADF_ACCEL_INLINE_CRYPTO) && num_inline_au > 0) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid accel unit config.\n"
			      "Inline feature not supported.\n");
		return EFAULT;
	}

	hw_data->service_mask = service_mask;
	/* Ensure the user doesn't allocate more than max accel units */
	if (num_au != (num_cy_au + num_dc_au + num_inline_au)) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid accel unit config.\n");
		device_printf(GET_DEV(accel_dev),
			      "Max accel units is %d\n",
			      num_au);
		return EFAULT;
	}

	/* Ensure user allocates hardware resources for enabled services */
	if (!num_cy_au && (service_mask & ADF_ACCEL_CRYPTO)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to enable cy service!\n");
		device_printf(GET_DEV(accel_dev),
			      "%s should not be 0",
			      ADF_NUM_CY_ACCEL_UNITS);
		return EFAULT;
	}
	if (!num_dc_au && (service_mask & ADF_ACCEL_COMPRESSION)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to enable dc service!\n");
		device_printf(GET_DEV(accel_dev),
			      "%s should not be 0",
			      ADF_NUM_DC_ACCEL_UNITS);
		return EFAULT;
	}
	if (!num_inline_au && (service_mask & ADF_ACCEL_INLINE_CRYPTO)) {
		device_printf(GET_DEV(accel_dev), "Failed to enable");
		device_printf(GET_DEV(accel_dev), " inline service!");
		device_printf(GET_DEV(accel_dev),
			      " %s should not be 0\n",
			      ADF_NUM_INLINE_ACCEL_UNITS);
		return EFAULT;
	}

	return 0;
}

static int
get_accel_unit_config(struct adf_accel_dev *accel_dev,
		      u8 *num_cy_au,
		      u8 *num_dc_au,
		      u8 *num_inline_au)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	/* Get the number of accel units allocated for each service */
	snprintf(key, sizeof(key), ADF_NUM_CY_ACCEL_UNITS);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	if (compat_strtou8(val, 10, num_cy_au))
		return EFAULT;
	snprintf(key, sizeof(key), ADF_NUM_DC_ACCEL_UNITS);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	if (compat_strtou8(val, 10, num_dc_au))
		return EFAULT;

	snprintf(key, sizeof(key), ADF_NUM_INLINE_ACCEL_UNITS);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	if (compat_strtou8(val, 10, num_inline_au))
		return EFAULT;

	return 0;
}

/* Function reads the inline ingress/egress configuration
 * and returns the number of AEs reserved for ingress
 * and egress for accel units which are allocated for
 * inline service
 */
static int
adf_get_inline_config(struct adf_accel_dev *accel_dev, u32 *num_ingress_aes)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	char *value;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	unsigned long ingress, egress = 0;
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;
	u32 num_inline_aes = 0, num_ingress_ae = 0;
	u32 i = 0;

	snprintf(key, sizeof(key), ADF_INLINE_INGRESS);
	if (adf_cfg_get_param_value(accel_dev, ADF_INLINE_SEC, key, val)) {
		device_printf(GET_DEV(accel_dev), "Failed to find ingress\n");
		return EFAULT;
	}
	value = val;
	value = strsep(&value, ADF_C4XXX_PERCENTAGE);
	if (compat_strtoul(value, 10, &ingress))
		return EFAULT;

	snprintf(key, sizeof(key), ADF_INLINE_EGRESS);
	if (adf_cfg_get_param_value(accel_dev, ADF_INLINE_SEC, key, val)) {
		device_printf(GET_DEV(accel_dev), "Failed to find egress\n");
		return EFAULT;
	}
	value = val;
	value = strsep(&value, ADF_C4XXX_PERCENTAGE);
	if (compat_strtoul(value, 10, &egress))
		return EFAULT;

	if (ingress + egress != ADF_C4XXX_100) {
		device_printf(GET_DEV(accel_dev),
			      "The sum of ingress and egress should be 100\n");
		return EFAULT;
	}

	for (i = 0; i < num_au; i++) {
		if (accel_unit[i].services == ADF_ACCEL_INLINE_CRYPTO)
			num_inline_aes += accel_unit[i].num_ae;
	}

	num_ingress_ae = num_inline_aes * ingress / ADF_C4XXX_100;
	if (((num_inline_aes * ingress) % ADF_C4XXX_100) >
	    ADF_C4XXX_ROUND_LIMIT)
		num_ingress_ae++;

	*num_ingress_aes = num_ingress_ae;
	return 0;
}

static int
adf_set_inline_ae_mask(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	struct adf_accel_unit_info *au_info = accel_dev->au_info;
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;
	u32 num_ingress_ae = 0;
	u32 ingress_msk = 0;
	u32 i, j, ae_mask;

	if (adf_get_inline_config(accel_dev, &num_ingress_ae))
		return EFAULT;

	for (i = 0; i < num_au; i++) {
		j = 0;
		if (accel_unit[i].services == ADF_ACCEL_INLINE_CRYPTO) {
			/* AEs with inline service enabled are also used
			 * for asymmetric crypto
			 */
			au_info->asym_ae_msk |= accel_unit[i].ae_mask;
			ae_mask = accel_unit[i].ae_mask;
			while (num_ingress_ae && ae_mask) {
				if (ae_mask & 1) {
					ingress_msk |= BIT(j);
					num_ingress_ae--;
				}
				ae_mask = ae_mask >> 1;
				j++;
			}
			au_info->inline_ingress_msk |= ingress_msk;

			au_info->inline_egress_msk |=
			    ~(au_info->inline_ingress_msk) &
			    accel_unit[i].ae_mask;
		}
	}

	return 0;
}

static int
adf_set_ae_mask(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	struct adf_accel_unit_info *au_info = accel_dev->au_info;
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	char *token, *cur_str;
	bool asym_en = false, sym_en = false;
	u32 i;

	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	cur_str = val;
	token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	while (token) {
		if (!strncmp(token, ADF_CFG_ASYM, strlen(ADF_CFG_ASYM)))
			asym_en = true;
		if (!strncmp(token, ADF_CFG_SYM, strlen(ADF_CFG_SYM)))
			sym_en = true;
		if (!strncmp(token, ADF_CFG_CY, strlen(ADF_CFG_CY))) {
			sym_en = true;
			asym_en = true;
		}
		token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	}

	for (i = 0; i < num_au; i++) {
		if (accel_unit[i].services == ADF_ACCEL_CRYPTO) {
			/* AEs that support crypto can perform both
			 * symmetric and asymmetric crypto, however
			 * we only enable the threads if the relevant
			 * service is also enabled
			 */
			if (asym_en)
				au_info->asym_ae_msk |= accel_unit[i].ae_mask;
			if (sym_en)
				au_info->sym_ae_msk |= accel_unit[i].ae_mask;
		} else if (accel_unit[i].services == ADF_ACCEL_COMPRESSION) {
			au_info->dc_ae_msk |= accel_unit[i].comp_ae_mask;
		}
	}
	return 0;
}

static int
adf_init_accel_unit_services(struct adf_accel_dev *accel_dev)
{
	u8 num_cy_au, num_dc_au, num_inline_au;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	struct adf_accel_unit *accel_unit;
	const struct adf_ae_info *ae_info;
	int i;

	if (get_accel_unit_config(
		accel_dev, &num_cy_au, &num_dc_au, &num_inline_au)) {
		device_printf(GET_DEV(accel_dev), "Invalid accel unit cfg\n");
		return EFAULT;
	}

	if (check_accel_unit_config(
		accel_dev, num_cy_au, num_dc_au, num_inline_au))
		return EFAULT;

	accel_dev->au_info = kzalloc(sizeof(*accel_dev->au_info), GFP_KERNEL);
	if (!accel_dev->au_info)
		return ENOMEM;

	accel_dev->au_info->num_cy_au = num_cy_au;
	accel_dev->au_info->num_dc_au = num_dc_au;
	accel_dev->au_info->num_inline_au = num_inline_au;

	if (get_ae_info(hw_data, &ae_info)) {
		device_printf(GET_DEV(accel_dev), "Failed to get ae info\n");
		goto err_au_info;
	}
	accel_dev->au_info->ae_info = ae_info;

	if (get_accel_unit(hw_data, &accel_unit)) {
		device_printf(GET_DEV(accel_dev), "Failed to get accel unit\n");
		goto err_ae_info;
	}

	/* Enable compression accel units */
	/* Accel units with 4AEs are reserved for compression first */
	for (i = num_au - 1; i >= 0 && num_dc_au > 0; i--) {
		if (accel_unit[i].num_ae == ADF_C4XXX_4_AE) {
			accel_unit[i].services = ADF_ACCEL_COMPRESSION;
			num_dc_au--;
		}
	}
	for (i = num_au - 1; i >= 0 && num_dc_au > 0; i--) {
		if (accel_unit[i].services == ADF_ACCEL_SERVICE_NULL) {
			accel_unit[i].services = ADF_ACCEL_COMPRESSION;
			num_dc_au--;
		}
	}

	/* Enable inline accel units */
	for (i = 0; i < num_au && num_inline_au > 0; i++) {
		if (accel_unit[i].services == ADF_ACCEL_SERVICE_NULL) {
			accel_unit[i].services = ADF_ACCEL_INLINE_CRYPTO;
			num_inline_au--;
		}
	}

	/* Enable crypto accel units */
	for (i = 0; i < num_au && num_cy_au > 0; i++) {
		if (accel_unit[i].services == ADF_ACCEL_SERVICE_NULL) {
			accel_unit[i].services = ADF_ACCEL_CRYPTO;
			num_cy_au--;
		}
	}
	accel_dev->au_info->au = accel_unit;
	return 0;

err_ae_info:
	accel_dev->au_info->ae_info = NULL;
err_au_info:
	kfree(accel_dev->au_info);
	accel_dev->au_info = NULL;
	return EFAULT;
}

static void
adf_exit_accel_unit_services(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	int i;

	if (accel_dev->au_info) {
		if (accel_dev->au_info->au) {
			for (i = 0; i < num_au; i++) {
				accel_dev->au_info->au[i].services =
				    ADF_ACCEL_SERVICE_NULL;
			}
		}
		accel_dev->au_info->au = NULL;
		accel_dev->au_info->ae_info = NULL;
		kfree(accel_dev->au_info);
		accel_dev->au_info = NULL;
	}
}

static inline void
adf_c4xxx_reset_hw_units(struct adf_accel_dev *accel_dev)
{
	struct resource *pmisc =
	    (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;

	u32 global_clk_enable = ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ARAM |
	    ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ICI_ENABLE |
	    ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ICE_ENABLE;

	u32 ixp_reset_generic = ADF_C4XXX_IXP_RESET_GENERIC_ARAM |
	    ADF_C4XXX_IXP_RESET_GENERIC_INLINE_EGRESS |
	    ADF_C4XXX_IXP_RESET_GENERIC_INLINE_INGRESS;

	/* To properly reset each of the units driver must:
	 * 1)Call out resetactive state using ixp reset generic
	 *   register;
	 * 2)Disable generic clock;
	 * 3)Take device out of reset by clearing ixp reset
	 *   generic register;
	 * 4)Re-enable generic clock;
	 */
	ADF_CSR_WR(pmisc, ADF_C4XXX_IXP_RESET_GENERIC, ixp_reset_generic);
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC,
		   ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_DISABLE_ALL);
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_IXP_RESET_GENERIC,
		   ADF_C4XXX_IXP_RESET_GENERIC_OUT_OF_RESET_TRIGGER);
	ADF_CSR_WR(pmisc,
		   ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC,
		   global_clk_enable);
}

static int
adf_init_accel_units(struct adf_accel_dev *accel_dev)
{
	struct resource *csr =
	    (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;

	if (adf_init_accel_unit_services(accel_dev))
		return EFAULT;

	/* Set cy and dc enabled AE masks */
	if (accel_dev->au_info->num_cy_au || accel_dev->au_info->num_dc_au) {
		if (adf_set_ae_mask(accel_dev)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to set ae masks\n");
			goto err_au;
		}
	}
	/* Set ingress/egress ae mask if inline is enabled */
	if (accel_dev->au_info->num_inline_au) {
		if (adf_set_inline_ae_mask(accel_dev)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to set inline ae masks\n");
			goto err_au;
		}
	}
	/* Define ARAM regions */
	if (c4xxx_init_aram_config(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to init aram config\n");
		goto err_au;
	}
	/* Configure h/w registers for inline operations */
	if (accel_dev->au_info->num_inline_au > 0)
		/* Initialise configuration parsing registers */
		if (c4xxx_init_inline_hw(accel_dev))
			goto err_au;

	c4xxx_set_sadb_size(accel_dev);

	if (accel_dev->au_info->num_inline_au > 0) {
		/* ici/ice interrupt shall be enabled after msi-x enabled */
		ADF_CSR_WR(csr,
			   ADF_C4XXX_ERRMSK11,
			   ADF_C4XXX_ERRMSK11_ERR_DISABLE_ICI_ICE_INTR);
		adf_enable_inline_notification(accel_dev);
	}

	update_hw_capability(accel_dev);
	if (adf_add_debugfs_info(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to add debug FS information\n");
		goto err_au;
	}
	return 0;

err_au:
	/* Free and clear accel unit data structures */
	adf_exit_accel_unit_services(accel_dev);
	return EFAULT;
}

static void
adf_exit_accel_units(struct adf_accel_dev *accel_dev)
{
	adf_exit_accel_unit_services(accel_dev);
	/* Free aram mapping structure */
	c4xxx_exit_aram_config(accel_dev);
	/* Remove entries in debug FS */
	adf_remove_debugfs_info(accel_dev);
}

static const char *
get_obj_name(struct adf_accel_dev *accel_dev,
	     enum adf_accel_unit_services service)
{
	u32 capabilities = GET_HW_DATA(accel_dev)->accel_capabilities_mask;
	bool sym_only_sku = false;

	/* Check if SKU is capable only of symmetric cryptography
	 * via device capabilities.
	 */
	if ((capabilities & ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC) &&
	    !(capabilities & ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) &&
	    !(capabilities & ADF_ACCEL_CAPABILITIES_COMPRESSION))
		sym_only_sku = true;

	switch (service) {
	case ADF_ACCEL_INLINE_CRYPTO:
		return ADF_C4XXX_INLINE_OBJ;
	case ADF_ACCEL_CRYPTO:
		if (sym_only_sku)
			return ADF_C4XXX_SYM_OBJ;
		else
			return ADF_C4XXX_CY_OBJ;
		break;
	case ADF_ACCEL_COMPRESSION:
		return ADF_C4XXX_DC_OBJ;
	default:
		return NULL;
	}
}

static uint32_t
get_objs_num(struct adf_accel_dev *accel_dev)
{
	u32 srv = 0;
	u32 max_srv_id = 0;
	unsigned long service_mask = accel_dev->hw_device->service_mask;

	/* The objects number corresponds to the number of services */
	for_each_set_bit(srv, &service_mask, ADF_C4XXX_MAX_OBJ)
	{
		max_srv_id = srv;
	}

	return (max_srv_id + 1);
}

static uint32_t
get_obj_cfg_ae_mask(struct adf_accel_dev *accel_dev,
		    enum adf_accel_unit_services service)
{
	u32 ae_mask = 0;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;
	u32 i = 0;

	if (service == ADF_ACCEL_SERVICE_NULL)
		return 0;

	for (i = 0; i < num_au; i++) {
		if (accel_unit[i].services == service)
			ae_mask |= accel_unit[i].ae_mask;
	}
	return ae_mask;
}

static void
configure_iov_threads(struct adf_accel_dev *accel_dev, bool enable)
{
	struct resource *addr;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_aes = hw_data->get_num_aes(hw_data);
	u32 reg = 0x0;
	u32 i;

	addr = (&GET_BARS(accel_dev)[ADF_C4XXX_PMISC_BAR])->virt_addr;

	/* Set/Unset Valid bits in AE Thread to PCIe Function Mapping */
	for (i = 0; i < ADF_C4XXX_AE2FUNC_REG_PER_AE * num_aes; i++) {
		reg = ADF_CSR_RD(addr + ADF_C4XXX_AE2FUNC_MAP_OFFSET,
				 i * ADF_C4XXX_AE2FUNC_MAP_REG_SIZE);
		if (enable)
			reg |= ADF_C4XXX_AE2FUNC_MAP_VALID;
		else
			reg &= ~ADF_C4XXX_AE2FUNC_MAP_VALID;
		ADF_CSR_WR(addr + ADF_C4XXX_AE2FUNC_MAP_OFFSET,
			   i * ADF_C4XXX_AE2FUNC_MAP_REG_SIZE,
			   reg);
	}
}

void
adf_init_hw_data_c4xxx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class = &c4xxx_class;
	hw_data->instance_id = c4xxx_class.instances++;
	hw_data->num_banks = ADF_C4XXX_ETR_MAX_BANKS;
	hw_data->num_rings_per_bank = ADF_C4XXX_NUM_RINGS_PER_BANK;
	hw_data->num_accel = ADF_C4XXX_MAX_ACCELERATORS;
	hw_data->num_engines = ADF_C4XXX_MAX_ACCELENGINES;
	hw_data->num_logical_accel = 1;
	hw_data->tx_rx_gap = ADF_C4XXX_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_C4XXX_TX_RINGS_MASK;
	hw_data->alloc_irq = adf_isr_resource_alloc;
	hw_data->free_irq = adf_isr_resource_free;
	hw_data->enable_error_correction = adf_enable_error_correction;
	hw_data->init_ras = adf_init_ras;
	hw_data->exit_ras = adf_exit_ras;
	hw_data->ras_interrupts = adf_ras_interrupts;
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_num_accel_units = get_num_accel_units;
	hw_data->get_sram_bar_id = get_sram_bar_id;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_arb_info = get_arb_info;
	hw_data->get_admin_info = get_admin_info;
	hw_data->get_errsou_offset = get_errsou_offset;
	hw_data->get_clock_speed = get_clock_speed;
	hw_data->get_eth_doorbell_msg = get_eth_doorbell_msg;
	hw_data->get_sku = get_sku;
	hw_data->heartbeat_ctr_num = ADF_NUM_THREADS_PER_AE;
	hw_data->check_prod_sku = c4xxx_check_prod_sku;
	hw_data->fw_name = ADF_C4XXX_FW;
	hw_data->fw_mmp_name = ADF_C4XXX_MMP;
	hw_data->get_obj_name = get_obj_name;
	hw_data->get_objs_num = get_objs_num;
	hw_data->get_obj_cfg_ae_mask = get_obj_cfg_ae_mask;
	hw_data->init_admin_comms = adf_init_admin_comms;
	hw_data->exit_admin_comms = adf_exit_admin_comms;
	hw_data->configure_iov_threads = configure_iov_threads;
	hw_data->disable_iov = adf_disable_sriov;
	hw_data->send_admin_init = adf_send_admin_init;
	hw_data->init_arb = adf_init_arb_c4xxx;
	hw_data->exit_arb = adf_exit_arb_c4xxx;
	hw_data->disable_arb = adf_disable_arb;
	hw_data->enable_ints = adf_enable_ints;
	hw_data->set_ssm_wdtimer = c4xxx_set_ssm_wdtimer;
	hw_data->check_slice_hang = c4xxx_check_slice_hang;
	hw_data->reset_device = adf_reset_flr;
	hw_data->restore_device = adf_c4xxx_dev_restore;
	hw_data->init_accel_units = adf_init_accel_units;
	hw_data->reset_hw_units = adf_c4xxx_reset_hw_units;
	hw_data->exit_accel_units = adf_exit_accel_units;
	hw_data->ring_to_svc_map = ADF_DEFAULT_RING_TO_SRV_MAP;
	hw_data->get_heartbeat_status = adf_get_heartbeat_status;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->clock_frequency = ADF_C4XXX_AE_FREQ;
	hw_data->measure_clock = measure_clock;
	hw_data->add_pke_stats = adf_pke_replay_counters_add_c4xxx;
	hw_data->remove_pke_stats = adf_pke_replay_counters_remove_c4xxx;
	hw_data->add_misc_error = adf_misc_error_add_c4xxx;
	hw_data->remove_misc_error = adf_misc_error_remove_c4xxx;
	hw_data->extended_dc_capabilities = 0;
	hw_data->get_storage_enabled = get_storage_enabled;
	hw_data->query_storage_cap = 0;
	hw_data->get_accel_cap = c4xxx_get_hw_cap;
	hw_data->configure_accel_units = c4xxx_configure_accel_units;
	hw_data->pre_reset = adf_dev_pre_reset;
	hw_data->post_reset = adf_dev_post_reset;
	hw_data->get_ring_to_svc_map = adf_cfg_get_services_enabled;
	hw_data->count_ras_event = adf_fw_count_ras_event;
	hw_data->config_device = adf_config_device;
	hw_data->set_asym_rings_mask = adf_cfg_set_asym_rings_mask;

	adf_gen2_init_hw_csr_info(&hw_data->csr_info);
	adf_gen2_init_pf_pfvf_ops(&hw_data->csr_info.pfvf_ops);
	hw_data->csr_info.arb_enable_mask = 0xF;
}

void
adf_clean_hw_data_c4xxx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
}

void
remove_oid(struct adf_accel_dev *accel_dev, struct sysctl_oid *oid)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	int ret;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);

	ret = sysctl_ctx_entry_del(qat_sysctl_ctx, oid);
	if (ret)
		device_printf(GET_DEV(accel_dev), "Failed to delete entry\n");

	ret = sysctl_remove_oid(oid, 1, 1);
	if (ret)
		device_printf(GET_DEV(accel_dev), "Failed to delete oid\n");
}
