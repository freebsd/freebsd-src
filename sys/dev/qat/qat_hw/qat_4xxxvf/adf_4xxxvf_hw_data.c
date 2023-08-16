/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include <adf_accel_devices.h>
#include <adf_cfg.h>
#include <adf_common_drv.h>
#include <adf_gen4vf_hw_csr_data.h>
#include <adf_gen4_pfvf.h>
#include <adf_pfvf_vf_msg.h>
#include "adf_4xxxvf_hw_data.h"
#include "icp_qat_hw.h"
#include "adf_transport_internal.h"
#include "adf_pfvf_vf_proto.h"

static struct adf_hw_device_class adf_4xxxiov_class =
    { .name = ADF_4XXXVF_DEVICE_NAME, .type = DEV_4XXXVF, .instances = 0 };

#define ADF_4XXXIOV_DEFAULT_RING_TO_SRV_MAP                                    \
	(ASYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                        \
	 ASYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 SYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_ASYM_SYM ADF_4XXXIOV_DEFAULT_RING_TO_SRV_MAP

#define ADF_4XXXIOV_DC                                                         \
	(COMP | COMP << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_SYM                                                        \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 SYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                               \
	 SYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_ASYM                                                       \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 ASYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 ASYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_ASYM_DC                                                    \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_SYM_DC                                                     \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXXIOV_NA                                                         \
	(NA | NA << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                           \
	 NA << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                                \
	 NA << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

struct adf_enabled_services {
	const char svcs_enabled[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u16 rng_to_svc_msk;
};

static struct adf_enabled_services adf_4xxxiov_svcs[] =
    { { "dc", ADF_4XXXIOV_DC },
      { "sym", ADF_4XXXIOV_SYM },
      { "asym", ADF_4XXXIOV_ASYM },
      { "dc;asym", ADF_4XXXIOV_ASYM_DC },
      { "asym;dc", ADF_4XXXIOV_ASYM_DC },
      { "sym;dc", ADF_4XXXIOV_SYM_DC },
      { "dc;sym", ADF_4XXXIOV_SYM_DC },
      { "asym;sym", ADF_4XXXIOV_ASYM_SYM },
      { "sym;asym", ADF_4XXXIOV_ASYM_SYM },
      { "cy", ADF_4XXXIOV_ASYM_SYM } };

static u32
get_accel_mask(struct adf_accel_dev *accel_dev)
{
	return ADF_4XXXIOV_ACCELERATORS_MASK;
}

static u32
get_ae_mask(struct adf_accel_dev *accel_dev)
{
	return ADF_4XXXIOV_ACCELENGINES_MASK;
}

static u32
get_num_accels(struct adf_hw_device_data *self)
{
	return ADF_4XXXIOV_MAX_ACCELERATORS;
}

static u32
get_num_aes(struct adf_hw_device_data *self)
{
	return ADF_4XXXIOV_MAX_ACCELENGINES;
}

static u32
get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_4XXXIOV_PMISC_BAR;
}

static u32
get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_4XXXIOV_ETR_BAR;
}

static u32
get_clock_speed(struct adf_hw_device_data *self)
{
	/* CPP clock is half high-speed clock */
	return self->clock_frequency / 2;
}

static enum dev_sku_info
get_sku(struct adf_hw_device_data *self)
{
	return DEV_SKU_VF;
}

static int
adf_vf_int_noop(struct adf_accel_dev *accel_dev)
{
	return 0;
}

static void
adf_vf_void_noop(struct adf_accel_dev *accel_dev)
{
}

u32
adf_4xxxvf_get_hw_cap(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 vffusectl1;
	u32 capabilities;

	capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC +
	    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC +
	    ICP_ACCEL_CAPABILITIES_CIPHER +
	    ICP_ACCEL_CAPABILITIES_AUTHENTICATION +
	    ICP_ACCEL_CAPABILITIES_COMPRESSION +
	    ICP_ACCEL_CAPABILITIES_SHA3_EXT + ICP_ACCEL_CAPABILITIES_SM2 +
	    ICP_ACCEL_CAPABILITIES_SM3 + ICP_ACCEL_CAPABILITIES_SM4 +
	    ICP_ACCEL_CAPABILITIES_CHACHA_POLY +
	    ICP_ACCEL_CAPABILITIES_AESGCM_SPC +
	    ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY64 +
	    ICP_ACCEL_CAPABILITIES_LZ4_COMPRESSION +
	    ICP_ACCEL_CAPABILITIES_LZ4S_COMPRESSION;

	/* Get fused capabilities */
	vffusectl1 = pci_read_config(pdev, ADF_4XXXIOV_VFFUSECTL1_OFFSET, 4);

	if (vffusectl1 & BIT(7)) {
		capabilities &=
		    ~(ICP_ACCEL_CAPABILITIES_SM3 + ICP_ACCEL_CAPABILITIES_SM4);
	}
	if (vffusectl1 & BIT(6)) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SM3;
	}
	if (vffusectl1 & BIT(3)) {
		capabilities &= ~(ICP_ACCEL_CAPABILITIES_COMPRESSION +
				  ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY64);
	}
	if (vffusectl1 & BIT(2)) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
	}
	if (vffusectl1 & BIT(1)) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
	}
	if (vffusectl1 & BIT(0)) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}
	return capabilities;
}

