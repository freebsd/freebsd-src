/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_cfg.h"
#include "cpa.h"
#include "icp_accel_devices.h"
#include "adf_common_drv.h"
#include "icp_adf_accel_mgr.h"
#include "icp_adf_cfg.h"
#include "icp_adf_debug.h"
#include "icp_adf_init.h"
#include "lac_sal_ctrl.h"

static subservice_registation_handle_t *salService = NULL;
static struct service_hndl adfService = { 0 };
static icp_accel_dev_t *adfDevices = NULL;
static icp_accel_dev_t *adfDevicesHead = NULL;
struct mtx *adfDevicesLock;

/*
 * Need to keep track of what device is currently in reset state
 */
static char accel_dev_reset_stat[ADF_MAX_DEVICES] = { 0 };

/*
 * Need to keep track of what device is currently in error state
 */
static char accel_dev_error_stat[ADF_MAX_DEVICES] = { 0 };

/*
 * Need to preserve sal handle during restart
 */
static void *accel_dev_sal_hdl_ptr[ADF_MAX_DEVICES] = { 0 };

static icp_accel_dev_t *
create_adf_dev_structure(struct adf_accel_dev *accel_dev)
{
	icp_accel_dev_t *adf = NULL;

	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	adf = malloc(sizeof(*adf), M_QAT, M_WAITOK);
	memset(adf, 0, sizeof(*adf));
	adf->accelId = accel_dev->accel_id;
	adf->pAccelName = (char *)hw_data->dev_class->name;
	adf->deviceType = (device_type_t)hw_data->dev_class->type;
	strlcpy(adf->deviceName,
		hw_data->dev_class->name,
		sizeof(adf->deviceName));
	adf->accelCapabilitiesMask = hw_data->accel_capabilities_mask;
	adf->sku = hw_data->get_sku(hw_data);
	adf->accel_dev = accel_dev;
	accel_dev->lac_dev = adf;

	return adf;
}

/*
 * adf_event_handler
 * Handle device init/uninit/start/stop event
 */
static CpaStatus
adf_event_handler(struct adf_accel_dev *accel_dev, enum adf_event event)
{
	CpaStatus status = CPA_STATUS_FAIL;
	icp_accel_dev_t *adf = NULL;

	if (!adf_cfg_sec_find(accel_dev, ADF_KERNEL_SAL_SEC)) {
		return CPA_STATUS_SUCCESS;
	}

	if (event == ADF_EVENT_INIT) {
		adf = create_adf_dev_structure(accel_dev);
		if (NULL == adf) {
			return CPA_STATUS_FAIL;
		}
		if (accel_dev_sal_hdl_ptr[accel_dev->accel_id]) {
			adf->pSalHandle =
			    accel_dev_sal_hdl_ptr[accel_dev->accel_id];
			accel_dev_sal_hdl_ptr[accel_dev->accel_id] = NULL;
		}

		qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
		ICP_ADD_ELEMENT_TO_END_OF_LIST(adf, adfDevices, adfDevicesHead);
		qatUtilsMutexUnlock(&adfDevicesLock);
	} else {
		adf = accel_dev->lac_dev;
	}

	if (event == ADF_EVENT_START) {
		adf->dcExtendedFeatures =
		    accel_dev->hw_device->extended_dc_capabilities;
	}

	if (event == ADF_EVENT_RESTARTING) {
		accel_dev_reset_stat[accel_dev->accel_id] = 1;
		accel_dev_sal_hdl_ptr[accel_dev->accel_id] = adf->pSalHandle;
	}

	if (event == ADF_EVENT_RESTARTED) {
		accel_dev_reset_stat[accel_dev->accel_id] = 0;
		accel_dev_error_stat[accel_dev->accel_id] = 0;
	}

	status =
	    salService->subserviceEventHandler(adf,
					       (icp_adf_subsystemEvent_t)event,
					       NULL);

	if (event == ADF_EVENT_ERROR) {
		accel_dev_error_stat[accel_dev->accel_id] = 1;
	}

	if ((status == CPA_STATUS_SUCCESS && event == ADF_EVENT_SHUTDOWN) ||
	    (status != CPA_STATUS_SUCCESS && event == ADF_EVENT_INIT)) {
		qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
		ICP_REMOVE_ELEMENT_FROM_LIST(adf, adfDevices, adfDevicesHead);
		qatUtilsMutexUnlock(&adfDevicesLock);
		accel_dev->lac_dev = NULL;
		free(adf, M_QAT);
	}

	if (status == CPA_STATUS_SUCCESS && event == ADF_EVENT_START) {
		qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
		adf->adfSubsystemStatus = 1;
		qatUtilsMutexUnlock(&adfDevicesLock);
	}

	if ((status == CPA_STATUS_SUCCESS && event == ADF_EVENT_STOP) ||
	    (status == CPA_STATUS_RETRY && event == ADF_EVENT_STOP)) {
		qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
		adf->adfSubsystemStatus = 0;
		qatUtilsMutexUnlock(&adfDevicesLock);
		status = CPA_STATUS_SUCCESS;
	}

	return status;
}

