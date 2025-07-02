/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "adf_cfg_instance.h"
#include "adf_cfg_section.h"
#include "adf_cfg_device.h"
#include "icp_qat_hw.h"
#include "adf_common_drv.h"

#define ADF_CFG_SVCS_MAX (12)
#define ADF_CFG_DEPRE_PARAMS_NUM (4)

#define ADF_CFG_CAP_DC ADF_ACCEL_CAPABILITIES_COMPRESSION
#define ADF_CFG_CAP_ASYM ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC
#define ADF_CFG_CAP_SYM                                                        \
	(ADF_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |                             \
	 ADF_ACCEL_CAPABILITIES_CIPHER |                                       \
	 ADF_ACCEL_CAPABILITIES_AUTHENTICATION)
#define ADF_CFG_CAP_CY (ADF_CFG_CAP_ASYM | ADF_CFG_CAP_SYM)

#define ADF_CFG_FW_CAP_RL ICP_ACCEL_CAPABILITIES_RL
#define ADF_CFG_FW_CAP_HKDF ICP_ACCEL_CAPABILITIES_HKDF
#define ADF_CFG_FW_CAP_ECEDMONT ICP_ACCEL_CAPABILITIES_ECEDMONT
#define ADF_CFG_FW_CAP_EXT_ALGCHAIN ICP_ACCEL_CAPABILITIES_EXT_ALGCHAIN