static void
adf_set_asym_rings_mask(struct adf_accel_dev *accel_dev)
{
	accel_dev->hw_device->asym_rings_mask = ADF_4XXX_DEF_ASYM_MASK;
}

static void
enable_pf2vm_interrupt(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data;
	struct adf_bar *pmisc;
	struct resource *pmisc_bar_addr;

	hw_data = accel_dev->hw_device;
	pmisc = &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	pmisc_bar_addr = pmisc->virt_addr;

	ADF_CSR_WR(pmisc_bar_addr, ADF_4XXXIOV_VINTMSKPF2VM_OFFSET, 0x0);
}

static void
disable_pf2vm_interrupt(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data;
	struct adf_bar *pmisc;
	struct resource *pmisc_bar_addr;

	hw_data = accel_dev->hw_device;
	pmisc = &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	pmisc_bar_addr = pmisc->virt_addr;

	ADF_CSR_WR(pmisc_bar_addr, ADF_4XXXIOV_VINTMSKPF2VM_OFFSET, BIT(0));
}

static int
interrupt_active_pf2vm(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data;
	struct adf_bar *pmisc;
	struct resource *pmisc_bar_addr;
	u32 v_sou, v_msk;

	hw_data = accel_dev->hw_device;
	pmisc = &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	pmisc_bar_addr = pmisc->virt_addr;

	v_sou = ADF_CSR_RD(pmisc_bar_addr, ADF_4XXXIOV_VINTSOUPF2VM_OFFSET);
	v_msk = ADF_CSR_RD(pmisc_bar_addr, ADF_4XXXIOV_VINTMSKPF2VM_OFFSET);

	return ((v_sou & ~v_msk) & BIT(0)) ? 1 : 0;
}

static int
get_int_active_bundles(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data;
	struct adf_bar *pmisc;
	struct resource *pmisc_bar_addr;
	u32 v_sou, v_msk;

	hw_data = accel_dev->hw_device;
	pmisc = &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	pmisc_bar_addr = pmisc->virt_addr;

	v_sou = ADF_CSR_RD(pmisc_bar_addr, ADF_4XXXIOV_VINTSOU_OFFSET);
	v_msk = ADF_CSR_RD(pmisc_bar_addr, ADF_4XXXIOV_VINTMSK_OFFSET);

	return v_sou & ~v_msk & 0xF;
}

static void
get_ring_svc_map_data(int ring_pair_index,
		      u16 ring_to_svc_map,
		      u8 *serv_type,
		      int *ring_index,
		      int *num_rings_per_srv,
		      int bank_num)
{
	*serv_type =
	    GET_SRV_TYPE(ring_to_svc_map, bank_num % ADF_CFG_NUM_SERVICES);
	*ring_index = 0;
	*num_rings_per_srv = ADF_4XXXIOV_NUM_RINGS_PER_BANK / 2;
}

static int
get_ring_to_svc_map(struct adf_accel_dev *accel_dev, u16 *ring_to_svc_map)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	u32 i = 0;

	/* Get the services enabled by user if provided.
	 * The function itself will also be called during the driver probe
	 * procedure where no ServicesEnable is provided. Then the device
	 * should still start with default configuration without
	 * ServicesEnable. Hence it still returns 0 when the
	 * adf_cfg_get_param_value() function returns failure.
	 */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return 0;

	for (i = 0; i < ARRAY_SIZE(adf_4xxxiov_svcs); i++) {
		if (!strncmp(val,
			     adf_4xxxiov_svcs[i].svcs_enabled,
			     ADF_CFG_MAX_KEY_LEN_IN_BYTES)) {
			*ring_to_svc_map = adf_4xxxiov_svcs[i].rng_to_svc_msk;
			return 0;
		}
	}

	device_printf(GET_DEV(accel_dev),
		      "Invalid services enabled: %s\n",
		      val);
	return EFAULT;
}

