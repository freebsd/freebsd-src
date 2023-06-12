/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include <linux/iopoll.h>
#include <adf_accel_devices.h>
#include <adf_cfg.h>
#include <adf_common_drv.h>
#include <adf_dev_err.h>
#include <adf_pfvf_msg.h>
#include <adf_gen4_hw_data.h>
#include <adf_gen4_pfvf.h>
#include <adf_gen4_timer.h>
#include "adf_4xxx_hw_data.h"
#include "adf_heartbeat.h"
#include "icp_qat_fw_init_admin.h"
#include "icp_qat_hw.h"

#define ADF_CONST_TABLE_SIZE 1024

struct adf_fw_config {
	u32 ae_mask;
	char *obj_name;
};

/* Accel unit information */
static const struct adf_accel_unit adf_4xxx_au_a_ae[] = {
	{ 0x1, 0x1, 0xF, 0x1B, 4, ADF_ACCEL_SERVICE_NULL },
	{ 0x2, 0x1, 0xF0, 0x6C0, 4, ADF_ACCEL_SERVICE_NULL },
	{ 0x4, 0x1, 0x100, 0xF000, 1, ADF_ACCEL_ADMIN },
};

/* Worker thread to service arbiter mappings */
static u32 thrd_to_arb_map[ADF_4XXX_MAX_ACCELENGINES] = { 0x5555555, 0x5555555,
							  0x5555555, 0x5555555,
							  0xAAAAAAA, 0xAAAAAAA,
							  0xAAAAAAA, 0xAAAAAAA,
							  0x0 };

/* Masks representing ME thread-service mappings.
 * Thread 7 carries out Admin work and is thus
 * left out.
 */
static u8 default_active_thd_mask = 0x7F;
static u8 dc_me_active_thd_mask = 0x03;

static u32 thrd_to_arb_map_gen[ADF_4XXX_MAX_ACCELENGINES] = { 0 };

