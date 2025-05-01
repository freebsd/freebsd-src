/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/**
 *****************************************************************************
 * @file sal_get_instances.c
 *
 * @defgroup SalCtrl Service Access Layer Controller
 *
 * @ingroup SalCtrl
 *
 * @description
 *      This file contains generic functions to get instances of a specified
 *      service type. Note these are complementary to the already existing
 *      service-specific functions.
 *
 *****************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

/* QAT-API includes */
#include "cpa.h"
#include "cpa_cy_common.h"
#include "cpa_cy_im.h"
#include "cpa_dc.h"

/* ADF includes */
#include "icp_accel_devices.h"
#include "icp_adf_accel_mgr.h"

/* SAL includes */
#include "lac_mem.h"
#include "lac_list.h"
#include "lac_sal_types.h"
#include "lac_sal_types_crypto.h"

/**
 ******************************************************************************
 * @ingroup SalCtrl
 * @description
 *   Get the total number of either sym, asym or cy instances
 *****************************************************************************/
CpaStatus
Lac_GetCyNumInstancesByType(
    const CpaAccelerationServiceType accelerationServiceType,
    Cpa16U *pNumInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle instanceHandle;
	CpaInstanceInfo2 info;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U num_inst = 0;
	Cpa16U i = 0;
	Cpa32U accel_capability = 0;
	char *service = NULL;

	LAC_CHECK_NULL_PARAM(pNumInstances);
	*pNumInstances = 0;

	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		service = "asym";
		break;

	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		service = "sym";
		break;

	case CPA_ACC_SVC_TYPE_CRYPTO:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		service = "cy";
		break;

	default:
		QAT_UTILS_LOG("Invalid service type\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get the number of accel_dev in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts = malloc(num_accel_dev * sizeof(icp_accel_dev_t *),
			   M_QAT,
			   M_WAITOK | M_ZERO);
	if (NULL == pAdfInsts) {
		QAT_UTILS_LOG("Failed to allocate dev instance memory\n");
		return CPA_STATUS_RESOURCE;
	}

	num_accel_dev = 0;
	status = icp_amgr_getAllAccelDevByCapabilities(accel_capability,
						       pAdfInsts,
						       &num_accel_dev);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("No support for service %s\n", service);
		free(pAdfInsts, M_QAT);
		return status;
	}

	for (i = 0; i < num_accel_dev; i++) {
		dev_addr = pAdfInsts[i];
		if (NULL == dev_addr || NULL == dev_addr->pSalHandle) {
			continue;
		}
		base_addr = dev_addr->pSalHandle;

		if (CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->crypto_services;
			while (NULL != list_temp) {
				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				if (CPA_STATUS_SUCCESS == status &&
				    CPA_TRUE == info.isPolled) {
					num_inst++;
				}
				list_temp = SalList_next(list_temp);
			}
		}

		if (CPA_ACC_SVC_TYPE_CRYPTO_ASYM == accelerationServiceType ||
		    CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->asym_services;
			while (NULL != list_temp) {
				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				if (CPA_STATUS_SUCCESS == status &&
				    CPA_TRUE == info.isPolled) {
					num_inst++;
				}
				list_temp = SalList_next(list_temp);
			}
		}

		if (CPA_ACC_SVC_TYPE_CRYPTO_SYM == accelerationServiceType ||
		    CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->sym_services;
			while (NULL != list_temp) {
				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				if (CPA_STATUS_SUCCESS == status &&
				    CPA_TRUE == info.isPolled) {
					num_inst++;
				}
				list_temp = SalList_next(list_temp);
			}
		}
	}

	*pNumInstances = num_inst;
	free(pAdfInsts, M_QAT);

	return status;
}

/**
 ******************************************************************************
 * @ingroup SalCtrl
 * @description
 *   Get either sym, asym or cy instance
 *****************************************************************************/