/*
 * icp_adf_subsystemRegister
 * adapter function from SAL to adf driver
 * call adf_service_register from adf driver directly with same
 * parameters
 */
CpaStatus
icp_adf_subsystemRegister(
    subservice_registation_handle_t *sal_service_reg_handle)
{
	if (salService != NULL)
		return CPA_STATUS_FAIL;

	salService = sal_service_reg_handle;
	adfService.name = sal_service_reg_handle->subsystem_name;
	adfService.event_hld = adf_event_handler;

	if (adf_service_register(&adfService) == 0) {
		return CPA_STATUS_SUCCESS;
	} else {
		salService = NULL;
		return CPA_STATUS_FAIL;
	}
}

/*
 * icp_adf_subsystemUnegister
 * adapter function from SAL to adf driver
 */
CpaStatus
icp_adf_subsystemUnregister(
    subservice_registation_handle_t *sal_service_reg_handle)
{
	if (adf_service_unregister(&adfService) == 0) {
		salService = NULL;
		return CPA_STATUS_SUCCESS;
	} else {
		return CPA_STATUS_FAIL;
	}
}

/*
 * icp_adf_cfgGetParamValue
 * get parameter value from section @section with key @param
 */
CpaStatus
icp_adf_cfgGetParamValue(icp_accel_dev_t *adf,
			 const char *section,
			 const char *param,
			 char *value)
{
	if (adf_cfg_get_param_value(adf->accel_dev, section, param, value) ==
	    0) {
		return CPA_STATUS_SUCCESS;
	} else {
		return CPA_STATUS_FAIL;
	}
}

CpaBoolean
icp_adf_is_dev_in_reset(icp_accel_dev_t *accel_dev)
{
	return (CpaBoolean)accel_dev_reset_stat[accel_dev->accelId];
}

CpaStatus
icp_adf_debugAddDir(icp_accel_dev_t *adf, debug_dir_info_t *dir_info)
{
	return CPA_STATUS_SUCCESS;
}

void
icp_adf_debugRemoveDir(debug_dir_info_t *dir_info)
{
}

CpaStatus
icp_adf_debugAddFile(icp_accel_dev_t *adf, debug_file_info_t *file_info)
{
	return CPA_STATUS_SUCCESS;
}

void
icp_adf_debugRemoveFile(debug_file_info_t *file_info)
{
}

/*
 * icp_adf_getAccelDevByAccelId
 * return acceleration device with id @accelId
 */
icp_accel_dev_t *
icp_adf_getAccelDevByAccelId(Cpa32U accelId)
{
	icp_accel_dev_t *adf = NULL;

	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	adf = adfDevicesHead;
	while (adf != NULL && adf->accelId != accelId)
		adf = adf->pNext;
	qatUtilsMutexUnlock(&adfDevicesLock);
	return adf;
}

/*
 * icp_amgr_getNumInstances
 * Return the number of acceleration devices it the system.
 */
