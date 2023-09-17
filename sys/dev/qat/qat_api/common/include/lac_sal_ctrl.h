/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/**
 ***************************************************************************
 * @file lac_sal_ctrl.h
 *
 * @ingroup SalCtrl
 *
 * Functions to register and deregister qat and service controllers with ADF.
 *
 ***************************************************************************/

#ifndef LAC_SAL_CTRL_H
#define LAC_SAL_CTRL_H

/*******************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to check whether the service component
 *    has been successfully started.
 *
 * @context
 *      This function is called from the icp_sal_userStart() function.
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
 ******************************************************************/

CpaStatus SalCtrl_AdfServicesStartedCheck(void);

/*******************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to check whether the user's parameter
 *    for concurrent request is valid.
 *
 * @context
 *      This function is called when crypto or compression is init
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
 ******************************************************************/
CpaStatus validateConcurrRequest(Cpa32U numConcurrRequests);

/*******************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to register adf services
 *
 * @context
 *      This function is called from do_userStart() function
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
 ******************************************************************/
CpaStatus SalCtrl_AdfServicesRegister(void);

/*******************************************************************
 * @ingroup SalCtrl
 * @description
 *    This function is used to unregister adf services.
 *
 * @context
 *      This function is called from do_userStart() function
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
 ******************************************************************/
CpaStatus SalCtrl_AdfServicesUnregister(void);

#endif