static int
adf_4xxxvf_ring_pair_reset(struct adf_accel_dev *accel_dev, u32 bank_number)
{
	struct pfvf_message req = { 0 };
	unsigned long timeout = msecs_to_jiffies(ADF_PFVF_MSG_RESP_TIMEOUT);
	int ret = 0;

	if (bank_number >= accel_dev->hw_device->num_banks)
		return EINVAL;

	req.type = ADF_VF2PF_MSGTYPE_RP_RESET;
	req.data = bank_number;
	mutex_lock(&accel_dev->u1.vf.rpreset_lock);
	init_completion(&accel_dev->u1.vf.msg_received);
	accel_dev->u1.vf.rpreset_sts = RPRESET_SUCCESS;
	if (adf_send_vf2pf_msg(accel_dev, req)) {
		device_printf(GET_DEV(accel_dev),
			      "vf ring pair reset failure (vf2pf msg error)\n");
		ret = EFAULT;
		goto out;
	}
	if (!wait_for_completion_timeout(&accel_dev->u1.vf.msg_received,
					 timeout)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "vf ring pair reset failure (pf2vf msg timeout)\n");
		ret = EFAULT;
		goto out;
	}
	if (accel_dev->u1.vf.rpreset_sts != RPRESET_SUCCESS) {
		device_printf(
		    GET_DEV(accel_dev),
		    "vf ring pair reset failure (pf reports error)\n");
		ret = EFAULT;
		goto out;
	}

out:
	mutex_unlock(&accel_dev->u1.vf.rpreset_lock);
	return ret;
}

void
adf_init_hw_data_4xxxiov(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class = &adf_4xxxiov_class;
	hw_data->num_banks = ADF_4XXXIOV_ETR_MAX_BANKS;
	hw_data->num_rings_per_bank = ADF_4XXXIOV_NUM_RINGS_PER_BANK;
	hw_data->num_accel = ADF_4XXXIOV_MAX_ACCELERATORS;
	hw_data->num_logical_accel = 1;
	hw_data->num_engines = ADF_4XXXIOV_MAX_ACCELENGINES;
	hw_data->tx_rx_gap = ADF_4XXXIOV_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_4XXXIOV_TX_RINGS_MASK;
	hw_data->ring_to_svc_map = ADF_4XXXIOV_DEFAULT_RING_TO_SRV_MAP;
	hw_data->alloc_irq = adf_vf_isr_resource_alloc;
	hw_data->free_irq = adf_vf_isr_resource_free;
	hw_data->enable_error_correction = adf_vf_void_noop;
	hw_data->init_admin_comms = adf_vf_int_noop;
	hw_data->exit_admin_comms = adf_vf_void_noop;
	hw_data->send_admin_init = adf_vf2pf_notify_init;
	hw_data->init_arb = adf_vf_int_noop;
	hw_data->exit_arb = adf_vf_void_noop;
	hw_data->disable_iov = adf_vf2pf_notify_shutdown;
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_clock_speed = get_clock_speed;
	hw_data->get_sku = get_sku;
	hw_data->enable_ints = adf_vf_void_noop;
	hw_data->reset_device = adf_reset_flr;
	hw_data->restore_device = adf_dev_restore;
	hw_data->get_ring_svc_map_data = get_ring_svc_map_data;
	hw_data->get_ring_to_svc_map = get_ring_to_svc_map;
	hw_data->get_accel_cap = adf_4xxxvf_get_hw_cap;
	hw_data->config_device = adf_config_device;
	hw_data->set_asym_rings_mask = adf_set_asym_rings_mask;
	hw_data->ring_pair_reset = adf_4xxxvf_ring_pair_reset;
	hw_data->enable_pf2vf_interrupt = enable_pf2vm_interrupt;
	hw_data->disable_pf2vf_interrupt = disable_pf2vm_interrupt;
	hw_data->interrupt_active_pf2vf = interrupt_active_pf2vm;
	hw_data->get_int_active_bundles = get_int_active_bundles;
	hw_data->dev_class->instances++;
	adf_devmgr_update_class_index(hw_data);
	gen4vf_init_hw_csr_info(&hw_data->csr_info);
	adf_gen4_init_vf_pfvf_ops(&hw_data->csr_info.pfvf_ops);
}

void
adf_clean_hw_data_4xxxiov(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
	adf_devmgr_update_class_index(hw_data);
}