CpaStatus
icp_amgr_getNumInstances(Cpa16U *pNumInstances)
{
	icp_accel_dev_t *adf = NULL;
	Cpa16U count = 0;

	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	for (adf = adfDevicesHead; adf != NULL; adf = adf->pNext)
		count++;
	qatUtilsMutexUnlock(&adfDevicesLock);
	*pNumInstances = count;
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_amgr_getAccelDevByCapabilities
 * Returns a started accel device that implements
 * the capabilities specified in capabilitiesMask.
 */
CpaStatus
icp_amgr_getAccelDevByCapabilities(Cpa32U capabilitiesMask,
				   icp_accel_dev_t **pAccel_devs,
				   Cpa16U *pNumInstances)
{
	icp_accel_dev_t *adf = NULL;
	*pNumInstances = 0;

	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	for (adf = adfDevicesHead; adf != NULL; adf = adf->pNext) {
		if (adf->accelCapabilitiesMask & capabilitiesMask) {
			if (adf->adfSubsystemStatus) {
				pAccel_devs[0] = adf;
				*pNumInstances = 1;
				qatUtilsMutexUnlock(&adfDevicesLock);
				return CPA_STATUS_SUCCESS;
			}
		}
	}
	qatUtilsMutexUnlock(&adfDevicesLock);
	return CPA_STATUS_FAIL;
}

/*
 * icp_amgr_getAllAccelDevByEachCapabilities
 * Returns table of accel devices that are started and implement
 * each of the capabilities specified in capabilitiesMask.
 */
CpaStatus
icp_amgr_getAllAccelDevByEachCapability(Cpa32U capabilitiesMask,
					icp_accel_dev_t **pAccel_devs,
					Cpa16U *pNumInstances)
{
	icp_accel_dev_t *adf = NULL;
	*pNumInstances = 0;
	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	for (adf = adfDevicesHead; adf != NULL; adf = adf->pNext) {
		Cpa32U enabled_caps =
		    adf->accelCapabilitiesMask & capabilitiesMask;
		if (enabled_caps == capabilitiesMask) {
			if (adf->adfSubsystemStatus) {
				pAccel_devs[(*pNumInstances)++] =
				    (icp_accel_dev_t *)adf;
			}
		}
	}
	qatUtilsMutexUnlock(&adfDevicesLock);
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_amgr_getAllAccelDevByCapabilities
 * Fetches accel devices based on the capability
 * and returns the count of the device
 */
CpaStatus
icp_amgr_getAllAccelDevByCapabilities(Cpa32U capabilitiesMask,
				      icp_accel_dev_t **pAccel_devs,
				      Cpa16U *pNumInstances)
{
	icp_accel_dev_t *adf = NULL;
	Cpa16U i = 0;

	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	for (adf = adfDevicesHead; adf != NULL; adf = adf->pNext) {
		if (adf->accelCapabilitiesMask & capabilitiesMask) {
			if (adf->adfSubsystemStatus) {
				pAccel_devs[i++] = adf;
			}
		}
	}
	qatUtilsMutexUnlock(&adfDevicesLock);
	*pNumInstances = i;
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_amgr_getAccelDevCapabilities
 * Returns accel devices capabilities specified in capabilitiesMask.
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus
icp_amgr_getAccelDevCapabilities(icp_accel_dev_t *accel_dev,
				 Cpa32U *pCapabilitiesMask)
{
	ICP_CHECK_FOR_NULL_PARAM(accel_dev);
	ICP_CHECK_FOR_NULL_PARAM(pCapabilitiesMask);

	*pCapabilitiesMask = accel_dev->accelCapabilitiesMask;
	return CPA_STATUS_SUCCESS;
}

/*
 * icp_qa_dev_get
 *
 * Description:
 * Function increments the device usage counter.
 *
 * Returns: void
 */
void
icp_qa_dev_get(icp_accel_dev_t *pDev)
{
	ICP_CHECK_FOR_NULL_PARAM_VOID(pDev);
	adf_dev_get(pDev->accel_dev);
}

/*
 * icp_qa_dev_put
 *
 * Description:
 * Function decrements the device usage counter.
 *
 * Returns: void
 */
void
icp_qa_dev_put(icp_accel_dev_t *pDev)
{
	ICP_CHECK_FOR_NULL_PARAM_VOID(pDev);
	adf_dev_put(pDev->accel_dev);
}

Cpa16U
icp_adf_get_busAddress(Cpa16U packageId)
{
	Cpa16U busAddr = 0xFFFF;
	icp_accel_dev_t *adf = NULL;

	qatUtilsMutexLock(&adfDevicesLock, QAT_UTILS_WAIT_FOREVER);
	for (adf = adfDevicesHead; adf != NULL; adf = adf->pNext) {
		if (adf->accelId == packageId) {
			busAddr = pci_get_bus(accel_to_pci_dev(adf->accel_dev))
				<< 8 |
			    pci_get_slot(accel_to_pci_dev(adf->accel_dev))
				<< 3 |
			    pci_get_function(accel_to_pci_dev(adf->accel_dev));
			break;
		}
	}
	qatUtilsMutexUnlock(&adfDevicesLock);
	return busAddr;
}

CpaBoolean
icp_adf_isSubsystemStarted(subservice_registation_handle_t *subsystem_hdl)
{
	if (subsystem_hdl == salService)
		return CPA_TRUE;
	else
		return CPA_FALSE;
}

CpaBoolean
icp_adf_is_dev_in_error(icp_accel_dev_t *accel_dev)
{
	return (CpaBoolean)accel_dev_error_stat[accel_dev->accelId];
}
