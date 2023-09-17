/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/firmware.h>
#include <dev/pci/pcivar.h>
#include "adf_cfg.h"
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "icp_qat_uclo.h"
#include "icp_qat_hw.h"

#define MMP_VERSION_LEN 4

struct adf_mmp_version_s {
	u8 ver_val[MMP_VERSION_LEN];
};

static int
request_firmware(const struct firmware **firmware_p, const char *name)
{
	int retval = 0;
	if (NULL == firmware_p) {
		return -1;
	}
	*firmware_p = firmware_get(name);
	if (NULL == *firmware_p) {
		retval = -1;
	}
	return retval;
}

int
adf_ae_fw_load(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	const void *fw_addr, *mmp_addr;
	u32 fw_size, mmp_size;
	s32 i = 0;
	u32 max_objs = 1;
	const char *obj_name = NULL;
	struct adf_mmp_version_s mmp_ver = { { 0 } };
	unsigned int cfg_ae_mask = 0;

	if (!hw_device->fw_name)
		return 0;

	if (request_firmware(&loader_data->uof_fw, hw_device->fw_name)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to load UOF FW %s\n",
			      hw_device->fw_name);
		goto out_err;
	}

	if (request_firmware(&loader_data->mmp_fw, hw_device->fw_mmp_name)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to load MMP FW %s\n",
			      hw_device->fw_mmp_name);
		goto out_err;
	}

	fw_size = loader_data->uof_fw->datasize;
	fw_addr = loader_data->uof_fw->data;
	mmp_size = loader_data->mmp_fw->datasize;
	mmp_addr = loader_data->mmp_fw->data;

	memcpy(&mmp_ver, mmp_addr, MMP_VERSION_LEN);

	accel_dev->fw_versions.mmp_version_major = mmp_ver.ver_val[0];
	accel_dev->fw_versions.mmp_version_minor = mmp_ver.ver_val[1];
	accel_dev->fw_versions.mmp_version_patch = mmp_ver.ver_val[2];

	if (hw_device->accel_capabilities_mask &
	    ADF_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC)
		if (qat_uclo_wr_mimage(loader_data->fw_loader,
				       mmp_addr,
				       mmp_size)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to load MMP\n");
			goto out_err;
		}

	if (hw_device->get_objs_num)
		max_objs = hw_device->get_objs_num(accel_dev);

	for (i = max_objs - 1; i >= 0; i--) {
		/* obj_name is used to indicate the firmware name in MOF,
		 * config unit0 must be loaded at end for authentication
		 */
		if (hw_device->get_obj_name && hw_device->get_obj_cfg_ae_mask) {
			unsigned long service_mask = hw_device->service_mask;
			enum adf_accel_unit_services service_type =
			    ADF_ACCEL_SERVICE_NULL;

			if (hw_device->get_service_type)
				service_type =
				    hw_device->get_service_type(accel_dev, i);
			else
				service_type = BIT(i);

			if (service_mask && !(service_mask & service_type))
				continue;

			obj_name =
			    hw_device->get_obj_name(accel_dev, service_type);
			cfg_ae_mask =
			    hw_device->get_obj_cfg_ae_mask(accel_dev,
							   service_type);

			if (!obj_name) {
				device_printf(
				    GET_DEV(accel_dev),
				    "Invalid object (service = %lx)\n",
				    BIT(i));
				goto out_err;
			}
			if (!cfg_ae_mask)
				continue;
			if (qat_uclo_set_cfg_ae_mask(loader_data->fw_loader,
						     cfg_ae_mask)) {
				device_printf(GET_DEV(accel_dev),
					      "Invalid config AE mask\n");
				goto out_err;
			}
		}

		if (qat_uclo_map_obj(
			loader_data->fw_loader, fw_addr, fw_size, obj_name)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to map UOF firmware\n");
			goto out_err;
		}
		if (qat_uclo_wr_all_uimage(loader_data->fw_loader)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to load UOF firmware\n");
			goto out_err;
		}
		qat_uclo_del_obj(loader_data->fw_loader);
		obj_name = NULL;
	}

	return 0;

out_err:
	adf_ae_fw_release(accel_dev);
	return EFAULT;
}

void
adf_ae_fw_release(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	if (!hw_device->fw_name)
		return;
	if (loader_data->fw_loader)
		qat_uclo_del_obj(loader_data->fw_loader);
	if (loader_data->fw_loader && loader_data->fw_loader->mobj_handle)
		qat_uclo_del_mof(loader_data->fw_loader);
	qat_hal_deinit(loader_data->fw_loader);
	if (loader_data->uof_fw)
		firmware_put(loader_data->uof_fw, FIRMWARE_UNLOAD);
	if (loader_data->mmp_fw)
		firmware_put(loader_data->mmp_fw, FIRMWARE_UNLOAD);
	loader_data->uof_fw = NULL;
	loader_data->mmp_fw = NULL;
	loader_data->fw_loader = NULL;
}

int
adf_ae_start(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	uint32_t ae_ctr;

	if (!hw_data->fw_name)
		return 0;

	ae_ctr = qat_hal_start(loader_data->fw_loader);
	device_printf(GET_DEV(accel_dev),
		      "qat_dev%d started %d acceleration engines\n",
		      accel_dev->accel_id,
		      ae_ctr);
	return 0;
}

int
adf_ae_stop(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	uint32_t ae_ctr, ae, max_aes = GET_MAX_ACCELENGINES(accel_dev);

	if (!hw_data->fw_name)
		return 0;

	for (ae = 0, ae_ctr = 0; ae < max_aes; ae++) {
		if (hw_data->ae_mask & (1 << ae)) {
			qat_hal_stop(loader_data->fw_loader, ae, 0xFF);
			ae_ctr++;
		}
	}
	device_printf(GET_DEV(accel_dev),
		      "qat_dev%d stopped %d acceleration engines\n",
		      accel_dev->accel_id,
		      ae_ctr);
	return 0;
}

static int
adf_ae_reset(struct adf_accel_dev *accel_dev, int ae)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;

	qat_hal_reset(loader_data->fw_loader);
	if (qat_hal_clr_reset(loader_data->fw_loader))
		return EFAULT;

	return 0;
}

int
adf_ae_init(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	if (!hw_device->fw_name)
		return 0;

	loader_data = malloc(sizeof(*loader_data), M_QAT, M_WAITOK | M_ZERO);

	accel_dev->fw_loader = loader_data;
	if (qat_hal_init(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Failed to init the AEs\n");
		free(loader_data, M_QAT);
		return EFAULT;
	}
	if (adf_ae_reset(accel_dev, 0)) {
		device_printf(GET_DEV(accel_dev), "Failed to reset the AEs\n");
		qat_hal_deinit(loader_data->fw_loader);
		free(loader_data, M_QAT);
		return EFAULT;
	}
	return 0;
}

int
adf_ae_shutdown(struct adf_accel_dev *accel_dev)
{
	struct adf_fw_loader_data *loader_data = accel_dev->fw_loader;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;

	if (!hw_device->fw_name)
		return 0;

	qat_hal_deinit(loader_data->fw_loader);
	free(accel_dev->fw_loader, M_QAT);
	accel_dev->fw_loader = NULL;
	return 0;
}
