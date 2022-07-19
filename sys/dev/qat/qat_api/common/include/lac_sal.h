/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 *****************************************************************************
 * @file lac_sal.h
 *
 * @defgroup SalCtrl Service Access Layer Controller
 *
 * @ingroup SalCtrl
 *
 * @description
 *      These functions are the functions to be executed for each state
 *      of the state machine for each service.
 *
 *****************************************************************************/

#ifndef LAC_SAL_H
#define LAC_SAL_H

#include "cpa_cy_im.h"

/**
*******************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function allocates memory for a specific instance type.
 *      Zeros this memory and sets the generic service section of
 *      the instance memory.
 *
 * @context
 *      This function is called from the generic services init.
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
 * @param[in]  service         The type of the service to be created
 *                             (e.g. CRYPTO)
 * @param[in]  instance_num    The logical instance number which will
 *                             run the service
 * @param[out] pObj            Pointer to specific service instance memory
 * @retVal CPA_STATUS_SUCCESS  Instance memory successfully allocated
 * @retVal CPA_STATUS_RESOURCE Instance memory not successfully allocated
 * @retVal CPA_STATUS_FAIL     Unsupported service type
 *
 *****************************************************************************/
CpaStatus SalCtrl_ServiceCreate(sal_service_type_t service,
				Cpa32U instance_num,
				sal_service_t **pObj);

/**
*******************************************************************************
 * @ingroup SalCtl
 * @description
 *      This macro goes through the 'list' passed in as a parameter. For each
 *      element found in the list, it peforms a cast to the type of the element
 *      given by the 'type' parameter. Finally, it calls the function given by
 *      the 'function' parameter, passing itself and the device as parameters.
 *
 *      In case of error (i.e. 'function' does not return _SUCCESS or _RETRY)
 *      processing of the 'list' elements will stop and the status_ret will be
 *      updated.
 *
 *      In case of _RETRY status_ret will be updated but the 'list'
 *      will continue to be processed. _RETRY is only expected when
 *      'function' is stop.
 *
 * @context
 *      This macro is used by both the service and qat event handlers.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 *
 * @param[in]  list             The list of services or qats as a type of list_t
 * @param[in]  type             It identifies the type of the object inside the
 *                              list: service or qat
 * @param[in]  device           The ADF accelerator handle for the device
 * @param[in]  function         The function pointer to call
 * @param[in/out] status_ret    If an error occured (i.e. status returned from
 *                              function is not _SUCCESS) then status_ret is
 *                              overwritten with status returned from function.
 *
 *****************************************************************************/
#define SAL_FOR_EACH(list, type, device, function, status_ret)                 \
	do {                                                                   \
		sal_list_t *curr_element = list;                               \
		CpaStatus status_temp = CPA_STATUS_SUCCESS;                    \
		typeof(type) *process = NULL;                                  \
		while (NULL != curr_element) {                                 \
			process =                                              \
			    (typeof(type) *)SalList_getObject(curr_element);   \
			status_temp = process->function(device, process);      \
			if ((CPA_STATUS_SUCCESS != status_temp) &&             \
			    (CPA_STATUS_RETRY != status_temp)) {               \
				status_ret = status_temp;                      \
				break;                                         \
			} else {                                               \
				if (CPA_STATUS_RETRY == status_temp) {         \
					status_ret = status_temp;              \
				}                                              \
			}                                                      \
			curr_element = SalList_next(curr_element);             \
		}                                                              \
	} while (0)

/**
*******************************************************************************
 * @ingroup SalCtl
 * @description
 *      This macro goes through the 'list' passed in as a parameter. For each
 *      element found in the list, it peforms a cast to the type of the element
 *      given by the 'type' parameter. Finally, it checks the state of the
 *      element and if it is in state 'state_check' then it calls the
 *      function given by the 'function' parameter, passing itself
 *      and the device as parameters.
 *      If the element is not in 'state_check' it returns from the macro.
 *
 *      In case of error (i.e. 'function' does not return _SUCCESS)
 *      processing of the 'list' elements will continue.
 *
 * @context
 *      This macro is used by both the service and qat event handlers.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 *
 * @param[in]  list             The list of services or qats as a type of list_t
 * @param[in]  type             It identifies the type of the object
 *                              inside the list: service or qat
 * @param[in]  device           The ADF accelerator handle for the device
 * @param[in]  function         The function pointer to call
 * @param[in]  state_check      The state to check for
 *
 *****************************************************************************/
#define SAL_FOR_EACH_STATE(list, type, device, function, state_check)          \
	do {                                                                   \
		sal_list_t *curr_element = list;                               \
		typeof(type) *process = NULL;                                  \
		while (NULL != curr_element) {                                 \
			process =                                              \
			    (typeof(type) *)SalList_getObject(curr_element);   \
			if (process->state == state_check) {                   \
				process->function(device, process);            \
			} else {                                               \
				break;                                         \
			}                                                      \
			curr_element = SalList_next(curr_element);             \
		}                                                              \
	} while (0)

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to initialize an instance of crypto service.
 *   It creates a crypto instance's memory pools. It calls ADF to create
 *   its required transport handles. It calls the sub crypto service init
 *   functions. Resets the stats.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A crypto instance
 *
 *************************************************************************/
