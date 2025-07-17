/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
/**
 ***************************************************************************
 * @file icp_sal_user.h
 *
 * @ingroup SalUser
 *
 * User space process init and shutdown functions.
 *
 ***************************************************************************/

#ifndef ICP_SAL_USER_H
#define ICP_SAL_USER_H

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function initialises and starts user space service access layer
  *    (SAL) - it registers SAL with ADF and initialises the ADF proxy.
  *    This function must only be called once per user space process.
  *
  * @context
  *      This function is called from the user process context
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
  * @param[in] pProcessName           Process address space name described in
  *                                   the config file for this device
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  *************************************************************************/
CpaStatus icp_sal_userStart(const char *pProcessName);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function is to be used with simplified config file, where user
  *    defines many user space processes. The driver generates unique
  *    process names based on the pProcessName provided.
  *    For example:
  *    If a config file in simplified format contains:
  *    [SSL]
  *    NumProcesses = 3
  *
  *    Then three internal sections will be generated and the three
  *    applications can be started at a given time. Each application can call
  *    icp_sal_userStartMultiProcess("SSL"). In this case the driver will
  *    figure out the unique name to use for each process.
  *
  * @context
  *      This function is called from the user process context
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
  * @param[in] pProcessName           Process address space name described in
  *                                   the new format of the config file
  *                                   for this device.
  *
  * @param[in] limitDevAccess         Specifies if the address space is limited
  *                                   to one device (true) or if it spans
  *                                   across multiple devices.
  *
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed. In this case user
  *                                   can wait and retry.
  *
  *************************************************************************/
CpaStatus icp_sal_userStartMultiProcess(const char *pProcessName,
					CpaBoolean limitDevAccess);

/*************************************************************************
 * @ingroup SalUser
 * @description
 *    This function stops and shuts down user space SAL
 *     - it deregisters SAL with ADF and shuts down ADF proxy
 *
 * @context
 *      This function is called from the user process context
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
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 *
 ************************************************************************/
CpaStatus icp_sal_userStop(void);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function gets the number of the available dynamic allocated
  *    crypto instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userCyGetAvailableNumDynInstances(Cpa32U *pNumCyInstances);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function gets the number of the available dynamic allocated
  *    compression instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userDcGetAvailableNumDynInstances(Cpa32U *pNumDcInstances);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function gets the number of the available dynamic allocated
  *    crypto instances which are from the specific device package.
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus
icp_sal_userCyGetAvailableNumDynInstancesByDevPkg(Cpa32U *pNumCyInstances,
						  Cpa32U devPkgID);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function gets the number of the available dynamic allocated
  *    crypto instances which are from the specific device package and specific
  *    accelerator.
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus
icp_sal_userCyGetAvailableNumDynInstancesByPkgAccel(Cpa32U *pNumCyInstances,
						    Cpa32U devPkgID,
						    Cpa32U accelerator_number);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function gets the number of the available dynamic allocated
  *    compression instances which are from the specific device package.
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus
icp_sal_userDcGetAvailableNumDynInstancesByDevPkg(Cpa32U *pNumDcInstances,
						  Cpa32U devPkgID);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function allocates crypto instances
  *    from dynamic crypto instance pool
  *     - it adds new allocated instances into crypto_services
  *     - it initializes new allocated instances
  *     - it starts new allocated instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userCyInstancesAlloc(Cpa32U numCyInstances,
				       CpaInstanceHandle *pCyInstances);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function allocates crypto instances
  *    from dynamic crypto instance pool
  *    which are from the specific device package.
  *     - it adds new allocated instances into crypto_services
  *     - it initializes new allocated instances
  *     - it starts new allocated instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userCyInstancesAllocByDevPkg(Cpa32U numCyInstances,
					       CpaInstanceHandle *pCyInstances,
					       Cpa32U devPkgID);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function allocates crypto instances
  *    from dynamic crypto instance pool
  *    which are from the specific device package and specific accelerator
  *     - it adds new allocated instances into crypto_services
  *     - it initializes new allocated instances
  *     - it starts new allocated instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus
