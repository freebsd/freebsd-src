/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file sal_service_state.c     Service state checks
 *
 * @ingroup SalServiceState
 *
 ***************************************************************************/

/*
*******************************************************************************
* Include public/global header files
*******************************************************************************
*/

#include "cpa.h"
#include "qat_utils.h"
#include "lac_list.h"
#include "icp_accel_devices.h"
#include "icp_adf_debug.h"
#include "lac_sal_types.h"
#include "sal_service_state.h"

CpaBoolean
Sal_ServiceIsRunning(CpaInstanceHandle instanceHandle)
{
	sal_service_t *pService = (sal_service_t *)instanceHandle;

	if (SAL_SERVICE_STATE_RUNNING == pService->state) {
		return CPA_TRUE;
	}
	return CPA_FALSE;
}

CpaBoolean
Sal_ServiceIsRestarting(CpaInstanceHandle instanceHandle)
{
	sal_service_t *pService = (sal_service_t *)instanceHandle;

	if (SAL_SERVICE_STATE_RESTARTING == pService->state) {
		return CPA_TRUE;
	}
	return CPA_FALSE;
}