CpaStatus SalCtrl_CryptoInit(icp_accel_dev_t *device, sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to start an instance of crypto service.
 *  It sends the first messages to FW on its crypto instance transport
 *  handles. For asymmetric crypto it verifies the header on the downloaded
 *  MMP library.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventStart function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A crypto instance
 *
 *************************************************************************/
CpaStatus SalCtrl_CryptoStart(icp_accel_dev_t *device, sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to stop an instance of crypto service.
 *  It checks for inflight messages to the FW. If no messages are pending
 * it returns success. If messages are pending it returns retry.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventStop function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A crypto instance
 *
 *************************************************************************/
CpaStatus SalCtrl_CryptoStop(icp_accel_dev_t *device, sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to shutdown an instance of crypto service.
 *  It frees resources allocated at initialisation - e.g. frees the
 *  memory pools and ADF transport handles.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventShutdown function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A crypto instance
 *
 *************************************************************************/
CpaStatus SalCtrl_CryptoShutdown(icp_accel_dev_t *device,
				 sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function sets the capability info of crypto instances.
 *
 * @context
 *    This function is called from the cpaCyQueryCapabilities and
 *    LacSymSession_ParamCheck function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] service            A sal_service_t* type
 * @param[in] cyCapabilityInfo   A CpaCyCapabilitiesInfo* type
 *
 *************************************************************************/
void SalCtrl_CyQueryCapabilities(sal_service_t *pGenericService,
				 CpaCyCapabilitiesInfo *pCapInfo);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to initialize an instance of compression service.
 *   It creates a compression instance's memory pools. It calls ADF to create
 *   its required transport handles. It zeros an instances stats.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventInit function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A compression instance
 *
 *************************************************************************/

CpaStatus SalCtrl_CompressionInit(icp_accel_dev_t *device,
				  sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to start an instance of compression service.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventStart function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A compression instance
 *
 *************************************************************************/

CpaStatus SalCtrl_CompressionStart(icp_accel_dev_t *device,
				   sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to stop an instance of compression service.
 *  It checks for inflight messages to the FW. If no messages are pending
 * it returns success. If messages are pending it returns retry.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventStop function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A compression instance
 *
 *************************************************************************/

CpaStatus SalCtrl_CompressionStop(icp_accel_dev_t *device,
				  sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *      This function is used to shutdown an instance of compression service.
 *  It frees resources allocated at initialisation - e.g. frees the
 *  memory pools and ADF transport handles.
 *
 * @context
 *    This function is called from the SalCtrl_ServiceEventShutdown function.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No (ADF ensures that this function doesn't need to be thread safe)
 *
 * @param[in] device    An icp_accel_dev_t* type
 * @param[in] service   A compression instance
 *
 *************************************************************************/

CpaStatus SalCtrl_CompressionShutdown(icp_accel_dev_t *device,
				      sal_service_t *service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to get the number of services enabled
 *    from the config table.
 *
 * @context
 *    This function is called from the SalCtrl_QatInit
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
 * param[in] device            An icp_accel_dev_t* type
 * param[in] pEnabledServices  pointer to a variable used to store
 *                             the enabled services
 *
 *************************************************************************/

CpaStatus SalCtrl_GetEnabledServices(icp_accel_dev_t *device,
				     Cpa32U *pEnabledServices);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to check if a service is enabled
 *
 * @context
 *    This function is called from the SalCtrl_QatInit
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
 * param[in] enabled_services
 * param[in] service
 *
 *************************************************************************/

CpaBoolean SalCtrl_IsServiceEnabled(Cpa32U enabled_services,
				    sal_service_type_t service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to check if a service is supported on the device
 *    The key difference between this and SalCtrl_GetSupportedServices() is
 *    that the latter treats it as an error if the service is unsupported.
 *
 * @context
 *      This can be called anywhere.
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
 * param[in] device
 * param[in] service    service or services to check
 *
 *************************************************************************/
CpaBoolean SalCtrl_IsServiceSupported(icp_accel_dev_t *device,
				      sal_service_type_t service);

/*************************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to check whether enabled services has associated
 *    hardware capability support
 *
 * @context
 *      This functions is called from the SalCtrl_ServiceEventInit function.
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
 * param[in] device              A pointer to an icp_accel_dev_t
 * param[in] enabled_services    It is the bitmask for the enabled services
 *************************************************************************/

CpaStatus SalCtrl_GetSupportedServices(icp_accel_dev_t *device,
				       Cpa32U enabled_services);

#endif
