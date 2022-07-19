/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
/**
 ***************************************************************************
 * @file icp_sal_poll.h
 *
 * @defgroup SalPoll
 *
 * @ingroup SalPoll
 *
 * @description
 *    Polling APIs for instance polling.
 *    These functions retrieve requests on appropriate response rings and
 *    dispatch the associated callbacks. Callbacks are called in the
 *    context of the polling function itself.
 *
 *
 ***************************************************************************/

#ifndef ICP_SAL_POLL_H
#define ICP_SAL_POLL_H

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll a Cy logical instance to retrieve requests that are on the
  *    response rings associated with that instance and dispatch the
  *    associated callbacks.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the rings
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_CyPollInstance(CpaInstanceHandle instanceHandle,
				 Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll a Sym Cy ring to retrieve requests that are on the
  *    response rings associated with that instance and dispatch the
  *    associated callbacks.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the rings
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_CyPollSymRing(CpaInstanceHandle instanceHandle,
				Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll an Asym Cy ring to retrieve requests that are on the
  *    response rings associated with that instance and dispatch the
  *    associated callbacks.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the rings
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_CyPollAsymRing(CpaInstanceHandle instanceHandle,
				 Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll a Cy NRBG ring to retrieve requests that are on the
  *    response rings associated with that instance and dispatch the
  *    associated callbacks.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the rings
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_CyPollNRBGRing(CpaInstanceHandle instanceHandle,
				 Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll the high priority symmetric response ring associated with a Cy
  *    logical instance to retrieve requests and dispatch the
  *    associated callbacks.
  *
  *    This API is recommended for data plane applications, in which the
  *    cost of offload - that is, the cycles consumed by the driver in
  *    sending requests to the hardware, and processing responses - needs
  *    to be minimized.  In particular, use of this API is recommended
  *    if the following constraints are acceptable to your application:
  *
  *    - Thread safety is not guaranteed.  Each software thread should
  *      have access to its own unique instance (CpaInstanceHandle) to
  *      avoid contention.
  *    - The "default" instance (@ref CPA_INSTANCE_HANDLE_SINGLE) is not
  *      supported on this API.  The specific handle should be obtained
  *      using the instance discovery functions (@ref cpaCyGetNumInstances,
  *      @ref cpaCyGetInstances).
  *
  *    This polling function should be used with the functions described
  *    in cpa_cy_sym_dp.h
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the ring
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_CyPollDpInstance(const CpaInstanceHandle instanceHandle,
				   const Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll a Dc logical instance to retrieve requests that are on the
  *    response ring associated with that instance and dispatch the
  *    associated callbacks.
  *
  * @context
  *      This function is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the ring
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_DcPollInstance(CpaInstanceHandle instanceHandle,
				 Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    Poll the response ring associated with a Dc logical instance to
  *    retrieve requests and dispatch the associated callbacks.
  *
  *    This API is recommended for data plane applications, in which the
  *    cost of offload - that is, the cycles consumed by the driver in
  *    sending requests to the hardware, and processing responses - needs
  *    to be minimized.  In particular, use of this API is recommended
  *    if the following constraints are acceptable to your application:
  *
  *    - Thread safety is not guaranteed.  Each software thread should
  *      have access to its own unique instance (CpaInstanceHandle) to
  *      avoid contention.
  *    - The "default" instance (@ref CPA_INSTANCE_HANDLE_SINGLE) is not
  *      supported on this API.  The specific handle should be obtained
  *      using the instance discovery functions (@ref cpaDcGetNumInstances,
  *      @ref cpaDcGetInstances).
  *
  *    This polling function should be used with the functions described
  *    in cpa_dc_dp.h

  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] instanceHandle     Instance handle.
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There are no responses on the ring
  *                               associated with this instance
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_DcPollDpInstance(CpaInstanceHandle dcInstance,
				   Cpa32U responseQuota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    This function polls the rings on the given bank to determine
  *    if any of the rings contain messages to be read. The
  *    response quota is per ring.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] accelId            Acceleration device Id, also known as
  *                               packageId. This can be obtained using
  *                               instance info functions (
  *                               @ref cpaCyInstanceGetInfo2
  *                               and @ref cpaDcInstanceGetInfo2)
  *
  * @param[in] bank_number        Bank number
  *
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There is no data on any ring on the bank
  *                               or the bank is already being polled
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus
icp_sal_pollBank(Cpa32U accelId, Cpa32U bank_number, Cpa32U response_quota);

/*************************************************************************
  * @ingroup SalPoll
  * @description
  *    This function polls the rings on all banks to determine
  *    if any of the rings contain messages to be read. The
  *    response quota is per ring.
  *
  * @context
  *      This functions is called from both the user and kernel context
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
  * @param[in] accelId            Acceleration device Id, also known as
  *                               packageId. This can be obtained using
  *                               instance info functions (
  *                               @ref cpaCyInstanceGetInfo2
  *                               and @ref cpaDcInstanceGetInfo2)
  *
  * @param[in] response_quota     The maximum number of messages that
  *                               will be read in one polling. Setting
  *                               the response quota to zero means that
  *                               all messages on the ring will be read.
  *
  * @retval CPA_STATUS_SUCCESS    Successfully polled a ring with data
  * @retval CPA_STATUS_RETRY      There is no data on any ring on any bank
  *                               or the banks are already being polled
  * @retval CPA_STATUS_FAIL       Indicates a failure
  *************************************************************************/
CpaStatus icp_sal_pollAllBanks(Cpa32U accelId, Cpa32U response_quota);

#endif