#define ADF_4XXX_ASYM_SYM                                                      \
	(ASYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                        \
	 ASYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 SYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_DC                                                            \
	(COMP | COMP << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_SYM                                                           \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 SYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                               \
	 SYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_ASYM                                                          \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 ASYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 ASYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_ASYM_DC                                                       \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_SYM_DC                                                        \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_NA                                                            \
	(NA | NA << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                           \
	 NA << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                                \
	 NA << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_4XXX_DEFAULT_RING_TO_SRV_MAP ADF_4XXX_ASYM_SYM

struct adf_enabled_services {
	const char svcs_enabled[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u16 rng_to_svc_msk;
};

static struct adf_enabled_services adf_4xxx_svcs[] =
    { { "dc", ADF_4XXX_DC },
      { "sym", ADF_4XXX_SYM },
      { "asym", ADF_4XXX_ASYM },
      { "dc;asym", ADF_4XXX_ASYM_DC },
      { "asym;dc", ADF_4XXX_ASYM_DC },
      { "sym;dc", ADF_4XXX_SYM_DC },
      { "dc;sym", ADF_4XXX_SYM_DC },
      { "asym;sym", ADF_4XXX_ASYM_SYM },
      { "sym;asym", ADF_4XXX_ASYM_SYM },
      { "cy", ADF_4XXX_ASYM_SYM } };

static struct adf_hw_device_class adf_4xxx_class = {
	.name = ADF_4XXX_DEVICE_NAME,
	.type = DEV_4XXX,
	.instances = 0,
};

static u32
get_accel_mask(struct adf_accel_dev *accel_dev)
{
	return ADF_4XXX_ACCELERATORS_MASK;
}

static u32
get_ae_mask(struct adf_accel_dev *accel_dev)
{
	u32 fusectl4 = accel_dev->hw_device->fuses;

	return ~fusectl4 & ADF_4XXX_ACCELENGINES_MASK;
}

static void
adf_set_asym_rings_mask(struct adf_accel_dev *accel_dev)
{
	accel_dev->hw_device->asym_rings_mask = ADF_4XXX_DEF_ASYM_MASK;
}

static int
get_ring_to_svc_map(struct adf_accel_dev *accel_dev, u16 *ring_to_svc_map)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	u32 i = 0;

	*ring_to_svc_map = 0;
	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;

	for (i = 0; i < ARRAY_SIZE(adf_4xxx_svcs); i++) {
		if (!strncmp(val,
			     adf_4xxx_svcs[i].svcs_enabled,
			     ADF_CFG_MAX_KEY_LEN_IN_BYTES)) {
			*ring_to_svc_map = adf_4xxx_svcs[i].rng_to_svc_msk;
			return 0;
		}
	}

	device_printf(GET_DEV(accel_dev),
		      "Invalid services enabled: %s\n",
		      val);
	return EFAULT;
}

static u32
get_num_accels(struct adf_hw_device_data *self)
{
	return ADF_4XXX_MAX_ACCELERATORS;
}

static u32
get_num_aes(struct adf_hw_device_data *self)
{
	if (!self || !self->ae_mask)
		return 0;

	return hweight32(self->ae_mask);
}

static u32
get_misc_bar_id(struct adf_hw_device_data *self)
{
	return ADF_4XXX_PMISC_BAR;
}

static u32
get_etr_bar_id(struct adf_hw_device_data *self)
{
	return ADF_4XXX_ETR_BAR;
}

static u32
get_sram_bar_id(struct adf_hw_device_data *self)
{
	return ADF_4XXX_SRAM_BAR;
}

/*
 * The vector routing table is used to select the MSI-X entry to use for each
 * interrupt source.
 * The first ADF_4XXX_ETR_MAX_BANKS entries correspond to ring interrupts.
 * The final entry corresponds to VF2PF or error interrupts.
 * This vector table could be used to configure one MSI-X entry to be shared
 * between multiple interrupt sources.
 *
 * The default routing is set to have a one to one correspondence between the
 * interrupt source and the MSI-X entry used.
 */
static void
set_msix_default_rttable(struct adf_accel_dev *accel_dev)
{
	struct resource *csr;
	int i;

	csr = (&GET_BARS(accel_dev)[ADF_4XXX_PMISC_BAR])->virt_addr;
	for (i = 0; i <= ADF_4XXX_ETR_MAX_BANKS; i++)
		ADF_CSR_WR(csr, ADF_4XXX_MSIX_RTTABLE_OFFSET(i), i);
}

static u32
adf_4xxx_get_hw_cap(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 fusectl1;
	u32 capabilities;

	/* Read accelerator capabilities mask */
	fusectl1 = pci_read_config(pdev, ADF_4XXX_FUSECTL1_OFFSET, 4);
	capabilities = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
	    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
	    ICP_ACCEL_CAPABILITIES_CIPHER |
	    ICP_ACCEL_CAPABILITIES_AUTHENTICATION |
	    ICP_ACCEL_CAPABILITIES_COMPRESSION |
	    ICP_ACCEL_CAPABILITIES_LZ4_COMPRESSION |
	    ICP_ACCEL_CAPABILITIES_LZ4S_COMPRESSION |
	    ICP_ACCEL_CAPABILITIES_SHA3 | ICP_ACCEL_CAPABILITIES_HKDF |
	    ICP_ACCEL_CAPABILITIES_SHA3_EXT | ICP_ACCEL_CAPABILITIES_SM3 |
	    ICP_ACCEL_CAPABILITIES_SM4 | ICP_ACCEL_CAPABILITIES_CHACHA_POLY |
	    ICP_ACCEL_CAPABILITIES_AESGCM_SPC | ICP_ACCEL_CAPABILITIES_AES_V2 |
	    ICP_ACCEL_CAPABILITIES_RL | ICP_ACCEL_CAPABILITIES_ECEDMONT |
	    ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY64;

	if (fusectl1 & ICP_ACCEL_4XXX_MASK_CIPHER_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_HKDF;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}
	if (fusectl1 & ICP_ACCEL_4XXX_MASK_AUTH_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SHA3;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SHA3_EXT;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}
	if (fusectl1 & ICP_ACCEL_MASK_PKE_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_ECEDMONT;
	}
	if (fusectl1 & ICP_ACCEL_4XXX_MASK_COMPRESS_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_COMPRESSION;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_LZ4_COMPRESSION;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_LZ4S_COMPRESSION;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY64;
	}
	if (fusectl1 & ICP_ACCEL_4XXX_MASK_SMX_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SM3;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_SM4;
	}
	if (fusectl1 & ICP_ACCEL_4XXX_MASK_UCS_SLICE) {
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CHACHA_POLY;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AESGCM_SPC;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_AES_V2;
		capabilities &= ~ICP_ACCEL_CAPABILITIES_CIPHER;
	}

	return capabilities;
}

static u32
get_hb_clock(struct adf_hw_device_data *self)
{
	/*
	 * 4XXX uses KPT counter for HB
	 */
	return ADF_4XXX_KPT_COUNTER_FREQ;
}

static u32
get_ae_clock(struct adf_hw_device_data *self)
{
	/*
	 * Clock update interval is <16> ticks for qat_4xxx.
	 */
	return self->clock_frequency / 16;
}

static int
measure_clock(struct adf_accel_dev *accel_dev)
{
	u32 frequency;
	int ret = 0;

	ret = adf_dev_measure_clock(accel_dev,
				    &frequency,
				    ADF_4XXX_MIN_AE_FREQ,
				    ADF_4XXX_MAX_AE_FREQ);
	if (ret)
		return ret;

	accel_dev->hw_device->clock_frequency = frequency;
	return 0;
}

static int
adf_4xxx_configure_accel_units(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES] = { 0 };
	char val_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };

	if (adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC))
		goto err;

	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	snprintf(val_str,
		 sizeof(val_str),
		 ADF_CFG_ASYM ADF_SERVICES_SEPARATOR ADF_CFG_SYM);

	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)val_str, ADF_STR))
		goto err;

	return 0;