#define ADF_CFG_CY_RINGS                                                       \
	(CRYPTO | CRYPTO << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                   \
	 CRYPTO << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                            \
	 CRYPTO << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_SYM_RINGS                                                      \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 SYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                               \
	 SYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_ASYM_RINGS                                                     \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 ASYM << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 ASYM << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_CY_DC_RINGS                                                    \
	(CRYPTO | CRYPTO << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                   \
	 NA << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                                \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_ASYM_DC_RINGS                                                  \
	(ASYM | ASYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_SYM_DC_RINGS                                                   \
	(SYM | SYM << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                         \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

#define ADF_CFG_DC_RINGS                                                       \
	(COMP | COMP << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                       \
	 COMP << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                              \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

static char adf_cfg_deprecated_params[][ADF_CFG_MAX_KEY_LEN_IN_BYTES] =
    { ADF_DEV_KPT_ENABLE,
      ADF_STORAGE_FIRMWARE_ENABLED,
      ADF_RL_FIRMWARE_ENABLED,
      ADF_PKE_DISABLED };

struct adf_cfg_enabled_services {
	const char svcs_enabled[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u16 rng_to_svc_msk;
	u32 enabled_svc_cap;
	u32 enabled_fw_cap;
};

struct adf_cfg_profile {
	enum adf_cfg_fw_image_type fw_image_type;
	struct adf_cfg_enabled_services supported_svcs[ADF_CFG_SVCS_MAX];
};

static struct adf_cfg_profile adf_profiles[] =
    { { ADF_FW_IMAGE_DEFAULT,
	{
	    { "cy",
	      ADF_CFG_CY_RINGS,
	      ADF_CFG_CAP_CY,
	      ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym;sym",
	      ADF_CFG_CY_RINGS,
	      ADF_CFG_CAP_CY,
	      ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "sym;asym",
	      ADF_CFG_CY_RINGS,
	      ADF_CFG_CAP_CY,
	      ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc", ADF_CFG_DC_RINGS, ADF_CFG_CAP_DC, 0 },
	    { "sym",
	      ADF_CFG_SYM_RINGS,
	      ADF_CFG_CAP_SYM,
	      ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym",
	      ADF_CFG_ASYM_RINGS,
	      ADF_CFG_CAP_ASYM,
	      ADF_CFG_FW_CAP_ECEDMONT },
	    { "cy;dc",
	      ADF_CFG_CY_DC_RINGS,
	      ADF_CFG_CAP_CY | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc;cy",
	      ADF_CFG_CY_DC_RINGS,
	      ADF_CFG_CAP_CY | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym;dc",
	      ADF_CFG_ASYM_DC_RINGS,
	      ADF_CFG_CAP_ASYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_ECEDMONT },
	    { "dc;asym",
	      ADF_CFG_ASYM_DC_RINGS,
	      ADF_CFG_CAP_ASYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_ECEDMONT },
	    { "sym;dc",
	      ADF_CFG_SYM_DC_RINGS,
	      ADF_CFG_CAP_SYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc;sym",
	      ADF_CFG_SYM_DC_RINGS,
	      ADF_CFG_CAP_SYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	} },
      { ADF_FW_IMAGE_CRYPTO,
	{
	    { "cy",
	      ADF_CFG_CY_RINGS,
	      ADF_CFG_CAP_CY,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "sym",
	      ADF_CFG_SYM_RINGS,
	      ADF_CFG_CAP_SYM,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym",
	      ADF_CFG_ASYM_RINGS,
	      ADF_CFG_CAP_ASYM,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_ECEDMONT },
	} },
      { ADF_FW_IMAGE_COMPRESSION,
	{
	    { "dc", ADF_CFG_DC_RINGS, ADF_CFG_CAP_DC, 0 },
	} },
      { ADF_FW_IMAGE_CUSTOM1,
	{
	    { "cy",
	      ADF_CFG_CY_RINGS,
	      ADF_CFG_CAP_CY,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc", ADF_CFG_DC_RINGS, ADF_CFG_CAP_DC, 0 },
	    { "sym",
	      ADF_CFG_SYM_RINGS,
	      ADF_CFG_CAP_SYM,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym",
	      ADF_CFG_ASYM_RINGS,
	      ADF_CFG_CAP_ASYM,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_ECEDMONT },
	    { "cy;dc",
	      ADF_CFG_CY_DC_RINGS,
	      ADF_CFG_CAP_CY | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc;cy",
	      ADF_CFG_CY_DC_RINGS,
	      ADF_CFG_CAP_CY | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_ECEDMONT | ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "asym;dc",
	      ADF_CFG_ASYM_DC_RINGS,
	      ADF_CFG_CAP_ASYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_ECEDMONT },
	    { "dc;asym",
	      ADF_CFG_ASYM_DC_RINGS,
	      ADF_CFG_CAP_ASYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_ECEDMONT },
	    { "sym;dc",
	      ADF_CFG_SYM_DC_RINGS,
	      ADF_CFG_CAP_SYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	    { "dc;sym",
	      ADF_CFG_SYM_DC_RINGS,
	      ADF_CFG_CAP_SYM | ADF_CFG_CAP_DC,
	      ADF_CFG_FW_CAP_RL | ADF_CFG_FW_CAP_HKDF |
		  ADF_CFG_FW_CAP_EXT_ALGCHAIN },
	} } };

int
adf_cfg_get_ring_pairs(struct adf_cfg_device *device,
		       struct adf_cfg_instance *inst,
		       const char *process_name,
		       struct adf_accel_dev *accel_dev)
{
	int i = 0;
	int ret = EFAULT;
	struct adf_cfg_instance *free_inst = NULL;
	enum adf_cfg_bundle_type free_bundle_type;
	int first_user_bundle = 0;

	/* Section of user process with poll mode */
	if (strcmp(ADF_KERNEL_SEC, process_name) &&
	    strcmp(ADF_KERNEL_SAL_SEC, process_name) &&
	    inst->polling_mode == ADF_CFG_RESP_POLL) {
		first_user_bundle = device->max_kernel_bundle_nr + 1;
		for (i = first_user_bundle; i < device->bundle_num; i++) {
			free_inst = adf_cfg_get_free_instance(
			    device, device->bundles[i], inst, process_name);

			if (!free_inst)
				continue;

			ret = adf_cfg_get_ring_pairs_from_bundle(
			    device->bundles[i], inst, process_name, free_inst);
			return ret;
		}
	} else {
		/* Section of in-tree, or kernel API or user process
		 * with epoll mode
		 */
		if (!strcmp(ADF_KERNEL_SEC, process_name) ||
		    !strcmp(ADF_KERNEL_SAL_SEC, process_name))
			free_bundle_type = KERNEL;
		else
			free_bundle_type = USER;

		for (i = 0; i < device->bundle_num; i++) {
			/* Since both in-tree and kernel API's bundle type
			 * are kernel, use cpumask_subset to check if the
			 * ring's affinity mask is a subset of a bundle's
			 * one.
			 */
			if (free_bundle_type == device->bundles[i]->type &&
			    CPU_SUBSET(&device->bundles[i]->affinity_mask,
				       &inst->affinity_mask)) {
				free_inst = adf_cfg_get_free_instance(
				    device,
				    device->bundles[i],
				    inst,
				    process_name);

				if (!free_inst)
					continue;
				ret = adf_cfg_get_ring_pairs_from_bundle(
				    device->bundles[i],
				    inst,
				    process_name,
				    free_inst);

				return ret;

			}
		}
		for (i = 0; i < device->bundle_num; i++) {
			if (adf_cfg_is_free(device->bundles[i])) {
				free_inst = adf_cfg_get_free_instance(
				    device,
				    device->bundles[i],
				    inst,
				    process_name);
				if (!free_inst)
					continue;

				ret = adf_cfg_get_ring_pairs_from_bundle(
				    device->bundles[i],
				    inst,
				    process_name,
				    free_inst);
				return ret;
			}
		}
	}
	pr_err("Don't have enough rings for instance %s in process %s\n",
	       inst->name,
	       process_name);

	return ret;
}

int
adf_cfg_get_services_enabled(struct adf_accel_dev *accel_dev,
			     u16 *ring_to_svc_map)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u32 i = 0;
	struct adf_cfg_enabled_services *svcs = NULL;
	enum adf_cfg_fw_image_type fw_image_type = ADF_FW_IMAGE_DEFAULT;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	*ring_to_svc_map = 0;

	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;

	if (hw_data->get_fw_image_type) {
		if (hw_data->get_fw_image_type(accel_dev, &fw_image_type))
			return EFAULT;
	}

	for (i = 0; i < ADF_CFG_SVCS_MAX; i++) {
		svcs = &adf_profiles[fw_image_type].supported_svcs[i];

		if (!strncmp(svcs->svcs_enabled,
			     "",
			     ADF_CFG_MAX_VAL_LEN_IN_BYTES))
			break;

		if (!strncmp(val,
			     svcs->svcs_enabled,
			     ADF_CFG_MAX_VAL_LEN_IN_BYTES)) {
			*ring_to_svc_map = svcs->rng_to_svc_msk;
			return 0;
		}
	}

	device_printf(GET_DEV(accel_dev),
		      "Invalid ServicesEnabled %s for ServicesProfile: %d\n",
		      val,
		      fw_image_type);

	return EFAULT;
}

void
adf_cfg_set_asym_rings_mask(struct adf_accel_dev *accel_dev)
{
	int service;
	u16 ena_srv_mask;
	u16 service_type;
	u16 asym_mask = 0;
	struct adf_cfg_device *cfg_dev = accel_dev->cfg->dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	if (!cfg_dev) {
		hw_data->asym_rings_mask = ADF_CFG_DEF_ASYM_MASK;
		return;
	}

	ena_srv_mask = accel_dev->hw_device->ring_to_svc_map;

	/* parse each service */
	for (service = 0; service < ADF_CFG_MAX_SERVICES; service++) {
		service_type = GET_SRV_TYPE(ena_srv_mask, service);
		switch (service_type) {
		case CRYPTO:
		case ASYM:
			SET_ASYM_MASK(asym_mask, service);
			if (service_type == CRYPTO)
				service++;
			break;
		}
	}

	hw_data->asym_rings_mask = asym_mask;
}

void
adf_cfg_gen_dispatch_arbiter(struct adf_accel_dev *accel_dev,
			     const u32 *thrd_to_arb_map,
			     u32 *thrd_to_arb_map_gen,
			     u32 total_engines)
{
	int engine, thread, service, bits;
	u32 thread_ability, ability_map, service_mask, service_type;
	u16 ena_srv_mask = GET_HW_DATA(accel_dev)->ring_to_svc_map;

	for (engine = 0; engine < total_engines; engine++) {
		if (!(GET_HW_DATA(accel_dev)->ae_mask & (1 << engine)))
			continue;
		bits = 0;
		/* ability_map is used to indicate the threads ability */
		ability_map = thrd_to_arb_map[engine];
		thrd_to_arb_map_gen[engine] = 0;
		/* parse each thread on the engine */
		for (thread = 0; thread < ADF_NUM_THREADS_PER_AE; thread++) {
			/* get the ability of this thread */
			thread_ability = ability_map & ADF_THRD_ABILITY_MASK;
			ability_map >>= ADF_THRD_ABILITY_BIT_LEN;
			/* parse each service */
			for (service = 0; service < ADF_CFG_MAX_SERVICES;
			     service++) {
				service_type =
				    GET_SRV_TYPE(ena_srv_mask, service);
				switch (service_type) {
				case CRYPTO:
					service_mask = ADF_CFG_ASYM_SRV_MASK;
					if (thread_ability & service_mask)
						thrd_to_arb_map_gen[engine] |=
						    (1 << bits);
					bits++;
					service++;
					service_mask = ADF_CFG_SYM_SRV_MASK;
					break;
				case COMP:
					service_mask = ADF_CFG_DC_SRV_MASK;
					break;
				case SYM:
					service_mask = ADF_CFG_SYM_SRV_MASK;
					break;
				case ASYM:
					service_mask = ADF_CFG_ASYM_SRV_MASK;
					break;
				default:
					service_mask = ADF_CFG_UNKNOWN_SRV_MASK;
				}
				if (thread_ability & service_mask)
					thrd_to_arb_map_gen[engine] |=
					    (1 << bits);
				bits++;
			}
		}
	}
}

int
adf_cfg_get_fw_image_type(struct adf_accel_dev *accel_dev,
			  enum adf_cfg_fw_image_type *fw_image_type)
{
	*fw_image_type = ADF_FW_IMAGE_CUSTOM1;

	return 0;
}

static int
adf_cfg_get_caps_enabled(struct adf_accel_dev *accel_dev,
			 u32 *enabled_svc_caps,
			 u32 *enabled_fw_caps)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u8 i = 0;
	struct adf_cfg_enabled_services *svcs = NULL;
	enum adf_cfg_fw_image_type fw_image_type = ADF_FW_IMAGE_DEFAULT;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	*enabled_svc_caps = 0;
	*enabled_fw_caps = 0;

	/* Get the services enabled by user */
	snprintf(key, sizeof(key), ADF_SERVICES_ENABLED);
	if (adf_cfg_get_param_value(accel_dev, ADF_GENERAL_SEC, key, val))
		return EFAULT;

	/*
	 * Only the PF driver has the hook for get_fw_image_type as the VF's
	 * enabled service is from PFVF communication. The fw_image_type for
	 * the VF is set to DEFAULT since this type contains all kinds of
	 * enabled service.
	 */
	if (hw_data->get_fw_image_type) {
		if (hw_data->get_fw_image_type(accel_dev, &fw_image_type))
			return EFAULT;
	}

	for (i = 0; i < ADF_CFG_SVCS_MAX; i++) {
		svcs = &adf_profiles[fw_image_type].supported_svcs[i];

		if (!strncmp(svcs->svcs_enabled,
			     "",
			     ADF_CFG_MAX_VAL_LEN_IN_BYTES))
			break;

		if (!strncmp(val,
			     svcs->svcs_enabled,
			     ADF_CFG_MAX_VAL_LEN_IN_BYTES)) {
			*enabled_svc_caps = svcs->enabled_svc_cap;
			*enabled_fw_caps = svcs->enabled_fw_cap;
			return 0;
		}
	}
	device_printf(GET_DEV(accel_dev),
		      "Invalid ServicesEnabled %s for ServicesProfile: %d\n",
		      val,
		      fw_image_type);

	return EFAULT;
}

static void
adf_cfg_check_deprecated_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u8 i = 0;

	for (i = 0; i < ADF_CFG_DEPRE_PARAMS_NUM; i++) {
		/* give a warning if the deprecated params are set by user */
		snprintf(key, sizeof(key), "%s", adf_cfg_deprecated_params[i]);
		if (!adf_cfg_get_param_value(
			accel_dev, ADF_GENERAL_SEC, key, val)) {
			device_printf(GET_DEV(accel_dev),
				      "Parameter '%s' has been deprecated\n",
				      key);
		}
	}
}

static int
adf_cfg_check_enabled_services(struct adf_accel_dev *accel_dev,
			       u32 enabled_svc_caps)
{
	u32 hw_caps = GET_HW_DATA(accel_dev)->accel_capabilities_mask;

	if ((enabled_svc_caps & hw_caps) == enabled_svc_caps)
		return 0;

	device_printf(GET_DEV(accel_dev), "Unsupported device configuration\n");

	return EFAULT;
}

static int
adf_cfg_update_pf_accel_cap_mask(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 enabled_svc_caps = 0;
	u32 enabled_fw_caps = 0;

	if (hw_data->get_accel_cap) {
		hw_data->accel_capabilities_mask =
		    hw_data->get_accel_cap(accel_dev);
	}

	if (adf_cfg_get_caps_enabled(accel_dev,
				     &enabled_svc_caps,
				     &enabled_fw_caps))
		return EFAULT;

	if (adf_cfg_check_enabled_services(accel_dev, enabled_svc_caps))
		return EFAULT;

	if (!(enabled_svc_caps & ADF_CFG_CAP_ASYM))
		hw_data->accel_capabilities_mask &= ~ADF_CFG_CAP_ASYM;
	if (!(enabled_svc_caps & ADF_CFG_CAP_SYM))
		hw_data->accel_capabilities_mask &= ~ADF_CFG_CAP_SYM;
	if (!(enabled_svc_caps & ADF_CFG_CAP_DC))
		hw_data->accel_capabilities_mask &= ~ADF_CFG_CAP_DC;

	/* Enable FW defined capabilities*/
	if (enabled_fw_caps)
		hw_data->accel_capabilities_mask |= enabled_fw_caps;

	return 0;
}

static int
adf_cfg_update_vf_accel_cap_mask(struct adf_accel_dev *accel_dev)
{
	u32 enabled_svc_caps = 0;
	u32 enabled_fw_caps = 0;
	if (adf_cfg_get_caps_enabled(accel_dev,
				     &enabled_svc_caps,
				     &enabled_fw_caps))
		return EFAULT;

	if (adf_cfg_check_enabled_services(accel_dev, enabled_svc_caps))
		return EFAULT;

	return 0;
}

int
adf_cfg_device_init(struct adf_cfg_device *device,
		    struct adf_accel_dev *accel_dev)
{
	int i = 0;
	/* max_inst indicates the max instance number one bank can hold */
	int max_inst = accel_dev->hw_device->tx_rx_gap;
	int ret = ENOMEM;
	struct adf_hw_device_data *hw_data = GET_HW_DATA(accel_dev);

	adf_cfg_check_deprecated_params(accel_dev);

	device->bundle_num = 0;
	device->bundles = (struct adf_cfg_bundle **)malloc(
	    sizeof(struct adf_cfg_bundle *) * accel_dev->hw_device->num_banks,
	    M_QAT,
	    M_WAITOK | M_ZERO);

	device->bundle_num = accel_dev->hw_device->num_banks;

	device->instances = (struct adf_cfg_instance **)malloc(
	    sizeof(struct adf_cfg_instance *) * device->bundle_num * max_inst,
	    M_QAT,
	    M_WAITOK | M_ZERO);

	device->instance_index = 0;

	device->max_kernel_bundle_nr = -1;

	ret = EFAULT;

	/* Update the acceleration capability mask based on User capability */
	if (!accel_dev->is_vf) {
		if (adf_cfg_update_pf_accel_cap_mask(accel_dev))
			goto failed;
	} else {
		if (adf_cfg_update_vf_accel_cap_mask(accel_dev))
			goto failed;
	}

	/* Based on the svc configured, get ring_to_svc_map */
	if (hw_data->get_ring_to_svc_map) {
		if (hw_data->get_ring_to_svc_map(accel_dev,
						 &hw_data->ring_to_svc_map))
			goto failed;
	}

	ret = ENOMEM;
	/*
	 * 1) get the config information to generate the ring to service
	 *    mapping table
	 * 2) init each bundle of this device
	 */
	for (i = 0; i < device->bundle_num; i++) {
		device->bundles[i] = malloc(sizeof(struct adf_cfg_bundle),
					    M_QAT,
					    M_WAITOK | M_ZERO);

		device->bundles[i]->max_section = max_inst;
		adf_cfg_bundle_init(device->bundles[i], device, i, accel_dev);
	}

	return 0;

failed:
	for (i = 0; i < device->bundle_num; i++) {
		if (device->bundles[i])
			adf_cfg_bundle_clear(device->bundles[i], accel_dev);
	}

	for (i = 0; i < (device->bundle_num * max_inst); i++) {
		if (device->instances && device->instances[i])
			free(device->instances[i], M_QAT);
	}

	free(device->instances, M_QAT);
	device->instances = NULL;

	device_printf(GET_DEV(accel_dev), "Failed to do device init\n");
	return ret;
}

void
adf_cfg_device_clear(struct adf_cfg_device *device,
		     struct adf_accel_dev *accel_dev)
{
	int i = 0;

	for (i = 0; i < device->bundle_num; i++) {
		if (device->bundles && device->bundles[i]) {
			adf_cfg_bundle_clear(device->bundles[i], accel_dev);
			free(device->bundles[i], M_QAT);
			device->bundles[i] = NULL;
		}
	}

	free(device->bundles, M_QAT);
	device->bundles = NULL;

	for (i = 0; i < device->instance_index; i++) {
		if (device->instances && device->instances[i]) {
			free(device->instances[i], M_QAT);
			device->instances[i] = NULL;
		}
	}

	free(device->instances, M_QAT);
	device->instances = NULL;
}

void
adf_cfg_device_clear_all(struct adf_accel_dev *accel_dev)
{
	sx_xlock(&accel_dev->cfg->lock);
	if (accel_dev->cfg->dev) {
		adf_cfg_device_clear(accel_dev->cfg->dev, accel_dev);
                free(accel_dev->cfg->dev, M_QAT);
		accel_dev->cfg->dev = NULL;
	}
	sx_xunlock(&accel_dev->cfg->lock);
}

/*
 * Static configuration for userspace
 */
static int
adf_cfg_static_conf_user(struct adf_accel_dev *accel_dev,
			 int cy_enabled,
			 int dc_enabled)
{
	int ret = 0;
	unsigned long val = 0;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	int cy_user_instances = 0;
	int dc_user_instances = 0;
	int i = 0;
	int cpus = num_online_cpus();

	if (!(IS_QAT_GEN4(pci_get_device(GET_DEV(accel_dev))))) {
		device_printf(
		    GET_DEV(accel_dev),
		    "User space configuration supported only on QAT 4xxx devices\n");
		return ENXIO;
	}

	ret |= adf_cfg_section_add(accel_dev, ADF_SAL_SEC);

	if (accel_dev->is_vf) {
		if (cy_enabled)
			cy_user_instances =
			    ADF_CFG_STATIC_CONF_USER_INST_NUM_CY_VF;

		if (dc_enabled)
			dc_user_instances =
			    ADF_CFG_STATIC_CONF_USER_INST_NUM_DC_VF;
	} else {
		if (cy_enabled)
			cy_user_instances =
			    ADF_CFG_STATIC_CONF_USER_INST_NUM_CY;

		if (dc_enabled)
			dc_user_instances =
			    ADF_CFG_STATIC_CONF_USER_INST_NUM_DC;
	}

	val = cy_user_instances;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_NUM_CY);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

	val = dc_user_instances;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_NUM_DC);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

	val = accel_dev->cfg->num_user_processes;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_NUM_PROCESSES);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

	for (i = 0; i < cy_user_instances; i++) {
		val = (accel_dev->accel_id * cy_user_instances + i) % cpus;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_ETRMGR_CORE_AFFINITY,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_POLL;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_POLL_MODE,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

		snprintf(value, ADF_CFG_MAX_VAL_LEN_IN_BYTES, ADF_CY "%d", i);
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_NAME_FORMAT,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)value, ADF_STR);
	}

	for (i = 0; i < dc_user_instances; i++) {
		val = (accel_dev->accel_id * dc_user_instances + i) % cpus;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC "%d" ADF_ETRMGR_CORE_AFFINITY,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_POLL;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC "%d" ADF_POLL_MODE,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)&val, ADF_DEC);

		snprintf(value, ADF_CFG_MAX_VAL_LEN_IN_BYTES, ADF_DC "%d", i);
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC_NAME_FORMAT,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_SAL_SEC, key, (void *)value, ADF_STR);
	}

	return ret;
}

