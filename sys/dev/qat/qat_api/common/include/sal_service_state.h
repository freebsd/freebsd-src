/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 ***************************************************************************
 * @file sal_service_state.h
 *
 * @defgroup SalServiceState
 *
 * @ingroup SalCtrl
 *
 * Checks state for generic service instance
 *
 ***************************************************************************/

#ifndef SAL_SERVICE_STATE_H_
#define SAL_SERVICE_STATE_H_

/**
*******************************************************************************
 * @ingroup SalServiceState
 *      Check to see if the instance is in the running state
 *
 * @description
 *      This function checks the state of an instance to see if it is in the
 *      running state
 *
 * @param[in]  instance   Instance handle (assumes this is valid, i.e. checked
 *                        before this function is called)
 * @retval CPA_TRUE       Instance in the RUNNING state
 * @retval CPA_FALSE      Instance not in RUNNING state
 *
 *****************************************************************************/
CpaBoolean Sal_ServiceIsRunning(CpaInstanceHandle instanceHandle);

/**
*******************************************************************************
 * @ingroup SalServiceState
 *      Check to see if the instance is beign restarted
 *
 * @description
 *      This function checks the state of an instance to see if the device it
 *      uses is being restarted because of hardware error.
 *
 * @param[in]  instance   Instance handle (assumes this is valid, i.e. checked
 *                        before this function is called)
 * @retval CPA_TRUE       Device the instance is using is restarting.
 * @retval CPA_FALSE      Device the instance is running.
 *
 *****************************************************************************/
CpaBoolean Sal_ServiceIsRestarting(CpaInstanceHandle instanceHandle);

/**
 *******************************************************************************
 * @ingroup SalServiceState
 *      This macro checks if an instance is running. An error message is logged
 *      if it is not in a running state.
 *
 * @return CPA_STATUS_FAIL Instance not in RUNNING state.
 * @return void            Instance is in RUNNING state.
 ******************************************************************************/
#define SAL_RUNNING_CHECK(instanceHandle)                                      \
	do {                                                                   \
		if (unlikely(CPA_TRUE !=                                       \
			     Sal_ServiceIsRunning(instanceHandle))) {          \
			if (CPA_TRUE ==                                        \
			    Sal_ServiceIsRestarting(instanceHandle)) {         \
				return CPA_STATUS_RESTARTING;                  \
			}                                                      \
			QAT_UTILS_LOG("Instance not in a Running state\n");    \
			return CPA_STATUS_FAIL;                                \
		}                                                              \
	} while (0)

/**
 *******************************************************************************
 * @ingroup SalServiceState
 *      This macro checks if an instance is in a state to get init event.
 *
 * @return CPA_STATUS_FAIL Instance not in good state.
 * @return void            Instance is in good state.
 ******************************************************************************/
#define SAL_SERVICE_GOOD_FOR_INIT(instanceHandle)                              \
	do {                                                                   \
		sal_service_t *pService = (sal_service_t *)instanceHandle;     \
		if ((SAL_SERVICE_STATE_UNINITIALIZED != pService->state) &&    \
		    (SAL_SERVICE_STATE_RESTARTING != pService->state)) {       \
			QAT_UTILS_LOG(                                         \
			    "Not in the correct state to call init\n");        \
			return CPA_STATUS_FAIL;                                \
		}                                                              \
	} while (0)

#endif /*SAL_SERVICE_STATE_H_*/
