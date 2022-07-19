/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file dev_info.c
 *
 * @defgroup Device
 *
 * @description
 *    This file contains implementation of functions for device level APIs
 *
 *****************************************************************************/

/* QAT-API includes */
#include "cpa_dev.h"
#include "icp_accel_devices.h"
#include "lac_common.h"
#include "icp_adf_cfg.h"
#include "lac_sal_types.h"
#include "icp_adf_accel_mgr.h"
#include "sal_string_parse.h"
#include "lac_sal.h"

CpaStatus
cpaGetNumDevices(Cpa16U *numDevices)
{
	LAC_CHECK_NULL_PARAM(numDevices);

	return icp_amgr_getNumInstances(numDevices);
}

CpaStatus
cpaGetDeviceInfo(Cpa16U device, CpaDeviceInfo *deviceInfo)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	icp_accel_dev_t *pDevice = NULL;
	Cpa16U numDevicesAvail = 0;
	Cpa32U capabilitiesMask = 0;
	Cpa32U enabledServices = 0;

	LAC_CHECK_NULL_PARAM(deviceInfo);
	status = icp_amgr_getNumInstances(&numDevicesAvail);
	/* Check if the application is not attempting to access a
	 * device that does not exist.
	 */
	if (0 == numDevicesAvail) {
		QAT_UTILS_LOG("Failed to retrieve number of devices!\n");
		return CPA_STATUS_FAIL;
	}
	if (device >= numDevicesAvail) {
		QAT_UTILS_LOG(
		    "Invalid device access! Number of devices available: %d.\n",
		    numDevicesAvail);
		return CPA_STATUS_FAIL;
	}

	/* Clear the entire capability structure before initialising it */
	memset(deviceInfo, 0x00, sizeof(CpaDeviceInfo));
	/* Bus/Device/Function should be 0xFF until initialised */
	deviceInfo->bdf = 0xffff;

	pDevice = icp_adf_getAccelDevByAccelId(device);
	if (NULL == pDevice) {
		QAT_UTILS_LOG("Failed to retrieve device.\n");
		return status;
	}

	/* Device of interest is found, retrieve the information for it */
	deviceInfo->sku = pDevice->sku;
	deviceInfo->deviceId = pDevice->pciDevId;
	deviceInfo->bdf = icp_adf_get_busAddress(pDevice->accelId);
	deviceInfo->numaNode = pDevice->pkg_id;

	if (DEVICE_DH895XCCVF == pDevice->deviceType ||
	    DEVICE_C62XVF == pDevice->deviceType ||
	    DEVICE_C3XXXVF == pDevice->deviceType ||
	    DEVICE_C4XXXVF == pDevice->deviceType) {
		deviceInfo->isVf = CPA_TRUE;
	}

	status = SalCtrl_GetEnabledServices(pDevice, &enabledServices);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to retrieve enabled services!\n");
		return status;
	}

	status = icp_amgr_getAccelDevCapabilities(pDevice, &capabilitiesMask);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("Failed to retrieve accel capabilities mask!\n");
		return status;
	}

	/* Determine if Compression service is enabled */
	if (enabledServices & SAL_SERVICE_TYPE_COMPRESSION) {
		deviceInfo->dcEnabled =
		    (((capabilitiesMask & ICP_ACCEL_CAPABILITIES_COMPRESSION) !=
		      0) ?
			 CPA_TRUE :
			 CPA_FALSE);
	}

	/* Determine if Crypto service is enabled */
	if (enabledServices & SAL_SERVICE_TYPE_CRYPTO) {
		deviceInfo->cySymEnabled =
		    (((capabilitiesMask &
		       ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC)) ?
			 CPA_TRUE :
			 CPA_FALSE);
		deviceInfo->cyAsymEnabled =
		    (((capabilitiesMask &
		       ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) != 0) ?
			 CPA_TRUE :
			 CPA_FALSE);
	}
	/* Determine if Crypto Sym service is enabled */
	if (enabledServices & SAL_SERVICE_TYPE_CRYPTO_SYM) {
		deviceInfo->cySymEnabled =
		    (((capabilitiesMask &
		       ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC)) ?
			 CPA_TRUE :
			 CPA_FALSE);
	}
	/* Determine if Crypto Asym service is enabled */
	if (enabledServices & SAL_SERVICE_TYPE_CRYPTO_ASYM) {
		deviceInfo->cyAsymEnabled =
		    (((capabilitiesMask &
		       ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC) != 0) ?
			 CPA_TRUE :
			 CPA_FALSE);
	}
	deviceInfo->deviceMemorySizeAvailable = pDevice->deviceMemAvail;

	return status;
}