icp_sal_userCyInstancesAllocByPkgAccel(Cpa32U numCyInstances,
				       CpaInstanceHandle *pCyInstances,
				       Cpa32U devPkgID,
				       Cpa32U accelerator_number);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function frees crypto instances allocated
  *    from dynamic crypto instance pool
  *     - it stops the instances
  *     - it shutdowns the instances
  *     - it removes the instances from crypto_services
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userCyFreeInstances(Cpa32U numCyInstances,
				      CpaInstanceHandle *pCyInstances);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function allocates compression instances
  *    from dynamic compression instance pool
  *     - it adds new allocated instances into compression_services
  *     - it initializes new allocated instances
  *     - it starts new allocated instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userDcInstancesAlloc(Cpa32U numDcInstances,
				       CpaInstanceHandle *pDcInstances);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function allocates compression instances
  *    from dynamic compression instance pool
  *    which are from the specific device package.
  *     - it adds new allocated instances into compression_services
  *     - it initializes new allocated instances
  *     - it starts new allocated instances
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userDcInstancesAllocByDevPkg(Cpa32U numDcInstances,
					       CpaInstanceHandle *pDcInstances,
					       Cpa32U devPkgID);

/*************************************************************************
  * @ingroup SalUser
  * @description
  *    This function frees compression instances allocated
  *    from dynamic compression instance pool
  *     - it stops the instances
  *     - it shutdowns the instances
  *     - it removes the instances from compression_services
  *
  * @context
  *      This function is called from the user process context
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
  * @retval CPA_STATUS_SUCCESS        No error
  * @retval CPA_STATUS_FAIL           Operation failed
  *
  ************************************************************************/
CpaStatus icp_sal_userDcFreeInstances(Cpa32U numDcInstances,
				      CpaInstanceHandle *pDcInstances);

/*************************************************************************
 * @ingroup SalUser
 * @description
 *    This function checks if new devices have been started and if so
 *    starts to use them.
 *
 * @context
 *      This function is called from the user process context
 *      in threadless mode
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
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 *
 ************************************************************************/
CpaStatus icp_sal_find_new_devices(void);

/*************************************************************************
 * @ingroup SalUser
 * @description
 *    This function polls device events.
 *
 * @context
 *      This function is called from the user process context
 *      in threadless mode
 *
 * @assumptions
 *      None
 * @sideEffects
 *      In case a device has been stopped or restarted the application
 *      will get restarting/stop/shutdown events
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 *
 ************************************************************************/
CpaStatus icp_sal_poll_device_events(void);

/*
 * icp_adf_check_device
 *
 * @description:
 *  This function checks the status of the firmware/hardware for a given device.
 *  This function is used as part of the heartbeat functionality.
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      In case a device is unresponsive the device will
 *      be restarted.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] accelId                Device Id.
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 */
CpaStatus icp_sal_check_device(Cpa32U accelId);

/*
 * icp_adf_check_all_devices
 *
 * @description:
 *  This function checks the status of the firmware/hardware for all devices.
 *  This function is used as part of the heartbeat functionality.
 *
 * @context
 *      This function is called from the user process context
 * @assumptions
 *      None
 * @sideEffects
 *      In case a device is unresponsive the device will
 *      be restarted.
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @retval CPA_STATUS_SUCCESS        No error
 * @retval CPA_STATUS_FAIL           Operation failed
 */
CpaStatus icp_sal_check_all_devices(void);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to send messages to VF
 *
 * @context
 *      None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_userSendMsgToVf(Cpa32U accelId, Cpa32U vfNum, Cpa32U message);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to send messages to PF
 *
 * @context
 *      None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_userSendMsgToPf(Cpa32U accelId, Cpa32U message);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to get messages from VF
 *
 * @context
 *      None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_userGetMsgFromVf(Cpa32U accelId,
				   Cpa32U vfNum,
				   Cpa32U *message,
				   Cpa32U *messageCounter);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to get messages from PF
 *
 * @context
 *      None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_userGetMsgFromPf(Cpa32U accelId,
				   Cpa32U *message,
				   Cpa32U *messageCounter);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to get pfvf comms status
 *
 * @context
 *      None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_userGetPfVfcommsStatus(CpaBoolean *unreadMessage);

/*
 * @ingroup icp_sal_user
 * @description
 *      This is a stub function to reset the device
 *
 * @context
 *     None
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @reentrant
 *      Yes
 * @threadSafe
 *      Yes
 *
*/
CpaStatus icp_sal_reset_device(Cpa32U accelId);

