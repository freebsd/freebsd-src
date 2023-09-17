/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 *****************************************************************************
 * @file sal_versions.c
 *
 * @ingroup SalVersions
 *
 * @description
 *    This file contains implementation of functions used to obtain version
 *    information
 *
 *****************************************************************************/

#include "cpa.h"
#include "qat_utils.h"

#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_cfg.h"

#include "lac_common.h"

#include "icp_sal_versions.h"

#define ICP_SAL_VERSIONS_ALL_CAP_MASK 0xFFFFFFFF
/**< Mask used to get all devices from ADF */

/**
*******************************************************************************
 * @ingroup SalVersions
 *      Fills in the version info structure
 * @description
 *      This function obtains hardware and software information associated with
 *      a given device and fills in the version info structure
 *
 * @param[in]   device      Pointer to the device for which version information
 *                          is to be obtained.
 * @param[out]  pVerInfo    Pointer to a structure that will hold version
 *                          information
 *
 * @context
 *      This function might sleep. It cannot be executed in a context that
 *      does not permit sleeping.
 * @assumptions
 *      The system has been started
 * @sideEffects
 *      None
 * @blocking
 *      No
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @return CPA_STATUS_SUCCESS       Operation finished successfully
 * @return CPA_STATUS_FAIL          Operation failed
 *
 *****************************************************************************/
static CpaStatus
SalVersions_FillVersionInfo(icp_accel_dev_t *device,
			    icp_sal_dev_version_info_t *pVerInfo)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	char param_value[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	Cpa32S strSize = 0;

	memset(pVerInfo, 0, sizeof(icp_sal_dev_version_info_t));
	pVerInfo->devId = device->accelId;

	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_HW_REV_ID_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	strSize = snprintf((char *)pVerInfo->hardwareVersion,
			   ICP_SAL_VERSIONS_HW_VERSION_SIZE,
			   "%s",
			   param_value);
	LAC_CHECK_PARAM_RANGE(strSize, 1, ICP_SAL_VERSIONS_HW_VERSION_SIZE);

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_UOF_VER_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	strSize = snprintf((char *)pVerInfo->firmwareVersion,
			   ICP_SAL_VERSIONS_FW_VERSION_SIZE,
			   "%s",
			   param_value);
	LAC_CHECK_PARAM_RANGE(strSize, 1, ICP_SAL_VERSIONS_FW_VERSION_SIZE);

	memset(param_value, 0, ADF_CFG_MAX_VAL_LEN_IN_BYTES);
	status = icp_adf_cfgGetParamValue(device,
					  LAC_CFG_SECTION_GENERAL,
					  ICP_CFG_MMP_VER_KEY,
					  param_value);
	LAC_CHECK_STATUS(status);

	strSize = snprintf((char *)pVerInfo->mmpVersion,
			   ICP_SAL_VERSIONS_MMP_VERSION_SIZE,
			   "%s",
			   param_value);
	LAC_CHECK_PARAM_RANGE(strSize, 1, ICP_SAL_VERSIONS_MMP_VERSION_SIZE);

	snprintf((char *)pVerInfo->softwareVersion,
		 ICP_SAL_VERSIONS_SW_VERSION_SIZE,
		 "%d.%d.%d",
		 SAL_INFO2_DRIVER_SW_VERSION_MAJ_NUMBER,
		 SAL_INFO2_DRIVER_SW_VERSION_MIN_NUMBER,
		 SAL_INFO2_DRIVER_SW_VERSION_PATCH_NUMBER);

	return status;
}

CpaStatus
icp_sal_getDevVersionInfo(Cpa32U devId, icp_sal_dev_version_info_t *pVerInfo)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	Cpa16U numInstances = 0;
	icp_accel_dev_t **pAccel_dev = NULL;
	Cpa16U num_accel_dev = 0, index = 0;
	icp_accel_dev_t *pDevice = NULL;

	LAC_CHECK_NULL_PARAM(pVerInfo);

	status = icp_amgr_getNumInstances(&numInstances);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Error while getting number of devices.\n");
		return CPA_STATUS_FAIL;
	}

	if (devId >= ADF_MAX_DEVICES) {
		QAT_UTILS_LOG("Invalid devId\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	pAccel_dev =
	    malloc(numInstances * sizeof(icp_accel_dev_t *), M_QAT, M_WAITOK);

	/* Get ADF to return all accel_devs */
	status =
	    icp_amgr_getAllAccelDevByCapabilities(ICP_SAL_VERSIONS_ALL_CAP_MASK,
						  pAccel_dev,
						  &num_accel_dev);

	if (CPA_STATUS_SUCCESS == status) {
		for (index = 0; index < num_accel_dev; index++) {
			pDevice = (icp_accel_dev_t *)pAccel_dev[index];

			if (pDevice->accelId == devId) {
				status = SalVersions_FillVersionInfo(pDevice,
								     pVerInfo);
				if (CPA_STATUS_SUCCESS != status) {
					QAT_UTILS_LOG(
					    "Error while filling in version info.\n");
				}
				break;
			}
		}

		if (index == num_accel_dev) {
			QAT_UTILS_LOG("Device %d not found or not started.\n",
				      devId);
			status = CPA_STATUS_FAIL;
		}
	} else {
		QAT_UTILS_LOG("Error while getting devices.\n");
	}

	free(pAccel_dev, M_QAT);
	return status;
}