err:
	device_printf(GET_DEV(accel_dev), "Failed to configure accel units\n");
	return EINVAL;
}

static u32
get_num_accel_units(struct adf_hw_device_data *self)
{
	return ADF_4XXX_MAX_ACCELUNITS;
}

static void
get_accel_unit(struct adf_hw_device_data *self,
	       struct adf_accel_unit **accel_unit)
{
	memcpy(*accel_unit, adf_4xxx_au_a_ae, sizeof(adf_4xxx_au_a_ae));
}

static void
adf_exit_accel_unit_services(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->au_info) {
		kfree(accel_dev->au_info->au);
		accel_dev->au_info->au = NULL;
		kfree(accel_dev->au_info);
		accel_dev->au_info = NULL;
	}
}

static int
get_accel_unit_config(struct adf_accel_dev *accel_dev,
		      u8 *num_sym_au,
		      u8 *num_dc_au,
		      u8 *num_asym_au)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	/* One AU will be allocated by default if a service enabled */
	u32 alloc_au = 1;
	/* There's always one AU that is used for Admin AE */
	u32 service_mask = ADF_ACCEL_ADMIN;
	char *token, *cur_str;
	u32 disabled_caps = 0;

	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;
	cur_str = val;
	token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	while (token) {
		if (!strncmp(token, ADF_CFG_SYM, strlen(ADF_CFG_SYM)))
			service_mask |= ADF_ACCEL_CRYPTO;
		if (!strncmp(token, ADF_CFG_ASYM, strlen(ADF_CFG_ASYM)))
			service_mask |= ADF_ACCEL_ASYM;

		/* cy means both asym & crypto should be enabled
		 * Hardware resources allocation check will be done later
		 */
		if (!strncmp(token, ADF_CFG_CY, strlen(ADF_CFG_CY)))
			service_mask |= ADF_ACCEL_ASYM | ADF_ACCEL_CRYPTO;
		if (!strncmp(token, ADF_SERVICE_DC, strlen(ADF_SERVICE_DC)))
			service_mask |= ADF_ACCEL_COMPRESSION;

		token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	}

	/* Ensure the user won't enable more services than it can support */
	if (hweight32(service_mask) > num_au) {
		device_printf(GET_DEV(accel_dev),
			      "Can't enable more services than ");
		device_printf(GET_DEV(accel_dev), "%d!\n", num_au);
		return EFAULT;
	} else if (hweight32(service_mask) == 2) {
		/* Due to limitation, besides AU for Admin AE
		 * only 2 more AUs can be allocated
		 */
		alloc_au = 2;
	}

	if (service_mask & ADF_ACCEL_CRYPTO)
		*num_sym_au = alloc_au;
	if (service_mask & ADF_ACCEL_ASYM)
		*num_asym_au = alloc_au;
	if (service_mask & ADF_ACCEL_COMPRESSION)
		*num_dc_au = alloc_au;

	/*update capability*/
	if (!*num_sym_au || !(service_mask & ADF_ACCEL_CRYPTO)) {
		disabled_caps = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_CIPHER |
		    ICP_ACCEL_CAPABILITIES_SHA3 |
		    ICP_ACCEL_CAPABILITIES_SHA3_EXT |
		    ICP_ACCEL_CAPABILITIES_HKDF | ICP_ACCEL_CAPABILITIES_SM3 |
		    ICP_ACCEL_CAPABILITIES_SM4 |
		    ICP_ACCEL_CAPABILITIES_CHACHA_POLY |
		    ICP_ACCEL_CAPABILITIES_AESGCM_SPC |
		    ICP_ACCEL_CAPABILITIES_AES_V2 |
		    ICP_ACCEL_CAPABILITIES_AUTHENTICATION;
	}
	if (!*num_asym_au || !(service_mask & ADF_ACCEL_ASYM)) {
		disabled_caps |= ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_ECEDMONT;
	}
	if (!*num_dc_au || !(service_mask & ADF_ACCEL_COMPRESSION)) {
		disabled_caps |= ICP_ACCEL_CAPABILITIES_COMPRESSION |
		    ICP_ACCEL_CAPABILITIES_LZ4_COMPRESSION |
		    ICP_ACCEL_CAPABILITIES_LZ4S_COMPRESSION |
		    ICP_ACCEL_CAPABILITIES_CNV_INTEGRITY64;
		accel_dev->hw_device->extended_dc_capabilities = 0;
	}
	accel_dev->hw_device->accel_capabilities_mask =
	    adf_4xxx_get_hw_cap(accel_dev) & ~disabled_caps;

	hw_data->service_mask = service_mask;
	hw_data->service_to_load_mask = service_mask;

	return 0;
}