/**
 *****************************************************************************
 * @ingroup icp_sal_user
 *      Retrieve number of in flight requests for a nrbg tx ring
 *      from a crypto instance (Traditional API).
 *
 * @description
 *      This function is a part of back-pressure mechanism.
 *      Applications can query for inflight requests in
 *      the appropriate service/ring on each instance
 *      and select any instance with sufficient space or
 *      the instance with the lowest number.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle         Crypto API instance handle.
 * @param[out] maxInflightRequests    Maximal number of in flight requests.
 * @param[out] numInflightRequests    Current number of in flight requests.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_NrbgGetInflightRequests(CpaInstanceHandle instanceHandle,
					  Cpa32U *maxInflightRequests,
					  Cpa32U *numInflightRequests);

/**
 *****************************************************************************
 * @ingroup icp_sal_user
 *      Retrieve number of in flight requests for a symmetric tx ring
 *      from a crypto instance (Traditional API).
 *
 * @description
 *      This function is a part of back-pressure mechanism.
 *      Applications can query for inflight requests in
 *      the appropriate service/ring on each instance
 *      and select any instance with sufficient space or
 *      the instance with the lowest number.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle         Crypto API instance handle.
 * @param[out] maxInflightRequests    Maximal number of in flight requests.
 * @param[out] numInflightRequests    Current number of in flight requests.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_SymGetInflightRequests(CpaInstanceHandle instanceHandle,
					 Cpa32U *maxInflightRequests,
					 Cpa32U *numInflightRequests);

/**
 *****************************************************************************
 * @ingroup icp_sal_user
 *      Retrieve number of in flight requests for an asymmetric tx ring
 *      from a crypto instance (Traditional API).
 *
 * @description
 *      This function is a part of back-pressure mechanism.
 *      Applications can query the appropriate service/ring on each instance
 *      and select any instance with sufficient space or
 *      the instance with the lowest number.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle         Crypto API instance handle.
 * @param[out] maxInflightRequests    Maximal number of in flight requests.
 * @param[out] numInflightRequests    Current number of in flight requests.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_AsymGetInflightRequests(CpaInstanceHandle instanceHandle,
					  Cpa32U *maxInflightRequests,
					  Cpa32U *numInflightRequests);

/**
 *****************************************************************************
 * @ingroup icp_sal_user
 *      Retrieve number of in flight requests for a symmetric tx ring
 *      from a crypto instancei (Data Plane API).
 *
 * @description
 *      This function is a part of back-pressure mechanism.
 *      Applications can query the appropriate service/ring on each instance
 *      and select any instance with sufficient space or
 *      the instance with the lowest number.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in]  instanceHandle         Crypto API instance handle.
 * @param[out] maxInflightRequests    Maximal number of in flight requests.
 * @param[out] numInflightRequests    Current number of in flight requests.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_dp_SymGetInflightRequests(CpaInstanceHandle instanceHandle,
					    Cpa32U *maxInflightRequests,
					    Cpa32U *numInflightRequests);

/**
 *****************************************************************************
 * @ingroup icp_sal_user
 *      Updates the CSR with queued requests in the asymmetric tx ring.
 *
 * @description
 *      The function writes current shadow tail pointer of the asymmetric
 *      TX ring into ring's CSR. Updating the CSR will notify the HW that
 *      there are request(s) queued to be processed. The CSR is updated
 *      always, disregarding the current value of shadow tail pointer and
 *      the current CSR's tail value.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      Yes
 *
 * @param[in] instanceHandle         Crypto API instance handle.
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_AsymPerformOpNow(CpaInstanceHandle instanceHandle);

/**
 *****************************************************************************
 * @ingroup icp_sal_setForceAEADMACVerify
 *      Sets forceAEADMacVerify for particular instance to force HW MAC
 *      validation.
 *
 * @description
 * 	By default HW MAC verification is set to CPA_TRUE - this utility
 * 	function allows to change default behavior.
 *
 * @assumptions
 *      None
 * @sideEffects
 *      None
 * @blocking
 *      None
 * @reentrant
 *      No
 * @threadSafe
 *      No
 *
 * @param[in] instanceHandle         Crypto API instance handle.
 * @param[in] forceAEADMacVerify     new value
 *
 * @retval CPA_STATUS_SUCCESS        Function executed successfully.
 * @retval CPA_STATUS_FAIL           Function failed.
 * @pre
 *      None
 * @post
 *      None
 * @see
 *      None
 *
 *****************************************************************************/
CpaStatus icp_sal_setForceAEADMACVerify(CpaInstanceHandle instanceHandle,
					CpaBoolean forceAEADMacVerify);
#endif