static int
adf_cfg_static_conf_kernel(struct adf_accel_dev *accel_dev,
			   int asym_enabled,
			   int sym_enabled,
			   int dc_enabled)
{
	int ret = 0;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	unsigned long val = 0;
	int i = 0;
	int instances = 0;
	int cy_poll_instances = 0;
	int cy_irq_instances = 0;
	int dc_instances = 0;
	int def_cy_poll_inst = ADF_CFG_STATIC_CONF_INST_NUM_CY_POLL;
	int def_cy_irq_inst = ADF_CFG_STATIC_CONF_INST_NUM_CY_IRQ;
	int def_dc_inst = ADF_CFG_STATIC_CONF_INST_NUM_DC;
	int cpus = num_online_cpus();

	instances = GET_MAX_BANKS(accel_dev);
	if (!instances)
		return EFAULT;

	if (accel_dev->is_vf) {
		def_cy_poll_inst = ADF_CFG_STATIC_CONF_INST_NUM_CY_POLL_VF;
		def_cy_irq_inst = ADF_CFG_STATIC_CONF_INST_NUM_CY_IRQ_VF;
		def_dc_inst = ADF_CFG_STATIC_CONF_INST_NUM_DC_VF;
	}

	/* Get the mode enabled by user */
	ret |= adf_cfg_section_add(accel_dev, ADF_KERNEL_SAL_SEC);

	if (dc_enabled) {
		if (instances >= def_dc_inst) {
			dc_instances = def_dc_inst;
			instances -= dc_instances;
		} else {
			return EFAULT;
		}
	}

	if (asym_enabled || sym_enabled) {
		if (instances >= def_cy_poll_inst) {
			cy_poll_instances = def_cy_poll_inst;
			instances -= cy_poll_instances;
		} else {
			return EFAULT;
		}

		if (sym_enabled) {
			if (instances >= def_cy_irq_inst) {
				cy_irq_instances = def_cy_irq_inst;
				instances -= cy_irq_instances;
			} else {
				return EFAULT;
			}
		}
	}

	val = (cy_poll_instances + cy_irq_instances);
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_NUM_CY);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

	val = dc_instances;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_NUM_DC);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

	for (i = 0; i < (cy_irq_instances); i++) {
		val = (accel_dev->accel_id * cy_irq_instances + i) % cpus;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_ETRMGR_CORE_AFFINITY,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_IRQ;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_POLL_MODE,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		snprintf(value, ADF_CFG_MAX_VAL_LEN_IN_BYTES, ADF_CY "%d", i);
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_NAME_FORMAT,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)value, ADF_STR);
	}

	for (i = cy_irq_instances; i < (cy_poll_instances + cy_irq_instances);
	     i++) {
		val = (accel_dev->accel_id * cy_poll_instances + i) % cpus;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_ETRMGR_CORE_AFFINITY,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_POLL;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY "%d" ADF_POLL_MODE,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		snprintf(value, ADF_CFG_MAX_VAL_LEN_IN_BYTES, ADF_CY "%d", i);
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_CY_NAME_FORMAT,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)value, ADF_STR);
	}

	for (i = 0; i < dc_instances; i++) {
		val = (accel_dev->accel_id * dc_instances + i) % cpus;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC "%d" ADF_ETRMGR_CORE_AFFINITY,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_POLL;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC "%d" ADF_POLL_MODE,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)&val, ADF_DEC);

		snprintf(value, ADF_CFG_MAX_VAL_LEN_IN_BYTES, ADF_DC "%d", i);
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_DC_NAME_FORMAT,
			 i);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_KERNEL_SAL_SEC, key, (void *)value, ADF_STR);
	}

	return ret;
}

