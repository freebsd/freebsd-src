/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file sal_user_process.c
 *
 * @ingroup SalUserProcess
 *
 * @description
 *    This file contains implementation of functions to set/get user process
 *    name
 *
 *****************************************************************************/

#include "qat_utils.h"
#include "lac_common.h"
static char lacProcessName[LAC_USER_PROCESS_NAME_MAX_LEN + 1] =
    LAC_KERNEL_PROCESS_NAME;

/**< Process name used to obtain values from correct section of config file. */

/*
 * @ingroup LacCommon
 * @description
 *      This function sets the process name
 *
 * @context
 *      This functions is called from module_init or from user space process
 *      initialisation function
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * param[in]  processName    Process name to be set
*/
CpaStatus
icpSetProcessName(const char *processName)
{
	LAC_CHECK_NULL_PARAM(processName);

	if (strnlen(processName, LAC_USER_PROCESS_NAME_MAX_LEN) ==
	    LAC_USER_PROCESS_NAME_MAX_LEN) {
		QAT_UTILS_LOG(
		    "Process name too long, maximum process name is %d>\n",
		    LAC_USER_PROCESS_NAME_MAX_LEN);
		return CPA_STATUS_FAIL;
	}

	strncpy(lacProcessName, processName, LAC_USER_PROCESS_NAME_MAX_LEN);
	lacProcessName[LAC_USER_PROCESS_NAME_MAX_LEN] = '\0';

	return CPA_STATUS_SUCCESS;
}

/*
 * @ingroup LacCommon
 * @description
 *      This function gets the process name
 *
 * @context
 *      This functions is called from LAC context
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
*/
char *
icpGetProcessName(void)
{
	return lacProcessName;
}