static int
adf_init_accel_unit_services(struct adf_accel_dev *accel_dev)
{
	u8 num_sym_au = 0, num_dc_au = 0, num_asym_au = 0;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 num_au = hw_data->get_num_accel_units(hw_data);
	u32 au_size = num_au * sizeof(struct adf_accel_unit);
	u8 i;

	if (get_accel_unit_config(
		accel_dev, &num_sym_au, &num_dc_au, &num_asym_au))
		return EFAULT;

	accel_dev->au_info = kzalloc(sizeof(*accel_dev->au_info), GFP_KERNEL);
	if (!accel_dev->au_info)
		return ENOMEM;

	accel_dev->au_info->au = kzalloc(au_size, GFP_KERNEL);
	if (!accel_dev->au_info->au) {
		kfree(accel_dev->au_info);
		accel_dev->au_info = NULL;
		return ENOMEM;
	}

	accel_dev->au_info->num_cy_au = num_sym_au;
	accel_dev->au_info->num_dc_au = num_dc_au;
	accel_dev->au_info->num_asym_au = num_asym_au;

	get_accel_unit(hw_data, &accel_dev->au_info->au);

	/* Enable ASYM accel units */
	for (i = 0; i < num_au && num_asym_au > 0; i++) {
		if (accel_dev->au_info->au[i].services ==
		    ADF_ACCEL_SERVICE_NULL) {
			accel_dev->au_info->au[i].services = ADF_ACCEL_ASYM;
			num_asym_au--;
		}
	}
	/* Enable SYM accel units */
	for (i = 0; i < num_au && num_sym_au > 0; i++) {
		if (accel_dev->au_info->au[i].services ==
		    ADF_ACCEL_SERVICE_NULL) {
			accel_dev->au_info->au[i].services = ADF_ACCEL_CRYPTO;
			num_sym_au--;
		}
	}
	/* Enable compression accel units */
	for (i = 0; i < num_au && num_dc_au > 0; i++) {
		if (accel_dev->au_info->au[i].services ==
		    ADF_ACCEL_SERVICE_NULL) {
			accel_dev->au_info->au[i].services =
			    ADF_ACCEL_COMPRESSION;
			num_dc_au--;
		}
	}
	accel_dev->au_info->dc_ae_msk |=
	    hw_data->get_obj_cfg_ae_mask(accel_dev, ADF_ACCEL_COMPRESSION);

	return 0;
}

static int
adf_init_accel_units(struct adf_accel_dev *accel_dev)
{
	return adf_init_accel_unit_services(accel_dev);
}

static void
adf_exit_accel_units(struct adf_accel_dev *accel_dev)
{
	/* reset the AU service */
	adf_exit_accel_unit_services(accel_dev);
}

static const char *
get_obj_name(struct adf_accel_dev *accel_dev,
	     enum adf_accel_unit_services service)
{
	switch (service) {
	case ADF_ACCEL_ASYM:
		return ADF_4XXX_ASYM_OBJ;
	case ADF_ACCEL_CRYPTO:
		return ADF_4XXX_SYM_OBJ;
	case ADF_ACCEL_COMPRESSION:
		return ADF_4XXX_DC_OBJ;
	case ADF_ACCEL_ADMIN:
		return ADF_4XXX_ADMIN_OBJ;
	default:
		return NULL;
	}
}