CpaStatus
Lac_GetCyInstancesByType(
    const CpaAccelerationServiceType accelerationServiceType,
    Cpa16U numInstances,
    CpaInstanceHandle *pInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
	CpaInstanceHandle instanceHandle = NULL;
	CpaInstanceInfo2 info;
	icp_accel_dev_t **pAdfInsts = NULL;
	icp_accel_dev_t *dev_addr = NULL;
	sal_t *base_addr = NULL;
	sal_list_t *list_temp = NULL;
	Cpa16U num_accel_dev = 0;
	Cpa16U num_allocated_instances = 0;
	Cpa16U index = 0;
	Cpa16U i = 0;
	Cpa32U accel_capability = 0;
	char *service = NULL;

	LAC_CHECK_NULL_PARAM(pInstances);
	if (0 == numInstances) {
		QAT_UTILS_LOG("NumInstances is 0\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		service = "asym";
		break;

	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC;
		service = "sym";
		break;

	case CPA_ACC_SVC_TYPE_CRYPTO:
		accel_capability = ICP_ACCEL_CAPABILITIES_CRYPTO_SYMMETRIC |
		    ICP_ACCEL_CAPABILITIES_CRYPTO_ASYMMETRIC;
		service = "cy";
		break;

	default:
		QAT_UTILS_LOG("Invalid service type\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get the number of instances */
	status = Lac_GetCyNumInstancesByType(accelerationServiceType,
					     &num_allocated_instances);
	if (CPA_STATUS_SUCCESS != status) {
		return status;
	}

	if (numInstances > num_allocated_instances) {
		QAT_UTILS_LOG("Only %d instances available\n",
			      num_allocated_instances);
		return CPA_STATUS_RESOURCE;
	}

	/* Get the number of accel devices in the system */
	status = icp_amgr_getNumInstances(&num_accel_dev);
	LAC_CHECK_STATUS(status);

	/* Allocate memory to store addr of accel_devs */
	pAdfInsts = malloc(num_accel_dev * sizeof(icp_accel_dev_t *),
			   M_QAT,
			   M_WAITOK | M_ZERO);
	if (NULL == pAdfInsts) {
		QAT_UTILS_LOG("Failed to allocate dev instance memory\n");
		return CPA_STATUS_RESOURCE;
	}

	num_accel_dev = 0;
	status = icp_amgr_getAllAccelDevByCapabilities(accel_capability,
						       pAdfInsts,
						       &num_accel_dev);
	if (CPA_STATUS_SUCCESS != status) {
		QAT_UTILS_LOG("No support for service %s\n", service);
		free(pAdfInsts, M_QAT);
		return status;
	}

	for (i = 0; i < num_accel_dev; i++) {
		dev_addr = pAdfInsts[i];
		/* Note dev_addr cannot be NULL here as numInstances = 0
		 * is not valid and if dev_addr = NULL then index = 0 (which
		 * is less than numInstances and status is set to _RESOURCE
		 * above)
		 */
		base_addr = dev_addr->pSalHandle;
		if (NULL == base_addr) {
			continue;
		}

		if (CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->crypto_services;
			while (NULL != list_temp) {
				if (index > (numInstances - 1))
					break;

				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				list_temp = SalList_next(list_temp);
				if (CPA_STATUS_SUCCESS != status ||
				    CPA_TRUE != info.isPolled) {
					continue;
				}
				pInstances[index] = instanceHandle;
				index++;
			}
		}

		if (CPA_ACC_SVC_TYPE_CRYPTO_ASYM == accelerationServiceType ||
		    CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->asym_services;
			while (NULL != list_temp) {
				if (index > (numInstances - 1))
					break;

				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				list_temp = SalList_next(list_temp);
				if (CPA_STATUS_SUCCESS != status ||
				    CPA_TRUE != info.isPolled) {
					continue;
				}
				pInstances[index] = instanceHandle;
				index++;
			}
		}

		if (CPA_ACC_SVC_TYPE_CRYPTO_SYM == accelerationServiceType ||
		    CPA_ACC_SVC_TYPE_CRYPTO == accelerationServiceType) {
			list_temp = base_addr->sym_services;
			while (NULL != list_temp) {
				if (index > (numInstances - 1))
					break;

				instanceHandle = SalList_getObject(list_temp);
				status = cpaCyInstanceGetInfo2(instanceHandle,
							       &info);
				list_temp = SalList_next(list_temp);
				if (CPA_STATUS_SUCCESS != status ||
				    CPA_TRUE != info.isPolled) {
					continue;
				}
				pInstances[index] = instanceHandle;
				index++;
			}
		}
	}
	free(pAdfInsts, M_QAT);

	return status;
}

/**
 ******************************************************************************
 * @ingroup SalCtrl
 *****************************************************************************/
CpaStatus
cpaGetNumInstances(const CpaAccelerationServiceType accelerationServiceType,
		   Cpa16U *pNumInstances)
{
	LAC_CHECK_NULL_PARAM(pNumInstances);

	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
	case CPA_ACC_SVC_TYPE_CRYPTO:
		return Lac_GetCyNumInstancesByType(accelerationServiceType,
						   pNumInstances);

	case CPA_ACC_SVC_TYPE_DATA_COMPRESSION:
		return cpaDcGetNumInstances(pNumInstances);

	case CPA_ACC_SVC_TYPE_PATTERN_MATCH:
	case CPA_ACC_SVC_TYPE_RAID:
	case CPA_ACC_SVC_TYPE_XML:
		QAT_UTILS_LOG("Unsupported service type\n");
		return CPA_STATUS_UNSUPPORTED;

	default:
		QAT_UTILS_LOG("Invalid service type\n");
		*pNumInstances = 0;
		return CPA_STATUS_INVALID_PARAM;
	}
}

/**
 ******************************************************************************
 * @ingroup SalCtrl
 *****************************************************************************/
CpaStatus
cpaGetInstances(const CpaAccelerationServiceType accelerationServiceType,
		Cpa16U numInstances,
		CpaInstanceHandle *pInstances)
{
	LAC_CHECK_NULL_PARAM(pInstances);

	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
	case CPA_ACC_SVC_TYPE_CRYPTO:
		return Lac_GetCyInstancesByType(accelerationServiceType,
						numInstances,
						pInstances);

	case CPA_ACC_SVC_TYPE_DATA_COMPRESSION:
		return cpaDcGetInstances(numInstances, pInstances);

	case CPA_ACC_SVC_TYPE_PATTERN_MATCH:
	case CPA_ACC_SVC_TYPE_RAID:
	case CPA_ACC_SVC_TYPE_XML:
		QAT_UTILS_LOG("Unsupported service type\n");
		return CPA_STATUS_UNSUPPORTED;

	default:
		QAT_UTILS_LOG("Invalid service type\n");
		return CPA_STATUS_INVALID_PARAM;
	}
}