static int
adf_cfg_static_conf(struct adf_accel_dev *accel_dev)
{
	int ret = 0;
	unsigned long val = 0;
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	char *token, *cur_str;
	int ks_enabled = 0;
	int us_enabled = 0;
	int asym_enabled = 0;
	int sym_enabled = 0;
	int cy_enabled = 0;
	int dc_enabled = 0;

	strncpy(value, accel_dev->cfg->cfg_mode, ADF_CFG_MAX_VAL);
	cur_str = value;

	token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	while (token) {
		if (!strncmp(token, ADF_CFG_KERNEL, strlen(ADF_CFG_KERNEL)))
			ks_enabled = 1;
		if (!strncmp(token, ADF_CFG_USER, strlen(ADF_CFG_USER)))
			us_enabled = 1;
		token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	}

	/* Get the services enabled by user */
	strncpy(value, accel_dev->cfg->cfg_services, ADF_CFG_MAX_VAL);
	cur_str = value;

	token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	while (token) {
		if (!strncmp(token, ADF_CFG_SYM, strlen(ADF_CFG_SYM))) {
			sym_enabled = 1;
		}
		if (!strncmp(token, ADF_CFG_ASYM, strlen(ADF_CFG_ASYM))) {
			asym_enabled = 1;
		}
		/* cy means both asym & crypto should be enabled
		 * Hardware resources allocation check will be done later
		 */
		if (!strncmp(token, ADF_CFG_CY, strlen(ADF_CFG_CY))) {
			asym_enabled = 1;
			sym_enabled = 1;
		}
		if (!strncmp(token, ADF_SERVICE_DC, strlen(ADF_SERVICE_DC))) {
			dc_enabled = 1;
		}

		token = strsep(&cur_str, ADF_SERVICES_SEPARATOR);
	}

	if (asym_enabled || sym_enabled) {
		cy_enabled = 1;
	}

	ret |= adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC);
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_SERVICES_ENABLED);

	if (strcmp(ADF_CFG_SYM_ASYM, accel_dev->cfg->cfg_services) == 0) {
		strncpy(value, ADF_CFG_CY, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	} else {
		strncpy(value,
			accel_dev->cfg->cfg_services,
			ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	}

	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)value, ADF_STR);

	val = ADF_CFG_STATIC_CONF_VER;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_CONFIG_VERSION);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_AUTO_RESET;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_AUTO_RESET_ON_ERROR);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	if (accel_dev->hw_device->get_num_accel_units) {
		int cy_au = 0;
		int dc_au = 0;
		int num_au = accel_dev->hw_device->get_num_accel_units(
		    accel_dev->hw_device);

		if (num_au > ADF_CFG_STATIC_CONF_NUM_DC_ACCEL_UNITS) {
			cy_au = num_au - ADF_CFG_STATIC_CONF_NUM_DC_ACCEL_UNITS;
			dc_au = ADF_CFG_STATIC_CONF_NUM_DC_ACCEL_UNITS;
		} else if (num_au == ADF_CFG_STATIC_CONF_NUM_DC_ACCEL_UNITS) {
			cy_au = 1;
			dc_au = 1;
		} else {
			return EFAULT;
		}

		/* User defined adjustement basing on serives enabled */
		if (cy_enabled && !dc_enabled) {
			cy_au += dc_au;
			dc_au = 0;
		} else if (!cy_enabled && dc_enabled) {
			dc_au += cy_au;
			cy_au = 0;
		}

		val = cy_au;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_NUM_CY_ACCEL_UNITS);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

		val = dc_au;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_NUM_DC_ACCEL_UNITS);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

		val = ADF_CFG_STATIC_CONF_NUM_INLINE_ACCEL_UNITS;
		snprintf(key,
			 ADF_CFG_MAX_KEY_LEN_IN_BYTES,
			 ADF_NUM_INLINE_ACCEL_UNITS);
		ret |= adf_cfg_add_key_value_param(
		    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);
	}

	val = ADF_CFG_STATIC_CONF_CY_ASYM_RING_SIZE;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_CY ADF_RING_ASYM_SIZE);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_CY_SYM_RING_SIZE;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_CY ADF_RING_SYM_SIZE);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_DC_INTER_BUF_SIZE;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, ADF_INTER_BUF_SIZE);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DC;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_DC);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DH;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_DH);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DRBG;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_DRBG);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_DSA;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_DSA);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_ECC;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_ECC);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_ENABLED;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_ENABLED);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_KEYGEN;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_KEYGEN);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_LN;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_LN);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_PRIME;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_PRIME);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_RSA;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_RSA);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	val = ADF_CFG_STATIC_CONF_SAL_STATS_CFG_SYM;
	snprintf(key, ADF_CFG_MAX_KEY_LEN_IN_BYTES, SAL_STATS_CFG_SYM);
	ret |= adf_cfg_add_key_value_param(
	    accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC);

	if (ks_enabled) {
		ret |= adf_cfg_static_conf_kernel(accel_dev,
						  asym_enabled,
						  sym_enabled,
						  dc_enabled);
	}

	if (us_enabled) {
		ret |=
		    adf_cfg_static_conf_user(accel_dev, cy_enabled, dc_enabled);
	}

	if (ret)
		ret = ENXIO;
	return ret;
}