static uint32_t
get_objs_num(struct adf_accel_dev *accel_dev)
{
	return ADF_4XXX_MAX_OBJ;
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

static enum adf_accel_unit_services
adf_4xxx_get_service_type(struct adf_accel_dev *accel_dev, s32 obj_num)
{
	struct adf_accel_unit *accel_unit;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_au = hw_data->get_num_accel_units(hw_data);
	int i;

	if (!hw_data->service_to_load_mask)
		return ADF_ACCEL_SERVICE_NULL;

	if (accel_dev->au_info && accel_dev->au_info->au)
		accel_unit = accel_dev->au_info->au;
	else
		return ADF_ACCEL_SERVICE_NULL;

	for (i = num_au - 2; i >= 0; i--) {
		if (hw_data->service_to_load_mask & accel_unit[i].services) {
			hw_data->service_to_load_mask &=
			    ~accel_unit[i].services;
			return accel_unit[i].services;
		}
	}

	/* admin AE should be loaded last */
	if (hw_data->service_to_load_mask & accel_unit[num_au - 1].services) {
		hw_data->service_to_load_mask &=
		    ~accel_unit[num_au - 1].services;
		return accel_unit[num_au - 1].services;
	}

	return ADF_ACCEL_SERVICE_NULL;
}

static void
get_ring_svc_map_data(int ring_pair_index,
		      u16 ring_to_svc_map,
		      u8 *serv_type,
		      int *ring_index,
		      int *num_rings_per_srv,
		      int bundle_num)
{
	*serv_type =
	    GET_SRV_TYPE(ring_to_svc_map, bundle_num % ADF_CFG_NUM_SERVICES);
	*ring_index = 0;
	*num_rings_per_srv = ADF_4XXX_NUM_RINGS_PER_BANK / 2;
}

static int
adf_get_dc_extcapabilities(struct adf_accel_dev *accel_dev, u32 *capabilities)
{
	struct icp_qat_fw_init_admin_req req;
	struct icp_qat_fw_init_admin_resp resp;
	u8 i;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_au = hw_data->get_num_accel_units(hw_data);
	u32 first_dc_ae = 0;

	for (i = 0; i < num_au; i++) {
		if (accel_dev->au_info->au[i].services &
		    ADF_ACCEL_COMPRESSION) {
			first_dc_ae = accel_dev->au_info->au[i].ae_mask;
			first_dc_ae &= ~(first_dc_ae - 1);
		}
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	req.cmd_id = ICP_QAT_FW_COMP_CAPABILITY_GET;

	if (likely(first_dc_ae)) {
		if (adf_send_admin(accel_dev, &req, &resp, first_dc_ae) ||
		    resp.status) {
			*capabilities = 0;
			return EFAULT;
		}

		*capabilities = resp.extended_features;
	}

	return 0;
}

static int
adf_get_fw_status(struct adf_accel_dev *accel_dev,
		  u8 *major,
		  u8 *minor,
		  u8 *patch)
{
	struct icp_qat_fw_init_admin_req req;
	struct icp_qat_fw_init_admin_resp resp;
	u32 ae_mask = 1;

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	req.cmd_id = ICP_QAT_FW_STATUS_GET;

	if (adf_send_admin(accel_dev, &req, &resp, ae_mask))
		return EFAULT;

	*major = resp.version_major_num;
	*minor = resp.version_minor_num;
	*patch = resp.version_patch_num;

	return 0;
}

static int
adf_4xxx_send_admin_init(struct adf_accel_dev *accel_dev)
{
	int ret = 0;
	struct icp_qat_fw_init_admin_req req;
	struct icp_qat_fw_init_admin_resp resp;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 ae_mask = hw_data->ae_mask;
	u32 admin_ae_mask = hw_data->admin_ae_mask;
	u8 num_au = hw_data->get_num_accel_units(hw_data);
	u8 i;
	u32 dc_capabilities = 0;

	for (i = 0; i < num_au; i++) {
		if (accel_dev->au_info->au[i].services ==
		    ADF_ACCEL_SERVICE_NULL)
			ae_mask &= ~accel_dev->au_info->au[i].ae_mask;

		if (accel_dev->au_info->au[i].services != ADF_ACCEL_ADMIN)
			admin_ae_mask &= ~accel_dev->au_info->au[i].ae_mask;
	}

	if (!accel_dev->admin) {
		device_printf(GET_DEV(accel_dev), "adf_admin not available\n");
		return EFAULT;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	req.cmd_id = ICP_QAT_FW_CONSTANTS_CFG;
	req.init_cfg_sz = ADF_CONST_TABLE_SIZE;
	req.init_cfg_ptr = accel_dev->admin->const_tbl_addr;
	if (adf_send_admin(accel_dev, &req, &resp, admin_ae_mask)) {
		device_printf(GET_DEV(accel_dev),
			      "Error sending constants config message\n");
		return EFAULT;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	req.cmd_id = ICP_QAT_FW_INIT_ME;
	if (adf_send_admin(accel_dev, &req, &resp, ae_mask)) {
		device_printf(GET_DEV(accel_dev),
			      "Error sending init message\n");
		return EFAULT;
	}

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));
	req.cmd_id = ICP_QAT_FW_HEARTBEAT_TIMER_SET;
	req.init_cfg_ptr = accel_dev->admin->phy_hb_addr;
	if (adf_get_hb_timer(accel_dev, &req.heartbeat_ticks))
		return EINVAL;

	if (adf_send_admin(accel_dev, &req, &resp, ae_mask))
		device_printf(GET_DEV(accel_dev),
			      "Heartbeat is not supported\n");

	ret = adf_get_dc_extcapabilities(accel_dev, &dc_capabilities);
	if (unlikely(ret)) {
		device_printf(GET_DEV(accel_dev),
			      "Could not get FW ext. capabilities\n");
	}

	accel_dev->hw_device->extended_dc_capabilities = dc_capabilities;

	adf_get_fw_status(accel_dev,
			  &accel_dev->fw_versions.fw_version_major,
			  &accel_dev->fw_versions.fw_version_minor,
			  &accel_dev->fw_versions.fw_version_patch);

	device_printf(GET_DEV(accel_dev),
		      "FW version: %d.%d.%d\n",
		      accel_dev->fw_versions.fw_version_major,
		      accel_dev->fw_versions.fw_version_minor,
		      accel_dev->fw_versions.fw_version_patch);

	return ret;
}

static enum dev_sku_info
get_sku(struct adf_hw_device_data *self)
{
	return DEV_SKU_1;
}

static struct adf_accel_unit *
get_au_by_ae(struct adf_accel_dev *accel_dev, int ae_num)
{
	int i = 0;
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;

	if (!accel_unit)
		return NULL;

	for (i = 0; i < ADF_4XXX_MAX_ACCELUNITS; i++)
		if (accel_unit[i].ae_mask & BIT(ae_num))
			return &accel_unit[i];

	return NULL;
}

static bool
check_accel_unit_service(enum adf_accel_unit_services au_srv,
			 enum adf_cfg_service_type ring_srv)
{
	if ((ADF_ACCEL_SERVICE_NULL == au_srv) && ring_srv == NA)
		return true;
	if ((au_srv & ADF_ACCEL_COMPRESSION) && ring_srv == COMP)
		return true;
	if ((au_srv & ADF_ACCEL_ASYM) && ring_srv == ASYM)
		return true;
	if ((au_srv & ADF_ACCEL_CRYPTO) && ring_srv == SYM)
		return true;

	return false;
}

static void
adf_4xxx_cfg_gen_dispatch_arbiter(struct adf_accel_dev *accel_dev,
				  u32 *thrd_to_arb_map_gen)
{
	struct adf_accel_unit *au = NULL;
	int engine = 0;
	int thread = 0;
	int service;
	u16 ena_srv_mask;
	u16 service_type;
	u32 service_mask;
	unsigned long thd_srv_mask = default_active_thd_mask;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	ena_srv_mask = accel_dev->hw_device->ring_to_svc_map;
	/* If ring_to_svc_map is not changed, return default arbiter value */
	if (ena_srv_mask == ADF_4XXX_DEFAULT_RING_TO_SRV_MAP) {
		memcpy(thrd_to_arb_map_gen,
		       thrd_to_arb_map,
		       sizeof(thrd_to_arb_map_gen[0]) *
			   ADF_4XXX_MAX_ACCELENGINES);
		return;
	}

	for (engine = 0; engine < ADF_4XXX_MAX_ACCELENGINES - 1; engine++) {
		thrd_to_arb_map_gen[engine] = 0;
		service_mask = 0;
		au = get_au_by_ae(accel_dev, engine);
		if (!au)
			continue;

		for (service = 0; service < ADF_CFG_MAX_SERVICES; service++) {
			service_type = GET_SRV_TYPE(ena_srv_mask, service);
			if (check_accel_unit_service(au->services,
						     service_type))
				service_mask |= BIT(service);
		}

		if (au->services == ADF_ACCEL_COMPRESSION)
			thd_srv_mask = dc_me_active_thd_mask;
		else if (au->services == ADF_ACCEL_ASYM)
			thd_srv_mask = hw_data->asym_ae_active_thd_mask;
		else
			thd_srv_mask = default_active_thd_mask;

		for_each_set_bit(thread, &thd_srv_mask, 8)
		{
			thrd_to_arb_map_gen[engine] |=
			    (service_mask << (ADF_CFG_MAX_SERVICES * thread));
		}
	}
}

static void
adf_get_arbiter_mapping(struct adf_accel_dev *accel_dev,
			u32 const **arb_map_config)
{
	int i;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	for (i = 1; i < ADF_4XXX_MAX_ACCELENGINES; i++) {
		if (~hw_device->ae_mask & (1 << i))
			thrd_to_arb_map[i] = 0;
	}
	adf_4xxx_cfg_gen_dispatch_arbiter(accel_dev, thrd_to_arb_map_gen);
	*arb_map_config = thrd_to_arb_map_gen;
}

static void
get_arb_info(struct arb_info *arb_info)
{
	arb_info->wrk_cfg_offset = ADF_4XXX_ARB_CONFIG;
	arb_info->arbiter_offset = ADF_4XXX_ARB_OFFSET;
	arb_info->wrk_thd_2_srv_arb_map = ADF_4XXX_ARB_WRK_2_SER_MAP_OFFSET;
}

static void
get_admin_info(struct admin_info *admin_csrs_info)
{
	admin_csrs_info->mailbox_offset = ADF_4XXX_MAILBOX_BASE_OFFSET;
	admin_csrs_info->admin_msg_ur = ADF_4XXX_ADMINMSGUR_OFFSET;
	admin_csrs_info->admin_msg_lr = ADF_4XXX_ADMINMSGLR_OFFSET;
}

static void
adf_enable_error_correction(struct adf_accel_dev *accel_dev)
{
	struct adf_bar *misc_bar = &GET_BARS(accel_dev)[ADF_4XXX_PMISC_BAR];
	struct resource *csr = misc_bar->virt_addr;

	/* Enable all in errsou3 except VFLR notification on host */
	ADF_CSR_WR(csr, ADF_4XXX_ERRMSK3, ADF_4XXX_VFLNOTIFY);
}

static void
adf_enable_ints(struct adf_accel_dev *accel_dev)
{
	struct resource *addr;

	addr = (&GET_BARS(accel_dev)[ADF_4XXX_PMISC_BAR])->virt_addr;

	/* Enable bundle interrupts */
	ADF_CSR_WR(addr, ADF_4XXX_SMIAPF_RP_X0_MASK_OFFSET, 0);
	ADF_CSR_WR(addr, ADF_4XXX_SMIAPF_RP_X1_MASK_OFFSET, 0);

	/* Enable misc interrupts */
	ADF_CSR_WR(addr, ADF_4XXX_SMIAPF_MASK_OFFSET, 0);
}

static int
adf_init_device(struct adf_accel_dev *accel_dev)
{
	struct resource *addr;
	u32 status;
	u32 csr;
	int ret;

	addr = (&GET_BARS(accel_dev)[ADF_4XXX_PMISC_BAR])->virt_addr;

	/* Temporarily mask PM interrupt */
	csr = ADF_CSR_RD(addr, ADF_4XXX_ERRMSK2);
	csr |= ADF_4XXX_PM_SOU;
	ADF_CSR_WR(addr, ADF_4XXX_ERRMSK2, csr);

	/* Set DRV_ACTIVE bit to power up the device */
	ADF_CSR_WR(addr, ADF_4XXX_PM_INTERRUPT, ADF_4XXX_PM_DRV_ACTIVE);

	/* Poll status register to make sure the device is powered up */
	status = 0;
	ret = read_poll_timeout(ADF_CSR_RD,
				status,
				status & ADF_4XXX_PM_INIT_STATE,
				ADF_4XXX_PM_POLL_DELAY_US,
				ADF_4XXX_PM_POLL_TIMEOUT_US,
				true,
				addr,
				ADF_4XXX_PM_STATUS);
	if (ret)
		device_printf(GET_DEV(accel_dev),
			      "Failed to power up the device\n");

	return ret;
}

void
adf_init_hw_data_4xxx(struct adf_hw_device_data *hw_data, u32 id)
{
	hw_data->dev_class = &adf_4xxx_class;
	hw_data->instance_id = adf_4xxx_class.instances++;
	hw_data->num_banks = ADF_4XXX_ETR_MAX_BANKS;
	hw_data->num_rings_per_bank = ADF_4XXX_NUM_RINGS_PER_BANK;
	hw_data->num_accel = ADF_4XXX_MAX_ACCELERATORS;
	hw_data->num_engines = ADF_4XXX_MAX_ACCELENGINES;
	hw_data->num_logical_accel = 1;
	hw_data->tx_rx_gap = ADF_4XXX_RX_RINGS_OFFSET;
	hw_data->tx_rings_mask = ADF_4XXX_TX_RINGS_MASK;
	hw_data->alloc_irq = adf_isr_resource_alloc;
	hw_data->free_irq = adf_isr_resource_free;
	hw_data->enable_error_correction = adf_enable_error_correction;
	hw_data->get_accel_mask = get_accel_mask;
	hw_data->get_ae_mask = get_ae_mask;
	hw_data->get_num_accels = get_num_accels;
	hw_data->get_num_aes = get_num_aes;
	hw_data->get_sram_bar_id = get_sram_bar_id;
	hw_data->get_etr_bar_id = get_etr_bar_id;
	hw_data->get_misc_bar_id = get_misc_bar_id;
	hw_data->get_arb_info = get_arb_info;
	hw_data->get_admin_info = get_admin_info;
	hw_data->get_accel_cap = adf_4xxx_get_hw_cap;
	hw_data->clock_frequency = ADF_4XXX_AE_FREQ;
	hw_data->get_sku = get_sku;
	hw_data->heartbeat_ctr_num = ADF_NUM_HB_CNT_PER_AE;
	hw_data->fw_name = ADF_4XXX_FW;
	hw_data->fw_mmp_name = ADF_4XXX_MMP;
	hw_data->init_admin_comms = adf_init_admin_comms;
	hw_data->exit_admin_comms = adf_exit_admin_comms;
	hw_data->send_admin_init = adf_4xxx_send_admin_init;
	hw_data->init_arb = adf_init_gen2_arb;
	hw_data->exit_arb = adf_exit_arb;
	hw_data->get_arb_mapping = adf_get_arbiter_mapping;
	hw_data->enable_ints = adf_enable_ints;
	hw_data->init_device = adf_init_device;
	hw_data->reset_device = adf_reset_flr;
	hw_data->restore_device = adf_dev_restore;
	hw_data->init_accel_units = adf_init_accel_units;
	hw_data->exit_accel_units = adf_exit_accel_units;
	hw_data->get_num_accel_units = get_num_accel_units;
	hw_data->configure_accel_units = adf_4xxx_configure_accel_units;
	hw_data->get_ring_to_svc_map = get_ring_to_svc_map;
	hw_data->get_ring_svc_map_data = get_ring_svc_map_data;
	hw_data->admin_ae_mask = ADF_4XXX_ADMIN_AE_MASK;
	hw_data->get_objs_num = get_objs_num;
	hw_data->get_obj_name = get_obj_name;
	hw_data->get_obj_cfg_ae_mask = get_obj_cfg_ae_mask;
	hw_data->get_service_type = adf_4xxx_get_service_type;
	hw_data->set_msix_rttable = set_msix_default_rttable;
	hw_data->set_ssm_wdtimer = adf_gen4_set_ssm_wdtimer;
	hw_data->disable_iov = adf_disable_sriov;
	hw_data->config_device = adf_config_device;
	hw_data->set_asym_rings_mask = adf_set_asym_rings_mask;
	hw_data->get_hb_clock = get_hb_clock;
	hw_data->int_timer_init = adf_int_timer_init;
	hw_data->int_timer_exit = adf_int_timer_exit;
	hw_data->get_heartbeat_status = adf_get_heartbeat_status;
	hw_data->get_ae_clock = get_ae_clock;
	hw_data->measure_clock = measure_clock;
	hw_data->query_storage_cap = 1;
	hw_data->ring_pair_reset = adf_gen4_ring_pair_reset;

	switch (id) {
	case ADF_401XX_PCI_DEVICE_ID:
		hw_data->asym_ae_active_thd_mask = DEFAULT_401XX_ASYM_AE_MASK;
		break;
	case ADF_4XXX_PCI_DEVICE_ID:
	default:
		hw_data->asym_ae_active_thd_mask = DEFAULT_4XXX_ASYM_AE_MASK;
	}

	adf_gen4_init_hw_csr_info(&hw_data->csr_info);
	adf_gen4_init_pf_pfvf_ops(&hw_data->csr_info.pfvf_ops);
}

void
adf_clean_hw_data_4xxx(struct adf_hw_device_data *hw_data)
{
	hw_data->dev_class->instances--;
}
