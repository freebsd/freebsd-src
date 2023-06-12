/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */

/**
 *****************************************************************************
 * @file sal_get_instances.c
 *
 * @defgroup SalCtrl Service Access Layer Controller
 *
 * @ingroup SalCtrl
 *
 * @description
 *      This file contains the main function to get SAL instances.
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

/**
 ******************************************************************************
 * @ingroup SalCtrl
 * @description
 *   Get either sym or asym instance number
 *****************************************************************************/
static CpaStatus
Lac_GetSingleCyNumInstances(
    const CpaAccelerationServiceType accelerationServiceType,
    Cpa16U *pNumInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
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

		if (CPA_ACC_SVC_TYPE_CRYPTO_ASYM == accelerationServiceType) {
			list_temp = base_addr->asym_services;
		} else {
			list_temp = base_addr->sym_services;
		}
		while (NULL != list_temp) {
			num_inst++;
			list_temp = SalList_next(list_temp);
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
 *   Get either sym or asym instance
 *****************************************************************************/
static CpaStatus
Lac_GetSingleCyInstances(
    const CpaAccelerationServiceType accelerationServiceType,
    Cpa16U numInstances,
    CpaInstanceHandle *pInstances)
{
	CpaStatus status = CPA_STATUS_SUCCESS;
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
	default:
		QAT_UTILS_LOG("Invalid service type\n");
		return CPA_STATUS_INVALID_PARAM;
	}

	/* Get the number of instances */
	status = cpaGetNumInstances(accelerationServiceType,
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

		if (CPA_ACC_SVC_TYPE_CRYPTO_ASYM == accelerationServiceType)
			list_temp = base_addr->asym_services;
		else
			list_temp = base_addr->sym_services;
		while (NULL != list_temp) {
			if (index > (numInstances - 1))
				break;

			pInstances[index] = SalList_getObject(list_temp);
			list_temp = SalList_next(list_temp);
			index++;
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
	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
		return Lac_GetSingleCyNumInstances(accelerationServiceType,
						   pNumInstances);
	case CPA_ACC_SVC_TYPE_CRYPTO:
		return cpaCyGetNumInstances(pNumInstances);
	case CPA_ACC_SVC_TYPE_DATA_COMPRESSION:
		return cpaDcGetNumInstances(pNumInstances);

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
	switch (accelerationServiceType) {
	case CPA_ACC_SVC_TYPE_CRYPTO_ASYM:
	case CPA_ACC_SVC_TYPE_CRYPTO_SYM:
		return Lac_GetSingleCyInstances(accelerationServiceType,
						numInstances,
						pInstances);

	case CPA_ACC_SVC_TYPE_CRYPTO:
		return cpaCyGetInstances(numInstances, pInstances);
	case CPA_ACC_SVC_TYPE_DATA_COMPRESSION:
		return cpaDcGetInstances(numInstances, pInstances);

	default:
		QAT_UTILS_LOG("Invalid service type\n");
		return CPA_STATUS_INVALID_PARAM;
	}
}