int
adf_config_device(struct adf_accel_dev *accel_dev)
{
	struct adf_cfg_device_data *cfg = NULL;
	struct adf_cfg_device *cfg_device = NULL;
	struct adf_cfg_section *sec;
	struct list_head *list;
	int ret = ENOMEM;

	if (!accel_dev)
		return ret;

	ret = adf_cfg_static_conf(accel_dev);
	if (ret)
		goto failed;

	cfg = accel_dev->cfg;
	cfg->dev = NULL;
	cfg_device = (struct adf_cfg_device *)malloc(sizeof(*cfg_device),
						     M_QAT,
						     M_WAITOK | M_ZERO);

	ret = EFAULT;

	if (adf_cfg_device_init(cfg_device, accel_dev))
		goto failed;

	cfg->dev = cfg_device;

	/* GENERAL and KERNEL section must be processed before others */
	list_for_each(list, &cfg->sec_list)
	{
		sec = list_entry(list, struct adf_cfg_section, list);
		if (!strcmp(sec->name, ADF_GENERAL_SEC)) {
			ret = adf_cfg_process_section(accel_dev,
						      sec->name,
						      accel_dev->accel_id);
			if (ret)
				goto failed;
			sec->processed = true;
			break;
		}
	}

	list_for_each(list, &cfg->sec_list)
	{
		sec = list_entry(list, struct adf_cfg_section, list);
		if (!strcmp(sec->name, ADF_KERNEL_SEC)) {
			ret = adf_cfg_process_section(accel_dev,
						      sec->name,
						      accel_dev->accel_id);
			if (ret)
				goto failed;
			sec->processed = true;
			break;
		}
	}

	list_for_each(list, &cfg->sec_list)
	{
		sec = list_entry(list, struct adf_cfg_section, list);
		if (!strcmp(sec->name, ADF_KERNEL_SAL_SEC)) {
			ret = adf_cfg_process_section(accel_dev,
						      sec->name,
						      accel_dev->accel_id);
			if (ret)
				goto failed;
			sec->processed = true;
			break;
		}
	}

	list_for_each(list, &cfg->sec_list)
	{
		sec = list_entry(list, struct adf_cfg_section, list);
		/* avoid reprocessing one section */
		if (!sec->processed && !sec->is_derived) {
			ret = adf_cfg_process_section(accel_dev,
						      sec->name,
						      accel_dev->accel_id);
			if (ret)
				goto failed;
			sec->processed = true;
		}
	}

	/* newly added accel section */
	ret = adf_cfg_process_section(accel_dev,
				      ADF_ACCEL_SEC,
				      accel_dev->accel_id);
	if (ret)
		goto failed;

	/*
	 * put item-remove task after item-process
	 * because during process we may fetch values from those items
	 */
	list_for_each(list, &cfg->sec_list)
	{
		sec = list_entry(list, struct adf_cfg_section, list);
		if (!sec->is_derived) {
			ret = adf_cfg_cleanup_section(accel_dev,
						      sec->name,
						      accel_dev->accel_id);
			if (ret)
				goto failed;
		}
	}

	ret = 0;
	set_bit(ADF_STATUS_CONFIGURED, &accel_dev->status);
failed:
	if (ret) {
		if (cfg_device) {
			adf_cfg_device_clear(cfg_device, accel_dev);
			free(cfg_device, M_QAT);
			cfg->dev = NULL;
		}
		adf_cfg_del_all(accel_dev);
		device_printf(GET_DEV(accel_dev), "Failed to config device\n");
	}

	return ret;
}
